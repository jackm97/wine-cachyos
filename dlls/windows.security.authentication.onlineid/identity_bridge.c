/* Host identity bridge client
 *
 * Copyright (C) 2026 Mirko Brombin
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 */

#include "ntstatus.h"
#define WIN32_NO_STATUS
#include "private.h"

#include "winternl.h"
#include "unixlib.h"

#define BRIDGE_MAGIC 0x31424953
#define BRIDGE_VERSION 1
#define BRIDGE_RESPONSE 0x8000
#define BRIDGE_MAX_PAYLOAD (1024 * 1024)
#define BRIDGE_MAX_FIELDS 64

#define OP_AUTHORIZE 2
#define OP_GET_TOKEN 3
#define OP_LIST_ACCOUNTS 5

#define STATUS_OK 0
#define STATUS_CANCELED 1
#define STATUS_UNAVAILABLE 2
#define STATUS_INVALID_REQUEST 3
#define STATUS_AUTH_FAILED 4

#define FIELD_CLIENT_ID 1
#define FIELD_SCOPES 2
#define FIELD_AUTHORITY 3
#define FIELD_LOGIN_HINT 4
#define FIELD_CALLER_ID 5
#define FIELD_ACCESS_TOKEN 10
#define FIELD_ID_TOKEN 11
#define FIELD_EXPIRES_AT 12
#define FIELD_ACCOUNT_ID 13
#define FIELD_USERNAME 14
#define FIELD_DISPLAY_NAME 17
#define FIELD_TENANT_ID 18
#define FIELD_CLIENT_INFO 19
#define FIELD_ERROR 20
#define FIELD_POLICY 21
#define FIELD_ACCOUNTS 22

#pragma pack(push, 1)
struct bridge_header
{
    UINT32 magic;
    UINT16 version;
    UINT16 opcode;
    UINT32 request_id;
    UINT32 payload_size;
    INT32 status;
};

struct bridge_field
{
    UINT16 id;
    UINT16 flags;
    UINT32 size;
};
#pragma pack(pop)

struct message_builder
{
    BYTE *data;
    SIZE_T size;
    SIZE_T capacity;
};

#define ONLINEID_CALL( func, params ) WINE_UNIX_CALL( unix_ ## func, params )

static INIT_ONCE unix_init_once = INIT_ONCE_STATIC_INIT;
static BOOL unix_lib_initialized;
static LONG request_counter;

static BOOL WINAPI init_unix_lib_once( INIT_ONCE *once, void *param, void **context )
{
    unix_lib_initialized = !__wine_init_unix_call();
    return TRUE;
}

static BOOL init_unix_lib(void)
{
    InitOnceExecuteOnce( &unix_init_once, init_unix_lib_once, NULL, NULL );
    return unix_lib_initialized;
}

static HRESULT builder_reserve( struct message_builder *builder, SIZE_T extra )
{
    SIZE_T capacity;
    BYTE *data;

    if (extra > BRIDGE_MAX_PAYLOAD || builder->size > BRIDGE_MAX_PAYLOAD - extra) return E_INVALIDARG;
    if (builder->size + extra <= builder->capacity) return S_OK;
    capacity = max( builder->capacity * 2, builder->size + extra );
    capacity = min( capacity, (SIZE_T)BRIDGE_MAX_PAYLOAD );
    if (!(data = realloc( builder->data, capacity ))) return E_OUTOFMEMORY;
    builder->data = data;
    builder->capacity = capacity;
    return S_OK;
}

static HRESULT builder_add( struct message_builder *builder, UINT16 id, const void *value, UINT32 size )
{
    struct bridge_field field = {id, 0, size};
    HRESULT hr;

    if (FAILED(hr = builder_reserve( builder, sizeof(field) + size ))) return hr;
    memcpy( builder->data + builder->size, &field, sizeof(field) );
    builder->size += sizeof(field);
    if (size)
    {
        memcpy( builder->data + builder->size, value, size );
        builder->size += size;
    }
    return S_OK;
}

static HRESULT hstring_to_utf8( HSTRING string, char **out, UINT32 *size )
{
    const WCHAR *value;
    UINT32 length;
    int count;

    if (!out || !size) return E_POINTER;
    *out = NULL;
    *size = 0;
    value = WindowsGetStringRawBuffer( string, &length );
    if (!length)
    {
        if (!(*out = calloc( 1, 1 ))) return E_OUTOFMEMORY;
        return S_OK;
    }
    if (!(count = WideCharToMultiByte( CP_UTF8, WC_ERR_INVALID_CHARS, value, length, NULL, 0, NULL, NULL )))
        return HRESULT_FROM_WIN32( GetLastError() );
    if (!(*out = malloc( count + 1 ))) return E_OUTOFMEMORY;
    if (!WideCharToMultiByte( CP_UTF8, WC_ERR_INVALID_CHARS, value, length, *out, count, NULL, NULL ))
    {
        free( *out );
        *out = NULL;
        return HRESULT_FROM_WIN32( GetLastError() );
    }
    (*out)[count] = 0;
    *size = count;
    return S_OK;
}

static HRESULT utf8_to_hstring( const BYTE *value, UINT32 size, HSTRING *out )
{
    WCHAR *buffer;
    int count;
    HRESULT hr;

    if (!out) return E_POINTER;
    *out = NULL;
    if (!size) return WindowsCreateString( L"", 0, out );
    if (!(count = MultiByteToWideChar( CP_UTF8, MB_ERR_INVALID_CHARS, (const char *)value, size, NULL, 0 )))
        return HRESULT_FROM_WIN32( GetLastError() );
    if (!(buffer = malloc( count * sizeof(*buffer) ))) return E_OUTOFMEMORY;
    MultiByteToWideChar( CP_UTF8, MB_ERR_INVALID_CHARS, (const char *)value, size, buffer, count );
    hr = WindowsCreateString( buffer, count, out );
    free( buffer );
    return hr;
}

static HRESULT device_authorize_client( HSTRING client_id, HSTRING scopes, HSTRING authority,
                                       HSTRING account_id, HSTRING login_hint, BOOL silent, HANDLE cancel,
                                       struct identity_reply *reply )
{
    const WCHAR *value;
    WCHAR guid_string[39];
    UINT32 length;
    GUID application_id;
    HRESULT hr;

    value = WindowsGetStringRawBuffer( client_id, &length );
    if (length != 36) return E_INVALIDARG;
    guid_string[0] = '{';
    memcpy( guid_string + 1, value, length * sizeof(*value) );
    guid_string[37] = '}';
    guid_string[38] = 0;
    if (FAILED(hr = CLSIDFromString( guid_string, &application_id ))) return hr;
    if (silent) return identity_device_get_token( &application_id, scopes, authority, account_id, login_hint, cancel, reply );
    return identity_device_authorize( &application_id, scopes, authority, login_hint, cancel, reply );
}

HRESULT identity_native_credentials( UINT operation, const char *client_id, HSTRING authority,
                                    char *data, DWORD capacity, DWORD *size )
{
    struct credential_params params = {0};
    WCHAR tenant[37];
    NTSTATUS status;
    HRESULT hr;

    if (!client_id || strlen(client_id) >= sizeof(params.client_id)) return E_INVALIDARG;
    if (!init_unix_lib()) return HRESULT_FROM_WIN32( ERROR_SERVICE_NOT_ACTIVE );
    if (FAILED(hr = identity_authority_tenant( authority, tenant ))) return hr;
    strcpy( params.client_id, client_id );
    if (!WideCharToMultiByte( CP_UTF8, 0, tenant, -1, params.authority, sizeof(params.authority), NULL, NULL ))
        return HRESULT_FROM_WIN32( GetLastError() );
    params.operation = operation;
    params.data = (ULONG_PTR)data;
    params.capacity = capacity;
    status = ONLINEID_CALL( credentials, &params );
    if (size) *size = params.size;
    if (status == STATUS_NOT_SUPPORTED || status == STATUS_ACCESS_DENIED)
        return HRESULT_FROM_WIN32( ERROR_SERVICE_NOT_ACTIVE );
    return status ? HRESULT_FROM_NT(status) : S_OK;
}

static HRESULT response_status_to_hresult( INT32 status )
{
    switch (status)
    {
    case STATUS_OK: return S_OK;
    case STATUS_CANCELED: return HRESULT_FROM_WIN32( ERROR_CANCELLED );
    case STATUS_UNAVAILABLE: return HRESULT_FROM_WIN32( ERROR_SERVICE_NOT_ACTIVE );
    case STATUS_INVALID_REQUEST: return E_INVALIDARG;
    case STATUS_AUTH_FAILED: return E_ACCESSDENIED;
    default: return E_FAIL;
    }
}

static HRESULT parse_response( const struct bridge_header *header, const BYTE *payload, struct identity_reply *reply )
{
    UINT16 seen[BRIDGE_MAX_FIELDS];
    UINT32 offset = 0, count = 0, i;
    HRESULT hr = response_status_to_hresult( header->status );

    reply->error = hr;
    while (offset < header->payload_size)
    {
        const struct bridge_field *field;
        HSTRING *target = NULL;

        if (++count > BRIDGE_MAX_FIELDS || header->payload_size - offset < sizeof(*field)) return E_INVALIDARG;
        field = (const struct bridge_field *)(payload + offset);
        offset += sizeof(*field);
        if (field->flags || field->size > header->payload_size - offset) return E_INVALIDARG;
        for (i = 0; i + 1 < count; ++i)
            if (seen[i] == field->id) return E_INVALIDARG;
        seen[count - 1] = field->id;
        switch (field->id)
        {
        case FIELD_SCOPES: target = &reply->scopes; break;
        case FIELD_ACCESS_TOKEN: target = &reply->access_token; break;
        case FIELD_ID_TOKEN: target = &reply->id_token; break;
        case FIELD_EXPIRES_AT:
            if (!field->size || field->size > 20) return E_INVALIDARG;
            for (i = 0; i < field->size; ++i)
            {
                BYTE digit = payload[offset + i];
                if (digit < '0' || digit > '9' || reply->expires_at >
                    (~(UINT64)0 - (digit - '0')) / 10) return E_INVALIDARG;
                reply->expires_at = reply->expires_at * 10 + digit - '0';
            }
            break;
        case FIELD_ACCOUNT_ID: target = &reply->account_id; break;
        case FIELD_USERNAME: target = &reply->username; break;
        case FIELD_DISPLAY_NAME: target = &reply->display_name; break;
        case FIELD_TENANT_ID: target = &reply->tenant_id; break;
        case FIELD_CLIENT_INFO: target = &reply->client_info; break;
        case FIELD_ERROR: target = &reply->error_message; break;
        case FIELD_ACCOUNTS:
            if (header->opcode != (OP_LIST_ACCOUNTS | BRIDGE_RESPONSE) || field->size < sizeof(UINT32))
                return E_INVALIDARG;
            if (!(reply->accounts = malloc( field->size ))) return E_OUTOFMEMORY;
            memcpy( reply->accounts, payload + offset, field->size );
            reply->accounts_size = field->size;
            break;
        }
        if (target && FAILED(hr = utf8_to_hstring( payload + offset, field->size, target ))) return hr;
        offset += field->size;
    }
    if (SUCCEEDED(reply->error))
    {
        if (header->opcode == (OP_LIST_ACCOUNTS | BRIDGE_RESPONSE))
        {
            if (!reply->accounts) return E_UNEXPECTED;
        }
        else if (!reply->access_token || !reply->account_id) return E_UNEXPECTED;
    }
    return reply->error;
}

void identity_reply_clear( struct identity_reply *reply )
{
    if (!reply) return;
    WindowsDeleteString( reply->access_token );
    WindowsDeleteString( reply->id_token );
    WindowsDeleteString( reply->account_id );
    WindowsDeleteString( reply->username );
    WindowsDeleteString( reply->display_name );
    WindowsDeleteString( reply->tenant_id );
    WindowsDeleteString( reply->client_info );
    WindowsDeleteString( reply->scopes );
    WindowsDeleteString( reply->error_message );
    free( reply->accounts );
    memset( reply, 0, sizeof(*reply) );
}

static HRESULT bridge_exchange( UINT16 opcode, const struct message_builder *builder,
                                HANDLE cancel, struct identity_reply *reply )
{
    struct bridge_header request, response;
    struct exchange_params params = {0};
    BYTE *request_data = NULL, *response_data = NULL;
    NTSTATUS status;
    HRESULT hr;

    if (!init_unix_lib()) return HRESULT_FROM_WIN32( ERROR_SERVICE_NOT_ACTIVE );
    request.magic = BRIDGE_MAGIC;
    request.version = BRIDGE_VERSION;
    request.opcode = opcode;
    request.request_id = InterlockedIncrement( &request_counter );
    request.payload_size = builder->size;
    request.status = 0;
    if (!(request_data = malloc( sizeof(request) + builder->size )) ||
        !(response_data = malloc( sizeof(response) + BRIDGE_MAX_PAYLOAD )))
    {
        hr = E_OUTOFMEMORY;
        goto done;
    }
    memcpy( request_data, &request, sizeof(request) );
    memcpy( request_data + sizeof(request), builder->data, builder->size );
    params.request = (ULONG_PTR)request_data;
    params.response = (ULONG_PTR)response_data;
    params.request_size = sizeof(request) + builder->size;
    params.response_capacity = sizeof(response) + BRIDGE_MAX_PAYLOAD;
    params.cancel = (ULONG_PTR)cancel;
    if ((status = ONLINEID_CALL( exchange, &params )))
    {
        hr = status == STATUS_CANCELLED ? HRESULT_FROM_WIN32( ERROR_CANCELLED ) : HRESULT_FROM_NT( status );
        goto done;
    }
    if (params.response_size < sizeof(response))
    {
        hr = E_INVALIDARG;
        goto done;
    }
    memcpy( &response, response_data, sizeof(response) );
    if (response.magic != BRIDGE_MAGIC || response.version != BRIDGE_VERSION ||
        response.opcode != (opcode | BRIDGE_RESPONSE) || response.request_id != request.request_id ||
        response.payload_size > BRIDGE_MAX_PAYLOAD ||
        params.response_size != sizeof(response) + response.payload_size)
    {
        hr = E_INVALIDARG;
        goto done;
    }
    hr = parse_response( &response, response_data + sizeof(response), reply );

done:
    free( response_data );
    free( request_data );
    return hr;
}

HRESULT identity_bridge_list_accounts( HSTRING client_id, HSTRING authority, HANDLE cancel,
                                      struct identity_reply *reply )
{
    struct message_builder builder = {0};
    char *client = NULL, *tenant = NULL;
    UINT32 client_size, tenant_size;
    HRESULT hr;

    if (!reply) return E_POINTER;
    memset( reply, 0, sizeof(*reply) );
    if (!GetEnvironmentVariableW( L"SODA_IDENTITY_BRIDGE_SOCKET", NULL, 0 ))
        return identity_device_list_accounts( client_id, authority, cancel, reply );
    if (FAILED(hr = hstring_to_utf8( client_id, &client, &client_size )) ||
        FAILED(hr = hstring_to_utf8( authority, &tenant, &tenant_size ))) goto done;
    if (client_size && FAILED(hr = builder_add( &builder, FIELD_CLIENT_ID, client, client_size ))) goto done;
    if (tenant_size && FAILED(hr = builder_add( &builder, FIELD_AUTHORITY, tenant, tenant_size ))) goto done;
    hr = bridge_exchange( OP_LIST_ACCOUNTS, &builder, cancel, reply );

done:
    free( builder.data );
    free( client );
    free( tenant );
    return hr;
}

HRESULT identity_bridge_authorize_client( HSTRING client_id, HSTRING scopes, HSTRING authority,
                                          HSTRING login_hint, HSTRING policy, HANDLE cancel,
                                          struct identity_reply *reply )
{
    struct message_builder builder = {0};
    char *client_utf8 = NULL, *scopes_utf8 = NULL, *authority_utf8 = NULL;
    char *login_hint_utf8 = NULL, *policy_utf8 = NULL;
    UINT32 client_size, scopes_size, authority_size, login_hint_size, policy_size;
    HRESULT hr;

    if (!client_id || !reply) return E_POINTER;
    memset( reply, 0, sizeof(*reply) );
    if (!GetEnvironmentVariableW( L"SODA_IDENTITY_BRIDGE_SOCKET", NULL, 0 ))
        return device_authorize_client( client_id, scopes, authority, NULL, login_hint, FALSE, cancel, reply );
    if (FAILED(hr = hstring_to_utf8( client_id, &client_utf8, &client_size )) ||
        FAILED(hr = hstring_to_utf8( scopes, &scopes_utf8, &scopes_size )) ||
        FAILED(hr = hstring_to_utf8( authority, &authority_utf8, &authority_size )) ||
        FAILED(hr = hstring_to_utf8( login_hint, &login_hint_utf8, &login_hint_size )) ||
        FAILED(hr = hstring_to_utf8( policy, &policy_utf8, &policy_size )) ||
        FAILED(hr = builder_add( &builder, FIELD_CLIENT_ID, client_utf8, client_size )) ||
        FAILED(hr = builder_add( &builder, FIELD_SCOPES, scopes_utf8, scopes_size )) ||
        FAILED(hr = builder_add( &builder, FIELD_AUTHORITY, authority_utf8, authority_size )) ||
        FAILED(hr = builder_add( &builder, FIELD_LOGIN_HINT, login_hint_utf8, login_hint_size )) ||
        FAILED(hr = builder_add( &builder, FIELD_POLICY, policy_utf8, policy_size )))
        goto done;
    hr = bridge_exchange( OP_AUTHORIZE, &builder, cancel, reply );

done:
    if (FAILED(hr) && !reply->error) reply->error = hr;
    free( builder.data );
    free( client_utf8 );
    free( scopes_utf8 );
    free( authority_utf8 );
    free( login_hint_utf8 );
    free( policy_utf8 );
    return hr;
}

HRESULT identity_bridge_get_token( HSTRING client_id, HSTRING scopes, HSTRING authority,
                                   HSTRING account_id, HANDLE cancel, struct identity_reply *reply )
{
    struct message_builder builder = {0};
    char *client_utf8 = NULL, *scopes_utf8 = NULL, *authority_utf8 = NULL, *account_utf8 = NULL;
    UINT32 client_size, scopes_size, authority_size, account_size;
    HRESULT hr;

    if (!client_id || !reply) return E_POINTER;
    memset( reply, 0, sizeof(*reply) );
    if (!GetEnvironmentVariableW( L"SODA_IDENTITY_BRIDGE_SOCKET", NULL, 0 ))
        return device_authorize_client( client_id, scopes, authority, account_id, NULL, TRUE, cancel, reply );
    if (FAILED(hr = hstring_to_utf8( client_id, &client_utf8, &client_size )) ||
        FAILED(hr = hstring_to_utf8( scopes, &scopes_utf8, &scopes_size )) ||
        FAILED(hr = hstring_to_utf8( authority, &authority_utf8, &authority_size )) ||
        FAILED(hr = hstring_to_utf8( account_id, &account_utf8, &account_size )) ||
        FAILED(hr = builder_add( &builder, FIELD_CLIENT_ID, client_utf8, client_size )) ||
        FAILED(hr = builder_add( &builder, FIELD_SCOPES, scopes_utf8, scopes_size )) ||
        FAILED(hr = builder_add( &builder, FIELD_AUTHORITY, authority_utf8, authority_size )) ||
        FAILED(hr = builder_add( &builder, FIELD_ACCOUNT_ID, account_utf8, account_size )))
        goto done;
    hr = bridge_exchange( OP_GET_TOKEN, &builder, cancel, reply );

done:
    if (FAILED(hr) && !reply->error) reply->error = hr;
    free( builder.data );
    free( client_utf8 );
    free( scopes_utf8 );
    free( authority_utf8 );
    free( account_utf8 );
    return hr;
}

HRESULT identity_bridge_authorize( const GUID *application_id, HSTRING service, HSTRING policy,
                                   HANDLE cancel, struct identity_reply *reply )
{
    struct message_builder builder = {0};
    HSTRING caller_id = NULL, authority = NULL, login_hint = NULL;
    char *caller_utf8 = NULL, *scopes_utf8 = NULL, *authority_utf8 = NULL;
    char *login_hint_utf8 = NULL, *policy_utf8 = NULL;
    WCHAR caller_buffer[39];
    UINT32 caller_size, scopes_size, authority_size, login_hint_size, policy_size;
    HRESULT hr;

    if (!application_id || !reply) return E_POINTER;
    memset( reply, 0, sizeof(*reply) );
    if (!GetEnvironmentVariableW( L"SODA_IDENTITY_BRIDGE_SOCKET", NULL, 0 ))
        return identity_device_authorize( application_id, service, NULL, NULL, cancel, reply );
    if (StringFromGUID2( application_id, caller_buffer, ARRAY_SIZE(caller_buffer) ) != ARRAY_SIZE(caller_buffer))
        return E_INVALIDARG;
    if (FAILED(hr = WindowsCreateString( caller_buffer + 1, 36, &caller_id )) ||
        FAILED(hr = WindowsCreateString( L"common", 6, &authority )) ||
        FAILED(hr = WindowsCreateString( L"", 0, &login_hint )))
        goto done;
    if (FAILED(hr = hstring_to_utf8( caller_id, &caller_utf8, &caller_size )) ||
        FAILED(hr = hstring_to_utf8( service, &scopes_utf8, &scopes_size )) ||
        FAILED(hr = hstring_to_utf8( authority, &authority_utf8, &authority_size )) ||
        FAILED(hr = hstring_to_utf8( login_hint, &login_hint_utf8, &login_hint_size )) ||
        FAILED(hr = hstring_to_utf8( policy, &policy_utf8, &policy_size )) ||
        FAILED(hr = builder_add( &builder, FIELD_CALLER_ID, caller_utf8, caller_size )) ||
        FAILED(hr = builder_add( &builder, FIELD_SCOPES, scopes_utf8, scopes_size )) ||
        FAILED(hr = builder_add( &builder, FIELD_AUTHORITY, authority_utf8, authority_size )) ||
        FAILED(hr = builder_add( &builder, FIELD_LOGIN_HINT, login_hint_utf8, login_hint_size )) ||
        FAILED(hr = builder_add( &builder, FIELD_POLICY, policy_utf8, policy_size )))
        goto done;
    hr = bridge_exchange( OP_AUTHORIZE, &builder, cancel, reply );

done:
    if (FAILED(hr) && !reply->error) reply->error = hr;
    free( builder.data );
    free( caller_utf8 );
    free( scopes_utf8 );
    free( authority_utf8 );
    free( login_hint_utf8 );
    free( policy_utf8 );
    WindowsDeleteString( caller_id );
    WindowsDeleteString( authority );
    WindowsDeleteString( login_hint );
    return hr;
}
