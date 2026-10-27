/* WinRT Windows.Security.Authentication.OnlineId result objects
 *
 * Copyright (C) 2026 Mirko Brombin
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 */

#include "private.h"

struct ticket_result
{
    IOnlineIdSystemTicketResult IOnlineIdSystemTicketResult_iface;
    IOnlineIdSystemIdentity IOnlineIdSystemIdentity_iface;
    IOnlineIdServiceTicket IOnlineIdServiceTicket_iface;
    LONG ref;
    IOnlineIdServiceTicketRequest *request;
    HSTRING ticket;
    HSTRING id;
    HRESULT error;
};

static inline struct ticket_result *impl_from_result( IOnlineIdSystemTicketResult *iface )
{
    return CONTAINING_RECORD( iface, struct ticket_result, IOnlineIdSystemTicketResult_iface );
}

static inline struct ticket_result *impl_from_identity( IOnlineIdSystemIdentity *iface )
{
    return CONTAINING_RECORD( iface, struct ticket_result, IOnlineIdSystemIdentity_iface );
}

static inline struct ticket_result *impl_from_ticket( IOnlineIdServiceTicket *iface )
{
    return CONTAINING_RECORD( iface, struct ticket_result, IOnlineIdServiceTicket_iface );
}

static HRESULT result_query_interface( struct ticket_result *impl, REFIID iid, void **out )
{
    if (!out) return E_POINTER;
    *out = NULL;
    if (IsEqualGUID( iid, &IID_IUnknown ) || IsEqualGUID( iid, &IID_IInspectable ) ||
        IsEqualGUID( iid, &IID_IAgileObject ) || IsEqualGUID( iid, &IID_IOnlineIdSystemTicketResult ))
        *out = &impl->IOnlineIdSystemTicketResult_iface;
    else if (IsEqualGUID( iid, &IID_IOnlineIdSystemIdentity ))
        *out = &impl->IOnlineIdSystemIdentity_iface;
    else if (IsEqualGUID( iid, &IID_IOnlineIdServiceTicket ))
        *out = &impl->IOnlineIdServiceTicket_iface;
    else
        return E_NOINTERFACE;
    IInspectable_AddRef( *out );
    return S_OK;
}

static ULONG result_addref( struct ticket_result *impl )
{
    return InterlockedIncrement( &impl->ref );
}

static ULONG result_release( struct ticket_result *impl )
{
    ULONG ref = InterlockedDecrement( &impl->ref );
    if (!ref)
    {
        if (impl->request) IOnlineIdServiceTicketRequest_Release( impl->request );
        WindowsDeleteString( impl->ticket );
        WindowsDeleteString( impl->id );
        free( impl );
    }
    return ref;
}

static HRESULT get_single_iid( REFIID iid, ULONG *count, IID **iids )
{
    if (!count || !iids) return E_POINTER;
    *count = 1;
    if (!(*iids = CoTaskMemAlloc( sizeof(**iids) ))) return E_OUTOFMEMORY;
    **iids = *iid;
    return S_OK;
}

static HRESULT WINAPI result_QueryInterface( IOnlineIdSystemTicketResult *iface, REFIID iid, void **out )
{
    return result_query_interface( impl_from_result( iface ), iid, out );
}
static ULONG WINAPI result_AddRef( IOnlineIdSystemTicketResult *iface )
{
    return result_addref( impl_from_result( iface ) );
}
static ULONG WINAPI result_Release( IOnlineIdSystemTicketResult *iface )
{
    return result_release( impl_from_result( iface ) );
}
static HRESULT WINAPI result_GetIids( IOnlineIdSystemTicketResult *iface, ULONG *count, IID **iids )
{
    return get_single_iid( &IID_IOnlineIdSystemTicketResult, count, iids );
}
static HRESULT WINAPI result_GetRuntimeClassName( IOnlineIdSystemTicketResult *iface, HSTRING *name )
{
    const WCHAR *class_name = RuntimeClass_Windows_Security_Authentication_OnlineId_OnlineIdSystemTicketResult;
    if (!name) return E_POINTER;
    return WindowsCreateString( class_name, wcslen( class_name ), name );
}
static HRESULT WINAPI result_GetTrustLevel( IOnlineIdSystemTicketResult *iface, TrustLevel *level )
{
    if (!level) return E_POINTER;
    *level = BaseTrust;
    return S_OK;
}
static HRESULT WINAPI result_get_Identity( IOnlineIdSystemTicketResult *iface, IOnlineIdSystemIdentity **value )
{
    struct ticket_result *impl = impl_from_result( iface );
    if (!value) return E_POINTER;
    *value = &impl->IOnlineIdSystemIdentity_iface;
    IOnlineIdSystemIdentity_AddRef( *value );
    return S_OK;
}
static HRESULT WINAPI result_get_Status( IOnlineIdSystemTicketResult *iface, OnlineIdSystemTicketStatus *value )
{
    if (!value) return E_POINTER;
    *value = SUCCEEDED(impl_from_result( iface )->error) ? OnlineIdSystemTicketStatus_Success : OnlineIdSystemTicketStatus_Error;
    return S_OK;
}
static HRESULT WINAPI result_get_ExtendedError( IOnlineIdSystemTicketResult *iface, HRESULT *value )
{
    if (!value) return E_POINTER;
    *value = impl_from_result( iface )->error;
    return S_OK;
}

static const struct IOnlineIdSystemTicketResultVtbl result_vtbl =
{
    result_QueryInterface,
    result_AddRef,
    result_Release,
    result_GetIids,
    result_GetRuntimeClassName,
    result_GetTrustLevel,
    result_get_Identity,
    result_get_Status,
    result_get_ExtendedError,
};

static HRESULT WINAPI identity_QueryInterface( IOnlineIdSystemIdentity *iface, REFIID iid, void **out )
{
    return result_query_interface( impl_from_identity( iface ), iid, out );
}
static ULONG WINAPI identity_AddRef( IOnlineIdSystemIdentity *iface )
{
    return result_addref( impl_from_identity( iface ) );
}
static ULONG WINAPI identity_Release( IOnlineIdSystemIdentity *iface )
{
    return result_release( impl_from_identity( iface ) );
}
static HRESULT WINAPI identity_GetIids( IOnlineIdSystemIdentity *iface, ULONG *count, IID **iids )
{
    return get_single_iid( &IID_IOnlineIdSystemIdentity, count, iids );
}
static HRESULT WINAPI identity_GetRuntimeClassName( IOnlineIdSystemIdentity *iface, HSTRING *name )
{
    const WCHAR *class_name = RuntimeClass_Windows_Security_Authentication_OnlineId_OnlineIdSystemIdentity;
    if (!name) return E_POINTER;
    return WindowsCreateString( class_name, wcslen( class_name ), name );
}
static HRESULT WINAPI identity_GetTrustLevel( IOnlineIdSystemIdentity *iface, TrustLevel *level )
{
    if (!level) return E_POINTER;
    *level = BaseTrust;
    return S_OK;
}
static HRESULT WINAPI identity_get_Ticket( IOnlineIdSystemIdentity *iface, IOnlineIdServiceTicket **value )
{
    struct ticket_result *impl = impl_from_identity( iface );
    if (!value) return E_POINTER;
    *value = &impl->IOnlineIdServiceTicket_iface;
    IOnlineIdServiceTicket_AddRef( *value );
    return S_OK;
}
static HRESULT WINAPI identity_get_Id( IOnlineIdSystemIdentity *iface, HSTRING *value )
{
    if (!value) return E_POINTER;
    return WindowsDuplicateString( impl_from_identity( iface )->id, value );
}

static const struct IOnlineIdSystemIdentityVtbl identity_vtbl =
{
    identity_QueryInterface,
    identity_AddRef,
    identity_Release,
    identity_GetIids,
    identity_GetRuntimeClassName,
    identity_GetTrustLevel,
    identity_get_Ticket,
    identity_get_Id,
};

static HRESULT WINAPI ticket_QueryInterface( IOnlineIdServiceTicket *iface, REFIID iid, void **out )
{
    return result_query_interface( impl_from_ticket( iface ), iid, out );
}
static ULONG WINAPI ticket_AddRef( IOnlineIdServiceTicket *iface )
{
    return result_addref( impl_from_ticket( iface ) );
}
static ULONG WINAPI ticket_Release( IOnlineIdServiceTicket *iface )
{
    return result_release( impl_from_ticket( iface ) );
}
static HRESULT WINAPI ticket_GetIids( IOnlineIdServiceTicket *iface, ULONG *count, IID **iids )
{
    return get_single_iid( &IID_IOnlineIdServiceTicket, count, iids );
}
static HRESULT WINAPI ticket_GetRuntimeClassName( IOnlineIdServiceTicket *iface, HSTRING *name )
{
    const WCHAR *class_name = RuntimeClass_Windows_Security_Authentication_OnlineId_OnlineIdServiceTicket;
    if (!name) return E_POINTER;
    return WindowsCreateString( class_name, wcslen( class_name ), name );
}
static HRESULT WINAPI ticket_GetTrustLevel( IOnlineIdServiceTicket *iface, TrustLevel *level )
{
    if (!level) return E_POINTER;
    *level = BaseTrust;
    return S_OK;
}
static HRESULT WINAPI ticket_get_Value( IOnlineIdServiceTicket *iface, HSTRING *value )
{
    if (!value) return E_POINTER;
    return WindowsDuplicateString( impl_from_ticket( iface )->ticket, value );
}
static HRESULT WINAPI ticket_get_Request( IOnlineIdServiceTicket *iface, IOnlineIdServiceTicketRequest **value )
{
    struct ticket_result *impl = impl_from_ticket( iface );
    if (!value) return E_POINTER;
    *value = impl->request;
    IOnlineIdServiceTicketRequest_AddRef( *value );
    return S_OK;
}
static HRESULT WINAPI ticket_get_ErrorCode( IOnlineIdServiceTicket *iface, INT32 *value )
{
    if (!value) return E_POINTER;
    *value = impl_from_ticket( iface )->error;
    return S_OK;
}

static const struct IOnlineIdServiceTicketVtbl ticket_vtbl =
{
    ticket_QueryInterface,
    ticket_AddRef,
    ticket_Release,
    ticket_GetIids,
    ticket_GetRuntimeClassName,
    ticket_GetTrustLevel,
    ticket_get_Value,
    ticket_get_Request,
    ticket_get_ErrorCode,
};

HRESULT ticket_result_create( IOnlineIdServiceTicketRequest *request, const struct identity_reply *reply,
                              IOnlineIdSystemTicketResult **out )
{
    struct ticket_result *impl;
    HRESULT hr;

    if (!request || !reply || !out) return E_POINTER;
    *out = NULL;
    if (!(impl = calloc( 1, sizeof(*impl) ))) return E_OUTOFMEMORY;
    impl->IOnlineIdSystemTicketResult_iface.lpVtbl = &result_vtbl;
    impl->IOnlineIdSystemIdentity_iface.lpVtbl = &identity_vtbl;
    impl->IOnlineIdServiceTicket_iface.lpVtbl = &ticket_vtbl;
    impl->ref = 1;
    impl->request = request;
    impl->error = reply->error;
    IOnlineIdServiceTicketRequest_AddRef( request );
    if (reply->access_token)
        hr = WindowsDuplicateString( reply->access_token, &impl->ticket );
    else
        hr = WindowsCreateString( L"", 0, &impl->ticket );
    if (SUCCEEDED(hr) && reply->account_id)
        hr = WindowsDuplicateString( reply->account_id, &impl->id );
    else if (SUCCEEDED(hr))
        hr = WindowsCreateString( L"", 0, &impl->id );
    if (FAILED(hr))
    {
        IOnlineIdSystemTicketResult_Release( &impl->IOnlineIdSystemTicketResult_iface );
        return hr;
    }
    *out = &impl->IOnlineIdSystemTicketResult_iface;
    return S_OK;
}
