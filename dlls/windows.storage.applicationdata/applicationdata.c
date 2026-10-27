/* WinRT Windows.Storage.ApplicationData ApplicationData Implementation
 *
 * Copyright (C) 2023 Mohamad Al-Jaf
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

WINE_DEFAULT_DEBUG_CHANNEL(data);

struct application_data_statics
{
    IActivationFactory IActivationFactory_iface;
    IApplicationDataStatics IApplicationDataStatics_iface;
    LONG ref;
};

static inline struct application_data_statics *impl_from_IActivationFactory( IActivationFactory *iface )
{
    return CONTAINING_RECORD( iface, struct application_data_statics, IActivationFactory_iface );
}

static HRESULT WINAPI factory_QueryInterface( IActivationFactory *iface, REFIID iid, void **out )
{
    struct application_data_statics *impl = impl_from_IActivationFactory( iface );

    TRACE( "iface %p, iid %s, out %p.\n", iface, debugstr_guid( iid ), out );

    if (IsEqualGUID( iid, &IID_IUnknown ) ||
        IsEqualGUID( iid, &IID_IInspectable ) ||
        IsEqualGUID( iid, &IID_IAgileObject ) ||
        IsEqualGUID( iid, &IID_IActivationFactory ))
    {
        *out = &impl->IActivationFactory_iface;
        IInspectable_AddRef( *out );
        return S_OK;
    }

    if (IsEqualGUID( iid, &IID_IApplicationDataStatics ))
    {
        *out = &impl->IApplicationDataStatics_iface;
        IInspectable_AddRef( *out );
        return S_OK;
    }

    FIXME( "%s not implemented, returning E_NOINTERFACE.\n", debugstr_guid( iid ) );
    *out = NULL;
    return E_NOINTERFACE;
}

static ULONG WINAPI factory_AddRef( IActivationFactory *iface )
{
    struct application_data_statics *impl = impl_from_IActivationFactory( iface );
    ULONG ref = InterlockedIncrement( &impl->ref );
    TRACE( "iface %p increasing refcount to %lu.\n", iface, ref );
    return ref;
}

static ULONG WINAPI factory_Release( IActivationFactory *iface )
{
    struct application_data_statics *impl = impl_from_IActivationFactory( iface );
    ULONG ref = InterlockedDecrement( &impl->ref );
    TRACE( "iface %p decreasing refcount to %lu.\n", iface, ref );
    return ref;
}

static HRESULT WINAPI factory_GetIids( IActivationFactory *iface, ULONG *iid_count, IID **iids )
{
    FIXME( "iface %p, iid_count %p, iids %p stub!\n", iface, iid_count, iids );
    return E_NOTIMPL;
}

static HRESULT WINAPI factory_GetRuntimeClassName( IActivationFactory *iface, HSTRING *class_name )
{
    FIXME( "iface %p, class_name %p stub!\n", iface, class_name );
    return E_NOTIMPL;
}

static HRESULT WINAPI factory_GetTrustLevel( IActivationFactory *iface, TrustLevel *trust_level )
{
    FIXME( "iface %p, trust_level %p stub!\n", iface, trust_level );
    return E_NOTIMPL;
}

static HRESULT WINAPI factory_ActivateInstance( IActivationFactory *iface, IInspectable **instance )
{
    FIXME( "iface %p, instance %p stub!\n", iface, instance );
    return E_NOTIMPL;
}

static const struct IActivationFactoryVtbl factory_vtbl =
{
    factory_QueryInterface,
    factory_AddRef,
    factory_Release,
    /* IInspectable methods */
    factory_GetIids,
    factory_GetRuntimeClassName,
    factory_GetTrustLevel,
    /* IActivationFactory methods */
    factory_ActivateInstance,
};

struct application_data
{
    IApplicationData IApplicationData_iface;
    LONG ref;
    HSTRING package_family_name;
};

static inline struct application_data *impl_from_IApplicationData( IApplicationData *iface )
{
    return CONTAINING_RECORD( iface, struct application_data, IApplicationData_iface );
}

static HRESULT WINAPI application_data_QueryInterface( IApplicationData *iface, REFIID iid, void **out )
{
    struct application_data *impl = impl_from_IApplicationData( iface );

    TRACE( "iface %p, iid %s, out %p.\n", iface, debugstr_guid( iid ), out );

    if (IsEqualGUID( iid, &IID_IUnknown ) ||
        IsEqualGUID( iid, &IID_IInspectable ) ||
        IsEqualGUID( iid, &IID_IAgileObject ) ||
        IsEqualGUID( iid, &IID_IApplicationData ))
    {
        *out = &impl->IApplicationData_iface;
        IInspectable_AddRef( *out );
        return S_OK;
    }

    FIXME( "%s not implemented, returning E_NOINTERFACE.\n", debugstr_guid( iid ) );
    *out = NULL;
    return E_NOINTERFACE;
}

static ULONG WINAPI application_data_AddRef( IApplicationData *iface )
{
    struct application_data *impl = impl_from_IApplicationData( iface );
    ULONG ref = InterlockedIncrement( &impl->ref );
    TRACE( "iface %p increasing refcount to %lu.\n", iface, ref );
    return ref;
}

static ULONG WINAPI application_data_Release( IApplicationData *iface )
{
    struct application_data *impl = impl_from_IApplicationData( iface );
    ULONG ref = InterlockedDecrement( &impl->ref );

    TRACE( "iface %p decreasing refcount to %lu.\n", iface, ref );

    if (!ref)
    {
        WindowsDeleteString( impl->package_family_name );
        free( impl );
    }
    return ref;
}

static HRESULT WINAPI application_data_GetIids( IApplicationData *iface, ULONG *iid_count, IID **iids )
{
    FIXME( "iface %p, iid_count %p, iids %p stub!\n", iface, iid_count, iids );
    return E_NOTIMPL;
}

static HRESULT WINAPI application_data_GetRuntimeClassName( IApplicationData *iface, HSTRING *class_name )
{
    FIXME( "iface %p, class_name %p stub!\n", iface, class_name );
    return E_NOTIMPL;
}

static HRESULT WINAPI application_data_GetTrustLevel( IApplicationData *iface, TrustLevel *trust_level )
{
    FIXME( "iface %p, trust_level %p stub!\n", iface, trust_level );
    return E_NOTIMPL;
}

static HRESULT WINAPI application_data_get_Version( IApplicationData *iface, UINT32 *value )
{
    FIXME( "iface %p, value %p stub!\n", iface, value );
    return E_NOTIMPL;
}

static HRESULT WINAPI application_data_SetVersionAsync( IApplicationData *iface, UINT32 version, IApplicationDataSetVersionHandler *handler,
                                                        IAsyncAction **operation )
{
    FIXME( "iface %p, version %d, handler %p, operation %p stub!\n", iface, version, handler, operation );
    return E_NOTIMPL;
}

static HRESULT WINAPI application_data_ClearAllAsync( IApplicationData *iface, IAsyncAction **operation )
{
    FIXME( "iface %p, operation %p stub!\n", iface, operation );
    return E_NOTIMPL;
}

static HRESULT WINAPI application_data_ClearAsync( IApplicationData *iface, ApplicationDataLocality locality, IAsyncAction **operation )
{
    FIXME( "iface %p, %d locality, operation %p stub!\n", iface, locality, operation );
    return E_NOTIMPL;
}

static HRESULT WINAPI application_data_get_LocalSettings( IApplicationData *iface, IApplicationDataContainer **value )
{
    FIXME( "iface %p, value %p stub!\n", iface, value );
    return E_NOTIMPL;
}

static HRESULT WINAPI application_data_get_RoamingSettings( IApplicationData *iface, IApplicationDataContainer **value )
{
    FIXME( "iface %p, value %p stub!\n", iface, value );
    return E_NOTIMPL;
}

static HRESULT WINAPI application_data_get_LocalFolder( IApplicationData *iface, IStorageFolder **value )
{
    struct application_data *impl = impl_from_IApplicationData( iface );
    WCHAR path[MAX_PATH], *cursor;
    HSTRING path_string;
    DWORD len;
    HRESULT hr;

    TRACE( "iface %p, value %p.\n", iface, value );

    if (!value) return E_POINTER;
    *value = NULL;

    len = GetEnvironmentVariableW( L"LOCALAPPDATA", path, ARRAY_SIZE( path ) );
    if (!len || len >= ARRAY_SIZE( path )) return HRESULT_FROM_WIN32( GetLastError() );

    cursor = path + len;
    if (impl->package_family_name)
    {
        const WCHAR *family = WindowsGetStringRawBuffer( impl->package_family_name, NULL );
        if (swprintf( cursor, ARRAY_SIZE( path ) - len, L"\\Packages\\%s\\LocalState", family ) < 0)
            return HRESULT_FROM_WIN32( ERROR_INSUFFICIENT_BUFFER );
    }
    else if (swprintf( cursor, ARRAY_SIZE( path ) - len, L"\\Packages\\Wine.Application\\LocalState" ) < 0)
        return HRESULT_FROM_WIN32( ERROR_INSUFFICIENT_BUFFER );

    for (cursor = path + 3; *cursor; cursor++)
    {
        if (*cursor != '\\') continue;
        *cursor = 0;
        CreateDirectoryW( path, NULL );
        *cursor = '\\';
    }
    CreateDirectoryW( path, NULL );

    if (FAILED( hr = WindowsCreateString( path, wcslen( path ), &path_string ) )) return hr;
    hr = storage_folder_create( path_string, value );
    WindowsDeleteString( path_string );
    return hr;
}

static HRESULT WINAPI application_data_get_RoamingFolder( IApplicationData *iface, IStorageFolder **value )
{
    FIXME( "iface %p, value %p stub!\n", iface, value );
    return E_NOTIMPL;
}

static HRESULT WINAPI application_data_get_TemporaryFolder( IApplicationData *iface, IStorageFolder **value )
{
    FIXME( "iface %p, value %p stub!\n", iface, value );
    return E_NOTIMPL;
}

static HRESULT WINAPI application_data_add_DataChanged( IApplicationData *iface, ITypedEventHandler_ApplicationData_IInspectable *handler,
                                                        EventRegistrationToken *token )
{
    FIXME( "iface %p, handler %p, token %p stub!\n", iface, handler, token );
    return E_NOTIMPL;
}

static HRESULT WINAPI application_data_remove_DataChanged( IApplicationData *iface, EventRegistrationToken token )
{
    FIXME( "iface %p, token %#I64x stub!\n", iface, token.value );
    return E_NOTIMPL;
}

static HRESULT WINAPI application_data_SignalDataChanged( IApplicationData *iface )
{
    FIXME( "iface %p stub!\n", iface );
    return E_NOTIMPL;
}

static HRESULT WINAPI application_data_get_RoamingStorageQuota( IApplicationData *iface, UINT64 *value )
{
    FIXME( "iface %p, value %p stub!\n", iface, value );
    return E_NOTIMPL;
}

static const struct IApplicationDataVtbl application_data_vtbl =
{
    application_data_QueryInterface,
    application_data_AddRef,
    application_data_Release,
    /* IInspectable methods */
    application_data_GetIids,
    application_data_GetRuntimeClassName,
    application_data_GetTrustLevel,
    /* IApplicationData methods */
    application_data_get_Version,
    application_data_SetVersionAsync,
    application_data_ClearAllAsync,
    application_data_ClearAsync,
    application_data_get_LocalSettings,
    application_data_get_RoamingSettings,
    application_data_get_LocalFolder,
    application_data_get_RoamingFolder,
    application_data_get_TemporaryFolder,
    application_data_add_DataChanged,
    application_data_remove_DataChanged,
    application_data_SignalDataChanged,
    application_data_get_RoamingStorageQuota,
};

DEFINE_IINSPECTABLE( application_data_statics, IApplicationDataStatics, struct application_data_statics, IActivationFactory_iface )

static HRESULT WINAPI application_data_statics_get_Current( IApplicationDataStatics *iface, IApplicationData **value )
{
    TRACE( "iface %p, value %p\n", iface, value );
    return application_data_create( NULL, value );
}

HRESULT application_data_create( HSTRING package_family_name, IApplicationData **value )
{
    struct application_data *impl;
    HRESULT hr;

    TRACE( "value %p\n", value );

    if (!value) return E_INVALIDARG;
    if (!(impl = calloc( 1, sizeof(*impl) ))) return E_OUTOFMEMORY;

    impl->IApplicationData_iface.lpVtbl = &application_data_vtbl;
    impl->ref = 1;
    if (package_family_name && FAILED( hr = WindowsDuplicateString( package_family_name, &impl->package_family_name ) ))
    {
        free( impl );
        return hr;
    }

    *value = &impl->IApplicationData_iface;
    TRACE( "created IApplicationData %p.\n", *value );
    return S_OK;
}

static const struct IApplicationDataStaticsVtbl application_data_statics_vtbl =
{
    application_data_statics_QueryInterface,
    application_data_statics_AddRef,
    application_data_statics_Release,
    /* IInspectable methods */
    application_data_statics_GetIids,
    application_data_statics_GetRuntimeClassName,
    application_data_statics_GetTrustLevel,
    /* IApplicationDataStatics methods */
    application_data_statics_get_Current,
};

static struct application_data_statics application_data_statics =
{
    {&factory_vtbl},
    {&application_data_statics_vtbl},
    1,
};

IActivationFactory *application_data_factory = &application_data_statics.IActivationFactory_iface;

struct application_data_manager_statics
{
    IActivationFactory IActivationFactory_iface;
    IApplicationDataManagerStatics IApplicationDataManagerStatics_iface;
    LONG ref;
};

static inline struct application_data_manager_statics *impl_from_manager_IActivationFactory( IActivationFactory *iface )
{
    return CONTAINING_RECORD( iface, struct application_data_manager_statics, IActivationFactory_iface );
}

static HRESULT WINAPI manager_factory_QueryInterface( IActivationFactory *iface, REFIID iid, void **out )
{
    struct application_data_manager_statics *impl = impl_from_manager_IActivationFactory( iface );

    TRACE( "iface %p, iid %s, out %p.\n", iface, debugstr_guid( iid ), out );

    if (!out) return E_POINTER;
    *out = NULL;

    if (IsEqualGUID( iid, &IID_IUnknown ) || IsEqualGUID( iid, &IID_IInspectable ) ||
        IsEqualGUID( iid, &IID_IAgileObject ) || IsEqualGUID( iid, &IID_IActivationFactory ))
        *out = &impl->IActivationFactory_iface;
    else if (IsEqualGUID( iid, &IID_IApplicationDataManagerStatics ))
        *out = &impl->IApplicationDataManagerStatics_iface;

    if (!*out) return E_NOINTERFACE;
    IInspectable_AddRef( *out );
    return S_OK;
}

static ULONG WINAPI manager_factory_AddRef( IActivationFactory *iface )
{
    struct application_data_manager_statics *impl = impl_from_manager_IActivationFactory( iface );
    return InterlockedIncrement( &impl->ref );
}

static ULONG WINAPI manager_factory_Release( IActivationFactory *iface )
{
    struct application_data_manager_statics *impl = impl_from_manager_IActivationFactory( iface );
    return InterlockedDecrement( &impl->ref );
}

static HRESULT WINAPI manager_factory_GetIids( IActivationFactory *iface, ULONG *iid_count, IID **iids )
{
    FIXME( "iface %p, iid_count %p, iids %p stub!\n", iface, iid_count, iids );
    return E_NOTIMPL;
}

static HRESULT WINAPI manager_factory_GetRuntimeClassName( IActivationFactory *iface, HSTRING *class_name )
{
    FIXME( "iface %p, class_name %p stub!\n", iface, class_name );
    return E_NOTIMPL;
}

static HRESULT WINAPI manager_factory_GetTrustLevel( IActivationFactory *iface, TrustLevel *trust_level )
{
    FIXME( "iface %p, trust_level %p stub!\n", iface, trust_level );
    return E_NOTIMPL;
}

static HRESULT WINAPI manager_factory_ActivateInstance( IActivationFactory *iface, IInspectable **instance )
{
    FIXME( "iface %p, instance %p stub!\n", iface, instance );
    return E_NOTIMPL;
}

static const struct IActivationFactoryVtbl manager_factory_vtbl =
{
    manager_factory_QueryInterface,
    manager_factory_AddRef,
    manager_factory_Release,
    manager_factory_GetIids,
    manager_factory_GetRuntimeClassName,
    manager_factory_GetTrustLevel,
    manager_factory_ActivateInstance,
};

DEFINE_IINSPECTABLE( application_data_manager_statics, IApplicationDataManagerStatics,
        struct application_data_manager_statics, IActivationFactory_iface )

static HRESULT WINAPI application_data_manager_statics_CreateForPackageFamily(
        IApplicationDataManagerStatics *iface, HSTRING package_family_name, IApplicationData **value )
{
    TRACE( "iface %p, package_family_name %s, value %p.\n", iface,
            debugstr_hstring( package_family_name ), value );

    if (!WindowsGetStringLen( package_family_name )) return E_INVALIDARG;
    return application_data_create( package_family_name, value );
}

static const struct IApplicationDataManagerStaticsVtbl application_data_manager_statics_vtbl =
{
    application_data_manager_statics_QueryInterface,
    application_data_manager_statics_AddRef,
    application_data_manager_statics_Release,
    application_data_manager_statics_GetIids,
    application_data_manager_statics_GetRuntimeClassName,
    application_data_manager_statics_GetTrustLevel,
    application_data_manager_statics_CreateForPackageFamily,
};

static struct application_data_manager_statics application_data_manager_statics =
{
    {&manager_factory_vtbl},
    {&application_data_manager_statics_vtbl},
    1,
};

IActivationFactory *application_data_manager_factory = &application_data_manager_statics.IActivationFactory_iface;
