/* WinRT Windows.Management.Deployment.AddPackageOptions Implementation
 *
 * Copyright (C) 2026 Mirko Brombin
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

#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(appx);

struct add_package_options
{
    IAddPackageOptions IAddPackageOptions_iface;
    LONG ref;
    IPackageVolume *target_volume;
    IUriRuntimeClass *external_location_uri;
    StubPackageOption stub_package_option;
    boolean developer_mode;
    boolean force_app_shutdown;
    boolean force_target_app_shutdown;
    boolean force_update_from_any_version;
    boolean install_all_resources;
    boolean required_content_group_only;
    boolean retain_files_on_failure;
    boolean stage_in_place;
    boolean allow_unsigned;
    boolean defer_registration_when_packages_are_in_use;
};

static inline struct add_package_options *impl_from_IAddPackageOptions( IAddPackageOptions *iface )
{
    return CONTAINING_RECORD( iface, struct add_package_options, IAddPackageOptions_iface );
}

static HRESULT WINAPI add_options_QueryInterface( IAddPackageOptions *iface, REFIID iid, void **out )
{
    struct add_package_options *impl = impl_from_IAddPackageOptions( iface );

    TRACE( "iface %p, iid %s, out %p.\n", iface, debugstr_guid(iid), out );

    if (!out) return E_POINTER;
    if (IsEqualGUID( iid, &IID_IUnknown ) || IsEqualGUID( iid, &IID_IInspectable ) ||
        IsEqualGUID( iid, &IID_IAgileObject ) || IsEqualGUID( iid, &IID_IAddPackageOptions ))
    {
        *out = &impl->IAddPackageOptions_iface;
        IInspectable_AddRef( *out );
        return S_OK;
    }

    *out = NULL;
    return E_NOINTERFACE;
}

static ULONG WINAPI add_options_AddRef( IAddPackageOptions *iface )
{
    return InterlockedIncrement( &impl_from_IAddPackageOptions( iface )->ref );
}

static ULONG WINAPI add_options_Release( IAddPackageOptions *iface )
{
    struct add_package_options *impl = impl_from_IAddPackageOptions( iface );
    ULONG ref = InterlockedDecrement( &impl->ref );

    if (!ref)
    {
        if (impl->target_volume) IPackageVolume_Release( impl->target_volume );
        if (impl->external_location_uri) IUriRuntimeClass_Release( impl->external_location_uri );
        free( impl );
    }
    return ref;
}

static HRESULT WINAPI add_options_GetIids( IAddPackageOptions *iface, ULONG *count, IID **iids )
{
    if (!count || !iids) return E_POINTER;
    if (!(*iids = CoTaskMemAlloc( sizeof(**iids) ))) return E_OUTOFMEMORY;
    **iids = IID_IAddPackageOptions;
    *count = 1;
    return S_OK;
}

static HRESULT WINAPI add_options_GetRuntimeClassName( IAddPackageOptions *iface, HSTRING *name )
{
    const WCHAR *class_name = RuntimeClass_Windows_Management_Deployment_AddPackageOptions;
    return WindowsCreateString( class_name, wcslen(class_name), name );
}

static HRESULT WINAPI add_options_GetTrustLevel( IAddPackageOptions *iface, TrustLevel *level )
{
    if (!level) return E_POINTER;
    *level = BaseTrust;
    return S_OK;
}

static HRESULT WINAPI add_options_get_DependencyPackageUris( IAddPackageOptions *iface, IVector_Uri **value )
{
    FIXME( "iface %p, value %p stub!\n", iface, value );
    if (value) *value = NULL;
    return E_NOTIMPL;
}

static HRESULT WINAPI add_options_get_TargetVolume( IAddPackageOptions *iface, IPackageVolume **value )
{
    struct add_package_options *impl = impl_from_IAddPackageOptions( iface );
    if (!value) return E_POINTER;
    if ((*value = impl->target_volume)) IPackageVolume_AddRef( *value );
    return S_OK;
}

static HRESULT WINAPI add_options_put_TargetVolume( IAddPackageOptions *iface, IPackageVolume *value )
{
    struct add_package_options *impl = impl_from_IAddPackageOptions( iface );
    if (value) IPackageVolume_AddRef( value );
    if (impl->target_volume) IPackageVolume_Release( impl->target_volume );
    impl->target_volume = value;
    return S_OK;
}

static HRESULT WINAPI add_options_get_OptionalPackageFamilyNames( IAddPackageOptions *iface, IVector_HSTRING **value )
{
    FIXME( "iface %p, value %p stub!\n", iface, value );
    if (value) *value = NULL;
    return E_NOTIMPL;
}

static HRESULT WINAPI add_options_get_OptionalPackageUris( IAddPackageOptions *iface, IVector_Uri **value )
{
    FIXME( "iface %p, value %p stub!\n", iface, value );
    if (value) *value = NULL;
    return E_NOTIMPL;
}

static HRESULT WINAPI add_options_get_RelatedPackageUris( IAddPackageOptions *iface, IVector_Uri **value )
{
    FIXME( "iface %p, value %p stub!\n", iface, value );
    if (value) *value = NULL;
    return E_NOTIMPL;
}

static HRESULT WINAPI add_options_get_ExternalLocationUri( IAddPackageOptions *iface, IUriRuntimeClass **value )
{
    struct add_package_options *impl = impl_from_IAddPackageOptions( iface );
    if (!value) return E_POINTER;
    if ((*value = impl->external_location_uri)) IUriRuntimeClass_AddRef( *value );
    return S_OK;
}

static HRESULT WINAPI add_options_put_ExternalLocationUri( IAddPackageOptions *iface, IUriRuntimeClass *value )
{
    struct add_package_options *impl = impl_from_IAddPackageOptions( iface );
    HSTRING uri = NULL;

    if (value && SUCCEEDED(IUriRuntimeClass_get_AbsoluteUri( value, &uri )))
    {
        TRACE( "iface %p, value %p, uri %s.\n", iface, value, debugstr_hstring(uri) );
        WindowsDeleteString( uri );
    }
    if (value) IUriRuntimeClass_AddRef( value );
    if (impl->external_location_uri) IUriRuntimeClass_Release( impl->external_location_uri );
    impl->external_location_uri = value;
    return S_OK;
}

#define DEFINE_ADD_OPTION_PROPERTY(name, type, field) \
    static HRESULT WINAPI add_options_get_##name( IAddPackageOptions *iface, type *value ) \
    { \
        struct add_package_options *impl = impl_from_IAddPackageOptions( iface ); \
        if (!value) return E_POINTER; \
        *value = impl->field; \
        return S_OK; \
    } \
    static HRESULT WINAPI add_options_put_##name( IAddPackageOptions *iface, type value ) \
    { \
        struct add_package_options *impl = impl_from_IAddPackageOptions( iface ); \
        impl->field = value; \
        return S_OK; \
    }

DEFINE_ADD_OPTION_PROPERTY( StubPackageOption, StubPackageOption, stub_package_option );
DEFINE_ADD_OPTION_PROPERTY( DeveloperMode, boolean, developer_mode );
DEFINE_ADD_OPTION_PROPERTY( ForceAppShutdown, boolean, force_app_shutdown );
DEFINE_ADD_OPTION_PROPERTY( ForceTargetAppShutdown, boolean, force_target_app_shutdown );
DEFINE_ADD_OPTION_PROPERTY( ForceUpdateFromAnyVersion, boolean, force_update_from_any_version );
DEFINE_ADD_OPTION_PROPERTY( InstallAllResources, boolean, install_all_resources );
DEFINE_ADD_OPTION_PROPERTY( RequiredContentGroupOnly, boolean, required_content_group_only );
DEFINE_ADD_OPTION_PROPERTY( RetainFilesOnFailure, boolean, retain_files_on_failure );
DEFINE_ADD_OPTION_PROPERTY( StageInPlace, boolean, stage_in_place );
DEFINE_ADD_OPTION_PROPERTY( AllowUnsigned, boolean, allow_unsigned );
DEFINE_ADD_OPTION_PROPERTY( DeferRegistrationWhenPackagesAreInUse, boolean, defer_registration_when_packages_are_in_use );

static const IAddPackageOptionsVtbl add_package_options_vtbl =
{
    add_options_QueryInterface,
    add_options_AddRef,
    add_options_Release,
    add_options_GetIids,
    add_options_GetRuntimeClassName,
    add_options_GetTrustLevel,
    add_options_get_DependencyPackageUris,
    add_options_get_TargetVolume,
    add_options_put_TargetVolume,
    add_options_get_OptionalPackageFamilyNames,
    add_options_get_OptionalPackageUris,
    add_options_get_RelatedPackageUris,
    add_options_get_ExternalLocationUri,
    add_options_put_ExternalLocationUri,
    add_options_get_StubPackageOption,
    add_options_put_StubPackageOption,
    add_options_get_DeveloperMode,
    add_options_put_DeveloperMode,
    add_options_get_ForceAppShutdown,
    add_options_put_ForceAppShutdown,
    add_options_get_ForceTargetAppShutdown,
    add_options_put_ForceTargetAppShutdown,
    add_options_get_ForceUpdateFromAnyVersion,
    add_options_put_ForceUpdateFromAnyVersion,
    add_options_get_InstallAllResources,
    add_options_put_InstallAllResources,
    add_options_get_RequiredContentGroupOnly,
    add_options_put_RequiredContentGroupOnly,
    add_options_get_RetainFilesOnFailure,
    add_options_put_RetainFilesOnFailure,
    add_options_get_StageInPlace,
    add_options_put_StageInPlace,
    add_options_get_AllowUnsigned,
    add_options_put_AllowUnsigned,
    add_options_get_DeferRegistrationWhenPackagesAreInUse,
    add_options_put_DeferRegistrationWhenPackagesAreInUse,
};

static HRESULT WINAPI add_options_factory_QueryInterface( IActivationFactory *iface, REFIID iid, void **out )
{
    TRACE( "iface %p, iid %s, out %p.\n", iface, debugstr_guid(iid), out );

    if (!out) return E_POINTER;
    if (IsEqualGUID( iid, &IID_IUnknown ) || IsEqualGUID( iid, &IID_IInspectable ) ||
        IsEqualGUID( iid, &IID_IAgileObject ) || IsEqualGUID( iid, &IID_IActivationFactory ))
    {
        *out = iface;
        IActivationFactory_AddRef( iface );
        return S_OK;
    }
    *out = NULL;
    return E_NOINTERFACE;
}

static ULONG WINAPI add_options_factory_AddRef( IActivationFactory *iface )
{
    return 2;
}

static ULONG WINAPI add_options_factory_Release( IActivationFactory *iface )
{
    return 1;
}

static HRESULT WINAPI add_options_factory_GetIids( IActivationFactory *iface, ULONG *count, IID **iids )
{
    return E_NOTIMPL;
}

static HRESULT WINAPI add_options_factory_GetRuntimeClassName( IActivationFactory *iface, HSTRING *name )
{
    return E_NOTIMPL;
}

static HRESULT WINAPI add_options_factory_GetTrustLevel( IActivationFactory *iface, TrustLevel *level )
{
    return E_NOTIMPL;
}

static HRESULT WINAPI add_options_factory_ActivateInstance( IActivationFactory *iface, IInspectable **instance )
{
    struct add_package_options *impl;

    if (!instance) return E_POINTER;
    *instance = NULL;
    if (!(impl = calloc( 1, sizeof(*impl) ))) return E_OUTOFMEMORY;
    impl->IAddPackageOptions_iface.lpVtbl = &add_package_options_vtbl;
    impl->ref = 1;
    *instance = (IInspectable *)&impl->IAddPackageOptions_iface;
    return S_OK;
}

static const IActivationFactoryVtbl add_package_options_factory_vtbl =
{
    add_options_factory_QueryInterface,
    add_options_factory_AddRef,
    add_options_factory_Release,
    add_options_factory_GetIids,
    add_options_factory_GetRuntimeClassName,
    add_options_factory_GetTrustLevel,
    add_options_factory_ActivateInstance,
};

static IActivationFactory add_options_factory = {&add_package_options_factory_vtbl};

IActivationFactory *add_package_options_factory = &add_options_factory;
