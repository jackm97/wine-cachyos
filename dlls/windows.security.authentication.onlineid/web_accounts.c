/* WinRT account enumeration
 *
 * Copyright (C) 2026 Mirko Brombin
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 */

#include "private.h"

struct accounts_result
{
    IFindAllAccountsResult IFindAllAccountsResult_iface;
    IVectorView_WebAccount IVectorView_iface;
    IIterable_WebAccount IIterable_iface;
    LONG ref;
    UINT32 count;
    IWebAccount **accounts;
    IWebProviderError *error;
};

static inline struct accounts_result *impl_from_result( IFindAllAccountsResult *iface )
{
    return CONTAINING_RECORD( iface, struct accounts_result, IFindAllAccountsResult_iface );
}

static HRESULT WINAPI result_QueryInterface( IFindAllAccountsResult *iface, REFIID iid, void **out )
{
    struct accounts_result *impl = impl_from_result( iface );

    if (!out) return E_POINTER;
    *out = NULL;
    if (IsEqualGUID( iid, &IID_IUnknown ) || IsEqualGUID( iid, &IID_IInspectable ) ||
        IsEqualGUID( iid, &IID_IAgileObject ) || IsEqualGUID( iid, &IID_IFindAllAccountsResult ))
        *out = iface;
    else if (IsEqualGUID( iid, &IID_IVectorView_WebAccount ))
        *out = &impl->IVectorView_iface;
    else if (IsEqualGUID( iid, &IID_IIterable_WebAccount ))
        *out = &impl->IIterable_iface;
    else return E_NOINTERFACE;
    IFindAllAccountsResult_AddRef( iface );
    return S_OK;
}

static ULONG WINAPI result_AddRef( IFindAllAccountsResult *iface )
{
    return InterlockedIncrement( &impl_from_result( iface )->ref );
}

static ULONG WINAPI result_Release( IFindAllAccountsResult *iface )
{
    struct accounts_result *impl = impl_from_result( iface );
    ULONG ref = InterlockedDecrement( &impl->ref );
    UINT32 i;

    if (!ref)
    {
        for (i = 0; i < impl->count; ++i) IWebAccount_Release( impl->accounts[i] );
        if (impl->error) IWebProviderError_Release( impl->error );
        free( impl->accounts );
        free( impl );
    }
    return ref;
}

static HRESULT WINAPI result_GetIids( IFindAllAccountsResult *iface, ULONG *count, IID **iids )
{
    if (!count || !iids) return E_POINTER;
    *count = 0;
    if (!(*iids = CoTaskMemAlloc( 3 * sizeof(**iids) ))) return E_OUTOFMEMORY;
    (*iids)[0] = IID_IFindAllAccountsResult;
    (*iids)[1] = IID_IVectorView_WebAccount;
    (*iids)[2] = IID_IIterable_WebAccount;
    *count = 3;
    return S_OK;
}

static HRESULT WINAPI result_GetRuntimeClassName( IFindAllAccountsResult *iface, HSTRING *name )
{
    const WCHAR *class_name = RuntimeClass_Windows_Security_Authentication_Web_Core_FindAllAccountsResult;
    if (!name) return E_POINTER;
    return WindowsCreateString( class_name, wcslen( class_name ), name );
}

static HRESULT WINAPI result_GetTrustLevel( IFindAllAccountsResult *iface, TrustLevel *level )
{
    if (!level) return E_POINTER;
    *level = BaseTrust;
    return S_OK;
}

static HRESULT WINAPI result_get_Accounts( IFindAllAccountsResult *iface, IVectorView_WebAccount **out )
{
    if (!out) return E_POINTER;
    *out = &impl_from_result( iface )->IVectorView_iface;
    IVectorView_WebAccount_AddRef( *out );
    return S_OK;
}

static HRESULT WINAPI result_get_Status( IFindAllAccountsResult *iface, FindAllWebAccountsStatus *out )
{
    if (!out) return E_POINTER;
    *out = impl_from_result( iface )->error ? FindAllWebAccountsStatus_ProviderError : FindAllWebAccountsStatus_Success;
    return S_OK;
}

static HRESULT WINAPI result_get_ProviderError( IFindAllAccountsResult *iface, IWebProviderError **out )
{
    if (!out) return E_POINTER;
    *out = impl_from_result( iface )->error;
    if (*out) IWebProviderError_AddRef( *out );
    return S_OK;
}

static const IFindAllAccountsResultVtbl result_vtbl =
{
    result_QueryInterface,
    result_AddRef,
    result_Release,
    result_GetIids,
    result_GetRuntimeClassName,
    result_GetTrustLevel,
    result_get_Accounts,
    result_get_Status,
    result_get_ProviderError,
};

DEFINE_IINSPECTABLE_( view, IVectorView_WebAccount, struct accounts_result,
                     impl_from_view, IVectorView_iface, &impl->IFindAllAccountsResult_iface )

static HRESULT WINAPI view_GetAt( IVectorView_WebAccount *iface, UINT32 index, IWebAccount **out )
{
    struct accounts_result *impl = impl_from_view( iface );
    if (!out) return E_POINTER;
    *out = NULL;
    if (index >= impl->count) return E_BOUNDS;
    *out = impl->accounts[index];
    IWebAccount_AddRef( *out );
    return S_OK;
}

static HRESULT WINAPI view_get_Size( IVectorView_WebAccount *iface, UINT32 *out )
{
    if (!out) return E_POINTER;
    *out = impl_from_view( iface )->count;
    return S_OK;
}

static HRESULT WINAPI view_IndexOf( IVectorView_WebAccount *iface, IWebAccount *account, UINT32 *index, boolean *found )
{
    struct accounts_result *impl = impl_from_view( iface );
    UINT32 i;
    if (!index || !found) return E_POINTER;
    *index = 0;
    *found = FALSE;
    for (i = 0; i < impl->count; ++i)
    {
        if (impl->accounts[i] != account) continue;
        *index = i;
        *found = TRUE;
        break;
    }
    return S_OK;
}

static HRESULT WINAPI view_GetMany( IVectorView_WebAccount *iface, UINT32 start, UINT32 capacity,
                                    IWebAccount **items, UINT32 *count )
{
    struct accounts_result *impl = impl_from_view( iface );
    UINT32 i;
    if (!count || (capacity && !items)) return E_POINTER;
    *count = 0;
    if (start > impl->count) return E_BOUNDS;
    for (i = 0; i < min( capacity, impl->count - start ); ++i)
    {
        items[i] = impl->accounts[start + i];
        IWebAccount_AddRef( items[i] );
    }
    *count = i;
    return S_OK;
}

static const IVectorView_WebAccountVtbl view_vtbl =
{
    view_QueryInterface,
    view_AddRef,
    view_Release,
    view_GetIids,
    view_GetRuntimeClassName,
    view_GetTrustLevel,
    view_GetAt,
    view_get_Size,
    view_IndexOf,
    view_GetMany,
};

struct account_iterator
{
    IIterator_WebAccount IIterator_iface;
    LONG ref;
    IVectorView_WebAccount *view;
    UINT32 index;
};

static inline struct account_iterator *impl_from_iterator( IIterator_WebAccount *iface )
{
    return CONTAINING_RECORD( iface, struct account_iterator, IIterator_iface );
}

static HRESULT WINAPI iterator_QueryInterface( IIterator_WebAccount *iface, REFIID iid, void **out )
{
    if (!out) return E_POINTER;
    *out = NULL;
    if (!IsEqualGUID( iid, &IID_IUnknown ) && !IsEqualGUID( iid, &IID_IInspectable ) &&
        !IsEqualGUID( iid, &IID_IIterator_WebAccount )) return E_NOINTERFACE;
    *out = iface;
    IIterator_WebAccount_AddRef( iface );
    return S_OK;
}

static ULONG WINAPI iterator_AddRef( IIterator_WebAccount *iface )
{
    return InterlockedIncrement( &impl_from_iterator( iface )->ref );
}

static ULONG WINAPI iterator_Release( IIterator_WebAccount *iface )
{
    struct account_iterator *impl = impl_from_iterator( iface );
    ULONG ref = InterlockedDecrement( &impl->ref );
    if (!ref)
    {
        IVectorView_WebAccount_Release( impl->view );
        free( impl );
    }
    return ref;
}

static HRESULT WINAPI iterator_GetIids( IIterator_WebAccount *iface, ULONG *count, IID **iids )
{
    if (!count || !iids) return E_POINTER;
    *count = 0;
    if (!(*iids = CoTaskMemAlloc( sizeof(**iids) ))) return E_OUTOFMEMORY;
    **iids = IID_IIterator_WebAccount;
    *count = 1;
    return S_OK;
}

static HRESULT WINAPI iterator_GetRuntimeClassName( IIterator_WebAccount *iface, HSTRING *name )
{
    static const WCHAR class_name[] = L"Windows.Foundation.Collections.IIterator`1<Windows.Security.Credentials.WebAccount>";
    if (!name) return E_POINTER;
    return WindowsCreateString( class_name, ARRAY_SIZE(class_name) - 1, name );
}

static HRESULT WINAPI iterator_GetTrustLevel( IIterator_WebAccount *iface, TrustLevel *level )
{
    if (!level) return E_POINTER;
    *level = BaseTrust;
    return S_OK;
}

static HRESULT WINAPI iterator_get_Current( IIterator_WebAccount *iface, IWebAccount **out )
{
    struct account_iterator *impl = impl_from_iterator( iface );
    return IVectorView_WebAccount_GetAt( impl->view, impl->index, out );
}

static HRESULT WINAPI iterator_get_HasCurrent( IIterator_WebAccount *iface, boolean *out )
{
    struct account_iterator *impl = impl_from_iterator( iface );
    UINT32 size;
    if (!out) return E_POINTER;
    IVectorView_WebAccount_get_Size( impl->view, &size );
    *out = impl->index < size;
    return S_OK;
}

static HRESULT WINAPI iterator_MoveNext( IIterator_WebAccount *iface, boolean *out )
{
    struct account_iterator *impl = impl_from_iterator( iface );
    UINT32 size;
    if (!out) return E_POINTER;
    IVectorView_WebAccount_get_Size( impl->view, &size );
    if (impl->index < size) ++impl->index;
    *out = impl->index < size;
    return S_OK;
}

static HRESULT WINAPI iterator_GetMany( IIterator_WebAccount *iface, UINT32 capacity, IWebAccount **items, UINT32 *count )
{
    struct account_iterator *impl = impl_from_iterator( iface );
    HRESULT hr = IVectorView_WebAccount_GetMany( impl->view, impl->index, capacity, items, count );
    if (SUCCEEDED(hr)) impl->index += *count;
    return hr;
}

static const IIterator_WebAccountVtbl iterator_vtbl =
{
    iterator_QueryInterface,
    iterator_AddRef,
    iterator_Release,
    iterator_GetIids,
    iterator_GetRuntimeClassName,
    iterator_GetTrustLevel,
    iterator_get_Current,
    iterator_get_HasCurrent,
    iterator_MoveNext,
    iterator_GetMany,
};

DEFINE_IINSPECTABLE_( iterable, IIterable_WebAccount, struct accounts_result,
                     impl_from_iterable, IIterable_iface, &impl->IFindAllAccountsResult_iface )

static HRESULT WINAPI iterable_First( IIterable_WebAccount *iface, IIterator_WebAccount **out )
{
    struct account_iterator *impl;
    if (!out) return E_POINTER;
    *out = NULL;
    if (!(impl = calloc( 1, sizeof(*impl) ))) return E_OUTOFMEMORY;
    impl->IIterator_iface.lpVtbl = &iterator_vtbl;
    impl->ref = 1;
    impl->view = &impl_from_iterable( iface )->IVectorView_iface;
    IVectorView_WebAccount_AddRef( impl->view );
    *out = &impl->IIterator_iface;
    return S_OK;
}

static const IIterable_WebAccountVtbl iterable_vtbl =
{
    iterable_QueryInterface,
    iterable_AddRef,
    iterable_Release,
    iterable_GetIids,
    iterable_GetRuntimeClassName,
    iterable_GetTrustLevel,
    iterable_First,
};

static HRESULT read_string( const BYTE **data, UINT32 *remaining, HSTRING *out )
{
    UINT32 size;
    WCHAR *buffer;
    int count;
    HRESULT hr;

    *out = NULL;
    if (*remaining < sizeof(size)) return E_INVALIDARG;
    memcpy( &size, *data, sizeof(size) );
    *data += sizeof(size);
    *remaining -= sizeof(size);
    if (!size || size > 4096 || size > *remaining || memchr( *data, 0, size )) return E_INVALIDARG;
    if (!(count = MultiByteToWideChar( CP_UTF8, MB_ERR_INVALID_CHARS, (const char *)*data, size, NULL, 0 )))
        return E_INVALIDARG;
    if (!(buffer = malloc( count * sizeof(*buffer) ))) return E_OUTOFMEMORY;
    MultiByteToWideChar( CP_UTF8, MB_ERR_INVALID_CHARS, (const char *)*data, size, buffer, count );
    hr = WindowsCreateString( buffer, count, out );
    free( buffer );
    *data += size;
    *remaining -= size;
    return hr;
}

HRESULT web_accounts_result_create( IWebAccountProvider *provider, const struct identity_reply *reply,
                                    IFindAllAccountsResult **out )
{
    struct accounts_result *impl;
    IWebAccountProvider2 *provider2;
    const BYTE *data = reply->accounts;
    UINT32 count, remaining = reply->accounts_size, i;
    HSTRING id = NULL, username = NULL, authority = NULL;
    HRESULT hr = S_OK;

    if (!out) return E_POINTER;
    *out = NULL;
    if (!(impl = calloc( 1, sizeof(*impl) ))) return E_OUTOFMEMORY;
    impl->IFindAllAccountsResult_iface.lpVtbl = &result_vtbl;
    impl->IVectorView_iface.lpVtbl = &view_vtbl;
    impl->IIterable_iface.lpVtbl = &iterable_vtbl;
    impl->ref = 1;
    if (FAILED(reply->error))
    {
        hr = web_provider_error_create( reply->error, reply->error_message, &impl->error );
        goto done;
    }
    if (remaining < sizeof(count))
    {
        hr = E_INVALIDARG;
        goto done;
    }
    memcpy( &count, data, sizeof(count) );
    data += sizeof(count);
    remaining -= sizeof(count);
    if (count > 128)
    {
        hr = E_INVALIDARG;
        goto done;
    }
    if (count && !(impl->accounts = calloc( count, sizeof(*impl->accounts) )))
    {
        hr = E_OUTOFMEMORY;
        goto done;
    }
    if (count && SUCCEEDED(IWebAccountProvider_QueryInterface( provider, &IID_IWebAccountProvider2,
                                                              (void **)&provider2 )))
    {
        hr = IWebAccountProvider2_get_Authority( provider2, &authority );
        IWebAccountProvider2_Release( provider2 );
        if (FAILED(hr)) goto done;
    }
    for (i = 0; i < count; ++i)
    {
        if (FAILED(hr = read_string( &data, &remaining, &id )) ||
            FAILED(hr = read_string( &data, &remaining, &username )) ||
            FAILED(hr = web_account_create( provider, id, username, WebAccountState_Connected,
                                            &impl->accounts[impl->count] ))) goto done;
        ++impl->count;
        if (FAILED(hr = web_account_set_customer_id( impl->accounts[i], authority ))) goto done;
        WindowsDeleteString( id );
        WindowsDeleteString( username );
        id = username = NULL;
    }
    if (remaining) hr = E_INVALIDARG;

done:
    WindowsDeleteString( authority );
    WindowsDeleteString( id );
    WindowsDeleteString( username );
    if (FAILED(hr)) IFindAllAccountsResult_Release( &impl->IFindAllAccountsResult_iface );
    else *out = &impl->IFindAllAccountsResult_iface;
    return hr;
}
