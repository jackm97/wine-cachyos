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

struct input_configuration
{
    IInkInputProcessingConfiguration IInkInputProcessingConfiguration_iface;
    LONG ref, mode, right_drag;
    IUnknown *marshaler;
};

static struct input_configuration *impl_from_IInkInputProcessingConfiguration(IInkInputProcessingConfiguration *iface)
{
    return CONTAINING_RECORD(iface, struct input_configuration, IInkInputProcessingConfiguration_iface);
}

static HRESULT WINAPI configuration_QueryInterface(IInkInputProcessingConfiguration *iface, REFIID iid, void **out)
{
    struct input_configuration *impl = impl_from_IInkInputProcessingConfiguration(iface);

    if (!out) return E_POINTER;
    *out = NULL;
    if (IsEqualGUID(iid, &IID_IUnknown) || IsEqualGUID(iid, &IID_IInspectable)
            || IsEqualGUID(iid, &IID_IAgileObject) || IsEqualGUID(iid, &IID_IInkInputProcessingConfiguration))
    {
        *out = iface;
        IInkInputProcessingConfiguration_AddRef(iface);
        return S_OK;
    }
    if (IsEqualGUID(iid, &IID_IMarshal)) return IUnknown_QueryInterface(impl->marshaler, iid, out);
    return E_NOINTERFACE;
}

static ULONG WINAPI configuration_AddRef(IInkInputProcessingConfiguration *iface)
{
    return InterlockedIncrement(&impl_from_IInkInputProcessingConfiguration(iface)->ref);
}

static ULONG WINAPI configuration_Release(IInkInputProcessingConfiguration *iface)
{
    struct input_configuration *impl = impl_from_IInkInputProcessingConfiguration(iface);
    ULONG ref = InterlockedDecrement(&impl->ref);

    if (!ref)
    {
        if (impl->marshaler) IUnknown_Release(impl->marshaler);
        free(impl);
        InterlockedDecrement(&ink_object_count);
    }
    return ref;
}

static HRESULT WINAPI configuration_GetIids(IInkInputProcessingConfiguration *iface, ULONG *count, IID **iids)
{
    if (!count || !iids) return E_POINTER;
    *count = 0;
    *iids = NULL;
    if (!(*iids = CoTaskMemAlloc(sizeof(**iids)))) return E_OUTOFMEMORY;
    **iids = IID_IInkInputProcessingConfiguration;
    *count = 1;
    return S_OK;
}

static HRESULT WINAPI configuration_GetRuntimeClassName(IInkInputProcessingConfiguration *iface, HSTRING *name)
{
    const WCHAR *class = RuntimeClass_Windows_UI_Input_Inking_InkInputProcessingConfiguration;
    if (!name) return E_POINTER;
    return WindowsCreateString(class, wcslen(class), name);
}

static HRESULT WINAPI configuration_GetTrustLevel(IInkInputProcessingConfiguration *iface, TrustLevel *level)
{
    if (!level) return E_POINTER;
    *level = BaseTrust;
    return S_OK;
}

static HRESULT WINAPI configuration_get_Mode(IInkInputProcessingConfiguration *iface, enum InkInputProcessingMode *value)
{
    if (!value) return E_POINTER;
    *value = InterlockedCompareExchange(&impl_from_IInkInputProcessingConfiguration(iface)->mode, 0, 0);
    return S_OK;
}

static HRESULT WINAPI configuration_put_Mode(IInkInputProcessingConfiguration *iface, enum InkInputProcessingMode value)
{
    if (value < InkInputProcessingMode_None || value > InkInputProcessingMode_Erasing) return E_INVALIDARG;
    InterlockedExchange(&impl_from_IInkInputProcessingConfiguration(iface)->mode, value);
    return S_OK;
}

static HRESULT WINAPI configuration_get_RightDragAction(IInkInputProcessingConfiguration *iface, enum InkInputRightDragAction *value)
{
    if (!value) return E_POINTER;
    *value = InterlockedCompareExchange(&impl_from_IInkInputProcessingConfiguration(iface)->right_drag, 0, 0);
    return S_OK;
}

static HRESULT WINAPI configuration_put_RightDragAction(IInkInputProcessingConfiguration *iface, enum InkInputRightDragAction value)
{
    if (value != InkInputRightDragAction_LeaveUnprocessed && value != InkInputRightDragAction_AllowProcessing)
        return E_INVALIDARG;
    InterlockedExchange(&impl_from_IInkInputProcessingConfiguration(iface)->right_drag, value);
    return S_OK;
}

static const IInkInputProcessingConfigurationVtbl configuration_vtbl =
{
    configuration_QueryInterface, configuration_AddRef, configuration_Release,
    configuration_GetIids, configuration_GetRuntimeClassName, configuration_GetTrustLevel,
    configuration_get_Mode, configuration_put_Mode,
    configuration_get_RightDragAction, configuration_put_RightDragAction,
};

HRESULT ink_input_configuration_create(IInkInputProcessingConfiguration **out)
{
    struct input_configuration *impl;
    HRESULT hr;

    if (!out) return E_POINTER;
    *out = NULL;
    if (!(impl = calloc(1, sizeof(*impl)))) return E_OUTOFMEMORY;
    impl->IInkInputProcessingConfiguration_iface.lpVtbl = &configuration_vtbl;
    impl->ref = 1;
    impl->mode = InkInputProcessingMode_Inking;
    impl->right_drag = InkInputRightDragAction_AllowProcessing;
    InterlockedIncrement(&ink_object_count);
    hr = CoCreateFreeThreadedMarshaler((IUnknown *)&impl->IInkInputProcessingConfiguration_iface, &impl->marshaler);
    if (FAILED(hr)) IInkInputProcessingConfiguration_Release(&impl->IInkInputProcessingConfiguration_iface);
    else *out = &impl->IInkInputProcessingConfiguration_iface;
    return hr;
}
