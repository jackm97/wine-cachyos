/* WinRT Windows.Devices.Input implementation
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

#include "initguid.h"
#include <stdarg.h>

#define COBJMACROS
#include "windef.h"
#include "winbase.h"
#include "winstring.h"

#include "activation.h"
#include "roapi.h"

#define WIDL_using_Windows_Devices_Input
#include "windows.devices.input.h"

struct pen_device_statics
{
    IActivationFactory IActivationFactory_iface;
    IPenDeviceStatics IPenDeviceStatics_iface;
    LONG ref;
};

static inline struct pen_device_statics *impl_from_IActivationFactory( IActivationFactory *iface )
{
    return CONTAINING_RECORD( iface, struct pen_device_statics, IActivationFactory_iface );
}

static inline struct pen_device_statics *impl_from_IPenDeviceStatics( IPenDeviceStatics *iface )
{
    return CONTAINING_RECORD( iface, struct pen_device_statics, IPenDeviceStatics_iface );
}

static HRESULT WINAPI factory_QueryInterface( IActivationFactory *iface, REFIID iid, void **out )
{
    struct pen_device_statics *impl = impl_from_IActivationFactory( iface );

    if (!out) return E_POINTER;
    *out = NULL;
    if (IsEqualGUID( iid, &IID_IUnknown ) || IsEqualGUID( iid, &IID_IInspectable ) ||
        IsEqualGUID( iid, &IID_IAgileObject ) || IsEqualGUID( iid, &IID_IActivationFactory ))
        *out = &impl->IActivationFactory_iface;
    else if (IsEqualGUID( iid, &IID_IPenDeviceStatics ))
        *out = &impl->IPenDeviceStatics_iface;
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

static HRESULT WINAPI pen_device_statics_QueryInterface( IPenDeviceStatics *iface, REFIID iid, void **out )
{
    struct pen_device_statics *impl = impl_from_IPenDeviceStatics( iface );
    return IActivationFactory_QueryInterface( &impl->IActivationFactory_iface, iid, out );
}

static ULONG WINAPI pen_device_statics_AddRef( IPenDeviceStatics *iface )
{
    struct pen_device_statics *impl = impl_from_IPenDeviceStatics( iface );
    return IActivationFactory_AddRef( &impl->IActivationFactory_iface );
}

static ULONG WINAPI pen_device_statics_Release( IPenDeviceStatics *iface )
{
    struct pen_device_statics *impl = impl_from_IPenDeviceStatics( iface );
    return IActivationFactory_Release( &impl->IActivationFactory_iface );
}

static HRESULT WINAPI pen_device_statics_GetIids( IPenDeviceStatics *iface, ULONG *count, IID **iids )
{
    return E_NOTIMPL;
}

static HRESULT WINAPI pen_device_statics_GetRuntimeClassName( IPenDeviceStatics *iface, HSTRING *name )
{
    return E_NOTIMPL;
}

static HRESULT WINAPI pen_device_statics_GetTrustLevel( IPenDeviceStatics *iface, TrustLevel *level )
{
    return E_NOTIMPL;
}

static HRESULT WINAPI pen_device_statics_GetFromPointerId( IPenDeviceStatics *iface, UINT32 pointer_id,
        IPenDevice **result )
{
    if (!result) return E_POINTER;
    *result = NULL;
    return S_OK;
}

static const IPenDeviceStaticsVtbl pen_device_statics_vtbl =
{
    pen_device_statics_QueryInterface,
    pen_device_statics_AddRef,
    pen_device_statics_Release,
    pen_device_statics_GetIids,
    pen_device_statics_GetRuntimeClassName,
    pen_device_statics_GetTrustLevel,
    pen_device_statics_GetFromPointerId,
};

static struct pen_device_statics pen_device_statics =
{
    {&factory_vtbl},
    {&pen_device_statics_vtbl},
    1,
};

HRESULT WINAPI DllGetClassObject( REFCLSID clsid, REFIID iid, void **out )
{
    return CLASS_E_CLASSNOTAVAILABLE;
}

HRESULT WINAPI DllGetActivationFactory( HSTRING classid, IActivationFactory **factory )
{
    const WCHAR *name = WindowsGetStringRawBuffer( classid, NULL );

    if (!factory) return E_POINTER;
    *factory = NULL;
    if (!wcscmp( name, RuntimeClass_Windows_Devices_Input_PenDevice ))
        return IActivationFactory_QueryInterface( &pen_device_statics.IActivationFactory_iface,
                &IID_IActivationFactory, (void **)factory );
    return CLASS_E_CLASSNOTAVAILABLE;
}
