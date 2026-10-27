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
#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(ink);

struct ink_presenter;

struct stroke_input
{
    IInkStrokeInput IInkStrokeInput_iface;
    struct ink_presenter *owner;
    IUnknown *marshaler;
};

struct unprocessed_input
{
    IInkUnprocessedInput IInkUnprocessedInput_iface;
    struct ink_presenter *owner;
    IUnknown *marshaler;
};

struct ink_presenter
{
    IInkPresenter IInkPresenter_iface;
    LONG ref, input_enabled, device_types;
    IUnknown *marshaler;
    IInkInputProcessingConfiguration *configuration;
    CRITICAL_SECTION lock;
    IInkDrawingAttributes *attributes;
    IInkSynchronizer *synchronizer;
    struct stroke_input stroke;
    struct unprocessed_input unprocessed;
    struct ink_events events;
};

static struct ink_presenter *impl_from_IInkPresenter(IInkPresenter *iface)
{
    return CONTAINING_RECORD(iface, struct ink_presenter, IInkPresenter_iface);
}

static HRESULT WINAPI presenter_QueryInterface(IInkPresenter *iface, REFIID iid, void **out)
{
    struct ink_presenter *impl = impl_from_IInkPresenter(iface);

    if (!out) return E_POINTER;
    *out = NULL;
    if (IsEqualGUID(iid, &IID_IUnknown) || IsEqualGUID(iid, &IID_IInspectable)
            || IsEqualGUID(iid, &IID_IAgileObject) || IsEqualGUID(iid, &IID_IInkPresenter))
    {
        *out = iface;
        IInkPresenter_AddRef(iface);
        return S_OK;
    }
    if (IsEqualGUID(iid, &IID_IMarshal)) return IUnknown_QueryInterface(impl->marshaler, iid, out);
    return E_NOINTERFACE;
}

static ULONG WINAPI presenter_AddRef(IInkPresenter *iface)
{
    return InterlockedIncrement(&impl_from_IInkPresenter(iface)->ref);
}

static ULONG WINAPI presenter_Release(IInkPresenter *iface)
{
    struct ink_presenter *impl = impl_from_IInkPresenter(iface);
    ULONG ref = InterlockedDecrement(&impl->ref);

    if (!ref)
    {
        ink_events_clear(&impl->events);
        DeleteCriticalSection(&impl->events.lock);
        if (impl->stroke.marshaler) IUnknown_Release(impl->stroke.marshaler);
        if (impl->unprocessed.marshaler) IUnknown_Release(impl->unprocessed.marshaler);
        if (impl->attributes) IInkDrawingAttributes_Release(impl->attributes);
        if (impl->synchronizer) IInkSynchronizer_Release(impl->synchronizer);
        if (impl->configuration) IInkInputProcessingConfiguration_Release(impl->configuration);
        if (impl->marshaler) IUnknown_Release(impl->marshaler);
        DeleteCriticalSection(&impl->lock);
        free(impl);
        InterlockedDecrement(&ink_object_count);
    }
    return ref;
}

static HRESULT WINAPI presenter_GetIids(IInkPresenter *iface, ULONG *count, IID **iids)
{
    if (!count || !iids) return E_POINTER;
    *count = 0;
    *iids = NULL;
    if (!(*iids = CoTaskMemAlloc(sizeof(**iids)))) return E_OUTOFMEMORY;
    **iids = IID_IInkPresenter;
    *count = 1;
    return S_OK;
}

static HRESULT WINAPI presenter_GetRuntimeClassName(IInkPresenter *iface, HSTRING *name)
{
    const WCHAR *class = RuntimeClass_Windows_UI_Input_Inking_InkPresenter;
    if (!name) return E_POINTER;
    return WindowsCreateString(class, wcslen(class), name);
}

static HRESULT WINAPI presenter_GetTrustLevel(IInkPresenter *iface, TrustLevel *level)
{
    if (!level) return E_POINTER;
    *level = BaseTrust;
    return S_OK;
}

static HRESULT WINAPI presenter_get_IsInputEnabled(IInkPresenter *iface, boolean *value)
{
    if (!value) return E_POINTER;
    *value = InterlockedCompareExchange(&impl_from_IInkPresenter(iface)->input_enabled, 0, 0);
    return S_OK;
}

static HRESULT WINAPI presenter_put_IsInputEnabled(IInkPresenter *iface, boolean value)
{
    InterlockedExchange(&impl_from_IInkPresenter(iface)->input_enabled, !!value);
    return S_OK;
}

static HRESULT WINAPI presenter_get_InputDeviceTypes(IInkPresenter *iface, enum CoreInputDeviceTypes *value)
{
    if (!value) return E_POINTER;
    *value = InterlockedCompareExchange(&impl_from_IInkPresenter(iface)->device_types, 0, 0);
    return S_OK;
}

static HRESULT WINAPI presenter_put_InputDeviceTypes(IInkPresenter *iface, enum CoreInputDeviceTypes value)
{
    if (value & ~(CoreInputDeviceTypes_Touch | CoreInputDeviceTypes_Pen | CoreInputDeviceTypes_Mouse))
        return E_INVALIDARG;
    InterlockedExchange(&impl_from_IInkPresenter(iface)->device_types, value);
    return S_OK;
}

static HRESULT unsupported_output(const char *method, void **value)
{
    if (!value) return E_POINTER;
    *value = NULL;
    FIXME("%s is not implemented\n", method);
    return E_NOTIMPL;
}

static HRESULT WINAPI presenter_get_UnprocessedInput(IInkPresenter *iface, IInkUnprocessedInput **value)
{
    struct ink_presenter *impl = impl_from_IInkPresenter(iface);
    if (!value) return E_POINTER;
    *value = &impl->unprocessed.IInkUnprocessedInput_iface;
    IInkUnprocessedInput_AddRef(*value);
    return S_OK;
}

static HRESULT WINAPI presenter_get_StrokeInput(IInkPresenter *iface, IInkStrokeInput **value)
{
    struct ink_presenter *impl = impl_from_IInkPresenter(iface);
    if (!value) return E_POINTER;
    *value = &impl->stroke.IInkStrokeInput_iface;
    IInkStrokeInput_AddRef(*value);
    return S_OK;
}

static HRESULT WINAPI presenter_get_InputProcessingConfiguration(IInkPresenter *iface, IInkInputProcessingConfiguration **value)
{
    if (!value) return E_POINTER;
    *value = impl_from_IInkPresenter(iface)->configuration;
    IInkInputProcessingConfiguration_AddRef(*value);
    return S_OK;
}

static HRESULT WINAPI presenter_get_StrokeContainer(IInkPresenter *iface, IInkStrokeContainer **value)
{
    return unsupported_output(__func__, (void **)value);
}

static HRESULT WINAPI presenter_put_StrokeContainer(IInkPresenter *iface, IInkStrokeContainer *value)
{
    FIXME("stroke storage is not implemented\n");
    return E_NOTIMPL;
}

static HRESULT WINAPI presenter_CopyDefaultDrawingAttributes(IInkPresenter *iface, IInkDrawingAttributes **value)
{
    struct ink_presenter *impl = impl_from_IInkPresenter(iface);
    IInkDrawingAttributes *snapshot;
    HRESULT hr;

    if (!value) return E_POINTER;
    *value = NULL;
    EnterCriticalSection(&impl->lock);
    snapshot = impl->attributes;
    IInkDrawingAttributes_AddRef(snapshot);
    LeaveCriticalSection(&impl->lock);
    hr = ink_drawing_attributes_clone(snapshot, value);
    IInkDrawingAttributes_Release(snapshot);
    return hr;
}

static HRESULT WINAPI presenter_UpdateDefaultDrawingAttributes(IInkPresenter *iface, IInkDrawingAttributes *value)
{
    struct ink_presenter *impl = impl_from_IInkPresenter(iface);
    IInkDrawingAttributes *copy, *old;
    HRESULT hr;

    if (FAILED(hr = ink_drawing_attributes_clone(value, &copy))) return hr;
    EnterCriticalSection(&impl->lock);
    old = impl->attributes;
    impl->attributes = copy;
    LeaveCriticalSection(&impl->lock);
    IInkDrawingAttributes_Release(old);
    return S_OK;
}

static HRESULT WINAPI presenter_ActivateCustomDrying(IInkPresenter *iface, IInkSynchronizer **value)
{
    if (!value) return E_POINTER;
    *value = impl_from_IInkPresenter(iface)->synchronizer;
    IInkSynchronizer_AddRef(*value);
    return S_OK;
}

static HRESULT WINAPI presenter_SetPredefinedConfiguration(IInkPresenter *iface, enum InkPresenterPredefinedConfiguration value)
{
    FIXME("predefined configuration %u is not implemented\n", value);
    return E_NOTIMPL;
}

#define PRESENTER_EVENT(name, handler_type, event_id) \
static HRESULT WINAPI presenter_add_##name(IInkPresenter *iface, handler_type *handler, EventRegistrationToken *token) \
{ \
    return ink_event_add(&impl_from_IInkPresenter(iface)->events, event_id, (IUnknown *)handler, token); \
} \
static HRESULT WINAPI presenter_remove_##name(IInkPresenter *iface, EventRegistrationToken token) \
{ \
    return ink_event_remove(&impl_from_IInkPresenter(iface)->events, event_id, token); \
}

PRESENTER_EVENT(StrokesCollected, ITypedEventHandler_InkPresenter_InkStrokesCollectedEventArgs, 0)
PRESENTER_EVENT(StrokesErased, ITypedEventHandler_InkPresenter_InkStrokesErasedEventArgs, 1)

static const IInkPresenterVtbl presenter_vtbl =
{
    presenter_QueryInterface, presenter_AddRef, presenter_Release,
    presenter_GetIids, presenter_GetRuntimeClassName, presenter_GetTrustLevel,
    presenter_get_IsInputEnabled, presenter_put_IsInputEnabled,
    presenter_get_InputDeviceTypes, presenter_put_InputDeviceTypes,
    presenter_get_UnprocessedInput, presenter_get_StrokeInput,
    presenter_get_InputProcessingConfiguration, presenter_get_StrokeContainer, presenter_put_StrokeContainer,
    presenter_CopyDefaultDrawingAttributes, presenter_UpdateDefaultDrawingAttributes,
    presenter_ActivateCustomDrying, presenter_SetPredefinedConfiguration,
    presenter_add_StrokesCollected, presenter_remove_StrokesCollected,
    presenter_add_StrokesErased, presenter_remove_StrokesErased,
};

#define INPUT_INSPECTABLE(prefix, type, impl_type, class_name) \
static impl_type *prefix##_impl(type *iface) \
{ return CONTAINING_RECORD(iface, impl_type, type##_iface); } \
static HRESULT WINAPI prefix##_QueryInterface(type *iface, REFIID iid, void **out) \
{ \
    impl_type *impl = prefix##_impl(iface); \
    if (!out) return E_POINTER; \
    *out = NULL; \
    if (IsEqualGUID(iid, &IID_IUnknown) || IsEqualGUID(iid, &IID_IInspectable) \
            || IsEqualGUID(iid, &IID_IAgileObject) || IsEqualGUID(iid, &IID_##type)) \
    { \
        *out = iface; \
        IInkPresenter_AddRef(&impl->owner->IInkPresenter_iface); \
        return S_OK; \
    } \
    if (IsEqualGUID(iid, &IID_IMarshal)) return IUnknown_QueryInterface(impl->marshaler, iid, out); \
    return E_NOINTERFACE; \
} \
static ULONG WINAPI prefix##_AddRef(type *iface) \
{ return IInkPresenter_AddRef(&prefix##_impl(iface)->owner->IInkPresenter_iface); } \
static ULONG WINAPI prefix##_Release(type *iface) \
{ return IInkPresenter_Release(&prefix##_impl(iface)->owner->IInkPresenter_iface); } \
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
    const WCHAR *class = class_name; \
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
    *value = &prefix##_impl(iface)->owner->IInkPresenter_iface; \
    IInkPresenter_AddRef(*value); \
    return S_OK; \
}

INPUT_INSPECTABLE(stroke, IInkStrokeInput, struct stroke_input, RuntimeClass_Windows_UI_Input_Inking_InkStrokeInput)
INPUT_INSPECTABLE(unprocessed, IInkUnprocessedInput, struct unprocessed_input, RuntimeClass_Windows_UI_Input_Inking_InkUnprocessedInput)

#define INPUT_EVENT(prefix, type, handler_type, name, id) \
static HRESULT WINAPI prefix##_add_##name(type *iface, handler_type *handler, EventRegistrationToken *token) \
{ return ink_event_add(&prefix##_impl(iface)->owner->events, id, (IUnknown *)handler, token); } \
static HRESULT WINAPI prefix##_remove_##name(type *iface, EventRegistrationToken token) \
{ return ink_event_remove(&prefix##_impl(iface)->owner->events, id, token); }

INPUT_EVENT(stroke, IInkStrokeInput, ITypedEventHandler_InkStrokeInput_PointerEventArgs, StrokeStarted, INK_STROKE_STARTED)
INPUT_EVENT(stroke, IInkStrokeInput, ITypedEventHandler_InkStrokeInput_PointerEventArgs, StrokeContinued, INK_STROKE_CONTINUED)
INPUT_EVENT(stroke, IInkStrokeInput, ITypedEventHandler_InkStrokeInput_PointerEventArgs, StrokeEnded, INK_STROKE_ENDED)
INPUT_EVENT(stroke, IInkStrokeInput, ITypedEventHandler_InkStrokeInput_PointerEventArgs, StrokeCanceled, INK_STROKE_CANCELED)
INPUT_EVENT(unprocessed, IInkUnprocessedInput, ITypedEventHandler_InkUnprocessedInput_PointerEventArgs, PointerEntered, INK_POINTER_ENTERED)
INPUT_EVENT(unprocessed, IInkUnprocessedInput, ITypedEventHandler_InkUnprocessedInput_PointerEventArgs, PointerHovered, INK_POINTER_HOVERED)
INPUT_EVENT(unprocessed, IInkUnprocessedInput, ITypedEventHandler_InkUnprocessedInput_PointerEventArgs, PointerExited, INK_POINTER_EXITED)
INPUT_EVENT(unprocessed, IInkUnprocessedInput, ITypedEventHandler_InkUnprocessedInput_PointerEventArgs, PointerPressed, INK_POINTER_PRESSED)
INPUT_EVENT(unprocessed, IInkUnprocessedInput, ITypedEventHandler_InkUnprocessedInput_PointerEventArgs, PointerMoved, INK_POINTER_MOVED)
INPUT_EVENT(unprocessed, IInkUnprocessedInput, ITypedEventHandler_InkUnprocessedInput_PointerEventArgs, PointerReleased, INK_POINTER_RELEASED)
INPUT_EVENT(unprocessed, IInkUnprocessedInput, ITypedEventHandler_InkUnprocessedInput_PointerEventArgs, PointerLost, INK_POINTER_LOST)

static const IInkStrokeInputVtbl stroke_vtbl =
{
    stroke_QueryInterface, stroke_AddRef, stroke_Release,
    stroke_GetIids, stroke_GetRuntimeClassName, stroke_GetTrustLevel,
    stroke_add_StrokeStarted, stroke_remove_StrokeStarted,
    stroke_add_StrokeContinued, stroke_remove_StrokeContinued,
    stroke_add_StrokeEnded, stroke_remove_StrokeEnded,
    stroke_add_StrokeCanceled, stroke_remove_StrokeCanceled, stroke_get_InkPresenter,
};

static const IInkUnprocessedInputVtbl unprocessed_vtbl =
{
    unprocessed_QueryInterface, unprocessed_AddRef, unprocessed_Release,
    unprocessed_GetIids, unprocessed_GetRuntimeClassName, unprocessed_GetTrustLevel,
    unprocessed_add_PointerEntered, unprocessed_remove_PointerEntered,
    unprocessed_add_PointerHovered, unprocessed_remove_PointerHovered,
    unprocessed_add_PointerExited, unprocessed_remove_PointerExited,
    unprocessed_add_PointerPressed, unprocessed_remove_PointerPressed,
    unprocessed_add_PointerMoved, unprocessed_remove_PointerMoved,
    unprocessed_add_PointerReleased, unprocessed_remove_PointerReleased,
    unprocessed_add_PointerLost, unprocessed_remove_PointerLost, unprocessed_get_InkPresenter,
};

static HRESULT invoke_stroke(IUnknown *handler, void *sender, void *args)
{
    return ITypedEventHandler_InkStrokeInput_PointerEventArgs_Invoke(
            (ITypedEventHandler_InkStrokeInput_PointerEventArgs *)handler, sender, args);
}

static HRESULT invoke_unprocessed(IUnknown *handler, void *sender, void *args)
{
    return ITypedEventHandler_InkUnprocessedInput_PointerEventArgs_Invoke(
            (ITypedEventHandler_InkUnprocessedInput_PointerEventArgs *)handler, sender, args);
}

HRESULT ink_presenter_notify_pointer(IInkPresenter *iface, enum ink_pointer_event id, IPointerEventArgs *args)
{
    struct ink_presenter *impl = impl_from_IInkPresenter(iface);
    HRESULT hr;

    IInkPresenter_AddRef(iface);
    if (id >= INK_STROKE_STARTED && id <= INK_STROKE_CANCELED)
        hr = ink_event_notify(&impl->events, id, &impl->stroke.IInkStrokeInput_iface, args, invoke_stroke);
    else if (id >= INK_POINTER_ENTERED && id <= INK_POINTER_LOST)
        hr = ink_event_notify(&impl->events, id, &impl->unprocessed.IInkUnprocessedInput_iface, args, invoke_unprocessed);
    else hr = E_INVALIDARG;
    IInkPresenter_Release(iface);
    return hr;
}

HRESULT ink_presenter_create(IInkPresenter **out)
{
    struct ink_presenter *impl;
    HRESULT hr;

    if (!out) return E_POINTER;
    *out = NULL;
    if (!(impl = calloc(1, sizeof(*impl)))) return E_OUTOFMEMORY;
    impl->IInkPresenter_iface.lpVtbl = &presenter_vtbl;
    impl->ref = 1;
    impl->input_enabled = TRUE;
    impl->device_types = CoreInputDeviceTypes_Pen;
    InitializeCriticalSection(&impl->lock);
    ink_events_init(&impl->events);
    impl->stroke.IInkStrokeInput_iface.lpVtbl = &stroke_vtbl;
    impl->stroke.owner = impl;
    impl->unprocessed.IInkUnprocessedInput_iface.lpVtbl = &unprocessed_vtbl;
    impl->unprocessed.owner = impl;
    InterlockedIncrement(&ink_object_count);
    hr = CoCreateFreeThreadedMarshaler((IUnknown *)&impl->IInkPresenter_iface, &impl->marshaler);
    if (SUCCEEDED(hr)) hr = ink_input_configuration_create(&impl->configuration);
    if (SUCCEEDED(hr)) hr = ink_drawing_attributes_create(&impl->attributes);
    if (SUCCEEDED(hr)) hr = ink_synchronizer_create(&impl->synchronizer);
    if (SUCCEEDED(hr)) hr = CoCreateFreeThreadedMarshaler((IUnknown *)&impl->stroke.IInkStrokeInput_iface, &impl->stroke.marshaler);
    if (SUCCEEDED(hr)) hr = CoCreateFreeThreadedMarshaler((IUnknown *)&impl->unprocessed.IInkUnprocessedInput_iface, &impl->unprocessed.marshaler);
    if (FAILED(hr)) IInkPresenter_Release(&impl->IInkPresenter_iface);
    else *out = &impl->IInkPresenter_iface;
    return hr;
}
