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

struct stroke_view
{
    IVectorView_InkStroke IVectorView_InkStroke_iface;
    IIterable_InkStroke IIterable_InkStroke_iface;
    LONG ref;
    IUnknown *marshaler;
    UINT32 count;
    IInkStroke **items;
};

struct stroke_iterator
{
    IIterator_InkStroke IIterator_InkStroke_iface;
    LONG ref;
    IUnknown *marshaler;
    CRITICAL_SECTION lock;
    struct stroke_view *view;
    UINT32 index;
};

static struct stroke_view *impl_from_IVectorView_InkStroke(IVectorView_InkStroke *iface)
{
    return CONTAINING_RECORD(iface, struct stroke_view, IVectorView_InkStroke_iface);
}

static HRESULT WINAPI view_QueryInterface(IVectorView_InkStroke *iface, REFIID iid, void **out)
{
    struct stroke_view *view = impl_from_IVectorView_InkStroke(iface);

    if (!out) return E_POINTER;
    *out = NULL;
    if (IsEqualGUID(iid, &IID_IUnknown) || IsEqualGUID(iid, &IID_IInspectable)
            || IsEqualGUID(iid, &IID_IAgileObject) || IsEqualGUID(iid, &IID_IVectorView_InkStroke)) *out = iface;
    else if (IsEqualGUID(iid, &IID_IIterable_InkStroke)) *out = &view->IIterable_InkStroke_iface;
    else if (IsEqualGUID(iid, &IID_IMarshal)) return IUnknown_QueryInterface(view->marshaler, iid, out);
    else return E_NOINTERFACE;
    IVectorView_InkStroke_AddRef(iface);
    return S_OK;
}

static ULONG WINAPI view_AddRef(IVectorView_InkStroke *iface)
{
    return InterlockedIncrement(&impl_from_IVectorView_InkStroke(iface)->ref);
}

static ULONG WINAPI view_Release(IVectorView_InkStroke *iface)
{
    struct stroke_view *view = impl_from_IVectorView_InkStroke(iface);
    ULONG ref = InterlockedDecrement(&view->ref);
    UINT32 i;

    if (!ref)
    {
        for (i = 0; i < view->count; ++i) IInkStroke_Release(view->items[i]);
        if (view->marshaler) IUnknown_Release(view->marshaler);
        free(view->items);
        free(view);
        InterlockedDecrement(&ink_object_count);
    }
    return ref;
}

static HRESULT WINAPI view_GetIids(IVectorView_InkStroke *iface, ULONG *count, IID **iids)
{
    if (!count || !iids) return E_POINTER;
    *count = 0;
    *iids = NULL;
    if (!(*iids = CoTaskMemAlloc(2 * sizeof(**iids)))) return E_OUTOFMEMORY;
    (*iids)[0] = IID_IVectorView_InkStroke;
    (*iids)[1] = IID_IIterable_InkStroke;
    *count = 2;
    return S_OK;
}

static HRESULT WINAPI view_GetRuntimeClassName(IVectorView_InkStroke *iface, HSTRING *name)
{
    const WCHAR *class = L"Windows.Foundation.Collections.IVectorView`1<Windows.UI.Input.Inking.InkStroke>";
    if (!name) return E_POINTER;
    return WindowsCreateString(class, wcslen(class), name);
}

static HRESULT WINAPI view_GetTrustLevel(IVectorView_InkStroke *iface, TrustLevel *level)
{
    if (!level) return E_POINTER;
    *level = BaseTrust;
    return S_OK;
}

static HRESULT WINAPI view_GetAt(IVectorView_InkStroke *iface, UINT32 index, IInkStroke **out)
{
    struct stroke_view *view = impl_from_IVectorView_InkStroke(iface);

    if (!out) return E_POINTER;
    *out = NULL;
    if (index >= view->count) return E_BOUNDS;
    *out = view->items[index];
    IInkStroke_AddRef(*out);
    return S_OK;
}

static HRESULT WINAPI view_get_Size(IVectorView_InkStroke *iface, UINT32 *value)
{
    if (!value) return E_POINTER;
    *value = impl_from_IVectorView_InkStroke(iface)->count;
    return S_OK;
}

static HRESULT WINAPI view_IndexOf(IVectorView_InkStroke *iface, IInkStroke *item, UINT32 *index, boolean *found)
{
    struct stroke_view *view = impl_from_IVectorView_InkStroke(iface);
    IUnknown *identity, *current;
    HRESULT hr;
    UINT32 i;

    if (!index || !found) return E_POINTER;
    *index = 0;
    *found = FALSE;
    if (!item) return S_OK;
    IVectorView_InkStroke_AddRef(iface);
    hr = IInkStroke_QueryInterface(item, &IID_IUnknown, (void **)&identity);
    if (SUCCEEDED(hr))
    {
        for (i = 0; i < view->count; ++i)
        {
            hr = IInkStroke_QueryInterface(view->items[i], &IID_IUnknown, (void **)&current);
            if (FAILED(hr)) break;
            *found = current == identity;
            IUnknown_Release(current);
            if (*found)
            {
                *index = i;
                break;
            }
        }
        IUnknown_Release(identity);
    }
    IVectorView_InkStroke_Release(iface);
    return hr;
}

static HRESULT WINAPI view_GetMany(IVectorView_InkStroke *iface, UINT32 start, UINT32 capacity, IInkStroke **items, UINT32 *count)
{
    struct stroke_view *view = impl_from_IVectorView_InkStroke(iface);
    UINT32 i, actual;

    if (!count || (capacity && !items)) return E_POINTER;
    *count = 0;
    if (start > view->count) return E_BOUNDS;
    IVectorView_InkStroke_AddRef(iface);
    actual = min(capacity, view->count - start);
    *count = actual;
    for (i = 0; i < actual; ++i)
    {
        items[i] = view->items[start + i];
        IInkStroke_AddRef(items[i]);
    }
    IVectorView_InkStroke_Release(iface);
    return S_OK;
}

static const IVectorView_InkStrokeVtbl view_vtbl =
{
    view_QueryInterface, view_AddRef, view_Release, view_GetIids, view_GetRuntimeClassName, view_GetTrustLevel,
    view_GetAt, view_get_Size, view_IndexOf, view_GetMany,
};

static struct stroke_iterator *impl_from_IIterator_InkStroke(IIterator_InkStroke *iface)
{
    return CONTAINING_RECORD(iface, struct stroke_iterator, IIterator_InkStroke_iface);
}

static HRESULT WINAPI iterator_QueryInterface(IIterator_InkStroke *iface, REFIID iid, void **out)
{
    struct stroke_iterator *iterator = impl_from_IIterator_InkStroke(iface);

    if (!out) return E_POINTER;
    *out = NULL;
    if (IsEqualGUID(iid, &IID_IUnknown) || IsEqualGUID(iid, &IID_IInspectable)
            || IsEqualGUID(iid, &IID_IAgileObject) || IsEqualGUID(iid, &IID_IIterator_InkStroke))
    {
        *out = iface;
        IIterator_InkStroke_AddRef(iface);
        return S_OK;
    }
    if (IsEqualGUID(iid, &IID_IMarshal)) return IUnknown_QueryInterface(iterator->marshaler, iid, out);
    return E_NOINTERFACE;
}

static ULONG WINAPI iterator_AddRef(IIterator_InkStroke *iface)
{
    return InterlockedIncrement(&impl_from_IIterator_InkStroke(iface)->ref);
}

static ULONG WINAPI iterator_Release(IIterator_InkStroke *iface)
{
    struct stroke_iterator *iterator = impl_from_IIterator_InkStroke(iface);
    ULONG ref = InterlockedDecrement(&iterator->ref);

    if (!ref)
    {
        IVectorView_InkStroke_Release(&iterator->view->IVectorView_InkStroke_iface);
        if (iterator->marshaler) IUnknown_Release(iterator->marshaler);
        DeleteCriticalSection(&iterator->lock);
        free(iterator);
        InterlockedDecrement(&ink_object_count);
    }
    return ref;
}

static HRESULT WINAPI iterator_GetIids(IIterator_InkStroke *iface, ULONG *count, IID **iids)
{
    if (!count || !iids) return E_POINTER;
    *count = 0;
    *iids = NULL;
    if (!(*iids = CoTaskMemAlloc(sizeof(**iids)))) return E_OUTOFMEMORY;
    **iids = IID_IIterator_InkStroke;
    *count = 1;
    return S_OK;
}

static HRESULT WINAPI iterator_GetRuntimeClassName(IIterator_InkStroke *iface, HSTRING *name)
{
    const WCHAR *class = L"Windows.Foundation.Collections.IIterator`1<Windows.UI.Input.Inking.InkStroke>";
    if (!name) return E_POINTER;
    return WindowsCreateString(class, wcslen(class), name);
}

static HRESULT WINAPI iterator_GetTrustLevel(IIterator_InkStroke *iface, TrustLevel *level)
{
    if (!level) return E_POINTER;
    *level = BaseTrust;
    return S_OK;
}

static HRESULT WINAPI iterator_get_Current(IIterator_InkStroke *iface, IInkStroke **out)
{
    struct stroke_iterator *iterator = impl_from_IIterator_InkStroke(iface);
    UINT32 index;
    HRESULT hr;

    IIterator_InkStroke_AddRef(iface);
    EnterCriticalSection(&iterator->lock);
    index = iterator->index;
    LeaveCriticalSection(&iterator->lock);
    hr = IVectorView_InkStroke_GetAt(&iterator->view->IVectorView_InkStroke_iface, index, out);
    IIterator_InkStroke_Release(iface);
    return hr;
}

static HRESULT WINAPI iterator_get_HasCurrent(IIterator_InkStroke *iface, boolean *value)
{
    struct stroke_iterator *iterator = impl_from_IIterator_InkStroke(iface);
    if (!value) return E_POINTER;
    EnterCriticalSection(&iterator->lock);
    *value = iterator->index < iterator->view->count;
    LeaveCriticalSection(&iterator->lock);
    return S_OK;
}

static HRESULT WINAPI iterator_MoveNext(IIterator_InkStroke *iface, boolean *value)
{
    struct stroke_iterator *iterator = impl_from_IIterator_InkStroke(iface);
    if (!value) return E_POINTER;
    EnterCriticalSection(&iterator->lock);
    if (iterator->index < iterator->view->count) ++iterator->index;
    *value = iterator->index < iterator->view->count;
    LeaveCriticalSection(&iterator->lock);
    return S_OK;
}

static HRESULT WINAPI iterator_GetMany(IIterator_InkStroke *iface, UINT32 capacity, IInkStroke **items, UINT32 *count)
{
    struct stroke_iterator *iterator = impl_from_IIterator_InkStroke(iface);
    UINT32 start;
    HRESULT hr;

    if (!count || (capacity && !items)) return E_POINTER;
    IIterator_InkStroke_AddRef(iface);
    EnterCriticalSection(&iterator->lock);
    start = iterator->index;
    iterator->index += min(capacity, iterator->view->count - start);
    LeaveCriticalSection(&iterator->lock);
    hr = IVectorView_InkStroke_GetMany(&iterator->view->IVectorView_InkStroke_iface, start, capacity, items, count);
    IIterator_InkStroke_Release(iface);
    return hr;
}

static const IIterator_InkStrokeVtbl iterator_vtbl =
{
    iterator_QueryInterface, iterator_AddRef, iterator_Release,
    iterator_GetIids, iterator_GetRuntimeClassName, iterator_GetTrustLevel,
    iterator_get_Current, iterator_get_HasCurrent, iterator_MoveNext, iterator_GetMany,
};

DEFINE_IINSPECTABLE(iterable, IIterable_InkStroke, struct stroke_view, IVectorView_InkStroke_iface)

static HRESULT WINAPI iterable_First(IIterable_InkStroke *iface, IIterator_InkStroke **out)
{
    struct stroke_view *view = impl_from_IIterable_InkStroke(iface);
    struct stroke_iterator *iterator;
    HRESULT hr;

    if (!out) return E_POINTER;
    *out = NULL;
    if (!(iterator = calloc(1, sizeof(*iterator)))) return E_OUTOFMEMORY;
    iterator->IIterator_InkStroke_iface.lpVtbl = &iterator_vtbl;
    iterator->ref = 1;
    iterator->view = view;
    IVectorView_InkStroke_AddRef(&view->IVectorView_InkStroke_iface);
    InitializeCriticalSection(&iterator->lock);
    InterlockedIncrement(&ink_object_count);
    hr = CoCreateFreeThreadedMarshaler((IUnknown *)&iterator->IIterator_InkStroke_iface, &iterator->marshaler);
    if (FAILED(hr)) IIterator_InkStroke_Release(&iterator->IIterator_InkStroke_iface);
    else *out = &iterator->IIterator_InkStroke_iface;
    return hr;
}

static const IIterable_InkStrokeVtbl iterable_vtbl =
{
    iterable_QueryInterface, iterable_AddRef, iterable_Release,
    iterable_GetIids, iterable_GetRuntimeClassName, iterable_GetTrustLevel, iterable_First,
};

HRESULT ink_stroke_view_take(struct list *strokes, IVectorView_InkStroke **out)
{
    struct stroke_view *view;
    struct ink_pending_stroke *stroke, *next;
    UINT32 count, i = 0;
    HRESULT hr;

    if (!out) return E_POINTER;
    *out = NULL;
    count = list_count(strokes);
    if (!(view = calloc(1, sizeof(*view)))) return E_OUTOFMEMORY;
    if (count && !(view->items = calloc(count, sizeof(*view->items))))
    {
        free(view);
        return E_OUTOFMEMORY;
    }
    view->IVectorView_InkStroke_iface.lpVtbl = &view_vtbl;
    view->IIterable_InkStroke_iface.lpVtbl = &iterable_vtbl;
    view->ref = 1;
    InterlockedIncrement(&ink_object_count);
    hr = CoCreateFreeThreadedMarshaler((IUnknown *)&view->IVectorView_InkStroke_iface, &view->marshaler);
    if (FAILED(hr))
    {
        IVectorView_InkStroke_Release(&view->IVectorView_InkStroke_iface);
        return hr;
    }
    LIST_FOR_EACH_ENTRY_SAFE(stroke, next, strokes, struct ink_pending_stroke, entry)
    {
        list_remove(&stroke->entry);
        view->items[i++] = stroke->stroke;
        free(stroke);
    }
    view->count = count;
    *out = &view->IVectorView_InkStroke_iface;
    return S_OK;
}
