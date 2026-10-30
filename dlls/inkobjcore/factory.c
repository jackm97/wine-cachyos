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

struct ink_factory
{
    IActivationFactory IActivationFactory_iface;
    IInkPointFactory IInkPointFactory_iface;
    IInkPointFactory2 IInkPointFactory2_iface;
    LONG ref;
    enum { FACTORY_DRAWING, FACTORY_POINT, FACTORY_HOST } kind;
};

static struct ink_factory *impl_from_IActivationFactory(IActivationFactory *iface)
{
    return CONTAINING_RECORD(iface, struct ink_factory, IActivationFactory_iface);
}

static HRESULT WINAPI factory_QueryInterface(IActivationFactory *iface, REFIID iid, void **out)
{
    struct ink_factory *impl = impl_from_IActivationFactory(iface);

    if (!out) return E_POINTER;
    *out = NULL;
    if (IsEqualGUID(iid, &IID_IUnknown) || IsEqualGUID(iid, &IID_IInspectable)
            || IsEqualGUID(iid, &IID_IAgileObject) || IsEqualGUID(iid, &IID_IActivationFactory))
        *out = &impl->IActivationFactory_iface;
    else if (impl->kind == FACTORY_POINT && IsEqualGUID(iid, &IID_IInkPointFactory))
        *out = &impl->IInkPointFactory_iface;
    else if (impl->kind == FACTORY_POINT && IsEqualGUID(iid, &IID_IInkPointFactory2))
        *out = &impl->IInkPointFactory2_iface;
    else return E_NOINTERFACE;
    IActivationFactory_AddRef(iface);
    return S_OK;
}

static ULONG WINAPI factory_AddRef(IActivationFactory *iface)
{
    InterlockedIncrement(&ink_factory_refs);
    return InterlockedIncrement(&impl_from_IActivationFactory(iface)->ref);
}

static ULONG WINAPI factory_Release(IActivationFactory *iface)
{
    ULONG ref = InterlockedDecrement(&impl_from_IActivationFactory(iface)->ref);
    InterlockedDecrement(&ink_factory_refs);
    return ref;
}

static HRESULT WINAPI factory_GetIids(IActivationFactory *iface, ULONG *count, IID **iids)
{
    struct ink_factory *impl = impl_from_IActivationFactory(iface);
    ULONG size = impl->kind == FACTORY_POINT ? 3 : 1;

    if (!count || !iids) return E_POINTER;
    *count = 0;
    *iids = NULL;
    if (!(*iids = CoTaskMemAlloc(size * sizeof(**iids)))) return E_OUTOFMEMORY;
    (*iids)[0] = IID_IActivationFactory;
    if (impl->kind == FACTORY_POINT)
    {
        (*iids)[1] = IID_IInkPointFactory;
        (*iids)[2] = IID_IInkPointFactory2;
    }
    *count = size;
    return S_OK;
}

static HRESULT WINAPI factory_GetRuntimeClassName(IActivationFactory *iface, HSTRING *name)
{
    struct ink_factory *impl = impl_from_IActivationFactory(iface);
    const WCHAR *class = impl->kind == FACTORY_POINT ? RuntimeClass_Windows_UI_Input_Inking_InkPoint
            : RuntimeClass_Windows_UI_Input_Inking_InkDrawingAttributes;

    if (impl->kind == FACTORY_HOST) class = RuntimeClass_Windows_UI_Input_Inking_Core_CoreInkPresenterHost;
    if (!name) return E_POINTER;
    return WindowsCreateString(class, wcslen(class), name);
}

static HRESULT WINAPI factory_GetTrustLevel(IActivationFactory *iface, TrustLevel *level)
{
    if (!level) return E_POINTER;
    *level = BaseTrust;
    return S_OK;
}

static HRESULT WINAPI factory_ActivateInstance(IActivationFactory *iface, IInspectable **out)
{
    if (!out) return E_POINTER;
    *out = NULL;
    if (impl_from_IActivationFactory(iface)->kind == FACTORY_POINT) return E_NOTIMPL;
    if (impl_from_IActivationFactory(iface)->kind == FACTORY_HOST) return ink_core_host_create(out);
    return ink_drawing_attributes_create((IInkDrawingAttributes **)out);
}

static const IActivationFactoryVtbl factory_vtbl =
{
    factory_QueryInterface, factory_AddRef, factory_Release,
    factory_GetIids, factory_GetRuntimeClassName, factory_GetTrustLevel, factory_ActivateInstance,
};

DEFINE_IINSPECTABLE(point_factory, IInkPointFactory, struct ink_factory, IActivationFactory_iface)

static HRESULT WINAPI point_factory_CreateInkPoint(IInkPointFactory *iface, struct Point position,
        float pressure, IInkPoint **out)
{
    return ink_point_create(position, pressure, 0, 0, 0, out);
}

static const IInkPointFactoryVtbl point_factory_vtbl =
{
    point_factory_QueryInterface, point_factory_AddRef, point_factory_Release,
    point_factory_GetIids, point_factory_GetRuntimeClassName, point_factory_GetTrustLevel,
    point_factory_CreateInkPoint,
};

DEFINE_IINSPECTABLE(point_factory2, IInkPointFactory2, struct ink_factory, IActivationFactory_iface)

static HRESULT WINAPI point_factory2_CreateInkPointWithTiltAndTimestamp(IInkPointFactory2 *iface,
        struct Point position, float pressure, float tilt_x, float tilt_y, UINT64 timestamp, IInkPoint **out)
{
    return ink_point_create(position, pressure, tilt_x, tilt_y, timestamp, out);
}

static const IInkPointFactory2Vtbl point_factory2_vtbl =
{
    point_factory2_QueryInterface, point_factory2_AddRef, point_factory2_Release,
    point_factory2_GetIids, point_factory2_GetRuntimeClassName, point_factory2_GetTrustLevel,
    point_factory2_CreateInkPointWithTiltAndTimestamp,
};

static struct ink_factory point_factory =
{
    {&factory_vtbl}, {&point_factory_vtbl}, {&point_factory2_vtbl}, 0, FACTORY_POINT,
};
static struct ink_factory drawing_factory = {{&factory_vtbl}};
static struct ink_factory host_factory = {{&factory_vtbl}, {0}, {0}, 0, FACTORY_HOST};

HRESULT WINAPI DllGetActivationFactory(HSTRING name, IActivationFactory **out)
{
    const WCHAR *class = WindowsGetStringRawBuffer(name, NULL);

    if (!out) return E_POINTER;
    *out = NULL;
    if (!wcscmp(class, RuntimeClass_Windows_UI_Input_Inking_InkPoint))
        return IActivationFactory_QueryInterface(&point_factory.IActivationFactory_iface,
                &IID_IActivationFactory, (void **)out);
    if (!wcscmp(class, RuntimeClass_Windows_UI_Input_Inking_InkDrawingAttributes))
        return IActivationFactory_QueryInterface(&drawing_factory.IActivationFactory_iface,
                &IID_IActivationFactory, (void **)out);
    if (!wcscmp(class, RuntimeClass_Windows_UI_Input_Inking_Core_CoreInkPresenterHost))
        return IActivationFactory_QueryInterface(&host_factory.IActivationFactory_iface,
                &IID_IActivationFactory, (void **)out);
    if (!wcscmp(class, RuntimeClass_Windows_UI_Input_Inking_Core_CoreWetStrokeUpdateSource))
        return ink_source_get_factory(TRUE, out);
    if (!wcscmp(class, RuntimeClass_Windows_UI_Input_Inking_Core_CoreInkIndependentInputSource))
        return ink_source_get_factory(FALSE, out);
    return CLASS_E_CLASSNOTAVAILABLE;
}
