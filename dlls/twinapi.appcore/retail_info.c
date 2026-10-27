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

#include "roapi.h"

WINE_DEFAULT_DEBUG_CHANNEL(twinapi);

struct retail_info_factory
{
    struct winrt_factory base;
    IRetailInfoStatics IRetailInfoStatics_iface;
};

DEFINE_FACTORY_IINSPECTABLE( statics, IRetailInfoStatics, struct retail_info_factory )

static HRESULT WINAPI statics_get_IsDemoModeEnabled( IRetailInfoStatics *iface, boolean *value )
{
    TRACE( "iface %p, value %p.\n", iface, value );

    if (!value) return E_POINTER;
    *value = FALSE;
    return S_OK;
}

static HRESULT WINAPI statics_get_Properties( IRetailInfoStatics *iface, IMapView_HSTRING_IInspectable **value )
{
    IActivationFactory *activation_factory = NULL;
    IMap_HSTRING_IInspectable *map = NULL;
    IInspectable *property_set = NULL;
    HSTRING class_name = NULL;
    HRESULT hr;

    TRACE( "iface %p, value %p.\n", iface, value );

    if (!value) return E_POINTER;
    *value = NULL;

    if (FAILED(hr = WindowsCreateString( RuntimeClass_Windows_Foundation_Collections_PropertySet,
                                        wcslen(RuntimeClass_Windows_Foundation_Collections_PropertySet),
                                        &class_name )))
        return hr;
    hr = RoGetActivationFactory( class_name, &IID_IActivationFactory, (void **)&activation_factory );
    WindowsDeleteString( class_name );
    if (SUCCEEDED(hr)) hr = IActivationFactory_ActivateInstance( activation_factory, &property_set );
    if (SUCCEEDED(hr))
        hr = IInspectable_QueryInterface( property_set, &IID_IMap_HSTRING_IInspectable, (void **)&map );
    if (SUCCEEDED(hr)) hr = IMap_HSTRING_IInspectable_GetView( map, value );
    if (map) IMap_HSTRING_IInspectable_Release( map );
    if (property_set) IInspectable_Release( property_set );
    if (activation_factory) IActivationFactory_Release( activation_factory );
    TRACE( "returning %#lx.\n", hr );
    return hr;
}

static const IRetailInfoStaticsVtbl statics_vtbl =
{
    statics_QueryInterface,
    statics_AddRef,
    statics_Release,
    /* IInspectable methods */
    statics_GetIids,
    statics_GetRuntimeClassName,
    statics_GetTrustLevel,
    /* IRetailInfoStatics methods */
    statics_get_IsDemoModeEnabled,
    statics_get_Properties,
};

static const struct winrt_factory_interface factory_interfaces[] =
{
    {&IID_IRetailInfoStatics, offsetof(struct retail_info_factory, IRetailInfoStatics_iface)},
};

static struct retail_info_factory factory =
{
    {{&winrt_factory_vtbl}, 1, RuntimeClass_Windows_System_Profile_RetailInfo,
      factory_interfaces, ARRAY_SIZE(factory_interfaces), TRUE, NULL},
    {&statics_vtbl},
};

IActivationFactory *retail_info_factory = &factory.base.IActivationFactory_iface;
