/* Host identity bridge Unix interface
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

#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <time.h>
#include <unistd.h>

#include "ntstatus.h"
#define WIN32_NO_STATUS
#include "windef.h"
#include "winternl.h"

#include "unixlib.h"

static NTSTATUS errno_status( int error )
{
    switch (error)
    {
    case ENOENT: return STATUS_OBJECT_NAME_NOT_FOUND;
    case EACCES: return STATUS_ACCESS_DENIED;
    case ECONNREFUSED: return STATUS_CONNECTION_REFUSED;
    case ETIMEDOUT: return STATUS_IO_TIMEOUT;
    case ENOMEM: return STATUS_NO_MEMORY;
    default: return STATUS_UNSUCCESSFUL;
    }
}

static LONGLONG monotonic_ms(void)
{
    struct timespec now;
    clock_gettime( CLOCK_MONOTONIC, &now );
    return (LONGLONG)now.tv_sec * 1000 + now.tv_nsec / 1000000;
}

static NTSTATUS wait_ready( int fd, short events, HANDLE cancel, LONGLONG deadline )
{
    struct pollfd descriptor = {fd, events, 0};
    LARGE_INTEGER timeout = {0};
    int result;

    for (;;)
    {
        if (cancel && NtWaitForSingleObject( cancel, FALSE, &timeout ) == STATUS_SUCCESS) return STATUS_CANCELLED;
        if (monotonic_ms() >= deadline) return STATUS_IO_TIMEOUT;
        if ((result = poll( &descriptor, 1, 100 )) > 0) return STATUS_SUCCESS;
        if (result < 0 && errno != EINTR) return errno_status( errno );
    }
}

static NTSTATUS write_all( int fd, const void *data, size_t size, HANDLE cancel, LONGLONG deadline )
{
    const char *cursor = data;
    ssize_t count;
    NTSTATUS status;

    while (size)
    {
        if ((status = wait_ready( fd, POLLOUT, cancel, deadline ))) return status;
        if ((count = send( fd, cursor, size, MSG_NOSIGNAL )) < 0)
        {
            if (errno == EINTR || errno == EAGAIN) continue;
            return errno_status( errno );
        }
        if (!count) return STATUS_PIPE_BROKEN;
        cursor += count;
        size -= count;
    }
    return STATUS_SUCCESS;
}

static NTSTATUS read_all( int fd, void *data, size_t size, HANDLE cancel, LONGLONG deadline )
{
    char *cursor = data;
    ssize_t count;
    NTSTATUS status;

    while (size)
    {
        if ((status = wait_ready( fd, POLLIN, cancel, deadline ))) return status;
        if ((count = read( fd, cursor, size )) < 0)
        {
            if (errno == EINTR || errno == EAGAIN) continue;
            return errno_status( errno );
        }
        if (!count) return STATUS_PIPE_BROKEN;
        cursor += count;
        size -= count;
    }
    return STATUS_SUCCESS;
}

static NTSTATUS exchange( void *args )
{
    struct exchange_params *params = args;
    const char *socket_path = getenv( "SODA_IDENTITY_BRIDGE_SOCKET" );
    struct sockaddr_un address = {0};
    unsigned char *request = (void *)(uintptr_t)params->request;
    unsigned char *response = (void *)(uintptr_t)params->response;
    uint32_t payload_size;
    HANDLE cancel = (HANDLE)(uintptr_t)params->cancel;
    LONGLONG deadline = monotonic_ms() + 620000;
    NTSTATUS status;
    size_t path_length;
    int fd = -1, directory_fd = -1;

    params->response_size = 0;
    if (!socket_path || !*socket_path) return STATUS_OBJECT_NAME_NOT_FOUND;
    if (!request || !response || params->request_size < 20 || params->response_capacity < 20)
        return STATUS_INVALID_PARAMETER;
    path_length = strlen( socket_path );
    if (path_length < sizeof(address.sun_path))
        memcpy( address.sun_path, socket_path, path_length + 1 );
    else
    {
        const char *name = strrchr( socket_path, '/' );
        size_t directory_length;
        char *directory;

        if (!name || !name[1]) return STATUS_NAME_TOO_LONG;
        directory_length = name - socket_path;
        if (!directory_length) directory_length = 1;
        if (!(directory = malloc( directory_length + 1 ))) return STATUS_NO_MEMORY;
        memcpy( directory, socket_path, directory_length );
        directory[directory_length] = 0;
        directory_fd = open( directory, O_RDONLY | O_DIRECTORY | O_CLOEXEC | O_NOFOLLOW );
        free( directory );
        if (directory_fd < 0) return errno_status( errno );
        path_length = snprintf( address.sun_path, sizeof(address.sun_path),
                                "/proc/self/fd/%d/%s", directory_fd, name + 1 );
        if (path_length >= sizeof(address.sun_path))
        {
            status = STATUS_NAME_TOO_LONG;
            goto done;
        }
    }
    if ((fd = socket( AF_UNIX, SOCK_STREAM, 0 )) < 0)
    {
        status = errno_status( errno );
        goto done;
    }
    address.sun_family = AF_UNIX;
    if (fcntl( fd, F_SETFL, O_NONBLOCK ) < 0)
    {
        status = errno_status( errno );
        goto done;
    }
    if (connect( fd, (struct sockaddr *)&address, offsetof(struct sockaddr_un, sun_path) + path_length + 1 ) < 0)
    {
        int error = 0;
        socklen_t size = sizeof(error);
        if (errno != EINPROGRESS)
        {
            status = errno_status( errno );
            goto done;
        }
        if ((status = wait_ready( fd, POLLOUT, cancel, deadline ))) goto done;
        if (getsockopt( fd, SOL_SOCKET, SO_ERROR, &error, &size ) < 0 || error)
        {
            status = errno_status( error ? error : errno );
            goto done;
        }
    }
    if ((status = write_all( fd, request, params->request_size, cancel, deadline ))) goto done;
    if ((status = read_all( fd, response, 20, cancel, deadline ))) goto done;
    memcpy( &payload_size, response + 12, sizeof(payload_size) );
    if (payload_size > params->response_capacity - 20)
    {
        status = STATUS_BUFFER_TOO_SMALL;
        goto done;
    }
    if ((status = read_all( fd, response + 20, payload_size, cancel, deadline ))) goto done;
    params->response_size = 20 + payload_size;
    status = STATUS_SUCCESS;

done:
    if (fd != -1) close( fd );
    if (directory_fd != -1) close( directory_fd );
    return status;
}

const unixlib_entry_t __wine_unix_call_funcs[] =
{
    exchange,
    credentials,
};

C_ASSERT( ARRAY_SIZE(__wine_unix_call_funcs) == unix_funcs_count );
