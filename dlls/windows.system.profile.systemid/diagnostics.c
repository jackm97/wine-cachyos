/* WinRT Windows.System.Profile.PlatformDiagnosticsAndUsageDataSettings implementation
 *
 * Copyright (C) 2026 Mirko Brombin
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 */

#include "private.h"

struct platform_diagnostics_statics
{
    IActivationFactory IActivationFactory_iface;
    IPlatformDiagnosticsAndUsageDataSettingsStatics IPlatformDiagnosticsAndUsageDataSettingsStatics_iface;
    LONG ref;
    LONG next_token;
};

static inline struct platform_diagnostics_statics *impl_from_IActivationFactory( IActivationFactory *iface )
{
    return CONTAINING_RECORD( iface, struct platform_diagnostics_statics, IActivationFactory_iface );
}

static HRESULT WINAPI factory_QueryInterface( IActivationFactory *iface, REFIID iid, void **out )
{
    struct platform_diagnostics_statics *impl = impl_from_IActivationFactory( iface );

    if (!out) return E_POINTER;
    *out = NULL;
    if (IsEqualGUID( iid, &IID_IUnknown ) || IsEqualGUID( iid, &IID_IInspectable ) ||
        IsEqualGUID( iid, &IID_IAgileObject ) || IsEqualGUID( iid, &IID_IActivationFactory ))
        *out = &impl->IActivationFactory_iface;
    else if (IsEqualGUID( iid, &IID_IPlatformDiagnosticsAndUsageDataSettingsStatics ))
        *out = &impl->IPlatformDiagnosticsAndUsageDataSettingsStatics_iface;
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

DEFINE_IINSPECTABLE( diagnostics_statics, IPlatformDiagnosticsAndUsageDataSettingsStatics,
        struct platform_diagnostics_statics, IActivationFactory_iface )

static HRESULT WINAPI diagnostics_statics_get_CollectionLevel(
        IPlatformDiagnosticsAndUsageDataSettingsStatics *iface, PlatformDataCollectionLevel *value )
{
    if (!value) return E_POINTER;
    *value = PlatformDataCollectionLevel_Security;
    return S_OK;
}

static HRESULT WINAPI diagnostics_statics_add_CollectionLevelChanged(
        IPlatformDiagnosticsAndUsageDataSettingsStatics *iface, __FIEventHandler_1_IInspectable *handler,
        EventRegistrationToken *token )
{
    struct platform_diagnostics_statics *impl = CONTAINING_RECORD( iface,
            struct platform_diagnostics_statics, IPlatformDiagnosticsAndUsageDataSettingsStatics_iface );

    if (!handler || !token) return E_POINTER;
    token->value = InterlockedIncrement( &impl->next_token );
    return S_OK;
}

static HRESULT WINAPI diagnostics_statics_remove_CollectionLevelChanged(
        IPlatformDiagnosticsAndUsageDataSettingsStatics *iface, EventRegistrationToken token )
{
    return S_OK;
}

static HRESULT WINAPI diagnostics_statics_CanCollectDiagnostics(
        IPlatformDiagnosticsAndUsageDataSettingsStatics *iface, PlatformDataCollectionLevel level, boolean *value )
{
    if (!value) return E_POINTER;
    *value = level == PlatformDataCollectionLevel_Security;
    return S_OK;
}

static const IPlatformDiagnosticsAndUsageDataSettingsStaticsVtbl diagnostics_statics_vtbl =
{
    diagnostics_statics_QueryInterface,
    diagnostics_statics_AddRef,
    diagnostics_statics_Release,
    diagnostics_statics_GetIids,
    diagnostics_statics_GetRuntimeClassName,
    diagnostics_statics_GetTrustLevel,
    diagnostics_statics_get_CollectionLevel,
    diagnostics_statics_add_CollectionLevelChanged,
    diagnostics_statics_remove_CollectionLevelChanged,
    diagnostics_statics_CanCollectDiagnostics,
};

static struct platform_diagnostics_statics platform_diagnostics_statics =
{
    {&factory_vtbl},
    {&diagnostics_statics_vtbl},
    1,
};

IActivationFactory *platform_diagnostics_factory = &platform_diagnostics_statics.IActivationFactory_iface;
