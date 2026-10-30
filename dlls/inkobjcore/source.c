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

struct core_source
{
    ICoreWetStrokeUpdateSource ICoreWetStrokeUpdateSource_iface;
    ICoreInkIndependentInputSource ICoreInkIndependentInputSource_iface;
    LONG ref;
    BOOL wet;
    IUnknown *marshaler;
    IInkPresenter *presenter;
    struct ink_events events;
};

static IUnknown *source_identity(struct core_source *source)
{
    if (source->wet) return (IUnknown *)&source->ICoreWetStrokeUpdateSource_iface;
    return (IUnknown *)&source->ICoreInkIndependentInputSource_iface;
}

static const WCHAR *source_name(BOOL wet)
{
    return wet ? RuntimeClass_Windows_UI_Input_Inking_Core_CoreWetStrokeUpdateSource
            : RuntimeClass_Windows_UI_Input_Inking_Core_CoreInkIndependentInputSource;
}

static HRESULT source_query(struct core_source *source, REFIID iid, void **out)
{
    const IID *type = source->wet ? &IID_ICoreWetStrokeUpdateSource : &IID_ICoreInkIndependentInputSource;

    if (!out) return E_POINTER;
    *out = NULL;
    if (IsEqualGUID(iid, &IID_IUnknown) || IsEqualGUID(iid, &IID_IInspectable)
            || IsEqualGUID(iid, &IID_IAgileObject) || IsEqualGUID(iid, type))
    {
        *out = source_identity(source);
        InterlockedIncrement(&source->ref);
        return S_OK;
    }
    if (IsEqualGUID(iid, &IID_IMarshal)) return IUnknown_QueryInterface(source->marshaler, iid, out);
    return E_NOINTERFACE;
}

static ULONG source_release(struct core_source *source)
{
    ULONG ref = InterlockedDecrement(&source->ref);

    if (!ref)
    {
        ink_events_clear(&source->events);
        DeleteCriticalSection(&source->events.lock);
        if (source->presenter) IInkPresenter_Release(source->presenter);
        if (source->marshaler) IUnknown_Release(source->marshaler);
        free(source);
        InterlockedDecrement(&ink_object_count);
    }
    return ref;
}

#define SOURCE_INSPECTABLE(prefix, type) \
static struct core_source *prefix##_impl(type *iface) \
{ return CONTAINING_RECORD(iface, struct core_source, type##_iface); } \
static HRESULT WINAPI prefix##_QueryInterface(type *iface, REFIID iid, void **out) \
{ return source_query(prefix##_impl(iface), iid, out); } \
static ULONG WINAPI prefix##_AddRef(type *iface) { return InterlockedIncrement(&prefix##_impl(iface)->ref); } \
static ULONG WINAPI prefix##_Release(type *iface) { return source_release(prefix##_impl(iface)); } \
static HRESULT WINAPI prefix##_GetIids(type *iface, ULONG *count, IID **iids) \
{ \
    if (!count || !iids) return E_POINTER; \
    *count = 0; \
    *iids = NULL; \
    if (!(*iids = CoTaskMemAlloc(sizeof(**iids)))) return E_OUTOFMEMORY; \
    **iids = IID_##type; \
    *count = 1; \
    return S_OK; \
} \
static HRESULT WINAPI prefix##_GetRuntimeClassName(type *iface, HSTRING *name) \
{ \
    const WCHAR *class = source_name(prefix##_impl(iface)->wet); \
    if (!name) return E_POINTER; \
    return WindowsCreateString(class, wcslen(class), name); \
} \
static HRESULT WINAPI prefix##_GetTrustLevel(type *iface, TrustLevel *level) \
{ \
    if (!level) return E_POINTER; \
    *level = BaseTrust; \
    return S_OK; \
} \
static HRESULT WINAPI prefix##_get_InkPresenter(type *iface, IInkPresenter **value) \
{ \
    if (!value) return E_POINTER; \
    *value = prefix##_impl(iface)->presenter; \
    IInkPresenter_AddRef(*value); \
    return S_OK; \
}

SOURCE_INSPECTABLE(wet, ICoreWetStrokeUpdateSource)
SOURCE_INSPECTABLE(independent, ICoreInkIndependentInputSource)

#define SOURCE_EVENT(prefix, type, handler_type, name, id) \
static HRESULT WINAPI prefix##_add_##name(type *iface, handler_type *handler, EventRegistrationToken *token) \
{ return ink_event_add(&prefix##_impl(iface)->events, id, (IUnknown *)handler, token); } \
static HRESULT WINAPI prefix##_remove_##name(type *iface, EventRegistrationToken token) \
{ return ink_event_remove(&prefix##_impl(iface)->events, id, token); }

#define WET_EVENT(name, id) SOURCE_EVENT(wet, ICoreWetStrokeUpdateSource, ITypedEventHandler_CoreWetStrokeUpdateSource_CoreWetStrokeUpdateEventArgs, name, id)
#define POINTER_EVENT(name, id) SOURCE_EVENT(independent, ICoreInkIndependentInputSource, ITypedEventHandler_CoreInkIndependentInputSource_PointerEventArgs, name, id)
WET_EVENT(WetStrokeStarting, 0)
WET_EVENT(WetStrokeContinuing, 1)
WET_EVENT(WetStrokeStopping, 2)
WET_EVENT(WetStrokeCompleted, 3)
WET_EVENT(WetStrokeCanceled, 4)
POINTER_EVENT(PointerEntering, 0)
POINTER_EVENT(PointerHovering, 1)
POINTER_EVENT(PointerExiting, 2)
POINTER_EVENT(PointerPressing, 3)
POINTER_EVENT(PointerMoving, 4)
POINTER_EVENT(PointerReleasing, 5)
POINTER_EVENT(PointerLost, 6)

static const ICoreWetStrokeUpdateSourceVtbl wet_vtbl =
{
    wet_QueryInterface, wet_AddRef, wet_Release, wet_GetIids, wet_GetRuntimeClassName, wet_GetTrustLevel,
    wet_add_WetStrokeStarting, wet_remove_WetStrokeStarting,
    wet_add_WetStrokeContinuing, wet_remove_WetStrokeContinuing,
    wet_add_WetStrokeStopping, wet_remove_WetStrokeStopping,
    wet_add_WetStrokeCompleted, wet_remove_WetStrokeCompleted,
    wet_add_WetStrokeCanceled, wet_remove_WetStrokeCanceled, wet_get_InkPresenter,
};

static const ICoreInkIndependentInputSourceVtbl independent_vtbl =
{
    independent_QueryInterface, independent_AddRef, independent_Release,
    independent_GetIids, independent_GetRuntimeClassName, independent_GetTrustLevel,
    independent_add_PointerEntering, independent_remove_PointerEntering,
    independent_add_PointerHovering, independent_remove_PointerHovering,
    independent_add_PointerExiting, independent_remove_PointerExiting,
    independent_add_PointerPressing, independent_remove_PointerPressing,
    independent_add_PointerMoving, independent_remove_PointerMoving,
    independent_add_PointerReleasing, independent_remove_PointerReleasing,
    independent_add_PointerLost, independent_remove_PointerLost, independent_get_InkPresenter,
};

static HRESULT source_create(BOOL wet, IInkPresenter *presenter, void **out)
{
    struct core_source *source;
    IInkPresenter *retained;
    HRESULT hr;

    if (!out) return E_POINTER;
    *out = NULL;
    if (!presenter) return E_POINTER;
    hr = IInkPresenter_QueryInterface(presenter, &IID_IInkPresenter, (void **)&retained);
    if (FAILED(hr)) return hr;
    if (!(source = calloc(1, sizeof(*source))))
    {
        IInkPresenter_Release(retained);
        return E_OUTOFMEMORY;
    }
    source->ICoreWetStrokeUpdateSource_iface.lpVtbl = &wet_vtbl;
    source->ICoreInkIndependentInputSource_iface.lpVtbl = &independent_vtbl;
    source->ref = 1;
    source->wet = wet;
    source->presenter = retained;
    ink_events_init(&source->events);
    InterlockedIncrement(&ink_object_count);
    hr = CoCreateFreeThreadedMarshaler(source_identity(source), &source->marshaler);
    if (FAILED(hr)) source_release(source);
    else *out = source_identity(source);
    return hr;
}

struct source_factory
{
    IActivationFactory IActivationFactory_iface;
    ICoreWetStrokeUpdateSourceStatics ICoreWetStrokeUpdateSourceStatics_iface;
    ICoreInkIndependentInputSourceStatics ICoreInkIndependentInputSourceStatics_iface;
    LONG ref;
    BOOL wet;
};

static struct source_factory *impl_from_IActivationFactory(IActivationFactory *iface)
{
    return CONTAINING_RECORD(iface, struct source_factory, IActivationFactory_iface);
}

static HRESULT WINAPI factory_QueryInterface(IActivationFactory *iface, REFIID iid, void **out)
{
    struct source_factory *factory = impl_from_IActivationFactory(iface);

    if (!out) return E_POINTER;
    *out = NULL;
    if (IsEqualGUID(iid, &IID_IUnknown) || IsEqualGUID(iid, &IID_IInspectable)
            || IsEqualGUID(iid, &IID_IAgileObject) || IsEqualGUID(iid, &IID_IActivationFactory)) *out = iface;
    else if (factory->wet && IsEqualGUID(iid, &IID_ICoreWetStrokeUpdateSourceStatics))
        *out = &factory->ICoreWetStrokeUpdateSourceStatics_iface;
    else if (!factory->wet && IsEqualGUID(iid, &IID_ICoreInkIndependentInputSourceStatics))
        *out = &factory->ICoreInkIndependentInputSourceStatics_iface;
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
    if (!count || !iids) return E_POINTER;
    *count = 0;
    *iids = NULL;
    if (!(*iids = CoTaskMemAlloc(2 * sizeof(**iids)))) return E_OUTOFMEMORY;
    (*iids)[0] = IID_IActivationFactory;
    (*iids)[1] = impl_from_IActivationFactory(iface)->wet ? IID_ICoreWetStrokeUpdateSourceStatics
            : IID_ICoreInkIndependentInputSourceStatics;
    *count = 2;
    return S_OK;
}

static HRESULT WINAPI factory_GetRuntimeClassName(IActivationFactory *iface, HSTRING *name)
{
    const WCHAR *class = source_name(impl_from_IActivationFactory(iface)->wet);
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
    return E_NOTIMPL;
}

static const IActivationFactoryVtbl factory_vtbl =
{
    factory_QueryInterface, factory_AddRef, factory_Release,
    factory_GetIids, factory_GetRuntimeClassName, factory_GetTrustLevel, factory_ActivateInstance,
};

DEFINE_IINSPECTABLE(wet_factory, ICoreWetStrokeUpdateSourceStatics, struct source_factory, IActivationFactory_iface)
DEFINE_IINSPECTABLE(independent_factory, ICoreInkIndependentInputSourceStatics, struct source_factory, IActivationFactory_iface)

static HRESULT WINAPI wet_factory_Create(ICoreWetStrokeUpdateSourceStatics *iface, IInkPresenter *presenter, ICoreWetStrokeUpdateSource **out)
{
    return source_create(TRUE, presenter, (void **)out);
}

static HRESULT WINAPI independent_factory_Create(ICoreInkIndependentInputSourceStatics *iface, IInkPresenter *presenter, ICoreInkIndependentInputSource **out)
{
    return source_create(FALSE, presenter, (void **)out);
}

static const ICoreWetStrokeUpdateSourceStaticsVtbl wet_factory_vtbl =
{
    wet_factory_QueryInterface, wet_factory_AddRef, wet_factory_Release,
    wet_factory_GetIids, wet_factory_GetRuntimeClassName, wet_factory_GetTrustLevel, wet_factory_Create,
};

static const ICoreInkIndependentInputSourceStaticsVtbl independent_factory_vtbl =
{
    independent_factory_QueryInterface, independent_factory_AddRef, independent_factory_Release,
    independent_factory_GetIids, independent_factory_GetRuntimeClassName, independent_factory_GetTrustLevel, independent_factory_Create,
};

static struct source_factory wet_factory = {{&factory_vtbl}, {&wet_factory_vtbl}, {0}, 0, TRUE};
static struct source_factory independent_factory = {{&factory_vtbl}, {0}, {&independent_factory_vtbl}, 0, FALSE};

HRESULT ink_source_get_factory(BOOL wet, IActivationFactory **out)
{
    return IActivationFactory_QueryInterface(wet ? &wet_factory.IActivationFactory_iface
            : &independent_factory.IActivationFactory_iface, &IID_IActivationFactory, (void **)out);
}
