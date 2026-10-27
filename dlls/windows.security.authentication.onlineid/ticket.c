/* WinRT Windows.Security.Authentication.OnlineId ticket objects
 *
 * Copyright (C) 2024 Mohamad Al-Jaf
 * Copyright (C) 2026 Mirko Brombin
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 */

#include "private.h"
struct ticket_request
{
    IOnlineIdServiceTicketRequest IOnlineIdServiceTicketRequest_iface;
    LONG ref;
    HSTRING service;
    HSTRING policy;
};

static inline struct ticket_request *impl_from_IOnlineIdServiceTicketRequest( IOnlineIdServiceTicketRequest *iface )
{
    return CONTAINING_RECORD( iface, struct ticket_request, IOnlineIdServiceTicketRequest_iface );
}

static HRESULT WINAPI service_request_QueryInterface( IOnlineIdServiceTicketRequest *iface, REFIID iid, void **out )
{
    if (!out) return E_POINTER;
    *out = NULL;
    if (IsEqualGUID( iid, &IID_IUnknown ) || IsEqualGUID( iid, &IID_IInspectable ) ||
        IsEqualGUID( iid, &IID_IAgileObject ) || IsEqualGUID( iid, &IID_IOnlineIdServiceTicketRequest ))
    {
        *out = iface;
        IOnlineIdServiceTicketRequest_AddRef( iface );
        return S_OK;
    }
    return E_NOINTERFACE;
}

static ULONG WINAPI service_request_AddRef( IOnlineIdServiceTicketRequest *iface )
{
    return InterlockedIncrement( &impl_from_IOnlineIdServiceTicketRequest( iface )->ref );
}

static ULONG WINAPI service_request_Release( IOnlineIdServiceTicketRequest *iface )
{
    struct ticket_request *impl = impl_from_IOnlineIdServiceTicketRequest( iface );
    ULONG ref = InterlockedDecrement( &impl->ref );

    if (!ref)
    {
        WindowsDeleteString( impl->service );
        WindowsDeleteString( impl->policy );
        free( impl );
    }
    return ref;
}

static HRESULT WINAPI service_request_GetIids( IOnlineIdServiceTicketRequest *iface, ULONG *count, IID **iids )
{
    if (!count || !iids) return E_POINTER;
    *count = 1;
    if (!(*iids = CoTaskMemAlloc( sizeof(**iids) ))) return E_OUTOFMEMORY;
    **iids = IID_IOnlineIdServiceTicketRequest;
    return S_OK;
}

static HRESULT WINAPI service_request_GetRuntimeClassName( IOnlineIdServiceTicketRequest *iface, HSTRING *name )
{
    const WCHAR *class_name = RuntimeClass_Windows_Security_Authentication_OnlineId_OnlineIdServiceTicketRequest;
    if (!name) return E_POINTER;
    return WindowsCreateString( class_name, wcslen( class_name ), name );
}

static HRESULT WINAPI service_request_GetTrustLevel( IOnlineIdServiceTicketRequest *iface, TrustLevel *level )
{
    if (!level) return E_POINTER;
    *level = BaseTrust;
    return S_OK;
}

static HRESULT WINAPI service_request_get_Service( IOnlineIdServiceTicketRequest *iface, HSTRING *value )
{
    if (!value) return E_POINTER;
    return WindowsDuplicateString( impl_from_IOnlineIdServiceTicketRequest( iface )->service, value );
}

static HRESULT WINAPI service_request_get_Policy( IOnlineIdServiceTicketRequest *iface, HSTRING *value )
{
    if (!value) return E_POINTER;
    return WindowsDuplicateString( impl_from_IOnlineIdServiceTicketRequest( iface )->policy, value );
}

static const struct IOnlineIdServiceTicketRequestVtbl service_request_vtbl =
{
    service_request_QueryInterface,
    service_request_AddRef,
    service_request_Release,
    service_request_GetIids,
    service_request_GetRuntimeClassName,
    service_request_GetTrustLevel,
    service_request_get_Service,
    service_request_get_Policy,
};

HRESULT ticket_request_create( HSTRING service, HSTRING policy, IOnlineIdServiceTicketRequest **out )
{
    struct ticket_request *impl;
    HRESULT hr;

    if (!out) return E_POINTER;
    *out = NULL;
    if (!service) return E_INVALIDARG;
    if (!(impl = calloc( 1, sizeof(*impl) ))) return E_OUTOFMEMORY;
    impl->IOnlineIdServiceTicketRequest_iface.lpVtbl = &service_request_vtbl;
    impl->ref = 1;
    if (FAILED(hr = WindowsDuplicateString( service, &impl->service )) ||
        FAILED(hr = WindowsDuplicateString( policy, &impl->policy )))
    {
        IOnlineIdServiceTicketRequest_Release( &impl->IOnlineIdServiceTicketRequest_iface );
        return hr;
    }
    *out = &impl->IOnlineIdServiceTicketRequest_iface;
    return S_OK;
}

struct ticket_request_factory
{
    IActivationFactory IActivationFactory_iface;
    IOnlineIdServiceTicketRequestFactory IOnlineIdServiceTicketRequestFactory_iface;
    LONG ref;
};

static inline struct ticket_request_factory *factory_from_activation( IActivationFactory *iface )
{
    return CONTAINING_RECORD( iface, struct ticket_request_factory, IActivationFactory_iface );
}

static inline struct ticket_request_factory *factory_from_request_factory( IOnlineIdServiceTicketRequestFactory *iface )
{
    return CONTAINING_RECORD( iface, struct ticket_request_factory, IOnlineIdServiceTicketRequestFactory_iface );
}

static HRESULT WINAPI factory_QueryInterface( IActivationFactory *iface, REFIID iid, void **out )
{
    struct ticket_request_factory *impl = factory_from_activation( iface );

    if (!out) return E_POINTER;
    *out = NULL;
    if (IsEqualGUID( iid, &IID_IUnknown ) || IsEqualGUID( iid, &IID_IInspectable ) ||
        IsEqualGUID( iid, &IID_IAgileObject ) || IsEqualGUID( iid, &IID_IActivationFactory ))
        *out = &impl->IActivationFactory_iface;
    else if (IsEqualGUID( iid, &IID_IOnlineIdServiceTicketRequestFactory ))
        *out = &impl->IOnlineIdServiceTicketRequestFactory_iface;
    else
        return E_NOINTERFACE;
    IInspectable_AddRef( *out );
    return S_OK;
}

static ULONG WINAPI factory_AddRef( IActivationFactory *iface )
{
    return InterlockedIncrement( &factory_from_activation( iface )->ref );
}

static ULONG WINAPI factory_Release( IActivationFactory *iface )
{
    return InterlockedDecrement( &factory_from_activation( iface )->ref );
}

static HRESULT WINAPI factory_GetIids( IActivationFactory *iface, ULONG *count, IID **iids )
{
    if (!count || !iids) return E_POINTER;
    *count = 1;
    if (!(*iids = CoTaskMemAlloc( sizeof(**iids) ))) return E_OUTOFMEMORY;
    **iids = IID_IOnlineIdServiceTicketRequestFactory;
    return S_OK;
}

static HRESULT WINAPI factory_GetRuntimeClassName( IActivationFactory *iface, HSTRING *name )
{
    const WCHAR *class_name = RuntimeClass_Windows_Security_Authentication_OnlineId_OnlineIdServiceTicketRequest;
    if (!name) return E_POINTER;
    return WindowsCreateString( class_name, wcslen( class_name ), name );
}

static HRESULT WINAPI factory_GetTrustLevel( IActivationFactory *iface, TrustLevel *level )
{
    if (!level) return E_POINTER;
    *level = BaseTrust;
    return S_OK;
}

static HRESULT WINAPI factory_ActivateInstance( IActivationFactory *iface, IInspectable **instance )
{
    if (instance) *instance = NULL;
    return E_NOTIMPL;
}

static const struct IActivationFactoryVtbl factory_vtbl =
{
    factory_QueryInterface,
    factory_AddRef,
    factory_Release,
    factory_GetIids,
    factory_GetRuntimeClassName,
    factory_GetTrustLevel,
    factory_ActivateInstance,
};

static HRESULT WINAPI request_factory_QueryInterface( IOnlineIdServiceTicketRequestFactory *iface, REFIID iid, void **out )
{
    return factory_QueryInterface( &factory_from_request_factory( iface )->IActivationFactory_iface, iid, out );
}
static ULONG WINAPI request_factory_AddRef( IOnlineIdServiceTicketRequestFactory *iface )
{
    return factory_AddRef( &factory_from_request_factory( iface )->IActivationFactory_iface );
}
static ULONG WINAPI request_factory_Release( IOnlineIdServiceTicketRequestFactory *iface )
{
    return factory_Release( &factory_from_request_factory( iface )->IActivationFactory_iface );
}
static HRESULT WINAPI request_factory_GetIids( IOnlineIdServiceTicketRequestFactory *iface, ULONG *count, IID **iids )
{
    return factory_GetIids( &factory_from_request_factory( iface )->IActivationFactory_iface, count, iids );
}
static HRESULT WINAPI request_factory_GetRuntimeClassName( IOnlineIdServiceTicketRequestFactory *iface, HSTRING *name )
{
    return factory_GetRuntimeClassName( &factory_from_request_factory( iface )->IActivationFactory_iface, name );
}
static HRESULT WINAPI request_factory_GetTrustLevel( IOnlineIdServiceTicketRequestFactory *iface, TrustLevel *level )
{
    return factory_GetTrustLevel( &factory_from_request_factory( iface )->IActivationFactory_iface, level );
}

static HRESULT WINAPI request_factory_CreateOnlineIdServiceTicketRequest( IOnlineIdServiceTicketRequestFactory *iface,
                                                                           HSTRING service, HSTRING policy,
                                                                           IOnlineIdServiceTicketRequest **request )
{
    return ticket_request_create( service, policy, request );
}

static HRESULT WINAPI request_factory_CreateOnlineIdServiceTicketRequestAdvanced( IOnlineIdServiceTicketRequestFactory *iface,
                                                                                   HSTRING service,
                                                                                   IOnlineIdServiceTicketRequest **request )
{
    HSTRING policy;
    HRESULT hr;

    if (FAILED(hr = WindowsCreateString( L"", 0, &policy ))) return hr;
    hr = ticket_request_create( service, policy, request );
    WindowsDeleteString( policy );
    return hr;
}

static const struct IOnlineIdServiceTicketRequestFactoryVtbl request_factory_vtbl =
{
    request_factory_QueryInterface,
    request_factory_AddRef,
    request_factory_Release,
    request_factory_GetIids,
    request_factory_GetRuntimeClassName,
    request_factory_GetTrustLevel,
    request_factory_CreateOnlineIdServiceTicketRequest,
    request_factory_CreateOnlineIdServiceTicketRequestAdvanced,
};

static struct ticket_request_factory ticket_request_factory =
{
    {&factory_vtbl},
    {&request_factory_vtbl},
    1,
};

IActivationFactory *ticket_factory = &ticket_request_factory.IActivationFactory_iface;
