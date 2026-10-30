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

struct root_reference
{
    LONG ref;
    IContainerVisual *visual;
};

struct core_host
{
    ICoreInkPresenterHost ICoreInkPresenterHost_iface;
    LONG ref;
    IUnknown *marshaler;
    IInkPresenter *presenter;
    CRITICAL_SECTION lock;
    struct root_reference *root;
};

static struct core_host *impl_from_ICoreInkPresenterHost(ICoreInkPresenterHost *iface)
{
    return CONTAINING_RECORD(iface, struct core_host, ICoreInkPresenterHost_iface);
}

static void root_release(struct root_reference *root)
{
    if (root && !InterlockedDecrement(&root->ref))
    {
        IContainerVisual_Release(root->visual);
        free(root);
    }
}

static HRESULT WINAPI host_QueryInterface(ICoreInkPresenterHost *iface, REFIID iid, void **out)
{
    struct core_host *impl = impl_from_ICoreInkPresenterHost(iface);

    if (!out) return E_POINTER;
    *out = NULL;
    if (IsEqualGUID(iid, &IID_IUnknown) || IsEqualGUID(iid, &IID_IInspectable)
            || IsEqualGUID(iid, &IID_IAgileObject) || IsEqualGUID(iid, &IID_ICoreInkPresenterHost))
    {
        *out = iface;
        ICoreInkPresenterHost_AddRef(iface);
        return S_OK;
    }
    if (IsEqualGUID(iid, &IID_IMarshal)) return IUnknown_QueryInterface(impl->marshaler, iid, out);
    return E_NOINTERFACE;
}

static ULONG WINAPI host_AddRef(ICoreInkPresenterHost *iface)
{
    return InterlockedIncrement(&impl_from_ICoreInkPresenterHost(iface)->ref);
}

static ULONG WINAPI host_Release(ICoreInkPresenterHost *iface)
{
    struct core_host *impl = impl_from_ICoreInkPresenterHost(iface);
    ULONG ref = InterlockedDecrement(&impl->ref);

    if (!ref)
    {
        struct root_reference *root = impl->root;
        impl->root = NULL;
        root_release(root);
        if (impl->presenter) IInkPresenter_Release(impl->presenter);
        if (impl->marshaler) IUnknown_Release(impl->marshaler);
        DeleteCriticalSection(&impl->lock);
        free(impl);
        InterlockedDecrement(&ink_object_count);
    }
    return ref;
}

static HRESULT WINAPI host_GetIids(ICoreInkPresenterHost *iface, ULONG *count, IID **iids)
{
    if (!count || !iids) return E_POINTER;
    *count = 0;
    *iids = NULL;
    if (!(*iids = CoTaskMemAlloc(sizeof(**iids)))) return E_OUTOFMEMORY;
    **iids = IID_ICoreInkPresenterHost;
    *count = 1;
    return S_OK;
}

static HRESULT WINAPI host_GetRuntimeClassName(ICoreInkPresenterHost *iface, HSTRING *name)
{
    const WCHAR *class = RuntimeClass_Windows_UI_Input_Inking_Core_CoreInkPresenterHost;
    if (!name) return E_POINTER;
    return WindowsCreateString(class, wcslen(class), name);
}

static HRESULT WINAPI host_GetTrustLevel(ICoreInkPresenterHost *iface, TrustLevel *level)
{
    if (!level) return E_POINTER;
    *level = BaseTrust;
    return S_OK;
}

static HRESULT WINAPI host_get_InkPresenter(ICoreInkPresenterHost *iface, IInkPresenter **value)
{
    if (!value) return E_POINTER;
    *value = impl_from_ICoreInkPresenterHost(iface)->presenter;
    IInkPresenter_AddRef(*value);
    return S_OK;
}

static HRESULT WINAPI host_get_RootVisual(ICoreInkPresenterHost *iface, IContainerVisual **value)
{
    struct core_host *impl = impl_from_ICoreInkPresenterHost(iface);
    struct root_reference *root;

    if (!value) return E_POINTER;
    *value = NULL;
    EnterCriticalSection(&impl->lock);
    if ((root = impl->root)) InterlockedIncrement(&root->ref);
    LeaveCriticalSection(&impl->lock);
    if (root)
    {
        *value = root->visual;
        IContainerVisual_AddRef(*value);
        root_release(root);
    }
    return S_OK;
}

static HRESULT WINAPI host_put_RootVisual(ICoreInkPresenterHost *iface, IContainerVisual *value)
{
    struct core_host *impl = impl_from_ICoreInkPresenterHost(iface);
    struct root_reference *root = NULL, *old;

    if (value)
    {
        if (!(root = malloc(sizeof(*root)))) return E_OUTOFMEMORY;
        root->ref = 1;
        root->visual = value;
        IContainerVisual_AddRef(value);
    }
    EnterCriticalSection(&impl->lock);
    old = impl->root;
    impl->root = root;
    LeaveCriticalSection(&impl->lock);
    root_release(old);
    return S_OK;
}

static const ICoreInkPresenterHostVtbl host_vtbl =
{
    host_QueryInterface, host_AddRef, host_Release,
    host_GetIids, host_GetRuntimeClassName, host_GetTrustLevel,
    host_get_InkPresenter, host_get_RootVisual, host_put_RootVisual,
};

HRESULT ink_core_host_create(IInspectable **out)
{
    struct core_host *impl;
    HRESULT hr;

    if (!out) return E_POINTER;
    *out = NULL;
    if (!(impl = calloc(1, sizeof(*impl)))) return E_OUTOFMEMORY;
    impl->ICoreInkPresenterHost_iface.lpVtbl = &host_vtbl;
    impl->ref = 1;
    InitializeCriticalSection(&impl->lock);
    InterlockedIncrement(&ink_object_count);
    hr = CoCreateFreeThreadedMarshaler((IUnknown *)&impl->ICoreInkPresenterHost_iface, &impl->marshaler);
    if (SUCCEEDED(hr)) hr = ink_presenter_create(&impl->presenter);
    if (FAILED(hr)) ICoreInkPresenterHost_Release(&impl->ICoreInkPresenterHost_iface);
    else *out = (IInspectable *)&impl->ICoreInkPresenterHost_iface;
    return hr;
}
