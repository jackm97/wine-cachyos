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

WINE_DEFAULT_DEBUG_CHANNEL(twinapi);

static inline struct winrt_factory *impl_from_IActivationFactory( IActivationFactory *iface )
{
    return CONTAINING_RECORD( iface, struct winrt_factory, IActivationFactory_iface );
}

static HRESULT WINAPI factory_QueryInterface( IActivationFactory *iface, REFIID iid, void **out )
{
    struct winrt_factory *impl = impl_from_IActivationFactory( iface );
    UINT32 i;

    TRACE( "iface %p, iid %s, out %p.\n", iface, debugstr_guid(iid), out );

    if (!out) return E_POINTER;
    *out = NULL;

    if (IsEqualGUID( iid, &IID_IUnknown ) || IsEqualGUID( iid, &IID_IInspectable ) ||
        IsEqualGUID( iid, &IID_IActivationFactory ) || (impl->agile && IsEqualGUID( iid, &IID_IAgileObject )))
        *out = &impl->IActivationFactory_iface;
    else
    {
        for (i = 0; i < impl->interface_count; ++i)
        {
            if (!IsEqualGUID( iid, impl->interfaces[i].iid )) continue;
            *out = (char *)impl + impl->interfaces[i].offset;
            break;
        }
    }

    if (!*out)
    {
        FIXME( "%s not implemented, returning E_NOINTERFACE.\n", debugstr_guid(iid) );
        return E_NOINTERFACE;
    }

    IActivationFactory_AddRef( &impl->IActivationFactory_iface );
    return S_OK;
}

static ULONG WINAPI factory_AddRef( IActivationFactory *iface )
{
    struct winrt_factory *impl = impl_from_IActivationFactory( iface );
    ULONG ref = InterlockedIncrement( &impl->ref );

    TRACE( "iface %p, ref %lu.\n", iface, ref );
    return ref;
}

static ULONG WINAPI factory_Release( IActivationFactory *iface )
{
    struct winrt_factory *impl = impl_from_IActivationFactory( iface );
    ULONG ref = InterlockedDecrement( &impl->ref );

    TRACE( "iface %p, ref %lu.\n", iface, ref );
    return ref;
}

static HRESULT WINAPI factory_GetIids( IActivationFactory *iface, ULONG *iid_count, IID **iids )
{
    struct winrt_factory *impl = impl_from_IActivationFactory( iface );
    IID *result;
    UINT32 i;

    TRACE( "iface %p, iid_count %p, iids %p.\n", iface, iid_count, iids );

    if (!iid_count || !iids) return E_POINTER;
    *iid_count = 0;
    *iids = NULL;
    if (!impl->interface_count) return S_OK;
    if (!(result = CoTaskMemAlloc( impl->interface_count * sizeof(*result) ))) return E_OUTOFMEMORY;

    for (i = 0; i < impl->interface_count; ++i) result[i] = *impl->interfaces[i].iid;
    *iid_count = impl->interface_count;
    *iids = result;
    return S_OK;
}

static HRESULT WINAPI factory_GetRuntimeClassName( IActivationFactory *iface, HSTRING *class_name )
{
    struct winrt_factory *impl = impl_from_IActivationFactory( iface );

    TRACE( "iface %p, class_name %p.\n", iface, class_name );

    if (!class_name) return E_POINTER;
    return WindowsCreateString( impl->class_name, wcslen(impl->class_name), class_name );
}

static HRESULT WINAPI factory_GetTrustLevel( IActivationFactory *iface, TrustLevel *trust_level )
{
    TRACE( "iface %p, trust_level %p.\n", iface, trust_level );

    if (!trust_level) return E_POINTER;
    *trust_level = BaseTrust;
    return S_OK;
}

static HRESULT WINAPI factory_ActivateInstance( IActivationFactory *iface, IInspectable **instance )
{
    struct winrt_factory *impl = impl_from_IActivationFactory( iface );

    TRACE( "iface %p, instance %p.\n", iface, instance );

    if (!instance) return E_POINTER;
    *instance = NULL;
    if (!impl->activate) return E_NOTIMPL;
    return impl->activate( instance );
}

const IActivationFactoryVtbl winrt_factory_vtbl =
{
    factory_QueryInterface,
    factory_AddRef,
    factory_Release,
    /* IInspectable methods */
    factory_GetIids,
    factory_GetRuntimeClassName,
    factory_GetTrustLevel,
    /* IActivationFactory methods */
    factory_ActivateInstance,
};
