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
#include "hidusage.h"

WINE_DEFAULT_DEBUG_CHANNEL(twinapi);

struct haptics_waveforms_factory
{
    struct winrt_factory base;
    IKnownSimpleHapticsControllerWaveformsStatics IKnownSimpleHapticsControllerWaveformsStatics_iface;
};

DEFINE_FACTORY_IINSPECTABLE( statics, IKnownSimpleHapticsControllerWaveformsStatics,
                            struct haptics_waveforms_factory )

static HRESULT WINAPI statics_get_Click( IKnownSimpleHapticsControllerWaveformsStatics *iface, UINT16 *value )
{
    TRACE( "iface %p, value %p.\n", iface, value );

    if (!value) return E_POINTER;
    *value = HID_USAGE_HAPTICS_WAVEFORM_CLICK;
    return S_OK;
}

static HRESULT WINAPI statics_get_BuzzContinuous( IKnownSimpleHapticsControllerWaveformsStatics *iface, UINT16 *value )
{
    TRACE( "iface %p, value %p.\n", iface, value );

    if (!value) return E_POINTER;
    *value = HID_USAGE_HAPTICS_WAVEFORM_BUZZ;
    return S_OK;
}

static HRESULT WINAPI statics_get_RumbleContinuous( IKnownSimpleHapticsControllerWaveformsStatics *iface, UINT16 *value )
{
    TRACE( "iface %p, value %p.\n", iface, value );

    if (!value) return E_POINTER;
    *value = HID_USAGE_HAPTICS_WAVEFORM_RUMBLE;
    return S_OK;
}

static HRESULT WINAPI statics_get_Press( IKnownSimpleHapticsControllerWaveformsStatics *iface, UINT16 *value )
{
    TRACE( "iface %p, value %p.\n", iface, value );

    if (!value) return E_POINTER;
    *value = HID_USAGE_HAPTICS_WAVEFORM_PRESS;
    return S_OK;
}

static HRESULT WINAPI statics_get_Release( IKnownSimpleHapticsControllerWaveformsStatics *iface, UINT16 *value )
{
    TRACE( "iface %p, value %p.\n", iface, value );

    if (!value) return E_POINTER;
    *value = HID_USAGE_HAPTICS_WAVEFORM_RELEASE;
    return S_OK;
}

static const IKnownSimpleHapticsControllerWaveformsStaticsVtbl statics_vtbl =
{
    statics_QueryInterface,
    statics_AddRef,
    statics_Release,
    statics_GetIids,
    statics_GetRuntimeClassName,
    statics_GetTrustLevel,
    statics_get_Click,
    statics_get_BuzzContinuous,
    statics_get_RumbleContinuous,
    statics_get_Press,
    statics_get_Release,
};

static const struct winrt_factory_interface factory_interfaces[] =
{
    {&IID_IKnownSimpleHapticsControllerWaveformsStatics,
     offsetof(struct haptics_waveforms_factory, IKnownSimpleHapticsControllerWaveformsStatics_iface)},
};

static struct haptics_waveforms_factory factory =
{
    {{&winrt_factory_vtbl}, 1, RuntimeClass_Windows_Devices_Haptics_KnownSimpleHapticsControllerWaveforms,
      factory_interfaces, ARRAY_SIZE(factory_interfaces), TRUE, NULL},
    {&statics_vtbl},
};

IActivationFactory *haptics_waveforms_factory = &factory.base.IActivationFactory_iface;
