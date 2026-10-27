/* Windows.UI.Composition effect sources
 *
 * Copyright (C) 2026 Mirko Brombin
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA
 */

#include "private.h"
#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(ui);

struct effect_source_parameter
{
    ICompositionEffectSourceParameter ICompositionEffectSourceParameter_iface;
    IGraphicsEffectSource IGraphicsEffectSource_iface;
    LONG ref;
    HSTRING name;
};

static inline struct effect_source_parameter *impl_from_ICompositionEffectSourceParameter(
        ICompositionEffectSourceParameter *iface )
{
    return CONTAINING_RECORD( iface, struct effect_source_parameter, ICompositionEffectSourceParameter_iface );
}

static HRESULT WINAPI parameter_QueryInterface( ICompositionEffectSourceParameter *iface, REFIID iid, void **out )
{
    struct effect_source_parameter *impl = impl_from_ICompositionEffectSourceParameter( iface );

    TRACE( "iface %p, iid %s, out %p.\n", iface, debugstr_guid(iid), out );
    if (!out) return E_POINTER;
    *out = NULL;
    if (IsEqualGUID( iid, &IID_IUnknown ) || IsEqualGUID( iid, &IID_IInspectable ) ||
        IsEqualGUID( iid, &IID_IAgileObject ) || IsEqualGUID( iid, &IID_ICompositionEffectSourceParameter ))
        *out = iface;
    else if (IsEqualGUID( iid, &IID_IGraphicsEffectSource ))
        *out = &impl->IGraphicsEffectSource_iface;
    else return E_NOINTERFACE;

    IUnknown_AddRef( (IUnknown *)*out );
    return S_OK;
}

static ULONG WINAPI parameter_AddRef( ICompositionEffectSourceParameter *iface )
{
    return InterlockedIncrement( &impl_from_ICompositionEffectSourceParameter( iface )->ref );
}

static ULONG WINAPI parameter_Release( ICompositionEffectSourceParameter *iface )
{
    struct effect_source_parameter *impl = impl_from_ICompositionEffectSourceParameter( iface );
    ULONG ref = InterlockedDecrement( &impl->ref );

    if (!ref)
    {
        WindowsDeleteString( impl->name );
        free( impl );
    }
    return ref;
}

static HRESULT WINAPI parameter_GetIids( ICompositionEffectSourceParameter *iface, ULONG *count, IID **iids )
{
    if (!count || !iids) return E_POINTER;
    *count = 0;
    *iids = CoTaskMemAlloc( 2 * sizeof(**iids) );
    if (!*iids) return E_OUTOFMEMORY;
    (*iids)[0] = IID_ICompositionEffectSourceParameter;
    (*iids)[1] = IID_IGraphicsEffectSource;
    *count = 2;
    return S_OK;
}

static HRESULT parameter_class_name( HSTRING *name )
{
    const WCHAR *class_name = RuntimeClass_Windows_UI_Composition_CompositionEffectSourceParameter;

    if (!name) return E_POINTER;
    return WindowsCreateString( class_name, wcslen(class_name), name );
}

static HRESULT WINAPI parameter_GetRuntimeClassName( ICompositionEffectSourceParameter *iface, HSTRING *name )
{
    return parameter_class_name( name );
}

static HRESULT WINAPI parameter_GetTrustLevel( ICompositionEffectSourceParameter *iface, TrustLevel *level )
{
    if (!level) return E_POINTER;
    *level = BaseTrust;
    return S_OK;
}

static HRESULT WINAPI parameter_get_Name( ICompositionEffectSourceParameter *iface, HSTRING *name )
{
    struct effect_source_parameter *impl = impl_from_ICompositionEffectSourceParameter( iface );

    if (!name) return E_POINTER;
    return WindowsDuplicateString( impl->name, name );
}

static const ICompositionEffectSourceParameterVtbl parameter_vtbl =
{
    parameter_QueryInterface,
    parameter_AddRef,
    parameter_Release,
    parameter_GetIids,
    parameter_GetRuntimeClassName,
    parameter_GetTrustLevel,
    parameter_get_Name,
};

DEFINE_IINSPECTABLE( parameter_source, IGraphicsEffectSource, struct effect_source_parameter,
                    ICompositionEffectSourceParameter_iface )

static const IGraphicsEffectSourceVtbl parameter_source_vtbl =
{
    parameter_source_QueryInterface,
    parameter_source_AddRef,
    parameter_source_Release,
    parameter_source_GetIids,
    parameter_source_GetRuntimeClassName,
    parameter_source_GetTrustLevel,
};

struct parameter_factory
{
    IActivationFactory IActivationFactory_iface;
    ICompositionEffectSourceParameterFactory ICompositionEffectSourceParameterFactory_iface;
    LONG ref;
};

static inline struct parameter_factory *impl_from_IActivationFactory( IActivationFactory *iface )
{
    return CONTAINING_RECORD( iface, struct parameter_factory, IActivationFactory_iface );
}

static HRESULT WINAPI parameter_factory_QueryInterface( IActivationFactory *iface, REFIID iid, void **out )
{
    struct parameter_factory *impl = impl_from_IActivationFactory( iface );

    TRACE( "iface %p, iid %s, out %p.\n", iface, debugstr_guid(iid), out );
    if (!out) return E_POINTER;
    *out = NULL;
    if (IsEqualGUID( iid, &IID_IUnknown ) || IsEqualGUID( iid, &IID_IInspectable ) ||
        IsEqualGUID( iid, &IID_IAgileObject ) || IsEqualGUID( iid, &IID_IActivationFactory ))
        *out = iface;
    else if (IsEqualGUID( iid, &IID_ICompositionEffectSourceParameterFactory ))
        *out = &impl->ICompositionEffectSourceParameterFactory_iface;
    else return E_NOINTERFACE;

    IUnknown_AddRef( (IUnknown *)*out );
    return S_OK;
}

static ULONG WINAPI parameter_factory_AddRef( IActivationFactory *iface )
{
    return InterlockedIncrement( &impl_from_IActivationFactory( iface )->ref );
}

static ULONG WINAPI parameter_factory_Release( IActivationFactory *iface )
{
    return InterlockedDecrement( &impl_from_IActivationFactory( iface )->ref );
}

static HRESULT WINAPI parameter_factory_GetIids( IActivationFactory *iface, ULONG *count, IID **iids )
{
    if (!count || !iids) return E_POINTER;
    *count = 0;
    *iids = CoTaskMemAlloc( sizeof(**iids) );
    if (!*iids) return E_OUTOFMEMORY;
    **iids = IID_ICompositionEffectSourceParameterFactory;
    *count = 1;
    return S_OK;
}

static HRESULT WINAPI parameter_factory_GetRuntimeClassName( IActivationFactory *iface, HSTRING *name )
{
    return parameter_class_name( name );
}

static HRESULT WINAPI parameter_factory_GetTrustLevel( IActivationFactory *iface, TrustLevel *level )
{
    if (!level) return E_POINTER;
    *level = BaseTrust;
    return S_OK;
}

static HRESULT WINAPI parameter_factory_ActivateInstance( IActivationFactory *iface, IInspectable **instance )
{
    if (!instance) return E_POINTER;
    *instance = NULL;
    return E_NOTIMPL;
}

static const IActivationFactoryVtbl parameter_factory_vtbl =
{
    parameter_factory_QueryInterface,
    parameter_factory_AddRef,
    parameter_factory_Release,
    parameter_factory_GetIids,
    parameter_factory_GetRuntimeClassName,
    parameter_factory_GetTrustLevel,
    parameter_factory_ActivateInstance,
};

DEFINE_IINSPECTABLE( parameter_constructor, ICompositionEffectSourceParameterFactory, struct parameter_factory,
                    IActivationFactory_iface )

static HRESULT WINAPI parameter_constructor_Create( ICompositionEffectSourceParameterFactory *iface,
        HSTRING name, ICompositionEffectSourceParameter **result )
{
    struct effect_source_parameter *impl;
    HRESULT hr;

    TRACE( "iface %p, name %s, result %p.\n", iface, debugstr_hstring(name), result );
    if (!result) return E_POINTER;
    *result = NULL;
    if (!(impl = calloc( 1, sizeof(*impl) ))) return E_OUTOFMEMORY;
    if (FAILED(hr = WindowsDuplicateString( name, &impl->name )))
    {
        free( impl );
        return hr;
    }
    impl->ICompositionEffectSourceParameter_iface.lpVtbl = &parameter_vtbl;
    impl->IGraphicsEffectSource_iface.lpVtbl = &parameter_source_vtbl;
    impl->ref = 1;
    *result = &impl->ICompositionEffectSourceParameter_iface;
    return S_OK;
}

static const ICompositionEffectSourceParameterFactoryVtbl parameter_constructor_vtbl =
{
    parameter_constructor_QueryInterface,
    parameter_constructor_AddRef,
    parameter_constructor_Release,
    parameter_constructor_GetIids,
    parameter_constructor_GetRuntimeClassName,
    parameter_constructor_GetTrustLevel,
    parameter_constructor_Create,
};

static struct parameter_factory parameter_factory_impl =
{
    {&parameter_factory_vtbl},
    {&parameter_constructor_vtbl},
    1,
};

IActivationFactory *effect_source_parameter_factory = &parameter_factory_impl.IActivationFactory_iface;
