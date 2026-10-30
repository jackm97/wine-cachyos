/* WinRT Windows.Security.EnterpriseData implementation
 *
 * Copyright (C) 2026 Mirko Brombin
 *
 * This library is free software: you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 */

#define COBJMACROS
#include "initguid.h"

#include "activation.h"
#include "roapi.h"
#include "winstring.h"

#define WIDL_using_Windows_Foundation
#define WIDL_using_Windows_Security_EnterpriseData
#include "windows.security.enterprisedata.h"
#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(enterprisedata);

struct protection_policy_factory
{
    IActivationFactory IActivationFactory_iface;
    IProtectionPolicyManagerStatics2 IProtectionPolicyManagerStatics2_iface;
    LONG ref;
    LONG event_token;
};

static inline struct protection_policy_factory *impl_from_IActivationFactory( IActivationFactory *iface )
{
    return CONTAINING_RECORD( iface, struct protection_policy_factory, IActivationFactory_iface );
}

static HRESULT WINAPI factory_QueryInterface( IActivationFactory *iface, REFIID iid, void **out )
{
    struct protection_policy_factory *impl = impl_from_IActivationFactory( iface );

    if (!out) return E_POINTER;
    *out = NULL;
    if (IsEqualGUID( iid, &IID_IUnknown ) || IsEqualGUID( iid, &IID_IInspectable ) ||
        IsEqualGUID( iid, &IID_IAgileObject ) || IsEqualGUID( iid, &IID_IActivationFactory ))
        *out = &impl->IActivationFactory_iface;
    else if (IsEqualGUID( iid, &IID_IProtectionPolicyManagerStatics2 ))
        *out = &impl->IProtectionPolicyManagerStatics2_iface;
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
    if (!count || !iids) return E_POINTER;
    *count = 0;
    *iids = NULL;
    return S_OK;
}

static HRESULT WINAPI factory_GetRuntimeClassName( IActivationFactory *iface, HSTRING *name )
{
    if (!name) return E_POINTER;
    return WindowsCreateString( RuntimeClass_Windows_Security_EnterpriseData_ProtectionPolicyManager,
            ARRAY_SIZE(RuntimeClass_Windows_Security_EnterpriseData_ProtectionPolicyManager) - 1, name );
}

static HRESULT WINAPI factory_GetTrustLevel( IActivationFactory *iface, TrustLevel *level )
{
    if (!level) return E_POINTER;
    *level = BaseTrust;
    return S_OK;
}

static HRESULT WINAPI factory_ActivateInstance( IActivationFactory *iface, IInspectable **instance )
{
    if (!instance) return E_POINTER;
    *instance = NULL;
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

static inline struct protection_policy_factory *impl_from_IProtectionPolicyManagerStatics2(
        IProtectionPolicyManagerStatics2 *iface )
{
    return CONTAINING_RECORD( iface, struct protection_policy_factory, IProtectionPolicyManagerStatics2_iface );
}

static HRESULT WINAPI statics_QueryInterface( IProtectionPolicyManagerStatics2 *iface, REFIID iid, void **out )
{
    struct protection_policy_factory *impl = impl_from_IProtectionPolicyManagerStatics2( iface );
    return factory_QueryInterface( &impl->IActivationFactory_iface, iid, out );
}

static ULONG WINAPI statics_AddRef( IProtectionPolicyManagerStatics2 *iface )
{
    struct protection_policy_factory *impl = impl_from_IProtectionPolicyManagerStatics2( iface );
    return factory_AddRef( &impl->IActivationFactory_iface );
}

static ULONG WINAPI statics_Release( IProtectionPolicyManagerStatics2 *iface )
{
    struct protection_policy_factory *impl = impl_from_IProtectionPolicyManagerStatics2( iface );
    return factory_Release( &impl->IActivationFactory_iface );
}

static HRESULT WINAPI statics_GetIids( IProtectionPolicyManagerStatics2 *iface, ULONG *count, IID **iids )
{
    struct protection_policy_factory *impl = impl_from_IProtectionPolicyManagerStatics2( iface );
    return factory_GetIids( &impl->IActivationFactory_iface, count, iids );
}

static HRESULT WINAPI statics_GetRuntimeClassName( IProtectionPolicyManagerStatics2 *iface, HSTRING *name )
{
    struct protection_policy_factory *impl = impl_from_IProtectionPolicyManagerStatics2( iface );
    return factory_GetRuntimeClassName( &impl->IActivationFactory_iface, name );
}

static HRESULT WINAPI statics_GetTrustLevel( IProtectionPolicyManagerStatics2 *iface, TrustLevel *level )
{
    struct protection_policy_factory *impl = impl_from_IProtectionPolicyManagerStatics2( iface );
    return factory_GetTrustLevel( &impl->IActivationFactory_iface, level );
}

static HRESULT WINAPI statics_HasContentBeenRevokedSince( IProtectionPolicyManagerStatics2 *iface,
        HSTRING identity, DateTime since, boolean *value )
{
    if (!value) return E_POINTER;
    *value = FALSE;
    return S_OK;
}

static HRESULT WINAPI statics_CheckAccessForApp( IProtectionPolicyManagerStatics2 *iface,
        HSTRING source_identity, HSTRING app_package_family_name, ProtectionPolicyEvaluationResult *result )
{
    if (!result) return E_POINTER;
    *result = ProtectionPolicyEvaluationResult_Allowed;
    return S_OK;
}

static HRESULT WINAPI statics_RequestAccessForAppAsync( IProtectionPolicyManagerStatics2 *iface,
        HSTRING source_identity, HSTRING app_package_family_name,
        __FIAsyncOperation_1_ProtectionPolicyEvaluationResult **operation )
{
    if (!operation) return E_POINTER;
    *operation = NULL;
    return E_NOTIMPL;
}

static HRESULT WINAPI statics_GetEnforcementLevel( IProtectionPolicyManagerStatics2 *iface,
        HSTRING identity, EnforcementLevel *value )
{
    if (!value) return E_POINTER;
    *value = EnforcementLevel_NoProtection;
    return S_OK;
}

static HRESULT WINAPI statics_IsUserDecryptionAllowed( IProtectionPolicyManagerStatics2 *iface,
        HSTRING identity, boolean *value )
{
    if (!value) return E_POINTER;
    *value = TRUE;
    return S_OK;
}

static HRESULT WINAPI statics_IsProtectionUnderLockRequired( IProtectionPolicyManagerStatics2 *iface,
        HSTRING identity, boolean *value )
{
    if (!value) return E_POINTER;
    *value = FALSE;
    return S_OK;
}

static HRESULT WINAPI statics_add_PolicyChanged( IProtectionPolicyManagerStatics2 *iface,
        __FIEventHandler_1_IInspectable *handler, EventRegistrationToken *token )
{
    struct protection_policy_factory *impl = impl_from_IProtectionPolicyManagerStatics2( iface );

    if (!handler || !token) return E_INVALIDARG;
    token->value = InterlockedIncrement( &impl->event_token );
    return S_OK;
}

static HRESULT WINAPI statics_remove_PolicyChanged( IProtectionPolicyManagerStatics2 *iface,
        EventRegistrationToken token )
{
    return S_OK;
}

static HRESULT WINAPI statics_get_IsProtectionEnabled( IProtectionPolicyManagerStatics2 *iface,
        boolean *value )
{
    if (!value) return E_POINTER;
    *value = FALSE;
    return S_OK;
}

static const IProtectionPolicyManagerStatics2Vtbl statics_vtbl =
{
    statics_QueryInterface,
    statics_AddRef,
    statics_Release,
    statics_GetIids,
    statics_GetRuntimeClassName,
    statics_GetTrustLevel,
    statics_HasContentBeenRevokedSince,
    statics_CheckAccessForApp,
    statics_RequestAccessForAppAsync,
    statics_GetEnforcementLevel,
    statics_IsUserDecryptionAllowed,
    statics_IsProtectionUnderLockRequired,
    statics_add_PolicyChanged,
    statics_remove_PolicyChanged,
    statics_get_IsProtectionEnabled,
};

static struct protection_policy_factory protection_policy_factory =
{
    {&factory_vtbl},
    {&statics_vtbl},
    1,
};

HRESULT WINAPI DllGetActivationFactory( HSTRING classid, IActivationFactory **factory )
{
    const WCHAR *name = WindowsGetStringRawBuffer( classid, NULL );

    TRACE( "class %s, factory %p.\n", debugstr_hstring( classid ), factory );
    if (!factory) return E_POINTER;
    *factory = NULL;
    if (wcscmp( name, RuntimeClass_Windows_Security_EnterpriseData_ProtectionPolicyManager ))
        return CLASS_E_CLASSNOTAVAILABLE;
    return IActivationFactory_QueryInterface( &protection_policy_factory.IActivationFactory_iface,
            &IID_IActivationFactory, (void **)factory );
}
