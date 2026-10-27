/*
 * WinRT web authentication core manager
 *
 * Copyright (C) 2026 Mirko Brombin
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 */

#include "private.h"
#include "webauthenticationcoremanagerinterop.h"

struct manager_factory
{
    IActivationFactory IActivationFactory_iface;
    IWebAuthenticationCoreManagerStatics IWebAuthenticationCoreManagerStatics_iface;
    IWebAuthenticationCoreManagerStatics4 IWebAuthenticationCoreManagerStatics4_iface;
    IWebAuthenticationCoreManagerInterop IWebAuthenticationCoreManagerInterop_iface;
    LONG ref;
};

static inline struct manager_factory *factory_from_activation( IActivationFactory *iface )
{
    return CONTAINING_RECORD( iface, struct manager_factory, IActivationFactory_iface );
}

static inline struct manager_factory *factory_from_statics( IWebAuthenticationCoreManagerStatics *iface )
{
    return CONTAINING_RECORD( iface, struct manager_factory, IWebAuthenticationCoreManagerStatics_iface );
}

static inline struct manager_factory *factory_from_statics4( IWebAuthenticationCoreManagerStatics4 *iface )
{
    return CONTAINING_RECORD( iface, struct manager_factory, IWebAuthenticationCoreManagerStatics4_iface );
}

static inline struct manager_factory *factory_from_interop( IWebAuthenticationCoreManagerInterop *iface )
{
    return CONTAINING_RECORD( iface, struct manager_factory, IWebAuthenticationCoreManagerInterop_iface );
}

static HRESULT factory_query_interface( struct manager_factory *impl, REFIID iid, void **out )
{
    if (!out) return E_POINTER;
    *out = NULL;
    if (IsEqualGUID( iid, &IID_IUnknown ) || IsEqualGUID( iid, &IID_IInspectable ) ||
        IsEqualGUID( iid, &IID_IAgileObject ) || IsEqualGUID( iid, &IID_IActivationFactory ))
        *out = &impl->IActivationFactory_iface;
    else if (IsEqualGUID( iid, &IID_IWebAuthenticationCoreManagerStatics ))
        *out = &impl->IWebAuthenticationCoreManagerStatics_iface;
    else if (IsEqualGUID( iid, &IID_IWebAuthenticationCoreManagerStatics4 ))
        *out = &impl->IWebAuthenticationCoreManagerStatics4_iface;
    else if (IsEqualGUID( iid, &IID_IWebAuthenticationCoreManagerInterop ))
        *out = &impl->IWebAuthenticationCoreManagerInterop_iface;
    else
        return E_NOINTERFACE;
    InterlockedIncrement( &impl->ref );
    return S_OK;
}

#define MANAGER_INSPECTABLE_METHODS(pfx, iface_type, from_iface)                                  \
    static HRESULT WINAPI pfx##_QueryInterface( iface_type *iface, REFIID iid, void **out )        \
    { return factory_query_interface( from_iface( iface ), iid, out ); }                            \
    static ULONG WINAPI pfx##_AddRef( iface_type *iface )                                          \
    { return InterlockedIncrement( &from_iface( iface )->ref ); }                                   \
    static ULONG WINAPI pfx##_Release( iface_type *iface )                                         \
    { return InterlockedDecrement( &from_iface( iface )->ref ); }                                   \
    static HRESULT WINAPI pfx##_GetIids( iface_type *iface, ULONG *count, IID **iids )              \
    {                                                                                               \
        if (!count || !iids) return E_POINTER;                                                      \
        *count = 3;                                                                                 \
        if (!(*iids = CoTaskMemAlloc( *count * sizeof(**iids) ))) return E_OUTOFMEMORY;             \
        (*iids)[0] = IID_IWebAuthenticationCoreManagerStatics;                                      \
        (*iids)[1] = IID_IWebAuthenticationCoreManagerStatics4;                                     \
        (*iids)[2] = IID_IWebAuthenticationCoreManagerInterop;                                      \
        return S_OK;                                                                                \
    }                                                                                               \
    static HRESULT WINAPI pfx##_GetRuntimeClassName( iface_type *iface, HSTRING *name )            \
    {                                                                                               \
        const WCHAR *class_name = RuntimeClass_Windows_Security_Authentication_Web_Core_WebAuthenticationCoreManager; \
        return WindowsCreateString( class_name, wcslen( class_name ), name );                       \
    }                                                                                               \
    static HRESULT WINAPI pfx##_GetTrustLevel( iface_type *iface, TrustLevel *level )              \
    { if (!level) return E_POINTER; *level = BaseTrust; return S_OK; }

MANAGER_INSPECTABLE_METHODS( activation, IActivationFactory, factory_from_activation )
MANAGER_INSPECTABLE_METHODS( statics, IWebAuthenticationCoreManagerStatics, factory_from_statics )
MANAGER_INSPECTABLE_METHODS( statics4, IWebAuthenticationCoreManagerStatics4, factory_from_statics4 )
MANAGER_INSPECTABLE_METHODS( interop, IWebAuthenticationCoreManagerInterop, factory_from_interop )

static HRESULT WINAPI activation_ActivateInstance( IActivationFactory *iface, IInspectable **instance )
{
    if (!instance) return E_POINTER;
    *instance = NULL;
    return E_NOTIMPL;
}

static const IActivationFactoryVtbl activation_vtbl =
{
    activation_QueryInterface,
    activation_AddRef,
    activation_Release,
    activation_GetIids,
    activation_GetRuntimeClassName,
    activation_GetTrustLevel,
    activation_ActivateInstance,
};

static HRESULT WINAPI statics_GetTokenSilentlyAsync( IWebAuthenticationCoreManagerStatics *iface,
                                                     IWebTokenRequest *request,
                                                     IAsyncOperation_WebTokenRequestResult **operation )
{
    return web_async_token_create( request, NULL, TRUE, operation );
}

static HRESULT WINAPI statics_GetTokenSilentlyWithWebAccountAsync(
    IWebAuthenticationCoreManagerStatics *iface, IWebTokenRequest *request, IWebAccount *account,
    IAsyncOperation_WebTokenRequestResult **operation )
{
    return web_async_token_create( request, account, TRUE, operation );
}

static HRESULT WINAPI statics_RequestTokenAsync( IWebAuthenticationCoreManagerStatics *iface,
                                                 IWebTokenRequest *request,
                                                 IAsyncOperation_WebTokenRequestResult **operation )
{
    return web_async_token_create( request, NULL, FALSE, operation );
}

static HRESULT WINAPI statics_RequestTokenWithWebAccountAsync(
    IWebAuthenticationCoreManagerStatics *iface, IWebTokenRequest *request, IWebAccount *account,
    IAsyncOperation_WebTokenRequestResult **operation )
{
    return web_async_token_create( request, account, FALSE, operation );
}

static HRESULT WINAPI statics_FindAccountAsync( IWebAuthenticationCoreManagerStatics *iface,
                                                IWebAccountProvider *provider, HSTRING account_id,
                                                IAsyncOperation_WebAccount **operation )
{
    return web_async_account_create( provider, account_id, operation );
}

static HRESULT WINAPI statics_FindAccountProviderAsync( IWebAuthenticationCoreManagerStatics *iface,
                                                        HSTRING provider_id,
                                                        IAsyncOperation_WebAccountProvider **operation )
{
    return web_async_provider_create( provider_id, NULL, FALSE, operation );
}

static HRESULT WINAPI statics_FindAccountProviderWithAuthorityAsync(
    IWebAuthenticationCoreManagerStatics *iface, HSTRING provider_id, HSTRING authority,
    IAsyncOperation_WebAccountProvider **operation )
{
    return web_async_provider_create( provider_id, authority, FALSE, operation );
}

static const IWebAuthenticationCoreManagerStaticsVtbl statics_vtbl =
{
    statics_QueryInterface,
    statics_AddRef,
    statics_Release,
    statics_GetIids,
    statics_GetRuntimeClassName,
    statics_GetTrustLevel,
    statics_GetTokenSilentlyAsync,
    statics_GetTokenSilentlyWithWebAccountAsync,
    statics_RequestTokenAsync,
    statics_RequestTokenWithWebAccountAsync,
    statics_FindAccountAsync,
    statics_FindAccountProviderAsync,
    statics_FindAccountProviderWithAuthorityAsync,
};

static HRESULT WINAPI statics4_FindAllAccountsAsync( IWebAuthenticationCoreManagerStatics4 *iface,
                                                      IWebAccountProvider *provider,
                                                      IAsyncOperation_FindAllAccountsResult **operation )
{
    return web_async_accounts_create( provider, NULL, operation );
}

static HRESULT WINAPI statics4_FindAllAccountsWithClientIdAsync(
    IWebAuthenticationCoreManagerStatics4 *iface, IWebAccountProvider *provider, HSTRING client_id,
    IAsyncOperation_FindAllAccountsResult **operation )
{
    return web_async_accounts_create( provider, client_id, operation );
}

static HRESULT WINAPI statics4_FindSystemAccountProviderAsync(
    IWebAuthenticationCoreManagerStatics4 *iface, HSTRING provider_id,
    IAsyncOperation_WebAccountProvider **operation )
{
    return web_async_provider_create( provider_id, NULL, TRUE, operation );
}

static HRESULT WINAPI statics4_FindSystemAccountProviderWithAuthorityAsync(
    IWebAuthenticationCoreManagerStatics4 *iface, HSTRING provider_id, HSTRING authority,
    IAsyncOperation_WebAccountProvider **operation )
{
    return web_async_provider_create( provider_id, authority, TRUE, operation );
}

static HRESULT WINAPI statics4_FindSystemAccountProviderWithAuthorityForUserAsync(
    IWebAuthenticationCoreManagerStatics4 *iface, HSTRING provider_id, HSTRING authority, IUser *user,
    IAsyncOperation_WebAccountProvider **operation )
{
    return web_async_provider_create( provider_id, authority, TRUE, operation );
}

static const IWebAuthenticationCoreManagerStatics4Vtbl statics4_vtbl =
{
    statics4_QueryInterface,
    statics4_AddRef,
    statics4_Release,
    statics4_GetIids,
    statics4_GetRuntimeClassName,
    statics4_GetTrustLevel,
    statics4_FindAllAccountsAsync,
    statics4_FindAllAccountsWithClientIdAsync,
    statics4_FindSystemAccountProviderAsync,
    statics4_FindSystemAccountProviderWithAuthorityAsync,
    statics4_FindSystemAccountProviderWithAuthorityForUserAsync,
};

static HRESULT interop_request_token( IInspectable *request, IInspectable *account,
                                      REFIID iid, void **out )
{
    IWebTokenRequest *token_request;
    IWebAccount *web_account = NULL;
    HRESULT hr;

    if (!out) return E_POINTER;
    *out = NULL;
    if (!request || !iid) return E_POINTER;
    if (!IsEqualGUID( iid, &IID_IAsyncOperation_WebTokenRequestResult )) return E_NOINTERFACE;
    if (FAILED(hr = IInspectable_QueryInterface( request, &IID_IWebTokenRequest,
                                                 (void **)&token_request ))) return hr;
    if (account)
        hr = IInspectable_QueryInterface( account, &IID_IWebAccount, (void **)&web_account );
    if (SUCCEEDED(hr))
        hr = web_async_token_create( token_request, web_account, FALSE,
                                    (IAsyncOperation_WebTokenRequestResult **)out );
    if (web_account) IWebAccount_Release( web_account );
    IWebTokenRequest_Release( token_request );
    return hr;
}

static HRESULT WINAPI interop_RequestTokenForWindowAsync( IWebAuthenticationCoreManagerInterop *iface,
                                                          HWND window, IInspectable *request,
                                                          REFIID iid, void **out )
{
    return interop_request_token( request, NULL, iid, out );
}

static HRESULT WINAPI interop_RequestTokenWithWebAccountForWindowAsync(
    IWebAuthenticationCoreManagerInterop *iface, HWND window, IInspectable *request,
    IInspectable *account, REFIID iid, void **out )
{
    if (!out) return E_POINTER;
    *out = NULL;
    if (!account) return E_POINTER;
    return interop_request_token( request, account, iid, out );
}

static const IWebAuthenticationCoreManagerInteropVtbl interop_vtbl =
{
    interop_QueryInterface,
    interop_AddRef,
    interop_Release,
    interop_GetIids,
    interop_GetRuntimeClassName,
    interop_GetTrustLevel,
    interop_RequestTokenForWindowAsync,
    interop_RequestTokenWithWebAccountForWindowAsync,
};

static struct manager_factory manager_factory =
{
    {&activation_vtbl},
    {&statics_vtbl},
    {&statics4_vtbl},
    {&interop_vtbl},
    1,
};

IActivationFactory *web_authentication_core_manager_factory = &manager_factory.IActivationFactory_iface;
