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

struct ink_point
{
    IInkPoint IInkPoint_iface;
    IInkPoint2 IInkPoint2_iface;
    LONG ref;
    IUnknown *marshaler;
    struct Point position;
    float pressure, tilt_x, tilt_y;
    UINT64 timestamp;
};

static struct ink_point *impl_from_IInkPoint(IInkPoint *iface)
{
    return CONTAINING_RECORD(iface, struct ink_point, IInkPoint_iface);
}

static HRESULT WINAPI point_QueryInterface(IInkPoint *iface, REFIID iid, void **out)
{
    struct ink_point *impl = impl_from_IInkPoint(iface);

    if (!out) return E_POINTER;
    *out = NULL;
    if (IsEqualGUID(iid, &IID_IUnknown) || IsEqualGUID(iid, &IID_IInspectable)
            || IsEqualGUID(iid, &IID_IAgileObject) || IsEqualGUID(iid, &IID_IInkPoint))
        *out = &impl->IInkPoint_iface;
    else if (IsEqualGUID(iid, &IID_IInkPoint2))
        *out = &impl->IInkPoint2_iface;
    else if (IsEqualGUID(iid, &IID_IMarshal))
        return IUnknown_QueryInterface(impl->marshaler, iid, out);
    else return E_NOINTERFACE;
    IInkPoint_AddRef(iface);
    return S_OK;
}

static ULONG WINAPI point_AddRef(IInkPoint *iface)
{
    return InterlockedIncrement(&impl_from_IInkPoint(iface)->ref);
}

static ULONG WINAPI point_Release(IInkPoint *iface)
{
    struct ink_point *impl = impl_from_IInkPoint(iface);
    ULONG ref = InterlockedDecrement(&impl->ref);

    if (!ref)
    {
        if (impl->marshaler) IUnknown_Release(impl->marshaler);
        free(impl);
        InterlockedDecrement(&ink_object_count);
    }
    return ref;
}

static HRESULT WINAPI point_GetIids(IInkPoint *iface, ULONG *count, IID **iids)
{
    if (!count || !iids) return E_POINTER;
    *count = 0;
    *iids = NULL;
    if (!(*iids = CoTaskMemAlloc(2 * sizeof(**iids)))) return E_OUTOFMEMORY;
    (*iids)[0] = IID_IInkPoint;
    (*iids)[1] = IID_IInkPoint2;
    *count = 2;
    return S_OK;
}

static HRESULT WINAPI point_GetRuntimeClassName(IInkPoint *iface, HSTRING *name)
{
    const WCHAR *class = RuntimeClass_Windows_UI_Input_Inking_InkPoint;
    if (!name) return E_POINTER;
    return WindowsCreateString(class, wcslen(class), name);
}

static HRESULT WINAPI point_GetTrustLevel(IInkPoint *iface, TrustLevel *level)
{
    if (!level) return E_POINTER;
    *level = BaseTrust;
    return S_OK;
}

static HRESULT WINAPI point_get_Position(IInkPoint *iface, struct Point *value)
{
    if (!value) return E_POINTER;
    *value = impl_from_IInkPoint(iface)->position;
    return S_OK;
}

static HRESULT WINAPI point_get_Pressure(IInkPoint *iface, float *value)
{
    if (!value) return E_POINTER;
    *value = impl_from_IInkPoint(iface)->pressure;
    return S_OK;
}

static const IInkPointVtbl point_vtbl =
{
    point_QueryInterface, point_AddRef, point_Release,
    point_GetIids, point_GetRuntimeClassName, point_GetTrustLevel,
    point_get_Position, point_get_Pressure,
};

DEFINE_IINSPECTABLE(point2, IInkPoint2, struct ink_point, IInkPoint_iface)

static HRESULT WINAPI point2_get_TiltX(IInkPoint2 *iface, float *value)
{
    if (!value) return E_POINTER;
    *value = impl_from_IInkPoint2(iface)->tilt_x;
    return S_OK;
}

static HRESULT WINAPI point2_get_TiltY(IInkPoint2 *iface, float *value)
{
    if (!value) return E_POINTER;
    *value = impl_from_IInkPoint2(iface)->tilt_y;
    return S_OK;
}

static HRESULT WINAPI point2_get_Timestamp(IInkPoint2 *iface, UINT64 *value)
{
    if (!value) return E_POINTER;
    *value = impl_from_IInkPoint2(iface)->timestamp;
    return S_OK;
}

static const IInkPoint2Vtbl point2_vtbl =
{
    point2_QueryInterface, point2_AddRef, point2_Release,
    point2_GetIids, point2_GetRuntimeClassName, point2_GetTrustLevel,
    point2_get_TiltX, point2_get_TiltY, point2_get_Timestamp,
};

HRESULT ink_point_create(struct Point position, float pressure, float tilt_x, float tilt_y,
        UINT64 timestamp, IInkPoint **out)
{
    struct ink_point *impl;
    HRESULT hr;

    if (!out) return E_POINTER;
    *out = NULL;
    if (!(impl = calloc(1, sizeof(*impl)))) return E_OUTOFMEMORY;
    impl->IInkPoint_iface.lpVtbl = &point_vtbl;
    impl->IInkPoint2_iface.lpVtbl = &point2_vtbl;
    impl->ref = 1;
    impl->position = position;
    impl->pressure = pressure;
    impl->tilt_x = tilt_x;
    impl->tilt_y = tilt_y;
    impl->timestamp = timestamp;
    InterlockedIncrement(&ink_object_count);
    hr = CoCreateFreeThreadedMarshaler((IUnknown *)&impl->IInkPoint_iface, &impl->marshaler);
    if (FAILED(hr)) IInkPoint_Release(&impl->IInkPoint_iface);
    else *out = &impl->IInkPoint_iface;
    return hr;
}
