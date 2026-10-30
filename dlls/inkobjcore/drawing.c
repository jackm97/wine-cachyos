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

#include <math.h>
#include "private.h"

struct drawing_attributes
{
    IInkDrawingAttributes IInkDrawingAttributes_iface;
    IInkDrawingAttributes2 IInkDrawingAttributes2_iface;
    IInkDrawingAttributes4 IInkDrawingAttributes4_iface;
    LONG ref;
    IUnknown *marshaler;
    CRITICAL_SECTION lock;
    struct Color color;
    struct Size size;
    enum PenTipShape pen_tip;
    struct Matrix3x2 transform;
    boolean ignore_pressure, fit_to_curve, highlighter, ignore_tilt;
};

static struct drawing_attributes *impl_from_IInkDrawingAttributes(IInkDrawingAttributes *iface)
{
    return CONTAINING_RECORD(iface, struct drawing_attributes, IInkDrawingAttributes_iface);
}

static HRESULT WINAPI drawing_QueryInterface(IInkDrawingAttributes *iface, REFIID iid, void **out)
{
    struct drawing_attributes *impl = impl_from_IInkDrawingAttributes(iface);

    if (!out) return E_POINTER;
    *out = NULL;
    if (IsEqualGUID(iid, &IID_IUnknown) || IsEqualGUID(iid, &IID_IInspectable)
            || IsEqualGUID(iid, &IID_IAgileObject) || IsEqualGUID(iid, &IID_IInkDrawingAttributes))
        *out = &impl->IInkDrawingAttributes_iface;
    else if (IsEqualGUID(iid, &IID_IInkDrawingAttributes2))
        *out = &impl->IInkDrawingAttributes2_iface;
    else if (IsEqualGUID(iid, &IID_IInkDrawingAttributes4))
        *out = &impl->IInkDrawingAttributes4_iface;
    else if (IsEqualGUID(iid, &IID_IMarshal))
        return IUnknown_QueryInterface(impl->marshaler, iid, out);
    else return E_NOINTERFACE;
    IInkDrawingAttributes_AddRef(iface);
    return S_OK;
}

static ULONG WINAPI drawing_AddRef(IInkDrawingAttributes *iface)
{
    return InterlockedIncrement(&impl_from_IInkDrawingAttributes(iface)->ref);
}

static ULONG WINAPI drawing_Release(IInkDrawingAttributes *iface)
{
    struct drawing_attributes *impl = impl_from_IInkDrawingAttributes(iface);
    ULONG ref = InterlockedDecrement(&impl->ref);

    if (!ref)
    {
        if (impl->marshaler) IUnknown_Release(impl->marshaler);
        DeleteCriticalSection(&impl->lock);
        free(impl);
        InterlockedDecrement(&ink_object_count);
    }
    return ref;
}

static HRESULT WINAPI drawing_GetIids(IInkDrawingAttributes *iface, ULONG *count, IID **iids)
{
    if (!count || !iids) return E_POINTER;
    *count = 0;
    *iids = NULL;
    if (!(*iids = CoTaskMemAlloc(3 * sizeof(**iids)))) return E_OUTOFMEMORY;
    (*iids)[0] = IID_IInkDrawingAttributes;
    (*iids)[1] = IID_IInkDrawingAttributes2;
    (*iids)[2] = IID_IInkDrawingAttributes4;
    *count = 3;
    return S_OK;
}

static HRESULT WINAPI drawing_GetRuntimeClassName(IInkDrawingAttributes *iface, HSTRING *name)
{
    const WCHAR *class = RuntimeClass_Windows_UI_Input_Inking_InkDrawingAttributes;
    if (!name) return E_POINTER;
    return WindowsCreateString(class, wcslen(class), name);
}

static HRESULT WINAPI drawing_GetTrustLevel(IInkDrawingAttributes *iface, TrustLevel *level)
{
    if (!level) return E_POINTER;
    *level = BaseTrust;
    return S_OK;
}

#define DRAWING_GET(prefix, interface, name, type, member) \
static HRESULT WINAPI prefix##_get_##name(interface *iface, type *value) \
{ \
    struct drawing_attributes *impl = impl_from_##interface(iface); \
    if (!value) return E_POINTER; \
    EnterCriticalSection(&impl->lock); \
    *value = impl->member; \
    LeaveCriticalSection(&impl->lock); \
    return S_OK; \
}

#define DRAWING_PUT(prefix, interface, name, type, member, expression) \
static HRESULT WINAPI prefix##_put_##name(interface *iface, type value) \
{ \
    struct drawing_attributes *impl = impl_from_##interface(iface); \
    EnterCriticalSection(&impl->lock); \
    impl->member = expression; \
    LeaveCriticalSection(&impl->lock); \
    return S_OK; \
}

DRAWING_GET(drawing, IInkDrawingAttributes, Color, struct Color, color)
DRAWING_PUT(drawing, IInkDrawingAttributes, Color, struct Color, color, value)
DRAWING_GET(drawing, IInkDrawingAttributes, PenTip, enum PenTipShape, pen_tip)
DRAWING_GET(drawing, IInkDrawingAttributes, Size, struct Size, size)
DRAWING_GET(drawing, IInkDrawingAttributes, IgnorePressure, boolean, ignore_pressure)
DRAWING_PUT(drawing, IInkDrawingAttributes, IgnorePressure, boolean, ignore_pressure, !!value)
DRAWING_GET(drawing, IInkDrawingAttributes, FitToCurve, boolean, fit_to_curve)
DRAWING_PUT(drawing, IInkDrawingAttributes, FitToCurve, boolean, fit_to_curve, !!value)

static HRESULT WINAPI drawing_put_PenTip(IInkDrawingAttributes *iface, enum PenTipShape value)
{
    struct drawing_attributes *impl = impl_from_IInkDrawingAttributes(iface);

    if (value != PenTipShape_Circle && value != PenTipShape_Rectangle) return E_INVALIDARG;
    EnterCriticalSection(&impl->lock);
    impl->pen_tip = value;
    LeaveCriticalSection(&impl->lock);
    return S_OK;
}

static HRESULT WINAPI drawing_put_Size(IInkDrawingAttributes *iface, struct Size value)
{
    struct drawing_attributes *impl = impl_from_IInkDrawingAttributes(iface);

    if (!isfinite(value.Width) || !isfinite(value.Height) || value.Width <= 0 || value.Height <= 0)
        return E_INVALIDARG;
    EnterCriticalSection(&impl->lock);
    impl->size = value;
    LeaveCriticalSection(&impl->lock);
    return S_OK;
}

static const IInkDrawingAttributesVtbl drawing_vtbl =
{
    drawing_QueryInterface, drawing_AddRef, drawing_Release,
    drawing_GetIids, drawing_GetRuntimeClassName, drawing_GetTrustLevel,
    drawing_get_Color, drawing_put_Color, drawing_get_PenTip, drawing_put_PenTip,
    drawing_get_Size, drawing_put_Size, drawing_get_IgnorePressure, drawing_put_IgnorePressure,
    drawing_get_FitToCurve, drawing_put_FitToCurve,
};

DEFINE_IINSPECTABLE(drawing2, IInkDrawingAttributes2, struct drawing_attributes, IInkDrawingAttributes_iface)

DRAWING_GET(drawing2, IInkDrawingAttributes2, PenTipTransform, struct Matrix3x2, transform)
DRAWING_GET(drawing2, IInkDrawingAttributes2, DrawAsHighlighter, boolean, highlighter)
DRAWING_PUT(drawing2, IInkDrawingAttributes2, DrawAsHighlighter, boolean, highlighter, !!value)

static HRESULT WINAPI drawing2_put_PenTipTransform(IInkDrawingAttributes2 *iface, struct Matrix3x2 value)
{
    struct drawing_attributes *impl = impl_from_IInkDrawingAttributes2(iface);

    if (!isfinite(value.M11) || !isfinite(value.M12) || !isfinite(value.M21) || !isfinite(value.M22)
            || value.M31 != 0 || value.M32 != 0) return E_INVALIDARG;
    EnterCriticalSection(&impl->lock);
    impl->transform = value;
    LeaveCriticalSection(&impl->lock);
    return S_OK;
}

static const IInkDrawingAttributes2Vtbl drawing2_vtbl =
{
    drawing2_QueryInterface, drawing2_AddRef, drawing2_Release,
    drawing2_GetIids, drawing2_GetRuntimeClassName, drawing2_GetTrustLevel,
    drawing2_get_PenTipTransform, drawing2_put_PenTipTransform,
    drawing2_get_DrawAsHighlighter, drawing2_put_DrawAsHighlighter,
};

DEFINE_IINSPECTABLE(drawing4, IInkDrawingAttributes4, struct drawing_attributes, IInkDrawingAttributes_iface)

DRAWING_GET(drawing4, IInkDrawingAttributes4, IgnoreTilt, boolean, ignore_tilt)
DRAWING_PUT(drawing4, IInkDrawingAttributes4, IgnoreTilt, boolean, ignore_tilt, !!value)

static const IInkDrawingAttributes4Vtbl drawing4_vtbl =
{
    drawing4_QueryInterface, drawing4_AddRef, drawing4_Release,
    drawing4_GetIids, drawing4_GetRuntimeClassName, drawing4_GetTrustLevel,
    drawing4_get_IgnoreTilt, drawing4_put_IgnoreTilt,
};

HRESULT ink_drawing_attributes_create(IInkDrawingAttributes **out)
{
    struct drawing_attributes *impl;
    HRESULT hr;

    if (!out) return E_POINTER;
    *out = NULL;
    if (!(impl = calloc(1, sizeof(*impl)))) return E_OUTOFMEMORY;
    impl->IInkDrawingAttributes_iface.lpVtbl = &drawing_vtbl;
    impl->IInkDrawingAttributes2_iface.lpVtbl = &drawing2_vtbl;
    impl->IInkDrawingAttributes4_iface.lpVtbl = &drawing4_vtbl;
    impl->ref = 1;
    impl->color.A = 255;
    impl->size.Width = impl->size.Height = 2;
    impl->fit_to_curve = TRUE;
    impl->transform.M11 = impl->transform.M22 = 1;
    InitializeCriticalSection(&impl->lock);
    InterlockedIncrement(&ink_object_count);
    hr = CoCreateFreeThreadedMarshaler((IUnknown *)&impl->IInkDrawingAttributes_iface, &impl->marshaler);
    if (FAILED(hr)) IInkDrawingAttributes_Release(&impl->IInkDrawingAttributes_iface);
    else *out = &impl->IInkDrawingAttributes_iface;
    return hr;
}

HRESULT ink_drawing_attributes_clone(IInkDrawingAttributes *source, IInkDrawingAttributes **out)
{
    IInkDrawingAttributes2 *source2 = NULL, *copy2 = NULL;
    IInkDrawingAttributes4 *source4 = NULL, *copy4 = NULL;
    struct drawing_attributes *input, *copy;
    struct Matrix3x2 transform;
    struct Color color;
    struct Size size;
    enum PenTipShape tip;
    boolean value;
    HRESULT hr, query;

    if (!out) return E_POINTER;
    *out = NULL;
    if (!source) return E_POINTER;
    if (FAILED(hr = ink_drawing_attributes_create(out))) return hr;
    if (source->lpVtbl == &drawing_vtbl)
    {
        input = impl_from_IInkDrawingAttributes(source);
        copy = impl_from_IInkDrawingAttributes(*out);
        EnterCriticalSection(&input->lock);
        copy->color = input->color;
        copy->size = input->size;
        copy->pen_tip = input->pen_tip;
        copy->transform = input->transform;
        copy->ignore_pressure = input->ignore_pressure;
        copy->fit_to_curve = input->fit_to_curve;
        copy->highlighter = input->highlighter;
        copy->ignore_tilt = input->ignore_tilt;
        LeaveCriticalSection(&input->lock);
        return S_OK;
    }

    hr = IInkDrawingAttributes_get_Color(source, &color);
    if (SUCCEEDED(hr)) hr = IInkDrawingAttributes_put_Color(*out, color);
    if (SUCCEEDED(hr)) hr = IInkDrawingAttributes_get_PenTip(source, &tip);
    if (SUCCEEDED(hr)) hr = IInkDrawingAttributes_put_PenTip(*out, tip);
    if (SUCCEEDED(hr)) hr = IInkDrawingAttributes_get_Size(source, &size);
    if (SUCCEEDED(hr)) hr = IInkDrawingAttributes_put_Size(*out, size);
    if (SUCCEEDED(hr)) hr = IInkDrawingAttributes_get_IgnorePressure(source, &value);
    if (SUCCEEDED(hr)) hr = IInkDrawingAttributes_put_IgnorePressure(*out, value);
    if (SUCCEEDED(hr)) hr = IInkDrawingAttributes_get_FitToCurve(source, &value);
    if (SUCCEEDED(hr)) hr = IInkDrawingAttributes_put_FitToCurve(*out, value);
    if (SUCCEEDED(hr))
    {
        query = IInkDrawingAttributes_QueryInterface(source, &IID_IInkDrawingAttributes2, (void **)&source2);
        if (SUCCEEDED(query))
        {
            hr = IInkDrawingAttributes_QueryInterface(*out, &IID_IInkDrawingAttributes2, (void **)&copy2);
            if (SUCCEEDED(hr)) hr = IInkDrawingAttributes2_get_PenTipTransform(source2, &transform);
            if (SUCCEEDED(hr)) hr = IInkDrawingAttributes2_put_PenTipTransform(copy2, transform);
            if (SUCCEEDED(hr)) hr = IInkDrawingAttributes2_get_DrawAsHighlighter(source2, &value);
            if (SUCCEEDED(hr)) hr = IInkDrawingAttributes2_put_DrawAsHighlighter(copy2, value);
        }
        else if (query != E_NOINTERFACE) hr = query;
    }
    if (SUCCEEDED(hr))
    {
        query = IInkDrawingAttributes_QueryInterface(source, &IID_IInkDrawingAttributes4, (void **)&source4);
        if (SUCCEEDED(query))
        {
            hr = IInkDrawingAttributes_QueryInterface(*out, &IID_IInkDrawingAttributes4, (void **)&copy4);
            if (SUCCEEDED(hr)) hr = IInkDrawingAttributes4_get_IgnoreTilt(source4, &value);
            if (SUCCEEDED(hr)) hr = IInkDrawingAttributes4_put_IgnoreTilt(copy4, value);
        }
        else if (query != E_NOINTERFACE) hr = query;
    }
    if (source2) IInkDrawingAttributes2_Release(source2);
    if (copy2) IInkDrawingAttributes2_Release(copy2);
    if (source4) IInkDrawingAttributes4_Release(source4);
    if (copy4) IInkDrawingAttributes4_Release(copy4);
    if (FAILED(hr))
    {
        IInkDrawingAttributes_Release(*out);
        *out = NULL;
    }
    return hr;
}
