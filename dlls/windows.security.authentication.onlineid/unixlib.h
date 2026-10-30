/* Host identity bridge Unix interface
 *
 * Copyright (C) 2026 Mirko Brombin
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 */

#include "wine/unixlib.h"

enum onlineid_unix_funcs
{
    unix_exchange,
    unix_credentials,
    unix_funcs_count,
};

struct exchange_params
{
    UINT64 request;
    UINT64 response;
    UINT64 cancel;
    UINT32 request_size;
    UINT32 response_capacity;
    UINT32 response_size;
    UINT32 reserved;
};

enum credential_operation
{
    credential_lookup,
    credential_store,
    credential_clear,
    credential_list,
};

struct credential_params
{
    UINT64 data;
    UINT32 capacity;
    UINT32 size;
    UINT32 operation;
    char client_id[37];
    char authority[37];
};

extern NTSTATUS credentials( void *args );
