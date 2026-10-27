/* WinRT weak reference helpers
 *
 * Copyright 2025 Zhiyi Zhang for CodeWeavers
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 */

#define COBJMACROS
#include "weakref.h"

struct weak_reference
{
    IWeakReference IWeakReference_iface;
    IUnknown *object;
    LONG ref_strong;
    LONG ref_weak;
};

static inline struct weak_reference *impl_from_IWeakReference( IWeakReference *iface )
{
    return CONTAINING_RECORD( iface, struct weak_reference, IWeakReference_iface );
}

static HRESULT WINAPI weak_reference_QueryInterface( IWeakReference *iface, REFIID iid, void **out )
{
    if (!out) return E_POINTER;
    if (IsEqualGUID( iid, &IID_IUnknown ) || IsEqualGUID( iid, &IID_IWeakReference ))
    {
        *out = iface;
        IWeakReference_AddRef( iface );
        return S_OK;
    }
    *out = NULL;
    return E_NOINTERFACE;
}

static ULONG WINAPI weak_reference_AddRef( IWeakReference *iface )
{
    return InterlockedIncrement( &impl_from_IWeakReference( iface )->ref_weak );
}

static ULONG WINAPI weak_reference_Release( IWeakReference *iface )
{
    struct weak_reference *impl = impl_from_IWeakReference( iface );
    ULONG ref = InterlockedDecrement( &impl->ref_weak );

    if (!ref) free( impl );
    return ref;
}

static HRESULT WINAPI weak_reference_Resolve( IWeakReference *iface, REFIID iid, IInspectable **out )
{
    struct weak_reference *impl = impl_from_IWeakReference( iface );
    LONG ref;
    HRESULT hr;

    if (!out) return E_POINTER;
    *out = NULL;
    do
    {
        if (!(ref = ReadNoFence( &impl->ref_strong ))) return S_OK;
    } while (ref != InterlockedCompareExchange( &impl->ref_strong, ref + 1, ref ));

    hr = IUnknown_QueryInterface( impl->object, iid, (void **)out );
    IUnknown_Release( impl->object );
    return hr;
}

static const IWeakReferenceVtbl weak_reference_vtbl =
{
    weak_reference_QueryInterface,
    weak_reference_AddRef,
    weak_reference_Release,
    weak_reference_Resolve,
};

static inline struct weak_reference_source *impl_from_IWeakReferenceSource( IWeakReferenceSource *iface )
{
    return CONTAINING_RECORD( iface, struct weak_reference_source, IWeakReferenceSource_iface );
}

static HRESULT WINAPI weak_reference_source_QueryInterface( IWeakReferenceSource *iface,
        REFIID iid, void **out )
{
    struct weak_reference_source *source = impl_from_IWeakReferenceSource( iface );
    struct weak_reference *weak = impl_from_IWeakReference( source->weak_reference );

    return IUnknown_QueryInterface( weak->object, iid, out );
}

static ULONG WINAPI weak_reference_source_AddRef( IWeakReferenceSource *iface )
{
    struct weak_reference_source *source = impl_from_IWeakReferenceSource( iface );
    struct weak_reference *weak = impl_from_IWeakReference( source->weak_reference );

    return IUnknown_AddRef( weak->object );
}

static ULONG WINAPI weak_reference_source_Release( IWeakReferenceSource *iface )
{
    struct weak_reference_source *source = impl_from_IWeakReferenceSource( iface );
    struct weak_reference *weak = impl_from_IWeakReference( source->weak_reference );

    return IUnknown_Release( weak->object );
}

static HRESULT WINAPI weak_reference_source_GetWeakReference( IWeakReferenceSource *iface,
        IWeakReference **weak_reference )
{
    struct weak_reference_source *source = impl_from_IWeakReferenceSource( iface );

    if (!weak_reference) return E_POINTER;
    *weak_reference = source->weak_reference;
    IWeakReference_AddRef( *weak_reference );
    return S_OK;
}

static const IWeakReferenceSourceVtbl weak_reference_source_vtbl =
{
    weak_reference_source_QueryInterface,
    weak_reference_source_AddRef,
    weak_reference_source_Release,
    weak_reference_source_GetWeakReference,
};

HRESULT weak_reference_source_init( struct weak_reference_source *source, IUnknown *object )
{
    struct weak_reference *weak;

    if (!(weak = calloc( 1, sizeof(*weak) ))) return E_OUTOFMEMORY;
    weak->IWeakReference_iface.lpVtbl = &weak_reference_vtbl;
    weak->ref_strong = 1;
    weak->ref_weak = 1;
    weak->object = object;
    source->IWeakReferenceSource_iface.lpVtbl = &weak_reference_source_vtbl;
    source->weak_reference = &weak->IWeakReference_iface;
    return S_OK;
}

ULONG weak_reference_strong_add_ref( struct weak_reference_source *source )
{
    struct weak_reference *weak = impl_from_IWeakReference( source->weak_reference );

    return InterlockedIncrement( &weak->ref_strong );
}

ULONG weak_reference_strong_release( struct weak_reference_source *source )
{
    struct weak_reference *weak = impl_from_IWeakReference( source->weak_reference );
    ULONG ref = InterlockedDecrement( &weak->ref_strong );

    if (!ref) IWeakReference_Release( source->weak_reference );
    return ref;
}
