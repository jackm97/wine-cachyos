/*
 * Windows.UI.Composition.Interactions implementation
 *
 * Copyright 2026 Mirko Brombin
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 */

#include "private.h"
#include <math.h>
#include <float.h>
#include "roapi.h"
#include "objbase.h"

#include "wine/debug.h"
#include "wine/list.h"

WINE_DEFAULT_DEBUG_CHANNEL(ui);

struct interaction_source_collection
{
    ICompositionInteractionSourceCollection ICompositionInteractionSourceCollection_iface;
    ICompositionInteractionSource **values;
    unsigned int count;
    LONG ref;
};

static inline struct interaction_source_collection *collection_from_ICompositionInteractionSourceCollection(
        ICompositionInteractionSourceCollection *iface )
{
    return CONTAINING_RECORD( iface, struct interaction_source_collection,
                              ICompositionInteractionSourceCollection_iface );
}

static HRESULT WINAPI interaction_source_collection_QueryInterface(
        ICompositionInteractionSourceCollection *iface, REFIID iid, void **out )
{
    FIXME( "iface %p, iid %s, out %p.\n", iface, debugstr_guid( iid ), out );
    if (!out) return E_POINTER;
    if (IsEqualGUID( iid, &IID_IUnknown ) || IsEqualGUID( iid, &IID_IInspectable ) ||
        IsEqualGUID( iid, &IID_IAgileObject ) ||
        IsEqualGUID( iid, &IID_ICompositionInteractionSourceCollection ))
    {
        *out = iface;
        ICompositionInteractionSourceCollection_AddRef( iface );
        return S_OK;
    }
    *out = NULL;
    return E_NOINTERFACE;
}

static ULONG WINAPI interaction_source_collection_AddRef( ICompositionInteractionSourceCollection *iface )
{
    return InterlockedIncrement( &collection_from_ICompositionInteractionSourceCollection( iface )->ref );
}

static ULONG WINAPI interaction_source_collection_Release( ICompositionInteractionSourceCollection *iface )
{
    struct interaction_source_collection *impl =
        collection_from_ICompositionInteractionSourceCollection( iface );
    ULONG ref = InterlockedDecrement( &impl->ref );
    unsigned int i;

    if (!ref)
    {
        for (i = 0; i < impl->count; ++i)
            ICompositionInteractionSource_Release( impl->values[i] );
        free( impl->values );
        free( impl );
    }
    return ref;
}

static HRESULT WINAPI interaction_source_collection_GetIids( ICompositionInteractionSourceCollection *iface,
        ULONG *count, IID **iids )
{
    if (!count || !iids) return E_POINTER;
    if (!(*iids = CoTaskMemAlloc( sizeof(**iids) ))) return E_OUTOFMEMORY;
    **iids = IID_ICompositionInteractionSourceCollection;
    *count = 1;
    return S_OK;
}

static HRESULT WINAPI interaction_source_collection_GetRuntimeClassName(
        ICompositionInteractionSourceCollection *iface, HSTRING *name )
{
    if (!name) return E_POINTER;
    return WindowsCreateString(
            RuntimeClass_Windows_UI_Composition_Interactions_CompositionInteractionSourceCollection,
            ARRAY_SIZE(RuntimeClass_Windows_UI_Composition_Interactions_CompositionInteractionSourceCollection) - 1,
            name );
}

static HRESULT WINAPI interaction_source_collection_GetTrustLevel(
        ICompositionInteractionSourceCollection *iface, TrustLevel *level )
{
    if (!level) return E_POINTER;
    *level = BaseTrust;
    return S_OK;
}

static HRESULT WINAPI interaction_source_collection_get_Count(
        ICompositionInteractionSourceCollection *iface, INT32 *value )
{
    if (!value) return E_POINTER;
    *value = collection_from_ICompositionInteractionSourceCollection( iface )->count;
    return S_OK;
}

static HRESULT WINAPI interaction_source_collection_Add( ICompositionInteractionSourceCollection *iface,
        ICompositionInteractionSource *value )
{
    struct interaction_source_collection *impl =
        collection_from_ICompositionInteractionSourceCollection( iface );
    ICompositionInteractionSource **values;

    if (!value) return E_INVALIDARG;
    if (!(values = realloc( impl->values, (impl->count + 1) * sizeof(*values) ))) return E_OUTOFMEMORY;
    impl->values = values;
    impl->values[impl->count++] = value;
    ICompositionInteractionSource_AddRef( value );
    return S_OK;
}

static HRESULT WINAPI interaction_source_collection_Remove( ICompositionInteractionSourceCollection *iface,
        ICompositionInteractionSource *value )
{
    struct interaction_source_collection *impl =
        collection_from_ICompositionInteractionSourceCollection( iface );
    unsigned int i;

    if (!value) return E_INVALIDARG;
    for (i = 0; i < impl->count; ++i)
    {
        if (impl->values[i] != value) continue;
        ICompositionInteractionSource_Release( impl->values[i] );
        memmove( &impl->values[i], &impl->values[i + 1],
                 (impl->count - i - 1) * sizeof(*impl->values) );
        --impl->count;
        return S_OK;
    }
    return S_OK;
}

static HRESULT WINAPI interaction_source_collection_RemoveAll(
        ICompositionInteractionSourceCollection *iface )
{
    struct interaction_source_collection *impl =
        collection_from_ICompositionInteractionSourceCollection( iface );
    unsigned int i;

    for (i = 0; i < impl->count; ++i)
        ICompositionInteractionSource_Release( impl->values[i] );
    impl->count = 0;
    return S_OK;
}

static const ICompositionInteractionSourceCollectionVtbl interaction_source_collection_vtbl =
{
    interaction_source_collection_QueryInterface,
    interaction_source_collection_AddRef,
    interaction_source_collection_Release,
    interaction_source_collection_GetIids,
    interaction_source_collection_GetRuntimeClassName,
    interaction_source_collection_GetTrustLevel,
    interaction_source_collection_get_Count,
    interaction_source_collection_Add,
    interaction_source_collection_Remove,
    interaction_source_collection_RemoveAll,
};

static HRESULT interaction_source_collection_create( ICompositionInteractionSourceCollection **result )
{
    struct interaction_source_collection *impl;

    if (!result) return E_POINTER;
    *result = NULL;
    if (!(impl = calloc( 1, sizeof(*impl) ))) return E_OUTOFMEMORY;
    impl->ICompositionInteractionSourceCollection_iface.lpVtbl = &interaction_source_collection_vtbl;
    impl->ref = 1;
    *result = &impl->ICompositionInteractionSourceCollection_iface;
    return S_OK;
}

struct composition_conditional_value
{
    ICompositionConditionalValue ICompositionConditionalValue_iface;
    IInteractionTrackerInertiaRestingValue IInteractionTrackerInertiaRestingValue_iface;
    IInteractionTrackerInertiaModifier IInteractionTrackerInertiaModifier_iface;
    ICompositionObject ICompositionObject_iface;
    IClosable IClosable_iface;
    IExpressionAnimation *condition;
    IExpressionAnimation *value;
    ICompositionPropertySet *properties;
    ICompositor *compositor;
    CRITICAL_SECTION cs;
    BOOL inertia_resting_value;
    BOOL closed;
    LONG ref;
};

static inline struct composition_conditional_value *impl_from_ICompositionConditionalValue(
        ICompositionConditionalValue *iface )
{
    return CONTAINING_RECORD( iface, struct composition_conditional_value,
                              ICompositionConditionalValue_iface );
}

static inline struct composition_conditional_value *conditional_value_from_ICompositionObject(
        ICompositionObject *iface )
{
    return CONTAINING_RECORD( iface, struct composition_conditional_value, ICompositionObject_iface );
}

static inline struct composition_conditional_value *conditional_value_from_IClosable( IClosable *iface )
{
    return CONTAINING_RECORD( iface, struct composition_conditional_value, IClosable_iface );
}

static HRESULT composition_conditional_value_query_interface( struct composition_conditional_value *impl,
        REFIID iid, void **out )
{
    if (!out) return E_POINTER;
    if (IsEqualGUID( iid, &IID_IUnknown ) || IsEqualGUID( iid, &IID_IInspectable ) ||
        IsEqualGUID( iid, &IID_IAgileObject ))
        *out = impl->inertia_resting_value ? (void *)&impl->IInteractionTrackerInertiaRestingValue_iface :
                                           (void *)&impl->ICompositionConditionalValue_iface;
    else if (!impl->inertia_resting_value && IsEqualGUID( iid, &IID_ICompositionConditionalValue ))
        *out = &impl->ICompositionConditionalValue_iface;
    else if (impl->inertia_resting_value && IsEqualGUID( iid, &IID_IInteractionTrackerInertiaRestingValue ))
        *out = &impl->IInteractionTrackerInertiaRestingValue_iface;
    else if (impl->inertia_resting_value && IsEqualGUID( iid, &IID_IInteractionTrackerInertiaModifier ))
        *out = &impl->IInteractionTrackerInertiaModifier_iface;
    else if (IsEqualGUID( iid, &IID_ICompositionObject ))
        *out = &impl->ICompositionObject_iface;
    else if (IsEqualGUID( iid, &IID_IClosable ))
        *out = &impl->IClosable_iface;
    else
    {
        *out = NULL;
        return E_NOINTERFACE;
    }
    IUnknown_AddRef( (IUnknown *)*out );
    return S_OK;
}

static ULONG composition_conditional_value_add_ref( struct composition_conditional_value *impl )
{
    return InterlockedIncrement( &impl->ref );
}

static ULONG composition_conditional_value_release( struct composition_conditional_value *impl )
{
    ULONG ref = InterlockedDecrement( &impl->ref );

    if (!ref)
    {
        if (impl->condition) IExpressionAnimation_Release( impl->condition );
        if (impl->value) IExpressionAnimation_Release( impl->value );
        if (impl->properties) ICompositionPropertySet_Release( impl->properties );
        if (impl->compositor) ICompositor_Release( impl->compositor );
        DeleteCriticalSection( &impl->cs );
        free( impl );
    }
    return ref;
}

static HRESULT composition_conditional_value_get_iids( struct composition_conditional_value *impl,
        ULONG *count, IID **iids )
{
    IID *values;
    ULONG size = impl->inertia_resting_value ? 4 : 3;

    if (!count || !iids) return E_POINTER;
    if (!(values = CoTaskMemAlloc( size * sizeof(*values) ))) return E_OUTOFMEMORY;
    values[0] = impl->inertia_resting_value ? IID_IInteractionTrackerInertiaRestingValue : IID_ICompositionConditionalValue;
    values[1] = IID_ICompositionObject;
    values[2] = IID_IClosable;
    if (impl->inertia_resting_value) values[3] = IID_IInteractionTrackerInertiaModifier;
    *count = size;
    *iids = values;
    return S_OK;
}

static HRESULT composition_conditional_value_get_class_name( HSTRING *name )
{
    if (!name) return E_POINTER;
    return WindowsCreateString(
            RuntimeClass_Windows_UI_Composition_Interactions_CompositionConditionalValue,
            ARRAY_SIZE(RuntimeClass_Windows_UI_Composition_Interactions_CompositionConditionalValue) - 1,
            name );
}

static HRESULT inertia_resting_value_class_name( HSTRING *name )
{
    if (!name) return E_POINTER;
    return WindowsCreateString(
            RuntimeClass_Windows_UI_Composition_Interactions_InteractionTrackerInertiaRestingValue,
            ARRAY_SIZE(RuntimeClass_Windows_UI_Composition_Interactions_InteractionTrackerInertiaRestingValue) - 1,
            name );
}

static HRESULT conditional_value_class_name( struct composition_conditional_value *impl, HSTRING *name )
{
    if (impl->inertia_resting_value) return inertia_resting_value_class_name( name );
    return composition_conditional_value_get_class_name( name );
}

static HRESULT composition_conditional_value_get_trust_level( TrustLevel *level )
{
    if (!level) return E_POINTER;
    *level = BaseTrust;
    return S_OK;
}

static HRESULT WINAPI composition_conditional_value_QueryInterface( ICompositionConditionalValue *iface,
        REFIID iid, void **out )
{
    return composition_conditional_value_query_interface(
            impl_from_ICompositionConditionalValue( iface ), iid, out );
}

static ULONG WINAPI composition_conditional_value_AddRef( ICompositionConditionalValue *iface )
{
    return composition_conditional_value_add_ref( impl_from_ICompositionConditionalValue( iface ) );
}

static ULONG WINAPI composition_conditional_value_Release( ICompositionConditionalValue *iface )
{
    return composition_conditional_value_release( impl_from_ICompositionConditionalValue( iface ) );
}

static HRESULT WINAPI composition_conditional_value_GetIids( ICompositionConditionalValue *iface,
        ULONG *count, IID **iids )
{
    return composition_conditional_value_get_iids( impl_from_ICompositionConditionalValue( iface ), count, iids );
}

static HRESULT WINAPI composition_conditional_value_GetRuntimeClassName(
        ICompositionConditionalValue *iface, HSTRING *name )
{
    return conditional_value_class_name( impl_from_ICompositionConditionalValue( iface ), name );
}

static HRESULT WINAPI composition_conditional_value_GetTrustLevel(
        ICompositionConditionalValue *iface, TrustLevel *level )
{
    return composition_conditional_value_get_trust_level( level );
}

static HRESULT composition_conditional_value_get_animation( struct composition_conditional_value *impl,
        IExpressionAnimation **current, IExpressionAnimation **value )
{
    HRESULT hr = S_OK;

    if (!value) return E_POINTER;
    *value = NULL;
    EnterCriticalSection( &impl->cs );
    if (impl->closed) hr = RO_E_CLOSED;
    else
    {
        *value = *current;
        if (*value) IExpressionAnimation_AddRef( *value );
    }
    LeaveCriticalSection( &impl->cs );
    return hr;
}

static HRESULT composition_conditional_value_put_animation( struct composition_conditional_value *impl,
        IExpressionAnimation **current, IExpressionAnimation *value )
{
    IExpressionAnimation *old;

    EnterCriticalSection( &impl->cs );
    if (impl->closed)
    {
        LeaveCriticalSection( &impl->cs );
        return RO_E_CLOSED;
    }
    if (value) IExpressionAnimation_AddRef( value );
    old = *current;
    *current = value;
    LeaveCriticalSection( &impl->cs );
    if (old) IExpressionAnimation_Release( old );
    return S_OK;
}

static HRESULT WINAPI composition_conditional_value_get_Condition( ICompositionConditionalValue *iface,
        IExpressionAnimation **value )
{
    struct composition_conditional_value *impl = impl_from_ICompositionConditionalValue( iface );
    return composition_conditional_value_get_animation( impl, &impl->condition, value );
}

static HRESULT WINAPI composition_conditional_value_put_Condition( ICompositionConditionalValue *iface,
        IExpressionAnimation *value )
{
    struct composition_conditional_value *impl = impl_from_ICompositionConditionalValue( iface );
    return composition_conditional_value_put_animation( impl, &impl->condition, value );
}

static HRESULT WINAPI composition_conditional_value_get_Value( ICompositionConditionalValue *iface,
        IExpressionAnimation **value )
{
    struct composition_conditional_value *impl = impl_from_ICompositionConditionalValue( iface );
    return composition_conditional_value_get_animation( impl, &impl->value, value );
}

static HRESULT WINAPI composition_conditional_value_put_Value( ICompositionConditionalValue *iface,
        IExpressionAnimation *value )
{
    struct composition_conditional_value *impl = impl_from_ICompositionConditionalValue( iface );
    return composition_conditional_value_put_animation( impl, &impl->value, value );
}

static const ICompositionConditionalValueVtbl composition_conditional_value_vtbl =
{
    composition_conditional_value_QueryInterface,
    composition_conditional_value_AddRef,
    composition_conditional_value_Release,
    composition_conditional_value_GetIids,
    composition_conditional_value_GetRuntimeClassName,
    composition_conditional_value_GetTrustLevel,
    composition_conditional_value_get_Condition,
    composition_conditional_value_put_Condition,
    composition_conditional_value_get_Value,
    composition_conditional_value_put_Value,
};

static HRESULT WINAPI conditional_value_composition_object_QueryInterface( ICompositionObject *iface,
        REFIID iid, void **out )
{
    return composition_conditional_value_query_interface(
            conditional_value_from_ICompositionObject( iface ), iid, out );
}

static ULONG WINAPI conditional_value_composition_object_AddRef( ICompositionObject *iface )
{
    return composition_conditional_value_add_ref( conditional_value_from_ICompositionObject( iface ) );
}

static ULONG WINAPI conditional_value_composition_object_Release( ICompositionObject *iface )
{
    return composition_conditional_value_release( conditional_value_from_ICompositionObject( iface ) );
}

static HRESULT WINAPI conditional_value_composition_object_GetIids( ICompositionObject *iface,
        ULONG *count, IID **iids )
{
    return composition_conditional_value_get_iids( conditional_value_from_ICompositionObject( iface ), count, iids );
}

static HRESULT WINAPI conditional_value_composition_object_GetRuntimeClassName(
        ICompositionObject *iface, HSTRING *name )
{
    return conditional_value_class_name( conditional_value_from_ICompositionObject( iface ), name );
}

static HRESULT WINAPI conditional_value_composition_object_GetTrustLevel( ICompositionObject *iface,
        TrustLevel *level )
{
    return composition_conditional_value_get_trust_level( level );
}

static HRESULT WINAPI conditional_value_composition_object_get_Compositor( ICompositionObject *iface,
        ICompositor **value )
{
    struct composition_conditional_value *impl = conditional_value_from_ICompositionObject( iface );
    HRESULT hr = S_OK;

    if (!value) return E_POINTER;
    *value = NULL;
    EnterCriticalSection( &impl->cs );
    if (impl->closed) hr = RO_E_CLOSED;
    else
    {
        ICompositor_AddRef( impl->compositor );
        *value = impl->compositor;
    }
    LeaveCriticalSection( &impl->cs );
    return hr;
}

static HRESULT WINAPI conditional_value_composition_object_get_Dispatcher( ICompositionObject *iface,
        ICoreDispatcher **value )
{
    struct composition_conditional_value *impl = conditional_value_from_ICompositionObject( iface );
    HRESULT hr;

    if (!value) return E_POINTER;
    *value = NULL;
    EnterCriticalSection( &impl->cs );
    hr = impl->closed ? RO_E_CLOSED : S_OK;
    LeaveCriticalSection( &impl->cs );
    return hr;
}

static HRESULT WINAPI conditional_value_composition_object_get_Properties( ICompositionObject *iface,
        ICompositionPropertySet **value )
{
    struct composition_conditional_value *impl = conditional_value_from_ICompositionObject( iface );
    HRESULT hr = S_OK;

    if (!value) return E_POINTER;
    *value = NULL;
    EnterCriticalSection( &impl->cs );
    if (impl->closed) hr = RO_E_CLOSED;
    else
    {
        ICompositionPropertySet_AddRef( impl->properties );
        *value = impl->properties;
    }
    LeaveCriticalSection( &impl->cs );
    return hr;
}

static HRESULT WINAPI conditional_value_composition_object_StartAnimation( ICompositionObject *iface,
        HSTRING name, ICompositionAnimation *animation )
{
    struct composition_conditional_value *impl = conditional_value_from_ICompositionObject( iface );
    HRESULT hr;

    TRACE( "iface %p, name %s, animation %p.\n", iface, debugstr_hstring( name ), animation );
    EnterCriticalSection( &impl->cs );
    hr = impl->closed ? RO_E_CLOSED : impl->inertia_resting_value ? E_NOTIMPL : S_OK;
    LeaveCriticalSection( &impl->cs );
    return hr;
}

static HRESULT WINAPI conditional_value_composition_object_StopAnimation( ICompositionObject *iface,
        HSTRING name )
{
    struct composition_conditional_value *impl = conditional_value_from_ICompositionObject( iface );
    HRESULT hr;

    TRACE( "iface %p, name %s.\n", iface, debugstr_hstring( name ) );
    EnterCriticalSection( &impl->cs );
    hr = impl->closed ? RO_E_CLOSED : impl->inertia_resting_value ? E_NOTIMPL : S_OK;
    LeaveCriticalSection( &impl->cs );
    return hr;
}

static const ICompositionObjectVtbl conditional_value_composition_object_vtbl =
{
    conditional_value_composition_object_QueryInterface,
    conditional_value_composition_object_AddRef,
    conditional_value_composition_object_Release,
    conditional_value_composition_object_GetIids,
    conditional_value_composition_object_GetRuntimeClassName,
    conditional_value_composition_object_GetTrustLevel,
    conditional_value_composition_object_get_Compositor,
    conditional_value_composition_object_get_Dispatcher,
    conditional_value_composition_object_get_Properties,
    conditional_value_composition_object_StartAnimation,
    conditional_value_composition_object_StopAnimation,
};

static HRESULT WINAPI conditional_value_closable_QueryInterface( IClosable *iface,
        REFIID iid, void **out )
{
    return composition_conditional_value_query_interface(
            conditional_value_from_IClosable( iface ), iid, out );
}

static ULONG WINAPI conditional_value_closable_AddRef( IClosable *iface )
{
    return composition_conditional_value_add_ref( conditional_value_from_IClosable( iface ) );
}

static ULONG WINAPI conditional_value_closable_Release( IClosable *iface )
{
    return composition_conditional_value_release( conditional_value_from_IClosable( iface ) );
}

static HRESULT WINAPI conditional_value_closable_GetIids( IClosable *iface, ULONG *count, IID **iids )
{
    return composition_conditional_value_get_iids( conditional_value_from_IClosable( iface ), count, iids );
}

static HRESULT WINAPI conditional_value_closable_GetRuntimeClassName( IClosable *iface, HSTRING *name )
{
    return conditional_value_class_name( conditional_value_from_IClosable( iface ), name );
}

static HRESULT WINAPI conditional_value_closable_GetTrustLevel( IClosable *iface, TrustLevel *level )
{
    return composition_conditional_value_get_trust_level( level );
}

static HRESULT WINAPI conditional_value_closable_Close( IClosable *iface )
{
    struct composition_conditional_value *impl = conditional_value_from_IClosable( iface );
    IExpressionAnimation *condition, *value;
    ICompositionPropertySet *properties;
    ICompositor *compositor;

    TRACE( "iface %p.\n", iface );
    if (!impl->inertia_resting_value) return S_OK;
    EnterCriticalSection( &impl->cs );
    impl->closed = TRUE;
    condition = impl->condition;
    value = impl->value;
    properties = impl->properties;
    compositor = impl->compositor;
    impl->condition = impl->value = NULL;
    impl->properties = NULL;
    impl->compositor = NULL;
    LeaveCriticalSection( &impl->cs );
    if (condition) IExpressionAnimation_Release( condition );
    if (value) IExpressionAnimation_Release( value );
    if (properties) ICompositionPropertySet_Release( properties );
    if (compositor) ICompositor_Release( compositor );
    return S_OK;
}

static const IClosableVtbl conditional_value_closable_vtbl =
{
    conditional_value_closable_QueryInterface,
    conditional_value_closable_AddRef,
    conditional_value_closable_Release,
    conditional_value_closable_GetIids,
    conditional_value_closable_GetRuntimeClassName,
    conditional_value_closable_GetTrustLevel,
    conditional_value_closable_Close,
};

DEFINE_IINSPECTABLE_( inertia_resting_value, IInteractionTrackerInertiaRestingValue,
        struct composition_conditional_value, impl_from_IInteractionTrackerInertiaRestingValue,
        IInteractionTrackerInertiaRestingValue_iface, &impl->ICompositionConditionalValue_iface )

static HRESULT WINAPI inertia_resting_value_get_Condition( IInteractionTrackerInertiaRestingValue *iface,
        IExpressionAnimation **value )
{
    struct composition_conditional_value *impl = impl_from_IInteractionTrackerInertiaRestingValue( iface );
    return composition_conditional_value_get_animation( impl, &impl->condition, value );
}

static HRESULT WINAPI inertia_resting_value_put_Condition( IInteractionTrackerInertiaRestingValue *iface,
        IExpressionAnimation *value )
{
    struct composition_conditional_value *impl = impl_from_IInteractionTrackerInertiaRestingValue( iface );
    return composition_conditional_value_put_animation( impl, &impl->condition, value );
}

static HRESULT WINAPI inertia_resting_value_get_RestingValue( IInteractionTrackerInertiaRestingValue *iface,
        IExpressionAnimation **value )
{
    struct composition_conditional_value *impl = impl_from_IInteractionTrackerInertiaRestingValue( iface );
    return composition_conditional_value_get_animation( impl, &impl->value, value );
}

static HRESULT WINAPI inertia_resting_value_put_RestingValue( IInteractionTrackerInertiaRestingValue *iface,
        IExpressionAnimation *value )
{
    struct composition_conditional_value *impl = impl_from_IInteractionTrackerInertiaRestingValue( iface );
    return composition_conditional_value_put_animation( impl, &impl->value, value );
}

static const IInteractionTrackerInertiaRestingValueVtbl inertia_resting_value_vtbl =
{
    inertia_resting_value_QueryInterface,
    inertia_resting_value_AddRef,
    inertia_resting_value_Release,
    inertia_resting_value_GetIids,
    inertia_resting_value_GetRuntimeClassName,
    inertia_resting_value_GetTrustLevel,
    inertia_resting_value_get_Condition,
    inertia_resting_value_put_Condition,
    inertia_resting_value_get_RestingValue,
    inertia_resting_value_put_RestingValue,
};

DEFINE_IINSPECTABLE_( inertia_modifier, IInteractionTrackerInertiaModifier,
        struct composition_conditional_value, impl_from_IInteractionTrackerInertiaModifier,
        IInteractionTrackerInertiaModifier_iface, &impl->ICompositionConditionalValue_iface )

static const IInteractionTrackerInertiaModifierVtbl inertia_modifier_vtbl =
{
    inertia_modifier_QueryInterface,
    inertia_modifier_AddRef,
    inertia_modifier_Release,
    inertia_modifier_GetIids,
    inertia_modifier_GetRuntimeClassName,
    inertia_modifier_GetTrustLevel,
};

static HRESULT composition_conditional_value_create( ICompositor *compositor, BOOL inertia_resting_value,
        ICompositionConditionalValue **result )
{
    struct composition_conditional_value *impl;
    HRESULT hr;

    if (!compositor || !result) return E_INVALIDARG;
    *result = NULL;
    if (!(impl = calloc( 1, sizeof(*impl) ))) return E_OUTOFMEMORY;
    impl->ICompositionConditionalValue_iface.lpVtbl = &composition_conditional_value_vtbl;
    impl->IInteractionTrackerInertiaRestingValue_iface.lpVtbl = &inertia_resting_value_vtbl;
    impl->IInteractionTrackerInertiaModifier_iface.lpVtbl = &inertia_modifier_vtbl;
    impl->ICompositionObject_iface.lpVtbl = &conditional_value_composition_object_vtbl;
    impl->IClosable_iface.lpVtbl = &conditional_value_closable_vtbl;
    impl->compositor = compositor;
    impl->inertia_resting_value = inertia_resting_value;
    impl->ref = 1;
    InitializeCriticalSection( &impl->cs );
    ICompositor_AddRef( compositor );
    if (FAILED(hr = composition_property_set_create( compositor, &impl->properties )))
    {
        ICompositionConditionalValue_Release( &impl->ICompositionConditionalValue_iface );
        return hr;
    }
    *result = &impl->ICompositionConditionalValue_iface;
    return S_OK;
}

struct composition_conditional_value_factory
{
    IActivationFactory IActivationFactory_iface;
    ICompositionConditionalValueStatics ICompositionConditionalValueStatics_iface;
    LONG ref;
};

static inline struct composition_conditional_value_factory *conditional_value_factory_from_IActivationFactory(
        IActivationFactory *iface )
{
    return CONTAINING_RECORD( iface, struct composition_conditional_value_factory, IActivationFactory_iface );
}

static inline struct composition_conditional_value_factory *conditional_value_factory_from_statics(
        ICompositionConditionalValueStatics *iface )
{
    return CONTAINING_RECORD( iface, struct composition_conditional_value_factory,
                              ICompositionConditionalValueStatics_iface );
}

static HRESULT WINAPI composition_conditional_value_factory_QueryInterface( IActivationFactory *iface,
        REFIID iid, void **out )
{
    struct composition_conditional_value_factory *impl =
        conditional_value_factory_from_IActivationFactory( iface );

    if (!out) return E_POINTER;
    if (IsEqualGUID( iid, &IID_IUnknown ) || IsEqualGUID( iid, &IID_IInspectable ) ||
        IsEqualGUID( iid, &IID_IAgileObject ) || IsEqualGUID( iid, &IID_IActivationFactory ))
        *out = &impl->IActivationFactory_iface;
    else if (IsEqualGUID( iid, &IID_ICompositionConditionalValueStatics ))
        *out = &impl->ICompositionConditionalValueStatics_iface;
    else
    {
        *out = NULL;
        return E_NOINTERFACE;
    }
    IUnknown_AddRef( (IUnknown *)*out );
    return S_OK;
}

static ULONG WINAPI composition_conditional_value_factory_AddRef( IActivationFactory *iface )
{
    return InterlockedIncrement( &conditional_value_factory_from_IActivationFactory( iface )->ref );
}

static ULONG WINAPI composition_conditional_value_factory_Release( IActivationFactory *iface )
{
    return InterlockedDecrement( &conditional_value_factory_from_IActivationFactory( iface )->ref );
}

static HRESULT WINAPI composition_conditional_value_factory_GetIids( IActivationFactory *iface,
        ULONG *count, IID **iids )
{
    return E_NOTIMPL;
}

static HRESULT WINAPI composition_conditional_value_factory_GetRuntimeClassName(
        IActivationFactory *iface, HSTRING *name )
{
    return composition_conditional_value_get_class_name( name );
}

static HRESULT WINAPI composition_conditional_value_factory_GetTrustLevel(
        IActivationFactory *iface, TrustLevel *level )
{
    return composition_conditional_value_get_trust_level( level );
}

static HRESULT WINAPI composition_conditional_value_factory_ActivateInstance(
        IActivationFactory *iface, IInspectable **instance )
{
    if (!instance) return E_POINTER;
    *instance = NULL;
    return E_NOTIMPL;
}

static const IActivationFactoryVtbl composition_conditional_value_factory_vtbl =
{
    composition_conditional_value_factory_QueryInterface,
    composition_conditional_value_factory_AddRef,
    composition_conditional_value_factory_Release,
    composition_conditional_value_factory_GetIids,
    composition_conditional_value_factory_GetRuntimeClassName,
    composition_conditional_value_factory_GetTrustLevel,
    composition_conditional_value_factory_ActivateInstance,
};

static HRESULT WINAPI composition_conditional_value_statics_QueryInterface(
        ICompositionConditionalValueStatics *iface, REFIID iid, void **out )
{
    struct composition_conditional_value_factory *impl = conditional_value_factory_from_statics( iface );
    return IActivationFactory_QueryInterface( &impl->IActivationFactory_iface, iid, out );
}

static ULONG WINAPI composition_conditional_value_statics_AddRef(
        ICompositionConditionalValueStatics *iface )
{
    struct composition_conditional_value_factory *impl = conditional_value_factory_from_statics( iface );
    return IActivationFactory_AddRef( &impl->IActivationFactory_iface );
}

static ULONG WINAPI composition_conditional_value_statics_Release(
        ICompositionConditionalValueStatics *iface )
{
    struct composition_conditional_value_factory *impl = conditional_value_factory_from_statics( iface );
    return IActivationFactory_Release( &impl->IActivationFactory_iface );
}

static HRESULT WINAPI composition_conditional_value_statics_GetIids(
        ICompositionConditionalValueStatics *iface, ULONG *count, IID **iids )
{
    struct composition_conditional_value_factory *impl = conditional_value_factory_from_statics( iface );
    return IActivationFactory_GetIids( &impl->IActivationFactory_iface, count, iids );
}

static HRESULT WINAPI composition_conditional_value_statics_GetRuntimeClassName(
        ICompositionConditionalValueStatics *iface, HSTRING *name )
{
    struct composition_conditional_value_factory *impl = conditional_value_factory_from_statics( iface );
    return IActivationFactory_GetRuntimeClassName( &impl->IActivationFactory_iface, name );
}

static HRESULT WINAPI composition_conditional_value_statics_GetTrustLevel(
        ICompositionConditionalValueStatics *iface, TrustLevel *level )
{
    struct composition_conditional_value_factory *impl = conditional_value_factory_from_statics( iface );
    return IActivationFactory_GetTrustLevel( &impl->IActivationFactory_iface, level );
}

static HRESULT WINAPI composition_conditional_value_statics_Create(
        ICompositionConditionalValueStatics *iface, ICompositor *compositor,
        ICompositionConditionalValue **result )
{
    TRACE( "iface %p, compositor %p, result %p.\n", iface, compositor, result );
    return composition_conditional_value_create( compositor, FALSE, result );
}

static const ICompositionConditionalValueStaticsVtbl composition_conditional_value_statics_vtbl =
{
    composition_conditional_value_statics_QueryInterface,
    composition_conditional_value_statics_AddRef,
    composition_conditional_value_statics_Release,
    composition_conditional_value_statics_GetIids,
    composition_conditional_value_statics_GetRuntimeClassName,
    composition_conditional_value_statics_GetTrustLevel,
    composition_conditional_value_statics_Create,
};

static struct composition_conditional_value_factory composition_conditional_value_factory_impl =
{
    {&composition_conditional_value_factory_vtbl},
    {&composition_conditional_value_statics_vtbl},
    1,
};

IActivationFactory *composition_conditional_value_factory =
    &composition_conditional_value_factory_impl.IActivationFactory_iface;

struct inertia_resting_value_factory
{
    IActivationFactory IActivationFactory_iface;
    IInteractionTrackerInertiaRestingValueStatics IInteractionTrackerInertiaRestingValueStatics_iface;
    LONG ref;
};

static inline struct inertia_resting_value_factory *resting_value_factory_from_IActivationFactory(
        IActivationFactory *iface )
{
    return CONTAINING_RECORD( iface, struct inertia_resting_value_factory, IActivationFactory_iface );
}

static HRESULT WINAPI inertia_resting_value_factory_QueryInterface( IActivationFactory *iface,
        REFIID iid, void **out )
{
    struct inertia_resting_value_factory *impl = resting_value_factory_from_IActivationFactory( iface );

    if (!out) return E_POINTER;
    *out = NULL;
    if (IsEqualGUID( iid, &IID_IUnknown ) || IsEqualGUID( iid, &IID_IInspectable ) ||
        IsEqualGUID( iid, &IID_IAgileObject ) || IsEqualGUID( iid, &IID_IActivationFactory ))
        *out = &impl->IActivationFactory_iface;
    else if (IsEqualGUID( iid, &IID_IInteractionTrackerInertiaRestingValueStatics ))
        *out = &impl->IInteractionTrackerInertiaRestingValueStatics_iface;
    else return E_NOINTERFACE;
    IUnknown_AddRef( (IUnknown *)*out );
    return S_OK;
}

static ULONG WINAPI inertia_resting_value_factory_AddRef( IActivationFactory *iface )
{
    return InterlockedIncrement( &resting_value_factory_from_IActivationFactory( iface )->ref );
}

static ULONG WINAPI inertia_resting_value_factory_Release( IActivationFactory *iface )
{
    return InterlockedDecrement( &resting_value_factory_from_IActivationFactory( iface )->ref );
}

static HRESULT WINAPI inertia_resting_value_factory_GetIids( IActivationFactory *iface,
        ULONG *count, IID **iids )
{
    IID *values;

    if (!count || !iids) return E_POINTER;
    if (!(values = CoTaskMemAlloc( 2 * sizeof(*values) ))) return E_OUTOFMEMORY;
    values[0] = IID_IActivationFactory;
    values[1] = IID_IInteractionTrackerInertiaRestingValueStatics;
    *count = 2;
    *iids = values;
    return S_OK;
}

static HRESULT WINAPI inertia_resting_value_factory_GetRuntimeClassName( IActivationFactory *iface,
        HSTRING *name )
{
    return inertia_resting_value_class_name( name );
}

static HRESULT WINAPI inertia_resting_value_factory_GetTrustLevel( IActivationFactory *iface,
        TrustLevel *level )
{
    return composition_conditional_value_get_trust_level( level );
}

static HRESULT WINAPI inertia_resting_value_factory_ActivateInstance( IActivationFactory *iface,
        IInspectable **instance )
{
    if (!instance) return E_POINTER;
    *instance = NULL;
    return E_NOTIMPL;
}

static const IActivationFactoryVtbl inertia_resting_value_factory_vtbl =
{
    inertia_resting_value_factory_QueryInterface,
    inertia_resting_value_factory_AddRef,
    inertia_resting_value_factory_Release,
    inertia_resting_value_factory_GetIids,
    inertia_resting_value_factory_GetRuntimeClassName,
    inertia_resting_value_factory_GetTrustLevel,
    inertia_resting_value_factory_ActivateInstance,
};

DEFINE_IINSPECTABLE_( inertia_resting_value_statics, IInteractionTrackerInertiaRestingValueStatics,
        struct inertia_resting_value_factory, impl_from_IInteractionTrackerInertiaRestingValueStatics,
        IInteractionTrackerInertiaRestingValueStatics_iface, &impl->IActivationFactory_iface )

static HRESULT WINAPI inertia_resting_value_statics_Create( IInteractionTrackerInertiaRestingValueStatics *iface,
        ICompositor *compositor, IInteractionTrackerInertiaRestingValue **value )
{
    ICompositionConditionalValue *object;
    HRESULT hr;

    if (!value) return E_POINTER;
    *value = NULL;
    if (FAILED(hr = composition_conditional_value_create( compositor, TRUE, &object ))) return hr;
    hr = ICompositionConditionalValue_QueryInterface( object, &IID_IInteractionTrackerInertiaRestingValue, (void **)value );
    ICompositionConditionalValue_Release( object );
    return hr;
}

static const IInteractionTrackerInertiaRestingValueStaticsVtbl inertia_resting_value_statics_vtbl =
{
    inertia_resting_value_statics_QueryInterface,
    inertia_resting_value_statics_AddRef,
    inertia_resting_value_statics_Release,
    inertia_resting_value_statics_GetIids,
    inertia_resting_value_statics_GetRuntimeClassName,
    inertia_resting_value_statics_GetTrustLevel,
    inertia_resting_value_statics_Create,
};

static struct inertia_resting_value_factory inertia_resting_value_factory_impl =
{
    {&inertia_resting_value_factory_vtbl},
    {&inertia_resting_value_statics_vtbl},
    1,
};

IActivationFactory *inertia_resting_value_factory = &inertia_resting_value_factory_impl.IActivationFactory_iface;

static CRITICAL_SECTION tracker_binding_cs;
static CRITICAL_SECTION_DEBUG tracker_binding_cs_debug =
{
    0, 0, &tracker_binding_cs,
    { &tracker_binding_cs_debug.ProcessLocksList, &tracker_binding_cs_debug.ProcessLocksList },
    0, 0, { (DWORD_PTR)(__FILE__ ": tracker_binding_cs") }
};
static CRITICAL_SECTION tracker_binding_cs = { &tracker_binding_cs_debug, -1, 0, 0, 0, 0 };

struct interaction_tracker_binding
{
    struct list entry;
    struct interaction_tracker *peer;
    enum InteractionBindingAxisModes axis_mode;
};

struct tracker_inertia_operation;
struct tracker_decay_value;

struct interaction_tracker
{
    IInteractionTracker IInteractionTracker_iface;
    IInteractionTracker4 IInteractionTracker4_iface;
    IInteractionTracker5 IInteractionTracker5_iface;
    ICompositionObject ICompositionObject_iface;
    IClosable IClosable_iface;
    ICompositionInteractionSourceCollection *sources;
    ICompositionPropertySet *properties;
    ICompositor *compositor;
    IAgileReference *owner;
    CO_MTA_USAGE_COOKIE owner_mta_cookie;
    BOOL notifications_closed;
    BOOL clamp_position;
    struct tracker_inertia_operation *inertia;
    struct Vector3 position_velocity;
    struct Vector3 natural_position;
    struct Vector3 position_decay;
    FLOAT scale_decay;
    FLOAT scale_velocity;
    FLOAT natural_scale;
    struct Vector3 scale_center;
    struct list bindings;
    struct list update_entry;
    struct list changed_entry;
    struct tracker_decay_value *position_decay_rate;
    struct tracker_decay_value *scale_decay_rate;
    CRITICAL_SECTION animation_cs;
    IExpressionAnimation *expressions[6];
    UINT animation_revision;
    DWORD evaluation_thread;
    struct Vector3 max_position;
    struct Vector3 min_position;
    struct Vector3 position;
    FLOAT max_scale;
    FLOAT min_scale;
    FLOAT scale;
    LONG next_request_id;
    LONG ref;
};

struct tracker_decay_value
{
    LONG ref;
    IInspectable *value;
};

static void tracker_decay_release( struct tracker_decay_value *value );

struct tracker_inertia_operation
{
    struct list entry;
    struct list pending_entry;
    LONG ref;
    struct interaction_tracker *tracker;
    ULONGLONG last_tick;
    INT32 request_id;
};

static struct list tracker_inertia_operations = LIST_INIT(tracker_inertia_operations);
static BOOL tracker_inertia_worker_active;
static void tracker_inertia_predict_locked( struct interaction_tracker *impl );
static void interaction_tracker_clamp_scale( struct interaction_tracker *impl );
static void tracker_inertia_cancel_locked( struct interaction_tracker *impl, struct list *released );
static void tracker_inertia_free_list( struct list *released );
static void tracker_inertia_advance_locked( struct interaction_tracker *impl, ULONGLONG now,
        struct list *released, struct list *discarded );

static inline struct interaction_tracker *impl_from_IInteractionTracker( IInteractionTracker *iface )
{
    return CONTAINING_RECORD( iface, struct interaction_tracker, IInteractionTracker_iface );
}

static inline struct interaction_tracker *impl_from_IInteractionTracker4( IInteractionTracker4 *iface )
{
    return CONTAINING_RECORD( iface, struct interaction_tracker, IInteractionTracker4_iface );
}

static inline struct interaction_tracker *impl_from_IInteractionTracker5( IInteractionTracker5 *iface )
{
    return CONTAINING_RECORD( iface, struct interaction_tracker, IInteractionTracker5_iface );
}

static inline struct interaction_tracker *tracker_from_ICompositionObject( ICompositionObject *iface )
{
    return CONTAINING_RECORD( iface, struct interaction_tracker, ICompositionObject_iface );
}

static inline struct interaction_tracker *tracker_from_IClosable( IClosable *iface )
{
    return CONTAINING_RECORD( iface, struct interaction_tracker, IClosable_iface );
}

static const WCHAR *const tracker_component_names[] =
{
    L"MinPosition.X", L"MinPosition.Y", L"MinPosition.Z",
    L"MaxPosition.X", L"MaxPosition.Y", L"MaxPosition.Z",
};

static UINT tracker_component_index( HSTRING name )
{
    const WCHAR *property = WindowsGetStringRawBuffer( name, NULL );
    UINT i;

    for (i = 0; i < ARRAY_SIZE(tracker_component_names); ++i)
        if (!wcscmp( property, tracker_component_names[i] )) break;
    return i;
}

struct tracker_values_changed_args
{
    IInteractionTrackerValuesChangedArgs IInteractionTrackerValuesChangedArgs_iface;
    LONG ref;
    struct Vector3 position;
    FLOAT scale;
    INT32 request_id;
};

enum tracker_notification_kind
{
    TRACKER_VALUES,
    TRACKER_FRAME,
    TRACKER_INERTIA,
    TRACKER_IDLE,
};

struct tracker_state_args
{
    IInteractionTrackerInertiaStateEnteredArgs IInteractionTrackerInertiaStateEnteredArgs_iface;
    IInteractionTrackerInertiaStateEnteredArgs2 IInteractionTrackerInertiaStateEnteredArgs2_iface;
    IInteractionTrackerInertiaStateEnteredArgs3 IInteractionTrackerInertiaStateEnteredArgs3_iface;
    IInteractionTrackerIdleStateEnteredArgs IInteractionTrackerIdleStateEnteredArgs_iface;
    IInteractionTrackerIdleStateEnteredArgs2 IInteractionTrackerIdleStateEnteredArgs2_iface;
    enum tracker_notification_kind kind;
    LONG ref;
    INT32 request_id;
    struct Vector3 natural_position;
    struct Vector3 velocity;
    FLOAT scale;
    FLOAT scale_velocity;
    boolean impulse;
    boolean from_binding;
};

static HRESULT tracker_state_query( struct tracker_state_args *impl, REFIID iid, void **out )
{
    if (!out) return E_POINTER;
    *out = NULL;
    if (IsEqualGUID( iid, &IID_IUnknown ) || IsEqualGUID( iid, &IID_IInspectable ) || IsEqualGUID( iid, &IID_IAgileObject ))
        *out = impl->kind == TRACKER_INERTIA ? (void *)&impl->IInteractionTrackerInertiaStateEnteredArgs_iface :
                (void *)&impl->IInteractionTrackerIdleStateEnteredArgs_iface;
    else if (impl->kind == TRACKER_INERTIA)
    {
        if (IsEqualGUID( iid, &IID_IInteractionTrackerInertiaStateEnteredArgs ))
            *out = &impl->IInteractionTrackerInertiaStateEnteredArgs_iface;
        else if (IsEqualGUID( iid, &IID_IInteractionTrackerInertiaStateEnteredArgs2 ))
            *out = &impl->IInteractionTrackerInertiaStateEnteredArgs2_iface;
        else if (IsEqualGUID( iid, &IID_IInteractionTrackerInertiaStateEnteredArgs3 ))
            *out = &impl->IInteractionTrackerInertiaStateEnteredArgs3_iface;
    }
    else if (IsEqualGUID( iid, &IID_IInteractionTrackerIdleStateEnteredArgs ))
        *out = &impl->IInteractionTrackerIdleStateEnteredArgs_iface;
    else if (IsEqualGUID( iid, &IID_IInteractionTrackerIdleStateEnteredArgs2 ))
        *out = &impl->IInteractionTrackerIdleStateEnteredArgs2_iface;
    if (!*out) return E_NOINTERFACE;
    InterlockedIncrement( &impl->ref );
    return S_OK;
}

static ULONG tracker_state_release( struct tracker_state_args *impl )
{
    ULONG ref = InterlockedDecrement( &impl->ref );
    if (!ref) free( impl );
    return ref;
}

static HRESULT tracker_state_iids( struct tracker_state_args *impl, ULONG *count, IID **iids )
{
    if (!count || !iids) return E_POINTER;
    *count = impl->kind == TRACKER_INERTIA ? 3 : 2;
    if (!(*iids = CoTaskMemAlloc( *count * sizeof(**iids) ))) return E_OUTOFMEMORY;
    if (impl->kind == TRACKER_INERTIA)
    {
        (*iids)[0] = IID_IInteractionTrackerInertiaStateEnteredArgs;
        (*iids)[1] = IID_IInteractionTrackerInertiaStateEnteredArgs2;
        (*iids)[2] = IID_IInteractionTrackerInertiaStateEnteredArgs3;
    }
    else
    {
        (*iids)[0] = IID_IInteractionTrackerIdleStateEnteredArgs;
        (*iids)[1] = IID_IInteractionTrackerIdleStateEnteredArgs2;
    }
    return S_OK;
}

static HRESULT tracker_state_name( struct tracker_state_args *impl, HSTRING *name )
{
    const WCHAR *value = impl->kind == TRACKER_INERTIA ?
            RuntimeClass_Windows_UI_Composition_Interactions_InteractionTrackerInertiaStateEnteredArgs :
            RuntimeClass_Windows_UI_Composition_Interactions_InteractionTrackerIdleStateEnteredArgs;
    if (!name) return E_POINTER;
    return WindowsCreateString( value, wcslen( value ), name );
}

static inline struct tracker_state_args *tracker_inertia_args_impl( IInteractionTrackerInertiaStateEnteredArgs *iface )
{
    return CONTAINING_RECORD( iface, struct tracker_state_args, IInteractionTrackerInertiaStateEnteredArgs_iface );
}

static HRESULT WINAPI tracker_inertia_args_QueryInterface( IInteractionTrackerInertiaStateEnteredArgs *iface, REFIID iid, void **out )
{
    return tracker_state_query( tracker_inertia_args_impl( iface ), iid, out );
}

static ULONG WINAPI tracker_inertia_args_AddRef( IInteractionTrackerInertiaStateEnteredArgs *iface )
{
    return InterlockedIncrement( &tracker_inertia_args_impl( iface )->ref );
}

static ULONG WINAPI tracker_inertia_args_Release( IInteractionTrackerInertiaStateEnteredArgs *iface )
{
    return tracker_state_release( tracker_inertia_args_impl( iface ) );
}

static HRESULT WINAPI tracker_inertia_args_GetIids( IInteractionTrackerInertiaStateEnteredArgs *iface, ULONG *count, IID **iids )
{
    return tracker_state_iids( tracker_inertia_args_impl( iface ), count, iids );
}

static HRESULT WINAPI tracker_inertia_args_GetRuntimeClassName( IInteractionTrackerInertiaStateEnteredArgs *iface, HSTRING *name )
{
    return tracker_state_name( tracker_inertia_args_impl( iface ), name );
}

static HRESULT WINAPI tracker_inertia_args_GetTrustLevel( IInteractionTrackerInertiaStateEnteredArgs *iface, TrustLevel *level )
{
    if (!level) return E_POINTER;
    *level = BaseTrust;
    return S_OK;
}

static HRESULT WINAPI tracker_inertia_args_get_ModifiedRestingPosition( IInteractionTrackerInertiaStateEnteredArgs *iface, __FIReference_1_Vector3 * *value )
{
    if (!value) return E_POINTER;
    *value = NULL;
    return S_OK;
}

static HRESULT WINAPI tracker_inertia_args_get_ModifiedRestingScale( IInteractionTrackerInertiaStateEnteredArgs *iface, __FIReference_1_FLOAT * *value )
{
    if (!value) return E_POINTER;
    *value = NULL;
    return S_OK;
}

static HRESULT WINAPI tracker_inertia_args_get_NaturalRestingPosition( IInteractionTrackerInertiaStateEnteredArgs *iface, struct Vector3 *value )
{
    struct tracker_state_args *impl = tracker_inertia_args_impl( iface );

    if (!value) return E_POINTER;
    *value = impl->natural_position;
    return S_OK;
}

static HRESULT WINAPI tracker_inertia_args_get_NaturalRestingScale( IInteractionTrackerInertiaStateEnteredArgs *iface, FLOAT *value )
{
    struct tracker_state_args *impl = tracker_inertia_args_impl( iface );

    if (!value) return E_POINTER;
    *value = impl->scale;
    return S_OK;
}

static HRESULT WINAPI tracker_inertia_args_get_PositionVelocityInPixelsPerSecond( IInteractionTrackerInertiaStateEnteredArgs *iface, struct Vector3 *value )
{
    struct tracker_state_args *impl = tracker_inertia_args_impl( iface );

    if (!value) return E_POINTER;
    *value = impl->velocity;
    return S_OK;
}

static HRESULT WINAPI tracker_inertia_args_get_RequestId( IInteractionTrackerInertiaStateEnteredArgs *iface, INT32 *value )
{
    struct tracker_state_args *impl = tracker_inertia_args_impl( iface );

    if (!value) return E_POINTER;
    *value = impl->request_id;
    return S_OK;
}

static HRESULT WINAPI tracker_inertia_args_get_ScaleVelocityInPercentPerSecond( IInteractionTrackerInertiaStateEnteredArgs *iface, FLOAT *value )
{
    if (!value) return E_POINTER;
    *value = tracker_inertia_args_impl( iface )->scale_velocity;
    return S_OK;
}

static const IInteractionTrackerInertiaStateEnteredArgsVtbl tracker_inertia_args_vtbl =
{
    tracker_inertia_args_QueryInterface,
    tracker_inertia_args_AddRef,
    tracker_inertia_args_Release,
    tracker_inertia_args_GetIids,
    tracker_inertia_args_GetRuntimeClassName,
    tracker_inertia_args_GetTrustLevel,
    tracker_inertia_args_get_ModifiedRestingPosition,
    tracker_inertia_args_get_ModifiedRestingScale,
    tracker_inertia_args_get_NaturalRestingPosition,
    tracker_inertia_args_get_NaturalRestingScale,
    tracker_inertia_args_get_PositionVelocityInPixelsPerSecond,
    tracker_inertia_args_get_RequestId,
    tracker_inertia_args_get_ScaleVelocityInPercentPerSecond,
};

static inline struct tracker_state_args *tracker_inertia_args2_impl( IInteractionTrackerInertiaStateEnteredArgs2 *iface )
{
    return CONTAINING_RECORD( iface, struct tracker_state_args, IInteractionTrackerInertiaStateEnteredArgs2_iface );
}

static HRESULT WINAPI tracker_inertia_args2_QueryInterface( IInteractionTrackerInertiaStateEnteredArgs2 *iface, REFIID iid, void **out )
{
    return tracker_state_query( tracker_inertia_args2_impl( iface ), iid, out );
}

static ULONG WINAPI tracker_inertia_args2_AddRef( IInteractionTrackerInertiaStateEnteredArgs2 *iface )
{
    return InterlockedIncrement( &tracker_inertia_args2_impl( iface )->ref );
}

static ULONG WINAPI tracker_inertia_args2_Release( IInteractionTrackerInertiaStateEnteredArgs2 *iface )
{
    return tracker_state_release( tracker_inertia_args2_impl( iface ) );
}

static HRESULT WINAPI tracker_inertia_args2_GetIids( IInteractionTrackerInertiaStateEnteredArgs2 *iface, ULONG *count, IID **iids )
{
    return tracker_state_iids( tracker_inertia_args2_impl( iface ), count, iids );
}

static HRESULT WINAPI tracker_inertia_args2_GetRuntimeClassName( IInteractionTrackerInertiaStateEnteredArgs2 *iface, HSTRING *name )
{
    return tracker_state_name( tracker_inertia_args2_impl( iface ), name );
}

static HRESULT WINAPI tracker_inertia_args2_GetTrustLevel( IInteractionTrackerInertiaStateEnteredArgs2 *iface, TrustLevel *level )
{
    if (!level) return E_POINTER;
    *level = BaseTrust;
    return S_OK;
}

static HRESULT WINAPI tracker_inertia_args2_get_IsInertiaFromImpulse( IInteractionTrackerInertiaStateEnteredArgs2 *iface, boolean *value )
{
    struct tracker_state_args *impl = tracker_inertia_args2_impl( iface );

    if (!value) return E_POINTER;
    *value = impl->impulse;
    return S_OK;
}

static const IInteractionTrackerInertiaStateEnteredArgs2Vtbl tracker_inertia_args2_vtbl =
{
    tracker_inertia_args2_QueryInterface,
    tracker_inertia_args2_AddRef,
    tracker_inertia_args2_Release,
    tracker_inertia_args2_GetIids,
    tracker_inertia_args2_GetRuntimeClassName,
    tracker_inertia_args2_GetTrustLevel,
    tracker_inertia_args2_get_IsInertiaFromImpulse,
};

static inline struct tracker_state_args *tracker_inertia_args3_impl( IInteractionTrackerInertiaStateEnteredArgs3 *iface )
{
    return CONTAINING_RECORD( iface, struct tracker_state_args, IInteractionTrackerInertiaStateEnteredArgs3_iface );
}

static HRESULT WINAPI tracker_inertia_args3_QueryInterface( IInteractionTrackerInertiaStateEnteredArgs3 *iface, REFIID iid, void **out )
{
    return tracker_state_query( tracker_inertia_args3_impl( iface ), iid, out );
}

static ULONG WINAPI tracker_inertia_args3_AddRef( IInteractionTrackerInertiaStateEnteredArgs3 *iface )
{
    return InterlockedIncrement( &tracker_inertia_args3_impl( iface )->ref );
}

static ULONG WINAPI tracker_inertia_args3_Release( IInteractionTrackerInertiaStateEnteredArgs3 *iface )
{
    return tracker_state_release( tracker_inertia_args3_impl( iface ) );
}

static HRESULT WINAPI tracker_inertia_args3_GetIids( IInteractionTrackerInertiaStateEnteredArgs3 *iface, ULONG *count, IID **iids )
{
    return tracker_state_iids( tracker_inertia_args3_impl( iface ), count, iids );
}

static HRESULT WINAPI tracker_inertia_args3_GetRuntimeClassName( IInteractionTrackerInertiaStateEnteredArgs3 *iface, HSTRING *name )
{
    return tracker_state_name( tracker_inertia_args3_impl( iface ), name );
}

static HRESULT WINAPI tracker_inertia_args3_GetTrustLevel( IInteractionTrackerInertiaStateEnteredArgs3 *iface, TrustLevel *level )
{
    if (!level) return E_POINTER;
    *level = BaseTrust;
    return S_OK;
}

static HRESULT WINAPI tracker_inertia_args3_get_IsFromBinding( IInteractionTrackerInertiaStateEnteredArgs3 *iface, boolean *value )
{
    struct tracker_state_args *impl = tracker_inertia_args3_impl( iface );

    if (!value) return E_POINTER;
    *value = impl->from_binding;
    return S_OK;
}

static const IInteractionTrackerInertiaStateEnteredArgs3Vtbl tracker_inertia_args3_vtbl =
{
    tracker_inertia_args3_QueryInterface,
    tracker_inertia_args3_AddRef,
    tracker_inertia_args3_Release,
    tracker_inertia_args3_GetIids,
    tracker_inertia_args3_GetRuntimeClassName,
    tracker_inertia_args3_GetTrustLevel,
    tracker_inertia_args3_get_IsFromBinding,
};

static inline struct tracker_state_args *tracker_idle_args_impl( IInteractionTrackerIdleStateEnteredArgs *iface )
{
    return CONTAINING_RECORD( iface, struct tracker_state_args, IInteractionTrackerIdleStateEnteredArgs_iface );
}

static HRESULT WINAPI tracker_idle_args_QueryInterface( IInteractionTrackerIdleStateEnteredArgs *iface, REFIID iid, void **out )
{
    return tracker_state_query( tracker_idle_args_impl( iface ), iid, out );
}

static ULONG WINAPI tracker_idle_args_AddRef( IInteractionTrackerIdleStateEnteredArgs *iface )
{
    return InterlockedIncrement( &tracker_idle_args_impl( iface )->ref );
}

static ULONG WINAPI tracker_idle_args_Release( IInteractionTrackerIdleStateEnteredArgs *iface )
{
    return tracker_state_release( tracker_idle_args_impl( iface ) );
}

static HRESULT WINAPI tracker_idle_args_GetIids( IInteractionTrackerIdleStateEnteredArgs *iface, ULONG *count, IID **iids )
{
    return tracker_state_iids( tracker_idle_args_impl( iface ), count, iids );
}

static HRESULT WINAPI tracker_idle_args_GetRuntimeClassName( IInteractionTrackerIdleStateEnteredArgs *iface, HSTRING *name )
{
    return tracker_state_name( tracker_idle_args_impl( iface ), name );
}

static HRESULT WINAPI tracker_idle_args_GetTrustLevel( IInteractionTrackerIdleStateEnteredArgs *iface, TrustLevel *level )
{
    if (!level) return E_POINTER;
    *level = BaseTrust;
    return S_OK;
}

static HRESULT WINAPI tracker_idle_args_get_RequestId( IInteractionTrackerIdleStateEnteredArgs *iface, INT32 *value )
{
    struct tracker_state_args *impl = tracker_idle_args_impl( iface );

    if (!value) return E_POINTER;
    *value = impl->request_id;
    return S_OK;
}

static const IInteractionTrackerIdleStateEnteredArgsVtbl tracker_idle_args_vtbl =
{
    tracker_idle_args_QueryInterface,
    tracker_idle_args_AddRef,
    tracker_idle_args_Release,
    tracker_idle_args_GetIids,
    tracker_idle_args_GetRuntimeClassName,
    tracker_idle_args_GetTrustLevel,
    tracker_idle_args_get_RequestId,
};

static inline struct tracker_state_args *tracker_idle_args2_impl( IInteractionTrackerIdleStateEnteredArgs2 *iface )
{
    return CONTAINING_RECORD( iface, struct tracker_state_args, IInteractionTrackerIdleStateEnteredArgs2_iface );
}

static HRESULT WINAPI tracker_idle_args2_QueryInterface( IInteractionTrackerIdleStateEnteredArgs2 *iface, REFIID iid, void **out )
{
    return tracker_state_query( tracker_idle_args2_impl( iface ), iid, out );
}

static ULONG WINAPI tracker_idle_args2_AddRef( IInteractionTrackerIdleStateEnteredArgs2 *iface )
{
    return InterlockedIncrement( &tracker_idle_args2_impl( iface )->ref );
}

static ULONG WINAPI tracker_idle_args2_Release( IInteractionTrackerIdleStateEnteredArgs2 *iface )
{
    return tracker_state_release( tracker_idle_args2_impl( iface ) );
}

static HRESULT WINAPI tracker_idle_args2_GetIids( IInteractionTrackerIdleStateEnteredArgs2 *iface, ULONG *count, IID **iids )
{
    return tracker_state_iids( tracker_idle_args2_impl( iface ), count, iids );
}

static HRESULT WINAPI tracker_idle_args2_GetRuntimeClassName( IInteractionTrackerIdleStateEnteredArgs2 *iface, HSTRING *name )
{
    return tracker_state_name( tracker_idle_args2_impl( iface ), name );
}

static HRESULT WINAPI tracker_idle_args2_GetTrustLevel( IInteractionTrackerIdleStateEnteredArgs2 *iface, TrustLevel *level )
{
    if (!level) return E_POINTER;
    *level = BaseTrust;
    return S_OK;
}

static HRESULT WINAPI tracker_idle_args2_get_IsFromBinding( IInteractionTrackerIdleStateEnteredArgs2 *iface, boolean *value )
{
    struct tracker_state_args *impl = tracker_idle_args2_impl( iface );

    if (!value) return E_POINTER;
    *value = impl->from_binding;
    return S_OK;
}

static const IInteractionTrackerIdleStateEnteredArgs2Vtbl tracker_idle_args2_vtbl =
{
    tracker_idle_args2_QueryInterface,
    tracker_idle_args2_AddRef,
    tracker_idle_args2_Release,
    tracker_idle_args2_GetIids,
    tracker_idle_args2_GetRuntimeClassName,
    tracker_idle_args2_GetTrustLevel,
    tracker_idle_args2_get_IsFromBinding,
};

struct tracker_notification
{
    struct list entry;
    IInspectable *args;
    enum tracker_notification_kind kind;
    INT32 request_id;
    IInteractionTracker *sender;
    IAgileReference *owner;
};

static CRITICAL_SECTION tracker_notification_cs;
static CRITICAL_SECTION_DEBUG tracker_notification_cs_debug =
{
    0, 0, &tracker_notification_cs,
    { &tracker_notification_cs_debug.ProcessLocksList, &tracker_notification_cs_debug.ProcessLocksList },
    0, 0, { (DWORD_PTR)(__FILE__ ": tracker_notification_cs") }
};
static CRITICAL_SECTION tracker_notification_cs = { &tracker_notification_cs_debug, -1, 0, 0, 0, 0 };
static struct list tracker_notifications = LIST_INIT(tracker_notifications);
static BOOL tracker_notification_worker_active;

static inline struct tracker_values_changed_args *impl_from_IInteractionTrackerValuesChangedArgs(
        IInteractionTrackerValuesChangedArgs *iface )
{
    return CONTAINING_RECORD( iface, struct tracker_values_changed_args, IInteractionTrackerValuesChangedArgs_iface );
}

static HRESULT WINAPI tracker_values_QueryInterface( IInteractionTrackerValuesChangedArgs *iface,
        REFIID iid, void **out )
{
    if (!out) return E_POINTER;
    *out = NULL;
    if (!IsEqualGUID( iid, &IID_IUnknown ) && !IsEqualGUID( iid, &IID_IInspectable ) &&
            !IsEqualGUID( iid, &IID_IAgileObject ) && !IsEqualGUID( iid, &IID_IInteractionTrackerValuesChangedArgs ))
        return E_NOINTERFACE;
    *out = iface;
    IInteractionTrackerValuesChangedArgs_AddRef( iface );
    return S_OK;
}

static ULONG WINAPI tracker_values_AddRef( IInteractionTrackerValuesChangedArgs *iface )
{
    return InterlockedIncrement( &impl_from_IInteractionTrackerValuesChangedArgs( iface )->ref );
}

static ULONG WINAPI tracker_values_Release( IInteractionTrackerValuesChangedArgs *iface )
{
    struct tracker_values_changed_args *impl = impl_from_IInteractionTrackerValuesChangedArgs( iface );
    ULONG ref = InterlockedDecrement( &impl->ref );

    if (!ref) free( impl );
    return ref;
}

static HRESULT WINAPI tracker_values_GetIids( IInteractionTrackerValuesChangedArgs *iface,
        ULONG *count, IID **iids )
{
    if (!count || !iids) return E_POINTER;
    *count = 0;
    if (!(*iids = CoTaskMemAlloc( sizeof(**iids) ))) return E_OUTOFMEMORY;
    **iids = IID_IInteractionTrackerValuesChangedArgs;
    *count = 1;
    return S_OK;
}

static HRESULT WINAPI tracker_values_GetRuntimeClassName( IInteractionTrackerValuesChangedArgs *iface,
        HSTRING *value )
{
    if (!value) return E_POINTER;
    return WindowsCreateString( RuntimeClass_Windows_UI_Composition_Interactions_InteractionTrackerValuesChangedArgs,
            ARRAY_SIZE(RuntimeClass_Windows_UI_Composition_Interactions_InteractionTrackerValuesChangedArgs) - 1, value );
}

static HRESULT WINAPI tracker_values_GetTrustLevel( IInteractionTrackerValuesChangedArgs *iface, TrustLevel *value )
{
    if (!value) return E_POINTER;
    *value = BaseTrust;
    return S_OK;
}

static HRESULT WINAPI tracker_values_get_Position( IInteractionTrackerValuesChangedArgs *iface,
        struct Vector3 *value )
{
    if (!value) return E_POINTER;
    *value = impl_from_IInteractionTrackerValuesChangedArgs( iface )->position;
    return S_OK;
}

static HRESULT WINAPI tracker_values_get_RequestId( IInteractionTrackerValuesChangedArgs *iface, INT32 *value )
{
    if (!value) return E_POINTER;
    *value = impl_from_IInteractionTrackerValuesChangedArgs( iface )->request_id;
    return S_OK;
}

static HRESULT WINAPI tracker_values_get_Scale( IInteractionTrackerValuesChangedArgs *iface, FLOAT *value )
{
    if (!value) return E_POINTER;
    *value = impl_from_IInteractionTrackerValuesChangedArgs( iface )->scale;
    return S_OK;
}

static const IInteractionTrackerValuesChangedArgsVtbl tracker_values_vtbl =
{
    tracker_values_QueryInterface,
    tracker_values_AddRef,
    tracker_values_Release,
    tracker_values_GetIids,
    tracker_values_GetRuntimeClassName,
    tracker_values_GetTrustLevel,
    tracker_values_get_Position,
    tracker_values_get_RequestId,
    tracker_values_get_Scale,
};

static void tracker_notification_free( struct tracker_notification *notification )
{
    IInspectable_Release( notification->args );
    IAgileReference_Release( notification->owner );
    IInteractionTracker_Release( notification->sender );
    free( notification );
}

static void tracker_notifications_free( struct list *notifications )
{
    struct tracker_notification *notification;

    while (!list_empty( notifications ))
    {
        notification = LIST_ENTRY( list_head( notifications ), struct tracker_notification, entry );
        list_remove( &notification->entry );
        tracker_notification_free( notification );
    }
}

static void CALLBACK tracker_notification_worker( PTP_CALLBACK_INSTANCE instance, void *context )
{
    struct tracker_notification *notification;
    struct interaction_tracker *tracker;
    IInteractionTrackerOwner *owner;
    HRESULT initialized, hr;
    BOOL closed;

    initialized = CoInitializeEx( NULL, COINIT_MULTITHREADED );
    for (;;)
    {
        EnterCriticalSection( &tracker_notification_cs );
        if (list_empty( &tracker_notifications ))
        {
            tracker_notification_worker_active = FALSE;
            LeaveCriticalSection( &tracker_notification_cs );
            break;
        }
        notification = LIST_ENTRY( list_head( &tracker_notifications ), struct tracker_notification, entry );
        list_remove( &notification->entry );
        LeaveCriticalSection( &tracker_notification_cs );

        tracker = impl_from_IInteractionTracker( notification->sender );
        EnterCriticalSection( &tracker->animation_cs );
        closed = tracker->notifications_closed;
        LeaveCriticalSection( &tracker->animation_cs );
        hr = closed ? RO_E_CLOSED : initialized;
        if (SUCCEEDED(hr))
            hr = IAgileReference_Resolve( notification->owner, &IID_IInteractionTrackerOwner, (void **)&owner );
        if (SUCCEEDED(hr))
        {
            if (notification->kind == TRACKER_INERTIA)
                hr = IInteractionTrackerOwner_InertiaStateEntered( owner, notification->sender,
                        (IInteractionTrackerInertiaStateEnteredArgs *)notification->args );
            else if (notification->kind == TRACKER_IDLE)
                hr = IInteractionTrackerOwner_IdleStateEntered( owner, notification->sender,
                        (IInteractionTrackerIdleStateEnteredArgs *)notification->args );
            else
                hr = IInteractionTrackerOwner_ValuesChanged( owner, notification->sender,
                        (IInteractionTrackerValuesChangedArgs *)notification->args );
            IInteractionTrackerOwner_Release( owner );
        }
        if (FAILED(hr) && hr != RO_E_CLOSED) WARN( "Tracker notification %u failed, hr %#lx.\n", notification->kind, hr );
        tracker_notification_free( notification );
    }
    if (SUCCEEDED(initialized)) CoUninitialize();
}

static BOOL interaction_tracker_try_addref( struct interaction_tracker *impl )
{
    LONG ref = InterlockedCompareExchange( &impl->ref, 0, 0 ), previous;

    while (ref > 0)
    {
        previous = InterlockedCompareExchange( &impl->ref, ref + 1, ref );
        if (previous == ref) return TRUE;
        ref = previous;
    }
    return FALSE;
}

static void tracker_notification_submit( struct tracker_notification *notification, struct list *discarded )
{
    struct tracker_notification *previous;

    EnterCriticalSection( &tracker_notification_cs );
    if (notification->kind == TRACKER_FRAME)
    {
        LIST_FOR_EACH_ENTRY_REV( previous, &tracker_notifications, struct tracker_notification, entry )
        {
            if (previous->sender != notification->sender) continue;
            if (previous->kind == TRACKER_FRAME && previous->request_id == notification->request_id)
            {
                list_remove( &previous->entry );
                list_add_tail( discarded, &previous->entry );
            }
            break;
        }
    }
    list_add_tail( &tracker_notifications, &notification->entry );
    if (!tracker_notification_worker_active)
    {
        tracker_notification_worker_active = TRUE;
        if (!TrySubmitThreadpoolCallback( tracker_notification_worker, NULL, NULL ))
        {
            tracker_notification_worker_active = FALSE;
            list_remove( &notification->entry );
            list_add_tail( discarded, &notification->entry );
            WARN( "Could not queue ValuesChanged delivery, error %lu.\n", GetLastError() );
        }
    }
    LeaveCriticalSection( &tracker_notification_cs );
}

static void interaction_tracker_queue_notification( struct interaction_tracker *impl, INT32 request_id,
        BOOL frame, struct list *discarded )
{
    struct tracker_notification *notification;
    struct tracker_values_changed_args *args;
    BOOL enabled;

    EnterCriticalSection( &impl->animation_cs );
    enabled = impl->owner && !impl->notifications_closed;
    LeaveCriticalSection( &impl->animation_cs );
    if (!enabled) return;
    if (!(notification = calloc( 1, sizeof(*notification) ))) return;
    if (!(args = calloc( 1, sizeof(*args) )))
    {
        free( notification );
        return;
    }
    EnterCriticalSection( &impl->animation_cs );
    if (impl->notifications_closed || !impl->owner || !interaction_tracker_try_addref( impl ))
    {
        LeaveCriticalSection( &impl->animation_cs );
        free( args );
        free( notification );
        return;
    }
    args->IInteractionTrackerValuesChangedArgs_iface.lpVtbl = &tracker_values_vtbl;
    args->ref = 1;
    args->position = impl->position;
    args->scale = impl->scale;
    args->request_id = request_id;
    notification->kind = frame ? TRACKER_FRAME : TRACKER_VALUES;
    notification->request_id = request_id;
    notification->args = (IInspectable *)&args->IInteractionTrackerValuesChangedArgs_iface;
    notification->sender = &impl->IInteractionTracker_iface;
    notification->owner = impl->owner;
    IAgileReference_AddRef( notification->owner );
    LeaveCriticalSection( &impl->animation_cs );

    tracker_notification_submit( notification, discarded );
}

static void interaction_tracker_queue_state( struct interaction_tracker *impl,
        enum tracker_notification_kind kind, INT32 request_id, boolean from_binding, struct list *discarded )
{
    struct tracker_state_args *args;
    struct tracker_notification *notification;

    if (!(args = calloc( 1, sizeof(*args) ))) return;
    if (!(notification = calloc( 1, sizeof(*notification) )))
    {
        free( args );
        return;
    }
    EnterCriticalSection( &impl->animation_cs );
    if (impl->notifications_closed || !impl->owner || !interaction_tracker_try_addref( impl ))
    {
        LeaveCriticalSection( &impl->animation_cs );
        free( args );
        free( notification );
        return;
    }
    args->IInteractionTrackerInertiaStateEnteredArgs_iface.lpVtbl = &tracker_inertia_args_vtbl;
    args->IInteractionTrackerInertiaStateEnteredArgs2_iface.lpVtbl = &tracker_inertia_args2_vtbl;
    args->IInteractionTrackerInertiaStateEnteredArgs3_iface.lpVtbl = &tracker_inertia_args3_vtbl;
    args->IInteractionTrackerIdleStateEnteredArgs_iface.lpVtbl = &tracker_idle_args_vtbl;
    args->IInteractionTrackerIdleStateEnteredArgs2_iface.lpVtbl = &tracker_idle_args2_vtbl;
    args->ref = 1;
    args->kind = kind;
    args->request_id = request_id;
    args->natural_position = impl->natural_position;
    args->velocity = impl->position_velocity;
    args->scale = impl->natural_scale;
    args->scale_velocity = impl->scale_velocity;
    args->impulse = kind == TRACKER_INERTIA;
    args->from_binding = from_binding;
    notification->args = kind == TRACKER_INERTIA ? (IInspectable *)&args->IInteractionTrackerInertiaStateEnteredArgs_iface :
            (IInspectable *)&args->IInteractionTrackerIdleStateEnteredArgs_iface;
    notification->kind = kind;
    notification->sender = &impl->IInteractionTracker_iface;
    notification->owner = impl->owner;
    IAgileReference_AddRef( notification->owner );
    LeaveCriticalSection( &impl->animation_cs );
    tracker_notification_submit( notification, discarded );
}

static void interaction_tracker_cancel_notifications( struct interaction_tracker *impl )
{
    struct tracker_notification *notification, *next;
    struct list discarded;

    list_init( &discarded );
    EnterCriticalSection( &tracker_notification_cs );
    LIST_FOR_EACH_ENTRY_SAFE( notification, next, &tracker_notifications, struct tracker_notification, entry )
        if (notification->sender == &impl->IInteractionTracker_iface)
        {
            list_remove( &notification->entry );
            list_add_tail( &discarded, &notification->entry );
        }
    LeaveCriticalSection( &tracker_notification_cs );
    tracker_notifications_free( &discarded );
}

static void interaction_tracker_mark_changed( struct interaction_tracker *impl, struct list *changed )
{
    if (list_empty( &impl->changed_entry )) list_add_tail( changed, &impl->changed_entry );
}

static void interaction_tracker_notify_changes( struct interaction_tracker *impl, INT32 request_id,
        BOOL frame, struct list *changed, struct list *discarded )
{
    struct interaction_tracker *tracker;

    while (!list_empty( changed ))
    {
        tracker = LIST_ENTRY( list_head( changed ), struct interaction_tracker, changed_entry );
        list_remove( &tracker->changed_entry );
        list_init( &tracker->changed_entry );
        interaction_tracker_queue_notification( tracker, tracker == impl ? request_id : 0, frame, discarded );
    }
}

static void interaction_tracker_clear_animations( struct interaction_tracker *impl )
{
    IExpressionAnimation *previous[6];
    UINT i;

    EnterCriticalSection( &impl->animation_cs );
    memcpy( previous, impl->expressions, sizeof(previous) );
    memset( impl->expressions, 0, sizeof(impl->expressions) );
    ++impl->animation_revision;
    LeaveCriticalSection( &impl->animation_cs );
    for (i = 0; i < ARRAY_SIZE(previous); ++i)
        if (previous[i]) IExpressionAnimation_Release( previous[i] );
}

static HRESULT interaction_tracker_evaluate_bounds( struct interaction_tracker *impl )
{
    IExpressionAnimation *expressions[6];
    FLOAT values[6];
    UINT i, revision;
    DWORD thread = GetCurrentThreadId();
    BOOL active = FALSE;
    HRESULT hr = S_OK, result;

    EnterCriticalSection( &impl->animation_cs );
    if (impl->evaluation_thread)
    {
        hr = impl->evaluation_thread == thread ? E_ILLEGAL_METHOD_CALL : S_FALSE;
        LeaveCriticalSection( &impl->animation_cs );
        return hr;
    }
    for (i = 0; i < ARRAY_SIZE(expressions); ++i)
    {
        expressions[i] = impl->expressions[i];
        if (expressions[i])
        {
            IExpressionAnimation_AddRef( expressions[i] );
            active = TRUE;
        }
    }
    if (!active)
    {
        LeaveCriticalSection( &impl->animation_cs );
        return S_OK;
    }
    values[0] = impl->min_position.X;
    values[1] = impl->min_position.Y;
    values[2] = impl->min_position.Z;
    values[3] = impl->max_position.X;
    values[4] = impl->max_position.Y;
    values[5] = impl->max_position.Z;
    revision = impl->animation_revision;
    impl->evaluation_thread = thread;
    LeaveCriticalSection( &impl->animation_cs );

    for (i = 0; i < ARRAY_SIZE(expressions); ++i)
        if (expressions[i] && FAILED(result = expression_animation_evaluate_scalar( expressions[i],
                &impl->ICompositionObject_iface, &values[i] ))) hr = result;
    for (i = 0; i < 3; ++i)
        if (values[i] > values[i + 3]) hr = E_INVALIDARG;

    EnterCriticalSection( &impl->animation_cs );
    if (SUCCEEDED(hr) && revision == impl->animation_revision)
    {
        impl->min_position = (struct Vector3){values[0], values[1], values[2]};
        impl->max_position = (struct Vector3){values[3], values[4], values[5]};
        if (impl->clamp_position)
        {
            impl->position.X = min( max( impl->position.X, values[0] ), values[3] );
            impl->position.Y = min( max( impl->position.Y, values[1] ), values[4] );
            impl->position.Z = min( max( impl->position.Z, values[2] ), values[5] );
        }
    }
    impl->evaluation_thread = 0;
    LeaveCriticalSection( &impl->animation_cs );
    for (i = 0; i < ARRAY_SIZE(expressions); ++i)
        if (expressions[i]) IExpressionAnimation_Release( expressions[i] );
    return hr;
}

static struct interaction_tracker_binding *interaction_tracker_find_binding(
        struct interaction_tracker *impl, struct interaction_tracker *peer )
{
    struct interaction_tracker_binding *binding;

    LIST_FOR_EACH_ENTRY( binding, &impl->bindings, struct interaction_tracker_binding, entry )
    {
        if (binding->peer == peer) return binding;
    }
    return NULL;
}

static void interaction_tracker_remove_binding( struct interaction_tracker_binding *binding )
{
    if (!binding) return;
    list_remove( &binding->entry );
    free( binding );
}

static void interaction_tracker_clear_bindings( struct interaction_tracker *impl )
{
    struct interaction_tracker_binding *binding, *peer_binding;

    EnterCriticalSection( &tracker_binding_cs );
    while (!list_empty( &impl->bindings ))
    {
        binding = LIST_ENTRY( list_head( &impl->bindings ), struct interaction_tracker_binding, entry );
        peer_binding = interaction_tracker_find_binding( binding->peer, impl );
        interaction_tracker_remove_binding( peer_binding );
        interaction_tracker_remove_binding( binding );
    }
    LeaveCriticalSection( &tracker_binding_cs );
}

static HRESULT WINAPI interaction_tracker_QueryInterface( IInteractionTracker *iface,
        REFIID iid, void **out )
{
    FIXME( "iface %p, iid %s, out %p.\n", iface, debugstr_guid( iid ), out );
    if (!out) return E_POINTER;
    if (IsEqualGUID( iid, &IID_IUnknown ) || IsEqualGUID( iid, &IID_IInspectable ) ||
        IsEqualGUID( iid, &IID_IAgileObject ) || IsEqualGUID( iid, &IID_IInteractionTracker ))
        *out = iface;
    else if (IsEqualGUID( iid, &IID_IInteractionTracker4 ))
        *out = &impl_from_IInteractionTracker( iface )->IInteractionTracker4_iface;
    else if (IsEqualGUID( iid, &IID_IInteractionTracker5 ))
        *out = &impl_from_IInteractionTracker( iface )->IInteractionTracker5_iface;
    else if (IsEqualGUID( iid, &IID_ICompositionObject ))
        *out = &impl_from_IInteractionTracker( iface )->ICompositionObject_iface;
    else if (IsEqualGUID( iid, &IID_IClosable ))
        *out = &impl_from_IInteractionTracker( iface )->IClosable_iface;
    else
    {
        *out = NULL;
        return E_NOINTERFACE;
    }
    IUnknown_AddRef( (IUnknown *)*out );
    return S_OK;
}

static ULONG WINAPI interaction_tracker_AddRef( IInteractionTracker *iface )
{
    return InterlockedIncrement( &impl_from_IInteractionTracker( iface )->ref );
}

static ULONG WINAPI interaction_tracker_Release( IInteractionTracker *iface )
{
    struct interaction_tracker *impl = impl_from_IInteractionTracker( iface );
    ULONG ref = InterlockedDecrement( &impl->ref );

    if (!ref)
    {
        interaction_tracker_clear_animations( impl );
        interaction_tracker_clear_bindings( impl );
        if (impl->sources) ICompositionInteractionSourceCollection_Release( impl->sources );
        if (impl->properties) ICompositionPropertySet_Release( impl->properties );
        ICompositor_Release( impl->compositor );
        if (impl->owner) IAgileReference_Release( impl->owner );
        if (impl->owner_mta_cookie) CoDecrementMTAUsage( impl->owner_mta_cookie );
        tracker_decay_release( impl->position_decay_rate );
        tracker_decay_release( impl->scale_decay_rate );
        DeleteCriticalSection( &impl->animation_cs );
        free( impl );
    }
    return ref;
}

static HRESULT WINAPI interaction_tracker_GetIids( IInteractionTracker *iface, ULONG *count, IID **iids )
{
    IID *values;

    if (!count || !iids) return E_POINTER;
    if (!(values = CoTaskMemAlloc( 5 * sizeof(*values) ))) return E_OUTOFMEMORY;
    values[0] = IID_IInteractionTracker;
    values[1] = IID_IInteractionTracker5;
    values[2] = IID_ICompositionObject;
    values[3] = IID_IClosable;
    values[4] = IID_IInteractionTracker4;
    *count = 5;
    *iids = values;
    return S_OK;
}

static HRESULT WINAPI interaction_tracker_GetRuntimeClassName( IInteractionTracker *iface, HSTRING *name )
{
    if (!name) return E_POINTER;
    return WindowsCreateString( RuntimeClass_Windows_UI_Composition_Interactions_InteractionTracker,
                                ARRAY_SIZE(RuntimeClass_Windows_UI_Composition_Interactions_InteractionTracker) - 1,
                                name );
}

static HRESULT WINAPI interaction_tracker_GetTrustLevel( IInteractionTracker *iface, TrustLevel *level )
{
    if (!level) return E_POINTER;
    *level = BaseTrust;
    return S_OK;
}

static HRESULT WINAPI interaction_tracker_get_InteractionSources( IInteractionTracker *iface,
        ICompositionInteractionSourceCollection **value )
{
    struct interaction_tracker *impl = impl_from_IInteractionTracker( iface );

    if (!value) return E_POINTER;
    ICompositionInteractionSourceCollection_AddRef( impl->sources );
    *value = impl->sources;
    return S_OK;
}

static HRESULT WINAPI interaction_tracker_get_IsPositionRoundingSuggested(
        IInteractionTracker *iface, boolean *value )
{
    if (!value) return E_POINTER;
    *value = FALSE;
    return S_OK;
}

#define INTERACTION_TRACKER_PROPERTY(name, type, field) \
    static HRESULT WINAPI interaction_tracker_get_##name( IInteractionTracker *iface, type *value ) \
    { \
        struct interaction_tracker *impl = impl_from_IInteractionTracker( iface ); \
        if (!value) return E_POINTER; \
        EnterCriticalSection( &impl->animation_cs ); \
        *value = impl->field; \
        LeaveCriticalSection( &impl->animation_cs ); \
        return S_OK; \
    } \
    static HRESULT WINAPI interaction_tracker_put_##name( IInteractionTracker *iface, type value ) \
    { \
        struct interaction_tracker *impl = impl_from_IInteractionTracker( iface ); \
        EnterCriticalSection( &impl->animation_cs ); \
        impl->field = value; \
        ++impl->animation_revision; \
        LeaveCriticalSection( &impl->animation_cs ); \
        return S_OK; \
    }

INTERACTION_TRACKER_PROPERTY( MaxPosition, struct Vector3, max_position )
INTERACTION_TRACKER_PROPERTY( MaxScale, FLOAT, max_scale )
INTERACTION_TRACKER_PROPERTY( MinPosition, struct Vector3, min_position )
INTERACTION_TRACKER_PROPERTY( MinScale, FLOAT, min_scale )

static HRESULT WINAPI interaction_tracker_get_NaturalRestingPosition( IInteractionTracker *iface,
        struct Vector3 *value )
{
    struct interaction_tracker *impl = impl_from_IInteractionTracker( iface );

    if (!value) return E_POINTER;
    EnterCriticalSection( &impl->animation_cs );
    *value = impl->inertia ? impl->natural_position : impl->position;
    LeaveCriticalSection( &impl->animation_cs );
    return S_OK;
}

static HRESULT WINAPI interaction_tracker_get_NaturalRestingScale( IInteractionTracker *iface, FLOAT *value )
{
    struct interaction_tracker *impl = impl_from_IInteractionTracker( iface );

    if (!value) return E_POINTER;
    interaction_tracker_clamp_scale( impl );
    EnterCriticalSection( &impl->animation_cs );
    *value = impl->inertia ? impl->natural_scale : impl->scale;
    LeaveCriticalSection( &impl->animation_cs );
    return S_OK;
}

static HRESULT WINAPI interaction_tracker_get_Owner( IInteractionTracker *iface, IInteractionTrackerOwner **value )
{
    struct interaction_tracker *impl = impl_from_IInteractionTracker( iface );
    IAgileReference *owner;
    HRESULT hr = S_OK;

    if (!value) return E_POINTER;
    *value = NULL;
    EnterCriticalSection( &impl->animation_cs );
    if ((owner = impl->owner)) IAgileReference_AddRef( owner );
    LeaveCriticalSection( &impl->animation_cs );
    if (owner)
    {
        hr = IAgileReference_Resolve( owner, &IID_IInteractionTrackerOwner, (void **)value );
        IAgileReference_Release( owner );
    }
    return hr;
}

static HRESULT WINAPI interaction_tracker_get_Position( IInteractionTracker *iface, struct Vector3 *value )
{
    struct interaction_tracker *impl = impl_from_IInteractionTracker( iface );
    HRESULT hr;
    BOOL evaluating;

    if (!value) return E_POINTER;
    EnterCriticalSection( &impl->animation_cs );
    evaluating = impl->evaluation_thread == GetCurrentThreadId();
    LeaveCriticalSection( &impl->animation_cs );
    if (!evaluating && FAILED(hr = interaction_tracker_evaluate_bounds( impl ))) return hr;
    EnterCriticalSection( &impl->animation_cs );
    *value = impl->position;
    LeaveCriticalSection( &impl->animation_cs );
    return S_OK;
}

static void tracker_decay_release( struct tracker_decay_value *value )
{
    if (value && !InterlockedDecrement( &value->ref ))
    {
        IInspectable_Release( value->value );
        free( value );
    }
}

static HRESULT tracker_inertia_get_decay( struct interaction_tracker *impl, BOOL position, IInspectable **value )
{
    struct tracker_decay_value *current;

    if (!value) return E_POINTER;
    EnterCriticalSection( &impl->animation_cs );
    current = position ? impl->position_decay_rate : impl->scale_decay_rate;
    if (current) InterlockedIncrement( &current->ref );
    LeaveCriticalSection( &impl->animation_cs );
    *value = current ? current->value : NULL;
    if (*value) IInspectable_AddRef( *value );
    tracker_decay_release( current );
    return S_OK;
}

static HRESULT tracker_inertia_set_decay( struct interaction_tracker *impl, BOOL position, IInspectable *value )
{
    struct Vector3 rate = {0.95f,0.95f,0.95f};
    FLOAT scalar_rate = 0.985f;
    __FIReference_1_Vector3 *vector;
    __FIReference_1_FLOAT *scalar;
    struct tracker_decay_value *current = NULL, *previous;
    struct list released, discarded;
    HRESULT hr = S_OK;

    if (value)
    {
        if (position)
        {
            if (FAILED(hr = IInspectable_QueryInterface( value, &IID___FIReference_1_Vector3, (void **)&vector ))) return hr;
            hr = __FIReference_1_Vector3_get_Value( vector, &rate );
            __FIReference_1_Vector3_Release( vector );
        }
        else
        {
            if (FAILED(hr = IInspectable_QueryInterface( value, &IID___FIReference_1_FLOAT, (void **)&scalar ))) return hr;
            hr = __FIReference_1_FLOAT_get_Value( scalar, &scalar_rate );
            __FIReference_1_FLOAT_Release( scalar );
        }
        if (FAILED(hr)) return hr;
        if (!isfinite( rate.X ) || !isfinite( rate.Y ) || !isfinite( rate.Z ) ||
                rate.X < 0.0f || rate.X > 1.0f || rate.Y < 0.0f || rate.Y > 1.0f ||
                rate.Z < 0.0f || rate.Z > 1.0f || !isfinite( scalar_rate ) ||
                scalar_rate < 0.0f || scalar_rate > 1.0f) return E_INVALIDARG;
        if (!(current = calloc( 1, sizeof(*current) ))) return E_OUTOFMEMORY;
        current->ref = 1;
        current->value = value;
        IInspectable_AddRef( value );
    }
    list_init( &released );
    list_init( &discarded );
    EnterCriticalSection( &tracker_binding_cs );
    tracker_inertia_advance_locked( impl, GetTickCount64(), &released, &discarded );
    EnterCriticalSection( &impl->animation_cs );
    previous = position ? impl->position_decay_rate : impl->scale_decay_rate;
    if (position)
    {
        impl->position_decay_rate = current;
        impl->position_decay = rate;
    }
    else
    {
        impl->scale_decay_rate = current;
        impl->scale_decay = scalar_rate;
    }
    if (impl->inertia) tracker_inertia_predict_locked( impl );
    LeaveCriticalSection( &impl->animation_cs );
    LeaveCriticalSection( &tracker_binding_cs );
    tracker_decay_release( previous );
    tracker_notifications_free( &discarded );
    tracker_inertia_free_list( &released );
    return S_OK;
}

static HRESULT WINAPI interaction_tracker_get_PositionInertiaDecayRate( IInteractionTracker *iface,
        IInspectable **value )
{
    return tracker_inertia_get_decay( impl_from_IInteractionTracker( iface ), TRUE, value );
}

static HRESULT WINAPI interaction_tracker_put_PositionInertiaDecayRate( IInteractionTracker *iface,
        IInspectable *value )
{
    return tracker_inertia_set_decay( impl_from_IInteractionTracker( iface ), TRUE, value );
}

static HRESULT WINAPI interaction_tracker_get_PositionVelocityInPixelsPerSecond(
        IInteractionTracker *iface, struct Vector3 *value )
{
    struct interaction_tracker *impl = impl_from_IInteractionTracker( iface );

    if (!value) return E_POINTER;
    EnterCriticalSection( &impl->animation_cs );
    *value = impl->position_velocity;
    LeaveCriticalSection( &impl->animation_cs );
    return S_OK;
}

static HRESULT WINAPI interaction_tracker_get_Scale( IInteractionTracker *iface, FLOAT *value )
{
    struct interaction_tracker *impl = impl_from_IInteractionTracker( iface );

    if (!value) return E_POINTER;
    interaction_tracker_clamp_scale( impl );
    EnterCriticalSection( &impl->animation_cs );
    *value = impl->scale;
    LeaveCriticalSection( &impl->animation_cs );
    return S_OK;
}

static HRESULT WINAPI interaction_tracker_get_ScaleInertiaDecayRate( IInteractionTracker *iface,
        IInspectable **value )
{
    return tracker_inertia_get_decay( impl_from_IInteractionTracker( iface ), FALSE, value );
}

static HRESULT WINAPI interaction_tracker_put_ScaleInertiaDecayRate( IInteractionTracker *iface,
        IInspectable *value )
{
    return tracker_inertia_set_decay( impl_from_IInteractionTracker( iface ), FALSE, value );
}

static HRESULT WINAPI interaction_tracker_get_ScaleVelocityInPercentPerSecond( IInteractionTracker *iface, FLOAT *value )
{
    struct interaction_tracker *impl = impl_from_IInteractionTracker( iface );

    if (!value) return E_POINTER;
    EnterCriticalSection( &impl->animation_cs );
    *value = impl->scale_velocity;
    LeaveCriticalSection( &impl->animation_cs );
    return S_OK;
}

static HRESULT WINAPI interaction_tracker_AdjustPositionXIfGreaterThanThreshold(
        IInteractionTracker *iface, FLOAT adjustment, FLOAT position_threshold )
{
    struct interaction_tracker *impl = impl_from_IInteractionTracker( iface );
    EnterCriticalSection( &impl->animation_cs );
    if (impl->position.X > position_threshold) impl->position.X += adjustment;
    ++impl->animation_revision;
    LeaveCriticalSection( &impl->animation_cs );
    return S_OK;
}

static HRESULT WINAPI interaction_tracker_AdjustPositionYIfGreaterThanThreshold(
        IInteractionTracker *iface, FLOAT adjustment, FLOAT position_threshold )
{
    struct interaction_tracker *impl = impl_from_IInteractionTracker( iface );
    EnterCriticalSection( &impl->animation_cs );
    if (impl->position.Y > position_threshold) impl->position.Y += adjustment;
    ++impl->animation_revision;
    LeaveCriticalSection( &impl->animation_cs );
    return S_OK;
}

static HRESULT WINAPI interaction_tracker_ConfigureModifiers( IInteractionTracker *iface,
        IInspectable *modifiers )
{
    TRACE( "iface %p, modifiers %p.\n", iface, modifiers );
    return S_OK;
}

static INT32 interaction_tracker_next_request_id( struct interaction_tracker *impl )
{
    return InterlockedIncrement( &impl->next_request_id );
}

static void interaction_tracker_update_axis( struct interaction_tracker *impl,
        enum InteractionBindingAxisModes axis, FLOAT value, struct list *changed )
{
    struct interaction_tracker_binding *binding;
    struct interaction_tracker *current;
    struct list pending, visited;
    FLOAT *property;

    list_init( &pending );
    list_init( &visited );
    list_add_tail( &pending, &impl->update_entry );
    while (!list_empty( &pending ))
    {
        current = LIST_ENTRY( list_head( &pending ), struct interaction_tracker, update_entry );
        list_remove( &current->update_entry );
        list_add_tail( &visited, &current->update_entry );
        EnterCriticalSection( &current->animation_cs );
        property = axis == InteractionBindingAxisModes_PositionX ? &current->position.X :
                axis == InteractionBindingAxisModes_PositionY ? &current->position.Y : &current->scale;
        if (*property != value) interaction_tracker_mark_changed( current, changed );
        *property = value;
        ++current->animation_revision;
        LeaveCriticalSection( &current->animation_cs );
        LIST_FOR_EACH_ENTRY( binding, &current->bindings, struct interaction_tracker_binding, entry )
            if ((binding->axis_mode & axis) && list_empty( &binding->peer->update_entry ))
                list_add_tail( &pending, &binding->peer->update_entry );
    }
    while (!list_empty( &visited ))
    {
        current = LIST_ENTRY( list_head( &visited ), struct interaction_tracker, update_entry );
        list_remove( &current->update_entry );
        list_init( &current->update_entry );
    }
}

static void interaction_tracker_clamp_scale( struct interaction_tracker *impl )
{
    struct list changed, discarded;
    FLOAT value, previous;

    list_init( &changed );
    list_init( &discarded );
    EnterCriticalSection( &tracker_binding_cs );
    EnterCriticalSection( &impl->animation_cs );
    previous = value = impl->scale;
    if (!impl->inertia) value = min( max( value, impl->min_scale ), impl->max_scale );
    LeaveCriticalSection( &impl->animation_cs );
    if (value != previous)
    {
        interaction_tracker_update_axis( impl, InteractionBindingAxisModes_Scale, value, &changed );
        interaction_tracker_notify_changes( impl, 0, FALSE, &changed, &discarded );
    }
    LeaveCriticalSection( &tracker_binding_cs );
    tracker_notifications_free( &discarded );
}

static double tracker_inertia_rest( FLOAT position, FLOAT velocity, FLOAT decay )
{
    if (!velocity || decay >= 1.0f) return position;
    if (decay <= 0.0f) return velocity > 0 ? INFINITY : -INFINITY;
    return position + velocity / -log1p( -(double)decay );
}

static struct Vector3 tracker_scale_position( struct Vector3 position, struct Vector3 center,
        FLOAT before, FLOAT after )
{
    double ratio, x, y, z;

    if (!before || before == after) return position;
    ratio = (double)after / before;
    x = (double)position.X + center.X;
    y = (double)position.Y + center.Y;
    z = (double)position.Z + center.Z;
    return (struct Vector3){x ? x * ratio - center.X : -center.X,
            y ? y * ratio - center.Y : -center.Y,
            z ? z * ratio - center.Z : -center.Z};
}

static void tracker_inertia_predict_locked( struct interaction_tracker *impl )
{
    impl->natural_position.X = tracker_inertia_rest( impl->position.X, impl->position_velocity.X, impl->position_decay.X );
    impl->natural_position.Y = tracker_inertia_rest( impl->position.Y, impl->position_velocity.Y, impl->position_decay.Y );
    impl->natural_position.Z = tracker_inertia_rest( impl->position.Z, impl->position_velocity.Z, impl->position_decay.Z );
    impl->natural_scale = tracker_inertia_rest( impl->scale, impl->scale_velocity, impl->scale_decay );
    if (impl->scale_velocity)
        impl->natural_position = tracker_scale_position( impl->natural_position, impl->scale_center,
                impl->scale, impl->natural_scale );
}

static void tracker_inertia_release( struct tracker_inertia_operation *operation )
{
    IInteractionTracker *tracker;

    if (!InterlockedDecrement( &operation->ref ))
    {
        tracker = &operation->tracker->IInteractionTracker_iface;
        free( operation );
        IInteractionTracker_Release( tracker );
    }
}

static void tracker_inertia_free_list( struct list *released )
{
    struct tracker_inertia_operation *operation;

    while (!list_empty( released ))
    {
        operation = LIST_ENTRY( list_head( released ), struct tracker_inertia_operation, entry );
        list_remove( &operation->entry );
        tracker_inertia_release( operation );
    }
}

static void tracker_inertia_cancel_locked( struct interaction_tracker *impl, struct list *released )
{
    EnterCriticalSection( &impl->animation_cs );
    if (impl->inertia)
    {
        list_remove( &impl->inertia->entry );
        list_add_tail( released, &impl->inertia->entry );
        impl->inertia = NULL;
    }
    impl->position_velocity = (struct Vector3){0};
    impl->scale_velocity = 0.0f;
    LeaveCriticalSection( &impl->animation_cs );
}

static FLOAT tracker_inertia_axis( FLOAT position, FLOAT *velocity, FLOAT decay,
        FLOAT lower, FLOAT upper, double elapsed )
{
    double value = position, speed = *velocity, lambda, attenuation;

    if (decay >= 1.0f) speed = 0.0;
    else if (decay <= 0.0f) value += speed * elapsed;
    else
    {
        lambda = -log1p( -(double)decay );
        attenuation = exp( -lambda * elapsed );
        value += speed * -expm1( -lambda * elapsed ) / lambda;
        speed *= attenuation;
        if (fabs( speed / lambda ) <= FLT_EPSILON * max( 1.0, fabs( value ) ))
        {
            value += speed / lambda;
            speed = 0.0;
        }
    }
    value = min( max( value, lower ), upper );
    if ((value <= lower && speed < 0.0) || (value >= upper && speed > 0.0)) speed = 0.0;
    *velocity = speed;
    return value;
}

static void tracker_inertia_advance_locked( struct interaction_tracker *impl, ULONGLONG now,
        struct list *released, struct list *discarded )
{
    struct tracker_inertia_operation *operation;
    struct Vector3 position;
    struct list changed;
    INT32 request_id;
    double elapsed;
    FLOAT scale;
    BOOL finished;

    list_init( &changed );
    EnterCriticalSection( &impl->animation_cs );
    operation = impl->inertia;
    if (!operation || now <= operation->last_tick)
    {
        LeaveCriticalSection( &impl->animation_cs );
        return;
    }
    elapsed = (now - operation->last_tick) / 1000.0;
    operation->last_tick = now;
    request_id = operation->request_id;
    position.X = tracker_inertia_axis( impl->position.X, &impl->position_velocity.X,
            impl->position_decay.X, impl->min_position.X, impl->max_position.X, elapsed );
    position.Y = tracker_inertia_axis( impl->position.Y, &impl->position_velocity.Y,
            impl->position_decay.Y, impl->min_position.Y, impl->max_position.Y, elapsed );
    position.Z = tracker_inertia_axis( impl->position.Z, &impl->position_velocity.Z,
            impl->position_decay.Z, impl->min_position.Z, impl->max_position.Z, elapsed );
    scale = tracker_inertia_axis( impl->scale, &impl->scale_velocity, impl->scale_decay,
            impl->min_scale, impl->max_scale, elapsed );
    position = tracker_scale_position( position, impl->scale_center, impl->scale, scale );
    position.X = min( max( position.X, impl->min_position.X ), impl->max_position.X );
    position.Y = min( max( position.Y, impl->min_position.Y ), impl->max_position.Y );
    position.Z = min( max( position.Z, impl->min_position.Z ), impl->max_position.Z );
    if (impl->position.Z != position.Z) interaction_tracker_mark_changed( impl, &changed );
    impl->position.Z = position.Z;
    ++impl->animation_revision;
    LeaveCriticalSection( &impl->animation_cs );
    interaction_tracker_update_axis( impl, InteractionBindingAxisModes_Scale, scale, &changed );
    interaction_tracker_update_axis( impl, InteractionBindingAxisModes_PositionX, position.X, &changed );
    interaction_tracker_update_axis( impl, InteractionBindingAxisModes_PositionY, position.Y, &changed );
    EnterCriticalSection( &impl->animation_cs );
    tracker_inertia_predict_locked( impl );
    finished = !impl->position_velocity.X && !impl->position_velocity.Y && !impl->position_velocity.Z &&
            !impl->scale_velocity;
    if (finished) impl->clamp_position = TRUE;
    LeaveCriticalSection( &impl->animation_cs );
    interaction_tracker_notify_changes( impl, request_id, TRUE, &changed, discarded );
    if (finished)
    {
        tracker_inertia_cancel_locked( impl, released );
        interaction_tracker_queue_state( impl, TRACKER_IDLE, request_id, FALSE, discarded );
    }
}

static void CALLBACK tracker_inertia_worker( PTP_CALLBACK_INSTANCE instance, void *context )
{
    struct tracker_inertia_operation *operation;
    struct list pending, released, discarded;
    HRESULT initialized;
    ULONGLONG now;

    initialized = CoInitializeEx( NULL, COINIT_MULTITHREADED );
    for (;;)
    {
        list_init( &pending );
        list_init( &released );
        list_init( &discarded );
        EnterCriticalSection( &tracker_binding_cs );
        if (list_empty( &tracker_inertia_operations ))
        {
            tracker_inertia_worker_active = FALSE;
            LeaveCriticalSection( &tracker_binding_cs );
            break;
        }
        LIST_FOR_EACH_ENTRY( operation, &tracker_inertia_operations, struct tracker_inertia_operation, entry )
        {
            InterlockedIncrement( &operation->ref );
            list_add_tail( &pending, &operation->pending_entry );
        }
        LeaveCriticalSection( &tracker_binding_cs );

        LIST_FOR_EACH_ENTRY( operation, &pending, struct tracker_inertia_operation, pending_entry )
            interaction_tracker_evaluate_bounds( operation->tracker );

        EnterCriticalSection( &tracker_binding_cs );
        now = GetTickCount64();
        LIST_FOR_EACH_ENTRY( operation, &pending, struct tracker_inertia_operation, pending_entry )
            if (operation->tracker->inertia == operation)
                tracker_inertia_advance_locked( operation->tracker, now, &released, &discarded );
        LeaveCriticalSection( &tracker_binding_cs );
        tracker_notifications_free( &discarded );
        tracker_inertia_free_list( &released );
        while (!list_empty( &pending ))
        {
            operation = LIST_ENTRY( list_head( &pending ), struct tracker_inertia_operation, pending_entry );
            list_remove( &operation->pending_entry );
            tracker_inertia_release( operation );
        }
        Sleep( 16 );
    }
    if (SUCCEEDED(initialized)) CoUninitialize();
}

static HRESULT tracker_inertia_add_velocity( struct interaction_tracker *impl,
        struct Vector3 velocity, FLOAT scale_velocity, const struct Vector3 *center_point, INT32 *request_id )
{
    struct tracker_inertia_operation *operation;
    struct Vector3 combined;
    FLOAT combined_scale;
    struct list released, discarded;
    ULONGLONG now;
    HRESULT hr = S_OK;

    if (!request_id) return E_POINTER;
    if (!isfinite( velocity.X ) || !isfinite( velocity.Y ) || !isfinite( velocity.Z ) ||
            !isfinite( scale_velocity )) return E_INVALIDARG;
    if (center_point && (!isfinite( center_point->X ) || !isfinite( center_point->Y ) ||
            !isfinite( center_point->Z ))) return E_INVALIDARG;
    interaction_tracker_clamp_scale( impl );
    list_init( &released );
    list_init( &discarded );
    EnterCriticalSection( &tracker_binding_cs );
    now = GetTickCount64();
    tracker_inertia_advance_locked( impl, now, &released, &discarded );
    EnterCriticalSection( &impl->animation_cs );
    combined = (struct Vector3){impl->position_velocity.X + velocity.X,
            impl->position_velocity.Y + velocity.Y, impl->position_velocity.Z + velocity.Z};
    combined_scale = impl->scale_velocity + scale_velocity;
    if (impl->notifications_closed) hr = RO_E_CLOSED;
    else if (!isfinite( combined.X ) || !isfinite( combined.Y ) || !isfinite( combined.Z ) ||
            !isfinite( combined_scale )) hr = E_INVALIDARG;
    if (SUCCEEDED(hr) && !impl->inertia)
    {
        if (!(operation = calloc( 1, sizeof(*operation) ))) hr = E_OUTOFMEMORY;
        else
        {
            operation->ref = 1;
            operation->tracker = impl;
            operation->last_tick = now;
            IInteractionTracker_AddRef( &impl->IInteractionTracker_iface );
            impl->inertia = operation;
            list_add_tail( &tracker_inertia_operations, &operation->entry );
        }
    }
    if (SUCCEEDED(hr) && !tracker_inertia_worker_active)
    {
        tracker_inertia_worker_active = TRUE;
        if (!TrySubmitThreadpoolCallback( tracker_inertia_worker, NULL, NULL ))
        {
            tracker_inertia_worker_active = FALSE;
            hr = HRESULT_FROM_WIN32( GetLastError() );
            if (SUCCEEDED(hr)) hr = E_FAIL;
        }
    }
    if (SUCCEEDED(hr))
    {
        impl->position_velocity = combined;
        impl->scale_velocity = combined_scale;
        if (center_point) impl->scale_center = *center_point;
        impl->clamp_position = FALSE;
        *request_id = impl->inertia->request_id = interaction_tracker_next_request_id( impl );
        tracker_inertia_predict_locked( impl );
    }
    LeaveCriticalSection( &impl->animation_cs );
    if (SUCCEEDED(hr)) interaction_tracker_queue_state( impl, TRACKER_INERTIA, *request_id, FALSE, &discarded );
    else if (impl->inertia && !tracker_inertia_worker_active) tracker_inertia_cancel_locked( impl, &released );
    LeaveCriticalSection( &tracker_binding_cs );
    tracker_notifications_free( &discarded );
    tracker_inertia_free_list( &released );
    return hr;
}

static HRESULT interaction_tracker_update_position( struct interaction_tracker *impl,
        struct Vector3 value, BOOL relative, BOOL clamp, INT32 *request_id )
{
    struct list changed, discarded, released;
    BOOL was_active, closed;
    HRESULT hr;

    EnterCriticalSection( &impl->animation_cs );
    closed = impl->notifications_closed;
    LeaveCriticalSection( &impl->animation_cs );
    if (closed) return RO_E_CLOSED;
    if (clamp && FAILED(hr = interaction_tracker_evaluate_bounds( impl ))) return hr;
    list_init( &released );
    list_init( &changed );
    list_init( &discarded );
    EnterCriticalSection( &tracker_binding_cs );
    EnterCriticalSection( &impl->animation_cs );
    if (impl->notifications_closed)
    {
        LeaveCriticalSection( &impl->animation_cs );
        LeaveCriticalSection( &tracker_binding_cs );
        return RO_E_CLOSED;
    }
    LeaveCriticalSection( &impl->animation_cs );
    was_active = impl->inertia != NULL;
    tracker_inertia_cancel_locked( impl, &released );
    *request_id = interaction_tracker_next_request_id( impl );
    EnterCriticalSection( &impl->animation_cs );
    if (relative)
    {
        value.X += impl->position.X;
        value.Y += impl->position.Y;
        value.Z += impl->position.Z;
    }
    impl->clamp_position = clamp;
    if (clamp)
    {
        value.X = min( max( value.X, impl->min_position.X ), impl->max_position.X );
        value.Y = min( max( value.Y, impl->min_position.Y ), impl->max_position.Y );
        value.Z = min( max( value.Z, impl->min_position.Z ), impl->max_position.Z );
    }
    if (impl->position.Z != value.Z) interaction_tracker_mark_changed( impl, &changed );
    impl->position.Z = value.Z;
    ++impl->animation_revision;
    LeaveCriticalSection( &impl->animation_cs );
    interaction_tracker_update_axis( impl, InteractionBindingAxisModes_PositionX, value.X, &changed );
    interaction_tracker_update_axis( impl, InteractionBindingAxisModes_PositionY, value.Y, &changed );
    interaction_tracker_notify_changes( impl, *request_id, FALSE, &changed, &discarded );
    if (was_active) interaction_tracker_queue_state( impl, TRACKER_IDLE, *request_id, FALSE, &discarded );
    LeaveCriticalSection( &tracker_binding_cs );
    tracker_notifications_free( &discarded );
    tracker_inertia_free_list( &released );
    return S_OK;
}

static HRESULT WINAPI interaction_tracker_TryUpdatePosition( IInteractionTracker *iface,
        struct Vector3 value, INT32 *request_id )
{
    struct interaction_tracker *impl = impl_from_IInteractionTracker( iface );

    if (!request_id) return E_POINTER;
    return interaction_tracker_update_position( impl, value, FALSE, TRUE, request_id );
}

static HRESULT WINAPI interaction_tracker_TryUpdatePositionBy( IInteractionTracker *iface,
        struct Vector3 amount, INT32 *request_id )
{
    struct interaction_tracker *impl = impl_from_IInteractionTracker( iface );

    if (!request_id) return E_POINTER;
    return interaction_tracker_update_position( impl, amount, TRUE, TRUE, request_id );
}

static HRESULT WINAPI interaction_tracker_TryUpdatePositionWithAnimation( IInteractionTracker *iface,
        IInspectable *animation, INT32 *request_id )
{
    struct interaction_tracker *impl = impl_from_IInteractionTracker( iface );
    HRESULT hr = S_OK;

    if (!request_id) return E_POINTER;
    EnterCriticalSection( &impl->animation_cs );
    if (impl->notifications_closed) hr = RO_E_CLOSED;
    else *request_id = interaction_tracker_next_request_id( impl );
    LeaveCriticalSection( &impl->animation_cs );
    return hr;
}

static HRESULT WINAPI interaction_tracker_TryUpdatePositionWithAdditionalVelocity(
        IInteractionTracker *iface, struct Vector3 velocity, INT32 *request_id )
{
    return tracker_inertia_add_velocity( impl_from_IInteractionTracker( iface ), velocity, 0.0f, NULL, request_id );
}

static HRESULT WINAPI interaction_tracker_TryUpdateScale( IInteractionTracker *iface, FLOAT value,
        struct Vector3 center_point, INT32 *request_id )
{
    struct interaction_tracker *impl = impl_from_IInteractionTracker( iface );

    struct list changed, discarded, released;
    BOOL was_active;

    if (!request_id) return E_POINTER;
    if (!isfinite( value ) || value <= 0.0f || !isfinite( center_point.X ) ||
            !isfinite( center_point.Y ) || !isfinite( center_point.Z )) return E_INVALIDARG;
    list_init( &released );
    list_init( &changed );
    list_init( &discarded );
    EnterCriticalSection( &tracker_binding_cs );
    EnterCriticalSection( &impl->animation_cs );
    if (impl->notifications_closed)
    {
        LeaveCriticalSection( &impl->animation_cs );
        LeaveCriticalSection( &tracker_binding_cs );
        return RO_E_CLOSED;
    }
    LeaveCriticalSection( &impl->animation_cs );
    was_active = impl->inertia != NULL;
    tracker_inertia_cancel_locked( impl, &released );
    *request_id = interaction_tracker_next_request_id( impl );
    EnterCriticalSection( &impl->animation_cs );
    value = min( max( value, impl->min_scale ), impl->max_scale );
    impl->clamp_position = TRUE;
    LeaveCriticalSection( &impl->animation_cs );
    interaction_tracker_update_axis( impl, InteractionBindingAxisModes_Scale, value, &changed );
    interaction_tracker_notify_changes( impl, *request_id, FALSE, &changed, &discarded );
    if (was_active) interaction_tracker_queue_state( impl, TRACKER_IDLE, *request_id, FALSE, &discarded );
    LeaveCriticalSection( &tracker_binding_cs );
    tracker_notifications_free( &discarded );
    tracker_inertia_free_list( &released );
    return S_OK;
}

static HRESULT WINAPI interaction_tracker_TryUpdateScaleWithAnimation( IInteractionTracker *iface,
        IInspectable *animation, struct Vector3 center_point, INT32 *request_id )
{
    struct interaction_tracker *impl = impl_from_IInteractionTracker( iface );
    HRESULT hr = S_OK;

    if (!request_id) return E_POINTER;
    EnterCriticalSection( &impl->animation_cs );
    if (impl->notifications_closed) hr = RO_E_CLOSED;
    else *request_id = interaction_tracker_next_request_id( impl );
    LeaveCriticalSection( &impl->animation_cs );
    return hr;
}

static HRESULT WINAPI interaction_tracker_TryUpdateScaleWithAdditionalVelocity(
        IInteractionTracker *iface, FLOAT velocity, struct Vector3 center_point, INT32 *request_id )
{
    return tracker_inertia_add_velocity( impl_from_IInteractionTracker( iface ), (struct Vector3){0},
            velocity, &center_point, request_id );
}

static HRESULT WINAPI interaction_tracker5_QueryInterface( IInteractionTracker5 *iface,
        REFIID iid, void **out )
{
    struct interaction_tracker *impl = impl_from_IInteractionTracker5( iface );
    return IInteractionTracker_QueryInterface( &impl->IInteractionTracker_iface, iid, out );
}

static ULONG WINAPI interaction_tracker5_AddRef( IInteractionTracker5 *iface )
{
    struct interaction_tracker *impl = impl_from_IInteractionTracker5( iface );
    return IInteractionTracker_AddRef( &impl->IInteractionTracker_iface );
}

static ULONG WINAPI interaction_tracker5_Release( IInteractionTracker5 *iface )
{
    struct interaction_tracker *impl = impl_from_IInteractionTracker5( iface );
    return IInteractionTracker_Release( &impl->IInteractionTracker_iface );
}

static HRESULT WINAPI interaction_tracker5_GetIids( IInteractionTracker5 *iface,
        ULONG *count, IID **iids )
{
    struct interaction_tracker *impl = impl_from_IInteractionTracker5( iface );
    return IInteractionTracker_GetIids( &impl->IInteractionTracker_iface, count, iids );
}

static HRESULT WINAPI interaction_tracker5_GetRuntimeClassName( IInteractionTracker5 *iface,
        HSTRING *name )
{
    struct interaction_tracker *impl = impl_from_IInteractionTracker5( iface );
    return IInteractionTracker_GetRuntimeClassName( &impl->IInteractionTracker_iface, name );
}

static HRESULT WINAPI interaction_tracker5_GetTrustLevel( IInteractionTracker5 *iface,
        TrustLevel *level )
{
    struct interaction_tracker *impl = impl_from_IInteractionTracker5( iface );
    return IInteractionTracker_GetTrustLevel( &impl->IInteractionTracker_iface, level );
}

static HRESULT WINAPI interaction_tracker5_TryUpdatePositionWithOption( IInteractionTracker5 *iface,
        struct Vector3 value, enum InteractionTrackerClampingOption option,
        enum InteractionTrackerPositionUpdateOption position_update_option, INT32 *request_id )
{
    struct interaction_tracker *impl = impl_from_IInteractionTracker5( iface );

    if (!request_id) return E_POINTER;
    if (option != InteractionTrackerClampingOption_Auto && option != InteractionTrackerClampingOption_Disabled)
        return E_INVALIDARG;
    if (position_update_option != InteractionTrackerPositionUpdateOption_Default &&
            position_update_option != InteractionTrackerPositionUpdateOption_AllowActiveCustomScaleAnimation)
        return E_INVALIDARG;
    return interaction_tracker_update_position( impl, value, FALSE,
            option == InteractionTrackerClampingOption_Auto, request_id );
}

static HRESULT WINAPI interaction_tracker4_QueryInterface( IInteractionTracker4 *iface,
        REFIID iid, void **out )
{
    struct interaction_tracker *impl = impl_from_IInteractionTracker4( iface );
    return IInteractionTracker_QueryInterface( &impl->IInteractionTracker_iface, iid, out );
}

static ULONG WINAPI interaction_tracker4_AddRef( IInteractionTracker4 *iface )
{
    struct interaction_tracker *impl = impl_from_IInteractionTracker4( iface );
    return IInteractionTracker_AddRef( &impl->IInteractionTracker_iface );
}

static ULONG WINAPI interaction_tracker4_Release( IInteractionTracker4 *iface )
{
    struct interaction_tracker *impl = impl_from_IInteractionTracker4( iface );
    return IInteractionTracker_Release( &impl->IInteractionTracker_iface );
}

static HRESULT WINAPI interaction_tracker4_GetIids( IInteractionTracker4 *iface,
        ULONG *count, IID **iids )
{
    struct interaction_tracker *impl = impl_from_IInteractionTracker4( iface );
    return IInteractionTracker_GetIids( &impl->IInteractionTracker_iface, count, iids );
}

static HRESULT WINAPI interaction_tracker4_GetRuntimeClassName( IInteractionTracker4 *iface,
        HSTRING *name )
{
    struct interaction_tracker *impl = impl_from_IInteractionTracker4( iface );
    return IInteractionTracker_GetRuntimeClassName( &impl->IInteractionTracker_iface, name );
}

static HRESULT WINAPI interaction_tracker4_GetTrustLevel( IInteractionTracker4 *iface,
        TrustLevel *level )
{
    struct interaction_tracker *impl = impl_from_IInteractionTracker4( iface );
    return IInteractionTracker_GetTrustLevel( &impl->IInteractionTracker_iface, level );
}

static HRESULT WINAPI interaction_tracker4_TryUpdatePositionWithOption( IInteractionTracker4 *iface,
        struct Vector3 value, enum InteractionTrackerClampingOption option, INT32 *request_id )
{
    struct interaction_tracker *impl = impl_from_IInteractionTracker4( iface );

    return interaction_tracker5_TryUpdatePositionWithOption( &impl->IInteractionTracker5_iface,
            value, option, InteractionTrackerPositionUpdateOption_Default, request_id );
}

static HRESULT WINAPI interaction_tracker4_TryUpdatePositionByWithOption( IInteractionTracker4 *iface,
        struct Vector3 amount, enum InteractionTrackerClampingOption option, INT32 *request_id )
{
    struct interaction_tracker *impl = impl_from_IInteractionTracker4( iface );

    if (!request_id) return E_POINTER;
    if (option != InteractionTrackerClampingOption_Auto && option != InteractionTrackerClampingOption_Disabled)
        return E_INVALIDARG;
    return interaction_tracker_update_position( impl, amount, TRUE,
            option == InteractionTrackerClampingOption_Auto, request_id );
}

static HRESULT WINAPI interaction_tracker4_get_IsInertiaFromImpulse( IInteractionTracker4 *iface,
        boolean *value )
{
    struct interaction_tracker *impl = impl_from_IInteractionTracker4( iface );

    if (!value) return E_POINTER;
    EnterCriticalSection( &impl->animation_cs );
    *value = impl->inertia != NULL;
    LeaveCriticalSection( &impl->animation_cs );
    return S_OK;
}

static const IInteractionTracker4Vtbl interaction_tracker4_vtbl =
{
    interaction_tracker4_QueryInterface,
    interaction_tracker4_AddRef,
    interaction_tracker4_Release,
    interaction_tracker4_GetIids,
    interaction_tracker4_GetRuntimeClassName,
    interaction_tracker4_GetTrustLevel,
    interaction_tracker4_TryUpdatePositionWithOption,
    interaction_tracker4_TryUpdatePositionByWithOption,
    interaction_tracker4_get_IsInertiaFromImpulse,
};

static const IInteractionTracker5Vtbl interaction_tracker5_vtbl =
{
    interaction_tracker5_QueryInterface,
    interaction_tracker5_AddRef,
    interaction_tracker5_Release,
    interaction_tracker5_GetIids,
    interaction_tracker5_GetRuntimeClassName,
    interaction_tracker5_GetTrustLevel,
    interaction_tracker5_TryUpdatePositionWithOption,
};

static HRESULT WINAPI tracker_composition_object_QueryInterface( ICompositionObject *iface,
        REFIID iid, void **out )
{
    struct interaction_tracker *impl = tracker_from_ICompositionObject( iface );
    return IInteractionTracker_QueryInterface( &impl->IInteractionTracker_iface, iid, out );
}

static ULONG WINAPI tracker_composition_object_AddRef( ICompositionObject *iface )
{
    struct interaction_tracker *impl = tracker_from_ICompositionObject( iface );
    return IInteractionTracker_AddRef( &impl->IInteractionTracker_iface );
}

static ULONG WINAPI tracker_composition_object_Release( ICompositionObject *iface )
{
    struct interaction_tracker *impl = tracker_from_ICompositionObject( iface );
    return IInteractionTracker_Release( &impl->IInteractionTracker_iface );
}

static HRESULT WINAPI tracker_composition_object_GetIids( ICompositionObject *iface,
        ULONG *count, IID **iids )
{
    struct interaction_tracker *impl = tracker_from_ICompositionObject( iface );
    return IInteractionTracker_GetIids( &impl->IInteractionTracker_iface, count, iids );
}

static HRESULT WINAPI tracker_composition_object_GetRuntimeClassName( ICompositionObject *iface,
        HSTRING *name )
{
    struct interaction_tracker *impl = tracker_from_ICompositionObject( iface );
    return IInteractionTracker_GetRuntimeClassName( &impl->IInteractionTracker_iface, name );
}

static HRESULT WINAPI tracker_composition_object_GetTrustLevel( ICompositionObject *iface,
        TrustLevel *level )
{
    struct interaction_tracker *impl = tracker_from_ICompositionObject( iface );
    return IInteractionTracker_GetTrustLevel( &impl->IInteractionTracker_iface, level );
}

static HRESULT WINAPI tracker_composition_object_get_Compositor( ICompositionObject *iface,
        ICompositor **value )
{
    struct interaction_tracker *impl = tracker_from_ICompositionObject( iface );

    if (!value) return E_POINTER;
    ICompositor_AddRef( impl->compositor );
    *value = impl->compositor;
    return S_OK;
}

static HRESULT WINAPI tracker_composition_object_get_Dispatcher( ICompositionObject *iface,
        ICoreDispatcher **value )
{
    if (!value) return E_POINTER;
    *value = NULL;
    return S_OK;
}

static HRESULT WINAPI tracker_composition_object_get_Properties( ICompositionObject *iface,
        ICompositionPropertySet **value )
{
    struct interaction_tracker *impl = tracker_from_ICompositionObject( iface );

    if (!value) return E_POINTER;
    ICompositionPropertySet_AddRef( impl->properties );
    *value = impl->properties;
    return S_OK;
}

static HRESULT WINAPI tracker_composition_object_StartAnimation( ICompositionObject *iface,
        HSTRING name, ICompositionAnimation *animation )
{
    struct interaction_tracker *impl = tracker_from_ICompositionObject( iface );
    IExpressionAnimation *expression, *copy, *previous;
    UINT index;
    HRESULT hr;

    TRACE( "iface %p, name %s, animation %p.\n", iface, debugstr_hstring( name ), animation );
    if (!name || !animation) return E_INVALIDARG;
    EnterCriticalSection( &impl->animation_cs );
    hr = impl->notifications_closed ? RO_E_CLOSED : S_OK;
    LeaveCriticalSection( &impl->animation_cs );
    if (FAILED(hr)) return hr;
    index = tracker_component_index( name );
    if (index == ARRAY_SIZE(tracker_component_names)) return E_NOTIMPL;
    if (FAILED(hr = ICompositionAnimation_QueryInterface( animation, &IID_IExpressionAnimation, (void **)&expression ))) return hr;
    hr = expression_animation_clone( expression, &copy );
    IExpressionAnimation_Release( expression );
    if (FAILED(hr)) return hr;
    EnterCriticalSection( &impl->animation_cs );
    if (impl->notifications_closed)
    {
        LeaveCriticalSection( &impl->animation_cs );
        IExpressionAnimation_Release( copy );
        return RO_E_CLOSED;
    }
    previous = impl->expressions[index];
    impl->expressions[index] = copy;
    ++impl->animation_revision;
    LeaveCriticalSection( &impl->animation_cs );
    if (previous) IExpressionAnimation_Release( previous );
    return S_OK;
}

static HRESULT WINAPI tracker_composition_object_StopAnimation( ICompositionObject *iface, HSTRING name )
{
    struct interaction_tracker *impl = tracker_from_ICompositionObject( iface );
    IExpressionAnimation *previous;
    UINT index;

    TRACE( "iface %p, name %s.\n", iface, debugstr_hstring( name ) );
    if (!name) return E_INVALIDARG;
    index = tracker_component_index( name );
    if (index == ARRAY_SIZE(tracker_component_names)) return E_NOTIMPL;
    EnterCriticalSection( &impl->animation_cs );
    if (impl->notifications_closed)
    {
        LeaveCriticalSection( &impl->animation_cs );
        return RO_E_CLOSED;
    }
    previous = impl->expressions[index];
    impl->expressions[index] = NULL;
    ++impl->animation_revision;
    LeaveCriticalSection( &impl->animation_cs );
    if (previous) IExpressionAnimation_Release( previous );
    return S_OK;
}

static const ICompositionObjectVtbl tracker_composition_object_vtbl =
{
    tracker_composition_object_QueryInterface,
    tracker_composition_object_AddRef,
    tracker_composition_object_Release,
    tracker_composition_object_GetIids,
    tracker_composition_object_GetRuntimeClassName,
    tracker_composition_object_GetTrustLevel,
    tracker_composition_object_get_Compositor,
    tracker_composition_object_get_Dispatcher,
    tracker_composition_object_get_Properties,
    tracker_composition_object_StartAnimation,
    tracker_composition_object_StopAnimation,
};

static const IInteractionTrackerVtbl interaction_tracker_vtbl =
{
    interaction_tracker_QueryInterface,
    interaction_tracker_AddRef,
    interaction_tracker_Release,
    interaction_tracker_GetIids,
    interaction_tracker_GetRuntimeClassName,
    interaction_tracker_GetTrustLevel,
    interaction_tracker_get_InteractionSources,
    interaction_tracker_get_IsPositionRoundingSuggested,
    interaction_tracker_get_MaxPosition,
    interaction_tracker_put_MaxPosition,
    interaction_tracker_get_MaxScale,
    interaction_tracker_put_MaxScale,
    interaction_tracker_get_MinPosition,
    interaction_tracker_put_MinPosition,
    interaction_tracker_get_MinScale,
    interaction_tracker_put_MinScale,
    interaction_tracker_get_NaturalRestingPosition,
    interaction_tracker_get_NaturalRestingScale,
    interaction_tracker_get_Owner,
    interaction_tracker_get_Position,
    interaction_tracker_get_PositionInertiaDecayRate,
    interaction_tracker_put_PositionInertiaDecayRate,
    interaction_tracker_get_PositionVelocityInPixelsPerSecond,
    interaction_tracker_get_Scale,
    interaction_tracker_get_ScaleInertiaDecayRate,
    interaction_tracker_put_ScaleInertiaDecayRate,
    interaction_tracker_get_ScaleVelocityInPercentPerSecond,
    interaction_tracker_AdjustPositionXIfGreaterThanThreshold,
    interaction_tracker_AdjustPositionYIfGreaterThanThreshold,
    interaction_tracker_ConfigureModifiers,
    interaction_tracker_ConfigureModifiers,
    interaction_tracker_ConfigureModifiers,
    interaction_tracker_TryUpdatePosition,
    interaction_tracker_TryUpdatePositionBy,
    interaction_tracker_TryUpdatePositionWithAnimation,
    interaction_tracker_TryUpdatePositionWithAdditionalVelocity,
    interaction_tracker_TryUpdateScale,
    interaction_tracker_TryUpdateScaleWithAnimation,
    interaction_tracker_TryUpdateScaleWithAdditionalVelocity,
};

static HRESULT WINAPI tracker_closable_QueryInterface( IClosable *iface, REFIID iid, void **out )
{
    struct interaction_tracker *impl = tracker_from_IClosable( iface );
    return IInteractionTracker_QueryInterface( &impl->IInteractionTracker_iface, iid, out );
}

static ULONG WINAPI tracker_closable_AddRef( IClosable *iface )
{
    struct interaction_tracker *impl = tracker_from_IClosable( iface );
    return IInteractionTracker_AddRef( &impl->IInteractionTracker_iface );
}

static ULONG WINAPI tracker_closable_Release( IClosable *iface )
{
    struct interaction_tracker *impl = tracker_from_IClosable( iface );
    return IInteractionTracker_Release( &impl->IInteractionTracker_iface );
}

static HRESULT WINAPI tracker_closable_GetIids( IClosable *iface, ULONG *count, IID **iids )
{
    struct interaction_tracker *impl = tracker_from_IClosable( iface );
    return IInteractionTracker_GetIids( &impl->IInteractionTracker_iface, count, iids );
}

static HRESULT WINAPI tracker_closable_GetRuntimeClassName( IClosable *iface, HSTRING *name )
{
    struct interaction_tracker *impl = tracker_from_IClosable( iface );
    return IInteractionTracker_GetRuntimeClassName( &impl->IInteractionTracker_iface, name );
}

static HRESULT WINAPI tracker_closable_GetTrustLevel( IClosable *iface, TrustLevel *level )
{
    struct interaction_tracker *impl = tracker_from_IClosable( iface );
    return IInteractionTracker_GetTrustLevel( &impl->IInteractionTracker_iface, level );
}

static HRESULT WINAPI tracker_closable_Close( IClosable *iface )
{
    struct interaction_tracker *impl = tracker_from_IClosable( iface );
    IAgileReference *owner;
    CO_MTA_USAGE_COOKIE cookie;
    struct list released;

    list_init( &released );
    TRACE( "iface %p.\n", iface );
    EnterCriticalSection( &impl->animation_cs );
    impl->notifications_closed = TRUE;
    owner = impl->owner;
    impl->owner = NULL;
    cookie = impl->owner_mta_cookie;
    impl->owner_mta_cookie = NULL;
    LeaveCriticalSection( &impl->animation_cs );
    EnterCriticalSection( &tracker_binding_cs );
    tracker_inertia_cancel_locked( impl, &released );
    LeaveCriticalSection( &tracker_binding_cs );
    tracker_inertia_free_list( &released );
    interaction_tracker_cancel_notifications( impl );
    interaction_tracker_clear_animations( impl );
    interaction_tracker_clear_bindings( impl );
    ICompositionInteractionSourceCollection_RemoveAll( impl->sources );
    if (owner) IAgileReference_Release( owner );
    if (cookie) CoDecrementMTAUsage( cookie );
    return S_OK;
}

static const IClosableVtbl tracker_closable_vtbl =
{
    tracker_closable_QueryInterface,
    tracker_closable_AddRef,
    tracker_closable_Release,
    tracker_closable_GetIids,
    tracker_closable_GetRuntimeClassName,
    tracker_closable_GetTrustLevel,
    tracker_closable_Close,
};

static HRESULT interaction_tracker_create( ICompositor *compositor, IInteractionTrackerOwner *owner,
        IInteractionTracker **result )
{
    struct interaction_tracker *impl;
    HRESULT hr;

    if (!compositor || !result) return E_INVALIDARG;
    *result = NULL;
    if (!(impl = calloc( 1, sizeof(*impl) ))) return E_OUTOFMEMORY;
    InitializeCriticalSection( &impl->animation_cs );
    impl->IInteractionTracker_iface.lpVtbl = &interaction_tracker_vtbl;
    impl->IInteractionTracker4_iface.lpVtbl = &interaction_tracker4_vtbl;
    impl->IInteractionTracker5_iface.lpVtbl = &interaction_tracker5_vtbl;
    impl->ICompositionObject_iface.lpVtbl = &tracker_composition_object_vtbl;
    impl->IClosable_iface.lpVtbl = &tracker_closable_vtbl;
    impl->compositor = compositor;
    list_init( &impl->bindings );
    list_init( &impl->update_entry );
    list_init( &impl->changed_entry );
    impl->max_scale = 1.0f;
    impl->min_scale = 1.0f;
    impl->scale = 1.0f;
    impl->clamp_position = TRUE;
    impl->position_decay = (struct Vector3){0.95f,0.95f,0.95f};
    impl->scale_decay = 0.985f;
    impl->ref = 1;
    ICompositor_AddRef( compositor );
    if (owner)
    {
        if (FAILED(hr = RoGetAgileReference( AGILEREFERENCE_DEFAULT, &IID_IInteractionTrackerOwner,
                (IUnknown *)owner, &impl->owner )) ||
            FAILED(hr = CoIncrementMTAUsage( &impl->owner_mta_cookie )))
        {
            IInteractionTracker_Release( &impl->IInteractionTracker_iface );
            return hr;
        }
    }
    if (FAILED(hr = interaction_source_collection_create( &impl->sources )) ||
        FAILED(hr = composition_property_set_create( compositor, &impl->properties )))
    {
        IInteractionTracker_Release( &impl->IInteractionTracker_iface );
        return hr;
    }
    *result = &impl->IInteractionTracker_iface;
    return S_OK;
}

struct interaction_tracker_factory
{
    IActivationFactory IActivationFactory_iface;
    IInteractionTrackerStatics IInteractionTrackerStatics_iface;
    IInteractionTrackerStatics2 IInteractionTrackerStatics2_iface;
    LONG ref;
};

static inline struct interaction_tracker_factory *tracker_factory_from_IActivationFactory(
        IActivationFactory *iface )
{
    return CONTAINING_RECORD( iface, struct interaction_tracker_factory, IActivationFactory_iface );
}

static inline struct interaction_tracker_factory *tracker_factory_from_IInteractionTrackerStatics(
        IInteractionTrackerStatics *iface )
{
    return CONTAINING_RECORD( iface, struct interaction_tracker_factory, IInteractionTrackerStatics_iface );
}

static inline struct interaction_tracker_factory *tracker_factory_from_IInteractionTrackerStatics2(
        IInteractionTrackerStatics2 *iface )
{
    return CONTAINING_RECORD( iface, struct interaction_tracker_factory, IInteractionTrackerStatics2_iface );
}

static HRESULT WINAPI interaction_tracker_factory_QueryInterface( IActivationFactory *iface,
        REFIID iid, void **out )
{
    struct interaction_tracker_factory *impl = tracker_factory_from_IActivationFactory( iface );

    FIXME( "iface %p, iid %s, out %p.\n", iface, debugstr_guid( iid ), out );
    if (!out) return E_POINTER;
    if (IsEqualGUID( iid, &IID_IUnknown ) || IsEqualGUID( iid, &IID_IInspectable ) ||
        IsEqualGUID( iid, &IID_IAgileObject ) || IsEqualGUID( iid, &IID_IActivationFactory ))
        *out = &impl->IActivationFactory_iface;
    else if (IsEqualGUID( iid, &IID_IInteractionTrackerStatics ))
        *out = &impl->IInteractionTrackerStatics_iface;
    else if (IsEqualGUID( iid, &IID_IInteractionTrackerStatics2 ))
        *out = &impl->IInteractionTrackerStatics2_iface;
    else
    {
        *out = NULL;
        return E_NOINTERFACE;
    }
    IUnknown_AddRef( (IUnknown *)*out );
    return S_OK;
}

static ULONG WINAPI interaction_tracker_factory_AddRef( IActivationFactory *iface )
{
    return InterlockedIncrement( &tracker_factory_from_IActivationFactory( iface )->ref );
}

static ULONG WINAPI interaction_tracker_factory_Release( IActivationFactory *iface )
{
    return InterlockedDecrement( &tracker_factory_from_IActivationFactory( iface )->ref );
}

static HRESULT WINAPI interaction_tracker_factory_GetIids( IActivationFactory *iface,
        ULONG *count, IID **iids )
{
    return E_NOTIMPL;
}

static HRESULT WINAPI interaction_tracker_factory_GetRuntimeClassName(
        IActivationFactory *iface, HSTRING *name )
{
    if (!name) return E_POINTER;
    return WindowsCreateString( RuntimeClass_Windows_UI_Composition_Interactions_InteractionTracker,
                                ARRAY_SIZE(RuntimeClass_Windows_UI_Composition_Interactions_InteractionTracker) - 1,
                                name );
}

static HRESULT WINAPI interaction_tracker_factory_GetTrustLevel( IActivationFactory *iface,
        TrustLevel *level )
{
    if (!level) return E_POINTER;
    *level = BaseTrust;
    return S_OK;
}

static HRESULT WINAPI interaction_tracker_factory_ActivateInstance( IActivationFactory *iface,
        IInspectable **instance )
{
    if (!instance) return E_POINTER;
    *instance = NULL;
    return E_NOTIMPL;
}

static const IActivationFactoryVtbl interaction_tracker_factory_vtbl =
{
    interaction_tracker_factory_QueryInterface,
    interaction_tracker_factory_AddRef,
    interaction_tracker_factory_Release,
    interaction_tracker_factory_GetIids,
    interaction_tracker_factory_GetRuntimeClassName,
    interaction_tracker_factory_GetTrustLevel,
    interaction_tracker_factory_ActivateInstance,
};

static HRESULT WINAPI interaction_tracker_statics_QueryInterface( IInteractionTrackerStatics *iface,
        REFIID iid, void **out )
{
    struct interaction_tracker_factory *impl = tracker_factory_from_IInteractionTrackerStatics( iface );
    return IActivationFactory_QueryInterface( &impl->IActivationFactory_iface, iid, out );
}

static ULONG WINAPI interaction_tracker_statics_AddRef( IInteractionTrackerStatics *iface )
{
    struct interaction_tracker_factory *impl = tracker_factory_from_IInteractionTrackerStatics( iface );
    return IActivationFactory_AddRef( &impl->IActivationFactory_iface );
}

static ULONG WINAPI interaction_tracker_statics_Release( IInteractionTrackerStatics *iface )
{
    struct interaction_tracker_factory *impl = tracker_factory_from_IInteractionTrackerStatics( iface );
    return IActivationFactory_Release( &impl->IActivationFactory_iface );
}

static HRESULT WINAPI interaction_tracker_statics_GetIids( IInteractionTrackerStatics *iface,
        ULONG *count, IID **iids )
{
    struct interaction_tracker_factory *impl = tracker_factory_from_IInteractionTrackerStatics( iface );
    return IActivationFactory_GetIids( &impl->IActivationFactory_iface, count, iids );
}

static HRESULT WINAPI interaction_tracker_statics_GetRuntimeClassName( IInteractionTrackerStatics *iface,
        HSTRING *name )
{
    struct interaction_tracker_factory *impl = tracker_factory_from_IInteractionTrackerStatics( iface );
    return IActivationFactory_GetRuntimeClassName( &impl->IActivationFactory_iface, name );
}

static HRESULT WINAPI interaction_tracker_statics_GetTrustLevel( IInteractionTrackerStatics *iface,
        TrustLevel *level )
{
    struct interaction_tracker_factory *impl = tracker_factory_from_IInteractionTrackerStatics( iface );
    return IActivationFactory_GetTrustLevel( &impl->IActivationFactory_iface, level );
}

static HRESULT WINAPI interaction_tracker_statics_Create( IInteractionTrackerStatics *iface,
        ICompositor *compositor, IInteractionTracker **result )
{
    TRACE( "iface %p, compositor %p, result %p.\n", iface, compositor, result );
    return interaction_tracker_create( compositor, NULL, result );
}

static HRESULT WINAPI interaction_tracker_statics_CreateWithOwner( IInteractionTrackerStatics *iface,
        ICompositor *compositor, IInteractionTrackerOwner *owner, IInteractionTracker **result )
{
    TRACE( "iface %p, compositor %p, owner %p, result %p.\n", iface, compositor, owner, result );
    return interaction_tracker_create( compositor, owner, result );
}

static const IInteractionTrackerStaticsVtbl interaction_tracker_statics_vtbl =
{
    interaction_tracker_statics_QueryInterface,
    interaction_tracker_statics_AddRef,
    interaction_tracker_statics_Release,
    interaction_tracker_statics_GetIids,
    interaction_tracker_statics_GetRuntimeClassName,
    interaction_tracker_statics_GetTrustLevel,
    interaction_tracker_statics_Create,
    interaction_tracker_statics_CreateWithOwner,
};

static HRESULT WINAPI interaction_tracker_statics2_QueryInterface( IInteractionTrackerStatics2 *iface,
        REFIID iid, void **out )
{
    struct interaction_tracker_factory *impl = tracker_factory_from_IInteractionTrackerStatics2( iface );
    return IActivationFactory_QueryInterface( &impl->IActivationFactory_iface, iid, out );
}

static ULONG WINAPI interaction_tracker_statics2_AddRef( IInteractionTrackerStatics2 *iface )
{
    struct interaction_tracker_factory *impl = tracker_factory_from_IInteractionTrackerStatics2( iface );
    return IActivationFactory_AddRef( &impl->IActivationFactory_iface );
}

static ULONG WINAPI interaction_tracker_statics2_Release( IInteractionTrackerStatics2 *iface )
{
    struct interaction_tracker_factory *impl = tracker_factory_from_IInteractionTrackerStatics2( iface );
    return IActivationFactory_Release( &impl->IActivationFactory_iface );
}

static HRESULT WINAPI interaction_tracker_statics2_GetIids( IInteractionTrackerStatics2 *iface,
        ULONG *count, IID **iids )
{
    struct interaction_tracker_factory *impl = tracker_factory_from_IInteractionTrackerStatics2( iface );
    return IActivationFactory_GetIids( &impl->IActivationFactory_iface, count, iids );
}

static HRESULT WINAPI interaction_tracker_statics2_GetRuntimeClassName( IInteractionTrackerStatics2 *iface,
        HSTRING *name )
{
    struct interaction_tracker_factory *impl = tracker_factory_from_IInteractionTrackerStatics2( iface );
    return IActivationFactory_GetRuntimeClassName( &impl->IActivationFactory_iface, name );
}

static HRESULT WINAPI interaction_tracker_statics2_GetTrustLevel( IInteractionTrackerStatics2 *iface,
        TrustLevel *level )
{
    struct interaction_tracker_factory *impl = tracker_factory_from_IInteractionTrackerStatics2( iface );
    return IActivationFactory_GetTrustLevel( &impl->IActivationFactory_iface, level );
}

static HRESULT WINAPI interaction_tracker_statics2_SetBindingMode( IInteractionTrackerStatics2 *iface,
        IInteractionTracker *bound_tracker1, IInteractionTracker *bound_tracker2,
        enum InteractionBindingAxisModes axis_mode )
{
    struct interaction_tracker *tracker1, *tracker2;
    struct interaction_tracker_binding *binding1, *binding2;
    BOOL new_binding1 = FALSE, new_binding2 = FALSE;
    HRESULT hr = S_OK;

    TRACE( "iface %p, bound_tracker1 %p, bound_tracker2 %p, axis_mode %#x.\n",
           iface, bound_tracker1, bound_tracker2, axis_mode );
    if (!bound_tracker1 || !bound_tracker2 || bound_tracker1 == bound_tracker2 ||
        bound_tracker1->lpVtbl != &interaction_tracker_vtbl ||
        bound_tracker2->lpVtbl != &interaction_tracker_vtbl ||
        (axis_mode & ~(InteractionBindingAxisModes_PositionX |
                       InteractionBindingAxisModes_PositionY |
                       InteractionBindingAxisModes_Scale)))
        return E_INVALIDARG;

    tracker1 = impl_from_IInteractionTracker( bound_tracker1 );
    tracker2 = impl_from_IInteractionTracker( bound_tracker2 );
    if (tracker1->compositor != tracker2->compositor) return E_INVALIDARG;

    EnterCriticalSection( &tracker_binding_cs );
    EnterCriticalSection( &tracker1->animation_cs );
    if (tracker1->notifications_closed) hr = RO_E_CLOSED;
    LeaveCriticalSection( &tracker1->animation_cs );
    EnterCriticalSection( &tracker2->animation_cs );
    if (tracker2->notifications_closed) hr = RO_E_CLOSED;
    LeaveCriticalSection( &tracker2->animation_cs );
    if (FAILED(hr)) goto done;
    binding1 = interaction_tracker_find_binding( tracker1, tracker2 );
    binding2 = interaction_tracker_find_binding( tracker2, tracker1 );
    if (axis_mode == InteractionBindingAxisModes_None)
    {
        interaction_tracker_remove_binding( binding1 );
        interaction_tracker_remove_binding( binding2 );
        goto done;
    }

    new_binding1 = !binding1;
    new_binding2 = !binding2;
    if (new_binding1 && !(binding1 = calloc( 1, sizeof(*binding1) )))
    {
        hr = E_OUTOFMEMORY;
        goto done;
    }
    if (new_binding2 && !(binding2 = calloc( 1, sizeof(*binding2) )))
    {
        if (new_binding1) free( binding1 );
        hr = E_OUTOFMEMORY;
        goto done;
    }

    binding1->peer = tracker2;
    binding1->axis_mode = axis_mode;
    binding2->peer = tracker1;
    binding2->axis_mode = axis_mode;
    if (new_binding1) list_add_tail( &tracker1->bindings, &binding1->entry );
    if (new_binding2) list_add_tail( &tracker2->bindings, &binding2->entry );
done:
    LeaveCriticalSection( &tracker_binding_cs );
    return hr;
}

static HRESULT WINAPI interaction_tracker_statics2_GetBindingMode( IInteractionTrackerStatics2 *iface,
        IInteractionTracker *bound_tracker1, IInteractionTracker *bound_tracker2,
        enum InteractionBindingAxisModes *axis_mode )
{
    struct interaction_tracker *tracker1, *tracker2;
    struct interaction_tracker_binding *binding;

    TRACE( "iface %p, bound_tracker1 %p, bound_tracker2 %p, axis_mode %p.\n",
           iface, bound_tracker1, bound_tracker2, axis_mode );
    if (!axis_mode) return E_POINTER;
    *axis_mode = InteractionBindingAxisModes_None;
    if (!bound_tracker1 || !bound_tracker2 || bound_tracker1 == bound_tracker2 ||
        bound_tracker1->lpVtbl != &interaction_tracker_vtbl ||
        bound_tracker2->lpVtbl != &interaction_tracker_vtbl)
        return E_INVALIDARG;

    tracker1 = impl_from_IInteractionTracker( bound_tracker1 );
    tracker2 = impl_from_IInteractionTracker( bound_tracker2 );
    if (tracker1->compositor != tracker2->compositor) return E_INVALIDARG;
    EnterCriticalSection( &tracker_binding_cs );
    if ((binding = interaction_tracker_find_binding( tracker1, tracker2 )))
        *axis_mode = binding->axis_mode;
    LeaveCriticalSection( &tracker_binding_cs );
    return S_OK;
}

static const IInteractionTrackerStatics2Vtbl interaction_tracker_statics2_vtbl =
{
    interaction_tracker_statics2_QueryInterface,
    interaction_tracker_statics2_AddRef,
    interaction_tracker_statics2_Release,
    interaction_tracker_statics2_GetIids,
    interaction_tracker_statics2_GetRuntimeClassName,
    interaction_tracker_statics2_GetTrustLevel,
    interaction_tracker_statics2_SetBindingMode,
    interaction_tracker_statics2_GetBindingMode,
};

static struct interaction_tracker_factory interaction_tracker_factory_impl =
{
    {&interaction_tracker_factory_vtbl},
    {&interaction_tracker_statics_vtbl},
    {&interaction_tracker_statics2_vtbl},
    1,
};

IActivationFactory *interaction_tracker_factory = &interaction_tracker_factory_impl.IActivationFactory_iface;

struct visual_interaction_source
{
    IVisualInteractionSource IVisualInteractionSource_iface;
    IVisualInteractionSource2 IVisualInteractionSource2_iface;
    IVisualInteractionSource3 IVisualInteractionSource3_iface;
    ICompositionInteractionSource ICompositionInteractionSource_iface;
    ICompositionObject ICompositionObject_iface;
    IClosable IClosable_iface;
    IVisual *source;
    ICompositionPropertySet *properties;
    ICompositor *compositor;
    enum InteractionChainingMode x_chaining_mode;
    enum InteractionChainingMode y_chaining_mode;
    enum InteractionChainingMode scale_chaining_mode;
    enum InteractionSourceMode x_source_mode;
    enum InteractionSourceMode y_source_mode;
    enum InteractionSourceMode scale_source_mode;
    enum VisualInteractionSourceRedirectionMode redirection_mode;
    boolean x_rails_enabled;
    boolean y_rails_enabled;
    LONG ref;
};

static inline struct visual_interaction_source *impl_from_IVisualInteractionSource(
        IVisualInteractionSource *iface )
{
    return CONTAINING_RECORD( iface, struct visual_interaction_source, IVisualInteractionSource_iface );
}

static inline struct visual_interaction_source *impl_from_IVisualInteractionSource2(
        IVisualInteractionSource2 *iface )
{
    return CONTAINING_RECORD( iface, struct visual_interaction_source, IVisualInteractionSource2_iface );
}

static inline struct visual_interaction_source *impl_from_IVisualInteractionSource3(
        IVisualInteractionSource3 *iface )
{
    return CONTAINING_RECORD( iface, struct visual_interaction_source, IVisualInteractionSource3_iface );
}

static inline struct visual_interaction_source *impl_from_ICompositionInteractionSource(
        ICompositionInteractionSource *iface )
{
    return CONTAINING_RECORD( iface, struct visual_interaction_source, ICompositionInteractionSource_iface );
}

static inline struct visual_interaction_source *visual_interaction_source_from_ICompositionObject(
        ICompositionObject *iface )
{
    return CONTAINING_RECORD( iface, struct visual_interaction_source, ICompositionObject_iface );
}

static inline struct visual_interaction_source *visual_interaction_source_from_IClosable( IClosable *iface )
{
    return CONTAINING_RECORD( iface, struct visual_interaction_source, IClosable_iface );
}

static HRESULT visual_interaction_source_query_interface( struct visual_interaction_source *impl,
        REFIID iid, void **out )
{
    FIXME( "impl %p, iid %s, out %p.\n", impl, debugstr_guid( iid ), out );
    if (!out) return E_POINTER;
    if (IsEqualGUID( iid, &IID_IUnknown ) || IsEqualGUID( iid, &IID_IInspectable ) ||
        IsEqualGUID( iid, &IID_IAgileObject ) || IsEqualGUID( iid, &IID_IVisualInteractionSource ))
        *out = &impl->IVisualInteractionSource_iface;
    else if (IsEqualGUID( iid, &IID_IVisualInteractionSource2 ))
        *out = &impl->IVisualInteractionSource2_iface;
    else if (IsEqualGUID( iid, &IID_IVisualInteractionSource3 ))
        *out = &impl->IVisualInteractionSource3_iface;
    else if (IsEqualGUID( iid, &IID_ICompositionInteractionSource ))
        *out = &impl->ICompositionInteractionSource_iface;
    else if (IsEqualGUID( iid, &IID_ICompositionObject ))
        *out = &impl->ICompositionObject_iface;
    else if (IsEqualGUID( iid, &IID_IClosable ))
        *out = &impl->IClosable_iface;
    else
    {
        *out = NULL;
        return E_NOINTERFACE;
    }
    IUnknown_AddRef( (IUnknown *)*out );
    return S_OK;
}

static ULONG visual_interaction_source_add_ref( struct visual_interaction_source *impl )
{
    return InterlockedIncrement( &impl->ref );
}

static ULONG visual_interaction_source_release( struct visual_interaction_source *impl )
{
    ULONG ref = InterlockedDecrement( &impl->ref );

    if (!ref)
    {
        IVisual_Release( impl->source );
        ICompositionPropertySet_Release( impl->properties );
        ICompositor_Release( impl->compositor );
        free( impl );
    }
    return ref;
}

static HRESULT visual_interaction_source_get_iids( ULONG *count, IID **iids )
{
    IID *values;

    if (!count || !iids) return E_POINTER;
    if (!(values = CoTaskMemAlloc( 6 * sizeof(*values) ))) return E_OUTOFMEMORY;
    values[0] = IID_IVisualInteractionSource;
    values[1] = IID_IVisualInteractionSource2;
    values[2] = IID_IVisualInteractionSource3;
    values[3] = IID_ICompositionInteractionSource;
    values[4] = IID_ICompositionObject;
    values[5] = IID_IClosable;
    *count = 6;
    *iids = values;
    return S_OK;
}

static HRESULT visual_interaction_source_get_class_name( HSTRING *name )
{
    if (!name) return E_POINTER;
    return WindowsCreateString( RuntimeClass_Windows_UI_Composition_Interactions_VisualInteractionSource,
                                ARRAY_SIZE(RuntimeClass_Windows_UI_Composition_Interactions_VisualInteractionSource) - 1,
                                name );
}

static HRESULT visual_interaction_source_get_trust_level( TrustLevel *level )
{
    if (!level) return E_POINTER;
    *level = BaseTrust;
    return S_OK;
}

static HRESULT WINAPI visual_interaction_source_QueryInterface( IVisualInteractionSource *iface,
        REFIID iid, void **out )
{
    return visual_interaction_source_query_interface( impl_from_IVisualInteractionSource( iface ), iid, out );
}

static ULONG WINAPI visual_interaction_source_AddRef( IVisualInteractionSource *iface )
{
    return visual_interaction_source_add_ref( impl_from_IVisualInteractionSource( iface ) );
}

static ULONG WINAPI visual_interaction_source_Release( IVisualInteractionSource *iface )
{
    return visual_interaction_source_release( impl_from_IVisualInteractionSource( iface ) );
}

static HRESULT WINAPI visual_interaction_source_GetIids( IVisualInteractionSource *iface,
        ULONG *count, IID **iids )
{
    return visual_interaction_source_get_iids( count, iids );
}

static HRESULT WINAPI visual_interaction_source_GetRuntimeClassName( IVisualInteractionSource *iface,
        HSTRING *name )
{
    return visual_interaction_source_get_class_name( name );
}

static HRESULT WINAPI visual_interaction_source_GetTrustLevel( IVisualInteractionSource *iface,
        TrustLevel *level )
{
    return visual_interaction_source_get_trust_level( level );
}

#define VISUAL_INTERACTION_SOURCE_PROPERTY(name, type, field) \
    static HRESULT WINAPI visual_interaction_source_get_##name( IVisualInteractionSource *iface, type *value ) \
    { \
        if (!value) return E_POINTER; \
        *value = impl_from_IVisualInteractionSource( iface )->field; \
        return S_OK; \
    } \
    static HRESULT WINAPI visual_interaction_source_put_##name( IVisualInteractionSource *iface, type value ) \
    { \
        impl_from_IVisualInteractionSource( iface )->field = value; \
        return S_OK; \
    }

VISUAL_INTERACTION_SOURCE_PROPERTY( IsPositionXRailsEnabled, boolean, x_rails_enabled )
VISUAL_INTERACTION_SOURCE_PROPERTY( IsPositionYRailsEnabled, boolean, y_rails_enabled )
VISUAL_INTERACTION_SOURCE_PROPERTY( ManipulationRedirectionMode,
                                    enum VisualInteractionSourceRedirectionMode, redirection_mode )
VISUAL_INTERACTION_SOURCE_PROPERTY( PositionXChainingMode, enum InteractionChainingMode, x_chaining_mode )
VISUAL_INTERACTION_SOURCE_PROPERTY( PositionXSourceMode, enum InteractionSourceMode, x_source_mode )
VISUAL_INTERACTION_SOURCE_PROPERTY( PositionYChainingMode, enum InteractionChainingMode, y_chaining_mode )
VISUAL_INTERACTION_SOURCE_PROPERTY( PositionYSourceMode, enum InteractionSourceMode, y_source_mode )
VISUAL_INTERACTION_SOURCE_PROPERTY( ScaleChainingMode, enum InteractionChainingMode, scale_chaining_mode )
VISUAL_INTERACTION_SOURCE_PROPERTY( ScaleSourceMode, enum InteractionSourceMode, scale_source_mode )

static HRESULT WINAPI visual_interaction_source_get_Source( IVisualInteractionSource *iface, IVisual **value )
{
    struct visual_interaction_source *impl = impl_from_IVisualInteractionSource( iface );

    if (!value) return E_POINTER;
    IVisual_AddRef( impl->source );
    *value = impl->source;
    return S_OK;
}

static HRESULT WINAPI visual_interaction_source_TryRedirectForManipulation(
        IVisualInteractionSource *iface, IPointerPoint *pointer )
{
    TRACE( "iface %p, pointer %p.\n", iface, pointer );
    return S_OK;
}

static const IVisualInteractionSourceVtbl visual_interaction_source_vtbl =
{
    visual_interaction_source_QueryInterface,
    visual_interaction_source_AddRef,
    visual_interaction_source_Release,
    visual_interaction_source_GetIids,
    visual_interaction_source_GetRuntimeClassName,
    visual_interaction_source_GetTrustLevel,
    visual_interaction_source_get_IsPositionXRailsEnabled,
    visual_interaction_source_put_IsPositionXRailsEnabled,
    visual_interaction_source_get_IsPositionYRailsEnabled,
    visual_interaction_source_put_IsPositionYRailsEnabled,
    visual_interaction_source_get_ManipulationRedirectionMode,
    visual_interaction_source_put_ManipulationRedirectionMode,
    visual_interaction_source_get_PositionXChainingMode,
    visual_interaction_source_put_PositionXChainingMode,
    visual_interaction_source_get_PositionXSourceMode,
    visual_interaction_source_put_PositionXSourceMode,
    visual_interaction_source_get_PositionYChainingMode,
    visual_interaction_source_put_PositionYChainingMode,
    visual_interaction_source_get_PositionYSourceMode,
    visual_interaction_source_put_PositionYSourceMode,
    visual_interaction_source_get_ScaleChainingMode,
    visual_interaction_source_put_ScaleChainingMode,
    visual_interaction_source_get_ScaleSourceMode,
    visual_interaction_source_put_ScaleSourceMode,
    visual_interaction_source_get_Source,
    visual_interaction_source_TryRedirectForManipulation,
};

#define VISUAL_INTERACTION_SOURCE_FORWARD(pfx, iface_type, from) \
    static HRESULT WINAPI pfx##_QueryInterface( iface_type *iface, REFIID iid, void **out ) \
    { \
        return visual_interaction_source_query_interface( from( iface ), iid, out ); \
    } \
    static ULONG WINAPI pfx##_AddRef( iface_type *iface ) \
    { \
        return visual_interaction_source_add_ref( from( iface ) ); \
    } \
    static ULONG WINAPI pfx##_Release( iface_type *iface ) \
    { \
        return visual_interaction_source_release( from( iface ) ); \
    } \
    static HRESULT WINAPI pfx##_GetIids( iface_type *iface, ULONG *count, IID **iids ) \
    { \
        return visual_interaction_source_get_iids( count, iids ); \
    } \
    static HRESULT WINAPI pfx##_GetRuntimeClassName( iface_type *iface, HSTRING *name ) \
    { \
        return visual_interaction_source_get_class_name( name ); \
    } \
    static HRESULT WINAPI pfx##_GetTrustLevel( iface_type *iface, TrustLevel *level ) \
    { \
        return visual_interaction_source_get_trust_level( level ); \
    }

VISUAL_INTERACTION_SOURCE_FORWARD( visual_interaction_source2, IVisualInteractionSource2,
                                   impl_from_IVisualInteractionSource2 )
VISUAL_INTERACTION_SOURCE_FORWARD( visual_interaction_source3, IVisualInteractionSource3,
                                   impl_from_IVisualInteractionSource3 )
VISUAL_INTERACTION_SOURCE_FORWARD( composition_interaction_source, ICompositionInteractionSource,
                                   impl_from_ICompositionInteractionSource )
VISUAL_INTERACTION_SOURCE_FORWARD( visual_interaction_source_object, ICompositionObject,
                                   visual_interaction_source_from_ICompositionObject )
VISUAL_INTERACTION_SOURCE_FORWARD( visual_interaction_source_closable, IClosable,
                                   visual_interaction_source_from_IClosable )

static HRESULT WINAPI visual_interaction_source2_get_DeltaPosition( IVisualInteractionSource2 *iface,
        struct Vector3 *value )
{
    if (!value) return E_POINTER;
    memset( value, 0, sizeof(*value) );
    return S_OK;
}

static HRESULT WINAPI visual_interaction_source2_get_DeltaScale( IVisualInteractionSource2 *iface,
        FLOAT *value )
{
    if (!value) return E_POINTER;
    *value = 1.0f;
    return S_OK;
}

static HRESULT WINAPI visual_interaction_source2_get_Position( IVisualInteractionSource2 *iface,
        struct Vector3 *value )
{
    if (!value) return E_POINTER;
    memset( value, 0, sizeof(*value) );
    return S_OK;
}

static HRESULT WINAPI visual_interaction_source2_get_PositionVelocity( IVisualInteractionSource2 *iface,
        struct Vector3 *value )
{
    if (!value) return E_POINTER;
    memset( value, 0, sizeof(*value) );
    return S_OK;
}

static HRESULT WINAPI visual_interaction_source2_get_Scale( IVisualInteractionSource2 *iface, FLOAT *value )
{
    if (!value) return E_POINTER;
    *value = 1.0f;
    return S_OK;
}

static HRESULT WINAPI visual_interaction_source2_get_ScaleVelocity( IVisualInteractionSource2 *iface,
        FLOAT *value )
{
    if (!value) return E_POINTER;
    *value = 0.0f;
    return S_OK;
}

static HRESULT WINAPI visual_interaction_source2_ConfigureModifiers( IVisualInteractionSource2 *iface,
        IInspectable *values )
{
    TRACE( "iface %p, values %p.\n", iface, values );
    return S_OK;
}

static const IVisualInteractionSource2Vtbl visual_interaction_source2_vtbl =
{
    visual_interaction_source2_QueryInterface,
    visual_interaction_source2_AddRef,
    visual_interaction_source2_Release,
    visual_interaction_source2_GetIids,
    visual_interaction_source2_GetRuntimeClassName,
    visual_interaction_source2_GetTrustLevel,
    visual_interaction_source2_get_DeltaPosition,
    visual_interaction_source2_get_DeltaScale,
    visual_interaction_source2_get_Position,
    visual_interaction_source2_get_PositionVelocity,
    visual_interaction_source2_get_Scale,
    visual_interaction_source2_get_ScaleVelocity,
    visual_interaction_source2_ConfigureModifiers,
    visual_interaction_source2_ConfigureModifiers,
    visual_interaction_source2_ConfigureModifiers,
    visual_interaction_source2_ConfigureModifiers,
    visual_interaction_source2_ConfigureModifiers,
};

static HRESULT WINAPI visual_interaction_source3_get_PointerWheelConfig(
        IVisualInteractionSource3 *iface, IInspectable **value )
{
    if (!value) return E_POINTER;
    *value = NULL;
    return S_OK;
}

static const IVisualInteractionSource3Vtbl visual_interaction_source3_vtbl =
{
    visual_interaction_source3_QueryInterface,
    visual_interaction_source3_AddRef,
    visual_interaction_source3_Release,
    visual_interaction_source3_GetIids,
    visual_interaction_source3_GetRuntimeClassName,
    visual_interaction_source3_GetTrustLevel,
    visual_interaction_source3_get_PointerWheelConfig,
};

static const ICompositionInteractionSourceVtbl composition_interaction_source_vtbl =
{
    composition_interaction_source_QueryInterface,
    composition_interaction_source_AddRef,
    composition_interaction_source_Release,
    composition_interaction_source_GetIids,
    composition_interaction_source_GetRuntimeClassName,
    composition_interaction_source_GetTrustLevel,
};

static HRESULT WINAPI visual_interaction_source_object_get_Compositor( ICompositionObject *iface,
        ICompositor **value )
{
    struct visual_interaction_source *impl = visual_interaction_source_from_ICompositionObject( iface );

    if (!value) return E_POINTER;
    ICompositor_AddRef( impl->compositor );
    *value = impl->compositor;
    return S_OK;
}

static HRESULT WINAPI visual_interaction_source_object_get_Dispatcher( ICompositionObject *iface,
        ICoreDispatcher **value )
{
    if (!value) return E_POINTER;
    *value = NULL;
    return S_OK;
}

static HRESULT WINAPI visual_interaction_source_object_get_Properties( ICompositionObject *iface,
        ICompositionPropertySet **value )
{
    struct visual_interaction_source *impl = visual_interaction_source_from_ICompositionObject( iface );

    if (!value) return E_POINTER;
    ICompositionPropertySet_AddRef( impl->properties );
    *value = impl->properties;
    return S_OK;
}

static HRESULT WINAPI visual_interaction_source_object_StartAnimation( ICompositionObject *iface,
        HSTRING name, ICompositionAnimation *animation )
{
    TRACE( "iface %p, name %s, animation %p.\n", iface, debugstr_hstring( name ), animation );
    return S_OK;
}

static HRESULT WINAPI visual_interaction_source_object_StopAnimation( ICompositionObject *iface,
        HSTRING name )
{
    TRACE( "iface %p, name %s.\n", iface, debugstr_hstring( name ) );
    return S_OK;
}

static const ICompositionObjectVtbl visual_interaction_source_object_vtbl =
{
    visual_interaction_source_object_QueryInterface,
    visual_interaction_source_object_AddRef,
    visual_interaction_source_object_Release,
    visual_interaction_source_object_GetIids,
    visual_interaction_source_object_GetRuntimeClassName,
    visual_interaction_source_object_GetTrustLevel,
    visual_interaction_source_object_get_Compositor,
    visual_interaction_source_object_get_Dispatcher,
    visual_interaction_source_object_get_Properties,
    visual_interaction_source_object_StartAnimation,
    visual_interaction_source_object_StopAnimation,
};

static HRESULT WINAPI visual_interaction_source_closable_Close( IClosable *iface )
{
    TRACE( "iface %p.\n", iface );
    return S_OK;
}

static const IClosableVtbl visual_interaction_source_closable_vtbl =
{
    visual_interaction_source_closable_QueryInterface,
    visual_interaction_source_closable_AddRef,
    visual_interaction_source_closable_Release,
    visual_interaction_source_closable_GetIids,
    visual_interaction_source_closable_GetRuntimeClassName,
    visual_interaction_source_closable_GetTrustLevel,
    visual_interaction_source_closable_Close,
};

static HRESULT visual_interaction_source_create( IVisual *source, IVisualInteractionSource **result )
{
    struct visual_interaction_source *impl;
    ICompositionObject *source_object;
    ICompositionPropertySet *properties;
    ICompositor *compositor;
    HRESULT hr;

    if (!source || !result) return E_INVALIDARG;
    *result = NULL;
    if (FAILED(hr = IVisual_QueryInterface( source, &IID_ICompositionObject,
                                           (void **)&source_object ))) return hr;
    hr = ICompositionObject_get_Compositor( source_object, &compositor );
    ICompositionObject_Release( source_object );
    if (FAILED(hr)) return hr;
    if (FAILED(hr = composition_property_set_create( compositor, &properties )))
    {
        ICompositor_Release( compositor );
        return hr;
    }
    if (!(impl = calloc( 1, sizeof(*impl) )))
    {
        ICompositionPropertySet_Release( properties );
        ICompositor_Release( compositor );
        return E_OUTOFMEMORY;
    }
    impl->IVisualInteractionSource_iface.lpVtbl = &visual_interaction_source_vtbl;
    impl->IVisualInteractionSource2_iface.lpVtbl = &visual_interaction_source2_vtbl;
    impl->IVisualInteractionSource3_iface.lpVtbl = &visual_interaction_source3_vtbl;
    impl->ICompositionInteractionSource_iface.lpVtbl = &composition_interaction_source_vtbl;
    impl->ICompositionObject_iface.lpVtbl = &visual_interaction_source_object_vtbl;
    impl->IClosable_iface.lpVtbl = &visual_interaction_source_closable_vtbl;
    impl->source = source;
    impl->properties = properties;
    impl->compositor = compositor;
    impl->x_rails_enabled = TRUE;
    impl->y_rails_enabled = TRUE;
    impl->ref = 1;
    IVisual_AddRef( source );
    *result = &impl->IVisualInteractionSource_iface;
    return S_OK;
}

struct visual_interaction_source_factory
{
    IActivationFactory IActivationFactory_iface;
    IVisualInteractionSourceStatics IVisualInteractionSourceStatics_iface;
    LONG ref;
};

static inline struct visual_interaction_source_factory *factory_from_IActivationFactory(
        IActivationFactory *iface )
{
    return CONTAINING_RECORD( iface, struct visual_interaction_source_factory, IActivationFactory_iface );
}

static inline struct visual_interaction_source_factory *factory_from_IVisualInteractionSourceStatics(
        IVisualInteractionSourceStatics *iface )
{
    return CONTAINING_RECORD( iface, struct visual_interaction_source_factory,
                              IVisualInteractionSourceStatics_iface );
}

static HRESULT WINAPI visual_interaction_source_factory_QueryInterface( IActivationFactory *iface,
        REFIID iid, void **out )
{
    struct visual_interaction_source_factory *impl = factory_from_IActivationFactory( iface );

    FIXME( "iface %p, iid %s, out %p.\n", iface, debugstr_guid( iid ), out );
    if (!out) return E_POINTER;
    if (IsEqualGUID( iid, &IID_IUnknown ) || IsEqualGUID( iid, &IID_IInspectable ) ||
        IsEqualGUID( iid, &IID_IAgileObject ) || IsEqualGUID( iid, &IID_IActivationFactory ))
        *out = &impl->IActivationFactory_iface;
    else if (IsEqualGUID( iid, &IID_IVisualInteractionSourceStatics ))
        *out = &impl->IVisualInteractionSourceStatics_iface;
    else
    {
        *out = NULL;
        return E_NOINTERFACE;
    }
    IUnknown_AddRef( (IUnknown *)*out );
    return S_OK;
}

static ULONG WINAPI visual_interaction_source_factory_AddRef( IActivationFactory *iface )
{
    return InterlockedIncrement( &factory_from_IActivationFactory( iface )->ref );
}

static ULONG WINAPI visual_interaction_source_factory_Release( IActivationFactory *iface )
{
    return InterlockedDecrement( &factory_from_IActivationFactory( iface )->ref );
}

static HRESULT WINAPI visual_interaction_source_factory_GetIids( IActivationFactory *iface,
        ULONG *count, IID **iids )
{
    return E_NOTIMPL;
}

static HRESULT WINAPI visual_interaction_source_factory_GetRuntimeClassName(
        IActivationFactory *iface, HSTRING *name )
{
    return visual_interaction_source_get_class_name( name );
}

static HRESULT WINAPI visual_interaction_source_factory_GetTrustLevel( IActivationFactory *iface,
        TrustLevel *level )
{
    return visual_interaction_source_get_trust_level( level );
}

static HRESULT WINAPI visual_interaction_source_factory_ActivateInstance( IActivationFactory *iface,
        IInspectable **instance )
{
    if (!instance) return E_POINTER;
    *instance = NULL;
    return E_NOTIMPL;
}

static const IActivationFactoryVtbl visual_interaction_source_factory_vtbl =
{
    visual_interaction_source_factory_QueryInterface,
    visual_interaction_source_factory_AddRef,
    visual_interaction_source_factory_Release,
    visual_interaction_source_factory_GetIids,
    visual_interaction_source_factory_GetRuntimeClassName,
    visual_interaction_source_factory_GetTrustLevel,
    visual_interaction_source_factory_ActivateInstance,
};

static HRESULT WINAPI visual_interaction_source_statics_QueryInterface(
        IVisualInteractionSourceStatics *iface, REFIID iid, void **out )
{
    struct visual_interaction_source_factory *impl = factory_from_IVisualInteractionSourceStatics( iface );
    return IActivationFactory_QueryInterface( &impl->IActivationFactory_iface, iid, out );
}

static ULONG WINAPI visual_interaction_source_statics_AddRef( IVisualInteractionSourceStatics *iface )
{
    struct visual_interaction_source_factory *impl = factory_from_IVisualInteractionSourceStatics( iface );
    return IActivationFactory_AddRef( &impl->IActivationFactory_iface );
}

static ULONG WINAPI visual_interaction_source_statics_Release( IVisualInteractionSourceStatics *iface )
{
    struct visual_interaction_source_factory *impl = factory_from_IVisualInteractionSourceStatics( iface );
    return IActivationFactory_Release( &impl->IActivationFactory_iface );
}

static HRESULT WINAPI visual_interaction_source_statics_GetIids( IVisualInteractionSourceStatics *iface,
        ULONG *count, IID **iids )
{
    struct visual_interaction_source_factory *impl = factory_from_IVisualInteractionSourceStatics( iface );
    return IActivationFactory_GetIids( &impl->IActivationFactory_iface, count, iids );
}

static HRESULT WINAPI visual_interaction_source_statics_GetRuntimeClassName(
        IVisualInteractionSourceStatics *iface, HSTRING *name )
{
    struct visual_interaction_source_factory *impl = factory_from_IVisualInteractionSourceStatics( iface );
    return IActivationFactory_GetRuntimeClassName( &impl->IActivationFactory_iface, name );
}

static HRESULT WINAPI visual_interaction_source_statics_GetTrustLevel(
        IVisualInteractionSourceStatics *iface, TrustLevel *level )
{
    struct visual_interaction_source_factory *impl = factory_from_IVisualInteractionSourceStatics( iface );
    return IActivationFactory_GetTrustLevel( &impl->IActivationFactory_iface, level );
}

static HRESULT WINAPI visual_interaction_source_statics_Create( IVisualInteractionSourceStatics *iface,
        IVisual *source, IVisualInteractionSource **result )
{
    TRACE( "iface %p, source %p, result %p.\n", iface, source, result );
    return visual_interaction_source_create( source, result );
}

static const IVisualInteractionSourceStaticsVtbl visual_interaction_source_statics_vtbl =
{
    visual_interaction_source_statics_QueryInterface,
    visual_interaction_source_statics_AddRef,
    visual_interaction_source_statics_Release,
    visual_interaction_source_statics_GetIids,
    visual_interaction_source_statics_GetRuntimeClassName,
    visual_interaction_source_statics_GetTrustLevel,
    visual_interaction_source_statics_Create,
};

static struct visual_interaction_source_factory visual_interaction_source_factory_impl =
{
    {&visual_interaction_source_factory_vtbl},
    {&visual_interaction_source_statics_vtbl},
    1,
};

IActivationFactory *visual_interaction_source_factory =
    &visual_interaction_source_factory_impl.IActivationFactory_iface;
