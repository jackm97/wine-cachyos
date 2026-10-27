/*
 * Copyright 2026 Mirko Brombin
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

struct shared_mode_settings_factory
{
    struct winrt_factory base;
    ISharedModeSettingsStatics ISharedModeSettingsStatics_iface;
    ISharedModeSettingsStatics2 ISharedModeSettingsStatics2_iface;
};

DEFINE_FACTORY_IINSPECTABLE( statics, ISharedModeSettingsStatics, struct shared_mode_settings_factory )
DEFINE_FACTORY_IINSPECTABLE( statics2, ISharedModeSettingsStatics2, struct shared_mode_settings_factory )

static HRESULT WINAPI statics_get_IsEnabled( ISharedModeSettingsStatics *iface, boolean *value )
{
    TRACE( "iface %p, value %p.\n", iface, value );

    if (!value) return E_POINTER;
    *value = FALSE;
    return S_OK;
}

static HRESULT WINAPI statics2_get_ShouldAvoidLocalStorage( ISharedModeSettingsStatics2 *iface, boolean *value )
{
    TRACE( "iface %p, value %p.\n", iface, value );

    if (!value) return E_POINTER;
    *value = FALSE;
    return S_OK;
}

static const ISharedModeSettingsStaticsVtbl statics_vtbl =
{
    statics_QueryInterface,
    statics_AddRef,
    statics_Release,
    /* IInspectable methods */
    statics_GetIids,
    statics_GetRuntimeClassName,
    statics_GetTrustLevel,
    /* ISharedModeSettingsStatics methods */
    statics_get_IsEnabled,
};

static const ISharedModeSettingsStatics2Vtbl statics2_vtbl =
{
    statics2_QueryInterface,
    statics2_AddRef,
    statics2_Release,
    /* IInspectable methods */
    statics2_GetIids,
    statics2_GetRuntimeClassName,
    statics2_GetTrustLevel,
    /* ISharedModeSettingsStatics2 methods */
    statics2_get_ShouldAvoidLocalStorage,
};

static const struct winrt_factory_interface factory_interfaces[] =
{
    {&IID_ISharedModeSettingsStatics, offsetof(struct shared_mode_settings_factory, ISharedModeSettingsStatics_iface)},
    {&IID_ISharedModeSettingsStatics2, offsetof(struct shared_mode_settings_factory, ISharedModeSettingsStatics2_iface)},
};

static struct shared_mode_settings_factory factory =
{
    {{&winrt_factory_vtbl}, 1, RuntimeClass_Windows_System_Profile_SharedModeSettings,
      factory_interfaces, ARRAY_SIZE(factory_interfaces), TRUE, NULL},
    {&statics_vtbl},
    {&statics2_vtbl},
};

IActivationFactory *shared_mode_settings_factory = &factory.base.IActivationFactory_iface;
