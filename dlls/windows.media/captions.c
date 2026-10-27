/* WinRT Windows.Media Closed Captions Implementation
 *
 * Copyright (C) 2022 Mohamad Al-Jaf
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

#include "wine/debug.h"
#include "wine/list.h"

WINE_DEFAULT_DEBUG_CHANNEL(media);

struct captions_statics
{
    IActivationFactory IActivationFactory_iface;
    IClosedCaptionPropertiesStatics IClosedCaptionPropertiesStatics_iface;
    IClosedCaptionPropertiesStatics2 IClosedCaptionPropertiesStatics2_iface;
    LONG ref;
};

struct captions_handler
{
    struct list entry;
    IEventHandler_IInspectable *handler;
    EventRegistrationToken token;
};

static struct list properties_changed_handlers = LIST_INIT( properties_changed_handlers );
static SRWLOCK properties_changed_lock = SRWLOCK_INIT;
static LONG64 properties_changed_token;

static inline struct captions_statics *impl_from_IActivationFactory( IActivationFactory *iface )
{
    return CONTAINING_RECORD( iface, struct captions_statics, IActivationFactory_iface );
}

static HRESULT WINAPI factory_QueryInterface( IActivationFactory *iface, REFIID iid, void **out )
{
    struct captions_statics *impl = impl_from_IActivationFactory( iface );

    TRACE( "iface %p, iid %s, out %p.\n", iface, debugstr_guid( iid ), out );

    if (IsEqualGUID( iid, &IID_IUnknown ) ||
        IsEqualGUID( iid, &IID_IInspectable ) ||
        IsEqualGUID( iid, &IID_IActivationFactory ))
    {
        *out = &impl->IActivationFactory_iface;
        IInspectable_AddRef( *out );
        return S_OK;
    }

    if (IsEqualGUID( iid, &IID_IClosedCaptionPropertiesStatics ))
    {
        *out = &impl->IClosedCaptionPropertiesStatics_iface;
        IInspectable_AddRef( *out );
        return S_OK;
    }

    if (IsEqualGUID( iid, &IID_IClosedCaptionPropertiesStatics2 ))
    {
        *out = &impl->IClosedCaptionPropertiesStatics2_iface;
        IInspectable_AddRef( *out );
        return S_OK;
    }

    FIXME( "%s not implemented, returning E_NOINTERFACE.\n", debugstr_guid( iid ) );
    *out = NULL;
    return E_NOINTERFACE;
}

static ULONG WINAPI factory_AddRef( IActivationFactory *iface )
{
    struct captions_statics *impl = impl_from_IActivationFactory( iface );
    ULONG ref = InterlockedIncrement( &impl->ref );
    TRACE( "iface %p increasing refcount to %lu.\n", iface, ref );
    return ref;
}

static ULONG WINAPI factory_Release( IActivationFactory *iface )
{
    struct captions_statics *impl = impl_from_IActivationFactory( iface );
    ULONG ref = InterlockedDecrement( &impl->ref );
    TRACE( "iface %p decreasing refcount to %lu.\n", iface, ref );
    return ref;
}

static HRESULT WINAPI factory_GetIids( IActivationFactory *iface, ULONG *iid_count, IID **iids )
{
    FIXME( "iface %p, iid_count %p, iids %p stub!\n", iface, iid_count, iids );
    return E_NOTIMPL;
}

static HRESULT WINAPI factory_GetRuntimeClassName( IActivationFactory *iface, HSTRING *class_name )
{
    FIXME( "iface %p, class_name %p stub!\n", iface, class_name );
    return E_NOTIMPL;
}

static HRESULT WINAPI factory_GetTrustLevel( IActivationFactory *iface, TrustLevel *trust_level )
{
    FIXME( "iface %p, trust_level %p stub!\n", iface, trust_level );
    return E_NOTIMPL;
}

static HRESULT WINAPI factory_ActivateInstance( IActivationFactory *iface, IInspectable **instance )
{
    FIXME( "iface %p, instance %p stub!\n", iface, instance );
    return E_NOTIMPL;
}

static const struct IActivationFactoryVtbl factory_vtbl =
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

DEFINE_IINSPECTABLE( captions, IClosedCaptionPropertiesStatics, struct captions_statics, IActivationFactory_iface )

static HRESULT WINAPI captions_get_FontColor( IClosedCaptionPropertiesStatics *iface, ClosedCaptionColor *value )
{
    FIXME( "iface %p, value %p semi-stub.\n", iface, value );

    *value = ClosedCaptionColor_Default;
    return S_OK;
}

static HRESULT WINAPI captions_get_ComputedFontColor( IClosedCaptionPropertiesStatics *iface, Color *value )
{
    FIXME( "iface %p, value %p stub!\n", iface, value );
    return E_NOTIMPL;
}

static HRESULT WINAPI captions_get_FontOpacity( IClosedCaptionPropertiesStatics *iface, ClosedCaptionOpacity *value )
{
    FIXME( "iface %p, value %p semi-stub.\n", iface, value );

    *value = ClosedCaptionOpacity_Default;
    return S_OK;
}

static HRESULT WINAPI captions_get_FontSize( IClosedCaptionPropertiesStatics *iface, ClosedCaptionSize *value )
{
    FIXME( "iface %p, value %p semi-stub.\n", iface, value );

    *value = ClosedCaptionSize_Default;
    return S_OK;
}

static HRESULT WINAPI captions_get_FontStyle( IClosedCaptionPropertiesStatics *iface, ClosedCaptionStyle *value )
{
    FIXME( "iface %p, value %p semi-stub.\n", iface, value );

    *value = ClosedCaptionStyle_Default;
    return S_OK;
}

static HRESULT WINAPI captions_get_FontEffect( IClosedCaptionPropertiesStatics *iface, ClosedCaptionEdgeEffect *value )
{
    FIXME( "iface %p, value %p semi-stub.\n", iface, value );

    *value = ClosedCaptionEdgeEffect_Default;
    return S_OK;
}

static HRESULT WINAPI captions_get_BackgroundColor( IClosedCaptionPropertiesStatics *iface, ClosedCaptionColor *value )
{
    FIXME( "iface %p, value %p semi-stub.\n", iface, value );

    *value = ClosedCaptionColor_Default;
    return S_OK;
}

static HRESULT WINAPI captions_get_ComputedBackgroundColor( IClosedCaptionPropertiesStatics *iface, Color *value )
{
    FIXME( "iface %p, value %p stub!\n", iface, value );
    return E_NOTIMPL;
}

static HRESULT WINAPI captions_get_BackgroundOpacity( IClosedCaptionPropertiesStatics *iface, ClosedCaptionOpacity *value )
{
    FIXME( "iface %p, value %p semi-stub.\n", iface, value );

    *value = ClosedCaptionOpacity_Default;
    return S_OK;
}

static HRESULT WINAPI captions_get_RegionColor( IClosedCaptionPropertiesStatics *iface, ClosedCaptionColor *value )
{
    FIXME( "iface %p, value %p semi-stub.\n", iface, value );

    *value = ClosedCaptionColor_Default;
    return S_OK;
}

static HRESULT WINAPI captions_get_ComputedRegionColor( IClosedCaptionPropertiesStatics *iface, Color *value )
{
    FIXME( "iface %p, value %p stub!\n", iface, value );
    return E_NOTIMPL;
}

static HRESULT WINAPI captions_get_RegionOpacity( IClosedCaptionPropertiesStatics *iface, ClosedCaptionOpacity *value )
{
    FIXME( "iface %p, value %p semi-stub.\n", iface, value );

    *value = ClosedCaptionOpacity_Default;
    return S_OK;
}

static const struct IClosedCaptionPropertiesStaticsVtbl captions_statics_vtbl =
{
    captions_QueryInterface,
    captions_AddRef,
    captions_Release,
    /* IInspectable methods */
    captions_GetIids,
    captions_GetRuntimeClassName,
    captions_GetTrustLevel,
    /* IClosedCaptionPropertiesStatics methods */
    captions_get_FontColor,
    captions_get_ComputedFontColor,
    captions_get_FontOpacity,
    captions_get_FontSize,
    captions_get_FontStyle,
    captions_get_FontEffect,
    captions_get_BackgroundColor,
    captions_get_ComputedBackgroundColor,
    captions_get_BackgroundOpacity,
    captions_get_RegionColor,
    captions_get_ComputedRegionColor,
    captions_get_RegionOpacity,
};

DEFINE_IINSPECTABLE( captions2, IClosedCaptionPropertiesStatics2, struct captions_statics,
                     IActivationFactory_iface )

static HRESULT WINAPI captions2_add_PropertiesChanged( IClosedCaptionPropertiesStatics2 *iface,
                                                        IEventHandler_IInspectable *handler,
                                                        EventRegistrationToken *token )
{
    struct captions_handler *entry;

    TRACE( "iface %p, handler %p, token %p.\n", iface, handler, token );

    if (!handler || !token) return E_POINTER;
    if (!(entry = calloc( 1, sizeof(*entry) ))) return E_OUTOFMEMORY;

    IEventHandler_IInspectable_AddRef( handler );
    entry->handler = handler;
    entry->token.value = InterlockedIncrement64( &properties_changed_token );

    AcquireSRWLockExclusive( &properties_changed_lock );
    list_add_tail( &properties_changed_handlers, &entry->entry );
    ReleaseSRWLockExclusive( &properties_changed_lock );

    *token = entry->token;
    return S_OK;
}

static HRESULT WINAPI captions2_remove_PropertiesChanged( IClosedCaptionPropertiesStatics2 *iface,
                                                           EventRegistrationToken token )
{
    struct captions_handler *entry, *next, *removed = NULL;

    TRACE( "iface %p, token %#I64x.\n", iface, token.value );

    AcquireSRWLockExclusive( &properties_changed_lock );
    LIST_FOR_EACH_ENTRY_SAFE( entry, next, &properties_changed_handlers, struct captions_handler, entry )
    {
        if (entry->token.value != token.value) continue;
        list_remove( &entry->entry );
        removed = entry;
        break;
    }
    ReleaseSRWLockExclusive( &properties_changed_lock );

    if (removed)
    {
        IEventHandler_IInspectable_Release( removed->handler );
        free( removed );
    }
    return S_OK;
}

static const struct IClosedCaptionPropertiesStatics2Vtbl captions_statics2_vtbl =
{
    captions2_QueryInterface,
    captions2_AddRef,
    captions2_Release,
    /* IInspectable methods */
    captions2_GetIids,
    captions2_GetRuntimeClassName,
    captions2_GetTrustLevel,
    /* IClosedCaptionPropertiesStatics2 methods */
    captions2_add_PropertiesChanged,
    captions2_remove_PropertiesChanged,
};

static struct captions_statics captions_statics =
{
    {&factory_vtbl},
    {&captions_statics_vtbl},
    {&captions_statics2_vtbl},
    1,
};

IActivationFactory *captions_factory = &captions_statics.IActivationFactory_iface;
