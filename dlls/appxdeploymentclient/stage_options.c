/* WinRT Windows.Management.Deployment.StagePackageOptions Implementation
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

struct stage_package_options
{
    IStagePackageOptions IStagePackageOptions_iface;
    LONG ref;
    IPackageVolume *target_volume;
    IUriRuntimeClass *external_location_uri;
    StubPackageOption stub_package_option;
    boolean developer_mode;
    boolean force_update_from_any_version;
    boolean install_all_resources;
    boolean required_content_group_only;
    boolean stage_in_place;
    boolean allow_unsigned;
};

static inline struct stage_package_options *impl_from_IStagePackageOptions( IStagePackageOptions *iface )
{
    return CONTAINING_RECORD( iface, struct stage_package_options, IStagePackageOptions_iface );
}

static HRESULT WINAPI stage_options_QueryInterface( IStagePackageOptions *iface, REFIID iid, void **out )
{
    struct stage_package_options *impl = impl_from_IStagePackageOptions( iface );

    TRACE( "iface %p, iid %s, out %p.\n", iface, debugstr_guid(iid), out );

    if (!out) return E_POINTER;
    if (IsEqualGUID( iid, &IID_IUnknown ) || IsEqualGUID( iid, &IID_IInspectable ) ||
        IsEqualGUID( iid, &IID_IAgileObject ) || IsEqualGUID( iid, &IID_IStagePackageOptions ))
    {
        *out = &impl->IStagePackageOptions_iface;
        IInspectable_AddRef( *out );
        return S_OK;
    }

    *out = NULL;
    return E_NOINTERFACE;
}

static ULONG WINAPI stage_options_AddRef( IStagePackageOptions *iface )
{
    struct stage_package_options *impl = impl_from_IStagePackageOptions( iface );
    return InterlockedIncrement( &impl->ref );
}

static ULONG WINAPI stage_options_Release( IStagePackageOptions *iface )
{
    struct stage_package_options *impl = impl_from_IStagePackageOptions( iface );
    ULONG ref = InterlockedDecrement( &impl->ref );

    if (!ref)
    {
        if (impl->target_volume) IPackageVolume_Release( impl->target_volume );
        if (impl->external_location_uri) IUriRuntimeClass_Release( impl->external_location_uri );
        free( impl );
    }
    return ref;
}

static HRESULT WINAPI stage_options_GetIids( IStagePackageOptions *iface, ULONG *count, IID **iids )
{
    if (!count || !iids) return E_POINTER;
    if (!(*iids = CoTaskMemAlloc( sizeof(**iids) ))) return E_OUTOFMEMORY;
    **iids = IID_IStagePackageOptions;
    *count = 1;
    return S_OK;
}

static HRESULT WINAPI stage_options_GetRuntimeClassName( IStagePackageOptions *iface, HSTRING *name )
{
    const WCHAR *class_name = RuntimeClass_Windows_Management_Deployment_StagePackageOptions;
    return WindowsCreateString( class_name, wcslen(class_name), name );
}

static HRESULT WINAPI stage_options_GetTrustLevel( IStagePackageOptions *iface, TrustLevel *level )
{
    if (!level) return E_POINTER;
    *level = BaseTrust;
    return S_OK;
}

static HRESULT WINAPI stage_options_get_DependencyPackageUris( IStagePackageOptions *iface, IVector_Uri **value )
{
    FIXME( "iface %p, value %p stub!\n", iface, value );
    if (value) *value = NULL;
    return E_NOTIMPL;
}

static HRESULT WINAPI stage_options_get_TargetVolume( IStagePackageOptions *iface, IPackageVolume **value )
{
    struct stage_package_options *impl = impl_from_IStagePackageOptions( iface );
    if (!value) return E_POINTER;
    if ((*value = impl->target_volume)) IPackageVolume_AddRef( *value );
    return S_OK;
}

static HRESULT WINAPI stage_options_put_TargetVolume( IStagePackageOptions *iface, IPackageVolume *value )
{
    struct stage_package_options *impl = impl_from_IStagePackageOptions( iface );
    if (value) IPackageVolume_AddRef( value );
    if (impl->target_volume) IPackageVolume_Release( impl->target_volume );
    impl->target_volume = value;
    return S_OK;
}

static HRESULT WINAPI stage_options_get_OptionalPackageFamilyNames( IStagePackageOptions *iface, IVector_HSTRING **value )
{
    FIXME( "iface %p, value %p stub!\n", iface, value );
    if (value) *value = NULL;
    return E_NOTIMPL;
}

static HRESULT WINAPI stage_options_get_OptionalPackageUris( IStagePackageOptions *iface, IVector_Uri **value )
{
    FIXME( "iface %p, value %p stub!\n", iface, value );
    if (value) *value = NULL;
    return E_NOTIMPL;
}

static HRESULT WINAPI stage_options_get_RelatedPackageUris( IStagePackageOptions *iface, IVector_Uri **value )
{
    FIXME( "iface %p, value %p stub!\n", iface, value );
    if (value) *value = NULL;
    return E_NOTIMPL;
}

static HRESULT WINAPI stage_options_get_ExternalLocationUri( IStagePackageOptions *iface, IUriRuntimeClass **value )
{
    struct stage_package_options *impl = impl_from_IStagePackageOptions( iface );
    if (!value) return E_POINTER;
    if ((*value = impl->external_location_uri)) IUriRuntimeClass_AddRef( *value );
    return S_OK;
}

static HRESULT WINAPI stage_options_put_ExternalLocationUri( IStagePackageOptions *iface, IUriRuntimeClass *value )
{
    struct stage_package_options *impl = impl_from_IStagePackageOptions( iface );
    if (value) IUriRuntimeClass_AddRef( value );
    if (impl->external_location_uri) IUriRuntimeClass_Release( impl->external_location_uri );
    impl->external_location_uri = value;
    return S_OK;
}

#define DEFINE_STAGE_OPTION_PROPERTY(name, type, field) \
    static HRESULT WINAPI stage_options_get_##name( IStagePackageOptions *iface, type *value ) \
    { \
        struct stage_package_options *impl = impl_from_IStagePackageOptions( iface ); \
        if (!value) return E_POINTER; \
        *value = impl->field; \
        return S_OK; \
    } \
    static HRESULT WINAPI stage_options_put_##name( IStagePackageOptions *iface, type value ) \
    { \
        struct stage_package_options *impl = impl_from_IStagePackageOptions( iface ); \
        impl->field = value; \
        return S_OK; \
    }

DEFINE_STAGE_OPTION_PROPERTY( StubPackageOption, StubPackageOption, stub_package_option );
DEFINE_STAGE_OPTION_PROPERTY( DeveloperMode, boolean, developer_mode );
DEFINE_STAGE_OPTION_PROPERTY( ForceUpdateFromAnyVersion, boolean, force_update_from_any_version );
DEFINE_STAGE_OPTION_PROPERTY( InstallAllResources, boolean, install_all_resources );
DEFINE_STAGE_OPTION_PROPERTY( RequiredContentGroupOnly, boolean, required_content_group_only );
DEFINE_STAGE_OPTION_PROPERTY( StageInPlace, boolean, stage_in_place );
DEFINE_STAGE_OPTION_PROPERTY( AllowUnsigned, boolean, allow_unsigned );

static const IStagePackageOptionsVtbl stage_package_options_vtbl =
{
    stage_options_QueryInterface,
    stage_options_AddRef,
    stage_options_Release,
    stage_options_GetIids,
    stage_options_GetRuntimeClassName,
    stage_options_GetTrustLevel,
    stage_options_get_DependencyPackageUris,
    stage_options_get_TargetVolume,
    stage_options_put_TargetVolume,
    stage_options_get_OptionalPackageFamilyNames,
    stage_options_get_OptionalPackageUris,
    stage_options_get_RelatedPackageUris,
    stage_options_get_ExternalLocationUri,
    stage_options_put_ExternalLocationUri,
    stage_options_get_StubPackageOption,
    stage_options_put_StubPackageOption,
    stage_options_get_DeveloperMode,
    stage_options_put_DeveloperMode,
    stage_options_get_ForceUpdateFromAnyVersion,
    stage_options_put_ForceUpdateFromAnyVersion,
    stage_options_get_InstallAllResources,
    stage_options_put_InstallAllResources,
    stage_options_get_RequiredContentGroupOnly,
    stage_options_put_RequiredContentGroupOnly,
    stage_options_get_StageInPlace,
    stage_options_put_StageInPlace,
    stage_options_get_AllowUnsigned,
    stage_options_put_AllowUnsigned,
};

struct stage_package_options_factory
{
    IActivationFactory IActivationFactory_iface;
};

static HRESULT WINAPI stage_options_factory_QueryInterface( IActivationFactory *iface, REFIID iid, void **out )
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

static ULONG WINAPI stage_options_factory_AddRef( IActivationFactory *iface )
{
    return 2;
}

static ULONG WINAPI stage_options_factory_Release( IActivationFactory *iface )
{
    return 1;
}

static HRESULT WINAPI stage_options_factory_GetIids( IActivationFactory *iface, ULONG *count, IID **iids )
{
    return E_NOTIMPL;
}

static HRESULT WINAPI stage_options_factory_GetRuntimeClassName( IActivationFactory *iface, HSTRING *name )
{
    return E_NOTIMPL;
}

static HRESULT WINAPI stage_options_factory_GetTrustLevel( IActivationFactory *iface, TrustLevel *level )
{
    return E_NOTIMPL;
}

static HRESULT WINAPI stage_options_factory_ActivateInstance( IActivationFactory *iface, IInspectable **instance )
{
    struct stage_package_options *impl;

    if (!instance) return E_POINTER;
    *instance = NULL;
    if (!(impl = calloc( 1, sizeof(*impl) ))) return E_OUTOFMEMORY;
    impl->IStagePackageOptions_iface.lpVtbl = &stage_package_options_vtbl;
    impl->ref = 1;
    *instance = (IInspectable *)&impl->IStagePackageOptions_iface;
    return S_OK;
}

static const IActivationFactoryVtbl stage_package_options_factory_vtbl =
{
    stage_options_factory_QueryInterface,
    stage_options_factory_AddRef,
    stage_options_factory_Release,
    stage_options_factory_GetIids,
    stage_options_factory_GetRuntimeClassName,
    stage_options_factory_GetTrustLevel,
    stage_options_factory_ActivateInstance,
};

static struct stage_package_options_factory stage_options_factory =
{
    {&stage_package_options_factory_vtbl},
};

IActivationFactory *stage_package_options_factory = &stage_options_factory.IActivationFactory_iface;
