/* WinRT Windows.ApplicationModel.LimitedAccessFeatures implementation
 *
 * Copyright (C) 2026 Mirko Brombin
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 */

#include "private.h"
#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(model);

struct limited_access_result
{
    ILimitedAccessFeatureRequestResult ILimitedAccessFeatureRequestResult_iface;
    LONG ref;
    HSTRING feature_id;
};

static inline struct limited_access_result *impl_from_ILimitedAccessFeatureRequestResult(
        ILimitedAccessFeatureRequestResult *iface )
{
    return CONTAINING_RECORD( iface, struct limited_access_result, ILimitedAccessFeatureRequestResult_iface );
}

static HRESULT WINAPI result_QueryInterface( ILimitedAccessFeatureRequestResult *iface, REFIID iid, void **out )
{
    if (!out) return E_POINTER;
    *out = NULL;
    if (IsEqualGUID( iid, &IID_IUnknown ) || IsEqualGUID( iid, &IID_IInspectable ) ||
        IsEqualGUID( iid, &IID_IAgileObject ) || IsEqualGUID( iid, &IID_ILimitedAccessFeatureRequestResult ))
        *out = iface;
    if (!*out) return E_NOINTERFACE;
    ILimitedAccessFeatureRequestResult_AddRef( iface );
    return S_OK;
}

static ULONG WINAPI result_AddRef( ILimitedAccessFeatureRequestResult *iface )
{
    return InterlockedIncrement( &impl_from_ILimitedAccessFeatureRequestResult( iface )->ref );
}

static ULONG WINAPI result_Release( ILimitedAccessFeatureRequestResult *iface )
{
    struct limited_access_result *impl = impl_from_ILimitedAccessFeatureRequestResult( iface );
    ULONG ref = InterlockedDecrement( &impl->ref );

    if (!ref)
    {
        WindowsDeleteString( impl->feature_id );
        free( impl );
    }
    return ref;
}

static HRESULT WINAPI result_GetIids( ILimitedAccessFeatureRequestResult *iface, ULONG *count, IID **iids )
{
    IID *values;

    if (!count || !iids) return E_POINTER;
    *count = 0;
    *iids = NULL;
    if (!(values = CoTaskMemAlloc( sizeof(*values) ))) return E_OUTOFMEMORY;
    values[0] = IID_ILimitedAccessFeatureRequestResult;
    *count = 1;
    *iids = values;
    return S_OK;
}

static HRESULT WINAPI result_GetRuntimeClassName( ILimitedAccessFeatureRequestResult *iface, HSTRING *name )
{
    if (!name) return E_POINTER;
    return WindowsCreateString( RuntimeClass_Windows_ApplicationModel_LimitedAccessFeatureRequestResult,
            ARRAY_SIZE(RuntimeClass_Windows_ApplicationModel_LimitedAccessFeatureRequestResult) - 1, name );
}

static HRESULT WINAPI result_GetTrustLevel( ILimitedAccessFeatureRequestResult *iface, TrustLevel *level )
{
    if (!level) return E_POINTER;
    *level = BaseTrust;
    return S_OK;
}

static HRESULT WINAPI result_get_FeatureId( ILimitedAccessFeatureRequestResult *iface, HSTRING *value )
{
    if (!value) return E_POINTER;
    return WindowsDuplicateString( impl_from_ILimitedAccessFeatureRequestResult( iface )->feature_id, value );
}

static HRESULT WINAPI result_get_Status( ILimitedAccessFeatureRequestResult *iface,
        LimitedAccessFeatureStatus *value )
{
    if (!value) return E_POINTER;
    *value = LimitedAccessFeatureStatus_Unavailable;
    return S_OK;
}

static HRESULT WINAPI result_get_EstimatedRemovalDate( ILimitedAccessFeatureRequestResult *iface,
        __FIReference_1_DateTime **value )
{
    if (!value) return E_POINTER;
    *value = NULL;
    return S_OK;
}

static const ILimitedAccessFeatureRequestResultVtbl result_vtbl =
{
    result_QueryInterface,
    result_AddRef,
    result_Release,
    result_GetIids,
    result_GetRuntimeClassName,
    result_GetTrustLevel,
    result_get_FeatureId,
    result_get_Status,
    result_get_EstimatedRemovalDate,
};

struct limited_access_features_statics
{
    IActivationFactory IActivationFactory_iface;
    ILimitedAccessFeaturesStatics ILimitedAccessFeaturesStatics_iface;
    LONG ref;
};

static inline struct limited_access_features_statics *impl_from_IActivationFactory( IActivationFactory *iface )
{
    return CONTAINING_RECORD( iface, struct limited_access_features_statics, IActivationFactory_iface );
}

static HRESULT WINAPI factory_QueryInterface( IActivationFactory *iface, REFIID iid, void **out )
{
    struct limited_access_features_statics *impl = impl_from_IActivationFactory( iface );

    if (!out) return E_POINTER;
    *out = NULL;
    if (IsEqualGUID( iid, &IID_IUnknown ) || IsEqualGUID( iid, &IID_IInspectable ) ||
        IsEqualGUID( iid, &IID_IAgileObject ) || IsEqualGUID( iid, &IID_IActivationFactory ))
        *out = &impl->IActivationFactory_iface;
    else if (IsEqualGUID( iid, &IID_ILimitedAccessFeaturesStatics ))
        *out = &impl->ILimitedAccessFeaturesStatics_iface;
    if (!*out) return E_NOINTERFACE;
    IInspectable_AddRef( *out );
    return S_OK;
}

static ULONG WINAPI factory_AddRef( IActivationFactory *iface )
{
    return InterlockedIncrement( &impl_from_IActivationFactory( iface )->ref );
}

static ULONG WINAPI factory_Release( IActivationFactory *iface )
{
    return InterlockedDecrement( &impl_from_IActivationFactory( iface )->ref );
}

static HRESULT WINAPI factory_GetIids( IActivationFactory *iface, ULONG *count, IID **iids )
{
    return E_NOTIMPL;
}

static HRESULT WINAPI factory_GetRuntimeClassName( IActivationFactory *iface, HSTRING *name )
{
    return E_NOTIMPL;
}

static HRESULT WINAPI factory_GetTrustLevel( IActivationFactory *iface, TrustLevel *level )
{
    return E_NOTIMPL;
}

static HRESULT WINAPI factory_ActivateInstance( IActivationFactory *iface, IInspectable **instance )
{
    return E_NOTIMPL;
}

static const IActivationFactoryVtbl factory_vtbl =
{
    factory_QueryInterface,
    factory_AddRef,
    factory_Release,
    factory_GetIids,
    factory_GetRuntimeClassName,
    factory_GetTrustLevel,
    factory_ActivateInstance,
};

DEFINE_IINSPECTABLE( limited_access_features_statics, ILimitedAccessFeaturesStatics,
        struct limited_access_features_statics, IActivationFactory_iface )

static HRESULT WINAPI limited_access_features_statics_TryUnlockFeature( ILimitedAccessFeaturesStatics *iface,
        HSTRING feature_id, HSTRING token, HSTRING attestation, ILimitedAccessFeatureRequestResult **value )
{
    struct limited_access_result *result;
    HRESULT hr;

    TRACE( "iface %p, feature_id %s, token <redacted>, attestation %s, value %p.\n", iface,
            debugstr_hstring( feature_id ), debugstr_hstring( attestation ), value );

    if (!value) return E_POINTER;
    *value = NULL;
    if (!WindowsGetStringLen( feature_id ) || !WindowsGetStringLen( token ) ||
        !WindowsGetStringLen( attestation )) return E_INVALIDARG;
    if (!(result = calloc( 1, sizeof(*result) ))) return E_OUTOFMEMORY;
    result->ILimitedAccessFeatureRequestResult_iface.lpVtbl = &result_vtbl;
    result->ref = 1;
    if (FAILED( hr = WindowsDuplicateString( feature_id, &result->feature_id ) ))
    {
        free( result );
        return hr;
    }
    *value = &result->ILimitedAccessFeatureRequestResult_iface;
    return S_OK;
}

static const ILimitedAccessFeaturesStaticsVtbl limited_access_features_statics_vtbl =
{
    limited_access_features_statics_QueryInterface,
    limited_access_features_statics_AddRef,
    limited_access_features_statics_Release,
    limited_access_features_statics_GetIids,
    limited_access_features_statics_GetRuntimeClassName,
    limited_access_features_statics_GetTrustLevel,
    limited_access_features_statics_TryUnlockFeature,
};

static struct limited_access_features_statics limited_access_features_statics =
{
    {&factory_vtbl},
    {&limited_access_features_statics_vtbl},
    1,
};

IActivationFactory *limited_access_features_factory =
        &limited_access_features_statics.IActivationFactory_iface;
