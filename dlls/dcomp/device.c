/*
 * Copyright 2020 Nikolay Sivov for CodeWeavers
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
#define COBJMACROS

#include <stdarg.h>

#include "windef.h"
#include "winbase.h"
#include "objidl.h"
#include "dxgi.h"
#include "wine/debug.h"
#include "wine/list.h"

WINE_DEFAULT_DEBUG_CHANNEL(dcomp);

static CRITICAL_SECTION dcomp_cs;
static CRITICAL_SECTION_DEBUG dcomp_cs_debug =
{
    0, 0, &dcomp_cs,
    { &dcomp_cs_debug.ProcessLocksList, &dcomp_cs_debug.ProcessLocksList },
      0, 0, { (DWORD_PTR)(__FILE__ ": dcomp_cs") }
};
static CRITICAL_SECTION dcomp_cs = { &dcomp_cs_debug, -1, 0, 0, 0, 0 };

struct dcomp_device;

struct dcomp_partner
{
    const void **lpVtbl;
    struct dcomp_device *device;
};

struct dcomp_device
{
    const void **lpVtbl;
    LONG refcount;
    struct dcomp_partner partner;
    struct list targets;
};

struct dcomp_target
{
    const void **lpVtbl;
    LONG refcount;
    HWND window;
    struct dcomp_device *device;
    struct dcomp_visual *root;
    struct list entry;
};

struct dcomp_visual
{
    const void **lpVtbl;
    LONG refcount;
    IUnknown *content;
    HWND content_window;
    struct dcomp_visual *parent;
    struct list children;
    struct list entry;
};

struct dcomp_scale_transform
{
    const void **lpVtbl;
    LONG refcount;
};

struct dcomp_hwnd_surface
{
    const void **lpVtbl;
    LONG refcount;
    HWND window;
};

static const GUID IID_IDCompositionDevice = {0xc37ea93a, 0xe7aa, 0x450d,
        {0xb1, 0x6f, 0x97, 0x46, 0xcb, 0x04, 0x07, 0xf3}};
static const GUID IID_IDCompositionDevice2 = {0x75f6468d, 0x1b8e, 0x447c,
        {0x9b, 0xc6, 0x75, 0xfe, 0xa8, 0x0b, 0x5b, 0x25}};
static const GUID IID_IDCompositionDevice3 = {0x0987cb06, 0xf916, 0x48bf,
        {0x8d, 0x35, 0xce, 0x76, 0x41, 0x78, 0x1b, 0xd9}};
static const GUID IID_IDCompositionDesktopDevice = {0x5f4633fe, 0x1e08, 0x4cb8,
        {0x8c, 0x75, 0xce, 0x24, 0x33, 0x3f, 0x56, 0x02}};
static const GUID IID_IDCompositionDesktopDevicePartner = {0xd14b6158, 0xc3fa, 0x4bce,
        {0x9c, 0x1f, 0xb6, 0x1d, 0x86, 0x65, 0xea, 0xb0}};
static const GUID IID_IUnknown_local = {0x00000000, 0x0000, 0x0000,
        {0xc0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46}};
static const GUID IID_IDCompositionTarget = {0xeacdd04c, 0x117e, 0x4e17,
        {0x88, 0xf4, 0xd1, 0xb1, 0x2b, 0x0e, 0x3d, 0x89}};
static const GUID IID_IDCompositionVisual = {0x4d93059d, 0x097b, 0x4651,
        {0x9a, 0x60, 0xf0, 0xf2, 0x51, 0x16, 0xe2, 0xf3}};
static const GUID IID_IDCompositionTransform = {0xfd55faa7, 0x37e0, 0x4c20,
        {0x95, 0xd2, 0x9b, 0xe4, 0x5b, 0xc3, 0x3f, 0x55}};
static const GUID IID_IDCompositionScaleTransform = {0x71fde914, 0x40ef, 0x45ef,
        {0xbd, 0x51, 0x68, 0xb0, 0x37, 0xc3, 0x39, 0xf9}};
static const GUID IID_IDXGISwapChain_local = {0x310d36a0, 0xd2e7, 0x4c0a,
        {0xaa, 0x04, 0x6a, 0x9d, 0x23, 0xb8, 0x88, 0x6a}};
static const WCHAR dxvk_dummy_window_classW[] = {'D','X','V','K','D','U','M','M','Y','W','N','D','C','L','A','S','S',0};
static const WCHAR wine_dcomp_visual_propW[] = {'_','_','w','i','n','e','_','d','c','o','m','p','_','v','i','s','u','a','l',0};
static const WCHAR wine_dcomp_target_window_propW[] = {'_','_','w','i','n','e','_','d','c','o','m','p','_','t','a','r','g','e','t','_','w','i','n','d','o','w',0};

static HRESULT STDMETHODCALLTYPE dcomp_device_query_interface(struct dcomp_device *device,
        REFIID iid, void **out);
static ULONG STDMETHODCALLTYPE dcomp_device_addref(struct dcomp_device *device);
static ULONG STDMETHODCALLTYPE dcomp_device_release(struct dcomp_device *device);
static HRESULT STDMETHODCALLTYPE dcomp_device_commit(struct dcomp_device *device);
static HRESULT STDMETHODCALLTYPE dcomp_device_wait(struct dcomp_device *device);
#define DEFINE_DCOMP_DEVICE_NOT_IMPLEMENTED(index) \
    static HRESULT STDMETHODCALLTYPE dcomp_device_not_implemented_##index(struct dcomp_device *device) \
    { \
        FIXME("device %p, method %u.\n", device, index); \
        return E_NOTIMPL; \
    }
DEFINE_DCOMP_DEVICE_NOT_IMPLEMENTED(5)
DEFINE_DCOMP_DEVICE_NOT_IMPLEMENTED(8)
DEFINE_DCOMP_DEVICE_NOT_IMPLEMENTED(9)
DEFINE_DCOMP_DEVICE_NOT_IMPLEMENTED(10)
DEFINE_DCOMP_DEVICE_NOT_IMPLEMENTED(12)
DEFINE_DCOMP_DEVICE_NOT_IMPLEMENTED(14)
DEFINE_DCOMP_DEVICE_NOT_IMPLEMENTED(15)
DEFINE_DCOMP_DEVICE_NOT_IMPLEMENTED(16)
DEFINE_DCOMP_DEVICE_NOT_IMPLEMENTED(17)
DEFINE_DCOMP_DEVICE_NOT_IMPLEMENTED(18)
DEFINE_DCOMP_DEVICE_NOT_IMPLEMENTED(19)
DEFINE_DCOMP_DEVICE_NOT_IMPLEMENTED(20)
DEFINE_DCOMP_DEVICE_NOT_IMPLEMENTED(21)
DEFINE_DCOMP_DEVICE_NOT_IMPLEMENTED(22)
DEFINE_DCOMP_DEVICE_NOT_IMPLEMENTED(23)
DEFINE_DCOMP_DEVICE_NOT_IMPLEMENTED(24)
DEFINE_DCOMP_DEVICE_NOT_IMPLEMENTED(25)
#undef DEFINE_DCOMP_DEVICE_NOT_IMPLEMENTED
static HRESULT STDMETHODCALLTYPE dcomp_device_create_target(struct dcomp_device *device,
        HWND window, BOOL topmost, struct dcomp_target **out);
static HRESULT STDMETHODCALLTYPE dcomp_device_create_visual(struct dcomp_device *device,
        struct dcomp_visual **out);
static HRESULT STDMETHODCALLTYPE dcomp_device_create_scale_transform(struct dcomp_device *device,
        struct dcomp_scale_transform **out);
static HRESULT STDMETHODCALLTYPE dcomp_device_create_surface_from_hwnd(struct dcomp_device *device,
        HWND window, struct dcomp_hwnd_surface **out);
static HRESULT STDMETHODCALLTYPE dcomp_device_check_state(struct dcomp_device *device, BOOL *valid);
static HRESULT STDMETHODCALLTYPE dcomp_partner_query_interface(struct dcomp_partner *partner,
        REFIID iid, void **out);
static ULONG STDMETHODCALLTYPE dcomp_partner_addref(struct dcomp_partner *partner);
static ULONG STDMETHODCALLTYPE dcomp_partner_release(struct dcomp_partner *partner);
static HRESULT STDMETHODCALLTYPE dcomp_partner_commit(struct dcomp_partner *partner);
static HRESULT STDMETHODCALLTYPE dcomp_partner_wait(struct dcomp_partner *partner);
static HRESULT STDMETHODCALLTYPE dcomp_partner_not_implemented(struct dcomp_partner *partner);
static HRESULT STDMETHODCALLTYPE dcomp_partner_create_visual(struct dcomp_partner *partner,
        struct dcomp_visual **out);
static HRESULT STDMETHODCALLTYPE dcomp_partner_create_target(struct dcomp_partner *partner,
        HWND window, BOOL topmost, struct dcomp_target **out);
static HRESULT STDMETHODCALLTYPE dcomp_partner_create_shared_resource(struct dcomp_partner *partner,
        REFIID iid, void **out);
static HRESULT STDMETHODCALLTYPE dcomp_partner_open_shared_resource_handle(struct dcomp_partner *partner,
        IUnknown *object, HANDLE *handle);
static HRESULT STDMETHODCALLTYPE dcomp_partner_create_from_shared_visual_handle(
        struct dcomp_partner *partner, HANDLE handle, REFIID iid, void **out);
static HRESULT STDMETHODCALLTYPE dcomp_target_query_interface(struct dcomp_target *target,
        REFIID iid, void **out);
static ULONG STDMETHODCALLTYPE dcomp_target_addref(struct dcomp_target *target);
static ULONG STDMETHODCALLTYPE dcomp_target_release(struct dcomp_target *target);
static HRESULT STDMETHODCALLTYPE dcomp_target_set_root(struct dcomp_target *target,
        struct dcomp_visual *visual);
static HRESULT STDMETHODCALLTYPE dcomp_visual_query_interface(struct dcomp_visual *visual,
        REFIID iid, void **out);
static ULONG STDMETHODCALLTYPE dcomp_visual_addref(struct dcomp_visual *visual);
static ULONG STDMETHODCALLTYPE dcomp_visual_release(struct dcomp_visual *visual);
static HRESULT STDMETHODCALLTYPE dcomp_visual_set_object(struct dcomp_visual *visual, void *object);
static HRESULT STDMETHODCALLTYPE dcomp_visual_set_float(struct dcomp_visual *visual, float value);
static HRESULT STDMETHODCALLTYPE dcomp_visual_set_matrix(struct dcomp_visual *visual, const void *matrix);
static HRESULT STDMETHODCALLTYPE dcomp_visual_set_mode(struct dcomp_visual *visual, UINT mode);
static HRESULT STDMETHODCALLTYPE dcomp_visual_set_rect(struct dcomp_visual *visual, const void *rect);
static HRESULT STDMETHODCALLTYPE dcomp_visual_set_content(struct dcomp_visual *visual, IUnknown *content);
static HRESULT STDMETHODCALLTYPE dcomp_visual_add_visual(struct dcomp_visual *visual,
        struct dcomp_visual *child, BOOL insert_above, struct dcomp_visual *reference);
static HRESULT STDMETHODCALLTYPE dcomp_visual_remove_visual(struct dcomp_visual *visual,
        struct dcomp_visual *child);
static HRESULT STDMETHODCALLTYPE dcomp_visual_remove_all(struct dcomp_visual *visual);
static HRESULT STDMETHODCALLTYPE dcomp_scale_transform_query_interface(
        struct dcomp_scale_transform *transform, REFIID iid, void **out);
static ULONG STDMETHODCALLTYPE dcomp_scale_transform_addref(struct dcomp_scale_transform *transform);
static ULONG STDMETHODCALLTYPE dcomp_scale_transform_release(struct dcomp_scale_transform *transform);
static HRESULT STDMETHODCALLTYPE dcomp_scale_transform_set_float(
        struct dcomp_scale_transform *transform, float value);
static HRESULT STDMETHODCALLTYPE dcomp_scale_transform_set_object(
        struct dcomp_scale_transform *transform, void *object);
static HRESULT STDMETHODCALLTYPE dcomp_hwnd_surface_query_interface(
        struct dcomp_hwnd_surface *surface, REFIID iid, void **out);
static ULONG STDMETHODCALLTYPE dcomp_hwnd_surface_addref(struct dcomp_hwnd_surface *surface);
static ULONG STDMETHODCALLTYPE dcomp_hwnd_surface_release(struct dcomp_hwnd_surface *surface);

static const void *dcomp_device_vtable[] =
{
    dcomp_device_query_interface,
    dcomp_device_addref,
    dcomp_device_release,
    dcomp_device_commit,
    dcomp_device_wait,
    dcomp_device_not_implemented_5,
    dcomp_device_create_target,
    dcomp_device_create_visual,
    dcomp_device_not_implemented_8,
    dcomp_device_not_implemented_9,
    dcomp_device_not_implemented_10,
    dcomp_device_create_surface_from_hwnd,
    dcomp_device_not_implemented_12,
    dcomp_device_create_scale_transform,
    dcomp_device_not_implemented_14,
    dcomp_device_not_implemented_15,
    dcomp_device_not_implemented_16,
    dcomp_device_not_implemented_17,
    dcomp_device_not_implemented_18,
    dcomp_device_not_implemented_19,
    dcomp_device_not_implemented_20,
    dcomp_device_not_implemented_21,
    dcomp_device_not_implemented_22,
    dcomp_device_not_implemented_23,
    dcomp_device_not_implemented_24,
    dcomp_device_not_implemented_25,
    dcomp_device_check_state,
};

static const void *dcomp_target_vtable[] =
{
    dcomp_target_query_interface,
    dcomp_target_addref,
    dcomp_target_release,
    dcomp_target_set_root,
};

static const void *dcomp_partner_vtable[] =
{
    dcomp_partner_query_interface,
    dcomp_partner_addref,
    dcomp_partner_release,
    dcomp_partner_commit,
    dcomp_partner_wait,
    dcomp_partner_not_implemented,
    dcomp_partner_create_visual,
    dcomp_partner_not_implemented,
    dcomp_partner_not_implemented,
    dcomp_partner_not_implemented,
    dcomp_partner_not_implemented,
    dcomp_partner_not_implemented,
    dcomp_partner_not_implemented,
    dcomp_partner_not_implemented,
    dcomp_partner_not_implemented,
    dcomp_partner_not_implemented,
    dcomp_partner_not_implemented,
    dcomp_partner_not_implemented,
    dcomp_partner_not_implemented,
    dcomp_partner_not_implemented,
    dcomp_partner_not_implemented,
    dcomp_partner_not_implemented,
    dcomp_partner_not_implemented,
    dcomp_partner_not_implemented,
    dcomp_partner_create_target,
    dcomp_partner_not_implemented,
    dcomp_partner_not_implemented,
    dcomp_partner_create_shared_resource,
    dcomp_partner_open_shared_resource_handle,
    dcomp_partner_create_from_shared_visual_handle,
};

static const void *dcomp_visual_vtable[] =
{
    dcomp_visual_query_interface,
    dcomp_visual_addref,
    dcomp_visual_release,
    dcomp_visual_set_object,
    dcomp_visual_set_float,
    dcomp_visual_set_object,
    dcomp_visual_set_float,
    dcomp_visual_set_object,
    dcomp_visual_set_matrix,
    dcomp_visual_set_object,
    dcomp_visual_set_object,
    dcomp_visual_set_mode,
    dcomp_visual_set_mode,
    dcomp_visual_set_object,
    dcomp_visual_set_rect,
    dcomp_visual_set_content,
    dcomp_visual_add_visual,
    dcomp_visual_remove_visual,
    dcomp_visual_remove_all,
    dcomp_visual_set_mode,
};

static const void *dcomp_scale_transform_vtable[] =
{
    dcomp_scale_transform_query_interface,
    dcomp_scale_transform_addref,
    dcomp_scale_transform_release,
    dcomp_scale_transform_set_object,
    dcomp_scale_transform_set_float,
    dcomp_scale_transform_set_object,
    dcomp_scale_transform_set_float,
    dcomp_scale_transform_set_object,
    dcomp_scale_transform_set_float,
    dcomp_scale_transform_set_object,
    dcomp_scale_transform_set_float,
};

static const void *dcomp_hwnd_surface_vtable[] =
{
    dcomp_hwnd_surface_query_interface,
    dcomp_hwnd_surface_addref,
    dcomp_hwnd_surface_release,
};

static HRESULT STDMETHODCALLTYPE dcomp_device_query_interface(struct dcomp_device *device,
        REFIID iid, void **out)
{
    if (!out) return E_POINTER;
    *out = NULL;
    if (IsEqualGUID(iid, &IID_IUnknown_local) || IsEqualGUID(iid, &IID_IDCompositionDevice))
        *out = device;
    else if (IsEqualGUID(iid, &IID_IDCompositionDevice2)
            || IsEqualGUID(iid, &IID_IDCompositionDevice3)
            || IsEqualGUID(iid, &IID_IDCompositionDesktopDevice)
            || IsEqualGUID(iid, &IID_IDCompositionDesktopDevicePartner))
        *out = &device->partner;
    else
    {
        FIXME("Unsupported interface %s.\n", debugstr_guid(iid));
        return E_NOINTERFACE;
    }

    dcomp_device_addref(device);
    return S_OK;
}

static ULONG STDMETHODCALLTYPE dcomp_device_addref(struct dcomp_device *device)
{
    return InterlockedIncrement(&device->refcount);
}

static ULONG STDMETHODCALLTYPE dcomp_device_release(struct dcomp_device *device)
{
    ULONG refcount = InterlockedDecrement(&device->refcount);

    if (!refcount) free(device);
    return refcount;
}

static void set_target_window(struct dcomp_visual *visual, HWND window)
{
    struct dcomp_visual *child;
    IDXGISwapChain *swapchain;

    if (visual->content && SUCCEEDED(IUnknown_QueryInterface(visual->content,
            &IID_IDXGISwapChain_local, (void **)&swapchain)))
    {
        if (visual->content_window
                && !SetPropW(visual->content_window, wine_dcomp_target_window_propW, (HANDLE)window))
            WARN("Failed to map content window %p to target %p, error %lu.\n",
                    visual->content_window, window, GetLastError());
        IDXGISwapChain_Release(swapchain);
    }
    LIST_FOR_EACH_ENTRY(child, &visual->children, struct dcomp_visual, entry)
        set_target_window(child, window);
}

static HRESULT STDMETHODCALLTYPE dcomp_device_commit(struct dcomp_device *device)
{
    struct dcomp_target *target;

    TRACE("device %p.\n", device);

    EnterCriticalSection(&dcomp_cs);
    LIST_FOR_EACH_ENTRY(target, &device->targets, struct dcomp_target, entry)
        if (target->window && target->root) set_target_window(target->root, target->window);
    LeaveCriticalSection(&dcomp_cs);
    return S_OK;
}

static HRESULT STDMETHODCALLTYPE dcomp_device_wait(struct dcomp_device *device)
{
    TRACE("device %p.\n", device);
    return S_OK;
}

static HRESULT STDMETHODCALLTYPE dcomp_device_create_target(struct dcomp_device *device,
        HWND window, BOOL topmost, struct dcomp_target **out)
{
    struct dcomp_target *target;
    WCHAR class_name[64];
    RECT rect;

    class_name[0] = 0;
    GetClassNameW(window, class_name, ARRAY_SIZE(class_name));
    GetWindowRect(window, &rect);
    TRACE("device %p, window %p class %s parent %p rect %s topmost %d, out %p.\n",
            device, window, debugstr_w(class_name), GetParent(window), wine_dbgstr_rect(&rect), topmost, out);
    if (!out) return E_POINTER;
    *out = NULL;
    if (!(target = calloc(1, sizeof(*target)))) return E_OUTOFMEMORY;
    target->lpVtbl = dcomp_target_vtable;
    target->refcount = 1;
    target->window = window;
    target->device = device;
    dcomp_device_addref(device);
    EnterCriticalSection(&dcomp_cs);
    list_add_tail(&device->targets, &target->entry);
    LeaveCriticalSection(&dcomp_cs);
    *out = target;
    return S_OK;
}

static HRESULT STDMETHODCALLTYPE dcomp_device_create_visual(struct dcomp_device *device,
        struct dcomp_visual **out)
{
    struct dcomp_visual *visual;

    TRACE("device %p, out %p.\n", device, out);
    if (!out) return E_POINTER;
    *out = NULL;
    if (!(visual = calloc(1, sizeof(*visual)))) return E_OUTOFMEMORY;
    visual->lpVtbl = dcomp_visual_vtable;
    visual->refcount = 1;
    list_init(&visual->children);
    *out = visual;
    return S_OK;
}

static HRESULT STDMETHODCALLTYPE dcomp_device_create_scale_transform(struct dcomp_device *device,
        struct dcomp_scale_transform **out)
{
    struct dcomp_scale_transform *transform;

    TRACE("device %p, out %p.\n", device, out);
    if (!out) return E_POINTER;
    *out = NULL;
    if (!(transform = calloc(1, sizeof(*transform)))) return E_OUTOFMEMORY;
    transform->lpVtbl = dcomp_scale_transform_vtable;
    transform->refcount = 1;
    *out = transform;
    return S_OK;
}

static HRESULT STDMETHODCALLTYPE dcomp_device_create_surface_from_hwnd(struct dcomp_device *device,
        HWND window, struct dcomp_hwnd_surface **out)
{
    struct dcomp_hwnd_surface *surface;
    WCHAR class_name[64];
    RECT rect;

    class_name[0] = 0;
    GetClassNameW(window, class_name, ARRAY_SIZE(class_name));
    GetWindowRect(window, &rect);
    TRACE("device %p, window %p class %s parent %p rect %s, out %p.\n",
            device, window, debugstr_w(class_name), GetParent(window), wine_dbgstr_rect(&rect), out);
    if (!window || !out) return E_INVALIDARG;
    *out = NULL;
    if (!(surface = calloc(1, sizeof(*surface)))) return E_OUTOFMEMORY;
    surface->lpVtbl = dcomp_hwnd_surface_vtable;
    surface->refcount = 1;
    surface->window = window;
    *out = surface;
    return S_OK;
}

static HRESULT STDMETHODCALLTYPE dcomp_device_check_state(struct dcomp_device *device, BOOL *valid)
{
    TRACE("device %p, valid %p.\n", device, valid);
    if (!valid) return E_POINTER;
    *valid = TRUE;
    return S_OK;
}

static HRESULT STDMETHODCALLTYPE dcomp_partner_query_interface(struct dcomp_partner *partner,
        REFIID iid, void **out)
{
    return dcomp_device_query_interface(partner->device, iid, out);
}

static ULONG STDMETHODCALLTYPE dcomp_partner_addref(struct dcomp_partner *partner)
{
    return dcomp_device_addref(partner->device);
}

static ULONG STDMETHODCALLTYPE dcomp_partner_release(struct dcomp_partner *partner)
{
    return dcomp_device_release(partner->device);
}

static HRESULT STDMETHODCALLTYPE dcomp_partner_commit(struct dcomp_partner *partner)
{
    return dcomp_device_commit(partner->device);
}

static HRESULT STDMETHODCALLTYPE dcomp_partner_wait(struct dcomp_partner *partner)
{
    return dcomp_device_wait(partner->device);
}

static HRESULT STDMETHODCALLTYPE dcomp_partner_not_implemented(struct dcomp_partner *partner)
{
    FIXME("partner %p, caller %p.\n", partner, __builtin_return_address(0));
    return E_NOTIMPL;
}

static HRESULT STDMETHODCALLTYPE dcomp_partner_create_visual(struct dcomp_partner *partner,
        struct dcomp_visual **out)
{
    return dcomp_device_create_visual(partner->device, out);
}

static HRESULT STDMETHODCALLTYPE dcomp_partner_create_target(struct dcomp_partner *partner,
        HWND window, BOOL topmost, struct dcomp_target **out)
{
    return dcomp_device_create_target(partner->device, window, topmost, out);
}

static HRESULT STDMETHODCALLTYPE dcomp_partner_create_shared_resource(struct dcomp_partner *partner,
        REFIID iid, void **out)
{
    TRACE("partner %p, iid %s, out %p.\n", partner, debugstr_guid(iid), out);
    if (!out) return E_POINTER;
    *out = NULL;
    if (!IsEqualGUID(iid, &IID_IDCompositionVisual)) return E_NOINTERFACE;
    return dcomp_device_create_visual(partner->device, (struct dcomp_visual **)out);
}

static HRESULT STDMETHODCALLTYPE dcomp_partner_open_shared_resource_handle(struct dcomp_partner *partner,
        IUnknown *object, HANDLE *handle)
{
    TRACE("partner %p, object %p, handle %p.\n", partner, object, handle);
    if (!object || !handle) return E_INVALIDARG;
    if (!(*handle = CreateEventW(NULL, TRUE, FALSE, NULL))) return HRESULT_FROM_WIN32(GetLastError());
    return S_OK;
}

static HRESULT STDMETHODCALLTYPE dcomp_partner_create_from_shared_visual_handle(
        struct dcomp_partner *partner, HANDLE handle, REFIID iid, void **out)
{
    struct dcomp_target *target;

    TRACE("partner %p, handle %p, iid %s, out %p.\n",
            partner, handle, debugstr_guid(iid), out);
    if (!handle || !out) return E_INVALIDARG;
    *out = NULL;
    if (IsEqualGUID(iid, &IID_IDCompositionVisual))
        return dcomp_device_create_visual(partner->device, (struct dcomp_visual **)out);
    if (!IsEqualGUID(iid, &IID_IDCompositionTarget)) return E_NOINTERFACE;
    if (!(target = calloc(1, sizeof(*target)))) return E_OUTOFMEMORY;
    target->lpVtbl = dcomp_target_vtable;
    target->refcount = 1;
    *out = target;
    return S_OK;
}

static HRESULT STDMETHODCALLTYPE dcomp_target_query_interface(struct dcomp_target *target,
        REFIID iid, void **out)
{
    if (!out) return E_POINTER;
    *out = NULL;
    if (!IsEqualGUID(iid, &IID_IUnknown_local) && !IsEqualGUID(iid, &IID_IDCompositionTarget))
        return E_NOINTERFACE;
    *out = target;
    dcomp_target_addref(target);
    return S_OK;
}

static ULONG STDMETHODCALLTYPE dcomp_target_addref(struct dcomp_target *target)
{
    return InterlockedIncrement(&target->refcount);
}

static ULONG STDMETHODCALLTYPE dcomp_target_release(struct dcomp_target *target)
{
    ULONG refcount = InterlockedDecrement(&target->refcount);

    if (!refcount)
    {
        if (target->device)
        {
            EnterCriticalSection(&dcomp_cs);
            list_remove(&target->entry);
            LeaveCriticalSection(&dcomp_cs);
            dcomp_device_release(target->device);
        }
        if (target->root) dcomp_visual_release(target->root);
        free(target);
    }
    return refcount;
}

static HRESULT STDMETHODCALLTYPE dcomp_target_set_root(struct dcomp_target *target,
        struct dcomp_visual *visual)
{
    struct dcomp_visual *old;

    TRACE("target %p, visual %p.\n", target, visual);
    if (visual) dcomp_visual_addref(visual);
    EnterCriticalSection(&dcomp_cs);
    old = target->root;
    target->root = visual;
    LeaveCriticalSection(&dcomp_cs);
    if (old) dcomp_visual_release(old);
    return S_OK;
}

static HRESULT STDMETHODCALLTYPE dcomp_visual_query_interface(struct dcomp_visual *visual,
        REFIID iid, void **out)
{
    if (!out) return E_POINTER;
    *out = NULL;
    if (!IsEqualGUID(iid, &IID_IUnknown_local) && !IsEqualGUID(iid, &IID_IDCompositionVisual))
        return E_NOINTERFACE;
    *out = visual;
    dcomp_visual_addref(visual);
    return S_OK;
}

static ULONG STDMETHODCALLTYPE dcomp_visual_addref(struct dcomp_visual *visual)
{
    return InterlockedIncrement(&visual->refcount);
}

static ULONG STDMETHODCALLTYPE dcomp_visual_release(struct dcomp_visual *visual)
{
    ULONG refcount = InterlockedDecrement(&visual->refcount);

    if (!refcount)
    {
        dcomp_visual_remove_all(visual);
        if (visual->content_window
                && GetPropW(visual->content_window, wine_dcomp_visual_propW) == (HANDLE)visual)
        {
            RemovePropW(visual->content_window, wine_dcomp_target_window_propW);
            RemovePropW(visual->content_window, wine_dcomp_visual_propW);
        }
        if (visual->content) IUnknown_Release(visual->content);
        free(visual);
    }
    return refcount;
}

static HRESULT STDMETHODCALLTYPE dcomp_visual_set_object(struct dcomp_visual *visual, void *object)
{
    TRACE("visual %p, object %p.\n", visual, object);
    return S_OK;
}

static HRESULT STDMETHODCALLTYPE dcomp_visual_set_float(struct dcomp_visual *visual, float value)
{
    TRACE("visual %p, value %.8e.\n", visual, value);
    return S_OK;
}

static HRESULT STDMETHODCALLTYPE dcomp_visual_set_matrix(struct dcomp_visual *visual, const void *matrix)
{
    TRACE("visual %p, matrix %p.\n", visual, matrix);
    return S_OK;
}

static HRESULT STDMETHODCALLTYPE dcomp_visual_set_mode(struct dcomp_visual *visual, UINT mode)
{
    TRACE("visual %p, mode %u.\n", visual, mode);
    return S_OK;
}

static HRESULT STDMETHODCALLTYPE dcomp_visual_set_rect(struct dcomp_visual *visual, const void *rect)
{
    TRACE("visual %p, rect %p.\n", visual, rect);
    return S_OK;
}

struct find_dummy_window
{
    struct dcomp_visual *visual;
    HWND window;
};

static BOOL CALLBACK find_dummy_window(HWND window, LPARAM param)
{
    struct find_dummy_window *find = (struct find_dummy_window *)param;
    WCHAR class_name[ARRAY_SIZE(dxvk_dummy_window_classW)];
    HANDLE owner;

    if ((!(owner = GetPropW(window, wine_dcomp_visual_propW)) || owner == (HANDLE)find->visual)
            && GetClassNameW(window, class_name, ARRAY_SIZE(class_name))
            && !lstrcmpW(class_name, dxvk_dummy_window_classW))
    {
        if (SetPropW(window, wine_dcomp_visual_propW, (HANDLE)find->visual))
        {
            find->window = window;
            return FALSE;
        }
    }
    return TRUE;
}

static HWND reserve_content_window(struct dcomp_visual *visual, IUnknown *content)
{
    struct find_dummy_window find = {visual};
    IDXGISwapChain *swapchain;
    DXGI_SWAP_CHAIN_DESC desc;
    WCHAR class_name[ARRAY_SIZE(dxvk_dummy_window_classW)];
    DWORD process_id;
    HANDLE owner;

    if (!content || FAILED(IUnknown_QueryInterface(content, &IID_IDXGISwapChain_local,
            (void **)&swapchain)))
        return NULL;

    if (SUCCEEDED(IDXGISwapChain_GetDesc(swapchain, &desc)) && desc.OutputWindow
            && GetClassNameW(desc.OutputWindow, class_name, ARRAY_SIZE(class_name))
            && !lstrcmpW(class_name, dxvk_dummy_window_classW)
            && GetWindowThreadProcessId(desc.OutputWindow, &process_id)
            && process_id == GetCurrentProcessId()
            && (!(owner = GetPropW(desc.OutputWindow, wine_dcomp_visual_propW)) || owner == (HANDLE)visual))
    {
        if (SetPropW(desc.OutputWindow, wine_dcomp_visual_propW, (HANDLE)visual))
            find.window = desc.OutputWindow;
    }
    else
        EnumThreadWindows(GetCurrentThreadId(), find_dummy_window, (LPARAM)&find);

    IDXGISwapChain_Release(swapchain);
    return find.window;
}

static HRESULT STDMETHODCALLTYPE dcomp_visual_set_content(struct dcomp_visual *visual, IUnknown *content)
{
    IUnknown *old_content;
    HWND old_window, new_window;

    TRACE("visual %p, content %p.\n", visual, content);
    if (content) IUnknown_AddRef(content);
    new_window = reserve_content_window(visual, content);

    EnterCriticalSection(&dcomp_cs);
    old_content = visual->content;
    old_window = visual->content_window;
    visual->content = content;
    visual->content_window = new_window;
    LeaveCriticalSection(&dcomp_cs);

    if (old_window && old_window != new_window
            && GetPropW(old_window, wine_dcomp_visual_propW) == (HANDLE)visual)
    {
        RemovePropW(old_window, wine_dcomp_target_window_propW);
        RemovePropW(old_window, wine_dcomp_visual_propW);
    }
    if (old_content) IUnknown_Release(old_content);
    TRACE("visual %p, content window %p.\n", visual, new_window);
    return S_OK;
}

static HRESULT STDMETHODCALLTYPE dcomp_visual_add_visual(struct dcomp_visual *visual,
        struct dcomp_visual *child, BOOL insert_above, struct dcomp_visual *reference)
{
    struct dcomp_visual *old, *ancestor;

    TRACE("visual %p, child %p, insert_above %d, reference %p.\n",
            visual, child, insert_above, reference);
    if (!child) return E_INVALIDARG;
    EnterCriticalSection(&dcomp_cs);
    for (ancestor = visual; ancestor; ancestor = ancestor->parent)
    {
        if (ancestor == child)
        {
            LeaveCriticalSection(&dcomp_cs);
            return E_INVALIDARG;
        }
    }
    dcomp_visual_addref(child);
    if ((old = child->parent)) list_remove(&child->entry);
    child->parent = visual;
    list_add_tail(&visual->children, &child->entry);
    LeaveCriticalSection(&dcomp_cs);
    if (old) dcomp_visual_release(child);
    return S_OK;
}

static HRESULT STDMETHODCALLTYPE dcomp_visual_remove_visual(struct dcomp_visual *visual,
        struct dcomp_visual *child)
{
    BOOL found;

    TRACE("visual %p, child %p.\n", visual, child);
    if (!child) return E_INVALIDARG;
    EnterCriticalSection(&dcomp_cs);
    if ((found = child->parent == visual))
    {
        list_remove(&child->entry);
        child->parent = NULL;
    }
    LeaveCriticalSection(&dcomp_cs);
    if (!found) return E_INVALIDARG;
    dcomp_visual_release(child);
    return S_OK;
}

static HRESULT STDMETHODCALLTYPE dcomp_visual_remove_all(struct dcomp_visual *visual)
{
    struct dcomp_visual *child;
    struct list *head;

    TRACE("visual %p.\n", visual);
    for (;;)
    {
        child = NULL;
        EnterCriticalSection(&dcomp_cs);
        if ((head = list_head(&visual->children)))
        {
            child = LIST_ENTRY(head, struct dcomp_visual, entry);
            list_remove(head);
            child->parent = NULL;
        }
        LeaveCriticalSection(&dcomp_cs);
        if (!child) break;
        dcomp_visual_release(child);
    }
    return S_OK;
}

static HRESULT STDMETHODCALLTYPE dcomp_scale_transform_query_interface(
        struct dcomp_scale_transform *transform, REFIID iid, void **out)
{
    if (!out) return E_POINTER;
    *out = NULL;
    if (!IsEqualGUID(iid, &IID_IUnknown_local)
            && !IsEqualGUID(iid, &IID_IDCompositionTransform)
            && !IsEqualGUID(iid, &IID_IDCompositionScaleTransform))
        return E_NOINTERFACE;
    *out = transform;
    dcomp_scale_transform_addref(transform);
    return S_OK;
}

static ULONG STDMETHODCALLTYPE dcomp_scale_transform_addref(struct dcomp_scale_transform *transform)
{
    return InterlockedIncrement(&transform->refcount);
}

static ULONG STDMETHODCALLTYPE dcomp_scale_transform_release(struct dcomp_scale_transform *transform)
{
    ULONG refcount = InterlockedDecrement(&transform->refcount);

    if (!refcount) free(transform);
    return refcount;
}

static HRESULT STDMETHODCALLTYPE dcomp_scale_transform_set_float(
        struct dcomp_scale_transform *transform, float value)
{
    TRACE("transform %p, value %.8e.\n", transform, value);
    return S_OK;
}

static HRESULT STDMETHODCALLTYPE dcomp_scale_transform_set_object(
        struct dcomp_scale_transform *transform, void *object)
{
    TRACE("transform %p, object %p.\n", transform, object);
    return S_OK;
}

static HRESULT STDMETHODCALLTYPE dcomp_hwnd_surface_query_interface(
        struct dcomp_hwnd_surface *surface, REFIID iid, void **out)
{
    if (!out) return E_POINTER;
    *out = NULL;
    if (!IsEqualGUID(iid, &IID_IUnknown_local))
    {
        FIXME("surface %p, unsupported interface %s.\n", surface, debugstr_guid(iid));
        return E_NOINTERFACE;
    }
    *out = surface;
    dcomp_hwnd_surface_addref(surface);
    return S_OK;
}

static ULONG STDMETHODCALLTYPE dcomp_hwnd_surface_addref(struct dcomp_hwnd_surface *surface)
{
    return InterlockedIncrement(&surface->refcount);
}

static ULONG STDMETHODCALLTYPE dcomp_hwnd_surface_release(struct dcomp_hwnd_surface *surface)
{
    ULONG refcount = InterlockedDecrement(&surface->refcount);

    if (!refcount) free(surface);
    return refcount;
}

static HRESULT dcomp_create_device(REFIID iid, void **out)
{
    struct dcomp_device *device;
    HRESULT hr;

    if (!out) return E_POINTER;
    *out = NULL;
    if (!(device = calloc(1, sizeof(*device)))) return E_OUTOFMEMORY;
    device->lpVtbl = dcomp_device_vtable;
    device->refcount = 1;
    device->partner.lpVtbl = dcomp_partner_vtable;
    device->partner.device = device;
    list_init(&device->targets);
    hr = dcomp_device_query_interface(device, iid, out);
    dcomp_device_release(device);
    return hr;
}

HRESULT WINAPI DCompositionCreateDevice(IDXGIDevice *dxgi_device, REFIID iid, void **device)
{
    FIXME("%p, %s, %p.\n", dxgi_device, debugstr_guid(iid), device);

    return dcomp_create_device(iid, device);
}

HRESULT WINAPI DCompositionCreateDevice2(IUnknown *rendering_device, REFIID iid, void **device)
{
    FIXME("%p, %s, %p.\n", rendering_device, debugstr_guid(iid), device);

    /* Try to workaround Chromium race fixed by Chromium commit 1c13c9105b40e83f87ebcad104fab57fcef43eb9. */
    Sleep(50);

    return dcomp_create_device(iid, device);
}

HRESULT WINAPI DCompositionCreateDevice3(IUnknown *rendering_device, REFIID iid, void **device)
{
    FIXME("%p, %s, %p.\n", rendering_device, debugstr_guid(iid), device);

    return dcomp_create_device(iid, device);
}
