/*
 * WinRT web token objects
 *
 * Copyright (C) 2026 Mirko Brombin
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 */

#include <stdio.h>
#include "private.h"

struct web_token_request
{
    IWebTokenRequest IWebTokenRequest_iface;
    IWebTokenRequest2 IWebTokenRequest2_iface;
    IWebTokenRequest3 IWebTokenRequest3_iface;
    LONG ref;
    IWebAccountProvider *provider;
    HSTRING scope;
    HSTRING client_id;
    HSTRING correlation_id;
    WebTokenRequestPromptType prompt_type;
    IMap_HSTRING_HSTRING *properties;
    IMap_HSTRING_HSTRING *app_properties;
};

struct request_factory
{
    IActivationFactory IActivationFactory_iface;
    IWebTokenRequestFactory IWebTokenRequestFactory_iface;
    LONG ref;
};

static inline struct web_token_request *request_from_IWebTokenRequest( IWebTokenRequest *iface )
{
    return CONTAINING_RECORD( iface, struct web_token_request, IWebTokenRequest_iface );
}

static inline struct web_token_request *request_from_IWebTokenRequest2( IWebTokenRequest2 *iface )
{
    return CONTAINING_RECORD( iface, struct web_token_request, IWebTokenRequest2_iface );
}

static inline struct web_token_request *request_from_IWebTokenRequest3( IWebTokenRequest3 *iface )
{
    return CONTAINING_RECORD( iface, struct web_token_request, IWebTokenRequest3_iface );
}

static HRESULT request_query_interface( struct web_token_request *impl, REFIID iid, void **out )
{
    if (!out) return E_POINTER;
    *out = NULL;
    if (IsEqualGUID( iid, &IID_IUnknown ) || IsEqualGUID( iid, &IID_IInspectable ) ||
        IsEqualGUID( iid, &IID_IAgileObject ) || IsEqualGUID( iid, &IID_IWebTokenRequest ))
        *out = &impl->IWebTokenRequest_iface;
    else if (IsEqualGUID( iid, &IID_IWebTokenRequest2 ))
        *out = &impl->IWebTokenRequest2_iface;
    else if (IsEqualGUID( iid, &IID_IWebTokenRequest3 ))
        *out = &impl->IWebTokenRequest3_iface;
    else
        return E_NOINTERFACE;
    InterlockedIncrement( &impl->ref );
    return S_OK;
}

static ULONG request_release( struct web_token_request *impl )
{
    ULONG ref = InterlockedDecrement( &impl->ref );
    if (!ref)
    {
        if (impl->provider) IWebAccountProvider_Release( impl->provider );
        if (impl->properties) IMap_HSTRING_HSTRING_Release( impl->properties );
        if (impl->app_properties) IMap_HSTRING_HSTRING_Release( impl->app_properties );
        WindowsDeleteString( impl->scope );
        WindowsDeleteString( impl->client_id );
        WindowsDeleteString( impl->correlation_id );
        free( impl );
    }
    return ref;
}

static HRESULT request_get_iids( ULONG *count, IID **iids )
{
    if (!count || !iids) return E_POINTER;
    *count = 3;
    if (!(*iids = CoTaskMemAlloc( 3 * sizeof(**iids) ))) return E_OUTOFMEMORY;
    (*iids)[0] = IID_IWebTokenRequest;
    (*iids)[1] = IID_IWebTokenRequest2;
    (*iids)[2] = IID_IWebTokenRequest3;
    return S_OK;
}

static HRESULT request_get_class_name( HSTRING *name )
{
    const WCHAR *class_name = RuntimeClass_Windows_Security_Authentication_Web_Core_WebTokenRequest;
    if (!name) return E_POINTER;
    return WindowsCreateString( class_name, wcslen( class_name ), name );
}

static HRESULT web_get_trust_level( TrustLevel *level )
{
    if (!level) return E_POINTER;
    *level = BaseTrust;
    return S_OK;
}

#define REQUEST_INSPECTABLE_METHODS( pfx, iface_type, from )                                      \
    static HRESULT WINAPI pfx##_QueryInterface( iface_type *iface, REFIID iid, void **out )        \
    { return request_query_interface( from( iface ), iid, out ); }                                 \
    static ULONG WINAPI pfx##_AddRef( iface_type *iface )                                          \
    { return InterlockedIncrement( &from( iface )->ref ); }                                         \
    static ULONG WINAPI pfx##_Release( iface_type *iface )                                         \
    { return request_release( from( iface ) ); }                                                    \
    static HRESULT WINAPI pfx##_GetIids( iface_type *iface, ULONG *count, IID **iids )              \
    { return request_get_iids( count, iids ); }                                                     \
    static HRESULT WINAPI pfx##_GetRuntimeClassName( iface_type *iface, HSTRING *name )             \
    { return request_get_class_name( name ); }                                                      \
    static HRESULT WINAPI pfx##_GetTrustLevel( iface_type *iface, TrustLevel *level )               \
    { return web_get_trust_level( level ); }

REQUEST_INSPECTABLE_METHODS( request, IWebTokenRequest, request_from_IWebTokenRequest )
REQUEST_INSPECTABLE_METHODS( request2, IWebTokenRequest2, request_from_IWebTokenRequest2 )
REQUEST_INSPECTABLE_METHODS( request3, IWebTokenRequest3, request_from_IWebTokenRequest3 )

static HRESULT WINAPI request_get_WebAccountProvider( IWebTokenRequest *iface, IWebAccountProvider **value )
{
    struct web_token_request *impl = request_from_IWebTokenRequest( iface );
    if (!value) return E_POINTER;
    *value = impl->provider;
    IWebAccountProvider_AddRef( *value );
    return S_OK;
}

static HRESULT WINAPI request_get_Scope( IWebTokenRequest *iface, HSTRING *value )
{
    return WindowsDuplicateString( request_from_IWebTokenRequest( iface )->scope, value );
}

static HRESULT WINAPI request_get_ClientId( IWebTokenRequest *iface, HSTRING *value )
{
    return WindowsDuplicateString( request_from_IWebTokenRequest( iface )->client_id, value );
}

static HRESULT WINAPI request_get_PromptType( IWebTokenRequest *iface, WebTokenRequestPromptType *value )
{
    if (!value) return E_POINTER;
    *value = request_from_IWebTokenRequest( iface )->prompt_type;
    return S_OK;
}

static HRESULT WINAPI request_get_Properties( IWebTokenRequest *iface, IMap_HSTRING_HSTRING **value )
{
    struct web_token_request *impl = request_from_IWebTokenRequest( iface );
    if (!value) return E_POINTER;
    *value = impl->properties;
    IMap_HSTRING_HSTRING_AddRef( *value );
    return S_OK;
}

static const IWebTokenRequestVtbl request_vtbl =
{
    request_QueryInterface,
    request_AddRef,
    request_Release,
    request_GetIids,
    request_GetRuntimeClassName,
    request_GetTrustLevel,
    request_get_WebAccountProvider,
    request_get_Scope,
    request_get_ClientId,
    request_get_PromptType,
    request_get_Properties,
};

static HRESULT WINAPI request2_get_AppProperties( IWebTokenRequest2 *iface, IMap_HSTRING_HSTRING **value )
{
    struct web_token_request *impl = request_from_IWebTokenRequest2( iface );
    if (!value) return E_POINTER;
    *value = impl->app_properties;
    IMap_HSTRING_HSTRING_AddRef( *value );
    return S_OK;
}

static const IWebTokenRequest2Vtbl request2_vtbl =
{
    request2_QueryInterface,
    request2_AddRef,
    request2_Release,
    request2_GetIids,
    request2_GetRuntimeClassName,
    request2_GetTrustLevel,
    request2_get_AppProperties,
};

static HRESULT WINAPI request3_get_CorrelationId( IWebTokenRequest3 *iface, HSTRING *value )
{
    return WindowsDuplicateString( request_from_IWebTokenRequest3( iface )->correlation_id, value );
}

static HRESULT WINAPI request3_put_CorrelationId( IWebTokenRequest3 *iface, HSTRING value )
{
    struct web_token_request *impl = request_from_IWebTokenRequest3( iface );
    HSTRING copy;
    HRESULT hr;
    if (FAILED(hr = WindowsDuplicateString( value, &copy ))) return hr;
    WindowsDeleteString( impl->correlation_id );
    impl->correlation_id = copy;
    return S_OK;
}

static const IWebTokenRequest3Vtbl request3_vtbl =
{
    request3_QueryInterface,
    request3_AddRef,
    request3_Release,
    request3_GetIids,
    request3_GetRuntimeClassName,
    request3_GetTrustLevel,
    request3_get_CorrelationId,
    request3_put_CorrelationId,
};

HRESULT web_token_request_create( IWebAccountProvider *provider, HSTRING scope, HSTRING client_id,
                                  WebTokenRequestPromptType prompt_type, IWebTokenRequest **out )
{
    struct web_token_request *impl;
    HRESULT hr;

    if (!provider || !out) return E_POINTER;
    *out = NULL;
    if (!(impl = calloc( 1, sizeof(*impl) ))) return E_OUTOFMEMORY;
    impl->IWebTokenRequest_iface.lpVtbl = &request_vtbl;
    impl->IWebTokenRequest2_iface.lpVtbl = &request2_vtbl;
    impl->IWebTokenRequest3_iface.lpVtbl = &request3_vtbl;
    impl->ref = 1;
    impl->provider = provider;
    impl->prompt_type = prompt_type;
    IWebAccountProvider_AddRef( provider );
    if (FAILED(hr = WindowsDuplicateString( scope, &impl->scope )) ||
        FAILED(hr = WindowsDuplicateString( client_id, &impl->client_id )) ||
        FAILED(hr = WindowsCreateString( L"", 0, &impl->correlation_id )) ||
        FAILED(hr = string_map_create( &impl->properties )) ||
        FAILED(hr = string_map_create( &impl->app_properties )))
    {
        request_release( impl );
        return hr;
    }
    *out = &impl->IWebTokenRequest_iface;
    return S_OK;
}

static inline struct request_factory *request_factory_from_activation( IActivationFactory *iface )
{
    return CONTAINING_RECORD( iface, struct request_factory, IActivationFactory_iface );
}

static inline struct request_factory *request_factory_from_factory( IWebTokenRequestFactory *iface )
{
    return CONTAINING_RECORD( iface, struct request_factory, IWebTokenRequestFactory_iface );
}

static HRESULT request_factory_query_interface( struct request_factory *impl, REFIID iid, void **out )
{
    if (!out) return E_POINTER;
    *out = NULL;
    if (IsEqualGUID( iid, &IID_IUnknown ) || IsEqualGUID( iid, &IID_IInspectable ) ||
        IsEqualGUID( iid, &IID_IAgileObject ) || IsEqualGUID( iid, &IID_IActivationFactory ))
        *out = &impl->IActivationFactory_iface;
    else if (IsEqualGUID( iid, &IID_IWebTokenRequestFactory ))
        *out = &impl->IWebTokenRequestFactory_iface;
    else
        return E_NOINTERFACE;
    InterlockedIncrement( &impl->ref );
    return S_OK;
}

#define REQUEST_FACTORY_INSPECTABLE( pfx, iface_type, from )                                      \
    static HRESULT WINAPI pfx##_QueryInterface( iface_type *iface, REFIID iid, void **out )        \
    { return request_factory_query_interface( from( iface ), iid, out ); }                         \
    static ULONG WINAPI pfx##_AddRef( iface_type *iface )                                          \
    { return InterlockedIncrement( &from( iface )->ref ); }                                         \
    static ULONG WINAPI pfx##_Release( iface_type *iface )                                         \
    { return InterlockedDecrement( &from( iface )->ref ); }                                         \
    static HRESULT WINAPI pfx##_GetIids( iface_type *iface, ULONG *count, IID **iids )              \
    { if (!count || !iids) return E_POINTER; *count = 1;                                            \
      if (!(*iids = CoTaskMemAlloc( sizeof(**iids) ))) return E_OUTOFMEMORY;                         \
      **iids = IID_IWebTokenRequestFactory; return S_OK; }                                          \
    static HRESULT WINAPI pfx##_GetRuntimeClassName( iface_type *iface, HSTRING *name )             \
    { return request_get_class_name( name ); }                                                      \
    static HRESULT WINAPI pfx##_GetTrustLevel( iface_type *iface, TrustLevel *level )               \
    { return web_get_trust_level( level ); }

REQUEST_FACTORY_INSPECTABLE( request_activation, IActivationFactory, request_factory_from_activation )
REQUEST_FACTORY_INSPECTABLE( request_factory, IWebTokenRequestFactory, request_factory_from_factory )

static HRESULT WINAPI request_activation_ActivateInstance( IActivationFactory *iface, IInspectable **instance )
{
    if (instance) *instance = NULL;
    return E_NOTIMPL;
}

static const IActivationFactoryVtbl request_activation_vtbl =
{
    request_activation_QueryInterface,
    request_activation_AddRef,
    request_activation_Release,
    request_activation_GetIids,
    request_activation_GetRuntimeClassName,
    request_activation_GetTrustLevel,
    request_activation_ActivateInstance,
};

static HRESULT WINAPI request_factory_Create( IWebTokenRequestFactory *iface, IWebAccountProvider *provider,
                                              HSTRING scope, HSTRING client_id, IWebTokenRequest **value )
{
    return web_token_request_create( provider, scope, client_id, WebTokenRequestPromptType_Default, value );
}

static HRESULT WINAPI request_factory_CreateWithPromptType( IWebTokenRequestFactory *iface,
                                                            IWebAccountProvider *provider, HSTRING scope,
                                                            HSTRING client_id, WebTokenRequestPromptType prompt_type,
                                                            IWebTokenRequest **value )
{
    return web_token_request_create( provider, scope, client_id, prompt_type, value );
}

static HRESULT create_request_with_defaults( IWebAccountProvider *provider, HSTRING scope,
                                             IWebTokenRequest **value )
{
    HSTRING client_id = NULL;
    HRESULT hr;
    if (FAILED(hr = WindowsCreateString( L"", 0, &client_id ))) return hr;
    hr = web_token_request_create( provider, scope, client_id, WebTokenRequestPromptType_Default, value );
    WindowsDeleteString( client_id );
    return hr;
}

static HRESULT WINAPI request_factory_CreateWithProvider( IWebTokenRequestFactory *iface,
                                                          IWebAccountProvider *provider,
                                                          IWebTokenRequest **value )
{
    HSTRING scope = NULL;
    HRESULT hr;
    if (FAILED(hr = WindowsCreateString( L"", 0, &scope ))) return hr;
    hr = create_request_with_defaults( provider, scope, value );
    WindowsDeleteString( scope );
    return hr;
}

static HRESULT WINAPI request_factory_CreateWithScope( IWebTokenRequestFactory *iface,
                                                       IWebAccountProvider *provider, HSTRING scope,
                                                       IWebTokenRequest **value )
{
    return create_request_with_defaults( provider, scope, value );
}

static const IWebTokenRequestFactoryVtbl request_factory_vtbl =
{
    request_factory_QueryInterface,
    request_factory_AddRef,
    request_factory_Release,
    request_factory_GetIids,
    request_factory_GetRuntimeClassName,
    request_factory_GetTrustLevel,
    request_factory_Create,
    request_factory_CreateWithPromptType,
    request_factory_CreateWithProvider,
    request_factory_CreateWithScope,
};

static struct request_factory request_factory_impl =
{
    {&request_activation_vtbl},
    {&request_factory_vtbl},
    1,
};

IActivationFactory *web_token_request_factory = &request_factory_impl.IActivationFactory_iface;

struct web_token_response
{
    IWebTokenResponse IWebTokenResponse_iface;
    LONG ref;
    HSTRING token;
    IWebAccount *account;
    IWebProviderError *error;
    IMap_HSTRING_HSTRING *properties;
};

struct response_factory
{
    IActivationFactory IActivationFactory_iface;
    IWebTokenResponseFactory IWebTokenResponseFactory_iface;
    LONG ref;
};

static inline struct web_token_response *response_from_IWebTokenResponse( IWebTokenResponse *iface )
{
    return CONTAINING_RECORD( iface, struct web_token_response, IWebTokenResponse_iface );
}

static HRESULT WINAPI response_QueryInterface( IWebTokenResponse *iface, REFIID iid, void **out )
{
    if (!out) return E_POINTER;
    *out = NULL;
    if (IsEqualGUID( iid, &IID_IUnknown ) || IsEqualGUID( iid, &IID_IInspectable ) ||
        IsEqualGUID( iid, &IID_IAgileObject ) || IsEqualGUID( iid, &IID_IWebTokenResponse ))
        *out = iface;
    if (!*out) return E_NOINTERFACE;
    IWebTokenResponse_AddRef( iface );
    return S_OK;
}

static ULONG WINAPI response_AddRef( IWebTokenResponse *iface )
{
    return InterlockedIncrement( &response_from_IWebTokenResponse( iface )->ref );
}

static ULONG WINAPI response_Release( IWebTokenResponse *iface )
{
    struct web_token_response *impl = response_from_IWebTokenResponse( iface );
    ULONG ref = InterlockedDecrement( &impl->ref );
    if (!ref)
    {
        if (impl->account) IWebAccount_Release( impl->account );
        if (impl->error) IWebProviderError_Release( impl->error );
        if (impl->properties) IMap_HSTRING_HSTRING_Release( impl->properties );
        WindowsDeleteString( impl->token );
        free( impl );
    }
    return ref;
}

static HRESULT WINAPI response_GetIids( IWebTokenResponse *iface, ULONG *count, IID **iids )
{
    if (!count || !iids) return E_POINTER;
    *count = 1;
    if (!(*iids = CoTaskMemAlloc( sizeof(**iids) ))) return E_OUTOFMEMORY;
    **iids = IID_IWebTokenResponse;
    return S_OK;
}

static HRESULT response_get_class_name( HSTRING *name )
{
    const WCHAR *class_name = RuntimeClass_Windows_Security_Authentication_Web_Core_WebTokenResponse;
    if (!name) return E_POINTER;
    return WindowsCreateString( class_name, wcslen( class_name ), name );
}

static HRESULT WINAPI response_GetRuntimeClassName( IWebTokenResponse *iface, HSTRING *name )
{
    return response_get_class_name( name );
}

static HRESULT WINAPI response_GetTrustLevel( IWebTokenResponse *iface, TrustLevel *level )
{
    return web_get_trust_level( level );
}

static HRESULT WINAPI response_get_Token( IWebTokenResponse *iface, HSTRING *value )
{
    return WindowsDuplicateString( response_from_IWebTokenResponse( iface )->token, value );
}

static HRESULT WINAPI response_get_ProviderError( IWebTokenResponse *iface, IWebProviderError **value )
{
    struct web_token_response *impl = response_from_IWebTokenResponse( iface );
    if (!value) return E_POINTER;
    *value = impl->error;
    if (*value) IWebProviderError_AddRef( *value );
    return S_OK;
}

static HRESULT WINAPI response_get_WebAccount( IWebTokenResponse *iface, IWebAccount **value )
{
    struct web_token_response *impl = response_from_IWebTokenResponse( iface );
    if (!value) return E_POINTER;
    *value = impl->account;
    if (*value) IWebAccount_AddRef( *value );
    return S_OK;
}

static HRESULT WINAPI response_get_Properties( IWebTokenResponse *iface, IMap_HSTRING_HSTRING **value )
{
    struct web_token_response *impl = response_from_IWebTokenResponse( iface );
    if (!value) return E_POINTER;
    *value = impl->properties;
    IMap_HSTRING_HSTRING_AddRef( *value );
    return S_OK;
}

static const IWebTokenResponseVtbl response_vtbl =
{
    response_QueryInterface,
    response_AddRef,
    response_Release,
    response_GetIids,
    response_GetRuntimeClassName,
    response_GetTrustLevel,
    response_get_Token,
    response_get_ProviderError,
    response_get_WebAccount,
    response_get_Properties,
};

HRESULT web_token_response_create( HSTRING token, IWebAccount *account, IWebProviderError *error,
                                   IWebTokenResponse **out )
{
    struct web_token_response *impl;
    HRESULT hr;

    if (!out) return E_POINTER;
    *out = NULL;
    if (!(impl = calloc( 1, sizeof(*impl) ))) return E_OUTOFMEMORY;
    impl->IWebTokenResponse_iface.lpVtbl = &response_vtbl;
    impl->ref = 1;
    impl->account = account;
    impl->error = error;
    if (account) IWebAccount_AddRef( account );
    if (error) IWebProviderError_AddRef( error );
    if (FAILED(hr = WindowsDuplicateString( token, &impl->token )) ||
        FAILED(hr = string_map_create( &impl->properties )))
    {
        response_Release( &impl->IWebTokenResponse_iface );
        return hr;
    }
    *out = &impl->IWebTokenResponse_iface;
    return S_OK;
}

static inline struct response_factory *response_factory_from_activation( IActivationFactory *iface )
{
    return CONTAINING_RECORD( iface, struct response_factory, IActivationFactory_iface );
}

static inline struct response_factory *response_factory_from_factory( IWebTokenResponseFactory *iface )
{
    return CONTAINING_RECORD( iface, struct response_factory, IWebTokenResponseFactory_iface );
}

static HRESULT response_factory_query_interface( struct response_factory *impl, REFIID iid, void **out )
{
    if (!out) return E_POINTER;
    *out = NULL;
    if (IsEqualGUID( iid, &IID_IUnknown ) || IsEqualGUID( iid, &IID_IInspectable ) ||
        IsEqualGUID( iid, &IID_IAgileObject ) || IsEqualGUID( iid, &IID_IActivationFactory ))
        *out = &impl->IActivationFactory_iface;
    else if (IsEqualGUID( iid, &IID_IWebTokenResponseFactory ))
        *out = &impl->IWebTokenResponseFactory_iface;
    else
        return E_NOINTERFACE;
    InterlockedIncrement( &impl->ref );
    return S_OK;
}

#define RESPONSE_FACTORY_INSPECTABLE( pfx, iface_type, from )                                     \
    static HRESULT WINAPI pfx##_QueryInterface( iface_type *iface, REFIID iid, void **out )        \
    { return response_factory_query_interface( from( iface ), iid, out ); }                        \
    static ULONG WINAPI pfx##_AddRef( iface_type *iface )                                          \
    { return InterlockedIncrement( &from( iface )->ref ); }                                         \
    static ULONG WINAPI pfx##_Release( iface_type *iface )                                         \
    { return InterlockedDecrement( &from( iface )->ref ); }                                         \
    static HRESULT WINAPI pfx##_GetIids( iface_type *iface, ULONG *count, IID **iids )              \
    { if (!count || !iids) return E_POINTER; *count = 1;                                            \
      if (!(*iids = CoTaskMemAlloc( sizeof(**iids) ))) return E_OUTOFMEMORY;                         \
      **iids = IID_IWebTokenResponseFactory; return S_OK; }                                         \
    static HRESULT WINAPI pfx##_GetRuntimeClassName( iface_type *iface, HSTRING *name )             \
    { return response_get_class_name( name ); }                                                     \
    static HRESULT WINAPI pfx##_GetTrustLevel( iface_type *iface, TrustLevel *level )               \
    { return web_get_trust_level( level ); }

RESPONSE_FACTORY_INSPECTABLE( response_activation, IActivationFactory, response_factory_from_activation )
RESPONSE_FACTORY_INSPECTABLE( response_factory, IWebTokenResponseFactory, response_factory_from_factory )

static HRESULT WINAPI response_activation_ActivateInstance( IActivationFactory *iface, IInspectable **instance )
{
    HSTRING token = NULL;
    IWebTokenResponse *response = NULL;
    HRESULT hr;
    if (!instance) return E_POINTER;
    *instance = NULL;
    if (FAILED(hr = WindowsCreateString( L"", 0, &token ))) return hr;
    hr = web_token_response_create( token, NULL, NULL, &response );
    WindowsDeleteString( token );
    if (SUCCEEDED(hr)) *instance = (IInspectable *)response;
    return hr;
}

static const IActivationFactoryVtbl response_activation_vtbl =
{
    response_activation_QueryInterface,
    response_activation_AddRef,
    response_activation_Release,
    response_activation_GetIids,
    response_activation_GetRuntimeClassName,
    response_activation_GetTrustLevel,
    response_activation_ActivateInstance,
};

static HRESULT WINAPI response_factory_CreateWithToken( IWebTokenResponseFactory *iface, HSTRING token,
                                                        IWebTokenResponse **value )
{
    return web_token_response_create( token, NULL, NULL, value );
}

static HRESULT WINAPI response_factory_CreateWithTokenAndAccount( IWebTokenResponseFactory *iface, HSTRING token,
                                                                  IWebAccount *account,
                                                                  IWebTokenResponse **value )
{
    return web_token_response_create( token, account, NULL, value );
}

static HRESULT WINAPI response_factory_CreateWithTokenAccountAndError( IWebTokenResponseFactory *iface,
                                                                       HSTRING token, IWebAccount *account,
                                                                       IWebProviderError *error,
                                                                       IWebTokenResponse **value )
{
    return web_token_response_create( token, account, error, value );
}

static const IWebTokenResponseFactoryVtbl response_factory_vtbl =
{
    response_factory_QueryInterface,
    response_factory_AddRef,
    response_factory_Release,
    response_factory_GetIids,
    response_factory_GetRuntimeClassName,
    response_factory_GetTrustLevel,
    response_factory_CreateWithToken,
    response_factory_CreateWithTokenAndAccount,
    response_factory_CreateWithTokenAccountAndError,
};

static struct response_factory response_factory_impl =
{
    {&response_activation_vtbl},
    {&response_factory_vtbl},
    1,
};

IActivationFactory *web_token_response_factory = &response_factory_impl.IActivationFactory_iface;

struct web_provider_error
{
    IWebProviderError IWebProviderError_iface;
    LONG ref;
    UINT32 error;
    HSTRING message;
    IMap_HSTRING_HSTRING *properties;
};

static inline struct web_provider_error *provider_error_from_IWebProviderError( IWebProviderError *iface )
{
    return CONTAINING_RECORD( iface, struct web_provider_error, IWebProviderError_iface );
}

static HRESULT WINAPI provider_error_QueryInterface( IWebProviderError *iface, REFIID iid, void **out )
{
    if (!out) return E_POINTER;
    *out = NULL;
    if (IsEqualGUID( iid, &IID_IUnknown ) || IsEqualGUID( iid, &IID_IInspectable ) ||
        IsEqualGUID( iid, &IID_IAgileObject ) || IsEqualGUID( iid, &IID_IWebProviderError ))
        *out = iface;
    if (!*out) return E_NOINTERFACE;
    IWebProviderError_AddRef( iface );
    return S_OK;
}

static ULONG WINAPI provider_error_AddRef( IWebProviderError *iface )
{
    return InterlockedIncrement( &provider_error_from_IWebProviderError( iface )->ref );
}

static ULONG WINAPI provider_error_Release( IWebProviderError *iface )
{
    struct web_provider_error *impl = provider_error_from_IWebProviderError( iface );
    ULONG ref = InterlockedDecrement( &impl->ref );
    if (!ref)
    {
        if (impl->properties) IMap_HSTRING_HSTRING_Release( impl->properties );
        WindowsDeleteString( impl->message );
        free( impl );
    }
    return ref;
}

static HRESULT WINAPI provider_error_GetIids( IWebProviderError *iface, ULONG *count, IID **iids )
{
    if (!count || !iids) return E_POINTER;
    *count = 1;
    if (!(*iids = CoTaskMemAlloc( sizeof(**iids) ))) return E_OUTOFMEMORY;
    **iids = IID_IWebProviderError;
    return S_OK;
}

static HRESULT WINAPI provider_error_GetRuntimeClassName( IWebProviderError *iface, HSTRING *name )
{
    const WCHAR *class_name = RuntimeClass_Windows_Security_Authentication_Web_Core_WebProviderError;
    if (!name) return E_POINTER;
    return WindowsCreateString( class_name, wcslen( class_name ), name );
}

static HRESULT WINAPI provider_error_GetTrustLevel( IWebProviderError *iface, TrustLevel *level )
{
    return web_get_trust_level( level );
}

static HRESULT WINAPI provider_error_get_ErrorCode( IWebProviderError *iface, UINT32 *value )
{
    if (!value) return E_POINTER;
    *value = provider_error_from_IWebProviderError( iface )->error;
    return S_OK;
}

static HRESULT WINAPI provider_error_get_ErrorMessage( IWebProviderError *iface, HSTRING *value )
{
    return WindowsDuplicateString( provider_error_from_IWebProviderError( iface )->message, value );
}

static HRESULT WINAPI provider_error_get_Properties( IWebProviderError *iface, IMap_HSTRING_HSTRING **value )
{
    struct web_provider_error *impl = provider_error_from_IWebProviderError( iface );
    if (!value) return E_POINTER;
    *value = impl->properties;
    IMap_HSTRING_HSTRING_AddRef( *value );
    return S_OK;
}

static const IWebProviderErrorVtbl provider_error_vtbl =
{
    provider_error_QueryInterface,
    provider_error_AddRef,
    provider_error_Release,
    provider_error_GetIids,
    provider_error_GetRuntimeClassName,
    provider_error_GetTrustLevel,
    provider_error_get_ErrorCode,
    provider_error_get_ErrorMessage,
    provider_error_get_Properties,
};

HRESULT web_provider_error_create( HRESULT error, HSTRING message, IWebProviderError **out )
{
    static const WCHAR fallback[] = L"Identity provider request failed.";
    struct web_provider_error *impl;
    HRESULT hr;

    if (!out) return E_POINTER;
    *out = NULL;
    if (!(impl = calloc( 1, sizeof(*impl) ))) return E_OUTOFMEMORY;
    impl->IWebProviderError_iface.lpVtbl = &provider_error_vtbl;
    impl->ref = 1;
    impl->error = error;
    if ((!message || WindowsIsStringEmpty(message)) &&
        FAILED(hr = WindowsCreateString( fallback, ARRAY_SIZE(fallback) - 1, &impl->message )))
        goto failed;
    if (message && !WindowsIsStringEmpty(message) &&
        FAILED(hr = WindowsDuplicateString( message, &impl->message )))
        goto failed;
    if (FAILED(hr = string_map_create( &impl->properties )))
        goto failed;
    *out = &impl->IWebProviderError_iface;
    return S_OK;

failed:
    provider_error_Release( &impl->IWebProviderError_iface );
    return hr;
}

struct response_view
{
    IVectorView_WebTokenResponse IVectorView_iface;
    LONG ref;
    IWebTokenResponse *response;
};

static inline struct response_view *view_from_IVectorView( IVectorView_WebTokenResponse *iface )
{
    return CONTAINING_RECORD( iface, struct response_view, IVectorView_iface );
}

static HRESULT WINAPI response_view_QueryInterface( IVectorView_WebTokenResponse *iface, REFIID iid, void **out )
{
    if (!out) return E_POINTER;
    *out = NULL;
    if (IsEqualGUID( iid, &IID_IUnknown ) || IsEqualGUID( iid, &IID_IInspectable ) ||
        IsEqualGUID( iid, &IID_IAgileObject ) || IsEqualGUID( iid, &IID_IVectorView_WebTokenResponse ))
        *out = iface;
    if (!*out) return E_NOINTERFACE;
    IVectorView_WebTokenResponse_AddRef( iface );
    return S_OK;
}

static ULONG WINAPI response_view_AddRef( IVectorView_WebTokenResponse *iface )
{
    return InterlockedIncrement( &view_from_IVectorView( iface )->ref );
}

static ULONG WINAPI response_view_Release( IVectorView_WebTokenResponse *iface )
{
    struct response_view *impl = view_from_IVectorView( iface );
    ULONG ref = InterlockedDecrement( &impl->ref );
    if (!ref)
    {
        if (impl->response) IWebTokenResponse_Release( impl->response );
        free( impl );
    }
    return ref;
}

static HRESULT WINAPI response_view_GetIids( IVectorView_WebTokenResponse *iface, ULONG *count, IID **iids )
{
    if (!count || !iids) return E_POINTER;
    *count = 1;
    if (!(*iids = CoTaskMemAlloc( sizeof(**iids) ))) return E_OUTOFMEMORY;
    **iids = IID_IVectorView_WebTokenResponse;
    return S_OK;
}

static HRESULT WINAPI response_view_GetRuntimeClassName( IVectorView_WebTokenResponse *iface, HSTRING *name )
{
    static const WCHAR class_name[] = L"Windows.Foundation.Collections.IVectorView`1<Windows.Security.Authentication.Web.Core.WebTokenResponse>";
    if (!name) return E_POINTER;
    return WindowsCreateString( class_name, ARRAY_SIZE(class_name) - 1, name );
}

static HRESULT WINAPI response_view_GetTrustLevel( IVectorView_WebTokenResponse *iface, TrustLevel *level )
{
    return web_get_trust_level( level );
}

static HRESULT WINAPI response_view_GetAt( IVectorView_WebTokenResponse *iface, UINT32 index,
                                           IWebTokenResponse **value )
{
    struct response_view *impl = view_from_IVectorView( iface );
    if (!value) return E_POINTER;
    *value = NULL;
    if (index || !impl->response) return E_BOUNDS;
    *value = impl->response;
    IWebTokenResponse_AddRef( *value );
    return S_OK;
}

static HRESULT WINAPI response_view_get_Size( IVectorView_WebTokenResponse *iface, UINT32 *value )
{
    if (!value) return E_POINTER;
    *value = !!view_from_IVectorView( iface )->response;
    return S_OK;
}

static HRESULT WINAPI response_view_IndexOf( IVectorView_WebTokenResponse *iface, IWebTokenResponse *element,
                                             UINT32 *index, boolean *found )
{
    struct response_view *impl = view_from_IVectorView( iface );
    if (!index || !found) return E_POINTER;
    *index = 0;
    *found = impl->response == element;
    return S_OK;
}

static HRESULT WINAPI response_view_GetMany( IVectorView_WebTokenResponse *iface, UINT32 start_index,
                                             UINT32 capacity, IWebTokenResponse **items, UINT32 *count )
{
    struct response_view *impl = view_from_IVectorView( iface );
    if (!count || (capacity && !items)) return E_POINTER;
    *count = 0;
    if (!capacity || start_index || !impl->response) return S_OK;
    items[0] = impl->response;
    IWebTokenResponse_AddRef( items[0] );
    *count = 1;
    return S_OK;
}

static const IVectorView_WebTokenResponseVtbl response_view_vtbl =
{
    response_view_QueryInterface,
    response_view_AddRef,
    response_view_Release,
    response_view_GetIids,
    response_view_GetRuntimeClassName,
    response_view_GetTrustLevel,
    response_view_GetAt,
    response_view_get_Size,
    response_view_IndexOf,
    response_view_GetMany,
};

static HRESULT response_view_create( IWebTokenResponse *response, IVectorView_WebTokenResponse **out )
{
    struct response_view *impl;
    if (!out) return E_POINTER;
    *out = NULL;
    if (!(impl = calloc( 1, sizeof(*impl) ))) return E_OUTOFMEMORY;
    impl->IVectorView_iface.lpVtbl = &response_view_vtbl;
    impl->ref = 1;
    impl->response = response;
    if (response) IWebTokenResponse_AddRef( response );
    *out = &impl->IVectorView_iface;
    return S_OK;
}

struct web_token_result
{
    IWebTokenRequestResult IWebTokenRequestResult_iface;
    LONG ref;
    IVectorView_WebTokenResponse *responses;
    IWebProviderError *error;
    WebTokenRequestStatus status;
};

static inline struct web_token_result *result_from_IWebTokenRequestResult( IWebTokenRequestResult *iface )
{
    return CONTAINING_RECORD( iface, struct web_token_result, IWebTokenRequestResult_iface );
}

static HRESULT WINAPI result_QueryInterface( IWebTokenRequestResult *iface, REFIID iid, void **out )
{
    if (!out) return E_POINTER;
    *out = NULL;
    if (IsEqualGUID( iid, &IID_IUnknown ) || IsEqualGUID( iid, &IID_IInspectable ) ||
        IsEqualGUID( iid, &IID_IAgileObject ) || IsEqualGUID( iid, &IID_IWebTokenRequestResult ))
        *out = iface;
    if (!*out) return E_NOINTERFACE;
    IWebTokenRequestResult_AddRef( iface );
    return S_OK;
}

static ULONG WINAPI result_AddRef( IWebTokenRequestResult *iface )
{
    return InterlockedIncrement( &result_from_IWebTokenRequestResult( iface )->ref );
}

static ULONG WINAPI result_Release( IWebTokenRequestResult *iface )
{
    struct web_token_result *impl = result_from_IWebTokenRequestResult( iface );
    ULONG ref = InterlockedDecrement( &impl->ref );
    if (!ref)
    {
        if (impl->responses) IVectorView_WebTokenResponse_Release( impl->responses );
        if (impl->error) IWebProviderError_Release( impl->error );
        free( impl );
    }
    return ref;
}

static HRESULT WINAPI result_GetIids( IWebTokenRequestResult *iface, ULONG *count, IID **iids )
{
    if (!count || !iids) return E_POINTER;
    *count = 1;
    if (!(*iids = CoTaskMemAlloc( sizeof(**iids) ))) return E_OUTOFMEMORY;
    **iids = IID_IWebTokenRequestResult;
    return S_OK;
}

static HRESULT WINAPI result_GetRuntimeClassName( IWebTokenRequestResult *iface, HSTRING *name )
{
    const WCHAR *class_name = RuntimeClass_Windows_Security_Authentication_Web_Core_WebTokenRequestResult;
    if (!name) return E_POINTER;
    return WindowsCreateString( class_name, wcslen( class_name ), name );
}

static HRESULT WINAPI result_GetTrustLevel( IWebTokenRequestResult *iface, TrustLevel *level )
{
    return web_get_trust_level( level );
}

static HRESULT WINAPI result_get_ResponseData( IWebTokenRequestResult *iface, IVectorView_WebTokenResponse **value )
{
    struct web_token_result *impl = result_from_IWebTokenRequestResult( iface );
    if (!value) return E_POINTER;
    *value = impl->responses;
    IVectorView_WebTokenResponse_AddRef( *value );
    return S_OK;
}

static HRESULT WINAPI result_get_ResponseStatus( IWebTokenRequestResult *iface, WebTokenRequestStatus *value )
{
    if (!value) return E_POINTER;
    *value = result_from_IWebTokenRequestResult( iface )->status;
    return S_OK;
}

static HRESULT WINAPI result_get_ResponseError( IWebTokenRequestResult *iface, IWebProviderError **value )
{
    struct web_token_result *impl = result_from_IWebTokenRequestResult( iface );
    if (!value) return E_POINTER;
    *value = impl->error;
    if (*value) IWebProviderError_AddRef( *value );
    return S_OK;
}

static HRESULT WINAPI result_InvalidateCacheAsync( IWebTokenRequestResult *iface, IAsyncAction **operation )
{
    if (operation) *operation = NULL;
    return E_NOTIMPL;
}

static const IWebTokenRequestResultVtbl result_vtbl =
{
    result_QueryInterface,
    result_AddRef,
    result_Release,
    result_GetIids,
    result_GetRuntimeClassName,
    result_GetTrustLevel,
    result_get_ResponseData,
    result_get_ResponseStatus,
    result_get_ResponseError,
    result_InvalidateCacheAsync,
};

static HRESULT response_set_property( IMap_HSTRING_HSTRING *properties, const WCHAR *name, HSTRING value )
{
    HSTRING key;
    boolean replaced;
    HRESULT hr;

    if (!value || WindowsIsStringEmpty(value)) return S_OK;
    if (FAILED(hr = WindowsCreateString( name, wcslen(name), &key ))) return hr;
    hr = IMap_HSTRING_HSTRING_Insert( properties, key, value, &replaced );
    WindowsDeleteString( key );
    return hr;
}

static HRESULT response_set_customer_id( IWebAccount *account, const struct identity_reply *reply )
{
    static const WCHAR personal_tenant[] = L"9188040d-6c67-4c5b-b112-36a304b66dad";
    const WCHAR *id = WindowsGetStringRawBuffer( reply->account_id, NULL );
    WCHAR buffer[16];
    HSTRING value;
    UINT32 i, count = 0;
    HRESULT hr;

    if (wcsicmp( WindowsGetStringRawBuffer(reply->tenant_id, NULL), personal_tenant ) ||
        WindowsGetStringLen( reply->account_id ) != 36 || wcsncmp( id, L"00000000-0000-0000-", 19 ))
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

static HRESULT response_set_identity( IWebAccountProvider *provider, const struct identity_reply *reply,
                                      IWebAccount *account, IWebTokenResponse *response )
{
    static const WCHAR prefix[] = L"https://login.microsoftonline.com/";
    IMap_HSTRING_HSTRING *properties = NULL;
    IWebAccountProvider2 *provider2 = NULL;
    HSTRING authority = NULL, tenant = NULL, expiration = NULL;
    const WCHAR *value;
    WCHAR buffer[192];
    UINT32 length;
    HRESULT hr;

    if (reply->tenant_id && !WindowsIsStringEmpty(reply->tenant_id))
        hr = WindowsDuplicateString( reply->tenant_id, &tenant );
    else
    {
        if (FAILED(hr = IWebAccountProvider_QueryInterface( provider, &IID_IWebAccountProvider2,
                                                            (void **)&provider2 ))) goto done;
        hr = IWebAccountProvider2_get_Authority( provider2, &tenant );
    }
    if (FAILED(hr)) goto done;
    value = WindowsGetStringRawBuffer( tenant, &length );
    if (length >= ARRAY_SIZE(buffer) - ARRAY_SIZE(prefix))
    {
        hr = E_INVALIDARG;
        goto done;
    }
    if (length >= 8 && !wcsncmp( value, L"https://", 8 ))
        hr = WindowsDuplicateString( tenant, &authority );
    else
    {
        wcscpy( buffer, prefix );
        if (length) wcscat( buffer, value );
        else wcscat( buffer, L"common" );
        wcscat( buffer, L"/" );
        hr = WindowsCreateString( buffer, wcslen(buffer), &authority );
    }
    if (FAILED(hr)) goto done;
    if (reply->expires_at)
    {
        if (reply->expires_at > ~(UINT64)0 - 11644473600ULL)
        {
            hr = E_INVALIDARG;
            goto done;
        }
        swprintf( buffer, ARRAY_SIZE(buffer), L"%I64u", reply->expires_at + 11644473600ULL );
        if (FAILED(hr = WindowsCreateString( buffer, wcslen(buffer), &expiration ))) goto done;
    }
    if (FAILED(hr = IWebTokenResponse_get_Properties( response, &properties )) ||
        FAILED(hr = response_set_property( properties, L"Authority", authority )) ||
        FAILED(hr = response_set_property( properties, L"TokenExpiresOn", expiration )) ||
        FAILED(hr = response_set_property( properties, L"wamcompat_id_token", reply->id_token )) ||
        FAILED(hr = response_set_property( properties, L"wamcompat_client_info", reply->client_info )) ||
        FAILED(hr = response_set_property( properties, L"wamcompat_scopes", reply->scopes )) ||
        FAILED(hr = web_account_set_property( account, L"Authority", authority )) ||
        FAILED(hr = web_account_set_property( account, L"OID", reply->account_id )) ||
        FAILED(hr = web_account_set_property( account, L"TenantId", reply->tenant_id )) ||
        FAILED(hr = response_set_customer_id( account, reply ))) goto done;

done:
    if (provider2) IWebAccountProvider2_Release( provider2 );
    if (properties) IMap_HSTRING_HSTRING_Release( properties );
    WindowsDeleteString( authority );
    WindowsDeleteString( tenant );
    WindowsDeleteString( expiration );
    return hr;
}

static HRESULT response_get_token( IWebTokenRequest *request, const struct identity_reply *reply, HSTRING *token )
{
    static const char hex[] = "0123456789ABCDEF";
    static const char *names[] = {"access_token", "id_token", "client_info", "scope"};
    HSTRING values[] = {reply->access_token, reply->id_token, reply->client_info, reply->scopes};
    IMap_HSTRING_HSTRING *properties;
    HSTRING key, value = NULL;
    FILETIME now;
    ULARGE_INTEGER ticks;
    UINT64 seconds, remaining;
    char *buffer, *cursor;
    WCHAR *wide;
    SIZE_T capacity = 128;
    UINT32 i, j;
    int length;
    HRESULT hr;

    if (!request) return WindowsDuplicateString( reply->access_token, token );
    if (FAILED(hr = IWebTokenRequest_get_Properties( request, &properties ))) return hr;
    hr = WindowsCreateString( L"oauth2_batch", 12, &key );
    if (SUCCEEDED(hr))
    {
        hr = IMap_HSTRING_HSTRING_Lookup( properties, key, &value );
        WindowsDeleteString( key );
    }
    IMap_HSTRING_HSTRING_Release( properties );
    if (hr == E_BOUNDS) return WindowsDuplicateString( reply->access_token, token );
    if (FAILED(hr)) return hr;
    i = !wcscmp( WindowsGetStringRawBuffer( value, NULL ), L"1" );
    WindowsDeleteString( value );
    if (!i) return WindowsDuplicateString( reply->access_token, token );

    for (i = 0; i < ARRAY_SIZE(values); ++i)
    {
        length = WideCharToMultiByte( CP_UTF8, 0, WindowsGetStringRawBuffer(values[i], NULL),
                                      WindowsGetStringLen(values[i]), NULL, 0, NULL, NULL );
        capacity += strlen(names[i]) + 2 + (SIZE_T)length * 3;
    }
    if (!(buffer = malloc( capacity ))) return E_OUTOFMEMORY;
    cursor = buffer;
    for (i = 0; i < ARRAY_SIZE(values); ++i)
    {
        const WCHAR *text = WindowsGetStringRawBuffer( values[i], NULL );
        UINT32 count = WindowsGetStringLen( values[i] );
        char *utf8;
        length = WideCharToMultiByte( CP_UTF8, 0, text, count, NULL, 0, NULL, NULL );
        if (!(utf8 = malloc( length ? length : 1 ))) { free( buffer ); return E_OUTOFMEMORY; }
        WideCharToMultiByte( CP_UTF8, 0, text, count, utf8, length, NULL, NULL );
        if (i) *cursor++ = '&';
        memcpy( cursor, names[i], strlen(names[i]) );
        cursor += strlen(names[i]);
        *cursor++ = '=';
        for (j = 0; j < length; ++j)
        {
            unsigned char c = utf8[j];
            if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
                (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.' || c == '~')
                *cursor++ = c;
            else
            {
                *cursor++ = '%';
                *cursor++ = hex[c >> 4];
                *cursor++ = hex[c & 15];
            }
        }
        free( utf8 );
    }
    GetSystemTimeAsFileTime( &now );
    ticks.LowPart = now.dwLowDateTime;
    ticks.HighPart = now.dwHighDateTime;
    seconds = ticks.QuadPart / 10000000 - 11644473600ULL;
    remaining = reply->expires_at > seconds ? reply->expires_at - seconds : 0;
    cursor += sprintf( cursor, "&token_type=Bearer&expires_in=%llu", (unsigned long long)remaining );
    length = MultiByteToWideChar( CP_UTF8, 0, buffer, cursor - buffer, NULL, 0 );
    if (!(wide = malloc( length * sizeof(*wide) ))) { free( buffer ); return E_OUTOFMEMORY; }
    MultiByteToWideChar( CP_UTF8, 0, buffer, cursor - buffer, wide, length );
    hr = WindowsCreateString( wide, length, token );
    free( wide );
    free( buffer );
    return hr;
}

HRESULT web_token_result_create( IWebTokenRequest *request, IWebAccountProvider *provider, const struct identity_reply *reply,
                                 WebTokenRequestStatus status, IWebTokenRequestResult **out )
{
    struct web_token_result *impl;
    IWebAccount *account = NULL;
    IWebTokenResponse *response = NULL;
    HSTRING token = NULL;
    HRESULT hr;

    if (!out) return E_POINTER;
    *out = NULL;
    if (!(impl = calloc( 1, sizeof(*impl) ))) return E_OUTOFMEMORY;
    impl->IWebTokenRequestResult_iface.lpVtbl = &result_vtbl;
    impl->ref = 1;
    impl->status = status;
    if (status != WebTokenRequestStatus_Success &&
        FAILED(hr = web_provider_error_create( reply && FAILED(reply->error) ? reply->error : E_FAIL,
                                                reply ? reply->error_message : NULL, &impl->error )))
        goto failed;
    if (status == WebTokenRequestStatus_Success)
    {
        if (!provider || !reply || !reply->access_token || !reply->account_id)
        {
            hr = E_INVALIDARG;
            goto failed;
        }
        if (FAILED(hr = response_get_token( request, reply, &token )) ||
            FAILED(hr = web_account_create( provider, reply->account_id,
                                            reply->username ? reply->username : reply->account_id,
                                            WebAccountState_Connected, &account )) ||
            FAILED(hr = web_token_response_create( token, account, NULL, &response )) ||
            FAILED(hr = response_set_identity( provider, reply, account, response )))
            goto failed;
    }
    if (FAILED(hr = response_view_create( response, &impl->responses ))) goto failed;
    *out = &impl->IWebTokenRequestResult_iface;
    hr = S_OK;

failed:
    if (account) IWebAccount_Release( account );
    if (response) IWebTokenResponse_Release( response );
    WindowsDeleteString( token );
    if (FAILED(hr)) result_Release( &impl->IWebTokenRequestResult_iface );
    return hr;
}
