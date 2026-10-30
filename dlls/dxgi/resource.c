/*
 * Copyright 2009 Henri Verbeet for CodeWeavers
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
 *
 */

#include "dxgi_private.h"
#include "wine/dxgi_shared.h"

WINE_DEFAULT_DEBUG_CHANNEL(dxgi);

/* Inner IUnknown methods */

static inline struct dxgi_resource *impl_from_IUnknown(IUnknown *iface)
{
    return CONTAINING_RECORD(iface, struct dxgi_resource, IUnknown_iface);
}

static HRESULT STDMETHODCALLTYPE dxgi_resource_inner_QueryInterface(IUnknown *iface, REFIID riid, void **out)
{
    struct dxgi_resource *resource = impl_from_IUnknown(iface);
    bool is_subresource = !!resource->parent_resource;

    TRACE("iface %p, riid %s, out %p.\n", iface, debugstr_guid(riid), out);

    if (IsEqualGUID(riid, &IID_IDXGIKeyedMutex) && resource->keyed_mutex)
    {
        *out = &resource->IDXGIKeyedMutex_iface;
        IDXGIKeyedMutex_AddRef(&resource->IDXGIKeyedMutex_iface);
        return S_OK;
    }

    if ((IsEqualGUID(riid, &IID_IDXGISurface2)
            || IsEqualGUID(riid, &IID_IDXGISurface1)
            || IsEqualGUID(riid, &IID_IDXGISurface)) && resource->IDXGISurface2_iface.lpVtbl != NULL)
    {
        IDXGISurface2_AddRef(&resource->IDXGISurface2_iface);
        *out = &resource->IDXGISurface2_iface;
        return S_OK;
    }
    else if ((!is_subresource && (IsEqualGUID(riid, &IID_IDXGIResource)
            || IsEqualGUID(riid, &IID_IDXGIResource1)))
            || IsEqualGUID(riid, &IID_IDXGIDeviceSubObject)
            || IsEqualGUID(riid, &IID_IDXGIObject)
            || IsEqualGUID(riid, &IID_IUnknown))
    {
        IDXGIResource1_AddRef(&resource->IDXGIResource1_iface);
        *out = &resource->IDXGIResource1_iface;
        return S_OK;
    }

    WARN("%s not implemented, returning E_NOINTERFACE\n", debugstr_guid(riid));

    *out = NULL;
    return E_NOINTERFACE;
}

static ULONG STDMETHODCALLTYPE dxgi_resource_inner_AddRef(IUnknown *iface)
{
    struct dxgi_resource *resource = impl_from_IUnknown(iface);
    ULONG refcount = InterlockedIncrement(&resource->refcount);

    TRACE("%p increasing refcount to %lu.\n", resource, refcount);

    return refcount;
}

static ULONG STDMETHODCALLTYPE dxgi_resource_inner_Release(IUnknown *iface)
{
    struct dxgi_resource *resource = impl_from_IUnknown(iface);
    ULONG refcount = InterlockedDecrement(&resource->refcount);

    TRACE("%p decreasing refcount to %lu.\n", resource, refcount);

    if (!refcount)
    {
        if (resource->parent_resource)
            IDXGIResource1_Release(resource->parent_resource);
        if (resource->keyed_mutex)
            wined3d_texture_destroy_keyed_mutex(wined3d_texture_from_resource(resource->wined3d_resource),
                    resource->keyed_mutex);
        wined3d_private_store_cleanup(&resource->private_store);
        free(resource);
    }

    return refcount;
}

static inline struct dxgi_resource *impl_from_IDXGISurface2(IDXGISurface2 *iface)
{
    return CONTAINING_RECORD(iface, struct dxgi_resource, IDXGISurface2_iface);
}

/* IUnknown methods */

static HRESULT STDMETHODCALLTYPE dxgi_surface_QueryInterface(IDXGISurface2 *iface, REFIID riid,
        void **object)
{
    struct dxgi_resource *resource = impl_from_IDXGISurface2(iface);
    return IDXGIResource1_QueryInterface(&resource->IDXGIResource1_iface, riid, object);
}

static ULONG STDMETHODCALLTYPE dxgi_surface_AddRef(IDXGISurface2 *iface)
{
    struct dxgi_resource *resource = impl_from_IDXGISurface2(iface);
    return IDXGIResource1_AddRef(&resource->IDXGIResource1_iface);
}

static ULONG STDMETHODCALLTYPE dxgi_surface_Release(IDXGISurface2 *iface)
{
    struct dxgi_resource *resource = impl_from_IDXGISurface2(iface);
    return IDXGIResource1_Release(&resource->IDXGIResource1_iface);
}

/* IDXGIObject methods */

static HRESULT STDMETHODCALLTYPE dxgi_surface_SetPrivateData(IDXGISurface2 *iface,
        REFGUID guid, UINT data_size, const void *data)
{
    struct dxgi_resource *resource = impl_from_IDXGISurface2(iface);
    return IDXGIResource1_SetPrivateData(&resource->IDXGIResource1_iface, guid, data_size, data);
}

static HRESULT STDMETHODCALLTYPE dxgi_surface_SetPrivateDataInterface(IDXGISurface2 *iface,
        REFGUID guid, const IUnknown *object)
{
    struct dxgi_resource *resource = impl_from_IDXGISurface2(iface);
    return IDXGIResource1_SetPrivateDataInterface(&resource->IDXGIResource1_iface, guid, object);
}

static HRESULT STDMETHODCALLTYPE dxgi_surface_GetPrivateData(IDXGISurface2 *iface,
        REFGUID guid, UINT *data_size, void *data)
{
    struct dxgi_resource *resource = impl_from_IDXGISurface2(iface);
    return IDXGIResource1_GetPrivateData(&resource->IDXGIResource1_iface, guid, data_size, data);
}

static HRESULT STDMETHODCALLTYPE dxgi_surface_GetParent(IDXGISurface2 *iface, REFIID riid, void **parent)
{
    struct dxgi_resource *resource = impl_from_IDXGISurface2(iface);
    return IDXGIResource1_GetParent(&resource->IDXGIResource1_iface, riid, parent);
}

/* IDXGIDeviceSubObject methods */

static HRESULT STDMETHODCALLTYPE dxgi_surface_GetDevice(IDXGISurface2 *iface, REFIID riid, void **device)
{
    struct dxgi_resource *resource = impl_from_IDXGISurface2(iface);
    return IDXGIResource1_GetDevice(&resource->IDXGIResource1_iface, riid, device);
}

/* IDXGISurface methods */
static HRESULT STDMETHODCALLTYPE dxgi_surface_GetDesc(IDXGISurface2 *iface, DXGI_SURFACE_DESC *desc)
{
    struct dxgi_resource *resource = impl_from_IDXGISurface2(iface);
    bool is_subresource = !!resource->parent_resource, is_buffer = false;
    struct wined3d_sub_resource_desc subresource_desc;
    struct wined3d_resource_desc resource_desc;

    TRACE("iface %p, desc %p.\n", iface, desc);

    wined3d_mutex_lock();
    wined3d_resource_get_sub_resource_desc(resource->wined3d_resource, resource->subresource_idx, &subresource_desc);
    if (is_subresource)
    {
        wined3d_resource_get_desc(resource->wined3d_resource, &resource_desc);
        is_buffer = resource_desc.resource_type == WINED3D_RTYPE_BUFFER;
    }
    wined3d_mutex_unlock();
    desc->Width = subresource_desc.width;
    desc->Height = subresource_desc.height;
    desc->Format = is_buffer ? DXGI_FORMAT_UNKNOWN : dxgi_format_from_wined3dformat(subresource_desc.format);
    dxgi_sample_desc_from_wined3d(&desc->SampleDesc, subresource_desc.multisample_type, subresource_desc.multisample_quality);

    return S_OK;
}

static HRESULT STDMETHODCALLTYPE dxgi_surface_Map(IDXGISurface2 *iface, DXGI_MAPPED_RECT *mapped_rect, UINT flags)
{
    struct dxgi_resource *resource = impl_from_IDXGISurface2(iface);
    struct wined3d_map_desc wined3d_map_desc;
    DWORD wined3d_map_flags = 0;
    HRESULT hr;

    TRACE("iface %p, mapped_rect %p, flags %#x.\n", iface, mapped_rect, flags);

    if (flags & DXGI_MAP_READ)
        wined3d_map_flags |= WINED3D_MAP_READ;
    if (flags & DXGI_MAP_WRITE)
        wined3d_map_flags |= WINED3D_MAP_WRITE;
    if (flags & DXGI_MAP_DISCARD)
        wined3d_map_flags |= WINED3D_MAP_DISCARD;

    if (SUCCEEDED(hr = wined3d_resource_map(resource->wined3d_resource, resource->subresource_idx,
            &wined3d_map_desc, NULL, wined3d_map_flags)))
    {
        mapped_rect->Pitch = wined3d_map_desc.row_pitch;
        mapped_rect->pBits = wined3d_map_desc.data;
    }

    return hr;
}

static HRESULT STDMETHODCALLTYPE dxgi_surface_Unmap(IDXGISurface2 *iface)
{
    struct dxgi_resource *resource = impl_from_IDXGISurface2(iface);

    TRACE("iface %p.\n", iface);
    wined3d_resource_unmap(resource->wined3d_resource, resource->subresource_idx);
    return S_OK;
}

/* IDXGISurface1 methods */
static HRESULT STDMETHODCALLTYPE dxgi_surface_GetDC(IDXGISurface2 *iface, BOOL discard, HDC *hdc)
{
    struct dxgi_resource *resource = impl_from_IDXGISurface2(iface);
    HRESULT hr;

    FIXME("iface %p, discard %d, hdc %p semi-stub!\n", iface, discard, hdc);

    if (!hdc)
        return E_INVALIDARG;

    wined3d_mutex_lock();
    hr = wined3d_texture_get_dc(wined3d_texture_from_resource(resource->wined3d_resource),
            resource->subresource_idx, hdc);
    wined3d_mutex_unlock();

    if (SUCCEEDED(hr))
       resource->dc = *hdc;

    return hr;
}

static HRESULT STDMETHODCALLTYPE dxgi_surface_ReleaseDC(IDXGISurface2 *iface, RECT *dirty_rect)
{
    struct dxgi_resource *resource = impl_from_IDXGISurface2(iface);
    HRESULT hr;

    TRACE("iface %p, rect %s\n", iface, wine_dbgstr_rect(dirty_rect));

    if (!IsRectEmpty(dirty_rect))
        FIXME("dirty rectangle is ignored.\n");

    wined3d_mutex_lock();
    hr = wined3d_texture_release_dc(wined3d_texture_from_resource(resource->wined3d_resource),
            resource->subresource_idx, resource->dc);
    wined3d_mutex_unlock();

    return hr;
}

/* IDXGISurface2 methods */
static HRESULT STDMETHODCALLTYPE dxgi_surface_GetResource(IDXGISurface2 *iface, REFIID iid,
        void **parent_resource, UINT *subresource_idx)
{
    struct dxgi_resource *resource = impl_from_IDXGISurface2(iface);
    HRESULT hr;

    TRACE("iface %p, iid %s, parent_resource %p, subresource_idx %p stub!\n", iface,
            wine_dbgstr_guid(iid), parent_resource, subresource_idx);

    if (!parent_resource)
        return E_POINTER;

    if (resource->parent_resource)
        hr = IDXGIResource1_QueryInterface(resource->parent_resource, iid, parent_resource);
    else
        hr = IDXGIResource1_QueryInterface(&resource->IDXGIResource1_iface, iid, parent_resource);

    if (SUCCEEDED(hr))
        *subresource_idx = resource->subresource_idx;
    else
        parent_resource = NULL;
    return hr;
}

static const struct IDXGISurface2Vtbl dxgi_surface_vtbl =
{
    /* IUnknown methods */
    dxgi_surface_QueryInterface,
    dxgi_surface_AddRef,
    dxgi_surface_Release,
    /* IDXGIObject methods */
    dxgi_surface_SetPrivateData,
    dxgi_surface_SetPrivateDataInterface,
    dxgi_surface_GetPrivateData,
    dxgi_surface_GetParent,
    /* IDXGIDeviceSubObject methods */
    dxgi_surface_GetDevice,
    /* IDXGISurface methods */
    dxgi_surface_GetDesc,
    dxgi_surface_Map,
    dxgi_surface_Unmap,
    /* IDXGISurface1 methods */
    dxgi_surface_GetDC,
    dxgi_surface_ReleaseDC,
    /* IDXGISurface2 methods */
    dxgi_surface_GetResource,
};

static inline struct dxgi_resource *impl_from_IDXGIResource1(IDXGIResource1 *iface)
{
    return CONTAINING_RECORD(iface, struct dxgi_resource, IDXGIResource1_iface);
}

/* IUnknown methods */

static HRESULT STDMETHODCALLTYPE dxgi_resource_QueryInterface(IDXGIResource1 *iface, REFIID riid,
        void **object)
{
    struct dxgi_resource *resource = impl_from_IDXGIResource1(iface);
    TRACE("Forwarding to outer IUnknown\n");
    return IUnknown_QueryInterface(resource->outer_unknown, riid, object);
}

static ULONG STDMETHODCALLTYPE dxgi_resource_AddRef(IDXGIResource1 *iface)
{
    struct dxgi_resource *resource = impl_from_IDXGIResource1(iface);
    TRACE("Forwarding to outer IUnknown\n");
    return IUnknown_AddRef(resource->outer_unknown);
}

static ULONG STDMETHODCALLTYPE dxgi_resource_Release(IDXGIResource1 *iface)
{
    struct dxgi_resource *resource = impl_from_IDXGIResource1(iface);
    TRACE("Forwarding to outer IUnknown\n");
    return IUnknown_Release(resource->outer_unknown);
}

/* IDXGIObject methods */

static HRESULT STDMETHODCALLTYPE dxgi_resource_SetPrivateData(IDXGIResource1 *iface,
        REFGUID guid, UINT data_size, const void *data)
{
    struct dxgi_resource *resource = impl_from_IDXGIResource1(iface);

    TRACE("iface %p, guid %s, data_size %u, data %p.\n", iface, debugstr_guid(guid), data_size, data);

    return dxgi_set_private_data(&resource->private_store, guid, data_size, data);
}

static HRESULT STDMETHODCALLTYPE dxgi_resource_SetPrivateDataInterface(IDXGIResource1 *iface,
        REFGUID guid, const IUnknown *object)
{
    struct dxgi_resource *resource = impl_from_IDXGIResource1(iface);

    TRACE("iface %p, guid %s, object %p.\n", iface, debugstr_guid(guid), object);

    return dxgi_set_private_data_interface(&resource->private_store, guid, object);
}

static HRESULT STDMETHODCALLTYPE dxgi_resource_GetPrivateData(IDXGIResource1 *iface,
        REFGUID guid, UINT *data_size, void *data)
{
    struct dxgi_resource *resource = impl_from_IDXGIResource1(iface);

    TRACE("iface %p, guid %s, data_size %p, data %p.\n", iface, debugstr_guid(guid), data_size, data);

    return dxgi_get_private_data(&resource->private_store, guid, data_size, data);
}

static HRESULT STDMETHODCALLTYPE dxgi_resource_GetParent(IDXGIResource1 *iface, REFIID riid, void **parent)
{
    struct dxgi_resource *resource = impl_from_IDXGIResource1(iface);

    TRACE("iface %p, riid %s, parent %p.\n", iface, debugstr_guid(riid), parent);

    return IDXGIDevice_QueryInterface(resource->device, riid, parent);
}

/* IDXGIDeviceSubObject methods */

static HRESULT STDMETHODCALLTYPE dxgi_resource_GetDevice(IDXGIResource1 *iface, REFIID riid, void **device)
{
    struct dxgi_resource *resource = impl_from_IDXGIResource1(iface);

    TRACE("iface %p, riid %s, device %p.\n", iface, debugstr_guid(riid), device);

    return IDXGIDevice_QueryInterface(resource->device, riid, device);
}

/* IDXGIResource methods */
static HRESULT dxgi_resource_publish_shared_texture(struct dxgi_resource *resource, HANDLE handle)
{
    struct dxgi_shared_texture_metadata metadata = {0}, observed = {0};
    D3DKMT_OPENADAPTERFROMLUID open_adapter = {0};
    D3DKMT_CREATEDEVICE create_device = {{0}};
    D3DKMT_QUERYRESOURCEINFO query = {0};
    D3DKMT_OPENRESOURCE open = {0};
    D3DDDI_OPENALLOCATIONINFO allocation = {0};
    D3DKMT_DESTROYALLOCATION destroy = {0};
    D3DKMT_DESTROYDEVICE destroy_device = {0};
    D3DKMT_CLOSEADAPTER close_adapter = {0};
    D3DKMT_ESCAPE escape = {0};
    ID3D11Texture2D *texture;
    DXGI_ADAPTER_DESC adapter_desc;
    IDXGIAdapter *adapter;
    HRESULT hr;
    NTSTATUS status;

    if (FAILED(hr = IUnknown_QueryInterface(resource->outer_unknown, &IID_ID3D11Texture2D, (void **)&texture)))
        return hr;
    ID3D11Texture2D_GetDesc(texture, &metadata.texture);
    ID3D11Texture2D_Release(texture);
    metadata.size = sizeof(metadata);
    metadata.version = 1;
    metadata.width = metadata.texture.Width;
    metadata.height = metadata.texture.Height;
    metadata.format = metadata.texture.Format;
    metadata.dimension = D3D11_RESOURCE_DIMENSION_TEXTURE2D;
    metadata.keyed_mutex = !!resource->keyed_mutex;
    metadata.mutex_handle = resource->shared_keyed_mutex;

    if (FAILED(hr = IDXGIDevice_GetAdapter(resource->device, &adapter))) return hr;
    hr = IDXGIAdapter_GetDesc(adapter, &adapter_desc);
    IDXGIAdapter_Release(adapter);
    if (FAILED(hr)) return hr;
    open_adapter.AdapterLuid = adapter_desc.AdapterLuid;
    hr = E_FAIL;
    if ((status = D3DKMTOpenAdapterFromLuid(&open_adapter))) goto done;
    close_adapter.hAdapter = open_adapter.hAdapter;
    create_device.hAdapter = open_adapter.hAdapter;
    if ((status = D3DKMTCreateDevice(&create_device))) goto done;
    destroy_device.hDevice = create_device.hDevice;
    query.hDevice = create_device.hDevice;
    query.hGlobalShare = (D3DKMT_HANDLE)(ULONG_PTR)handle;
    if ((status = D3DKMTQueryResourceInfo(&query))) goto done;
    if (query.PrivateRuntimeDataSize && query.PrivateRuntimeDataSize != sizeof(metadata)) goto done;
    open.hDevice = create_device.hDevice;
    open.hGlobalShare = query.hGlobalShare;
    open.NumAllocations = 1;
    open.pOpenAllocationInfo = &allocation;
    open.pPrivateRuntimeData = &observed;
    open.PrivateRuntimeDataSize = query.PrivateRuntimeDataSize;
    if ((status = D3DKMTOpenResource(&open))) goto done;
    destroy.hDevice = create_device.hDevice;
    destroy.hResource = open.hResource;
    if (query.PrivateRuntimeDataSize)
    {
        if (!memcmp(&metadata, &observed, sizeof(metadata))) hr = S_OK;
        goto done;
    }
    escape.hAdapter = open_adapter.hAdapter;
    escape.hDevice = create_device.hDevice;
    escape.hContext = open.hResource;
    escape.Type = D3DKMT_ESCAPE_UPDATE_RESOURCE_WINE;
    escape.pPrivateDriverData = &metadata;
    escape.PrivateDriverDataSize = sizeof(metadata);
    if (!(status = D3DKMTEscape(&escape))) hr = S_OK;
done:
    if (destroy.hResource) D3DKMTDestroyAllocation(&destroy);
    if (destroy_device.hDevice) D3DKMTDestroyDevice(&destroy_device);
    if (close_adapter.hAdapter) D3DKMTCloseAdapter(&close_adapter);
    if (FAILED(hr)) WARN("Failed to publish shared texture descriptor, status %#lx.\n", status);
    return hr;
}

static HRESULT STDMETHODCALLTYPE dxgi_resource_GetSharedHandle(IDXGIResource1 *iface, HANDLE *shared_handle)
{
    struct dxgi_resource *resource = impl_from_IDXGIResource1(iface);
    struct wined3d_resource_desc desc;
    HANDLE handle;
    HRESULT hr;

    if (!shared_handle) return E_INVALIDARG;
    *shared_handle = NULL;
    wined3d_mutex_lock();
    wined3d_resource_get_desc(resource->wined3d_resource, &desc);
    if (desc.resource_type != WINED3D_RTYPE_TEXTURE_2D) hr = DXGI_ERROR_INVALID_CALL;
    else hr = wined3d_texture_get_shared_handle(wined3d_texture_from_resource(resource->wined3d_resource), &handle);
    wined3d_mutex_unlock();
    if (SUCCEEDED(hr) && SUCCEEDED(hr = dxgi_resource_publish_shared_texture(resource, handle)))
        *shared_handle = handle;
    return hr == WINED3DERR_INVALIDCALL ? DXGI_ERROR_INVALID_CALL : hr;
}

static HRESULT STDMETHODCALLTYPE dxgi_resource_GetUsage(IDXGIResource1 *iface, DXGI_USAGE *usage)
{
    struct dxgi_resource *resource = impl_from_IDXGIResource1(iface);
    struct wined3d_resource_desc resource_desc;

    TRACE("iface %p, usage %p.\n", iface, usage);

    wined3d_resource_get_desc(resource->wined3d_resource, &resource_desc);

    *usage = dxgi_usage_from_wined3d_bind_flags(resource_desc.bind_flags);

    if (resource_desc.resource_type != WINED3D_RTYPE_BUFFER)
    {
        struct wined3d_texture *texture = wined3d_texture_from_resource(resource->wined3d_resource);
        struct wined3d_swapchain_desc swapchain_desc;
        struct wined3d_swapchain *swapchain;

        if ((swapchain = wined3d_texture_get_swapchain(texture)))
        {
            *usage |= DXGI_USAGE_BACK_BUFFER;

            wined3d_swapchain_get_desc(swapchain, &swapchain_desc);
            if (swapchain_desc.swap_effect == WINED3D_SWAP_EFFECT_DISCARD)
                *usage |= DXGI_USAGE_DISCARD_ON_PRESENT;

            if (wined3d_swapchain_get_back_buffer(swapchain, 0) != texture)
                *usage |= DXGI_USAGE_READ_ONLY;
        }
    }

    return S_OK;
}

static HRESULT STDMETHODCALLTYPE dxgi_resource_SetEvictionPriority(IDXGIResource1 *iface, UINT eviction_priority)
{
    FIXME("iface %p, eviction_priority %u stub!\n", iface, eviction_priority);

    return E_NOTIMPL;
}

static HRESULT STDMETHODCALLTYPE dxgi_resource_GetEvictionPriority(IDXGIResource1 *iface, UINT *eviction_priority)
{
    FIXME("iface %p, eviction_priority %p stub!\n", iface, eviction_priority);

    return E_NOTIMPL;
}

/* IDXGIResource1 methods */
static HRESULT STDMETHODCALLTYPE dxgi_resource_CreateSubresourceSurface(IDXGIResource1 *iface,
        UINT index, IDXGISurface2 **surface)
{
    struct dxgi_resource *resource = impl_from_IDXGIResource1(iface), *subresource;
    struct wined3d_resource_desc desc;
    unsigned int subresource_count;
    HRESULT hr;

    TRACE("iface %p, index %u, surface %p.\n", iface, index, surface);

    wined3d_mutex_lock();
    wined3d_resource_get_desc(resource->wined3d_resource, &desc);
    subresource_count = wined3d_resource_get_sub_resource_count(resource->wined3d_resource);
    wined3d_mutex_unlock();

    if ((desc.resource_type == WINED3D_RTYPE_TEXTURE_3D && desc.depth > 1)
            || index >= subresource_count)
        return E_INVALIDARG;

    if (!(subresource = calloc(1, sizeof(*subresource))))
    {
        ERR("Failed to allocate DXGI subresource surface object memory.\n");
        return E_OUTOFMEMORY;
    }

    if (FAILED(hr = dxgi_resource_init(subresource, resource->device, NULL, TRUE,
            resource->wined3d_resource, iface, index, NULL)))
    {
        WARN("Failed to initialise resource, hr %#lx.\n", hr);
        free(subresource);
        return hr;
    }

    *surface = &subresource->IDXGISurface2_iface;
    return S_OK;
}

static HRESULT STDMETHODCALLTYPE dxgi_resource_CreateSharedHandle(IDXGIResource1 *iface,
        const SECURITY_ATTRIBUTES *attributes, DWORD access, const WCHAR *name, HANDLE *handle)
{
    FIXME("iface %p, attributes %p, access %#lx, name %s, handle %p stub!\n", iface, attributes,
            access, wine_dbgstr_w(name), handle);

    return E_NOTIMPL;
}

static const struct IDXGIResource1Vtbl dxgi_resource_vtbl =
{
    /* IUnknown methods */
    dxgi_resource_QueryInterface,
    dxgi_resource_AddRef,
    dxgi_resource_Release,
    /* IDXGIObject methods */
    dxgi_resource_SetPrivateData,
    dxgi_resource_SetPrivateDataInterface,
    dxgi_resource_GetPrivateData,
    dxgi_resource_GetParent,
    /* IDXGIDeviceSubObject methods */
    dxgi_resource_GetDevice,
    /* IDXGIResource methods */
    dxgi_resource_GetSharedHandle,
    dxgi_resource_GetUsage,
    dxgi_resource_SetEvictionPriority,
    dxgi_resource_GetEvictionPriority,
    /* IDXGIResource1 methods */
    dxgi_resource_CreateSubresourceSurface,
    dxgi_resource_CreateSharedHandle,
};

static const struct IUnknownVtbl dxgi_resource_inner_unknown_vtbl =
{
    /* IUnknown methods */
    dxgi_resource_inner_QueryInterface,
    dxgi_resource_inner_AddRef,
    dxgi_resource_inner_Release,
};

static inline struct dxgi_resource *impl_from_IDXGIKeyedMutex(IDXGIKeyedMutex *iface)
{
    return CONTAINING_RECORD(iface, struct dxgi_resource, IDXGIKeyedMutex_iface);
}

static HRESULT STDMETHODCALLTYPE dxgi_keyed_mutex_QueryInterface(IDXGIKeyedMutex *iface, REFIID iid, void **out)
{
    return IDXGIResource1_QueryInterface(&impl_from_IDXGIKeyedMutex(iface)->IDXGIResource1_iface, iid, out);
}

static ULONG STDMETHODCALLTYPE dxgi_keyed_mutex_AddRef(IDXGIKeyedMutex *iface)
{
    return IDXGIResource1_AddRef(&impl_from_IDXGIKeyedMutex(iface)->IDXGIResource1_iface);
}

static ULONG STDMETHODCALLTYPE dxgi_keyed_mutex_Release(IDXGIKeyedMutex *iface)
{
    return IDXGIResource1_Release(&impl_from_IDXGIKeyedMutex(iface)->IDXGIResource1_iface);
}

static HRESULT STDMETHODCALLTYPE dxgi_keyed_mutex_SetPrivateData(IDXGIKeyedMutex *iface,
        REFGUID guid, UINT size, const void *data)
{
    return IDXGIResource1_SetPrivateData(&impl_from_IDXGIKeyedMutex(iface)->IDXGIResource1_iface, guid, size, data);
}

static HRESULT STDMETHODCALLTYPE dxgi_keyed_mutex_SetPrivateDataInterface(IDXGIKeyedMutex *iface,
        REFGUID guid, const IUnknown *object)
{
    return IDXGIResource1_SetPrivateDataInterface(&impl_from_IDXGIKeyedMutex(iface)->IDXGIResource1_iface, guid, object);
}

static HRESULT STDMETHODCALLTYPE dxgi_keyed_mutex_GetPrivateData(IDXGIKeyedMutex *iface,
        REFGUID guid, UINT *size, void *data)
{
    return IDXGIResource1_GetPrivateData(&impl_from_IDXGIKeyedMutex(iface)->IDXGIResource1_iface, guid, size, data);
}

static HRESULT STDMETHODCALLTYPE dxgi_keyed_mutex_GetParent(IDXGIKeyedMutex *iface, REFIID iid, void **parent)
{
    return IDXGIResource1_GetParent(&impl_from_IDXGIKeyedMutex(iface)->IDXGIResource1_iface, iid, parent);
}

static HRESULT STDMETHODCALLTYPE dxgi_keyed_mutex_GetDevice(IDXGIKeyedMutex *iface, REFIID iid, void **device)
{
    return IDXGIResource1_GetDevice(&impl_from_IDXGIKeyedMutex(iface)->IDXGIResource1_iface, iid, device);
}

static HRESULT STDMETHODCALLTYPE dxgi_keyed_mutex_AcquireSync(IDXGIKeyedMutex *iface, UINT64 key, DWORD timeout)
{
    struct dxgi_resource *resource = impl_from_IDXGIKeyedMutex(iface);

    return wined3d_texture_acquire_keyed_mutex(wined3d_texture_from_resource(resource->wined3d_resource),
            resource->keyed_mutex, resource->shared_keyed_mutex, key, timeout);
}

static HRESULT STDMETHODCALLTYPE dxgi_keyed_mutex_ReleaseSync(IDXGIKeyedMutex *iface, UINT64 key)
{
    struct dxgi_resource *resource = impl_from_IDXGIKeyedMutex(iface);

    return wined3d_texture_release_keyed_mutex(wined3d_texture_from_resource(resource->wined3d_resource),
            resource->keyed_mutex, resource->shared_keyed_mutex, key);
}

static const struct IDXGIKeyedMutexVtbl dxgi_keyed_mutex_vtbl =
{
    dxgi_keyed_mutex_QueryInterface,
    dxgi_keyed_mutex_AddRef,
    dxgi_keyed_mutex_Release,
    dxgi_keyed_mutex_SetPrivateData,
    dxgi_keyed_mutex_SetPrivateDataInterface,
    dxgi_keyed_mutex_GetPrivateData,
    dxgi_keyed_mutex_GetParent,
    dxgi_keyed_mutex_GetDevice,
    dxgi_keyed_mutex_AcquireSync,
    dxgi_keyed_mutex_ReleaseSync,
};

static HRESULT dxgi_resource_init_keyed_mutex(struct dxgi_resource *resource, HANDLE shared_handle)
{
    D3DKMT_CREATEKEYEDMUTEX create = {0};
    D3DKMT_OPENKEYEDMUTEX open_mutex = {0};
    D3DKMT_OPENADAPTERFROMLUID adapter = {0};
    D3DKMT_CREATEDEVICE device = {{0}};
    D3DKMT_CLOSEADAPTER close_adapter = {0};
    D3DKMT_DESTROYDEVICE destroy_device = {0};
    D3DKMT_DESTROYALLOCATION destroy = {0};
    D3DKMT_QUERYRESOURCEINFO query = {0};
    D3DKMT_OPENRESOURCE open = {0};
    D3DDDI_OPENALLOCATIONINFO allocation = {0};
    struct dxgi_shared_texture_metadata metadata;
    IDXGIAdapter *dxgi_adapter;
    DXGI_ADAPTER_DESC desc;
    HRESULT hr = E_FAIL;

    if (!shared_handle)
    {
        if (D3DKMTCreateKeyedMutex(&create)) return E_FAIL;
        resource->keyed_mutex = create.hKeyedMutex;
        resource->shared_keyed_mutex = create.hSharedHandle;
        return S_OK;
    }
    if (FAILED(IDXGIDevice_GetAdapter(resource->device, &dxgi_adapter))) return E_FAIL;
    hr = IDXGIAdapter_GetDesc(dxgi_adapter, &desc);
    IDXGIAdapter_Release(dxgi_adapter);
    if (FAILED(hr)) return hr;
    hr = E_FAIL;
    adapter.AdapterLuid = desc.AdapterLuid;
    if (D3DKMTOpenAdapterFromLuid(&adapter)) goto done;
    close_adapter.hAdapter = adapter.hAdapter;
    device.hAdapter = adapter.hAdapter;
    if (D3DKMTCreateDevice(&device)) goto done;
    destroy_device.hDevice = device.hDevice;
    query.hDevice = device.hDevice;
    query.hGlobalShare = (D3DKMT_HANDLE)(ULONG_PTR)shared_handle;
    if (D3DKMTQueryResourceInfo(&query) || query.PrivateRuntimeDataSize != sizeof(metadata)) goto done;
    open.hDevice = device.hDevice;
    open.hGlobalShare = query.hGlobalShare;
    open.NumAllocations = 1;
    open.pOpenAllocationInfo = &allocation;
    open.pPrivateRuntimeData = &metadata;
    open.PrivateRuntimeDataSize = sizeof(metadata);
    if (D3DKMTOpenResource(&open)) goto done;
    destroy.hDevice = device.hDevice;
    destroy.hResource = open.hResource;
    if (metadata.size != sizeof(metadata) || metadata.version != 1 || !metadata.keyed_mutex
            || !metadata.mutex_handle || metadata.nt_shared) goto done;
    open_mutex.hSharedHandle = metadata.mutex_handle;
    if (D3DKMTOpenKeyedMutex(&open_mutex)) goto done;
    resource->keyed_mutex = open_mutex.hKeyedMutex;
    resource->shared_keyed_mutex = metadata.mutex_handle;
    hr = S_OK;
done:
    if (destroy.hResource) D3DKMTDestroyAllocation(&destroy);
    if (destroy_device.hDevice) D3DKMTDestroyDevice(&destroy_device);
    if (close_adapter.hAdapter) D3DKMTCloseAdapter(&close_adapter);
    return hr;
}

HRESULT dxgi_resource_init(struct dxgi_resource *resource, IDXGIDevice *device,
        IUnknown *outer, BOOL needs_surface, struct wined3d_resource *wined3d_resource,
        IDXGIResource1 *parent_resource, unsigned int subresource_index, const DXGI_SHARED_RESOURCE *shared_resource)
{
    struct wined3d_resource_desc desc;
    bool is_subresource;

    is_subresource = !!parent_resource;
    wined3d_resource_get_desc(wined3d_resource, &desc);
    if (((desc.resource_type == WINED3D_RTYPE_TEXTURE_1D || desc.resource_type == WINED3D_RTYPE_TEXTURE_2D)
            && needs_surface) || is_subresource)
    {
        resource->IDXGISurface2_iface.lpVtbl = &dxgi_surface_vtbl;
    }
    else
        resource->IDXGISurface2_iface.lpVtbl = NULL;
    resource->IDXGIResource1_iface.lpVtbl = &dxgi_resource_vtbl;
    resource->IUnknown_iface.lpVtbl = &dxgi_resource_inner_unknown_vtbl;
    resource->refcount = 1;
    wined3d_private_store_init(&resource->private_store);
    resource->outer_unknown = outer ? outer : &resource->IUnknown_iface;
    resource->device = device;
    resource->wined3d_resource = wined3d_resource;
    resource->dc = NULL;
    if (is_subresource)
    {
        resource->parent_resource = parent_resource;
        IDXGIResource1_AddRef(parent_resource);
        resource->subresource_idx = subresource_index;
    }
    else
    {
        resource->parent_resource = NULL;
        resource->subresource_idx = 0;
    }

    resource->IDXGIKeyedMutex_iface.lpVtbl = &dxgi_keyed_mutex_vtbl;
    resource->keyed_mutex = resource->shared_keyed_mutex = 0;
    if (!is_subresource && outer && desc.resource_type == WINED3D_RTYPE_TEXTURE_2D)
    {
        ID3D11Texture2D *texture;
        D3D11_TEXTURE2D_DESC texture_desc;
        HRESULT hr;

        if (SUCCEEDED(IUnknown_QueryInterface(outer, &IID_ID3D11Texture2D, (void **)&texture)))
        {
            ID3D11Texture2D_GetDesc(texture, &texture_desc);
            ID3D11Texture2D_Release(texture);
            if (texture_desc.MiscFlags & D3D11_RESOURCE_MISC_SHARED_KEYEDMUTEX)
            {
                if (FAILED(hr = dxgi_resource_init_keyed_mutex(resource,
                        shared_resource ? shared_resource->Handle : NULL))) return hr;
            }
        }
    }
    return S_OK;
}
