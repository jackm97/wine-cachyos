/* Microsoft identity device authorization fallback
 *
 * Copyright (C) 2026 Mirko Brombin
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 */

#include "private.h"

#include <stdio.h>

#include "shellapi.h"
#include "wincrypt.h"
#include "winhttp.h"
#include "unixlib.h"
#define IDENTITY_HOST L"login.microsoftonline.com"
#define MAX_HTTP_RESPONSE (4 * 1024 * 1024)

static BOOL form_unreserved( BYTE value )
{
    return (value >= 'a' && value <= 'z') || (value >= 'A' && value <= 'Z') ||
           (value >= '0' && value <= '9') || value == '-' || value == '.' || value == '_' || value == '~';
}

static char *form_encode( const char *value )
{
    static const char hex[] = "0123456789ABCDEF";
    SIZE_T length = strlen( value ), i;
    char *encoded, *cursor;

    if (length > (SIZE_MAX - 1) / 3 || !(encoded = malloc( length * 3 + 1 ))) return NULL;
    cursor = encoded;
    for (i = 0; i < length; ++i)
    {
        BYTE ch = value[i];
        if (form_unreserved( ch )) *cursor++ = ch;
        else
        {
            *cursor++ = '%';
            *cursor++ = hex[ch >> 4];
            *cursor++ = hex[ch & 15];
        }
    }
    *cursor = 0;
    return encoded;
}

static HRESULT guid_to_utf8( const GUID *guid, char value[37] )
{
    static const GUID office_caller = {0x2b379600, 0xb42b, 0x4fe9,
                                      {0xa5, 0x9c, 0xa3, 0x12, 0xfb, 0x93, 0x49, 0x35}};
    WCHAR wide[39];
    unsigned int i;

    if (IsEqualGUID( guid, &office_caller ))
    {
        strcpy( value, "d3590ed6-52b3-4102-aeff-aad2292ab01c" );
        return S_OK;
    }
    if (StringFromGUID2( guid, wide, ARRAY_SIZE(wide) ) != 39) return E_INVALIDARG;
    if (!WideCharToMultiByte( CP_UTF8, 0, wide + 1, 36, value, 36, NULL, NULL ))
        return HRESULT_FROM_WIN32( GetLastError() );
    value[36] = 0;
    for (i = 0; i < 36; ++i)
        if (value[i] >= 'A' && value[i] <= 'F') value[i] += 'a' - 'A';
    return S_OK;
}

HRESULT identity_authority_tenant( HSTRING authority, WCHAR tenant[37] )
{
    static const WCHAR * const issuers[] = {L"https://login.microsoftonline.com/",
                                           L"https://login.windows.net/", L"https://login.microsoft.com/"};
    const WCHAR *value = WindowsGetStringRawBuffer( authority, NULL );
    WCHAR guid_string[39];
    GUID tenant_id;
    UINT32 length = WindowsGetStringLen( authority ), i, prefix;

    if (!length)
    {
        wcscpy( tenant, L"common" );
        return S_OK;
    }
    if (wcslen(value) != length) return E_INVALIDARG;
    for (i = 0; i < ARRAY_SIZE(issuers); ++i)
    {
        prefix = wcslen( issuers[i] );
        if (length > prefix && !_wcsnicmp( value, issuers[i], prefix ))
        {
            value += prefix;
            length -= prefix;
            if (value[length - 1] == '/') --length;
            break;
        }
    }
    if (!length || length > 36) return E_INVALIDARG;
    for (i = 0; i < length; ++i)
    {
        WCHAR ch = value[i];
        if (ch >= 'A' && ch <= 'Z') ch += 'a' - 'A';
        tenant[i] = ch;
    }
    tenant[length] = 0;
    if (wcscmp( tenant, L"common" ) && wcscmp( tenant, L"organizations" ) && wcscmp( tenant, L"consumers" ))
    {
        if (length != 36) return E_INVALIDARG;
        guid_string[0] = '{';
        memcpy( guid_string + 1, tenant, length * sizeof(*tenant) );
        guid_string[37] = '}';
        guid_string[38] = 0;
        if (FAILED(CLSIDFromString( guid_string, &tenant_id ))) return E_INVALIDARG;
    }
    return S_OK;
}

static HRESULT build_endpoint( HSTRING authority, const WCHAR *operation, WCHAR path[80] )
{
    WCHAR tenant[37];
    HRESULT hr;

    if (FAILED(hr = identity_authority_tenant( authority, tenant ))) return hr;
    swprintf( path, 80, L"/%s/oauth2/v2.0/%s", tenant, operation );
    return S_OK;
}

static HRESULT hstring_to_utf8( HSTRING string, char **out )
{
    const WCHAR *value;
    UINT32 length;
    int count;

    if (!out) return E_POINTER;
    *out = NULL;
    value = WindowsGetStringRawBuffer( string, &length );
    if (!(count = WideCharToMultiByte( CP_UTF8, WC_ERR_INVALID_CHARS, value, length, NULL, 0, NULL, NULL )) && length)
        return HRESULT_FROM_WIN32( GetLastError() );
    if (!(*out = malloc( count + 1 ))) return E_OUTOFMEMORY;
    if (count) WideCharToMultiByte( CP_UTF8, WC_ERR_INVALID_CHARS, value, length, *out, count, NULL, NULL );
    (*out)[count] = 0;
    return S_OK;
}

static BOOL cancelled( HANDLE cancel )
{
    return cancel && WaitForSingleObject( cancel, 0 ) == WAIT_OBJECT_0;
}

static void CALLBACK cancel_request( TP_CALLBACK_INSTANCE *instance, void *context, TP_WAIT *wait,
                                     TP_WAIT_RESULT result )
{
    HINTERNET request = InterlockedExchangePointer( context, NULL );
    if (request) WinHttpCloseHandle( request );
}

static HRESULT http_post( const WCHAR *path, const char *body, HANDLE cancel, char **response, DWORD *status )
{
    static const WCHAR headers[] = L"Content-Type: application/x-www-form-urlencoded\r\n";
    HINTERNET session = NULL, connection = NULL, request = NULL;
    HINTERNET guarded_request = NULL;
    TP_WAIT *wait = NULL;
    DWORD available, read, total = 0, capacity = 4096;
    char *buffer = NULL, *grown;
    HRESULT hr = E_FAIL;

    if (!response || !status) return E_POINTER;
    *response = NULL;
    *status = 0;
    if (cancelled( cancel )) return HRESULT_FROM_WIN32( ERROR_CANCELLED );
    if (!(session = WinHttpOpen( L"Soda Identity/1.0", WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,
                                 WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0 )) ||
        !(connection = WinHttpConnect( session, IDENTITY_HOST, INTERNET_DEFAULT_HTTPS_PORT, 0 )) ||
        !(request = WinHttpOpenRequest( connection, L"POST", path, NULL, WINHTTP_NO_REFERER,
                                        WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE )))
    {
        hr = HRESULT_FROM_WIN32( GetLastError() );
        goto done;
    }
    WinHttpSetTimeouts( request, 10000, 10000, 30000, 60000 );
    if (cancel)
    {
        guarded_request = request;
        if (!(wait = CreateThreadpoolWait( cancel_request, &guarded_request, NULL )))
        {
            hr = HRESULT_FROM_WIN32( GetLastError() );
            goto done;
        }
        SetThreadpoolWait( wait, cancel, NULL );
    }
    if (!WinHttpSendRequest( request, headers, -1, (void *)body, strlen(body), strlen(body), 0 ) ||
        !WinHttpReceiveResponse( request, NULL ))
    {
        hr = HRESULT_FROM_WIN32( GetLastError() );
        goto done;
    }
    available = sizeof(*status);
    if (!WinHttpQueryHeaders( request, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                              WINHTTP_HEADER_NAME_BY_INDEX, status, &available, WINHTTP_NO_HEADER_INDEX ))
    {
        hr = HRESULT_FROM_WIN32( GetLastError() );
        goto done;
    }
    if (!(buffer = malloc( capacity )))
    {
        hr = E_OUTOFMEMORY;
        goto done;
    }
    for (;;)
    {
        if (!WinHttpQueryDataAvailable( request, &available ))
        {
            hr = HRESULT_FROM_WIN32( GetLastError() );
            goto done;
        }
        if (!available) break;
        if (available > MAX_HTTP_RESPONSE - total)
        {
            hr = HRESULT_FROM_WIN32( ERROR_FILE_TOO_LARGE );
            goto done;
        }
        if (total + available + 1 > capacity)
        {
            capacity = total + available + 1;
            if (!(grown = realloc( buffer, capacity )))
            {
                hr = E_OUTOFMEMORY;
                goto done;
            }
            buffer = grown;
        }
        if (!WinHttpReadData( request, buffer + total, available, &read ))
        {
            hr = HRESULT_FROM_WIN32( GetLastError() );
            goto done;
        }
        total += read;
    }
    buffer[total] = 0;
    *response = buffer;
    buffer = NULL;
    hr = S_OK;

done:
    free( buffer );
    if (wait)
    {
        SetThreadpoolWait( wait, NULL, NULL );
        WaitForThreadpoolWaitCallbacks( wait, TRUE );
        CloseThreadpoolWait( wait );
        request = InterlockedExchangePointer( (void **)&guarded_request, NULL );
    }
    if (request) WinHttpCloseHandle( request );
    if (connection) WinHttpCloseHandle( connection );
    if (session) WinHttpCloseHandle( session );
    if (cancelled( cancel ))
    {
        free( *response );
        *response = NULL;
        hr = HRESULT_FROM_WIN32( ERROR_CANCELLED );
    }
    return hr;
}

static const char *skip_json_space( const char *cursor )
{
    while (*cursor == ' ' || *cursor == '\t' || *cursor == '\r' || *cursor == '\n') ++cursor;
    return cursor;
}

static BOOL json_hex_word( const char **cursor, WCHAR *value )
{
    const char *read = *cursor;
    unsigned int result = 0, digit, i;

    for (i = 0; i < 4; ++i)
    {
        char ch = *read++;
        if (ch >= '0' && ch <= '9') digit = ch - '0';
        else if (ch >= 'a' && ch <= 'f') digit = ch - 'a' + 10;
        else if (ch >= 'A' && ch <= 'F') digit = ch - 'A' + 10;
        else return FALSE;
        result = (result << 4) | digit;
    }
    *value = result;
    *cursor = read;
    return TRUE;
}

static HRESULT json_unicode( const char **cursor, char **out )
{
    const char *read = *cursor + 1;
    WCHAR wide[2];
    int length = 1, count;

    if (!json_hex_word( &read, &wide[0] ) || !wide[0]) return E_INVALIDARG;
    if (wide[0] >= 0xd800 && wide[0] <= 0xdbff)
    {
        if (strncmp( read, "\\u", 2 )) return E_INVALIDARG;
        read += 2;
        if (!json_hex_word( &read, &wide[1] ) || wide[1] < 0xdc00 || wide[1] > 0xdfff) return E_INVALIDARG;
        length = 2;
    }
    else if (wide[0] >= 0xdc00 && wide[0] <= 0xdfff) return E_INVALIDARG;
    if (!(count = WideCharToMultiByte( CP_UTF8, WC_ERR_INVALID_CHARS, wide, length, *out, 4, NULL, NULL )))
        return HRESULT_FROM_WIN32( GetLastError() );
    *out += count;
    *cursor = read;
    return S_OK;
}

static HRESULT json_string( const char *json, const char *name, char **out )
{
    SIZE_T name_length = strlen( name ), capacity;
    const char *cursor = json, *start;
    char *value, *write;
    HRESULT hr;

    if (!out) return E_POINTER;
    *out = NULL;
    while ((cursor = strchr( cursor, '"' )))
    {
        ++cursor;
        if (!strncmp( cursor, name, name_length ) && cursor[name_length] == '"')
        {
            cursor = skip_json_space( cursor + name_length + 1 );
            if (*cursor++ != ':') return E_INVALIDARG;
            cursor = skip_json_space( cursor );
            if (*cursor++ != '"') return E_INVALIDARG;
            start = cursor;
            capacity = strlen( start ) + 1;
            if (!(value = malloc( capacity ))) return E_OUTOFMEMORY;
            write = value;
            while (*cursor && *cursor != '"')
            {
                if (*cursor == '\\')
                {
                    ++cursor;
                    if (*cursor == 'n') *write++ = '\n';
                    else if (*cursor == 'r') *write++ = '\r';
                    else if (*cursor == 't') *write++ = '\t';
                    else if (*cursor == 'b') *write++ = '\b';
                    else if (*cursor == 'f') *write++ = '\f';
                    else if (*cursor == 'u')
                    {
                        if (FAILED(hr = json_unicode( &cursor, &write )))
                        {
                            free( value );
                            return hr;
                        }
                        continue;
                    }
                    else if (*cursor == '"' || *cursor == '\\' || *cursor == '/') *write++ = *cursor;
                    else
                    {
                        free( value );
                        return E_INVALIDARG;
                    }
                    if (*cursor) ++cursor;
                }
                else if ((unsigned char)*cursor < 0x20)
                {
                    free( value );
                    return E_INVALIDARG;
                }
                else
                    *write++ = *cursor++;
            }
            if (*cursor != '"')
            {
                free( value );
                return E_INVALIDARG;
            }
            *write = 0;
            *out = value;
            return S_OK;
        }
        cursor += strcspn( cursor, "\"" );
    }
    return HRESULT_FROM_WIN32( ERROR_NOT_FOUND );
}

static HRESULT json_uint( const char *json, const char *name, UINT32 *out )
{
    SIZE_T name_length = strlen( name );
    const char *cursor = json;
    char *end;
    unsigned long value;

    if (!out) return E_POINTER;
    while ((cursor = strchr( cursor, '"' )))
    {
        ++cursor;
        if (!strncmp( cursor, name, name_length ) && cursor[name_length] == '"')
        {
            cursor = skip_json_space( cursor + name_length + 1 );
            if (*cursor++ != ':') return E_INVALIDARG;
            cursor = skip_json_space( cursor );
            if (*cursor < '0' || *cursor > '9') return E_INVALIDARG;
            value = strtoul( cursor, &end, 10 );
            if (end == cursor || value > UINT_MAX ||
                (*skip_json_space(end) != ',' && *skip_json_space(end) != '}')) return E_INVALIDARG;
            *out = value;
            return S_OK;
        }
        cursor += strcspn( cursor, "\"" );
    }
    return HRESULT_FROM_WIN32( ERROR_NOT_FOUND );
}

static HRESULT utf8_to_hstring( const char *value, HSTRING *out )
{
    WCHAR *wide;
    int count;
    HRESULT hr;

    if (!value || !out) return E_POINTER;
    if (!(count = MultiByteToWideChar( CP_UTF8, MB_ERR_INVALID_CHARS, value, -1, NULL, 0 )))
        return HRESULT_FROM_WIN32( GetLastError() );
    if (!(wide = malloc( count * sizeof(*wide) ))) return E_OUTOFMEMORY;
    MultiByteToWideChar( CP_UTF8, MB_ERR_INVALID_CHARS, value, -1, wide, count );
    hr = WindowsCreateString( wide, count - 1, out );
    free( wide );
    return hr;
}

static INIT_ONCE prompt_once = INIT_ONCE_STATIC_INIT;
static DWORD prompt_tls;

static BOOL CALLBACK prompt_initialize( INIT_ONCE *once, void *param, void **context )
{
    return (prompt_tls = TlsAlloc()) != TLS_OUT_OF_INDEXES;
}

static void CALLBACK prompt_timer( HWND window, UINT message, UINT_PTR timer, DWORD time )
{
    if (cancelled( GetPropW( window, L"SodaIdentityCancel" ) )) EndDialog( window, IDCANCEL );
}

static LRESULT CALLBACK prompt_hook( int code, WPARAM wparam, LPARAM lparam )
{
    HANDLE cancel = TlsGetValue( prompt_tls );
    if (code == HCBT_ACTIVATE && cancel)
    {
        SetPropW( (HWND)wparam, L"SodaIdentityCancel", cancel );
        SetTimer( (HWND)wparam, 1, 100, prompt_timer );
    }
    return CallNextHookEx( NULL, code, wparam, lparam );
}

static HRESULT show_device_prompt( const char *verification_uri, const char *user_code, HANDLE cancel )
{
    WCHAR uri[1024], code[128], message[1400];
    HHOOK hook = NULL;
    int result;

    if (!MultiByteToWideChar( CP_UTF8, MB_ERR_INVALID_CHARS, verification_uri, -1, uri, ARRAY_SIZE(uri) ) ||
        !MultiByteToWideChar( CP_UTF8, MB_ERR_INVALID_CHARS, user_code, -1, code, ARRAY_SIZE(code) ))
        return HRESULT_FROM_WIN32( GetLastError() );
    swprintf( message, ARRAY_SIZE(message),
              L"Sign in to Microsoft 365 in your browser.\n\nCode: %s\n\nThe browser will open after you continue.", code );
    if (cancelled( cancel )) return HRESULT_FROM_WIN32( ERROR_CANCELLED );
    if (cancel)
    {
        if (!InitOnceExecuteOnce( &prompt_once, prompt_initialize, NULL, NULL ) || !TlsSetValue( prompt_tls, cancel ))
            return HRESULT_FROM_WIN32( GetLastError() );
        if (!(hook = SetWindowsHookExW( WH_CBT, prompt_hook, NULL, GetCurrentThreadId() )))
        {
            TlsSetValue( prompt_tls, NULL );
            return HRESULT_FROM_WIN32( GetLastError() );
        }
    }
    result = MessageBoxW( NULL, message, L"Microsoft 365 sign-in", MB_OKCANCEL | MB_ICONINFORMATION );
    if (hook) UnhookWindowsHookEx( hook );
    if (cancel) TlsSetValue( prompt_tls, NULL );
    if (result != IDOK || cancelled( cancel ))
        return HRESULT_FROM_WIN32( ERROR_CANCELLED );
    if ((INT_PTR)ShellExecuteW( NULL, L"open", uri, NULL, NULL, SW_SHOWNORMAL ) <= 32)
        return HRESULT_FROM_WIN32( ERROR_OPEN_FAILED );
    return S_OK;
}

static HRESULT read_account( const char *id_token, const char *client_id, struct identity_reply *reply )
{
    const char *start = strchr( id_token, '.' ), *end;
    char *encoded = NULL, *claims = NULL, *audience = NULL, *account = NULL, *username = NULL, *name = NULL, *tenant = NULL;
    SIZE_T length, size, i;
    DWORD decoded_size;
    HRESULT hr = E_INVALIDARG;

    if (!start || !(end = strchr( ++start, '.' )) || strchr( end + 1, '.' ) || !end[1]) return hr;
    length = end - start;
    if (!length || length > MAX_HTTP_RESPONSE || length % 4 == 1) return hr;
    size = (length + 3) & ~(SIZE_T)3;
    if (!(encoded = malloc( size + 1 ))) return E_OUTOFMEMORY;
    for (i = 0; i < length; ++i)
    {
        char ch = start[i];
        if (ch == '-') ch = '+';
        else if (ch == '_') ch = '/';
        else if (!((ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') || (ch >= '0' && ch <= '9')))
            goto done;
        encoded[i] = ch;
    }
    while (i < size) encoded[i++] = '=';
    encoded[size] = 0;
    if (!CryptStringToBinaryA( encoded, size, CRYPT_STRING_BASE64, NULL, &decoded_size, NULL, NULL )) goto done;
    if (!(claims = malloc( decoded_size + 1 )))
    {
        hr = E_OUTOFMEMORY;
        goto done;
    }
    if (!CryptStringToBinaryA( encoded, size, CRYPT_STRING_BASE64, (BYTE *)claims, &decoded_size, NULL, NULL ) ||
        memchr( claims, 0, decoded_size )) goto done;
    claims[decoded_size] = 0;
    if (FAILED(hr = json_string( claims, "aud", &audience ))) goto done;
    if (strcmp( audience, client_id ))
    {
        hr = E_ACCESSDENIED;
        goto done;
    }
    hr = json_string( claims, "oid", &account );
    if (hr == HRESULT_FROM_WIN32( ERROR_NOT_FOUND )) hr = json_string( claims, "sub", &account );
    if (FAILED(hr)) goto done;
    hr = json_string( claims, "preferred_username", &username );
    if (hr == HRESULT_FROM_WIN32( ERROR_NOT_FOUND )) hr = json_string( claims, "email", &username );
    if (FAILED(hr)) goto done;
    if (!*account || !*username)
    {
        hr = E_INVALIDARG;
        goto done;
    }
    hr = json_string( claims, "name", &name );
    if (FAILED(hr) && hr != HRESULT_FROM_WIN32( ERROR_NOT_FOUND )) goto done;
    hr = json_string( claims, "tid", &tenant );
    if (FAILED(hr) && hr != HRESULT_FROM_WIN32( ERROR_NOT_FOUND )) goto done;
    if (FAILED(hr = utf8_to_hstring( account, &reply->account_id )) ||
        FAILED(hr = utf8_to_hstring( username, &reply->username )) ||
        FAILED(hr = utf8_to_hstring( name && *name ? name : username, &reply->display_name ))) goto done;
    if (tenant) hr = utf8_to_hstring( tenant, &reply->tenant_id );

done:
    free( encoded );
    free( claims );
    free( audience );
    free( account );
    free( username );
    free( name );
    free( tenant );
    return hr;
}

HRESULT identity_device_list_accounts( HSTRING client_id, HSTRING authority, HANDLE cancel,
                                       struct identity_reply *reply )
{
    struct identity_reply account = {0};
    HSTRING ids[128] = {0};
    WCHAR guid_string[39], path[80];
    GUID application_id;
    char client[37] = "", *records = NULL, *cursor, *id, *username = NULL, *account_id = NULL;
    BYTE *data;
    DWORD size = 0, remaining, record_size, count, i, j, id_size, username_size, output_size = sizeof(count);
    UINT32 length, returned = 0;
    INT32 comparison;
    HRESULT hr;

    if (!reply) return E_POINTER;
    memset( reply, 0, sizeof(*reply) );
    if (cancelled( cancel )) return reply->error = HRESULT_FROM_WIN32( ERROR_CANCELLED );
    if (FAILED(hr = build_endpoint( authority, L"token", path ))) goto done;
    if (WindowsGetStringLen( client_id ))
    {
        const WCHAR *value = WindowsGetStringRawBuffer( client_id, &length );
        if (length != 36 || wcslen(value) != length)
        {
            hr = E_INVALIDARG;
            goto done;
        }
        swprintf( guid_string, ARRAY_SIZE(guid_string), L"{%s}", value );
        if (FAILED(hr = CLSIDFromString( guid_string, &application_id )) ||
            FAILED(hr = guid_to_utf8( &application_id, client ))) goto done;
    }
    if (!(records = calloc( 1, MAX_HTTP_RESPONSE )))
    {
        hr = E_OUTOFMEMORY;
        goto done;
    }
    if (FAILED(hr = identity_native_credentials( credential_list, client, authority,
                                                 records, MAX_HTTP_RESPONSE, &size ))) goto done;
    if (size < sizeof(count) || size > MAX_HTTP_RESPONSE)
    {
        hr = E_INVALIDARG;
        goto done;
    }
    memcpy( &count, records, sizeof(count) );
    cursor = records + sizeof(count);
    remaining = size - sizeof(count);
    if (count > ARRAY_SIZE(ids))
    {
        hr = E_INVALIDARG;
        goto done;
    }
    if (!(reply->accounts = malloc( output_size )))
    {
        hr = E_OUTOFMEMORY;
        goto done;
    }
    for (i = 0; i < count; ++i)
    {
        if (cancelled( cancel ))
        {
            hr = HRESULT_FROM_WIN32( ERROR_CANCELLED );
            goto done;
        }
        if (remaining < 37 + sizeof(record_size) || cursor[36] || memchr(cursor, 0, 36) ||
            (client[0] && strcmp(client, cursor)))
        {
            hr = E_INVALIDARG;
            goto done;
        }
        for (j = 0; j < 36; ++j) guid_string[j + 1] = (BYTE)cursor[j];
        guid_string[0] = '{';
        guid_string[37] = '}';
        guid_string[38] = 0;
        if (FAILED(CLSIDFromString( guid_string, &application_id )))
        {
            hr = E_INVALIDARG;
            goto done;
        }
        memcpy( &record_size, cursor + 37, sizeof(record_size) );
        remaining -= 37 + sizeof(record_size);
        if (record_size < 5 || record_size > remaining || cursor[41 + record_size - 1] ||
            memchr( cursor + 41, 0, record_size - 1 ) || strncmp(cursor + 41, "1\n", 2) ||
            !(id = strchr(cursor + 43, '\n')) || id == cursor + 43 || !id[1] || strchr(id + 1, '\n'))
        {
            hr = E_INVALIDARG;
            goto done;
        }
        if (FAILED(hr = read_account( id + 1, cursor, &account ))) goto done;
        cursor += 41 + record_size;
        remaining -= record_size;
        for (j = 0; j < returned; ++j)
        {
            if (FAILED(hr = WindowsCompareStringOrdinal( ids[j], account.account_id, &comparison ))) goto done;
            if (!comparison) break;
        }
        if (j == returned)
        {
            if (FAILED(hr = hstring_to_utf8( account.account_id, &account_id )) ||
                FAILED(hr = hstring_to_utf8( account.username, &username ))) goto done;
            id_size = strlen( account_id );
            username_size = strlen( username );
            if (!id_size || id_size > 4096 || !username_size || username_size > 4096)
            {
                hr = E_INVALIDARG;
                goto done;
            }
            if (!(data = realloc( reply->accounts, output_size + 2 * sizeof(count) + id_size + username_size )))
            {
                hr = E_OUTOFMEMORY;
                goto done;
            }
            reply->accounts = data;
            memcpy( data + output_size, &id_size, sizeof(id_size) );
            output_size += sizeof(id_size);
            memcpy( data + output_size, account_id, id_size );
            output_size += id_size;
            memcpy( data + output_size, &username_size, sizeof(username_size) );
            output_size += sizeof(username_size);
            memcpy( data + output_size, username, username_size );
            output_size += username_size;
            ids[returned++] = account.account_id;
            account.account_id = NULL;
            free( account_id );
            free( username );
            account_id = username = NULL;
        }
        identity_reply_clear( &account );
    }
    hr = remaining ? E_INVALIDARG : cancelled( cancel ) ? HRESULT_FROM_WIN32( ERROR_CANCELLED ) : S_OK;
    if (SUCCEEDED(hr))
    {
        memcpy( reply->accounts, &returned, sizeof(returned) );
        reply->accounts_size = output_size;
    }

done:
    identity_reply_clear( &account );
    for (i = 0; i < returned; ++i) WindowsDeleteString( ids[i] );
    free( account_id );
    free( username );
    if (records) SecureZeroMemory( records, MAX_HTTP_RESPONSE );
    free( records );
    if (FAILED(hr))
    {
        identity_reply_clear( reply );
        reply->error = hr;
    }
    return hr;
}

static HRESULT build_scopes( HSTRING service, char **out )
{
    static const char defaults[] = "openid profile email offline_access";
    static const char licensing_ticket[] = "service::https://licensing.m365.svc.cloud.microsoft/::MBI_SSL_SHORT";
    static const char licensing_scope[] = "https://licensing.m365.svc.cloud.microsoft/.default";
    char *requested = NULL;
    const char *scope;
    SIZE_T size;
    HRESULT hr;

    if (FAILED(hr = hstring_to_utf8( service, &requested ))) return hr;
    if (!strcmp( requested, "https://events.data.microsoft.com/OneCollector/1.0/" ))
    {
        free( requested );
        return E_INVALIDARG;
    }
    scope = !strcmp( requested, licensing_ticket ) ? licensing_scope : requested;
    size = sizeof(defaults) + strlen( scope ) + 1;
    if (!(*out = malloc( size )))
    {
        free( requested );
        return E_OUTOFMEMORY;
    }
    strcpy( *out, defaults );
    if (*scope)
    {
        strcat( *out, " " );
        strcat( *out, scope );
    }
    free( requested );
    return S_OK;
}

static HRESULT save_native_tokens( const char *response, const char *old_refresh, const char *old_id,
                                   const char *client_id, HSTRING authority )
{
    char *refresh = NULL, *id = NULL, *record = NULL;
    SIZE_T size;
    HRESULT hr;

    hr = json_string( response, "refresh_token", &refresh );
    if (hr == HRESULT_FROM_WIN32( ERROR_NOT_FOUND ) && old_refresh)
        hr = (refresh = strdup(old_refresh)) ? S_OK : E_OUTOFMEMORY;
    if (FAILED(hr)) goto done;
    hr = json_string( response, "id_token", &id );
    if (hr == HRESULT_FROM_WIN32( ERROR_NOT_FOUND ) && old_id)
        hr = (id = strdup(old_id)) ? S_OK : E_OUTOFMEMORY;
    if (FAILED(hr)) goto done;
    if (!*refresh || !*id || strchr(refresh, '\n') || strchr(id, '\n'))
    {
        hr = E_INVALIDARG;
        goto done;
    }
    size = strlen(refresh) + strlen(id) + 4;
    if (size > MAX_HTTP_RESPONSE)
    {
        hr = E_INVALIDARG;
        goto done;
    }
    if (!(record = malloc(size)))
    {
        hr = E_OUTOFMEMORY;
        goto done;
    }
    snprintf( record, size, "1\n%s\n%s", refresh, id );
    hr = identity_native_credentials( credential_store, client_id, authority, record, size, NULL );

done:
    if (refresh) SecureZeroMemory( refresh, strlen(refresh) );
    if (record) SecureZeroMemory( record, strlen(record) );
    free( refresh );
    free( id );
    free( record );
    return hr;
}

static HRESULT token_reply( const char *response, const char *old_id, const char *client_id,
                            struct identity_reply *reply )
{
    char *access = NULL, *id = NULL, *scopes = NULL, *client_info = NULL;
    FILETIME now;
    UINT64 seconds;
    UINT32 lifetime;
    HRESULT hr;

    if (FAILED(hr = json_uint( response, "expires_in", &lifetime ))) goto done;
    if (!lifetime)
    {
        hr = E_INVALIDARG;
        goto done;
    }
    if (FAILED(hr = json_string( response, "access_token", &access ))) goto done;
    hr = json_string( response, "id_token", &id );
    if (hr == HRESULT_FROM_WIN32( ERROR_NOT_FOUND ) && old_id)
        hr = (id = strdup(old_id)) ? S_OK : E_OUTOFMEMORY;
    if (FAILED(hr)) goto done;
    if (!*access || !*id)
    {
        hr = E_INVALIDARG;
        goto done;
    }
    if (FAILED(hr = utf8_to_hstring( access, &reply->access_token )) ||
        FAILED(hr = utf8_to_hstring( id, &reply->id_token )) ||
        FAILED(hr = read_account( id, client_id, reply ))) goto done;
    hr = json_string( response, "scope", &scopes );
    if (FAILED(hr) && hr != HRESULT_FROM_WIN32( ERROR_NOT_FOUND )) goto done;
    hr = json_string( response, "client_info", &client_info );
    if (FAILED(hr) && hr != HRESULT_FROM_WIN32( ERROR_NOT_FOUND )) goto done;
    if (scopes && FAILED(hr = utf8_to_hstring( scopes, &reply->scopes ))) goto done;
    if (client_info && FAILED(hr = utf8_to_hstring( client_info, &reply->client_info ))) goto done;
    GetSystemTimeAsFileTime( &now );
    seconds = (((UINT64)now.dwHighDateTime << 32) | now.dwLowDateTime) / 10000000;
    if (seconds < 11644473600)
    {
        hr = E_INVALIDARG;
        goto done;
    }
    reply->expires_at = seconds - 11644473600 + lifetime;
    hr = S_OK;

done:
    if (access) SecureZeroMemory( access, strlen(access) );
    free( access );
    free( id );
    free( scopes );
    free( client_info );
    return hr;
}

HRESULT identity_device_get_token( const GUID *application_id, HSTRING service, HSTRING authority,
                                   HSTRING account_id, HSTRING login_hint, HANDLE cancel, struct identity_reply *reply )
{
    char client_id[37], *record = NULL, *refresh, *id, *encoded_refresh = NULL, *encoded_scopes = NULL;
    char *scopes = NULL, *body = NULL, *response = NULL;
    WCHAR path[80];
    struct identity_reply previous = {0};
    DWORD size, status;
    INT32 comparison;
    HRESULT hr;

    if (!application_id || !reply) return E_POINTER;
    memset( reply, 0, sizeof(*reply) );
    if (cancelled( cancel )) return reply->error = HRESULT_FROM_WIN32( ERROR_CANCELLED );
    if (FAILED(hr = guid_to_utf8( application_id, client_id )) ||
        FAILED(hr = build_endpoint( authority, L"token", path )) ||
        FAILED(hr = build_scopes( service, &scopes ))) goto done;
    if (!(record = calloc( 1, MAX_HTTP_RESPONSE )))
    {
        hr = E_OUTOFMEMORY;
        goto done;
    }
    if (FAILED(hr = identity_native_credentials( credential_lookup, client_id, authority,
                                                 record, MAX_HTTP_RESPONSE, &size ))) goto done;
    if (size < 5 || size > MAX_HTTP_RESPONSE || record[size - 1] || strncmp(record, "1\n", 2) ||
        !(id = strchr( record + 2, '\n' )))
    {
        hr = E_INVALIDARG;
        goto done;
    }
    refresh = record + 2;
    *id++ = 0;
    if (!*refresh || !*id || strchr(id, '\n') || FAILED(hr = read_account( id, client_id, &previous )))
    {
        hr = E_INVALIDARG;
        goto done;
    }
    if (WindowsGetStringLen(account_id))
    {
        hr = WindowsCompareStringOrdinal( account_id, previous.account_id, &comparison );
        if (FAILED(hr)) goto done;
        if (comparison)
        {
            hr = E_ACCESSDENIED;
            goto done;
        }
    }
    if (WindowsGetStringLen(login_hint) &&
        CompareStringOrdinal( WindowsGetStringRawBuffer(login_hint, NULL), WindowsGetStringLen(login_hint),
                              WindowsGetStringRawBuffer(previous.username, NULL), WindowsGetStringLen(previous.username),
                              TRUE ) != CSTR_EQUAL)
    {
        hr = E_ACCESSDENIED;
        goto done;
    }
    if (!(encoded_refresh = form_encode(refresh)) || !(encoded_scopes = form_encode(scopes)))
    {
        hr = E_OUTOFMEMORY;
        goto done;
    }
    size = strlen(encoded_refresh) + strlen(encoded_scopes) + strlen(client_id) + 80;
    if (!(body = malloc(size)))
    {
        hr = E_OUTOFMEMORY;
        goto done;
    }
    snprintf( body, size, "grant_type=refresh_token&client_info=1&client_id=%s&refresh_token=%s&scope=%s",
              client_id, encoded_refresh, encoded_scopes );
    if (FAILED(hr = http_post( path, body, cancel, &response, &status ))) goto done;
    if (status != 200)
    {
        hr = E_ACCESSDENIED;
        goto done;
    }
    if (FAILED(hr = token_reply( response, id, client_id, reply )) ||
        FAILED(hr = WindowsCompareStringOrdinal( previous.account_id, reply->account_id, &comparison ))) goto done;
    if (!reply->scopes && FAILED(hr = utf8_to_hstring( scopes, &reply->scopes ))) goto done;
    if (comparison)
    {
        hr = E_ACCESSDENIED;
        goto done;
    }
    hr = cancelled( cancel ) ? HRESULT_FROM_WIN32( ERROR_CANCELLED ) :
                              save_native_tokens( response, refresh, id, client_id, authority );

done:
    identity_reply_clear( &previous );
    if (FAILED(hr))
    {
        identity_reply_clear( reply );
        reply->error = hr;
    }
    if (record) SecureZeroMemory( record, MAX_HTTP_RESPONSE );
    if (encoded_refresh) SecureZeroMemory( encoded_refresh, strlen(encoded_refresh) );
    if (body) SecureZeroMemory( body, strlen(body) );
    if (response) SecureZeroMemory( response, strlen(response) );
    free( record );
    free( encoded_refresh );
    free( encoded_scopes );
    free( scopes );
    free( body );
    free( response );
    return hr;
}

HRESULT identity_device_authorize( const GUID *application_id, HSTRING service, HSTRING authority, HSTRING login_hint,
                                   BOOL force, HANDLE cancel, struct identity_reply *reply )
{
    char client_id[37], *scopes = NULL, *encoded_scopes = NULL, *body = NULL, *response = NULL;
    char *device_code = NULL, *encoded_code = NULL, *user_code = NULL, *verification_uri = NULL;
    char *error = NULL;
    UINT32 expires = 900, interval = 5, elapsed = 0;
    DWORD status;
    SIZE_T size;
    HRESULT hr = E_FAIL;
    WCHAR device_path[80], token_path[80];

    if (!application_id || !reply) return E_POINTER;
    memset( reply, 0, sizeof(*reply) );
    if (!force)
        hr = identity_device_get_token( application_id, service, authority, NULL, login_hint, cancel, reply );
    if (SUCCEEDED(hr)) return S_OK;
    if (hr == HRESULT_FROM_WIN32( ERROR_SERVICE_NOT_ACTIVE ))
    {
        static const WCHAR message[] = L"Secure credential storage is unavailable. Start or unlock a Secret Service-compatible keyring and try again.";
        WindowsCreateString( message, ARRAY_SIZE(message) - 1, &reply->error_message );
        return hr;
    }
    identity_reply_clear( reply );
    if (cancelled( cancel )) return reply->error = HRESULT_FROM_WIN32( ERROR_CANCELLED );
    if (FAILED(hr = guid_to_utf8( application_id, client_id )) ||
        FAILED(hr = build_endpoint( authority, L"devicecode", device_path )) ||
        FAILED(hr = build_endpoint( authority, L"token", token_path )) ||
        FAILED(hr = build_scopes( service, &scopes )))
        goto done;
    if (!(encoded_scopes = form_encode( scopes )))
    {
        hr = E_OUTOFMEMORY;
        goto done;
    }
    size = strlen( client_id ) + strlen( encoded_scopes ) + 32;
    if (!(body = malloc( size )))
    {
        hr = E_OUTOFMEMORY;
        goto done;
    }
    snprintf( body, size, "client_id=%s&scope=%s", client_id, encoded_scopes );
    if (FAILED(hr = http_post( device_path, body, cancel, &response, &status ))) goto done;
    if (status != 200)
    {
        hr = E_ACCESSDENIED;
        goto done;
    }
    if (FAILED(hr = json_string( response, "device_code", &device_code )) ||
        FAILED(hr = json_string( response, "user_code", &user_code )) ||
        FAILED(hr = json_string( response, "verification_uri", &verification_uri )))
        goto done;
    json_uint( response, "expires_in", &expires );
    json_uint( response, "interval", &interval );
    expires = min( max( expires, 60 ), 3600 );
    interval = min( max( interval, 1 ), 60 );
    if (cancelled( cancel ))
    {
        hr = HRESULT_FROM_WIN32( ERROR_CANCELLED );
        goto done;
    }
    if (FAILED(hr = show_device_prompt( verification_uri, user_code, cancel ))) goto done;
    if (!(encoded_code = form_encode( device_code )))
    {
        hr = E_OUTOFMEMORY;
        goto done;
    }
    size = strlen( client_id ) + strlen( encoded_code ) + 112;
    free( body );
    if (!(body = malloc( size )))
    {
        hr = E_OUTOFMEMORY;
        goto done;
    }
    snprintf( body, size, "grant_type=urn%%3Aietf%%3Aparams%%3Aoauth%%3Agrant-type%%3Adevice_code&client_info=1&client_id=%s&device_code=%s",
              client_id, encoded_code );
    do
    {
        if (cancel ? WaitForSingleObject( cancel, interval * 1000 ) == WAIT_OBJECT_0 :
                     (Sleep( interval * 1000 ), FALSE))
        {
            hr = HRESULT_FROM_WIN32( ERROR_CANCELLED );
            goto done;
        }
        elapsed += interval;
        free( response );
        response = NULL;
        free( error );
        error = NULL;
        if (FAILED(hr = http_post( token_path, body, cancel, &response, &status ))) goto done;
        if (status == 200) break;
        if (FAILED(json_string( response, "error", &error )))
        {
            hr = E_ACCESSDENIED;
            goto done;
        }
        if (!strcmp( error, "slow_down" )) interval += 5;
        else if (strcmp( error, "authorization_pending" ))
        {
            hr = !strcmp( error, "authorization_declined" ) ? HRESULT_FROM_WIN32( ERROR_CANCELLED ) : E_ACCESSDENIED;
            goto done;
        }
    } while (elapsed < expires);
    if (status != 200)
    {
        hr = HRESULT_FROM_WIN32( ERROR_TIMEOUT );
        goto done;
    }
    if (cancelled( cancel ))
    {
        hr = HRESULT_FROM_WIN32( ERROR_CANCELLED );
        goto done;
    }
    if (FAILED(hr = token_reply( response, NULL, client_id, reply ))) goto done;
    if (!reply->scopes && FAILED(hr = utf8_to_hstring( scopes, &reply->scopes ))) goto done;
    if (FAILED(hr = save_native_tokens( response, NULL, NULL, client_id, authority ))) goto done;
    reply->error = S_OK;
    hr = S_OK;

done:
    if (FAILED(hr))
    {
        identity_reply_clear( reply );
        reply->error = hr;
    }
    free( scopes );
    free( encoded_scopes );
    free( body );
    free( response );
    free( device_code );
    free( encoded_code );
    free( user_code );
    free( verification_uri );
    free( error );
    return hr;
}
