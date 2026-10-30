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

struct ink_synchronizer
{
    IInkSynchronizer IInkSynchronizer_iface;
    LONG ref;
    IUnknown *marshaler;
    CRITICAL_SECTION lock;
    struct list pending;
    IVectorView_InkStroke *batch;
    enum { DRY_IDLE, DRY_PREPARING, DRY_ACTIVE } phase;
};

static struct ink_synchronizer *impl_from_IInkSynchronizer(IInkSynchronizer *iface)
{
    return CONTAINING_RECORD(iface, struct ink_synchronizer, IInkSynchronizer_iface);
}

static HRESULT WINAPI synchronizer_QueryInterface(IInkSynchronizer *iface, REFIID iid, void **out)
{
    struct ink_synchronizer *impl = impl_from_IInkSynchronizer(iface);
    if (!out) return E_POINTER;
    *out = NULL;
    if (IsEqualGUID(iid, &IID_IUnknown) || IsEqualGUID(iid, &IID_IInspectable)
            || IsEqualGUID(iid, &IID_IAgileObject) || IsEqualGUID(iid, &IID_IInkSynchronizer))
    {
        *out = iface;
        IInkSynchronizer_AddRef(iface);
        return S_OK;
    }
    if (IsEqualGUID(iid, &IID_IMarshal)) return IUnknown_QueryInterface(impl->marshaler, iid, out);
    return E_NOINTERFACE;
}

static ULONG WINAPI synchronizer_AddRef(IInkSynchronizer *iface)
{
    return InterlockedIncrement(&impl_from_IInkSynchronizer(iface)->ref);
}

static ULONG WINAPI synchronizer_Release(IInkSynchronizer *iface)
{
    struct ink_synchronizer *impl = impl_from_IInkSynchronizer(iface);
    ULONG ref = InterlockedDecrement(&impl->ref);
    struct ink_pending_stroke *stroke, *next;

    if (!ref)
    {
        LIST_FOR_EACH_ENTRY_SAFE(stroke, next, &impl->pending, struct ink_pending_stroke, entry)
        {
            list_remove(&stroke->entry);
            IInkStroke_Release(stroke->stroke);
            free(stroke);
        }
        if (impl->batch) IVectorView_InkStroke_Release(impl->batch);
        if (impl->marshaler) IUnknown_Release(impl->marshaler);
        DeleteCriticalSection(&impl->lock);
        free(impl);
        InterlockedDecrement(&ink_object_count);
    }
    return ref;
}

static HRESULT WINAPI synchronizer_GetIids(IInkSynchronizer *iface, ULONG *count, IID **iids)
{
    if (!count || !iids) return E_POINTER;
    *count = 0;
    *iids = NULL;
    if (!(*iids = CoTaskMemAlloc(sizeof(**iids)))) return E_OUTOFMEMORY;
    **iids = IID_IInkSynchronizer;
    *count = 1;
    return S_OK;
}

static HRESULT WINAPI synchronizer_GetRuntimeClassName(IInkSynchronizer *iface, HSTRING *name)
{
    const WCHAR *class = RuntimeClass_Windows_UI_Input_Inking_InkSynchronizer;
    if (!name) return E_POINTER;
    return WindowsCreateString(class, wcslen(class), name);
}

static HRESULT WINAPI synchronizer_GetTrustLevel(IInkSynchronizer *iface, TrustLevel *level)
{
    if (!level) return E_POINTER;
    *level = BaseTrust;
    return S_OK;
}

static HRESULT WINAPI synchronizer_BeginDry(IInkSynchronizer *iface, IVectorView_InkStroke **out)
{
    struct ink_synchronizer *impl = impl_from_IInkSynchronizer(iface);
    struct list batch = LIST_INIT(batch);
    IVectorView_InkStroke *view;
    HRESULT hr;

    if (!out) return E_POINTER;
    *out = NULL;
    EnterCriticalSection(&impl->lock);
    if (impl->phase != DRY_IDLE)
    {
        LeaveCriticalSection(&impl->lock);
        return E_ILLEGAL_METHOD_CALL;
    }
    impl->phase = DRY_PREPARING;
    list_move_tail(&batch, &impl->pending);
    LeaveCriticalSection(&impl->lock);
    hr = ink_stroke_view_take(&batch, &view);
    EnterCriticalSection(&impl->lock);
    if (FAILED(hr))
    {
        list_move_head(&impl->pending, &batch);
        impl->phase = DRY_IDLE;
    }
    else
    {
        impl->batch = view;
        impl->phase = DRY_ACTIVE;
        IVectorView_InkStroke_AddRef(view);
        *out = view;
    }
    LeaveCriticalSection(&impl->lock);
    return hr;
}

static HRESULT WINAPI synchronizer_EndDry(IInkSynchronizer *iface)
{
    struct ink_synchronizer *impl = impl_from_IInkSynchronizer(iface);
    IVectorView_InkStroke *batch;

    EnterCriticalSection(&impl->lock);
    if (impl->phase == DRY_PREPARING)
    {
        LeaveCriticalSection(&impl->lock);
        return E_ILLEGAL_METHOD_CALL;
    }
    batch = impl->batch;
    impl->batch = NULL;
    impl->phase = DRY_IDLE;
    LeaveCriticalSection(&impl->lock);
    if (batch) IVectorView_InkStroke_Release(batch);
    return S_OK;
}

static const IInkSynchronizerVtbl synchronizer_vtbl =
{
    synchronizer_QueryInterface, synchronizer_AddRef, synchronizer_Release,
    synchronizer_GetIids, synchronizer_GetRuntimeClassName, synchronizer_GetTrustLevel,
    synchronizer_BeginDry, synchronizer_EndDry,
};

HRESULT ink_synchronizer_create(IInkSynchronizer **out)
{
    struct ink_synchronizer *impl;
    HRESULT hr;

    if (!out) return E_POINTER;
    *out = NULL;
    if (!(impl = calloc(1, sizeof(*impl)))) return E_OUTOFMEMORY;
    impl->IInkSynchronizer_iface.lpVtbl = &synchronizer_vtbl;
    impl->ref = 1;
    InitializeCriticalSection(&impl->lock);
    list_init(&impl->pending);
    InterlockedIncrement(&ink_object_count);
    hr = CoCreateFreeThreadedMarshaler((IUnknown *)&impl->IInkSynchronizer_iface, &impl->marshaler);
    if (FAILED(hr)) IInkSynchronizer_Release(&impl->IInkSynchronizer_iface);
    else *out = &impl->IInkSynchronizer_iface;
    return hr;
}

HRESULT ink_synchronizer_queue(IInkSynchronizer *iface, IInkStroke *stroke)
{
    struct ink_synchronizer *impl;
    struct ink_pending_stroke *pending;
    HRESULT hr;

    if (!iface || !stroke) return E_POINTER;
    if (iface->lpVtbl != &synchronizer_vtbl) return E_INVALIDARG;
    impl = impl_from_IInkSynchronizer(iface);
    if (!(pending = malloc(sizeof(*pending)))) return E_OUTOFMEMORY;
    hr = IInkStroke_QueryInterface(stroke, &IID_IInkStroke, (void **)&pending->stroke);
    if (FAILED(hr))
    {
        free(pending);
        return hr;
    }
    EnterCriticalSection(&impl->lock);
    list_add_tail(&impl->pending, &pending->entry);
    LeaveCriticalSection(&impl->lock);
    return S_OK;
}
