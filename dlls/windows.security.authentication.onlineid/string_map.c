/*
 * WinRT string map
 *
 * Copyright (C) 2026 Mirko Brombin
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 */

#include "private.h"

struct map_entry
{
    HSTRING key;
    HSTRING value;
};

struct string_map
{
    IMap_HSTRING_HSTRING IMap_iface;
    IMapView_HSTRING_HSTRING IMapView_iface;
    IIterable_IKeyValuePair_HSTRING_HSTRING IIterable_iface;
    LONG ref;
    CRITICAL_SECTION cs;
    struct map_entry *entries;
    UINT32 count;
    UINT32 capacity;
    UINT64 version;
};

static inline struct string_map *impl_from_IMap( IMap_HSTRING_HSTRING *iface )
{
    return CONTAINING_RECORD( iface, struct string_map, IMap_iface );
}

static inline struct string_map *impl_from_IMapView( IMapView_HSTRING_HSTRING *iface )
{
    return CONTAINING_RECORD( iface, struct string_map, IMapView_iface );
}

static INT32 map_find( struct string_map *impl, HSTRING key )
{
    UINT32 i;

    for (i = 0; i < impl->count; ++i)
    {
        INT32 order;
        if (SUCCEEDED(WindowsCompareStringOrdinal( impl->entries[i].key, key, &order )) && !order) return i;
    }
    return -1;
}

static HRESULT map_query_interface( struct string_map *impl, REFIID iid, void **out )
{
    if (!out) return E_POINTER;
    *out = NULL;
    if (IsEqualGUID( iid, &IID_IUnknown ) || IsEqualGUID( iid, &IID_IInspectable ) ||
        IsEqualGUID( iid, &IID_IAgileObject ) || IsEqualGUID( iid, &IID_IMap_HSTRING_HSTRING ))
        *out = &impl->IMap_iface;
    else if (IsEqualGUID( iid, &IID_IMapView_HSTRING_HSTRING ))
        *out = &impl->IMapView_iface;
    else if (IsEqualGUID( iid, &IID_IIterable_IKeyValuePair_HSTRING_HSTRING ))
        *out = &impl->IIterable_iface;
    else
        return E_NOINTERFACE;
    InterlockedIncrement( &impl->ref );
    return S_OK;
}

static ULONG map_release( struct string_map *impl )
{
    ULONG ref = InterlockedDecrement( &impl->ref );

    if (!ref)
    {
        UINT32 i;
        for (i = 0; i < impl->count; ++i)
        {
            WindowsDeleteString( impl->entries[i].key );
            WindowsDeleteString( impl->entries[i].value );
        }
        free( impl->entries );
        impl->cs.DebugInfo->Spare[0] = 0;
        DeleteCriticalSection( &impl->cs );
        free( impl );
    }
    return ref;
}

static HRESULT map_get_iids( ULONG *count, IID **iids )
{
    if (!count || !iids) return E_POINTER;
    *count = 3;
    if (!(*iids = CoTaskMemAlloc( 3 * sizeof(**iids) ))) return E_OUTOFMEMORY;
    (*iids)[0] = IID_IMap_HSTRING_HSTRING;
    (*iids)[1] = IID_IMapView_HSTRING_HSTRING;
    (*iids)[2] = IID_IIterable_IKeyValuePair_HSTRING_HSTRING;
    return S_OK;
}

static HRESULT map_get_class_name( HSTRING *name )
{
    static const WCHAR class_name[] = L"Windows.Foundation.Collections.StringMap";
    if (!name) return E_POINTER;
    return WindowsCreateString( class_name, ARRAY_SIZE(class_name) - 1, name );
}

static HRESULT map_get_trust_level( TrustLevel *level )
{
    if (!level) return E_POINTER;
    *level = BaseTrust;
    return S_OK;
}

static HRESULT map_lookup( struct string_map *impl, HSTRING key, HSTRING *value )
{
    INT32 index;
    HRESULT hr;

    if (!value) return E_POINTER;
    *value = NULL;
    EnterCriticalSection( &impl->cs );
    index = map_find( impl, key );
    hr = index < 0 ? E_BOUNDS : WindowsDuplicateString( impl->entries[index].value, value );
    LeaveCriticalSection( &impl->cs );
    return hr;
}

static HRESULT map_get_size( struct string_map *impl, UINT32 *size )
{
    if (!size) return E_POINTER;
    EnterCriticalSection( &impl->cs );
    *size = impl->count;
    LeaveCriticalSection( &impl->cs );
    return S_OK;
}

static HRESULT map_has_key( struct string_map *impl, HSTRING key, boolean *found )
{
    if (!found) return E_POINTER;
    EnterCriticalSection( &impl->cs );
    *found = map_find( impl, key ) >= 0;
    LeaveCriticalSection( &impl->cs );
    return S_OK;
}

static HRESULT WINAPI map_QueryInterface( IMap_HSTRING_HSTRING *iface, REFIID iid, void **out )
{
    return map_query_interface( impl_from_IMap( iface ), iid, out );
}

static ULONG WINAPI map_AddRef( IMap_HSTRING_HSTRING *iface )
{
    return InterlockedIncrement( &impl_from_IMap( iface )->ref );
}

static ULONG WINAPI map_Release( IMap_HSTRING_HSTRING *iface )
{
    return map_release( impl_from_IMap( iface ) );
}

static HRESULT WINAPI map_GetIids( IMap_HSTRING_HSTRING *iface, ULONG *count, IID **iids )
{
    return map_get_iids( count, iids );
}

static HRESULT WINAPI map_GetRuntimeClassName( IMap_HSTRING_HSTRING *iface, HSTRING *name )
{
    return map_get_class_name( name );
}

static HRESULT WINAPI map_GetTrustLevel( IMap_HSTRING_HSTRING *iface, TrustLevel *level )
{
    return map_get_trust_level( level );
}

static HRESULT WINAPI map_Lookup( IMap_HSTRING_HSTRING *iface, HSTRING key, HSTRING *value )
{
    return map_lookup( impl_from_IMap( iface ), key, value );
}

static HRESULT WINAPI map_get_Size( IMap_HSTRING_HSTRING *iface, UINT32 *size )
{
    return map_get_size( impl_from_IMap( iface ), size );
}

static HRESULT WINAPI map_HasKey( IMap_HSTRING_HSTRING *iface, HSTRING key, boolean *found )
{
    return map_has_key( impl_from_IMap( iface ), key, found );
}

static HRESULT WINAPI map_GetView( IMap_HSTRING_HSTRING *iface, IMapView_HSTRING_HSTRING **view )
{
    struct string_map *impl = impl_from_IMap( iface );
    IMap_HSTRING_HSTRING *snapshot;
    boolean replaced;
    UINT32 i;
    HRESULT hr;

    if (!view) return E_POINTER;
    *view = NULL;
    if (FAILED(hr = string_map_create( &snapshot ))) return hr;
    EnterCriticalSection( &impl->cs );
    for (i = 0; i < impl->count; ++i)
        if (FAILED(hr = IMap_HSTRING_HSTRING_Insert( snapshot, impl->entries[i].key,
                                                    impl->entries[i].value, &replaced ))) break;
    LeaveCriticalSection( &impl->cs );
    if (SUCCEEDED(hr))
        hr = IMap_HSTRING_HSTRING_QueryInterface( snapshot, &IID_IMapView_HSTRING_HSTRING, (void **)view );
    IMap_HSTRING_HSTRING_Release( snapshot );
    return hr;
}

static HRESULT WINAPI map_Insert( IMap_HSTRING_HSTRING *iface, HSTRING key, HSTRING value, boolean *replaced )
{
    struct string_map *impl = impl_from_IMap( iface );
    HSTRING key_copy = NULL, value_copy = NULL;
    struct map_entry *entries;
    INT32 index;
    HRESULT hr;

    if (!replaced) return E_POINTER;
    if (FAILED(hr = WindowsDuplicateString( value, &value_copy ))) return hr;
    EnterCriticalSection( &impl->cs );
    index = map_find( impl, key );
    if (index >= 0)
    {
        WindowsDeleteString( impl->entries[index].value );
        impl->entries[index].value = value_copy;
        *replaced = TRUE;
        ++impl->version;
        LeaveCriticalSection( &impl->cs );
        return S_OK;
    }
    if (FAILED(hr = WindowsDuplicateString( key, &key_copy ))) goto failed;
    if (impl->count == impl->capacity)
    {
        UINT32 capacity = impl->capacity ? impl->capacity * 2 : 4;
        if (!(entries = realloc( impl->entries, capacity * sizeof(*entries) )))
        {
            hr = E_OUTOFMEMORY;
            goto failed;
        }
        impl->entries = entries;
        impl->capacity = capacity;
    }
    impl->entries[impl->count].key = key_copy;
    impl->entries[impl->count].value = value_copy;
    ++impl->count;
    ++impl->version;
    *replaced = FALSE;
    LeaveCriticalSection( &impl->cs );
    return S_OK;

failed:
    LeaveCriticalSection( &impl->cs );
    WindowsDeleteString( key_copy );
    WindowsDeleteString( value_copy );
    return hr;
}

static HRESULT WINAPI map_Remove( IMap_HSTRING_HSTRING *iface, HSTRING key )
{
    struct string_map *impl = impl_from_IMap( iface );
    INT32 index;

    EnterCriticalSection( &impl->cs );
    index = map_find( impl, key );
    if (index < 0)
    {
        LeaveCriticalSection( &impl->cs );
        return E_BOUNDS;
    }
    WindowsDeleteString( impl->entries[index].key );
    WindowsDeleteString( impl->entries[index].value );
    memmove( impl->entries + index, impl->entries + index + 1,
             (impl->count - index - 1) * sizeof(*impl->entries) );
    --impl->count;
    ++impl->version;
    LeaveCriticalSection( &impl->cs );
    return S_OK;
}

static HRESULT WINAPI map_Clear( IMap_HSTRING_HSTRING *iface )
{
    struct string_map *impl = impl_from_IMap( iface );
    UINT32 i;

    EnterCriticalSection( &impl->cs );
    for (i = 0; i < impl->count; ++i)
    {
        WindowsDeleteString( impl->entries[i].key );
        WindowsDeleteString( impl->entries[i].value );
    }
    impl->count = 0;
    ++impl->version;
    LeaveCriticalSection( &impl->cs );
    return S_OK;
}

static const IMap_HSTRING_HSTRINGVtbl map_vtbl =
{
    map_QueryInterface,
    map_AddRef,
    map_Release,
    map_GetIids,
    map_GetRuntimeClassName,
    map_GetTrustLevel,
    map_Lookup,
    map_get_Size,
    map_HasKey,
    map_GetView,
    map_Insert,
    map_Remove,
    map_Clear,
};

static HRESULT WINAPI view_QueryInterface( IMapView_HSTRING_HSTRING *iface, REFIID iid, void **out )
{
    return map_query_interface( impl_from_IMapView( iface ), iid, out );
}

static ULONG WINAPI view_AddRef( IMapView_HSTRING_HSTRING *iface )
{
    return InterlockedIncrement( &impl_from_IMapView( iface )->ref );
}

static ULONG WINAPI view_Release( IMapView_HSTRING_HSTRING *iface )
{
    return map_release( impl_from_IMapView( iface ) );
}

static HRESULT WINAPI view_GetIids( IMapView_HSTRING_HSTRING *iface, ULONG *count, IID **iids )
{
    return map_get_iids( count, iids );
}

static HRESULT WINAPI view_GetRuntimeClassName( IMapView_HSTRING_HSTRING *iface, HSTRING *name )
{
    return map_get_class_name( name );
}

static HRESULT WINAPI view_GetTrustLevel( IMapView_HSTRING_HSTRING *iface, TrustLevel *level )
{
    return map_get_trust_level( level );
}

static HRESULT WINAPI view_Lookup( IMapView_HSTRING_HSTRING *iface, HSTRING key, HSTRING *value )
{
    return map_lookup( impl_from_IMapView( iface ), key, value );
}

static HRESULT WINAPI view_get_Size( IMapView_HSTRING_HSTRING *iface, UINT32 *size )
{
    return map_get_size( impl_from_IMapView( iface ), size );
}

static HRESULT WINAPI view_HasKey( IMapView_HSTRING_HSTRING *iface, HSTRING key, boolean *found )
{
    return map_has_key( impl_from_IMapView( iface ), key, found );
}

static HRESULT WINAPI view_Split( IMapView_HSTRING_HSTRING *iface, IMapView_HSTRING_HSTRING **first,
                                  IMapView_HSTRING_HSTRING **second )
{
    if (!first || !second) return E_POINTER;
    *first = iface;
    *second = NULL;
    IMapView_HSTRING_HSTRING_AddRef( iface );
    return S_OK;
}

static const IMapView_HSTRING_HSTRINGVtbl view_vtbl =
{
    view_QueryInterface,
    view_AddRef,
    view_Release,
    view_GetIids,
    view_GetRuntimeClassName,
    view_GetTrustLevel,
    view_Lookup,
    view_get_Size,
    view_HasKey,
    view_Split,
};

struct string_pair
{
    IKeyValuePair_HSTRING_HSTRING IKeyValuePair_iface;
    LONG ref;
    HSTRING key, value;
};

static inline struct string_pair *impl_from_pair( IKeyValuePair_HSTRING_HSTRING *iface )
{
    return CONTAINING_RECORD( iface, struct string_pair, IKeyValuePair_iface );
}

static HRESULT WINAPI pair_QueryInterface( IKeyValuePair_HSTRING_HSTRING *iface, REFIID iid, void **out )
{
    if (!out) return E_POINTER;
    *out = NULL;
    if (!IsEqualGUID( iid, &IID_IUnknown ) && !IsEqualGUID( iid, &IID_IInspectable ) &&
        !IsEqualGUID( iid, &IID_IAgileObject ) && !IsEqualGUID( iid, &IID_IKeyValuePair_HSTRING_HSTRING ))
        return E_NOINTERFACE;
    *out = iface;
    IKeyValuePair_HSTRING_HSTRING_AddRef( iface );
    return S_OK;
}

static ULONG WINAPI pair_AddRef( IKeyValuePair_HSTRING_HSTRING *iface )
{
    return InterlockedIncrement( &impl_from_pair( iface )->ref );
}

static ULONG WINAPI pair_Release( IKeyValuePair_HSTRING_HSTRING *iface )
{
    struct string_pair *impl = impl_from_pair( iface );
    ULONG ref = InterlockedDecrement( &impl->ref );
    if (!ref)
    {
        WindowsDeleteString( impl->key );
        WindowsDeleteString( impl->value );
        free( impl );
    }
    return ref;
}

static HRESULT WINAPI pair_GetIids( IKeyValuePair_HSTRING_HSTRING *iface, ULONG *count, IID **iids )
{
    if (!count || !iids) return E_POINTER;
    *count = 0;
    if (!(*iids = CoTaskMemAlloc( sizeof(**iids) ))) return E_OUTOFMEMORY;
    **iids = IID_IKeyValuePair_HSTRING_HSTRING;
    *count = 1;
    return S_OK;
}

static HRESULT WINAPI pair_GetRuntimeClassName( IKeyValuePair_HSTRING_HSTRING *iface, HSTRING *name )
{
    static const WCHAR class_name[] = L"Windows.Foundation.Collections.IKeyValuePair`2<String,String>";
    if (!name) return E_POINTER;
    return WindowsCreateString( class_name, ARRAY_SIZE(class_name) - 1, name );
}

static HRESULT WINAPI pair_GetTrustLevel( IKeyValuePair_HSTRING_HSTRING *iface, TrustLevel *level )
{
    return map_get_trust_level( level );
}

static HRESULT WINAPI pair_get_Key( IKeyValuePair_HSTRING_HSTRING *iface, HSTRING *out )
{
    if (!out) return E_POINTER;
    return WindowsDuplicateString( impl_from_pair( iface )->key, out );
}

static HRESULT WINAPI pair_get_Value( IKeyValuePair_HSTRING_HSTRING *iface, HSTRING *out )
{
    if (!out) return E_POINTER;
    return WindowsDuplicateString( impl_from_pair( iface )->value, out );
}

static const IKeyValuePair_HSTRING_HSTRINGVtbl pair_vtbl =
{
    pair_QueryInterface, pair_AddRef, pair_Release, pair_GetIids,
    pair_GetRuntimeClassName, pair_GetTrustLevel, pair_get_Key, pair_get_Value,
};

static HRESULT pair_create( const struct map_entry *entry, IKeyValuePair_HSTRING_HSTRING **out )
{
    struct string_pair *impl;
    HRESULT hr;
    *out = NULL;
    if (!(impl = calloc( 1, sizeof(*impl) ))) return E_OUTOFMEMORY;
    impl->IKeyValuePair_iface.lpVtbl = &pair_vtbl;
    impl->ref = 1;
    if (FAILED(hr = WindowsDuplicateString( entry->key, &impl->key )) ||
        FAILED(hr = WindowsDuplicateString( entry->value, &impl->value )))
    {
        pair_Release( &impl->IKeyValuePair_iface );
        return hr;
    }
    *out = &impl->IKeyValuePair_iface;
    return S_OK;
}

struct map_iterator
{
    IIterator_IKeyValuePair_HSTRING_HSTRING IIterator_iface;
    LONG ref;
    struct string_map *map;
    UINT64 version;
    UINT32 index;
};

static inline struct map_iterator *impl_from_iterator( IIterator_IKeyValuePair_HSTRING_HSTRING *iface )
{
    return CONTAINING_RECORD( iface, struct map_iterator, IIterator_iface );
}

static HRESULT WINAPI iterator_QueryInterface( IIterator_IKeyValuePair_HSTRING_HSTRING *iface,
                                               REFIID iid, void **out )
{
    if (!out) return E_POINTER;
    *out = NULL;
    if (!IsEqualGUID( iid, &IID_IUnknown ) && !IsEqualGUID( iid, &IID_IInspectable ) &&
        !IsEqualGUID( iid, &IID_IIterator_IKeyValuePair_HSTRING_HSTRING )) return E_NOINTERFACE;
    *out = iface;
    IIterator_IKeyValuePair_HSTRING_HSTRING_AddRef( iface );
    return S_OK;
}

static ULONG WINAPI iterator_AddRef( IIterator_IKeyValuePair_HSTRING_HSTRING *iface )
{
    return InterlockedIncrement( &impl_from_iterator( iface )->ref );
}

static ULONG WINAPI iterator_Release( IIterator_IKeyValuePair_HSTRING_HSTRING *iface )
{
    struct map_iterator *impl = impl_from_iterator( iface );
    ULONG ref = InterlockedDecrement( &impl->ref );
    if (!ref)
    {
        map_release( impl->map );
        free( impl );
    }
    return ref;
}

static HRESULT WINAPI iterator_GetIids( IIterator_IKeyValuePair_HSTRING_HSTRING *iface,
                                       ULONG *count, IID **iids )
{
    if (!count || !iids) return E_POINTER;
    *count = 0;
    if (!(*iids = CoTaskMemAlloc( sizeof(**iids) ))) return E_OUTOFMEMORY;
    **iids = IID_IIterator_IKeyValuePair_HSTRING_HSTRING;
    *count = 1;
    return S_OK;
}

static HRESULT WINAPI iterator_GetRuntimeClassName( IIterator_IKeyValuePair_HSTRING_HSTRING *iface,
                                                    HSTRING *name )
{
    static const WCHAR class_name[] = L"Windows.Foundation.Collections.IIterator`1<IKeyValuePair<String,String>>";
    if (!name) return E_POINTER;
    return WindowsCreateString( class_name, ARRAY_SIZE(class_name) - 1, name );
}

static HRESULT WINAPI iterator_GetTrustLevel( IIterator_IKeyValuePair_HSTRING_HSTRING *iface,
                                              TrustLevel *level )
{
    return map_get_trust_level( level );
}

static HRESULT WINAPI iterator_get_Current( IIterator_IKeyValuePair_HSTRING_HSTRING *iface,
                                            IKeyValuePair_HSTRING_HSTRING **out )
{
    struct map_iterator *impl = impl_from_iterator( iface );
    HRESULT hr;
    if (!out) return E_POINTER;
    *out = NULL;
    EnterCriticalSection( &impl->map->cs );
    if (impl->version != impl->map->version) hr = E_CHANGED_STATE;
    else if (impl->index >= impl->map->count) hr = E_BOUNDS;
    else hr = pair_create( impl->map->entries + impl->index, out );
    LeaveCriticalSection( &impl->map->cs );
    return hr;
}

static HRESULT WINAPI iterator_get_HasCurrent( IIterator_IKeyValuePair_HSTRING_HSTRING *iface, boolean *out )
{
    struct map_iterator *impl = impl_from_iterator( iface );
    HRESULT hr = S_OK;
    if (!out) return E_POINTER;
    *out = FALSE;
    EnterCriticalSection( &impl->map->cs );
    if (impl->version != impl->map->version) hr = E_CHANGED_STATE;
    else *out = impl->index < impl->map->count;
    LeaveCriticalSection( &impl->map->cs );
    return hr;
}

static HRESULT WINAPI iterator_MoveNext( IIterator_IKeyValuePair_HSTRING_HSTRING *iface, boolean *out )
{
    struct map_iterator *impl = impl_from_iterator( iface );
    HRESULT hr = S_OK;
    if (!out) return E_POINTER;
    *out = FALSE;
    EnterCriticalSection( &impl->map->cs );
    if (impl->version != impl->map->version) hr = E_CHANGED_STATE;
    else
    {
        if (impl->index < impl->map->count) ++impl->index;
        *out = impl->index < impl->map->count;
    }
    LeaveCriticalSection( &impl->map->cs );
    return hr;
}

static HRESULT WINAPI iterator_GetMany( IIterator_IKeyValuePair_HSTRING_HSTRING *iface, UINT32 capacity,
                                        IKeyValuePair_HSTRING_HSTRING **items, UINT32 *count )
{
    struct map_iterator *impl = impl_from_iterator( iface );
    HRESULT hr = S_OK;
    UINT32 i;
    if (!count || (capacity && !items)) return E_POINTER;
    *count = 0;
    EnterCriticalSection( &impl->map->cs );
    if (impl->version != impl->map->version) hr = E_CHANGED_STATE;
    else
    {
        for (i = 0; i < capacity && impl->index + i < impl->map->count; ++i)
        {
            if (FAILED(hr = pair_create( impl->map->entries + impl->index + i, items + i ))) break;
            ++*count;
        }
        impl->index += *count;
    }
    LeaveCriticalSection( &impl->map->cs );
    return hr;
}

static const IIterator_IKeyValuePair_HSTRING_HSTRINGVtbl iterator_vtbl =
{
    iterator_QueryInterface, iterator_AddRef, iterator_Release, iterator_GetIids,
    iterator_GetRuntimeClassName, iterator_GetTrustLevel, iterator_get_Current,
    iterator_get_HasCurrent, iterator_MoveNext, iterator_GetMany,
};

DEFINE_IINSPECTABLE_( iterable, IIterable_IKeyValuePair_HSTRING_HSTRING, struct string_map,
                     impl_from_iterable, IIterable_iface, &impl->IMap_iface )

static HRESULT WINAPI iterable_First( IIterable_IKeyValuePair_HSTRING_HSTRING *iface,
                                      IIterator_IKeyValuePair_HSTRING_HSTRING **out )
{
    struct string_map *map = impl_from_iterable( iface );
    struct map_iterator *impl;
    if (!out) return E_POINTER;
    *out = NULL;
    if (!(impl = calloc( 1, sizeof(*impl) ))) return E_OUTOFMEMORY;
    impl->IIterator_iface.lpVtbl = &iterator_vtbl;
    impl->ref = 1;
    impl->map = map;
    IMap_HSTRING_HSTRING_AddRef( &map->IMap_iface );
    EnterCriticalSection( &map->cs );
    impl->version = map->version;
    LeaveCriticalSection( &map->cs );
    *out = &impl->IIterator_iface;
    return S_OK;
}

static const IIterable_IKeyValuePair_HSTRING_HSTRINGVtbl iterable_vtbl =
{
    iterable_QueryInterface, iterable_AddRef, iterable_Release, iterable_GetIids,
    iterable_GetRuntimeClassName, iterable_GetTrustLevel, iterable_First,
};

HRESULT string_map_create( IMap_HSTRING_HSTRING **out )
{
    struct string_map *impl;

    if (!out) return E_POINTER;
    *out = NULL;
    if (!(impl = calloc( 1, sizeof(*impl) ))) return E_OUTOFMEMORY;
    impl->IMap_iface.lpVtbl = &map_vtbl;
    impl->IMapView_iface.lpVtbl = &view_vtbl;
    impl->IIterable_iface.lpVtbl = &iterable_vtbl;
    impl->ref = 1;
    InitializeCriticalSectionEx( &impl->cs, 0, RTL_CRITICAL_SECTION_FLAG_FORCE_DEBUG_INFO );
    impl->cs.DebugInfo->Spare[0] = (DWORD_PTR)(__FILE__ ": string_map.cs");
    *out = &impl->IMap_iface;
    return S_OK;
}
