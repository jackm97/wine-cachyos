/*
 * Copyright (C) 2023 Mohamad Al-Jaf
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

#include <stdarg.h>

#define COBJMACROS
#include "initguid.h"
#include "dxcore.h"
#include "dxgi1_4.h"
#include "d3d11.h"
#include "d3d12.h"
#include "d3dkmdt.h"
#include "wine/wined3d.h"

#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(dxcore);

struct dxcore_adapter
{
    IDXCoreAdapter IDXCoreAdapter_iface;
    LONG refcount;

    struct wined3d_adapter_identifier identifier;
    char driver_description[128];
};

static inline struct dxcore_adapter *impl_from_IDXCoreAdapter(IDXCoreAdapter *iface)
{
    return CONTAINING_RECORD(iface, struct dxcore_adapter, IDXCoreAdapter_iface);
}

static HRESULT STDMETHODCALLTYPE dxcore_adapter_QueryInterface(IDXCoreAdapter *iface, REFIID riid, void **out)
{
    struct dxcore_adapter *adapter = impl_from_IDXCoreAdapter(iface);

    TRACE("iface %p, riid %s, out %p.\n", iface, debugstr_guid(riid), out);

    if (IsEqualGUID(riid, &IID_IDXCoreAdapter)
            || IsEqualGUID(riid, &IID_IUnknown))
    {
        *out = &adapter->IDXCoreAdapter_iface;
        IUnknown_AddRef((IUnknown *)*out);
        return S_OK;
    }

    FIXME("%s not implemented, returning E_NOINTERFACE.\n", debugstr_guid(riid));
    *out = NULL;
    return E_NOINTERFACE;
}

static ULONG STDMETHODCALLTYPE dxcore_adapter_AddRef(IDXCoreAdapter *iface)
{
    struct dxcore_adapter *adapter = impl_from_IDXCoreAdapter(iface);
    ULONG refcount = InterlockedIncrement(&adapter->refcount);

    TRACE("%p increasing refcount to %lu.\n", iface, refcount);

    return refcount;
}

static ULONG STDMETHODCALLTYPE dxcore_adapter_Release(IDXCoreAdapter *iface)
{
    struct dxcore_adapter *adapter = impl_from_IDXCoreAdapter(iface);
    ULONG refcount = InterlockedDecrement(&adapter->refcount);

    TRACE("%p decreasing refcount to %lu.\n", iface, refcount);

    if (!refcount)
        free(adapter);

    return refcount;
}

enum dxcore_attribute
{
    DXCORE_ATTRIBUTE_D3D12_GRAPHICS,
    DXCORE_ATTRIBUTE_D3D12_CORE_COMPUTE,
    DXCORE_ATTRIBUTE_D3D11_GRAPHICS,
};

/* Adapter objects are recreated on every CreateAdapterList(), so probe results
 * are cached process-wide, keyed by adapter LUID and attribute. */
#define DXCORE_ATTRIBUTE_CACHE_SIZE 64

static struct dxcore_attribute_cache_entry
{
    LUID luid;
    enum dxcore_attribute attribute;
    BOOL supported;
} dxcore_attribute_cache[DXCORE_ATTRIBUTE_CACHE_SIZE];
static unsigned int dxcore_attribute_cache_count;
static SRWLOCK dxcore_attribute_cache_lock = SRWLOCK_INIT;

static BOOL dxcore_attribute_cache_lookup(const LUID *luid, enum dxcore_attribute attribute, BOOL *supported)
{
    BOOL found = FALSE;
    unsigned int i;

    AcquireSRWLockShared(&dxcore_attribute_cache_lock);
    for (i = 0; i < dxcore_attribute_cache_count; ++i)
    {
        const struct dxcore_attribute_cache_entry *entry = &dxcore_attribute_cache[i];

        if (entry->attribute == attribute && !memcmp(&entry->luid, luid, sizeof(*luid)))
        {
            *supported = entry->supported;
            found = TRUE;
            break;
        }
    }
    ReleaseSRWLockShared(&dxcore_attribute_cache_lock);

    return found;
}

static void dxcore_attribute_cache_store(const LUID *luid, enum dxcore_attribute attribute, BOOL supported)
{
    unsigned int i;

    AcquireSRWLockExclusive(&dxcore_attribute_cache_lock);
    for (i = 0; i < dxcore_attribute_cache_count; ++i)
    {
        const struct dxcore_attribute_cache_entry *entry = &dxcore_attribute_cache[i];

        /* Another thread may have stored the result while we were probing. */
        if (entry->attribute == attribute && !memcmp(&entry->luid, luid, sizeof(*luid)))
            goto done;
    }

    if (dxcore_attribute_cache_count < ARRAY_SIZE(dxcore_attribute_cache))
    {
        struct dxcore_attribute_cache_entry *entry = &dxcore_attribute_cache[dxcore_attribute_cache_count++];

        entry->luid = *luid;
        entry->attribute = attribute;
        entry->supported = supported;
    }
    else
    {
        WARN("Attribute cache is full, not caching attribute %u.\n", attribute);
    }

done:
    ReleaseSRWLockExclusive(&dxcore_attribute_cache_lock);
}

/* Memory reservations set through SetState(AdapterMemoryBudget). Adapter objects
 * are recreated on every CreateAdapterList(), so the reservations are kept
 * process-wide, keyed by adapter LUID, node index and segment group. */
#define DXCORE_RESERVATION_TABLE_SIZE 16

static struct dxcore_reservation_entry
{
    LUID luid;
    UINT node_index;
    DXGI_MEMORY_SEGMENT_GROUP group;
    UINT64 reservation;
} dxcore_reservation_table[DXCORE_RESERVATION_TABLE_SIZE];
static unsigned int dxcore_reservation_count;
static SRWLOCK dxcore_reservation_lock = SRWLOCK_INIT;

static UINT64 dxcore_reservation_lookup(const LUID *luid, UINT node_index, DXGI_MEMORY_SEGMENT_GROUP group)
{
    UINT64 reservation = 0;
    unsigned int i;

    AcquireSRWLockShared(&dxcore_reservation_lock);
    for (i = 0; i < dxcore_reservation_count; ++i)
    {
        const struct dxcore_reservation_entry *entry = &dxcore_reservation_table[i];

        if (entry->node_index == node_index && entry->group == group && !memcmp(&entry->luid, luid, sizeof(*luid)))
        {
            reservation = entry->reservation;
            break;
        }
    }
    ReleaseSRWLockShared(&dxcore_reservation_lock);

    return reservation;
}

/* Returns FALSE if the table is full and the key is new. */
static BOOL dxcore_reservation_store(const LUID *luid, UINT node_index, DXGI_MEMORY_SEGMENT_GROUP group, UINT64 reservation)
{
    BOOL stored = TRUE;
    unsigned int i;

    AcquireSRWLockExclusive(&dxcore_reservation_lock);
    for (i = 0; i < dxcore_reservation_count; ++i)
    {
        struct dxcore_reservation_entry *entry = &dxcore_reservation_table[i];

        if (entry->node_index == node_index && entry->group == group && !memcmp(&entry->luid, luid, sizeof(*luid)))
        {
            entry->reservation = reservation;
            goto done;
        }
    }

    if (dxcore_reservation_count < ARRAY_SIZE(dxcore_reservation_table))
    {
        struct dxcore_reservation_entry *entry = &dxcore_reservation_table[dxcore_reservation_count++];

        entry->luid = *luid;
        entry->node_index = node_index;
        entry->group = group;
        entry->reservation = reservation;
    }
    else
    {
        WARN("Reservation table is full, not storing the reservation.\n");
        stored = FALSE;
    }

done:
    ReleaseSRWLockExclusive(&dxcore_reservation_lock);
    return stored;
}

/* The d3d12, d3d11 and dxgi DLLs are loaded by name at first use, rather than
 * imported, so that whichever DLL the prefix overrides (vkd3d-proton, DXVK) is
 * the one used. They are never freed. */
static HRESULT dxcore_probe_d3d12(IDXCoreAdapter *iface, D3D_FEATURE_LEVEL feature_level)
{
    PFN_D3D12_CREATE_DEVICE d3d12_create_device;
    HMODULE d3d12;

    if (!(d3d12 = LoadLibraryW(L"d3d12.dll")))
    {
        WARN("Failed to load d3d12.dll, error %lu.\n", GetLastError());
        return E_FAIL;
    }

    if (!(d3d12_create_device = (void *)GetProcAddress(d3d12, "D3D12CreateDevice")))
    {
        WARN("Failed to get D3D12CreateDevice, error %lu.\n", GetLastError());
        return E_FAIL;
    }

    /* Passing a NULL device only checks whether the device could be created. */
    return d3d12_create_device((IUnknown *)iface, feature_level, &IID_ID3D12Device, NULL);
}

typedef HRESULT (WINAPI *dxcore_create_dxgi_factory1)(REFIID riid, void **factory);

/* Gets the DXGI adapter with the given LUID, as the requested interface. */
static HRESULT dxcore_get_dxgi_adapter(const LUID *luid, REFIID iid, void **out)
{
    dxcore_create_dxgi_factory1 create_dxgi_factory1;
    IDXGIFactory4 *factory;
    HMODULE dxgi;
    HRESULT hr;

    if (!(dxgi = LoadLibraryW(L"dxgi.dll")))
    {
        WARN("Failed to load dxgi.dll, error %lu.\n", GetLastError());
        return E_FAIL;
    }

    if (!(create_dxgi_factory1 = (void *)GetProcAddress(dxgi, "CreateDXGIFactory1")))
    {
        WARN("Failed to get CreateDXGIFactory1, error %lu.\n", GetLastError());
        return E_FAIL;
    }

    if (FAILED(hr = create_dxgi_factory1(&IID_IDXGIFactory4, (void **)&factory)))
    {
        WARN("Failed to create DXGI factory, hr %#lx.\n", hr);
        return hr;
    }

    hr = IDXGIFactory4_EnumAdapterByLuid(factory, *luid, iid, out);
    IDXGIFactory4_Release(factory);
    if (FAILED(hr))
        WARN("Failed to get DXGI adapter for LUID %08lx:%08lx, hr %#lx.\n", luid->HighPart, luid->LowPart, hr);

    return hr;
}

static HRESULT dxcore_probe_d3d11(const LUID *luid)
{
    PFN_D3D11_CREATE_DEVICE d3d11_create_device;
    IDXGIAdapter *dxgi_adapter;
    HMODULE d3d11;
    HRESULT hr;

    if (!(d3d11 = LoadLibraryW(L"d3d11.dll")))
    {
        WARN("Failed to load d3d11.dll, error %lu.\n", GetLastError());
        return E_FAIL;
    }

    if (!(d3d11_create_device = (void *)GetProcAddress(d3d11, "D3D11CreateDevice")))
    {
        WARN("Failed to get D3D11CreateDevice, error %lu.\n", GetLastError());
        return E_FAIL;
    }

    if (FAILED(hr = dxcore_get_dxgi_adapter(luid, &IID_IDXGIAdapter, (void **)&dxgi_adapter)))
        return hr;

    /* All output parameters are NULL, so no device is created. */
    hr = d3d11_create_device(dxgi_adapter, D3D_DRIVER_TYPE_UNKNOWN, NULL, 0, NULL, 0,
            D3D11_SDK_VERSION, NULL, NULL, NULL);
    IDXGIAdapter_Release(dxgi_adapter);
    return hr;
}

static BOOL STDMETHODCALLTYPE dxcore_adapter_IsValid(IDXCoreAdapter *iface)
{
    WARN("iface %p, adapter removal is not modelled, returning TRUE.\n", iface);
    return TRUE;
}

/* Returns whether the attribute is supported, probing at most once per adapter LUID and attribute. */
static BOOL dxcore_adapter_probe_attribute(IDXCoreAdapter *iface, const LUID *luid,
        enum dxcore_attribute id, REFGUID attribute)
{
    BOOL supported;
    HRESULT hr;

    if (dxcore_attribute_cache_lookup(luid, id, &supported))
    {
        TRACE("attribute %s, LUID %08lx:%08lx, cached result %d.\n", debugstr_guid(attribute),
                luid->HighPart, luid->LowPart, supported);
        return supported;
    }

    /* Probe without holding the cache lock; the probes call into other DLLs. */
    switch (id)
    {
        case DXCORE_ATTRIBUTE_D3D12_GRAPHICS:
            hr = dxcore_probe_d3d12(iface, D3D_FEATURE_LEVEL_11_0);
            supported = hr == S_OK || hr == S_FALSE;
            break;

        case DXCORE_ATTRIBUTE_D3D12_CORE_COMPUTE:
            hr = dxcore_probe_d3d12(iface, D3D_FEATURE_LEVEL_1_0_CORE);
            supported = hr == S_OK || hr == S_FALSE;
            break;

        case DXCORE_ATTRIBUTE_D3D11_GRAPHICS:
            hr = dxcore_probe_d3d11(luid);
            supported = SUCCEEDED(hr);
            break;
    }

    TRACE("attribute %s, LUID %08lx:%08lx, hr %#lx, supported %d.\n", debugstr_guid(attribute),
            luid->HighPart, luid->LowPart, hr, supported);

    dxcore_attribute_cache_store(luid, id, supported);
    return supported;
}

static BOOL STDMETHODCALLTYPE dxcore_adapter_IsAttributeSupported(IDXCoreAdapter *iface, REFGUID attribute)
{
    struct dxcore_adapter *adapter = impl_from_IDXCoreAdapter(iface);
    const LUID *luid = &adapter->identifier.adapter_luid;

    TRACE("iface %p, attribute %s.\n", iface, debugstr_guid(attribute));

    if (IsEqualGUID(attribute, &DXCORE_ADAPTER_ATTRIBUTE_D3D12_GRAPHICS))
        return dxcore_adapter_probe_attribute(iface, luid, DXCORE_ATTRIBUTE_D3D12_GRAPHICS, attribute);

    if (IsEqualGUID(attribute, &DXCORE_ADAPTER_ATTRIBUTE_D3D11_GRAPHICS))
        return dxcore_adapter_probe_attribute(iface, luid, DXCORE_ATTRIBUTE_D3D11_GRAPHICS, attribute);

    if (IsEqualGUID(attribute, &DXCORE_ADAPTER_ATTRIBUTE_D3D12_CORE_COMPUTE))
    {
        /* On Windows, any D3D12 graphics adapter is also a core compute adapter. */
        if (dxcore_adapter_probe_attribute(iface, luid, DXCORE_ATTRIBUTE_D3D12_GRAPHICS,
                &DXCORE_ADAPTER_ATTRIBUTE_D3D12_GRAPHICS))
        {
            TRACE("Adapter supports D3D12 graphics, so it supports core compute.\n");
            return TRUE;
        }
        return dxcore_adapter_probe_attribute(iface, luid, DXCORE_ATTRIBUTE_D3D12_CORE_COMPUTE, attribute);
    }

    /* Every adapter wined3d enumerates is a GPU. */
    if (IsEqualGUID(attribute, &DXCORE_HARDWARE_TYPE_ATTRIBUTE_GPU))
        return TRUE;

    /* Exactly one hardware type applies to an adapter, and ours are all GPUs. */
    if (IsEqualGUID(attribute, &DXCORE_HARDWARE_TYPE_ATTRIBUTE_NPU)
            || IsEqualGUID(attribute, &DXCORE_HARDWARE_TYPE_ATTRIBUTE_COMPUTE_ACCELERATOR)
            || IsEqualGUID(attribute, &DXCORE_HARDWARE_TYPE_ATTRIBUTE_MEDIA_ACCELERATOR))
    {
        TRACE("Hardware type %s does not apply, returning FALSE.\n", debugstr_guid(attribute));
        return FALSE;
    }

    /* These are driver INF declarations of metacommand or video support, which
     * vkd3d-proton does not provide. */
    if (IsEqualGUID(attribute, &DXCORE_ADAPTER_ATTRIBUTE_D3D12_GENERIC_ML)
            || IsEqualGUID(attribute, &DXCORE_ADAPTER_ATTRIBUTE_D3D12_GENERIC_MEDIA))
    {
        TRACE("Attribute %s is not provided, returning FALSE.\n", debugstr_guid(attribute));
        return FALSE;
    }

    FIXME("Unhandled attribute %s.\n", debugstr_guid(attribute));
    return FALSE;
}

/* The single source of truth for which properties are supported and how big they are;
 * both IsPropertySupported() and GetPropertySize() use it. AdapterEngineCount and
 * AdapterEngineName are not supported. */
static BOOL dxcore_adapter_query_property_size(const struct dxcore_adapter *adapter,
        DXCoreAdapterProperty property, size_t *size)
{
    static const size_t property_sizes[] =
    {
        [InstanceLuid] = sizeof(LUID),
        [DriverVersion] = sizeof(LARGE_INTEGER),
        [HardwareID] = sizeof(DXCoreHardwareID),
        [KmdModelVersion] = sizeof(UINT),
        [ComputePreemptionGranularity] = sizeof(UINT16),
        [GraphicsPreemptionGranularity] = sizeof(UINT16),
        [DedicatedAdapterMemory] = sizeof(UINT64),
        [DedicatedSystemMemory] = sizeof(UINT64),
        [SharedSystemMemory] = sizeof(UINT64),
        [AcgCompatible] = sizeof(BYTE),
        [IsHardware] = sizeof(BYTE),
        [IsIntegrated] = sizeof(BYTE),
        [IsDetachable] = sizeof(BYTE),
        [HardwareIDParts] = sizeof(DXCoreHardwareIDParts),
        [PhysicalAdapterCount] = sizeof(UINT),
    };

    switch (property)
    {
        case InstanceLuid:
        case DriverVersion:
        case HardwareID:
        case KmdModelVersion:
        case ComputePreemptionGranularity:
        case GraphicsPreemptionGranularity:
        case DedicatedAdapterMemory:
        case DedicatedSystemMemory:
        case SharedSystemMemory:
        case AcgCompatible:
        case IsHardware:
        case IsIntegrated:
        case IsDetachable:
        case HardwareIDParts:
        case PhysicalAdapterCount:
            *size = property_sizes[property];
            return TRUE;

        case DriverDescription:
            *size = strlen(adapter->driver_description) + 1;
            return TRUE;

        default:
            return FALSE;
    }
}

static BOOL STDMETHODCALLTYPE dxcore_adapter_IsPropertySupported(IDXCoreAdapter *iface, DXCoreAdapterProperty property)
{
    struct dxcore_adapter *adapter = impl_from_IDXCoreAdapter(iface);
    size_t size;

    TRACE("iface %p, property %u.\n", iface, property);

    return dxcore_adapter_query_property_size(adapter, property, &size);
}

static HRESULT dxcore_adapter_get_property_size(struct dxcore_adapter *adapter,
        DXCoreAdapterProperty property, size_t *size)
{
    if (dxcore_adapter_query_property_size(adapter, property, size))
        return S_OK;

    /* Windows returns DXGI_ERROR_INVALID_CALL for an unsupported property. */
    FIXME("Property %u not implemented.\n", property);
    return DXGI_ERROR_INVALID_CALL;
}

static HRESULT STDMETHODCALLTYPE dxcore_adapter_GetProperty(IDXCoreAdapter *iface, DXCoreAdapterProperty property,
        size_t buffer_size, void *buffer)
{
    struct dxcore_adapter *adapter = impl_from_IDXCoreAdapter(iface);
    size_t size;
    HRESULT hr;

    TRACE("iface %p, property %u, buffer_size %Iu, buffer %p\n", iface, property, buffer_size, buffer);

    if (!buffer)
        return E_POINTER;

    if (FAILED(hr = dxcore_adapter_get_property_size(adapter, property, &size)))
        return hr;

    if (property == DriverDescription && buffer && buffer_size)
        *(char *)buffer = 0;

    if (buffer_size < size)
        return E_INVALIDARG;

    switch (property)
    {
        case InstanceLuid:
            *(LUID *)buffer = adapter->identifier.adapter_luid;
            break;

        case DriverVersion:
        {
            *(LARGE_INTEGER *)buffer = adapter->identifier.driver_version;
            break;
        }

        case HardwareID:
        {
            struct DXCoreHardwareID *hardware_id = buffer;

            hardware_id->vendorID = adapter->identifier.vendor_id;
            hardware_id->deviceID = adapter->identifier.device_id;
            hardware_id->subSysID = adapter->identifier.subsystem_id;
            hardware_id->revision = adapter->identifier.revision;
            break;
        }

        case KmdModelVersion:
            /* KMT_DRIVERVERSION_WDDM_3_2, the WDDM version of Windows 11 24H2. */
            *(UINT *)buffer = 3200;
            break;

        case ComputePreemptionGranularity:
            *(UINT16 *)buffer = D3DKMDT_COMPUTE_PREEMPTION_DMA_BUFFER_BOUNDARY;
            break;

        case GraphicsPreemptionGranularity:
            *(UINT16 *)buffer = D3DKMDT_GRAPHICS_PREEMPTION_DMA_BUFFER_BOUNDARY;
            break;

        case DedicatedAdapterMemory:
            *(UINT64 *)buffer = adapter->identifier.video_memory;
            break;

        case DedicatedSystemMemory:
            FIXME("Returning 0 for DedicatedSystemMemory.\n");
            *(UINT64 *)buffer = 0;
            break;

        case SharedSystemMemory:
            *(UINT64 *)buffer = adapter->identifier.shared_system_memory;
            break;

        case AcgCompatible:
            *(BYTE *)buffer = 1;
            break;

        case IsHardware:
            FIXME("Returning all adapters as Hardware.\n");
            *(BYTE *)buffer = 1;
            break;

        case IsIntegrated:
            FIXME("Returning all adapters as discrete.\n");
            *(BYTE *)buffer = 0;
            break;

        case IsDetachable:
            *(BYTE *)buffer = 0;
            break;

        case HardwareIDParts:
        {
            DXCoreHardwareIDParts *parts = buffer;

            parts->vendorID = adapter->identifier.vendor_id;
            parts->deviceID = adapter->identifier.device_id;
            parts->subSystemID = adapter->identifier.subsystem_id >> 16;
            parts->subVendorID = adapter->identifier.subsystem_id & 0xffff;
            parts->revisionID = adapter->identifier.revision;
            break;
        }

        case PhysicalAdapterCount:
            *(UINT *)buffer = 1;
            break;

        case DriverDescription:
        {
            memcpy(buffer, adapter->identifier.description, size);
            break;
        }

        default:
            break;
    }

    return S_OK;
}

static HRESULT STDMETHODCALLTYPE dxcore_adapter_GetPropertySize(IDXCoreAdapter *iface, DXCoreAdapterProperty property,
        size_t *buffer_size)
{
    struct dxcore_adapter *adapter = impl_from_IDXCoreAdapter(iface);

    TRACE("iface %p, property %u, buffer_size %p.\n", iface, property, buffer_size);

    if (!buffer_size)
        return E_POINTER;

    return dxcore_adapter_get_property_size(adapter, property, buffer_size);
}

static BOOL STDMETHODCALLTYPE dxcore_adapter_IsQueryStateSupported(IDXCoreAdapter *iface, DXCoreAdapterState property)
{
    TRACE("iface %p, property %u.\n", iface, property);
    return property == AdapterMemoryBudget;
}

static HRESULT STDMETHODCALLTYPE dxcore_adapter_QueryState(IDXCoreAdapter *iface, DXCoreAdapterState state, size_t state_details_size,
        const void *state_details, size_t buffer_size, void *buffer)
{
    struct dxcore_adapter *adapter = impl_from_IDXCoreAdapter(iface);

    if (state == AdapterMemoryBudget)
    {
        const DXCoreAdapterMemoryBudgetNodeSegmentGroup *input = state_details;
        DXCoreAdapterMemoryBudget *budget = buffer;
        DXGI_QUERY_VIDEO_MEMORY_INFO info;
        DXGI_MEMORY_SEGMENT_GROUP group;
        IDXGIAdapter3 *dxgi_adapter;
        HRESULT hr;

        TRACE("iface %p, state %u, state_details_size %Iu, state_details %p, buffer_size %Iu, buffer %p.\n",
                iface, state, state_details_size, state_details, buffer_size, buffer);

        if (!state_details || state_details_size != sizeof(*input) || !buffer || buffer_size < sizeof(*budget))
            return E_INVALIDARG;

        switch (input->segmentGroup)
        {
            case Local:
                group = DXGI_MEMORY_SEGMENT_GROUP_LOCAL;
                break;

            case NonLocal:
                group = DXGI_MEMORY_SEGMENT_GROUP_NON_LOCAL;
                break;

            default:
                return E_INVALIDARG;
        }

        if (FAILED(hr = dxcore_get_dxgi_adapter(&adapter->identifier.adapter_luid, &IID_IDXGIAdapter3,
                (void **)&dxgi_adapter)))
            return hr;

        hr = IDXGIAdapter3_QueryVideoMemoryInfo(dxgi_adapter, input->nodeIndex, group, &info);
        IDXGIAdapter3_Release(dxgi_adapter);
        if (FAILED(hr))
            return hr;

        budget->budget = info.Budget;
        budget->currentUsage = info.CurrentUsage;
        budget->availableForReservation = info.AvailableForReservation;
        budget->currentReservation = dxcore_reservation_lookup(&adapter->identifier.adapter_luid, input->nodeIndex, group);

        TRACE("node %u, group %u, budget %I64u, usage %I64u, available for reservation %I64u, reservation %I64u.\n",
                input->nodeIndex, input->segmentGroup, budget->budget, budget->currentUsage,
                budget->availableForReservation, budget->currentReservation);
        return S_OK;
    }

    FIXME("iface %p, state %u, state_details_size %Iu, state_details %p, buffer_size %Iu, buffer %p stub!\n",
            iface, state, state_details_size, state_details, buffer_size, buffer);
    return E_NOTIMPL;
}

static BOOL STDMETHODCALLTYPE dxcore_adapter_IsSetStateSupported(IDXCoreAdapter *iface, DXCoreAdapterState property)
{
    TRACE("iface %p, property %u.\n", iface, property);
    return property == AdapterMemoryBudget;
}

static HRESULT STDMETHODCALLTYPE dxcore_adapter_SetState(IDXCoreAdapter *iface, DXCoreAdapterState state, size_t state_details_size,
        const void *state_details, size_t buffer_size, const void *buffer)
{
    struct dxcore_adapter *adapter = impl_from_IDXCoreAdapter(iface);

    if (state == AdapterMemoryBudget)
    {
        const DXCoreAdapterMemoryBudgetNodeSegmentGroup *input = state_details;
        DXGI_QUERY_VIDEO_MEMORY_INFO info;
        DXGI_MEMORY_SEGMENT_GROUP group;
        IDXGIAdapter3 *dxgi_adapter;
        UINT64 reservation;
        HRESULT hr;

        TRACE("iface %p, state %u, state_details_size %Iu, state_details %p, buffer_size %Iu, buffer %p.\n",
                iface, state, state_details_size, state_details, buffer_size, buffer);

        if (!state_details || !buffer)
            return E_POINTER;

        if (state_details_size < sizeof(*input) || buffer_size < sizeof(UINT64))
            return E_INVALIDARG;

        switch (input->segmentGroup)
        {
            case Local:
                group = DXGI_MEMORY_SEGMENT_GROUP_LOCAL;
                break;

            case NonLocal:
                group = DXGI_MEMORY_SEGMENT_GROUP_NON_LOCAL;
                break;

            default:
                return E_INVALIDARG;
        }

        reservation = *(const UINT64 *)buffer;

        if (FAILED(hr = dxcore_get_dxgi_adapter(&adapter->identifier.adapter_luid, &IID_IDXGIAdapter3,
                (void **)&dxgi_adapter)))
            return hr;

        hr = IDXGIAdapter3_QueryVideoMemoryInfo(dxgi_adapter, input->nodeIndex, group, &info);
        IDXGIAdapter3_Release(dxgi_adapter);
        if (FAILED(hr))
            return hr;

        /* Like IDXGIAdapter3::SetVideoMemoryReservation(). */
        if (reservation > info.AvailableForReservation)
            hr = DXGI_ERROR_INVALID_CALL;
        else if (!dxcore_reservation_store(&adapter->identifier.adapter_luid, input->nodeIndex, group, reservation))
            hr = E_OUTOFMEMORY;

        TRACE("node %u, group %u, requested %I64u, available for reservation %I64u, hr %#lx.\n",
                input->nodeIndex, input->segmentGroup, reservation, info.AvailableForReservation, hr);
        return hr;
    }

    FIXME("iface %p, state %u, state_details_size %Iu, state_details %p, buffer_size %Iu, buffer %p stub!\n",
            iface, state, state_details_size, state_details, buffer_size, buffer);
    return E_NOTIMPL;
}

static HRESULT STDMETHODCALLTYPE dxcore_adapter_GetFactory(IDXCoreAdapter *iface, REFIID riid, void **out)
{
    FIXME("iface %p, riid %s, out %p stub!\n", iface, debugstr_guid(riid), out);
    return E_NOTIMPL;
}

static const struct IDXCoreAdapterVtbl dxcore_adapter_vtbl =
{
    /* IUnknown methods */
    dxcore_adapter_QueryInterface,
    dxcore_adapter_AddRef,
    dxcore_adapter_Release,
    /* IDXCoreAdapter methods */
    dxcore_adapter_IsValid,
    dxcore_adapter_IsAttributeSupported,
    dxcore_adapter_IsPropertySupported,
    dxcore_adapter_GetProperty,
    dxcore_adapter_GetPropertySize,
    dxcore_adapter_IsQueryStateSupported,
    dxcore_adapter_QueryState,
    dxcore_adapter_IsSetStateSupported,
    dxcore_adapter_SetState,
    dxcore_adapter_GetFactory,
};

struct dxcore_adapter_list
{
    IDXCoreAdapterList IDXCoreAdapterList_iface;
    LONG refcount;

    struct dxcore_adapter **adapters;
    uint32_t adapter_count;
};

static inline struct dxcore_adapter_list *impl_from_IDXCoreAdapterList(IDXCoreAdapterList *iface)
{
    return CONTAINING_RECORD(iface, struct dxcore_adapter_list, IDXCoreAdapterList_iface);
}

static HRESULT STDMETHODCALLTYPE dxcore_adapter_list_QueryInterface(IDXCoreAdapterList *iface, REFIID riid, void **out)
{
    struct dxcore_adapter_list *list = impl_from_IDXCoreAdapterList(iface);

    TRACE("iface %p, riid %s, out %p.\n", iface, debugstr_guid(riid), out);

    if (IsEqualGUID(riid, &IID_IDXCoreAdapterList)
            || IsEqualGUID(riid, &IID_IUnknown))
    {
        *out = &list->IDXCoreAdapterList_iface;
        IUnknown_AddRef((IUnknown *)*out);
        return S_OK;
    }

    FIXME("%s not implemented, returning E_NOINTERFACE.\n", debugstr_guid(riid));
    *out = NULL;
    return E_NOINTERFACE;
}

static ULONG STDMETHODCALLTYPE dxcore_adapter_list_AddRef(IDXCoreAdapterList *iface)
{
    struct dxcore_adapter_list *list = impl_from_IDXCoreAdapterList(iface);
    ULONG refcount = InterlockedIncrement(&list->refcount);

    TRACE("%p increasing refcount to %lu.\n", iface, refcount);

    return refcount;
}

static ULONG STDMETHODCALLTYPE dxcore_adapter_list_Release(IDXCoreAdapterList *iface)
{
    struct dxcore_adapter_list *list = impl_from_IDXCoreAdapterList(iface);
    ULONG refcount = InterlockedDecrement(&list->refcount);

    TRACE("%p decreasing refcount to %lu.\n", iface, refcount);

    if (!refcount)
    {
        for (UINT i = 0; i < list->adapter_count; i++)
        {
            if (list->adapters[i])
                IDXCoreAdapter_Release(&list->adapters[i]->IDXCoreAdapter_iface);
        }
        free(list->adapters);
        free(list);
    }
    return refcount;
}

static HRESULT STDMETHODCALLTYPE dxcore_adapter_list_GetAdapter(IDXCoreAdapterList *iface, uint32_t index,
        REFIID riid, void **out)
{
    struct dxcore_adapter_list *list = impl_from_IDXCoreAdapterList(iface);
    IDXCoreAdapter *adapter;

    TRACE("iface %p, index %u, riid %s, out %p\n", iface, index, debugstr_guid(riid), out);

    if (!out)
        return E_POINTER;

    if (index >= list->adapter_count)
    {
        *out = NULL;
        return E_INVALIDARG;
    }

    adapter = &list->adapters[index]->IDXCoreAdapter_iface;
    TRACE("returning IDXCoreAdapter %p for index %u.\n", adapter, index);
    return IDXCoreAdapter_QueryInterface(adapter, riid, out);
}

static uint32_t STDMETHODCALLTYPE dxcore_adapter_list_GetAdapterCount(IDXCoreAdapterList *iface)
{
    struct dxcore_adapter_list *list = impl_from_IDXCoreAdapterList(iface);

    TRACE("iface %p\n", iface);

    return list->adapter_count;
}

static BOOL STDMETHODCALLTYPE dxcore_adapter_list_IsStale(IDXCoreAdapterList *iface)
{
    FIXME("iface %p stub!\n", iface);
    return FALSE;
}

static HRESULT STDMETHODCALLTYPE dxcore_adapter_list_GetFactory(IDXCoreAdapterList *iface, REFIID riid, void **out)
{
    FIXME("iface %p, riid %s, out %p stub!\n", iface, debugstr_guid(riid), out);
    return E_NOTIMPL;
}

static BOOL dxcore_adapter_preference_is_supported(DXCoreAdapterPreference preference)
{
    switch (preference)
    {
        case Hardware:
        case MinimumPower:
        case HighPerformance:
            return TRUE;

        default:
            return FALSE;
    }
}

/* Returns a negative value if a sorts before b, positive if after, 0 if the
 * preferences cannot tell them apart. The preferences are criteria in priority order. */
static int dxcore_adapter_compare(const struct dxcore_adapter *a, const struct dxcore_adapter *b,
        uint32_t num_preferences, const DXCoreAdapterPreference *preferences)
{
    for (uint32_t i = 0; i < num_preferences; ++i)
    {
        UINT64 a_memory = a->identifier.video_memory;
        UINT64 b_memory = b->identifier.video_memory;

        switch (preferences[i])
        {
            case Hardware:
                /* All adapters are reported as hardware, see IsHardware in GetProperty(). */
                break;

            case HighPerformance:
                if (a_memory != b_memory)
                    return a_memory > b_memory ? -1 : 1;
                break;

            case MinimumPower:
                if (a_memory != b_memory)
                    return a_memory < b_memory ? -1 : 1;
                break;

            default:
                break;
        }
    }

    return 0;
}

static HRESULT STDMETHODCALLTYPE dxcore_adapter_list_Sort(IDXCoreAdapterList *iface, uint32_t num_preferences,
        const DXCoreAdapterPreference *preferences)
{
    struct dxcore_adapter_list *list = impl_from_IDXCoreAdapterList(iface);
    uint32_t i, j;

    TRACE("iface %p, num_preferences %u, preferences %p.\n", iface, num_preferences, preferences);

    if (num_preferences && !preferences)
        return E_INVALIDARG;

    for (i = 0; i < num_preferences; ++i)
    {
        if (!dxcore_adapter_preference_is_supported(preferences[i]))
        {
            WARN("Unsupported preference %u.\n", preferences[i]);
            return E_INVALIDARG;
        }
    }

    /* Insertion sort, which is stable. */
    for (i = 1; i < list->adapter_count; ++i)
    {
        struct dxcore_adapter *adapter = list->adapters[i];

        for (j = i; j > 0 && dxcore_adapter_compare(list->adapters[j - 1], adapter, num_preferences, preferences) > 0; --j)
            list->adapters[j] = list->adapters[j - 1];
        list->adapters[j] = adapter;
    }

    return S_OK;
}

static BOOL STDMETHODCALLTYPE dxcore_adapter_list_IsAdapterPreferenceSupported(IDXCoreAdapterList *iface,
        DXCoreAdapterPreference preference)
{
    TRACE("iface %p, preference %u.\n", iface, preference);

    return dxcore_adapter_preference_is_supported(preference);
}

static const struct IDXCoreAdapterListVtbl dxcore_adapter_list_vtbl =
{
    /* IUnknown methods */
    dxcore_adapter_list_QueryInterface,
    dxcore_adapter_list_AddRef,
    dxcore_adapter_list_Release,
    /* IDXCoreAdapterList methods */
    dxcore_adapter_list_GetAdapter,
    dxcore_adapter_list_GetAdapterCount,
    dxcore_adapter_list_IsStale,
    dxcore_adapter_list_GetFactory,
    dxcore_adapter_list_Sort,
    dxcore_adapter_list_IsAdapterPreferenceSupported,
};

struct dxcore_adapter_factory
{
    IDXCoreAdapterFactory IDXCoreAdapterFactory_iface;
    LONG refcount;
};

static inline struct dxcore_adapter_factory *impl_from_IDXCoreAdapterFactory(IDXCoreAdapterFactory *iface)
{
    return CONTAINING_RECORD(iface, struct dxcore_adapter_factory, IDXCoreAdapterFactory_iface);
}

static HRESULT STDMETHODCALLTYPE dxcore_adapter_factory_QueryInterface(IDXCoreAdapterFactory *iface, REFIID riid, void **out)
{
    struct dxcore_adapter_factory *factory = impl_from_IDXCoreAdapterFactory(iface);

    TRACE("iface %p, riid %s, out %p.\n", iface, debugstr_guid(riid), out);

    if (IsEqualGUID(riid, &IID_IDXCoreAdapterFactory)
            || IsEqualGUID(riid, &IID_IUnknown))
    {
        *out = &factory->IDXCoreAdapterFactory_iface;
        IUnknown_AddRef((IUnknown *)*out);
        return S_OK;
    }

    FIXME("%s not implemented, returning E_NOINTERFACE.\n", debugstr_guid(riid));
    *out = NULL;
    return E_NOINTERFACE;
}

static ULONG STDMETHODCALLTYPE dxcore_adapter_factory_AddRef(IDXCoreAdapterFactory *iface)
{
    struct dxcore_adapter_factory *factory = impl_from_IDXCoreAdapterFactory(iface);
    ULONG refcount = InterlockedIncrement(&factory->refcount);

    TRACE("%p increasing refcount to %lu.\n", iface, refcount);

    return refcount;
}

static ULONG STDMETHODCALLTYPE dxcore_adapter_factory_Release(IDXCoreAdapterFactory *iface)
{
    struct dxcore_adapter_factory *factory = impl_from_IDXCoreAdapterFactory(iface);
    ULONG refcount = InterlockedDecrement(&factory->refcount);

    TRACE("%p decreasing refcount to %lu.\n", iface, refcount);

    return refcount;
}

static HRESULT dxcore_adapter_create(const struct wined3d_adapter *wined3d_adapter,
        struct dxcore_adapter **ret_adapter)
{
    struct dxcore_adapter *adapter;
    HRESULT hr;

    if (!(adapter = calloc(1, sizeof(*adapter))))
        return E_OUTOFMEMORY;

    adapter->IDXCoreAdapter_iface.lpVtbl = &dxcore_adapter_vtbl;
    adapter->refcount = 1;

    adapter->identifier.description_size = sizeof(adapter->driver_description);
    adapter->identifier.description = adapter->driver_description;
    if (FAILED(hr = wined3d_adapter_get_identifier(wined3d_adapter, 0, &adapter->identifier)))
    {
        free(adapter);
        return hr;
    }

    TRACE("Created adapter %p.\n", adapter);
    *ret_adapter = adapter;
    return S_OK;
}

static HRESULT get_adapters(struct dxcore_adapter_list *list)
{
    struct wined3d *wined3d = wined3d_create(0);
    HRESULT hr = S_OK;

    if (!wined3d)
        return E_FAIL;

    if (!(list->adapter_count = wined3d_get_adapter_count(wined3d)))
        goto done;

    if (!(list->adapters = calloc(list->adapter_count, sizeof(*list->adapters))))
    {
        hr = E_OUTOFMEMORY;
        goto done;
    }

    for (UINT i = 0; i < list->adapter_count; i++)
    {
        if (FAILED(hr = dxcore_adapter_create(wined3d_get_adapter(wined3d, i), &list->adapters[i])))
            goto done;
    }

done:
    wined3d_decref(wined3d);
    return hr;
}

/* Keep only the adapters that support every filter attribute, preserving their order. */
static void dxcore_adapter_list_filter(struct dxcore_adapter_list *list, uint32_t num_attributes,
        const GUID *filter_attributes)
{
    uint32_t kept = 0;

    for (uint32_t i = 0; i < list->adapter_count; ++i)
    {
        IDXCoreAdapter *adapter = &list->adapters[i]->IDXCoreAdapter_iface;
        BOOL keep = TRUE;

        for (uint32_t j = 0; j < num_attributes && keep; ++j)
            keep = IDXCoreAdapter_IsAttributeSupported(adapter, &filter_attributes[j]);

        if (keep)
        {
            list->adapters[kept++] = list->adapters[i];
        }
        else
        {
            TRACE("Dropping adapter %p, it does not support all filter attributes.\n", adapter);
            IDXCoreAdapter_Release(adapter);
        }
    }

    for (uint32_t i = kept; i < list->adapter_count; ++i)
        list->adapters[i] = NULL;
    list->adapter_count = kept;
}

static HRESULT STDMETHODCALLTYPE dxcore_adapter_factory_CreateAdapterList(IDXCoreAdapterFactory *iface, uint32_t num_attributes,
        const GUID *filter_attributes, REFIID riid, void **out)
{
    struct dxcore_adapter_list *list;
    HRESULT hr;

    TRACE("iface %p, num_attributes %u, filter_attributes %p, riid %s, out %p.\n", iface, num_attributes, filter_attributes,
            debugstr_guid(riid), out);

    if (!out)
        return E_POINTER;

    *out = NULL;

    if (!num_attributes || !filter_attributes)
        return E_INVALIDARG;

    if (!(list = calloc(1, sizeof(*list))))
        return E_OUTOFMEMORY;

    list->IDXCoreAdapterList_iface.lpVtbl = &dxcore_adapter_list_vtbl;
    list->refcount = 1;
    if (FAILED(hr = get_adapters(list)))
    {
        IDXCoreAdapterList_Release(&list->IDXCoreAdapterList_iface);
        return hr;
    }

    dxcore_adapter_list_filter(list, num_attributes, filter_attributes);

    hr = IDXCoreAdapterList_QueryInterface(&list->IDXCoreAdapterList_iface, riid, out);
    IDXCoreAdapterList_Release(&list->IDXCoreAdapterList_iface);
    TRACE("created IDXCoreAdapterList %p.\n", *out);
    return hr;
}

static HRESULT STDMETHODCALLTYPE dxcore_adapter_factory_GetAdapterByLuid(IDXCoreAdapterFactory *iface, REFLUID adapter_luid,
        REFIID riid, void **out)
{
    struct dxcore_adapter *adapter;
    struct wined3d *wined3d;
    uint32_t count;
    HRESULT hr;

    TRACE("iface %p, adapter_luid %p, riid %s, out %p.\n", iface, adapter_luid, debugstr_guid(riid), out);

    if (!(wined3d = wined3d_create(0)))
        return E_FAIL;

    count = wined3d_get_adapter_count(wined3d);
    for (uint32_t i = 0; i < count; ++i)
    {
        struct wined3d_adapter_identifier adapter_id = {0};
        struct wined3d_adapter *wined3d_adapter;

        wined3d_adapter = wined3d_get_adapter(wined3d, i);
        wined3d_adapter_get_identifier(wined3d_adapter, 0, &adapter_id);

        if (!memcmp(adapter_luid, &adapter_id.adapter_luid, sizeof(LUID)))
        {
            hr = dxcore_adapter_create(wined3d_adapter, &adapter);
            wined3d_decref(wined3d);

            if (SUCCEEDED(hr))
            {
                hr = IDXCoreAdapter_QueryInterface(&adapter->IDXCoreAdapter_iface, riid, out);
                IDXCoreAdapter_Release(&adapter->IDXCoreAdapter_iface);
            }
            return hr;
        }
    }

    wined3d_decref(wined3d);
    return E_INVALIDARG;
}

static BOOL STDMETHODCALLTYPE dxcore_adapter_factory_IsNotificationTypeSupported(IDXCoreAdapterFactory *iface, DXCoreNotificationType type)
{
    FIXME("iface %p, type %u stub!\n", iface, type);
    return FALSE;
}

static HRESULT STDMETHODCALLTYPE dxcore_adapter_factory_RegisterEventNotification(IDXCoreAdapterFactory *iface, IUnknown *dxcore_object,
        DXCoreNotificationType type, PFN_DXCORE_NOTIFICATION_CALLBACK callback, void *callback_context, uint32_t *event_cookie)
{
    FIXME("iface %p, dxcore_object %p, type %u, callback %p, callback_context %p, event_cookie %p stub!\n", iface, dxcore_object, type, callback,
            callback_context, event_cookie);
    return E_NOTIMPL;
}

static HRESULT STDMETHODCALLTYPE dxcore_adapter_factory_UnregisterEventNotification(IDXCoreAdapterFactory *iface, uint32_t event_cookie)
{
    FIXME("iface %p, event_cookie %u stub!\n", iface, event_cookie);
    return E_NOTIMPL;
}

static const struct IDXCoreAdapterFactoryVtbl dxcore_adapter_factory_vtbl =
{
    /* IUnknown methods */
    dxcore_adapter_factory_QueryInterface,
    dxcore_adapter_factory_AddRef,
    dxcore_adapter_factory_Release,
    /* IDXCoreAdapterFactory methods */
    dxcore_adapter_factory_CreateAdapterList,
    dxcore_adapter_factory_GetAdapterByLuid,
    dxcore_adapter_factory_IsNotificationTypeSupported,
    dxcore_adapter_factory_RegisterEventNotification,
    dxcore_adapter_factory_UnregisterEventNotification,
};

static struct dxcore_adapter_factory dxcore_adapter_factory = {
    .IDXCoreAdapterFactory_iface.lpVtbl = &dxcore_adapter_factory_vtbl,
    .refcount = 0
};

HRESULT STDMETHODCALLTYPE DXCoreCreateAdapterFactory(REFIID riid, void **out)
{
    HRESULT hr;

    TRACE("riid %s, out %p\n", debugstr_guid(riid), out);

    if (!out)
        return E_POINTER;

    hr = IDXCoreAdapterFactory_QueryInterface(&dxcore_adapter_factory.IDXCoreAdapterFactory_iface, riid, out);
    if (SUCCEEDED(hr))
        TRACE("returning factory %p\n", *out);

    return hr;
}
