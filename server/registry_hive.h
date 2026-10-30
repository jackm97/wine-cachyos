/*
 * Registry hive file support
 *
 * Copyright 2026 Mirko Brombin
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 */

#ifndef __WINE_SERVER_REGISTRY_HIVE_H
#define __WINE_SERVER_REGISTRY_HIVE_H

#include <stddef.h>
#include <stdint.h>

struct reg_hive_value
{
    uint16_t *name;
    size_t namelen;
    uint32_t type;
    unsigned char *data;
    size_t size;
};

struct reg_hive_key
{
    uint16_t *name, *class_name;
    size_t namelen, classlen;
    uint64_t modified;
    uint16_t flags;
    unsigned char *security;
    size_t security_size;
    struct reg_hive_value *values;
    size_t value_count;
    struct reg_hive_key **children;
    size_t child_count;
};

struct reg_hive_log
{
    const void *data;
    size_t size;
};

int reg_hive_set_sequence( void *data, size_t size, uint32_t primary, uint32_t secondary );
int reg_hive_log_encode( const void *data, size_t size, uint32_t sequence, void **log, size_t *log_size );
int reg_hive_recover( const void *data, size_t size, const struct reg_hive_log logs[2],
                      void **output, size_t *output_size, uint32_t *sequence );
int reg_hive_decode( const void *data, size_t size, struct reg_hive_key **root );
int reg_hive_encode( const struct reg_hive_key *root, void **data, size_t *size );
void reg_hive_free( struct reg_hive_key *key );

#endif
