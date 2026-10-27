/* WinRT Windows.Storage.StorageFolder implementation
 *
 * Copyright 2026 Mirko Brombin
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 */

#include "private.h"
#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(data);

struct storage_folder
{
    IStorageFolder IStorageFolder_iface;
    IStorageItem IStorageItem_iface;
    LONG ref;
    HSTRING path;
};

static inline struct storage_folder *impl_from_IStorageFolder( IStorageFolder *iface )
{
    return CONTAINING_RECORD( iface, struct storage_folder, IStorageFolder_iface );
}

static inline struct storage_folder *impl_from_IStorageItem( IStorageItem *iface )
{
    return CONTAINING_RECORD( iface, struct storage_folder, IStorageItem_iface );
}

static HRESULT storage_folder_query_interface( struct storage_folder *impl, REFIID iid, void **out )
{
    if (!out) return E_POINTER;
    *out = NULL;

    if (IsEqualGUID( iid, &IID_IUnknown ) || IsEqualGUID( iid, &IID_IInspectable ) ||
        IsEqualGUID( iid, &IID_IAgileObject ) || IsEqualGUID( iid, &IID_IStorageFolder ))
        *out = &impl->IStorageFolder_iface;
    else if (IsEqualGUID( iid, &IID_IStorageItem ))
        *out = &impl->IStorageItem_iface;

    if (!*out) return E_NOINTERFACE;
    IInspectable_AddRef( *out );
    return S_OK;
}

static HRESULT WINAPI storage_folder_QueryInterface( IStorageFolder *iface, REFIID iid, void **out )
{
    return storage_folder_query_interface( impl_from_IStorageFolder( iface ), iid, out );
}

static ULONG WINAPI storage_folder_AddRef( IStorageFolder *iface )
{
    return InterlockedIncrement( &impl_from_IStorageFolder( iface )->ref );
}

static ULONG WINAPI storage_folder_Release( IStorageFolder *iface )
{
    struct storage_folder *impl = impl_from_IStorageFolder( iface );
    ULONG ref = InterlockedDecrement( &impl->ref );

    if (!ref)
    {
        WindowsDeleteString( impl->path );
        free( impl );
    }
    return ref;
}

static HRESULT WINAPI storage_folder_GetIids( IStorageFolder *iface, ULONG *count, IID **iids )
{
    FIXME( "iface %p, count %p, iids %p stub!\n", iface, count, iids );
    return E_NOTIMPL;
}

static HRESULT WINAPI storage_folder_GetRuntimeClassName( IStorageFolder *iface, HSTRING *name )
{
    static const WCHAR class_name[] = L"Windows.Storage.StorageFolder";
    return WindowsCreateString( class_name, ARRAY_SIZE( class_name ) - 1, name );
}

static HRESULT WINAPI storage_folder_GetTrustLevel( IStorageFolder *iface, TrustLevel *level )
{
    if (!level) return E_POINTER;
    *level = BaseTrust;
    return S_OK;
}

#define FOLDER_ASYNC_STUB(name, params, args) \
    static HRESULT WINAPI storage_folder_##name params \
    { \
        FIXME args; \
        if (operation) *operation = NULL; \
        return E_NOTIMPL; \
    }

FOLDER_ASYNC_STUB( CreateFileAsyncOverloadDefaultOptions,
        (IStorageFolder *iface, HSTRING name, IAsyncOperation_StorageFile **operation),
        ("iface %p, name %s, operation %p stub!\n", iface, debugstr_hstring( name ), operation) )
FOLDER_ASYNC_STUB( CreateFileAsync,
        (IStorageFolder *iface, HSTRING name, CreationCollisionOption options, IAsyncOperation_StorageFile **operation),
        ("iface %p, name %s, options %u, operation %p stub!\n", iface, debugstr_hstring( name ), options, operation) )
FOLDER_ASYNC_STUB( CreateFolderAsyncOverloadDefaultOptions,
        (IStorageFolder *iface, HSTRING name, IAsyncOperation_StorageFolder **operation),
        ("iface %p, name %s, operation %p stub!\n", iface, debugstr_hstring( name ), operation) )
FOLDER_ASYNC_STUB( CreateFolderAsync,
        (IStorageFolder *iface, HSTRING name, CreationCollisionOption options, IAsyncOperation_StorageFolder **operation),
        ("iface %p, name %s, options %u, operation %p stub!\n", iface, debugstr_hstring( name ), options, operation) )
FOLDER_ASYNC_STUB( GetFileAsync,
        (IStorageFolder *iface, HSTRING name, IAsyncOperation_StorageFile **operation),
        ("iface %p, name %s, operation %p stub!\n", iface, debugstr_hstring( name ), operation) )
FOLDER_ASYNC_STUB( GetFolderAsync,
        (IStorageFolder *iface, HSTRING name, IAsyncOperation_StorageFolder **operation),
        ("iface %p, name %s, operation %p stub!\n", iface, debugstr_hstring( name ), operation) )
FOLDER_ASYNC_STUB( GetItemAsync,
        (IStorageFolder *iface, HSTRING name, IAsyncOperation_IStorageItem **operation),
        ("iface %p, name %s, operation %p stub!\n", iface, debugstr_hstring( name ), operation) )
FOLDER_ASYNC_STUB( GetFilesAsyncOverloadDefaultOptionsStartAndCount,
        (IStorageFolder *iface, IAsyncOperation_IVectorView_StorageFile **operation),
        ("iface %p, operation %p stub!\n", iface, operation) )
FOLDER_ASYNC_STUB( GetFoldersAsyncOverloadDefaultOptionsStartAndCount,
        (IStorageFolder *iface, IAsyncOperation_IVectorView_StorageFolder **operation),
        ("iface %p, operation %p stub!\n", iface, operation) )
FOLDER_ASYNC_STUB( GetItemsAsyncOverloadDefaultStartAndCount,
        (IStorageFolder *iface, IAsyncOperation_IVectorView_IStorageItem **operation),
        ("iface %p, operation %p stub!\n", iface, operation) )

static const struct IStorageFolderVtbl storage_folder_vtbl =
{
    storage_folder_QueryInterface,
    storage_folder_AddRef,
    storage_folder_Release,
    storage_folder_GetIids,
    storage_folder_GetRuntimeClassName,
    storage_folder_GetTrustLevel,
    storage_folder_CreateFileAsyncOverloadDefaultOptions,
    storage_folder_CreateFileAsync,
    storage_folder_CreateFolderAsyncOverloadDefaultOptions,
    storage_folder_CreateFolderAsync,
    storage_folder_GetFileAsync,
    storage_folder_GetFolderAsync,
    storage_folder_GetItemAsync,
    storage_folder_GetFilesAsyncOverloadDefaultOptionsStartAndCount,
    storage_folder_GetFoldersAsyncOverloadDefaultOptionsStartAndCount,
    storage_folder_GetItemsAsyncOverloadDefaultStartAndCount,
};

static HRESULT WINAPI storage_item_QueryInterface( IStorageItem *iface, REFIID iid, void **out )
{
    return storage_folder_query_interface( impl_from_IStorageItem( iface ), iid, out );
}

static ULONG WINAPI storage_item_AddRef( IStorageItem *iface )
{
    return InterlockedIncrement( &impl_from_IStorageItem( iface )->ref );
}

static ULONG WINAPI storage_item_Release( IStorageItem *iface )
{
    struct storage_folder *impl = impl_from_IStorageItem( iface );
    return storage_folder_Release( &impl->IStorageFolder_iface );
}

static HRESULT WINAPI storage_item_GetIids( IStorageItem *iface, ULONG *count, IID **iids )
{
    return storage_folder_GetIids( &impl_from_IStorageItem( iface )->IStorageFolder_iface, count, iids );
}

static HRESULT WINAPI storage_item_GetRuntimeClassName( IStorageItem *iface, HSTRING *name )
{
    return storage_folder_GetRuntimeClassName( &impl_from_IStorageItem( iface )->IStorageFolder_iface, name );
}

static HRESULT WINAPI storage_item_GetTrustLevel( IStorageItem *iface, TrustLevel *level )
{
    return storage_folder_GetTrustLevel( &impl_from_IStorageItem( iface )->IStorageFolder_iface, level );
}

#define ITEM_ASYNC_STUB(name, params, args) \
    static HRESULT WINAPI storage_item_##name params \
    { \
        FIXME args; \
        if (operation) *operation = NULL; \
        return E_NOTIMPL; \
    }

ITEM_ASYNC_STUB( RenameAsyncOverloadDefaultOptions,
        (IStorageItem *iface, HSTRING name, IAsyncAction **operation),
        ("iface %p, name %s, operation %p stub!\n", iface, debugstr_hstring( name ), operation) )
ITEM_ASYNC_STUB( RenameAsync,
        (IStorageItem *iface, HSTRING name, NameCollisionOption option, IAsyncAction **operation),
        ("iface %p, name %s, option %u, operation %p stub!\n", iface, debugstr_hstring( name ), option, operation) )
ITEM_ASYNC_STUB( DeleteAsyncOverloadDefaultOptions,
        (IStorageItem *iface, IAsyncAction **operation),
        ("iface %p, operation %p stub!\n", iface, operation) )
ITEM_ASYNC_STUB( DeleteAsync,
        (IStorageItem *iface, StorageDeleteOption option, IAsyncAction **operation),
        ("iface %p, option %u, operation %p stub!\n", iface, option, operation) )
ITEM_ASYNC_STUB( GetBasicPropertiesAsync,
        (IStorageItem *iface, IAsyncOperation_BasicProperties **operation),
        ("iface %p, operation %p stub!\n", iface, operation) )

static HRESULT WINAPI storage_item_get_Name( IStorageItem *iface, HSTRING *value )
{
    struct storage_folder *impl = impl_from_IStorageItem( iface );
    const WCHAR *path = WindowsGetStringRawBuffer( impl->path, NULL ), *name;

    if (!value) return E_POINTER;
    name = wcsrchr( path, '\\' );
    if (!name) name = path;
    else name++;
    return WindowsCreateString( name, wcslen( name ), value );
}

static HRESULT WINAPI storage_item_get_Path( IStorageItem *iface, HSTRING *value )
{
    struct storage_folder *impl = impl_from_IStorageItem( iface );
    if (!value) return E_POINTER;
    return WindowsDuplicateString( impl->path, value );
}

static HRESULT WINAPI storage_item_get_Attributes( IStorageItem *iface, FileAttributes *value )
{
    if (!value) return E_POINTER;
    *value = FileAttributes_Directory;
    return S_OK;
}

static HRESULT WINAPI storage_item_get_DateCreated( IStorageItem *iface, DateTime *value )
{
    FIXME( "iface %p, value %p stub!\n", iface, value );
    return E_NOTIMPL;
}

static HRESULT WINAPI storage_item_IsOfType( IStorageItem *iface, StorageItemTypes type, boolean *value )
{
    if (!value) return E_POINTER;
    *value = type == StorageItemTypes_Folder;
    return S_OK;
}

static const struct IStorageItemVtbl storage_item_vtbl =
{
    storage_item_QueryInterface,
    storage_item_AddRef,
    storage_item_Release,
    storage_item_GetIids,
    storage_item_GetRuntimeClassName,
    storage_item_GetTrustLevel,
    storage_item_RenameAsyncOverloadDefaultOptions,
    storage_item_RenameAsync,
    storage_item_DeleteAsyncOverloadDefaultOptions,
    storage_item_DeleteAsync,
    storage_item_GetBasicPropertiesAsync,
    storage_item_get_Name,
    storage_item_get_Path,
    storage_item_get_Attributes,
    storage_item_get_DateCreated,
    storage_item_IsOfType,
};

HRESULT storage_folder_create( HSTRING path, IStorageFolder **value )
{
    struct storage_folder *impl;
    HRESULT hr;

    if (!value) return E_POINTER;
    *value = NULL;
    if (!path) return E_INVALIDARG;
    if (!(impl = calloc( 1, sizeof(*impl) ))) return E_OUTOFMEMORY;

    impl->IStorageFolder_iface.lpVtbl = &storage_folder_vtbl;
    impl->IStorageItem_iface.lpVtbl = &storage_item_vtbl;
    impl->ref = 1;
    if (FAILED( hr = WindowsDuplicateString( path, &impl->path ) ))
    {
        free( impl );
        return hr;
    }

    *value = &impl->IStorageFolder_iface;
    TRACE( "created storage folder %p for %s.\n", *value, debugstr_hstring( path ) );
    return S_OK;
}
