/* WinRT Windows.Management.Deployment.PackageManager Implementation
 *
 * Copyright 2026 Jack Myers
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

#include <stdlib.h>

#include "private.h"

#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(deployment);

/* Spelled-out names of the widl generated parameterized types. */
#define PACKAGES_T   __FIIterable_1_Windows__CApplicationModel__CPackage
#define PACKAGE_T    __x_ABI_CWindows_CApplicationModel_CIPackage
#define PKG_ITER_T   __FIIterator_1_Windows__CApplicationModel__CPackage
#define URI_T        __x_ABI_CWindows_CFoundation_CIUriRuntimeClass
#define URIS_T       __FIIterable_1_Windows__CFoundation__CUri
#define HSTRINGS_T   __FIIterable_1_HSTRING
#define USERS_T      __FIIterable_1_Windows__CManagement__CDeployment__CPackageUserInformation
#define OPERATION_T  __FIAsyncOperationWithProgress_2_Windows__CManagement__CDeployment__CDeploymentResult_DeploymentProgress
#define OPTIONS_T    __x_ABI_CWindows_CManagement_CDeployment_CDeploymentOptions
#define REMOVAL_T    __x_ABI_CWindows_CManagement_CDeployment_CRemovalOptions
#define STATE_T      __x_ABI_CWindows_CManagement_CDeployment_CPackageState
#define TYPES_T      __x_ABI_CWindows_CManagement_CDeployment_CPackageTypes

/*
 * Empty IIterator<Package *>
 */

struct empty_iterator
{
    PKG_ITER_T iface;
    LONG ref;
};

static inline struct empty_iterator *impl_from_iterator( PKG_ITER_T *iface )
{
    return CONTAINING_RECORD( iface, struct empty_iterator, iface );
}

static HRESULT WINAPI iterator_QueryInterface( PKG_ITER_T *iface, REFIID iid, void **out )
{
    struct empty_iterator *impl = impl_from_iterator( iface );

    TRACE( "iface %p, iid %s, out %p.\n", iface, debugstr_guid( iid ), out );

    if (IsEqualGUID( iid, &IID_IUnknown ) ||
        IsEqualGUID( iid, &IID_IInspectable ) ||
        IsEqualGUID( iid, &IID_IAgileObject ) ||
        IsEqualGUID( iid, &IID___FIIterator_1_Windows__CApplicationModel__CPackage ))
    {
        *out = &impl->iface;
        IInspectable_AddRef( *out );
        return S_OK;
    }

    FIXME( "%s not implemented, returning E_NOINTERFACE.\n", debugstr_guid( iid ) );
    *out = NULL;
    return E_NOINTERFACE;
}

static ULONG WINAPI iterator_AddRef( PKG_ITER_T *iface )
{
    struct empty_iterator *impl = impl_from_iterator( iface );
    ULONG ref = InterlockedIncrement( &impl->ref );
    TRACE( "iface %p increasing refcount to %lu.\n", iface, ref );
    return ref;
}

static ULONG WINAPI iterator_Release( PKG_ITER_T *iface )
{
    struct empty_iterator *impl = impl_from_iterator( iface );
    ULONG ref = InterlockedDecrement( &impl->ref );
    TRACE( "iface %p decreasing refcount to %lu.\n", iface, ref );
    if (!ref) free( impl );
    return ref;
}

static HRESULT WINAPI iterator_GetIids( PKG_ITER_T *iface, ULONG *iid_count, IID **iids )
{
    FIXME( "iface %p, iid_count %p, iids %p stub!\n", iface, iid_count, iids );
    return E_NOTIMPL;
}

static HRESULT WINAPI iterator_GetRuntimeClassName( PKG_ITER_T *iface, HSTRING *class_name )
{
    FIXME( "iface %p, class_name %p stub!\n", iface, class_name );
    return E_NOTIMPL;
}

static HRESULT WINAPI iterator_GetTrustLevel( PKG_ITER_T *iface, TrustLevel *trust_level )
{
    FIXME( "iface %p, trust_level %p stub!\n", iface, trust_level );
    return E_NOTIMPL;
}

static HRESULT WINAPI iterator_get_Current( PKG_ITER_T *iface, PACKAGE_T **value )
{
    TRACE( "iface %p, value %p.\n", iface, value );
    *value = NULL;
    return E_BOUNDS;
}

static HRESULT WINAPI iterator_get_HasCurrent( PKG_ITER_T *iface, boolean *value )
{
    TRACE( "iface %p, value %p.\n", iface, value );
    *value = FALSE;
    return S_OK;
}

static HRESULT WINAPI iterator_MoveNext( PKG_ITER_T *iface, boolean *value )
{
    TRACE( "iface %p, value %p.\n", iface, value );
    *value = FALSE;
    return S_OK;
}

static HRESULT WINAPI iterator_GetMany( PKG_ITER_T *iface, UINT32 items_size, PACKAGE_T **items, UINT32 *value )
{
    TRACE( "iface %p, items_size %u, items %p, value %p.\n", iface, items_size, items, value );
    *value = 0;
    return S_OK;
}

static const __FIIterator_1_Windows__CApplicationModel__CPackageVtbl iterator_vtbl =
{
    iterator_QueryInterface,
    iterator_AddRef,
    iterator_Release,
    /* IInspectable methods */
    iterator_GetIids,
    iterator_GetRuntimeClassName,
    iterator_GetTrustLevel,
    /* IIterator<Package *> methods */
    iterator_get_Current,
    iterator_get_HasCurrent,
    iterator_MoveNext,
    iterator_GetMany,
};

/*
 * Empty IIterable<Package *>
 */

struct empty_iterable
{
    PACKAGES_T iface;
    LONG ref;
};

static inline struct empty_iterable *impl_from_iterable( PACKAGES_T *iface )
{
    return CONTAINING_RECORD( iface, struct empty_iterable, iface );
}

static HRESULT WINAPI iterable_QueryInterface( PACKAGES_T *iface, REFIID iid, void **out )
{
    struct empty_iterable *impl = impl_from_iterable( iface );

    TRACE( "iface %p, iid %s, out %p.\n", iface, debugstr_guid( iid ), out );

    if (IsEqualGUID( iid, &IID_IUnknown ) ||
        IsEqualGUID( iid, &IID_IInspectable ) ||
        IsEqualGUID( iid, &IID_IAgileObject ) ||
        IsEqualGUID( iid, &IID___FIIterable_1_Windows__CApplicationModel__CPackage ))
    {
        *out = &impl->iface;
        IInspectable_AddRef( *out );
        return S_OK;
    }

    FIXME( "%s not implemented, returning E_NOINTERFACE.\n", debugstr_guid( iid ) );
    *out = NULL;
    return E_NOINTERFACE;
}

static ULONG WINAPI iterable_AddRef( PACKAGES_T *iface )
{
    struct empty_iterable *impl = impl_from_iterable( iface );
    ULONG ref = InterlockedIncrement( &impl->ref );
    TRACE( "iface %p increasing refcount to %lu.\n", iface, ref );
    return ref;
}

static ULONG WINAPI iterable_Release( PACKAGES_T *iface )
{
    struct empty_iterable *impl = impl_from_iterable( iface );
    ULONG ref = InterlockedDecrement( &impl->ref );
    TRACE( "iface %p decreasing refcount to %lu.\n", iface, ref );
    if (!ref) free( impl );
    return ref;
}

static HRESULT WINAPI iterable_GetIids( PACKAGES_T *iface, ULONG *iid_count, IID **iids )
{
    FIXME( "iface %p, iid_count %p, iids %p stub!\n", iface, iid_count, iids );
    return E_NOTIMPL;
}

static HRESULT WINAPI iterable_GetRuntimeClassName( PACKAGES_T *iface, HSTRING *class_name )
{
    FIXME( "iface %p, class_name %p stub!\n", iface, class_name );
    return E_NOTIMPL;
}

static HRESULT WINAPI iterable_GetTrustLevel( PACKAGES_T *iface, TrustLevel *trust_level )
{
    FIXME( "iface %p, trust_level %p stub!\n", iface, trust_level );
    return E_NOTIMPL;
}

static HRESULT WINAPI iterable_First( PACKAGES_T *iface, PKG_ITER_T **value )
{
    struct empty_iterator *iterator;

    TRACE( "iface %p, value %p.\n", iface, value );

    if (!(iterator = calloc( 1, sizeof(*iterator) ))) return E_OUTOFMEMORY;
    iterator->iface.lpVtbl = &iterator_vtbl;
    iterator->ref = 1;

    *value = &iterator->iface;
    return S_OK;
}

static const __FIIterable_1_Windows__CApplicationModel__CPackageVtbl iterable_vtbl =
{
    iterable_QueryInterface,
    iterable_AddRef,
    iterable_Release,
    /* IInspectable methods */
    iterable_GetIids,
    iterable_GetRuntimeClassName,
    iterable_GetTrustLevel,
    /* IIterable<Package *> methods */
    iterable_First,
};

static HRESULT empty_packages_create( PACKAGES_T **out )
{
    struct empty_iterable *impl;

    *out = NULL;
    if (!(impl = calloc( 1, sizeof(*impl) ))) return E_OUTOFMEMORY;
    impl->iface.lpVtbl = &iterable_vtbl;
    impl->ref = 1;

    *out = &impl->iface;
    return S_OK;
}

/*
 * PackageManager
 */

struct package_manager
{
    IPackageManager IPackageManager_iface;
    IPackageManager2 IPackageManager2_iface;
    LONG ref;
};

static inline struct package_manager *impl_from_IPackageManager( IPackageManager *iface )
{
    return CONTAINING_RECORD( iface, struct package_manager, IPackageManager_iface );
}

static HRESULT WINAPI manager_QueryInterface( IPackageManager *iface, REFIID iid, void **out )
{
    struct package_manager *impl = impl_from_IPackageManager( iface );

    TRACE( "iface %p, iid %s, out %p.\n", iface, debugstr_guid( iid ), out );

    if (IsEqualGUID( iid, &IID_IUnknown ) ||
        IsEqualGUID( iid, &IID_IInspectable ) ||
        IsEqualGUID( iid, &IID_IAgileObject ) ||
        IsEqualGUID( iid, &IID_IPackageManager ))
    {
        *out = &impl->IPackageManager_iface;
        IInspectable_AddRef( *out );
        return S_OK;
    }

    if (IsEqualGUID( iid, &IID_IPackageManager2 ))
    {
        *out = &impl->IPackageManager2_iface;
        IInspectable_AddRef( *out );
        return S_OK;
    }

    FIXME( "%s not implemented, returning E_NOINTERFACE.\n", debugstr_guid( iid ) );
    *out = NULL;
    return E_NOINTERFACE;
}

static ULONG WINAPI manager_AddRef( IPackageManager *iface )
{
    struct package_manager *impl = impl_from_IPackageManager( iface );
    ULONG ref = InterlockedIncrement( &impl->ref );
    TRACE( "iface %p increasing refcount to %lu.\n", iface, ref );
    return ref;
}

static ULONG WINAPI manager_Release( IPackageManager *iface )
{
    struct package_manager *impl = impl_from_IPackageManager( iface );
    ULONG ref = InterlockedDecrement( &impl->ref );
    TRACE( "iface %p decreasing refcount to %lu.\n", iface, ref );
    if (!ref) free( impl );
    return ref;
}

static HRESULT WINAPI manager_GetIids( IPackageManager *iface, ULONG *iid_count, IID **iids )
{
    FIXME( "iface %p, iid_count %p, iids %p stub!\n", iface, iid_count, iids );
    return E_NOTIMPL;
}

static HRESULT WINAPI manager_GetRuntimeClassName( IPackageManager *iface, HSTRING *class_name )
{
    const WCHAR *name = RuntimeClass_Windows_Management_Deployment_PackageManager;

    TRACE( "iface %p, class_name %p.\n", iface, class_name );

    return WindowsCreateString( name, wcslen( name ), class_name );
}

static HRESULT WINAPI manager_GetTrustLevel( IPackageManager *iface, TrustLevel *trust_level )
{
    TRACE( "iface %p, trust_level %p.\n", iface, trust_level );
    *trust_level = BaseTrust;
    return S_OK;
}

/* IPackageManager methods that return IAsyncOperationWithProgress */

static HRESULT WINAPI manager_AddPackageAsync( IPackageManager *iface, URI_T *uri, URIS_T *dependencies,
                                               OPTIONS_T options, OPERATION_T **operation )
{
    FIXME( "iface %p, uri %p, dependencies %p, options %#x, operation %p stub!\n", iface, uri, dependencies,
           (int)options, operation );
    *operation = NULL;
    return E_NOTIMPL;
}

static HRESULT WINAPI manager_UpdatePackageAsync( IPackageManager *iface, URI_T *uri, URIS_T *dependencies,
                                                  OPTIONS_T options, OPERATION_T **operation )
{
    FIXME( "iface %p, uri %p, dependencies %p, options %#x, operation %p stub!\n", iface, uri, dependencies,
           (int)options, operation );
    *operation = NULL;
    return E_NOTIMPL;
}

static HRESULT WINAPI manager_RemovePackageAsync( IPackageManager *iface, HSTRING name, OPERATION_T **operation )
{
    FIXME( "iface %p, name %s, operation %p stub!\n", iface, debugstr_hstring( name ), operation );
    *operation = NULL;
    return E_NOTIMPL;
}

static HRESULT WINAPI manager_StagePackageAsync( IPackageManager *iface, URI_T *uri, URIS_T *dependencies,
                                                 OPERATION_T **operation )
{
    FIXME( "iface %p, uri %p, dependencies %p, operation %p stub!\n", iface, uri, dependencies, operation );
    *operation = NULL;
    return E_NOTIMPL;
}

static HRESULT WINAPI manager_RegisterPackageAsync( IPackageManager *iface, URI_T *uri, URIS_T *dependencies,
                                                    OPTIONS_T options, OPERATION_T **operation )
{
    FIXME( "iface %p, uri %p, dependencies %p, options %#x, operation %p stub!\n", iface, uri, dependencies,
           (int)options, operation );
    *operation = NULL;
    return E_NOTIMPL;
}

/* IPackageManager methods that return IIterable<Package *>: always an empty enumeration */

static HRESULT WINAPI manager_FindPackages( IPackageManager *iface, PACKAGES_T **packages )
{
    FIXME( "iface %p, packages %p semi-stub!\n", iface, packages );
    return empty_packages_create( packages );
}

static HRESULT WINAPI manager_FindPackagesByUserSecurityId( IPackageManager *iface, HSTRING sid,
                                                            PACKAGES_T **packages )
{
    FIXME( "iface %p, sid %s, packages %p semi-stub!\n", iface, debugstr_hstring( sid ), packages );
    return empty_packages_create( packages );
}

static HRESULT WINAPI manager_FindPackagesByNamePublisher( IPackageManager *iface, HSTRING name,
                                                           HSTRING publisher, PACKAGES_T **packages )
{
    FIXME( "iface %p, name %s, publisher %s, packages %p semi-stub!\n", iface, debugstr_hstring( name ),
           debugstr_hstring( publisher ), packages );
    return empty_packages_create( packages );
}

static HRESULT WINAPI manager_FindPackagesByUserSecurityIdNamePublisher( IPackageManager *iface, HSTRING sid,
                                                                         HSTRING name, HSTRING publisher,
                                                                         PACKAGES_T **packages )
{
    FIXME( "iface %p, sid %s, name %s, publisher %s, packages %p semi-stub!\n", iface, debugstr_hstring( sid ),
           debugstr_hstring( name ), debugstr_hstring( publisher ), packages );
    return empty_packages_create( packages );
}

static HRESULT WINAPI manager_FindUsers( IPackageManager *iface, HSTRING name, USERS_T **users )
{
    FIXME( "iface %p, name %s, users %p stub!\n", iface, debugstr_hstring( name ), users );
    *users = NULL;
    return E_NOTIMPL;
}

static HRESULT WINAPI manager_SetPackageState( IPackageManager *iface, HSTRING name, STATE_T state )
{
    FIXME( "iface %p, name %s, state %#x stub!\n", iface, debugstr_hstring( name ), (int)state );
    return E_NOTIMPL;
}

static HRESULT WINAPI manager_FindPackageByPackageFullName( IPackageManager *iface, HSTRING name,
                                                            PACKAGE_T **package )
{
    FIXME( "iface %p, name %s, package %p semi-stub!\n", iface, debugstr_hstring( name ), package );
    *package = NULL;
    return S_OK;
}

static HRESULT WINAPI manager_CleanupPackageForUserAsync( IPackageManager *iface, HSTRING name, HSTRING sid,
                                                          OPERATION_T **operation )
{
    FIXME( "iface %p, name %s, sid %s, operation %p stub!\n", iface, debugstr_hstring( name ),
           debugstr_hstring( sid ), operation );
    *operation = NULL;
    return E_NOTIMPL;
}

static HRESULT WINAPI manager_FindPackagesByPackageFamilyName( IPackageManager *iface, HSTRING family_name,
                                                               PACKAGES_T **packages )
{
    FIXME( "iface %p, family_name %s, packages %p semi-stub!\n", iface, debugstr_hstring( family_name ),
           packages );
    return empty_packages_create( packages );
}

static HRESULT WINAPI manager_FindPackagesByUserSecurityIdPackageFamilyName( IPackageManager *iface, HSTRING sid,
                                                                             HSTRING family_name,
                                                                             PACKAGES_T **packages )
{
    FIXME( "iface %p, sid %s, family_name %s, packages %p semi-stub!\n", iface, debugstr_hstring( sid ),
           debugstr_hstring( family_name ), packages );
    return empty_packages_create( packages );
}

static HRESULT WINAPI manager_FindPackageByUserSecurityIdPackageFullName( IPackageManager *iface, HSTRING sid,
                                                                          HSTRING name, PACKAGE_T **package )
{
    FIXME( "iface %p, sid %s, name %s, package %p semi-stub!\n", iface, debugstr_hstring( sid ),
           debugstr_hstring( name ), package );
    *package = NULL;
    return S_OK;
}

static const IPackageManagerVtbl manager_vtbl =
{
    manager_QueryInterface,
    manager_AddRef,
    manager_Release,
    /* IInspectable methods */
    manager_GetIids,
    manager_GetRuntimeClassName,
    manager_GetTrustLevel,
    /* IPackageManager methods */
    manager_AddPackageAsync,
    manager_UpdatePackageAsync,
    manager_RemovePackageAsync,
    manager_StagePackageAsync,
    manager_RegisterPackageAsync,
    manager_FindPackages,
    manager_FindPackagesByUserSecurityId,
    manager_FindPackagesByNamePublisher,
    manager_FindPackagesByUserSecurityIdNamePublisher,
    manager_FindUsers,
    manager_SetPackageState,
    manager_FindPackageByPackageFullName,
    manager_CleanupPackageForUserAsync,
    manager_FindPackagesByPackageFamilyName,
    manager_FindPackagesByUserSecurityIdPackageFamilyName,
    manager_FindPackageByUserSecurityIdPackageFullName,
};

/* IPackageManager2: IInspectable methods forward to IPackageManager. */

DEFINE_IINSPECTABLE( manager2, IPackageManager2, struct package_manager, IPackageManager_iface )

static HRESULT WINAPI manager2_RemovePackageWithOptionsAsync( IPackageManager2 *iface, HSTRING name,
                                                              REMOVAL_T options, OPERATION_T **operation )
{
    FIXME( "iface %p, name %s, options %#x, operation %p stub!\n", iface, debugstr_hstring( name ),
           (int)options, operation );
    *operation = NULL;
    return E_NOTIMPL;
}

static HRESULT WINAPI manager2_StagePackageWithOptionsAsync( IPackageManager2 *iface, URI_T *uri,
                                                             URIS_T *dependencies, OPTIONS_T options,
                                                             OPERATION_T **operation )
{
    FIXME( "iface %p, uri %p, dependencies %p, options %#x, operation %p stub!\n", iface, uri, dependencies,
           (int)options, operation );
    *operation = NULL;
    return E_NOTIMPL;
}

static HRESULT WINAPI manager2_RegisterPackageByFullNameAsync( IPackageManager2 *iface, HSTRING name,
                                                               HSTRINGS_T *dependencies, OPTIONS_T options,
                                                               OPERATION_T **operation )
{
    FIXME( "iface %p, name %s, dependencies %p, options %#x, operation %p stub!\n", iface,
           debugstr_hstring( name ), dependencies, (int)options, operation );
    *operation = NULL;
    return E_NOTIMPL;
}

static HRESULT WINAPI manager2_FindPackagesWithPackageTypes( IPackageManager2 *iface, TYPES_T types,
                                                             PACKAGES_T **packages )
{
    FIXME( "iface %p, types %#x, packages %p semi-stub!\n", iface, (int)types, packages );
    return empty_packages_create( packages );
}

static HRESULT WINAPI manager2_FindPackagesByUserSecurityIdWithPackageTypes( IPackageManager2 *iface, HSTRING sid,
                                                                             TYPES_T types, PACKAGES_T **packages )
{
    FIXME( "iface %p, sid %s, types %#x, packages %p semi-stub!\n", iface, debugstr_hstring( sid ), (int)types,
           packages );
    return empty_packages_create( packages );
}

static HRESULT WINAPI manager2_FindPackagesByNamePublisherWithPackageTypes( IPackageManager2 *iface, HSTRING name,
                                                                            HSTRING publisher, TYPES_T types,
                                                                            PACKAGES_T **packages )
{
    FIXME( "iface %p, name %s, publisher %s, types %#x, packages %p semi-stub!\n", iface,
           debugstr_hstring( name ), debugstr_hstring( publisher ), (int)types, packages );
    return empty_packages_create( packages );
}

static HRESULT WINAPI manager2_FindPackagesByUserSecurityIdNamePublisherWithPackageTypes(
        IPackageManager2 *iface, HSTRING sid, HSTRING name, HSTRING publisher, TYPES_T types,
        PACKAGES_T **packages )
{
    FIXME( "iface %p, sid %s, name %s, publisher %s, types %#x, packages %p semi-stub!\n", iface,
           debugstr_hstring( sid ), debugstr_hstring( name ), debugstr_hstring( publisher ), (int)types,
           packages );
    return empty_packages_create( packages );
}

static HRESULT WINAPI manager2_FindPackagesByPackageFamilyNameWithPackageTypes( IPackageManager2 *iface,
                                                                                HSTRING family_name,
                                                                                TYPES_T types,
                                                                                PACKAGES_T **packages )
{
    FIXME( "iface %p, family_name %s, types %#x, packages %p semi-stub!\n", iface,
           debugstr_hstring( family_name ), (int)types, packages );
    return empty_packages_create( packages );
}

static HRESULT WINAPI manager2_FindPackagesByUserSecurityIdPackageFamilyNameWithPackageTypes(
        IPackageManager2 *iface, HSTRING sid, HSTRING family_name, TYPES_T types, PACKAGES_T **packages )
{
    FIXME( "iface %p, sid %s, family_name %s, types %#x, packages %p semi-stub!\n", iface,
           debugstr_hstring( sid ), debugstr_hstring( family_name ), (int)types, packages );
    return empty_packages_create( packages );
}

static HRESULT WINAPI manager2_StageUserDataAsync( IPackageManager2 *iface, HSTRING name,
                                                   OPERATION_T **operation )
{
    FIXME( "iface %p, name %s, operation %p stub!\n", iface, debugstr_hstring( name ), operation );
    *operation = NULL;
    return E_NOTIMPL;
}

static const IPackageManager2Vtbl manager2_vtbl =
{
    manager2_QueryInterface,
    manager2_AddRef,
    manager2_Release,
    /* IInspectable methods */
    manager2_GetIids,
    manager2_GetRuntimeClassName,
    manager2_GetTrustLevel,
    /* IPackageManager2 methods */
    manager2_RemovePackageWithOptionsAsync,
    manager2_StagePackageWithOptionsAsync,
    manager2_RegisterPackageByFullNameAsync,
    manager2_FindPackagesWithPackageTypes,
    manager2_FindPackagesByUserSecurityIdWithPackageTypes,
    manager2_FindPackagesByNamePublisherWithPackageTypes,
    manager2_FindPackagesByUserSecurityIdNamePublisherWithPackageTypes,
    manager2_FindPackagesByPackageFamilyNameWithPackageTypes,
    manager2_FindPackagesByUserSecurityIdPackageFamilyNameWithPackageTypes,
    manager2_StageUserDataAsync,
};

/*
 * Activation factory
 */

struct package_manager_statics
{
    IActivationFactory IActivationFactory_iface;
    LONG ref;
};

static inline struct package_manager_statics *impl_from_IActivationFactory( IActivationFactory *iface )
{
    return CONTAINING_RECORD( iface, struct package_manager_statics, IActivationFactory_iface );
}

static HRESULT WINAPI factory_QueryInterface( IActivationFactory *iface, REFIID iid, void **out )
{
    struct package_manager_statics *impl = impl_from_IActivationFactory( iface );

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

    FIXME( "%s not implemented, returning E_NOINTERFACE.\n", debugstr_guid( iid ) );
    *out = NULL;
    return E_NOINTERFACE;
}

static ULONG WINAPI factory_AddRef( IActivationFactory *iface )
{
    struct package_manager_statics *impl = impl_from_IActivationFactory( iface );
    ULONG ref = InterlockedIncrement( &impl->ref );
    TRACE( "iface %p increasing refcount to %lu.\n", iface, ref );
    return ref;
}

static ULONG WINAPI factory_Release( IActivationFactory *iface )
{
    struct package_manager_statics *impl = impl_from_IActivationFactory( iface );
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
    struct package_manager *impl;

    TRACE( "iface %p, instance %p.\n", iface, instance );

    *instance = NULL;
    if (!(impl = calloc( 1, sizeof(*impl) ))) return E_OUTOFMEMORY;
    impl->IPackageManager_iface.lpVtbl = &manager_vtbl;
    impl->IPackageManager2_iface.lpVtbl = &manager2_vtbl;
    impl->ref = 1;

    *instance = (IInspectable *)&impl->IPackageManager_iface;
    return S_OK;
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

static struct package_manager_statics package_manager_statics =
{
    {&factory_vtbl},
    1
};

IActivationFactory *package_manager_factory = &package_manager_statics.IActivationFactory_iface;
