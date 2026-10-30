/*
 * Copyright 2023 Rémi Bernon for CodeWeavers
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

WINE_DEFAULT_DEBUG_CHANNEL(twinapi);

struct client_device_information
{
    IEasClientDeviceInformation IEasClientDeviceInformation_iface;
    LONG ref;
};

static inline struct client_device_information *impl_from_IEasClientDeviceInformation( IEasClientDeviceInformation *iface )
{
    return CONTAINING_RECORD( iface, struct client_device_information, IEasClientDeviceInformation_iface );
}

static HRESULT WINAPI client_device_information_QueryInterface( IEasClientDeviceInformation *iface,
                                                                REFIID iid, void **out )
{
    struct client_device_information *impl = impl_from_IEasClientDeviceInformation( iface );

    TRACE( "iface %p, iid %s, out %p.\n", iface, debugstr_guid( iid ), out );

    if (IsEqualGUID( iid, &IID_IUnknown ) ||
        IsEqualGUID( iid, &IID_IInspectable ) ||
        IsEqualGUID( iid, &IID_IEasClientDeviceInformation ))
    {
        IEasClientDeviceInformation_AddRef( (*out = &impl->IEasClientDeviceInformation_iface) );
        return S_OK;
    }

    FIXME( "%s not implemented, returning E_NOINTERFACE.\n", debugstr_guid( iid ) );
    *out = NULL;
    return E_NOINTERFACE;
}

static ULONG WINAPI client_device_information_AddRef( IEasClientDeviceInformation *iface )
{
    struct client_device_information *impl = impl_from_IEasClientDeviceInformation( iface );
    ULONG ref = InterlockedIncrement( &impl->ref );
    TRACE( "iface %p increasing refcount to %lu.\n", iface, ref );
    return ref;
}

static ULONG WINAPI client_device_information_Release( IEasClientDeviceInformation *iface )
{
    struct client_device_information *impl = impl_from_IEasClientDeviceInformation( iface );
    ULONG ref = InterlockedDecrement( &impl->ref );
    TRACE( "iface %p decreasing refcount to %lu.\n", iface, ref );
    if (!ref) free( impl );
    return ref;
}

static HRESULT WINAPI client_device_information_GetIids( IEasClientDeviceInformation *iface, ULONG *iid_count, IID **iids )
{
    FIXME( "iface %p, iid_count %p, iids %p stub!\n", iface, iid_count, iids );
    return E_NOTIMPL;
}

static HRESULT WINAPI client_device_information_GetRuntimeClassName( IEasClientDeviceInformation *iface, HSTRING *class_name )
{
    FIXME( "iface %p, class_name %p stub!\n", iface, class_name );
    return E_NOTIMPL;
}

static HRESULT WINAPI client_device_information_GetTrustLevel( IEasClientDeviceInformation *iface, TrustLevel *trust_level )
{
    FIXME( "iface %p, trust_level %p stub!\n", iface, trust_level );
    return E_NOTIMPL;
}

static HRESULT WINAPI client_device_information_get_Id( IEasClientDeviceInformation *iface, GUID *value )
{
    FIXME( "iface %p, value %p stub.\n", iface, value );

    return E_NOTIMPL;
}

static HRESULT WINAPI client_device_information_get_OperatingSystem( IEasClientDeviceInformation *iface, HSTRING *value )
{
    FIXME( "iface %p, value %p stub.\n", iface, value );
    return WindowsCreateString( NULL, 0, value );
}

static HRESULT WINAPI client_device_information_get_FriendlyName( IEasClientDeviceInformation *iface, HSTRING *value )
{
    FIXME( "iface %p, value %p stub.\n", iface, value );
    return WindowsCreateString( NULL, 0, value );
}

static HRESULT WINAPI client_device_information_get_SystemManufacturer( IEasClientDeviceInformation *iface, HSTRING *value )
{
    FIXME( "iface %p, value %p stub.\n", iface, value );
    return WindowsCreateString( NULL, 0, value );
}

static HRESULT WINAPI client_device_information_get_SystemProductName( IEasClientDeviceInformation *iface, HSTRING *value )
{
    FIXME( "iface %p, value %p stub.\n", iface, value );
    return WindowsCreateString( NULL, 0, value );
}

static HRESULT WINAPI client_device_information_get_SystemSku( IEasClientDeviceInformation *iface, HSTRING *value )
{
    FIXME( "iface %p, value %p stub.\n", iface, value );
    return WindowsCreateString( NULL, 0, value );
}

static IEasClientDeviceInformationVtbl client_device_information_vtbl =
{
    client_device_information_QueryInterface,
    client_device_information_AddRef,
    client_device_information_Release,
    /* IInspectable methods */
    client_device_information_GetIids,
    client_device_information_GetRuntimeClassName,
    client_device_information_GetTrustLevel,
    /* IEasClientDeviceInformation methods */
    client_device_information_get_Id,
    client_device_information_get_OperatingSystem,
    client_device_information_get_FriendlyName,
    client_device_information_get_SystemManufacturer,
    client_device_information_get_SystemProductName,
    client_device_information_get_SystemSku,
};

static HRESULT create_client_device_information( IInspectable **instance )
{
    struct client_device_information *impl;

    TRACE( "instance %p.\n", instance );

    if (!(impl = calloc( 1, sizeof(*impl) ))) return E_OUTOFMEMORY;
    impl->IEasClientDeviceInformation_iface.lpVtbl = &client_device_information_vtbl;
    impl->ref = 1;

    *instance = (IInspectable *)&impl->IEasClientDeviceInformation_iface;
    return S_OK;
};

static struct winrt_factory factory =
{
    {&winrt_factory_vtbl},
    1,
    RuntimeClass_Windows_Security_ExchangeActiveSyncProvisioning_EasClientDeviceInformation,
    NULL,
    0,
    FALSE,
    create_client_device_information,
};

IActivationFactory *client_device_information_factory = &factory.IActivationFactory_iface;
