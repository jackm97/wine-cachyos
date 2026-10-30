/*
 * Copyright 2026 Mirko Brombin
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA
 */

#include "private.h"

struct ink_handler
{
    struct list entry;
    LONG ref;
    UINT id;
    EventRegistrationToken token;
    IUnknown *handler;
};

static void handler_release(struct ink_handler *handler)
{
    if (!InterlockedDecrement(&handler->ref))
    {
        IUnknown_Release(handler->handler);
        free(handler);
    }
}

void ink_events_init(struct ink_events *events)
{
    InitializeCriticalSection(&events->lock);
    list_init(&events->handlers);
    events->next_token = 0;
    events->closed = FALSE;
}

void ink_events_clear(struct ink_events *events)
{
    struct list handlers = LIST_INIT(handlers);
    struct ink_handler *handler, *next;

    EnterCriticalSection(&events->lock);
    events->closed = TRUE;
    list_move_tail(&handlers, &events->handlers);
    LeaveCriticalSection(&events->lock);
    LIST_FOR_EACH_ENTRY_SAFE(handler, next, &handlers, struct ink_handler, entry)
    {
        list_remove(&handler->entry);
        handler_release(handler);
    }
}

HRESULT ink_event_add(struct ink_events *events, UINT id, IUnknown *handler, EventRegistrationToken *token)
{
    struct ink_handler *entry;
    HRESULT hr = S_OK;

    if (!token) return E_POINTER;
    token->value = 0;
    if (!handler) return E_POINTER;
    if (!(entry = malloc(sizeof(*entry)))) return E_OUTOFMEMORY;
    entry->ref = 1;
    entry->id = id;
    entry->handler = handler;
    IUnknown_AddRef(handler);
    EnterCriticalSection(&events->lock);
    if (events->closed) hr = RO_E_CLOSED;
    else if (events->next_token == MAXLONGLONG) hr = E_UNEXPECTED;
    else
    {
        entry->token.value = ++events->next_token;
        list_add_tail(&events->handlers, &entry->entry);
        *token = entry->token;
    }
    LeaveCriticalSection(&events->lock);
    if (FAILED(hr)) handler_release(entry);
    return hr;
}

HRESULT ink_event_remove(struct ink_events *events, UINT id, EventRegistrationToken token)
{
    struct ink_handler *handler, *found = NULL;

    EnterCriticalSection(&events->lock);
    LIST_FOR_EACH_ENTRY(handler, &events->handlers, struct ink_handler, entry)
    {
        if (handler->id != id || handler->token.value != token.value) continue;
        list_remove(&handler->entry);
        found = handler;
        break;
    }
    LeaveCriticalSection(&events->lock);
    if (found) handler_release(found);
    return S_OK;
}

HRESULT ink_event_notify(struct ink_events *events, UINT id, void *sender, void *args, ink_event_callback invoke)
{
    struct ink_handler *handler, **snapshot = NULL;
    size_t count = 0, i = 0;
    HRESULT hr = S_OK, result;

    EnterCriticalSection(&events->lock);
    if (events->closed)
    {
        LeaveCriticalSection(&events->lock);
        return RO_E_CLOSED;
    }
    LIST_FOR_EACH_ENTRY(handler, &events->handlers, struct ink_handler, entry)
        if (handler->id == id) ++count;
    if (count && !(snapshot = malloc(count * sizeof(*snapshot))))
    {
        LeaveCriticalSection(&events->lock);
        return E_OUTOFMEMORY;
    }
    LIST_FOR_EACH_ENTRY(handler, &events->handlers, struct ink_handler, entry)
    {
        if (handler->id != id) continue;
        InterlockedIncrement(&handler->ref);
        snapshot[i++] = handler;
    }
    LeaveCriticalSection(&events->lock);
    for (i = 0; i < count; ++i)
    {
        result = invoke(snapshot[i]->handler, sender, args);
        if (SUCCEEDED(hr) && FAILED(result)) hr = result;
        handler_release(snapshot[i]);
    }
    free(snapshot);
    return hr;
}
