/* Native identity credential storage
 *
 * Copyright (C) 2026 Mirko Brombin
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 */

#if 0
#pragma makedep unix
#endif

#include "config.h"

#include <dlfcn.h>
#include <pthread.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#ifdef SONAME_LIBSECRET_1
#include <libsecret/secret.h>
#endif

#include "ntstatus.h"
#define WIN32_NO_STATUS
#include "windef.h"
#include "winternl.h"
#include "unixlib.h"

#ifdef SONAME_LIBSECRET_1
static const SecretSchema identity_schema =
{
    .name = "org.winehq.Soda.Identity",
    .flags = SECRET_SCHEMA_NONE,
    .attributes =
    {
        {"prefix", SECRET_SCHEMA_ATTRIBUTE_STRING},
        {"client", SECRET_SCHEMA_ATTRIBUTE_STRING},
        {"authority", SECRET_SCHEMA_ATTRIBUTE_STRING},
        {NULL, 0},
    },
};

static pthread_once_t secret_once = PTHREAD_ONCE_INIT;
static BOOL secret_available;
static typeof(secret_password_lookup_sync) *lookup;
static typeof(secret_password_store_sync) *store;
static typeof(secret_password_clear_sync) *clear;
static typeof(secret_password_free) *release;
static typeof(g_error_free) *release_error;
static typeof(secret_password_search_sync) *search;
static typeof(secret_retrievable_get_attributes) *get_attributes;
static typeof(secret_retrievable_retrieve_secret_sync) *retrieve;
static typeof(secret_value_get_text) *get_text;
static typeof(secret_value_unref) *release_value;
static typeof(g_hash_table_lookup) *get_attribute;
static typeof(g_hash_table_unref) *release_attributes;
static typeof(g_list_free_full) *release_list;
static typeof(g_object_unref) *release_object;

static void load_secret(void)
{
    void *library = dlopen( SONAME_LIBSECRET_1, RTLD_NOW );

    if (!library) return;
    lookup = dlsym( library, "secret_password_lookup_sync" );
    store = dlsym( library, "secret_password_store_sync" );
    clear = dlsym( library, "secret_password_clear_sync" );
    release = dlsym( library, "secret_password_free" );
    release_error = dlsym( library, "g_error_free" );
    search = dlsym( library, "secret_password_search_sync" );
    get_attributes = dlsym( library, "secret_retrievable_get_attributes" );
    retrieve = dlsym( library, "secret_retrievable_retrieve_secret_sync" );
    get_text = dlsym( library, "secret_value_get_text" );
    release_value = dlsym( library, "secret_value_unref" );
    get_attribute = dlsym( library, "g_hash_table_lookup" );
    release_attributes = dlsym( library, "g_hash_table_unref" );
    release_list = dlsym( library, "g_list_free_full" );
    release_object = dlsym( library, "g_object_unref" );
    secret_available = lookup && store && clear && release && release_error;
    if (!secret_available) dlclose( library );
    /* GLib retains registered types, so a used library must remain loaded. */
}

static NTSTATUS list_credentials( struct credential_params *params, const char *prefix )
{
    GList *items, *item;
    GHashTable *attributes = NULL;
    SecretValue *value = NULL;
    GError *error = NULL;
    const char *client, *record, *stored_prefix, *authority;
    char *data = (void *)(uintptr_t)params->data;
    UINT32 count = 0, size, offset = sizeof(count);
    NTSTATUS status = STATUS_SUCCESS;

    if (!search || !get_attributes || !retrieve || !get_text || !release_value ||
        !get_attribute || !release_attributes || !release_list || !release_object) return STATUS_NOT_SUPPORTED;
    if (params->capacity < sizeof(count)) return STATUS_BUFFER_TOO_SMALL;
    if (params->client_id[0])
        items = search( &identity_schema, SECRET_SEARCH_ALL | SECRET_SEARCH_UNLOCK, NULL, &error,
                        "prefix", prefix, "authority", params->authority, "client", params->client_id, NULL );
    else
        items = search( &identity_schema, SECRET_SEARCH_ALL | SECRET_SEARCH_UNLOCK, NULL, &error,
                        "prefix", prefix, "authority", params->authority, NULL );
    if (error)
    {
        status = STATUS_ACCESS_DENIED;
        goto done;
    }
    for (item = items; item; item = item->next)
    {
        if (++count > 128)
        {
            status = STATUS_BUFFER_TOO_SMALL;
            break;
        }
        attributes = get_attributes( item->data );
        client = attributes ? get_attribute( attributes, "client" ) : NULL;
        stored_prefix = attributes ? get_attribute( attributes, "prefix" ) : NULL;
        authority = attributes ? get_attribute( attributes, "authority" ) : NULL;
        if (!client || strlen(client) != 36 || !stored_prefix || strcmp(stored_prefix, prefix) ||
            !authority || strcmp(authority, params->authority) ||
            (params->client_id[0] && strcmp(client, params->client_id)))
        {
            status = STATUS_INVALID_PARAMETER;
            break;
        }
        value = retrieve( item->data, NULL, &error );
        if (error || !value || !(record = get_text( value )))
        {
            status = STATUS_ACCESS_DENIED;
            break;
        }
        size = strnlen( record, params->capacity ) + 1;
        if (params->capacity - offset < 37 + sizeof(size) ||
            size > params->capacity - offset - 37 - sizeof(size))
        {
            status = STATUS_BUFFER_TOO_SMALL;
            break;
        }
        memcpy( data + offset, client, 37 );
        offset += 37;
        memcpy( data + offset, &size, sizeof(size) );
        offset += sizeof(size);
        memcpy( data + offset, record, size );
        offset += size;
        release_value( value );
        release_attributes( attributes );
        value = NULL;
        attributes = NULL;
    }
    if (!status)
    {
        memcpy( data, &count, sizeof(count) );
        params->size = offset;
    }

done:
    if (value) release_value( value );
    if (attributes) release_attributes( attributes );
    if (items) release_list( items, release_object );
    if (error) release_error( error );
    if (status) memset( data, 0, params->capacity );
    return status;
}
#endif

NTSTATUS credentials( void *args )
{
#ifdef SONAME_LIBSECRET_1
    struct credential_params *params = args;
    const char *prefix = getenv( "WINEPREFIX" ), *home = getenv( "HOME" );
    char *default_prefix = NULL, *canonical_prefix = NULL, *secret = NULL;
    char *data = (void *)(uintptr_t)params->data;
    GError *error = NULL;
    size_t size;
    NTSTATUS status = STATUS_NOT_SUPPORTED;

    params->size = 0;
    if (!memchr( params->client_id, 0, sizeof(params->client_id) ) ||
        (!params->client_id[0] && params->operation != credential_list) ||
        !memchr( params->authority, 0, sizeof(params->authority) ) || !params->authority[0] ||
        params->operation > credential_list) return STATUS_INVALID_PARAMETER;
    if (params->operation != credential_clear && (!data || !params->capacity || params->capacity > 4 * 1024 * 1024))
        return STATUS_INVALID_PARAMETER;
    if (params->operation == credential_store && (!data[0] || !memchr( data, 0, params->capacity )))
        return STATUS_INVALID_PARAMETER;
    if (!prefix)
    {
        if (!home || asprintf( &default_prefix, "%s/.wine", home ) < 0) return STATUS_NO_MEMORY;
        prefix = default_prefix;
    }
    if (!(canonical_prefix = realpath( prefix, NULL )))
    {
        status = STATUS_OBJECT_PATH_NOT_FOUND;
        goto done;
    }
    pthread_once( &secret_once, load_secret );
    if (!secret_available) goto done;
    switch (params->operation)
    {
    case credential_list:
        status = list_credentials( params, canonical_prefix );
        break;
    case credential_lookup:
        secret = lookup( &identity_schema, NULL, &error, "prefix", canonical_prefix,
                         "client", params->client_id, "authority", params->authority, NULL );
        if (error) status = STATUS_ACCESS_DENIED;
        else if (!secret) status = STATUS_OBJECT_NAME_NOT_FOUND;
        else
        {
            size = strlen( secret ) + 1;
            if (size > params->capacity) status = STATUS_BUFFER_TOO_SMALL;
            else
            {
                memcpy( data, secret, size );
                params->size = size;
                status = STATUS_SUCCESS;
            }
        }
        break;
    case credential_store:
        status = store( &identity_schema, SECRET_COLLECTION_DEFAULT, "Soda Microsoft identity", data,
                        NULL, &error, "prefix", canonical_prefix, "client", params->client_id,
                        "authority", params->authority, NULL ) ? STATUS_SUCCESS : STATUS_ACCESS_DENIED;
        break;
    case credential_clear:
        status = clear( &identity_schema, NULL, &error, "prefix", canonical_prefix,
                        "client", params->client_id, "authority", params->authority, NULL )
                 ? STATUS_SUCCESS : error ? STATUS_ACCESS_DENIED : STATUS_OBJECT_NAME_NOT_FOUND;
        break;
    }
    if (secret) release( secret );
    if (error) release_error( error );

done:
    free( canonical_prefix );
    free( default_prefix );
    return status;
#else
    return STATUS_NOT_SUPPORTED;
#endif
}
