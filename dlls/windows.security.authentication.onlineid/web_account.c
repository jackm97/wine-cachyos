/*
 * WinRT web accounts
 *
 * Copyright (C) 2026 Mirko Brombin
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 */

#include "private.h"

struct web_account_provider
{
    IWebAccountProvider IWebAccountProvider_iface;
    IWebAccountProvider2 IWebAccountProvider2_iface;
    IWebAccountProvider3 IWebAccountProvider3_iface;
    IWebAccountProvider4 IWebAccountProvider4_iface;
    LONG ref;
    HSTRING id;
    HSTRING display_name;
    HSTRING display_purpose;
    HSTRING authority;
    boolean system_provider;
};

struct web_account
{
    IWebAccount IWebAccount_iface;
    IWebAccount2 IWebAccount2_iface;
    LONG ref;
    IWebAccountProvider *provider;
    HSTRING id;
    HSTRING username;
    WebAccountState state;
    IMap_HSTRING_HSTRING *properties;
};

struct account_factory
{
    IActivationFactory IActivationFactory_iface;
    IWebAccountFactory IWebAccountFactory_iface;
    LONG ref;
};

struct provider_factory
{
    IActivationFactory IActivationFactory_iface;
    IWebAccountProviderFactory IWebAccountProviderFactory_iface;
    LONG ref;
};

static inline struct web_account_provider *provider_from_IWebAccountProvider( IWebAccountProvider *iface )
{
    return CONTAINING_RECORD( iface, struct web_account_provider, IWebAccountProvider_iface );
}

static inline struct web_account_provider *provider_from_IWebAccountProvider2( IWebAccountProvider2 *iface )
{
    return CONTAINING_RECORD( iface, struct web_account_provider, IWebAccountProvider2_iface );
}

static inline struct web_account_provider *provider_from_IWebAccountProvider3( IWebAccountProvider3 *iface )
{
    return CONTAINING_RECORD( iface, struct web_account_provider, IWebAccountProvider3_iface );
}

static inline struct web_account_provider *provider_from_IWebAccountProvider4( IWebAccountProvider4 *iface )
{
    return CONTAINING_RECORD( iface, struct web_account_provider, IWebAccountProvider4_iface );
}

static HRESULT provider_query_interface( struct web_account_provider *impl, REFIID iid, void **out )
{
    if (!out) return E_POINTER;
    *out = NULL;
    if (IsEqualGUID( iid, &IID_IUnknown ) || IsEqualGUID( iid, &IID_IInspectable ) ||
        IsEqualGUID( iid, &IID_IAgileObject ) || IsEqualGUID( iid, &IID_IWebAccountProvider ))
        *out = &impl->IWebAccountProvider_iface;
    else if (IsEqualGUID( iid, &IID_IWebAccountProvider2 ))
        *out = &impl->IWebAccountProvider2_iface;
    else if (IsEqualGUID( iid, &IID_IWebAccountProvider3 ))
        *out = &impl->IWebAccountProvider3_iface;
    else if (IsEqualGUID( iid, &IID_IWebAccountProvider4 ))
        *out = &impl->IWebAccountProvider4_iface;
    else
        return E_NOINTERFACE;
    InterlockedIncrement( &impl->ref );
    return S_OK;
}

static ULONG provider_release( struct web_account_provider *impl )
{
    ULONG ref = InterlockedDecrement( &impl->ref );
    if (!ref)
    {
        WindowsDeleteString( impl->id );
        WindowsDeleteString( impl->display_name );
        WindowsDeleteString( impl->display_purpose );
        WindowsDeleteString( impl->authority );
        free( impl );
    }
    return ref;
}

static HRESULT provider_get_iids( ULONG *count, IID **iids )
{
    if (!count || !iids) return E_POINTER;
    *count = 4;
    if (!(*iids = CoTaskMemAlloc( 4 * sizeof(**iids) ))) return E_OUTOFMEMORY;
    (*iids)[0] = IID_IWebAccountProvider;
    (*iids)[1] = IID_IWebAccountProvider2;
    (*iids)[2] = IID_IWebAccountProvider3;
    (*iids)[3] = IID_IWebAccountProvider4;
    return S_OK;
}

static HRESULT provider_get_class_name( HSTRING *name )
{
    const WCHAR *class_name = RuntimeClass_Windows_Security_Credentials_WebAccountProvider;
    if (!name) return E_POINTER;
    return WindowsCreateString( class_name, wcslen( class_name ), name );
}

static HRESULT provider_get_trust_level( TrustLevel *level )
{
    if (!level) return E_POINTER;
    *level = BaseTrust;
    return S_OK;
}

#define PROVIDER_INSPECTABLE_METHODS( pfx, iface_type, from )                                    \
    static HRESULT WINAPI pfx##_QueryInterface( iface_type *iface, REFIID iid, void **out )       \
    { return provider_query_interface( from( iface ), iid, out ); }                               \
    static ULONG WINAPI pfx##_AddRef( iface_type *iface )                                         \
    { return InterlockedIncrement( &from( iface )->ref ); }                                        \
    static ULONG WINAPI pfx##_Release( iface_type *iface )                                        \
    { return provider_release( from( iface ) ); }                                                  \
    static HRESULT WINAPI pfx##_GetIids( iface_type *iface, ULONG *count, IID **iids )             \
    { return provider_get_iids( count, iids ); }                                                   \
    static HRESULT WINAPI pfx##_GetRuntimeClassName( iface_type *iface, HSTRING *name )            \
    { return provider_get_class_name( name ); }                                                    \
    static HRESULT WINAPI pfx##_GetTrustLevel( iface_type *iface, TrustLevel *level )              \
    { return provider_get_trust_level( level ); }

PROVIDER_INSPECTABLE_METHODS( provider, IWebAccountProvider, provider_from_IWebAccountProvider )
PROVIDER_INSPECTABLE_METHODS( provider2, IWebAccountProvider2, provider_from_IWebAccountProvider2 )
PROVIDER_INSPECTABLE_METHODS( provider3, IWebAccountProvider3, provider_from_IWebAccountProvider3 )
PROVIDER_INSPECTABLE_METHODS( provider4, IWebAccountProvider4, provider_from_IWebAccountProvider4 )

static HRESULT WINAPI provider_get_Id( IWebAccountProvider *iface, HSTRING *value )
{
    return WindowsDuplicateString( provider_from_IWebAccountProvider( iface )->id, value );
}

static HRESULT WINAPI provider_get_DisplayName( IWebAccountProvider *iface, HSTRING *value )
{
    return WindowsDuplicateString( provider_from_IWebAccountProvider( iface )->display_name, value );
}

static HRESULT WINAPI provider_get_IconUri( IWebAccountProvider *iface, IUriRuntimeClass **value )
{
    if (!value) return E_POINTER;
    *value = NULL;
    return S_OK;
}

static const IWebAccountProviderVtbl provider_vtbl =
{
    provider_QueryInterface,
    provider_AddRef,
    provider_Release,
    provider_GetIids,
    provider_GetRuntimeClassName,
    provider_GetTrustLevel,
    provider_get_Id,
    provider_get_DisplayName,
    provider_get_IconUri,
};

static HRESULT WINAPI provider2_get_DisplayPurpose( IWebAccountProvider2 *iface, HSTRING *value )
{
    return WindowsDuplicateString( provider_from_IWebAccountProvider2( iface )->display_purpose, value );
}

static HRESULT WINAPI provider2_get_Authority( IWebAccountProvider2 *iface, HSTRING *value )
{
    return WindowsDuplicateString( provider_from_IWebAccountProvider2( iface )->authority, value );
}

static const IWebAccountProvider2Vtbl provider2_vtbl =
{
    provider2_QueryInterface,
    provider2_AddRef,
    provider2_Release,
    provider2_GetIids,
    provider2_GetRuntimeClassName,
    provider2_GetTrustLevel,
    provider2_get_DisplayPurpose,
    provider2_get_Authority,
};

static HRESULT WINAPI provider3_get_User( IWebAccountProvider3 *iface, IUser **value )
{
    if (!value) return E_POINTER;
    *value = NULL;
    return S_OK;
}

static const IWebAccountProvider3Vtbl provider3_vtbl =
{
    provider3_QueryInterface,
    provider3_AddRef,
    provider3_Release,
    provider3_GetIids,
    provider3_GetRuntimeClassName,
    provider3_GetTrustLevel,
    provider3_get_User,
};

static HRESULT WINAPI provider4_get_IsSystemProvider( IWebAccountProvider4 *iface, boolean *value )
{
    if (!value) return E_POINTER;
    *value = provider_from_IWebAccountProvider4( iface )->system_provider;
    return S_OK;
}

static const IWebAccountProvider4Vtbl provider4_vtbl =
{
    provider4_QueryInterface,
    provider4_AddRef,
    provider4_Release,
    provider4_GetIids,
    provider4_GetRuntimeClassName,
    provider4_GetTrustLevel,
    provider4_get_IsSystemProvider,
};

HRESULT web_account_provider_create( HSTRING id, HSTRING display_name, HSTRING authority,
                                     boolean system_provider, IWebAccountProvider **out )
{
    struct web_account_provider *impl;
    HRESULT hr;

    if (!out) return E_POINTER;
    *out = NULL;
    if (!(impl = calloc( 1, sizeof(*impl) ))) return E_OUTOFMEMORY;
    impl->IWebAccountProvider_iface.lpVtbl = &provider_vtbl;
    impl->IWebAccountProvider2_iface.lpVtbl = &provider2_vtbl;
    impl->IWebAccountProvider3_iface.lpVtbl = &provider3_vtbl;
    impl->IWebAccountProvider4_iface.lpVtbl = &provider4_vtbl;
    impl->ref = 1;
    impl->system_provider = system_provider;
    if (FAILED(hr = WindowsDuplicateString( id, &impl->id )) ||
        FAILED(hr = WindowsDuplicateString( display_name, &impl->display_name )) ||
        FAILED(hr = WindowsDuplicateString( display_name, &impl->display_purpose )) ||
        FAILED(hr = WindowsDuplicateString( authority, &impl->authority )))
    {
        provider_release( impl );
        return hr;
    }
    *out = &impl->IWebAccountProvider_iface;
    return S_OK;
}

static inline struct web_account *account_from_IWebAccount( IWebAccount *iface )
{
    return CONTAINING_RECORD( iface, struct web_account, IWebAccount_iface );
}

static inline struct web_account *account_from_IWebAccount2( IWebAccount2 *iface )
{
    return CONTAINING_RECORD( iface, struct web_account, IWebAccount2_iface );
}

static HRESULT account_query_interface( struct web_account *impl, REFIID iid, void **out )
{
    if (!out) return E_POINTER;
    *out = NULL;
    if (IsEqualGUID( iid, &IID_IUnknown ) || IsEqualGUID( iid, &IID_IInspectable ) ||
        IsEqualGUID( iid, &IID_IAgileObject ) || IsEqualGUID( iid, &IID_IWebAccount ))
        *out = &impl->IWebAccount_iface;
    else if (IsEqualGUID( iid, &IID_IWebAccount2 ))
        *out = &impl->IWebAccount2_iface;
    else
        return E_NOINTERFACE;
    InterlockedIncrement( &impl->ref );
    return S_OK;
}

static ULONG account_release( struct web_account *impl )
{
    ULONG ref = InterlockedDecrement( &impl->ref );
    if (!ref)
    {
        if (impl->provider) IWebAccountProvider_Release( impl->provider );
        if (impl->properties) IMap_HSTRING_HSTRING_Release( impl->properties );
        WindowsDeleteString( impl->id );
        WindowsDeleteString( impl->username );
        free( impl );
    }
    return ref;
}

static HRESULT account_get_iids( ULONG *count, IID **iids )
{
    if (!count || !iids) return E_POINTER;
    *count = 2;
    if (!(*iids = CoTaskMemAlloc( 2 * sizeof(**iids) ))) return E_OUTOFMEMORY;
    (*iids)[0] = IID_IWebAccount;
    (*iids)[1] = IID_IWebAccount2;
    return S_OK;
}

static HRESULT account_get_class_name( HSTRING *name )
{
    const WCHAR *class_name = RuntimeClass_Windows_Security_Credentials_WebAccount;
    if (!name) return E_POINTER;
    return WindowsCreateString( class_name, wcslen( class_name ), name );
}

#define ACCOUNT_INSPECTABLE_METHODS( pfx, iface_type, from )                                     \
    static HRESULT WINAPI pfx##_QueryInterface( iface_type *iface, REFIID iid, void **out )       \
    { return account_query_interface( from( iface ), iid, out ); }                                \
    static ULONG WINAPI pfx##_AddRef( iface_type *iface )                                         \
    { return InterlockedIncrement( &from( iface )->ref ); }                                        \
    static ULONG WINAPI pfx##_Release( iface_type *iface )                                        \
    { return account_release( from( iface ) ); }                                                   \
    static HRESULT WINAPI pfx##_GetIids( iface_type *iface, ULONG *count, IID **iids )             \
    { return account_get_iids( count, iids ); }                                                    \
    static HRESULT WINAPI pfx##_GetRuntimeClassName( iface_type *iface, HSTRING *name )            \
    { return account_get_class_name( name ); }                                                     \
    static HRESULT WINAPI pfx##_GetTrustLevel( iface_type *iface, TrustLevel *level )              \
    { return provider_get_trust_level( level ); }

ACCOUNT_INSPECTABLE_METHODS( account, IWebAccount, account_from_IWebAccount )
ACCOUNT_INSPECTABLE_METHODS( account2, IWebAccount2, account_from_IWebAccount2 )

static HRESULT WINAPI account_get_WebAccountProvider( IWebAccount *iface, IWebAccountProvider **value )
{
    struct web_account *impl = account_from_IWebAccount( iface );
    if (!value) return E_POINTER;
    *value = impl->provider;
    IWebAccountProvider_AddRef( *value );
    return S_OK;
}

static HRESULT WINAPI account_get_UserName( IWebAccount *iface, HSTRING *value )
{
    return WindowsDuplicateString( account_from_IWebAccount( iface )->username, value );
}

static HRESULT WINAPI account_get_State( IWebAccount *iface, WebAccountState *value )
{
    if (!value) return E_POINTER;
    *value = account_from_IWebAccount( iface )->state;
    return S_OK;
}

static const IWebAccountVtbl account_vtbl =
{
    account_QueryInterface,
    account_AddRef,
    account_Release,
    account_GetIids,
    account_GetRuntimeClassName,
    account_GetTrustLevel,
    account_get_WebAccountProvider,
    account_get_UserName,
    account_get_State,
};

static HRESULT WINAPI account2_get_Id( IWebAccount2 *iface, HSTRING *value )
{
    return WindowsDuplicateString( account_from_IWebAccount2( iface )->id, value );
}

static HRESULT WINAPI account2_get_Properties( IWebAccount2 *iface, IMapView_HSTRING_HSTRING **value )
{
    return IMap_HSTRING_HSTRING_GetView( account_from_IWebAccount2( iface )->properties, value );
}

static HRESULT WINAPI account2_GetPictureAsync( IWebAccount2 *iface, INT32 size,
                                                 IAsyncOperation_IRandomAccessStream **operation )
{
    if (operation) *operation = NULL;
    return E_NOTIMPL;
}

static HRESULT WINAPI account2_SignOutAsync( IWebAccount2 *iface, IAsyncAction **operation )
{
    if (operation) *operation = NULL;
    return E_NOTIMPL;
}

static HRESULT WINAPI account2_SignOutWithClientIdAsync( IWebAccount2 *iface, HSTRING client_id,
                                                         IAsyncAction **operation )
{
    if (operation) *operation = NULL;
    return E_NOTIMPL;
}

static const IWebAccount2Vtbl account2_vtbl =
{
    account2_QueryInterface,
    account2_AddRef,
    account2_Release,
    account2_GetIids,
    account2_GetRuntimeClassName,
    account2_GetTrustLevel,
    account2_get_Id,
    account2_get_Properties,
    account2_GetPictureAsync,
    account2_SignOutAsync,
    account2_SignOutWithClientIdAsync,
};

HRESULT web_account_create( IWebAccountProvider *provider, HSTRING id, HSTRING username,
                            WebAccountState state, IWebAccount **out )
{
    struct web_account *impl;
    HRESULT hr;

    if (!provider || !out) return E_POINTER;
    *out = NULL;
    if (!(impl = calloc( 1, sizeof(*impl) ))) return E_OUTOFMEMORY;
    impl->IWebAccount_iface.lpVtbl = &account_vtbl;
    impl->IWebAccount2_iface.lpVtbl = &account2_vtbl;
    impl->ref = 1;
    impl->provider = provider;
    impl->state = state;
    IWebAccountProvider_AddRef( provider );
    if (FAILED(hr = WindowsDuplicateString( id, &impl->id )) ||
        FAILED(hr = WindowsDuplicateString( username, &impl->username )) ||
        FAILED(hr = string_map_create( &impl->properties )))
    {
        account_release( impl );
        return hr;
    }
    *out = &impl->IWebAccount_iface;
    return S_OK;
}

HRESULT web_account_set_property( IWebAccount *account, const WCHAR *name, HSTRING value )
{
    HSTRING key;
    boolean replaced;
    HRESULT hr;

    if (!account || !name) return E_POINTER;
    if (FAILED(hr = WindowsCreateString( name, wcslen(name), &key ))) return hr;
    hr = IMap_HSTRING_HSTRING_Insert( account_from_IWebAccount(account)->properties, key, value, &replaced );
    WindowsDeleteString( key );
    return hr;
}

HRESULT web_account_set_customer_id( IWebAccount *account, HSTRING authority )
{
    static const WCHAR personal_tenant[] = L"9188040d-6c67-4c5b-b112-36a304b66dad";
    struct web_account *impl = account_from_IWebAccount( account );
    const WCHAR *id = WindowsGetStringRawBuffer( impl->id, NULL );
    WCHAR buffer[16];
    HSTRING value;
    UINT32 i, count = 0;
    HRESULT hr;

    if ((wcsicmp( WindowsGetStringRawBuffer(authority, NULL), personal_tenant ) &&
         wcsicmp( WindowsGetStringRawBuffer(authority, NULL), L"consumers" )) ||
        WindowsGetStringLen( impl->id ) != 36 || wcsncmp( id, L"00000000-0000-0000-", 19 ))
        return S_OK;
    for (i = 19; i < 36; ++i)
    {
        WCHAR c = id[i];
        if (i == 23)
        {
            if (c != '-') return S_OK;
            continue;
        }
        if (c >= 'A' && c <= 'F') c += 'a' - 'A';
        if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'))) return S_OK;
        buffer[count++] = c;
    }
    if (FAILED(hr = WindowsCreateString( buffer, count, &value ))) return hr;
    hr = web_account_set_property( account, L"SafeCustomerId", value );
    WindowsDeleteString( value );
    return hr;
}

static inline struct account_factory *account_factory_from_activation( IActivationFactory *iface )
{
    return CONTAINING_RECORD( iface, struct account_factory, IActivationFactory_iface );
}

static inline struct account_factory *account_factory_from_factory( IWebAccountFactory *iface )
{
    return CONTAINING_RECORD( iface, struct account_factory, IWebAccountFactory_iface );
}

static HRESULT account_factory_query_interface( struct account_factory *impl, REFIID iid, void **out )
{
    if (!out) return E_POINTER;
    *out = NULL;
    if (IsEqualGUID( iid, &IID_IUnknown ) || IsEqualGUID( iid, &IID_IInspectable ) ||
        IsEqualGUID( iid, &IID_IAgileObject ) || IsEqualGUID( iid, &IID_IActivationFactory ))
        *out = &impl->IActivationFactory_iface;
    else if (IsEqualGUID( iid, &IID_IWebAccountFactory ))
        *out = &impl->IWebAccountFactory_iface;
    else
        return E_NOINTERFACE;
    InterlockedIncrement( &impl->ref );
    return S_OK;
}

#define ACCOUNT_FACTORY_INSPECTABLE( pfx, iface_type, from )                                      \
    static HRESULT WINAPI pfx##_QueryInterface( iface_type *iface, REFIID iid, void **out )        \
    { return account_factory_query_interface( from( iface ), iid, out ); }                         \
    static ULONG WINAPI pfx##_AddRef( iface_type *iface )                                          \
    { return InterlockedIncrement( &from( iface )->ref ); }                                         \
    static ULONG WINAPI pfx##_Release( iface_type *iface )                                         \
    { return InterlockedDecrement( &from( iface )->ref ); }                                         \
    static HRESULT WINAPI pfx##_GetIids( iface_type *iface, ULONG *count, IID **iids )              \
    { if (!count || !iids) return E_POINTER; *count = 1;                                            \
      if (!(*iids = CoTaskMemAlloc( sizeof(**iids) ))) return E_OUTOFMEMORY;                         \
      **iids = IID_IWebAccountFactory; return S_OK; }                                               \
    static HRESULT WINAPI pfx##_GetRuntimeClassName( iface_type *iface, HSTRING *name )             \
    { return account_get_class_name( name ); }                                                      \
    static HRESULT WINAPI pfx##_GetTrustLevel( iface_type *iface, TrustLevel *level )               \
    { return provider_get_trust_level( level ); }

ACCOUNT_FACTORY_INSPECTABLE( account_activation, IActivationFactory, account_factory_from_activation )
ACCOUNT_FACTORY_INSPECTABLE( account_factory, IWebAccountFactory, account_factory_from_factory )

static HRESULT WINAPI account_activation_ActivateInstance( IActivationFactory *iface, IInspectable **instance )
{
    if (instance) *instance = NULL;
    return E_NOTIMPL;
}

static const IActivationFactoryVtbl account_activation_vtbl =
{
    account_activation_QueryInterface,
    account_activation_AddRef,
    account_activation_Release,
    account_activation_GetIids,
    account_activation_GetRuntimeClassName,
    account_activation_GetTrustLevel,
    account_activation_ActivateInstance,
};

static HRESULT WINAPI account_factory_CreateWebAccount( IWebAccountFactory *iface, IWebAccountProvider *provider,
                                                        HSTRING username, WebAccountState state,
                                                        IWebAccount **value )
{
    return web_account_create( provider, username, username, state, value );
}

static const IWebAccountFactoryVtbl account_factory_vtbl =
{
    account_factory_QueryInterface,
    account_factory_AddRef,
    account_factory_Release,
    account_factory_GetIids,
    account_factory_GetRuntimeClassName,
    account_factory_GetTrustLevel,
    account_factory_CreateWebAccount,
};

static struct account_factory account_factory_impl =
{
    {&account_activation_vtbl},
    {&account_factory_vtbl},
    1,
};

IActivationFactory *web_account_factory = &account_factory_impl.IActivationFactory_iface;

static inline struct provider_factory *provider_factory_from_activation( IActivationFactory *iface )
{
    return CONTAINING_RECORD( iface, struct provider_factory, IActivationFactory_iface );
}

static inline struct provider_factory *provider_factory_from_factory( IWebAccountProviderFactory *iface )
{
    return CONTAINING_RECORD( iface, struct provider_factory, IWebAccountProviderFactory_iface );
}

static HRESULT provider_factory_query_interface( struct provider_factory *impl, REFIID iid, void **out )
{
    if (!out) return E_POINTER;
    *out = NULL;
    if (IsEqualGUID( iid, &IID_IUnknown ) || IsEqualGUID( iid, &IID_IInspectable ) ||
        IsEqualGUID( iid, &IID_IAgileObject ) || IsEqualGUID( iid, &IID_IActivationFactory ))
        *out = &impl->IActivationFactory_iface;
    else if (IsEqualGUID( iid, &IID_IWebAccountProviderFactory ))
        *out = &impl->IWebAccountProviderFactory_iface;
    else
        return E_NOINTERFACE;
    InterlockedIncrement( &impl->ref );
    return S_OK;
}

#define PROVIDER_FACTORY_INSPECTABLE( pfx, iface_type, from )                                     \
    static HRESULT WINAPI pfx##_QueryInterface( iface_type *iface, REFIID iid, void **out )        \
    { return provider_factory_query_interface( from( iface ), iid, out ); }                        \
    static ULONG WINAPI pfx##_AddRef( iface_type *iface )                                          \
    { return InterlockedIncrement( &from( iface )->ref ); }                                         \
    static ULONG WINAPI pfx##_Release( iface_type *iface )                                         \
    { return InterlockedDecrement( &from( iface )->ref ); }                                         \
    static HRESULT WINAPI pfx##_GetIids( iface_type *iface, ULONG *count, IID **iids )              \
    { if (!count || !iids) return E_POINTER; *count = 1;                                            \
      if (!(*iids = CoTaskMemAlloc( sizeof(**iids) ))) return E_OUTOFMEMORY;                         \
      **iids = IID_IWebAccountProviderFactory; return S_OK; }                                       \
    static HRESULT WINAPI pfx##_GetRuntimeClassName( iface_type *iface, HSTRING *name )             \
    { return provider_get_class_name( name ); }                                                     \
    static HRESULT WINAPI pfx##_GetTrustLevel( iface_type *iface, TrustLevel *level )               \
    { return provider_get_trust_level( level ); }

PROVIDER_FACTORY_INSPECTABLE( provider_activation, IActivationFactory, provider_factory_from_activation )
PROVIDER_FACTORY_INSPECTABLE( provider_factory, IWebAccountProviderFactory, provider_factory_from_factory )

static HRESULT WINAPI provider_activation_ActivateInstance( IActivationFactory *iface, IInspectable **instance )
{
    if (instance) *instance = NULL;
    return E_NOTIMPL;
}

static const IActivationFactoryVtbl provider_activation_vtbl =
{
    provider_activation_QueryInterface,
    provider_activation_AddRef,
    provider_activation_Release,
    provider_activation_GetIids,
    provider_activation_GetRuntimeClassName,
    provider_activation_GetTrustLevel,
    provider_activation_ActivateInstance,
};

static HRESULT WINAPI provider_factory_CreateWebAccountProvider( IWebAccountProviderFactory *iface, HSTRING id,
                                                                 HSTRING display_name, IUriRuntimeClass *icon_uri,
                                                                 IWebAccountProvider **value )
{
    HSTRING authority = NULL;
    HRESULT hr;
    if (FAILED(hr = WindowsCreateString( L"", 0, &authority ))) return hr;
    hr = web_account_provider_create( id, display_name, authority, FALSE, value );
    WindowsDeleteString( authority );
    return hr;
}

static const IWebAccountProviderFactoryVtbl provider_factory_vtbl =
{
    provider_factory_QueryInterface,
    provider_factory_AddRef,
    provider_factory_Release,
    provider_factory_GetIids,
    provider_factory_GetRuntimeClassName,
    provider_factory_GetTrustLevel,
    provider_factory_CreateWebAccountProvider,
};

static struct provider_factory provider_factory_impl =
{
    {&provider_activation_vtbl},
    {&provider_factory_vtbl},
    1,
};

IActivationFactory *web_account_provider_factory = &provider_factory_impl.IActivationFactory_iface;
