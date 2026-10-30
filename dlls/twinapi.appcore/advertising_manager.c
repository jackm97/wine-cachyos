/* WinRT Windows.Globalization implementation
 *
 * Copyright 2021 Rémi Bernon for CodeWeavers
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

struct advertising_manager_factory
{
    struct winrt_factory base;
    IAdvertisingManagerStatics IAdvertisingManagerStatics_iface;
};

DEFINE_FACTORY_IINSPECTABLE( statics, IAdvertisingManagerStatics, struct advertising_manager_factory )

static HRESULT WINAPI statics_get_AdvertisingId( IAdvertisingManagerStatics *iface, HSTRING *out )
{
    FIXME( "iface %p, out %p stub!\n", iface, out );
    return WindowsCreateString( NULL, 0, out );
}

static const struct IAdvertisingManagerStaticsVtbl statics_vtbl =
{
    statics_QueryInterface,
    statics_AddRef,
    statics_Release,
    /* IInspectable methods */
    statics_GetIids,
    statics_GetRuntimeClassName,
    statics_GetTrustLevel,
    /* IAdvertisingManagerStatics methods */
    statics_get_AdvertisingId,
};

static const struct winrt_factory_interface factory_interfaces[] =
{
    {&IID_IAdvertisingManagerStatics, offsetof(struct advertising_manager_factory, IAdvertisingManagerStatics_iface)},
};

static struct advertising_manager_factory factory =
{
    {{&winrt_factory_vtbl}, 1, RuntimeClass_Windows_System_UserProfile_AdvertisingManager,
      factory_interfaces, ARRAY_SIZE(factory_interfaces), TRUE, NULL},
    {&statics_vtbl},
};

IActivationFactory *advertising_manager_factory = &factory.base.IActivationFactory_iface;
