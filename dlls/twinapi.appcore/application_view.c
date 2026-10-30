/* WinRT Windows.UI.ViewManagement.ApplicationView implementation
 *
 * Copyright 2025 Zhiyi Zhang for CodeWeavers
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

struct factory
{
    struct winrt_factory base;
    IApplicationViewStatics IApplicationViewStatics_iface;
    IApplicationViewStatics2 IApplicationViewStatics2_iface;
};

DEFINE_FACTORY_IINSPECTABLE( statics, IApplicationViewStatics, struct factory )

static HRESULT WINAPI statics_Value(IApplicationViewStatics *iface, ApplicationViewState *value)
{
    FIXME("iface %p, value %p stub!\n", iface, value);
    return E_NOTIMPL;
}

static HRESULT WINAPI statics_TryUnsnap(IApplicationViewStatics *iface, boolean *success)
{
    FIXME("iface %p, success %p stub!\n", iface, success);
    return E_NOTIMPL;
}

static const struct IApplicationViewStaticsVtbl statics_vtbl =
{
    statics_QueryInterface,
    statics_AddRef,
    statics_Release,
    /* IInspectable methods */
    statics_GetIids,
    statics_GetRuntimeClassName,
    statics_GetTrustLevel,
    /* IApplicationViewStatics methods */
    statics_Value,
    statics_TryUnsnap,
};

DEFINE_FACTORY_IINSPECTABLE( statics2, IApplicationViewStatics2, struct factory )

static HRESULT WINAPI statics2_GetForCurrentView(IApplicationViewStatics2 *iface, IApplicationView **current)
{
    FIXME("iface %p, current %p stub!\n", iface, current);
    return E_NOTIMPL;
}

static HRESULT WINAPI statics2_get_TerminateAppOnFinalViewClose(IApplicationViewStatics2 *iface, boolean *value)
{
    FIXME("iface %p, value %p stub!\n", iface, value);
    return E_NOTIMPL;
}

static HRESULT WINAPI statics2_put_TerminateAppOnFinalViewClose(IApplicationViewStatics2 *iface, boolean value)
{
    FIXME("iface %p, value %d stub!\n", iface, value);
    return E_NOTIMPL;
}

static const struct IApplicationViewStatics2Vtbl statics2_vtbl =
{
    statics2_QueryInterface,
    statics2_AddRef,
    statics2_Release,
    /* IInspectable methods */
    statics2_GetIids,
    statics2_GetRuntimeClassName,
    statics2_GetTrustLevel,
    /* IApplicationViewStatics2 methods */
    statics2_GetForCurrentView,
    statics2_get_TerminateAppOnFinalViewClose,
    statics2_put_TerminateAppOnFinalViewClose
};

static const struct winrt_factory_interface factory_interfaces[] =
{
    {&IID_IApplicationViewStatics, offsetof(struct factory, IApplicationViewStatics_iface)},
    {&IID_IApplicationViewStatics2, offsetof(struct factory, IApplicationViewStatics2_iface)},
};

static struct factory factory =
{
    {{&winrt_factory_vtbl}, 1, RuntimeClass_Windows_UI_ViewManagement_ApplicationView,
      factory_interfaces, ARRAY_SIZE(factory_interfaces), TRUE, NULL},
    {&statics_vtbl},
    {&statics2_vtbl},
};

IActivationFactory *application_view_factory = &factory.base.IActivationFactory_iface;
