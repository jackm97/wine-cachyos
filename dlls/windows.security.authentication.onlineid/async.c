/* WinRT asynchronous ticket operation
 *
 * Copyright (C) 2026 Mirko Brombin
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 */

#include "private.h"

#define ASYNC_CLOSED ((AsyncStatus)4)
#define HANDLER_ASSIGNED ((void *)~(ULONG_PTR)0)

struct ticket_operation
{
    IAsyncOperation_OnlineIdSystemTicketResult IAsyncOperation_iface;
    IAsyncInfo IAsyncInfo_iface;
    LONG ref;
    CRITICAL_SECTION cs;
    TP_WORK *work;
    HANDLE cancel;
    IUnknown *invoker;
    IOnlineIdServiceTicketRequest *request;
    IAsyncOperationCompletedHandler_OnlineIdSystemTicketResult *handler;
    IOnlineIdSystemTicketResult *result;
    GUID application_id;
    AsyncStatus status;
    HRESULT error;
};

static inline struct ticket_operation *impl_from_operation( IAsyncOperation_OnlineIdSystemTicketResult *iface )
{
    return CONTAINING_RECORD( iface, struct ticket_operation, IAsyncOperation_iface );
}

static inline struct ticket_operation *impl_from_info( IAsyncInfo *iface )
{
    return CONTAINING_RECORD( iface, struct ticket_operation, IAsyncInfo_iface );
}

static HRESULT operation_query_interface( struct ticket_operation *impl, REFIID iid, void **out )
{
    if (!out) return E_POINTER;
    *out = NULL;
    if (IsEqualGUID( iid, &IID_IUnknown ) || IsEqualGUID( iid, &IID_IInspectable ) ||
        IsEqualGUID( iid, &IID_IAgileObject ) ||
        IsEqualGUID( iid, &IID_IAsyncOperation_OnlineIdSystemTicketResult ))
        *out = &impl->IAsyncOperation_iface;
    else if (IsEqualGUID( iid, &IID_IAsyncInfo ))
        *out = &impl->IAsyncInfo_iface;
    else
        return E_NOINTERFACE;
    IInspectable_AddRef( *out );
    return S_OK;
}

static ULONG operation_addref( struct ticket_operation *impl )
{
    return InterlockedIncrement( &impl->ref );
}

static ULONG operation_release( struct ticket_operation *impl )
{
    ULONG ref = InterlockedDecrement( &impl->ref );
    if (!ref)
    {
        if (impl->handler && impl->handler != HANDLER_ASSIGNED)
            IAsyncOperationCompletedHandler_OnlineIdSystemTicketResult_Release( impl->handler );
        if (impl->result) IOnlineIdSystemTicketResult_Release( impl->result );
        IOnlineIdServiceTicketRequest_Release( impl->request );
        if (impl->invoker) IUnknown_Release( impl->invoker );
        if (impl->work) CloseThreadpoolWork( impl->work );
        if (impl->cancel) CloseHandle( impl->cancel );
        impl->cs.DebugInfo->Spare[0] = 0;
        DeleteCriticalSection( &impl->cs );
        free( impl );
    }
    return ref;
}

static HRESULT WINAPI operation_QueryInterface( IAsyncOperation_OnlineIdSystemTicketResult *iface, REFIID iid, void **out )
{
    return operation_query_interface( impl_from_operation( iface ), iid, out );
}
static ULONG WINAPI operation_AddRef( IAsyncOperation_OnlineIdSystemTicketResult *iface )
{
    return operation_addref( impl_from_operation( iface ) );
}
static ULONG WINAPI operation_Release( IAsyncOperation_OnlineIdSystemTicketResult *iface )
{
    return operation_release( impl_from_operation( iface ) );
}
static HRESULT WINAPI operation_GetIids( IAsyncOperation_OnlineIdSystemTicketResult *iface, ULONG *count, IID **iids )
{
    if (!count || !iids) return E_POINTER;
    *count = 1;
    if (!(*iids = CoTaskMemAlloc( sizeof(**iids) ))) return E_OUTOFMEMORY;
    **iids = IID_IAsyncOperation_OnlineIdSystemTicketResult;
    return S_OK;
}
static HRESULT WINAPI operation_GetRuntimeClassName( IAsyncOperation_OnlineIdSystemTicketResult *iface, HSTRING *name )
{
    const WCHAR class_name[] = L"Windows.Foundation.IAsyncOperation`1<Windows.Security.Authentication.OnlineId.OnlineIdSystemTicketResult>";
    if (!name) return E_POINTER;
    return WindowsCreateString( class_name, ARRAY_SIZE(class_name) - 1, name );
}
static HRESULT WINAPI operation_GetTrustLevel( IAsyncOperation_OnlineIdSystemTicketResult *iface, TrustLevel *level )
{
    if (!level) return E_POINTER;
    *level = BaseTrust;
    return S_OK;
}

static HRESULT WINAPI operation_put_Completed( IAsyncOperation_OnlineIdSystemTicketResult *iface,
                                               IAsyncOperationCompletedHandler_OnlineIdSystemTicketResult *handler )
{
    struct ticket_operation *impl = impl_from_operation( iface );
    AsyncStatus status;

    EnterCriticalSection( &impl->cs );
    if (impl->status == ASYNC_CLOSED)
    {
        LeaveCriticalSection( &impl->cs );
        return E_ILLEGAL_METHOD_CALL;
    }
    if (impl->handler != HANDLER_ASSIGNED)
    {
        LeaveCriticalSection( &impl->cs );
        return E_ILLEGAL_DELEGATE_ASSIGNMENT;
    }
    if (handler) IAsyncOperationCompletedHandler_OnlineIdSystemTicketResult_AddRef( handler );
    impl->handler = handler;
    status = impl->status;
    if (status == Started)
    {
        LeaveCriticalSection( &impl->cs );
        return S_OK;
    }
    impl->handler = NULL;
    LeaveCriticalSection( &impl->cs );

    if (handler)
    {
        IAsyncOperationCompletedHandler_OnlineIdSystemTicketResult_Invoke( handler, iface, status );
        IAsyncOperationCompletedHandler_OnlineIdSystemTicketResult_Release( handler );
    }
    return S_OK;
}

static HRESULT WINAPI operation_get_Completed( IAsyncOperation_OnlineIdSystemTicketResult *iface,
                                               IAsyncOperationCompletedHandler_OnlineIdSystemTicketResult **handler )
{
    struct ticket_operation *impl = impl_from_operation( iface );
    if (!handler) return E_POINTER;
    EnterCriticalSection( &impl->cs );
    if (impl->status == ASYNC_CLOSED)
    {
        LeaveCriticalSection( &impl->cs );
        return E_ILLEGAL_METHOD_CALL;
    }
    if (!impl->handler || impl->handler == HANDLER_ASSIGNED)
        *handler = NULL;
    else
    {
        *handler = impl->handler;
        IAsyncOperationCompletedHandler_OnlineIdSystemTicketResult_AddRef( *handler );
    }
    LeaveCriticalSection( &impl->cs );
    return S_OK;
}

static HRESULT WINAPI operation_GetResults( IAsyncOperation_OnlineIdSystemTicketResult *iface,
                                            IOnlineIdSystemTicketResult **result )
{
    struct ticket_operation *impl = impl_from_operation( iface );
    HRESULT hr;

    if (!result) return E_POINTER;
    *result = NULL;
    EnterCriticalSection( &impl->cs );
    if (impl->status == Completed)
    {
        *result = impl->result;
        IOnlineIdSystemTicketResult_AddRef( *result );
        hr = S_OK;
    }
    else if (impl->status == Error)
        hr = impl->error;
    else
        hr = E_ILLEGAL_METHOD_CALL;
    LeaveCriticalSection( &impl->cs );
    return hr;
}

static const struct IAsyncOperation_OnlineIdSystemTicketResultVtbl operation_vtbl =
{
    operation_QueryInterface,
    operation_AddRef,
    operation_Release,
    operation_GetIids,
    operation_GetRuntimeClassName,
    operation_GetTrustLevel,
    operation_put_Completed,
    operation_get_Completed,
    operation_GetResults,
};

static HRESULT WINAPI info_QueryInterface( IAsyncInfo *iface, REFIID iid, void **out )
{
    return operation_query_interface( impl_from_info( iface ), iid, out );
}
static ULONG WINAPI info_AddRef( IAsyncInfo *iface )
{
    return operation_addref( impl_from_info( iface ) );
}
static ULONG WINAPI info_Release( IAsyncInfo *iface )
{
    return operation_release( impl_from_info( iface ) );
}
static HRESULT WINAPI info_GetIids( IAsyncInfo *iface, ULONG *count, IID **iids )
{
    if (!count || !iids) return E_POINTER;
    *count = 1;
    if (!(*iids = CoTaskMemAlloc( sizeof(**iids) ))) return E_OUTOFMEMORY;
    **iids = IID_IAsyncInfo;
    return S_OK;
}
static HRESULT WINAPI info_GetRuntimeClassName( IAsyncInfo *iface, HSTRING *name )
{
    return operation_GetRuntimeClassName( &impl_from_info( iface )->IAsyncOperation_iface, name );
}
static HRESULT WINAPI info_GetTrustLevel( IAsyncInfo *iface, TrustLevel *level )
{
    return operation_GetTrustLevel( &impl_from_info( iface )->IAsyncOperation_iface, level );
}
static HRESULT WINAPI info_get_Id( IAsyncInfo *iface, UINT32 *id )
{
    if (!id) return E_POINTER;
    *id = 1;
    return S_OK;
}
static HRESULT WINAPI info_get_Status( IAsyncInfo *iface, AsyncStatus *status )
{
    struct ticket_operation *impl = impl_from_info( iface );
    AsyncStatus value;
    if (!status) return E_POINTER;
    EnterCriticalSection( &impl->cs );
    value = impl->status;
    LeaveCriticalSection( &impl->cs );
    *status = value;
    return value == ASYNC_CLOSED ? E_ILLEGAL_METHOD_CALL : S_OK;
}
static HRESULT WINAPI info_get_ErrorCode( IAsyncInfo *iface, HRESULT *error )
{
    struct ticket_operation *impl = impl_from_info( iface );
    AsyncStatus status;
    if (!error) return E_POINTER;
    EnterCriticalSection( &impl->cs );
    *error = impl->error;
    status = impl->status;
    LeaveCriticalSection( &impl->cs );
    return status == ASYNC_CLOSED ? E_ILLEGAL_METHOD_CALL : S_OK;
}
static HRESULT WINAPI info_Cancel( IAsyncInfo *iface )
{
    struct ticket_operation *impl = impl_from_info( iface );
    IAsyncOperationCompletedHandler_OnlineIdSystemTicketResult *handler = NULL;
    EnterCriticalSection( &impl->cs );
    if (impl->status == ASYNC_CLOSED)
    {
        LeaveCriticalSection( &impl->cs );
        return E_ILLEGAL_METHOD_CALL;
    }
    if (impl->status == Started)
    {
        impl->status = Canceled;
        impl->error = E_ABORT;
        SetEvent( impl->cancel );
        if (impl->handler && impl->handler != HANDLER_ASSIGNED)
        {
            handler = impl->handler;
            impl->handler = NULL;
        }
    }
    LeaveCriticalSection( &impl->cs );
    if (handler)
    {
        IAsyncOperationCompletedHandler_OnlineIdSystemTicketResult_Invoke( handler, &impl->IAsyncOperation_iface,
                                                                          Canceled );
        IAsyncOperationCompletedHandler_OnlineIdSystemTicketResult_Release( handler );
    }
    return S_OK;
}
static HRESULT WINAPI info_Close( IAsyncInfo *iface )
{
    struct ticket_operation *impl = impl_from_info( iface );
    HRESULT hr = S_OK;
    EnterCriticalSection( &impl->cs );
    if (impl->status == Started) hr = E_ILLEGAL_STATE_CHANGE;
    else impl->status = ASYNC_CLOSED;
    LeaveCriticalSection( &impl->cs );
    return hr;
}

static const struct IAsyncInfoVtbl info_vtbl =
{
    info_QueryInterface,
    info_AddRef,
    info_Release,
    info_GetIids,
    info_GetRuntimeClassName,
    info_GetTrustLevel,
    info_get_Id,
    info_get_Status,
    info_get_ErrorCode,
    info_Cancel,
    info_Close,
};

static void CALLBACK operation_callback( TP_CALLBACK_INSTANCE *instance, void *context, TP_WORK *work )
{
    struct ticket_operation *impl = context;
    IAsyncOperationCompletedHandler_OnlineIdSystemTicketResult *handler = NULL;
    struct identity_reply reply = {0};
    HSTRING service = NULL, policy = NULL;
    IOnlineIdSystemTicketResult *result = NULL;
    AsyncStatus status = Error;
    HRESULT hr;

    if (SUCCEEDED(hr = IOnlineIdServiceTicketRequest_get_Service( impl->request, &service )) &&
        SUCCEEDED(hr = IOnlineIdServiceTicketRequest_get_Policy( impl->request, &policy )))
        hr = identity_bridge_authorize( &impl->application_id, service, policy, impl->cancel, &reply );
    if (FAILED(hr)) reply.error = hr;
    if (SUCCEEDED(hr) || reply.error)
        hr = ticket_result_create( impl->request, &reply, &result );
    identity_reply_clear( &reply );
    WindowsDeleteString( service );
    WindowsDeleteString( policy );

    EnterCriticalSection( &impl->cs );
    if (impl->status == Started)
    {
        impl->result = result;
        impl->error = hr;
        impl->status = FAILED(hr) ? Error : Completed;
        status = impl->status;
        if (impl->handler && impl->handler != HANDLER_ASSIGNED)
        {
            handler = impl->handler;
            impl->handler = NULL;
        }
    }
    else if (result)
        IOnlineIdSystemTicketResult_Release( result );
    LeaveCriticalSection( &impl->cs );

    if (handler)
    {
        IAsyncOperationCompletedHandler_OnlineIdSystemTicketResult_Invoke( handler, &impl->IAsyncOperation_iface,
                                                                          status );
        IAsyncOperationCompletedHandler_OnlineIdSystemTicketResult_Release( handler );
    }
    operation_release( impl );
}

HRESULT async_operation_ticket_result_create( IUnknown *invoker, IOnlineIdServiceTicketRequest *request,
                                              const GUID *application_id,
                                              IAsyncOperation_OnlineIdSystemTicketResult **out )
{
    struct ticket_operation *impl;

    if (!request || !application_id || !out) return E_POINTER;
    *out = NULL;
    if (!(impl = calloc( 1, sizeof(*impl) ))) return E_OUTOFMEMORY;
    impl->IAsyncOperation_iface.lpVtbl = &operation_vtbl;
    impl->IAsyncInfo_iface.lpVtbl = &info_vtbl;
    impl->ref = 2;
    impl->request = request;
    impl->invoker = invoker;
    impl->application_id = *application_id;
    impl->handler = HANDLER_ASSIGNED;
    impl->status = Started;
    IOnlineIdServiceTicketRequest_AddRef( request );
    if (invoker) IUnknown_AddRef( invoker );
    InitializeCriticalSectionEx( &impl->cs, 0, RTL_CRITICAL_SECTION_FLAG_FORCE_DEBUG_INFO );
    impl->cs.DebugInfo->Spare[0] = (DWORD_PTR)( __FILE__ ": ticket_operation.cs" );
    if (!(impl->cancel = CreateEventW( NULL, TRUE, FALSE, NULL )))
    {
        HRESULT hr = HRESULT_FROM_WIN32( GetLastError() );
        impl->ref = 1;
        operation_release( impl );
        return hr;
    }
    if (!(impl->work = CreateThreadpoolWork( operation_callback, impl, NULL )))
    {
        impl->ref = 1;
        operation_release( impl );
        return HRESULT_FROM_WIN32( GetLastError() );
    }
    SubmitThreadpoolWork( impl->work );
    *out = &impl->IAsyncOperation_iface;
    return S_OK;
}
