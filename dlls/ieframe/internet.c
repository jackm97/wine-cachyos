/*
 * Internet shell folder
 *
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

#include <stddef.h>

#include "ieframe.h"

#include "shlobj.h"
#include "shlwapi.h"
#include "shlguid.h"

#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(ieframe);

typedef struct {
    IShellFolder IShellFolder_iface;
    IPersistFolder2 IPersistFolder2_iface;

    LONG ref;

    LPITEMIDLIST pidl;
} InternetFolder;

/* Child items hold the URL they were parsed from. */
#include "pshpack1.h"
typedef struct {
    WORD cb;
    BYTE type;
    BYTE reserved;
    WCHAR url[1];
} url_item;
#include "poppack.h"

#define URL_ITEM_TYPE 0x61

static inline InternetFolder *impl_from_IShellFolder(IShellFolder *iface)
{
    return CONTAINING_RECORD(iface, InternetFolder, IShellFolder_iface);
}

static inline InternetFolder *impl_from_IPersistFolder2(IPersistFolder2 *iface)
{
    return CONTAINING_RECORD(iface, InternetFolder, IPersistFolder2_iface);
}

static BOOL is_internet_url(const WCHAR *name)
{
    PARSEDURLW url = { sizeof(url) };

    if(FAILED(ParseURLW(name, &url)))
        return FALSE;

    /* File URLs name file system items, everything else belongs here. */
    return url.nScheme != URL_SCHEME_FILE;
}

static const WCHAR *url_from_pidl(LPCITEMIDLIST pidl)
{
    const url_item *item = (const url_item*)pidl;
    DWORD len;

    if(!pidl || item->cb < offsetof(url_item, url) + sizeof(WCHAR) || item->type != URL_ITEM_TYPE)
        return NULL;

    len = (item->cb - offsetof(url_item, url)) / sizeof(WCHAR);
    if(item->url[len - 1])
        return NULL;

    /* Only single level child items belong to this folder. */
    if(((const SHITEMID*)((const BYTE*)pidl + item->cb))->cb)
        return NULL;

    return item->url;
}

static LPITEMIDLIST create_url_pidl(const WCHAR *url)
{
    DWORD size = offsetof(url_item, url) + (lstrlenW(url) + 1) * sizeof(WCHAR);
    url_item *item;

    if(size > 0xfff0)
        return NULL;

    item = CoTaskMemAlloc(size + sizeof(WORD));
    if(!item)
        return NULL;

    item->cb = size;
    item->type = URL_ITEM_TYPE;
    item->reserved = 0;
    lstrcpyW(item->url, url);
    *(WORD*)((BYTE*)item + size) = 0;
    return (LPITEMIDLIST)item;
}

static HRESULT WINAPI InternetFolder_QueryInterface(IShellFolder *iface, REFIID riid, void **ppv)
{
    InternetFolder *This = impl_from_IShellFolder(iface);

    if(IsEqualGUID(&IID_IUnknown, riid) || IsEqualGUID(&IID_IShellFolder, riid)) {
        TRACE("(%p)->(%s %p)\n", This, debugstr_guid(riid), ppv);
        *ppv = &This->IShellFolder_iface;
    }else if(IsEqualGUID(&IID_IPersist, riid) || IsEqualGUID(&IID_IPersistFolder, riid)
            || IsEqualGUID(&IID_IPersistFolder2, riid)) {
        TRACE("(%p)->(%s %p)\n", This, debugstr_guid(riid), ppv);
        *ppv = &This->IPersistFolder2_iface;
    }else {
        WARN("(%p)->(%s %p) interface not supported\n", This, debugstr_guid(riid), ppv);
        *ppv = NULL;
        return E_NOINTERFACE;
    }

    IUnknown_AddRef((IUnknown*)*ppv);
    return S_OK;
}

static ULONG WINAPI InternetFolder_AddRef(IShellFolder *iface)
{
    InternetFolder *This = impl_from_IShellFolder(iface);
    LONG ref = InterlockedIncrement(&This->ref);

    TRACE("(%p) ref=%ld\n", This, ref);

    return ref;
}

static ULONG WINAPI InternetFolder_Release(IShellFolder *iface)
{
    InternetFolder *This = impl_from_IShellFolder(iface);
    LONG ref = InterlockedDecrement(&This->ref);

    TRACE("(%p) ref=%ld\n", This, ref);

    if(!ref) {
        ILFree(This->pidl);
        free(This);
        unlock_module();
    }

    return ref;
}

static HRESULT WINAPI InternetFolder_ParseDisplayName(IShellFolder *iface, HWND hwnd, IBindCtx *pbc,
        LPOLESTR display_name, ULONG *eaten, LPITEMIDLIST *pidl, ULONG *attributes)
{
    InternetFolder *This = impl_from_IShellFolder(iface);

    TRACE("(%p)->(%p %p %s %p %p %p)\n", This, hwnd, pbc, debugstr_w(display_name), eaten, pidl, attributes);

    if(!display_name || !pidl)
        return E_INVALIDARG;

    *pidl = NULL;

    if(!is_internet_url(display_name))
        return E_INVALIDARG;

    *pidl = create_url_pidl(display_name);
    if(!*pidl)
        return E_OUTOFMEMORY;

    if(eaten)
        *eaten = lstrlenW(display_name);
    if(attributes)
        *attributes &= SFGAO_CANLINK;
    return S_OK;
}

static HRESULT WINAPI InternetFolder_EnumObjects(IShellFolder *iface, HWND hwnd, SHCONTF flags,
        IEnumIDList **list)
{
    InternetFolder *This = impl_from_IShellFolder(iface);
    FIXME("(%p)->(%p %lx %p)\n", This, hwnd, flags, list);
    return E_NOTIMPL;
}

static HRESULT WINAPI InternetFolder_BindToObject(IShellFolder *iface, LPCITEMIDLIST pidl, IBindCtx *pbc,
        REFIID riid, void **ppv)
{
    InternetFolder *This = impl_from_IShellFolder(iface);
    FIXME("(%p)->(%p %p %s %p)\n", This, pidl, pbc, debugstr_guid(riid), ppv);
    return E_NOTIMPL;
}

static HRESULT WINAPI InternetFolder_BindToStorage(IShellFolder *iface, LPCITEMIDLIST pidl, IBindCtx *pbc,
        REFIID riid, void **ppv)
{
    InternetFolder *This = impl_from_IShellFolder(iface);
    FIXME("(%p)->(%p %p %s %p)\n", This, pidl, pbc, debugstr_guid(riid), ppv);
    return E_NOTIMPL;
}

static HRESULT WINAPI InternetFolder_CompareIDs(IShellFolder *iface, LPARAM lparam,
        LPCITEMIDLIST pidl1, LPCITEMIDLIST pidl2)
{
    InternetFolder *This = impl_from_IShellFolder(iface);
    const WCHAR *url1 = url_from_pidl(pidl1), *url2 = url_from_pidl(pidl2);
    int ret;

    TRACE("(%p)->(%Ix %p %p)\n", This, lparam, pidl1, pidl2);

    if(!url1 || !url2)
        return E_INVALIDARG;

    ret = CompareStringOrdinal(url1, -1, url2, -1, TRUE) - CSTR_EQUAL;
    return MAKE_HRESULT(SEVERITY_SUCCESS, 0, (USHORT)ret);
}

static HRESULT WINAPI InternetFolder_CreateViewObject(IShellFolder *iface, HWND hwnd, REFIID riid, void **ppv)
{
    InternetFolder *This = impl_from_IShellFolder(iface);
    FIXME("(%p)->(%p %s %p)\n", This, hwnd, debugstr_guid(riid), ppv);
    return E_NOTIMPL;
}

static HRESULT WINAPI InternetFolder_GetAttributesOf(IShellFolder *iface, UINT cidl, LPCITEMIDLIST *apidl,
        SFGAOF *attributes)
{
    InternetFolder *This = impl_from_IShellFolder(iface);
    UINT i;

    TRACE("(%p)->(%u %p %p)\n", This, cidl, apidl, attributes);

    if(!attributes || (cidl && !apidl))
        return E_INVALIDARG;

    for(i = 0; i < cidl; i++) {
        if(!url_from_pidl(apidl[i]))
            return E_INVALIDARG;
    }

    *attributes &= SFGAO_CANLINK;
    return S_OK;
}

static HRESULT WINAPI InternetFolder_GetUIObjectOf(IShellFolder *iface, HWND hwnd, UINT cidl,
        LPCITEMIDLIST *apidl, REFIID riid, UINT *reserved, void **ppv)
{
    InternetFolder *This = impl_from_IShellFolder(iface);
    FIXME("(%p)->(%p %u %p %s %p %p)\n", This, hwnd, cidl, apidl, debugstr_guid(riid), reserved, ppv);
    return E_NOTIMPL;
}

static HRESULT WINAPI InternetFolder_GetDisplayNameOf(IShellFolder *iface, LPCITEMIDLIST pidl,
        SHGDNF flags, STRRET *name)
{
    InternetFolder *This = impl_from_IShellFolder(iface);
    const WCHAR *url = url_from_pidl(pidl);

    TRACE("(%p)->(%p %lx %p)\n", This, pidl, flags, name);

    if(!name || !url)
        return E_INVALIDARG;

    name->pOleStr = CoTaskMemAlloc((lstrlenW(url) + 1) * sizeof(WCHAR));
    if(!name->pOleStr)
        return E_OUTOFMEMORY;

    lstrcpyW(name->pOleStr, url);
    name->uType = STRRET_WSTR;
    return S_OK;
}

static HRESULT WINAPI InternetFolder_SetNameOf(IShellFolder *iface, HWND hwnd, LPCITEMIDLIST pidl,
        LPCOLESTR name, SHGDNF flags, LPITEMIDLIST *out)
{
    InternetFolder *This = impl_from_IShellFolder(iface);
    FIXME("(%p)->(%p %p %s %lx %p)\n", This, hwnd, pidl, debugstr_w(name), flags, out);
    return E_NOTIMPL;
}

static const IShellFolderVtbl InternetFolderVtbl = {
    InternetFolder_QueryInterface,
    InternetFolder_AddRef,
    InternetFolder_Release,
    InternetFolder_ParseDisplayName,
    InternetFolder_EnumObjects,
    InternetFolder_BindToObject,
    InternetFolder_BindToStorage,
    InternetFolder_CompareIDs,
    InternetFolder_CreateViewObject,
    InternetFolder_GetAttributesOf,
    InternetFolder_GetUIObjectOf,
    InternetFolder_GetDisplayNameOf,
    InternetFolder_SetNameOf
};

static HRESULT WINAPI PersistFolder_QueryInterface(IPersistFolder2 *iface, REFIID riid, void **ppv)
{
    InternetFolder *This = impl_from_IPersistFolder2(iface);
    return IShellFolder_QueryInterface(&This->IShellFolder_iface, riid, ppv);
}

static ULONG WINAPI PersistFolder_AddRef(IPersistFolder2 *iface)
{
    InternetFolder *This = impl_from_IPersistFolder2(iface);
    return IShellFolder_AddRef(&This->IShellFolder_iface);
}

static ULONG WINAPI PersistFolder_Release(IPersistFolder2 *iface)
{
    InternetFolder *This = impl_from_IPersistFolder2(iface);
    return IShellFolder_Release(&This->IShellFolder_iface);
}

static HRESULT WINAPI PersistFolder_GetClassID(IPersistFolder2 *iface, CLSID *clsid)
{
    InternetFolder *This = impl_from_IPersistFolder2(iface);

    TRACE("(%p)->(%p)\n", This, clsid);

    if(!clsid)
        return E_INVALIDARG;

    *clsid = CLSID_Internet;
    return S_OK;
}

static HRESULT WINAPI PersistFolder_Initialize(IPersistFolder2 *iface, LPCITEMIDLIST pidl)
{
    InternetFolder *This = impl_from_IPersistFolder2(iface);

    TRACE("(%p)->(%p)\n", This, pidl);

    ILFree(This->pidl);
    This->pidl = ILClone(pidl);
    return pidl && !This->pidl ? E_OUTOFMEMORY : S_OK;
}

static HRESULT WINAPI PersistFolder_GetCurFolder(IPersistFolder2 *iface, LPITEMIDLIST *pidl)
{
    InternetFolder *This = impl_from_IPersistFolder2(iface);

    TRACE("(%p)->(%p)\n", This, pidl);

    if(!pidl)
        return E_POINTER;

    *pidl = ILClone(This->pidl);
    return *pidl ? S_OK : S_FALSE;
}

static const IPersistFolder2Vtbl PersistFolder2Vtbl = {
    PersistFolder_QueryInterface,
    PersistFolder_AddRef,
    PersistFolder_Release,
    PersistFolder_GetClassID,
    PersistFolder_Initialize,
    PersistFolder_GetCurFolder
};

HRESULT WINAPI InternetFolder_Create(IClassFactory *iface, IUnknown *outer, REFIID riid, void **ppv)
{
    InternetFolder *This;
    HRESULT hres;

    TRACE("(%p, %s, %p)\n", outer, debugstr_guid(riid), ppv);

    *ppv = NULL;

    if(outer)
        return CLASS_E_NOAGGREGATION;

    This = calloc(1, sizeof(*This));
    if(!This)
        return E_OUTOFMEMORY;

    This->IShellFolder_iface.lpVtbl = &InternetFolderVtbl;
    This->IPersistFolder2_iface.lpVtbl = &PersistFolder2Vtbl;
    This->ref = 1;
    lock_module();

    hres = IShellFolder_QueryInterface(&This->IShellFolder_iface, riid, ppv);
    IShellFolder_Release(&This->IShellFolder_iface);
    return hres;
}
