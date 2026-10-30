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
    ICompositionObject ICompositionObject_iface;
    IClosable IClosable_iface;
    IExpressionAnimation *condition;
    IExpressionAnimation *value;
    ICompositionPropertySet *properties;
    ICompositor *compositor;
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
        IsEqualGUID( iid, &IID_IAgileObject ) || IsEqualGUID( iid, &IID_ICompositionConditionalValue ))
        *out = &impl->ICompositionConditionalValue_iface;
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
        ICompositionPropertySet_Release( impl->properties );
        ICompositor_Release( impl->compositor );
        free( impl );
    }
    return ref;
}

static HRESULT composition_conditional_value_get_iids( ULONG *count, IID **iids )
{
    IID *values;

    if (!count || !iids) return E_POINTER;
    if (!(values = CoTaskMemAlloc( 3 * sizeof(*values) ))) return E_OUTOFMEMORY;
    values[0] = IID_ICompositionConditionalValue;
    values[1] = IID_ICompositionObject;
    values[2] = IID_IClosable;
    *count = 3;
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
    return composition_conditional_value_get_iids( count, iids );
}

static HRESULT WINAPI composition_conditional_value_GetRuntimeClassName(
        ICompositionConditionalValue *iface, HSTRING *name )
{
    return composition_conditional_value_get_class_name( name );
}

static HRESULT WINAPI composition_conditional_value_GetTrustLevel(
        ICompositionConditionalValue *iface, TrustLevel *level )
{
    return composition_conditional_value_get_trust_level( level );
}

static HRESULT composition_conditional_value_get_animation( IExpressionAnimation *animation,
        IExpressionAnimation **value )
{
    if (!value) return E_POINTER;
    if (animation) IExpressionAnimation_AddRef( animation );
    *value = animation;
    return S_OK;
}

static HRESULT composition_conditional_value_put_animation( IExpressionAnimation **current,
        IExpressionAnimation *value )
{
    if (value) IExpressionAnimation_AddRef( value );
    if (*current) IExpressionAnimation_Release( *current );
    *current = value;
    return S_OK;
}

static HRESULT WINAPI composition_conditional_value_get_Condition( ICompositionConditionalValue *iface,
        IExpressionAnimation **value )
{
    return composition_conditional_value_get_animation(
            impl_from_ICompositionConditionalValue( iface )->condition, value );
}

static HRESULT WINAPI composition_conditional_value_put_Condition( ICompositionConditionalValue *iface,
        IExpressionAnimation *value )
{
    return composition_conditional_value_put_animation(
            &impl_from_ICompositionConditionalValue( iface )->condition, value );
}

static HRESULT WINAPI composition_conditional_value_get_Value( ICompositionConditionalValue *iface,
        IExpressionAnimation **value )
{
    return composition_conditional_value_get_animation(
            impl_from_ICompositionConditionalValue( iface )->value, value );
}

static HRESULT WINAPI composition_conditional_value_put_Value( ICompositionConditionalValue *iface,
        IExpressionAnimation *value )
{
    return composition_conditional_value_put_animation(
            &impl_from_ICompositionConditionalValue( iface )->value, value );
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
    return composition_conditional_value_get_iids( count, iids );
}

static HRESULT WINAPI conditional_value_composition_object_GetRuntimeClassName(
        ICompositionObject *iface, HSTRING *name )
{
    return composition_conditional_value_get_class_name( name );
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

    if (!value) return E_POINTER;
    ICompositor_AddRef( impl->compositor );
    *value = impl->compositor;
    return S_OK;
}

static HRESULT WINAPI conditional_value_composition_object_get_Dispatcher( ICompositionObject *iface,
        ICoreDispatcher **value )
{
    if (!value) return E_POINTER;
    *value = NULL;
    return S_OK;
}

static HRESULT WINAPI conditional_value_composition_object_get_Properties( ICompositionObject *iface,
        ICompositionPropertySet **value )
{
    struct composition_conditional_value *impl = conditional_value_from_ICompositionObject( iface );

    if (!value) return E_POINTER;
    ICompositionPropertySet_AddRef( impl->properties );
    *value = impl->properties;
    return S_OK;
}

static HRESULT WINAPI conditional_value_composition_object_StartAnimation( ICompositionObject *iface,
        HSTRING name, ICompositionAnimation *animation )
{
    TRACE( "iface %p, name %s, animation %p.\n", iface, debugstr_hstring( name ), animation );
    return S_OK;
}

static HRESULT WINAPI conditional_value_composition_object_StopAnimation( ICompositionObject *iface,
        HSTRING name )
{
    TRACE( "iface %p, name %s.\n", iface, debugstr_hstring( name ) );
    return S_OK;
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
    return composition_conditional_value_get_iids( count, iids );
}

static HRESULT WINAPI conditional_value_closable_GetRuntimeClassName( IClosable *iface, HSTRING *name )
{
    return composition_conditional_value_get_class_name( name );
}

static HRESULT WINAPI conditional_value_closable_GetTrustLevel( IClosable *iface, TrustLevel *level )
{
    return composition_conditional_value_get_trust_level( level );
}

static HRESULT WINAPI conditional_value_closable_Close( IClosable *iface )
{
    TRACE( "iface %p.\n", iface );
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

static HRESULT composition_conditional_value_create( ICompositor *compositor,
        ICompositionConditionalValue **result )
{
    struct composition_conditional_value *impl;
    HRESULT hr;

    if (!compositor || !result) return E_INVALIDARG;
    *result = NULL;
    if (!(impl = calloc( 1, sizeof(*impl) ))) return E_OUTOFMEMORY;
    impl->ICompositionConditionalValue_iface.lpVtbl = &composition_conditional_value_vtbl;
    impl->ICompositionObject_iface.lpVtbl = &conditional_value_composition_object_vtbl;
    impl->IClosable_iface.lpVtbl = &conditional_value_closable_vtbl;
    impl->compositor = compositor;
    impl->ref = 1;
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
    return composition_conditional_value_create( compositor, result );
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
    struct list bindings;
    struct list update_entry;
    struct list changed_entry;
    IInspectable *position_decay_rate;
    IInspectable *scale_decay_rate;
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

struct tracker_notification
{
    struct list entry;
    struct tracker_values_changed_args *args;
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
    IInteractionTrackerValuesChangedArgs_Release( &notification->args->IInteractionTrackerValuesChangedArgs_iface );
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
            hr = IInteractionTrackerOwner_ValuesChanged( owner, notification->sender,
                    &notification->args->IInteractionTrackerValuesChangedArgs_iface );
            IInteractionTrackerOwner_Release( owner );
        }
        if (FAILED(hr) && hr != RO_E_CLOSED) WARN( "ValuesChanged delivery failed, hr %#lx.\n", hr );
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

static void interaction_tracker_queue_notification( struct interaction_tracker *impl, INT32 request_id,
        struct list *discarded )
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
    notification->args = args;
    notification->sender = &impl->IInteractionTracker_iface;
    notification->owner = impl->owner;
    IAgileReference_AddRef( notification->owner );
    LeaveCriticalSection( &impl->animation_cs );

    EnterCriticalSection( &tracker_notification_cs );
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
        struct list *changed, struct list *discarded )
{
    struct interaction_tracker *tracker;

    while (!list_empty( changed ))
    {
        tracker = LIST_ENTRY( list_head( changed ), struct interaction_tracker, changed_entry );
        list_remove( &tracker->changed_entry );
        list_init( &tracker->changed_entry );
        interaction_tracker_queue_notification( tracker, tracker == impl ? request_id : 0, discarded );
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
        if (impl->position_decay_rate) IInspectable_Release( impl->position_decay_rate );
        if (impl->scale_decay_rate) IInspectable_Release( impl->scale_decay_rate );
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
    *value = impl->position;
    LeaveCriticalSection( &impl->animation_cs );
    return S_OK;
}

static HRESULT WINAPI interaction_tracker_get_NaturalRestingScale( IInteractionTracker *iface, FLOAT *value )
{
    struct interaction_tracker *impl = impl_from_IInteractionTracker( iface );

    if (!value) return E_POINTER;
    EnterCriticalSection( &impl->animation_cs );
    *value = impl->scale;
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

static HRESULT interaction_tracker_get_decay_rate( IInspectable *rate, IInspectable **value )
{
    if (!value) return E_POINTER;
    if (rate) IInspectable_AddRef( rate );
    *value = rate;
    return S_OK;
}

static HRESULT interaction_tracker_put_decay_rate( IInspectable **current, IInspectable *value )
{
    if (value) IInspectable_AddRef( value );
    if (*current) IInspectable_Release( *current );
    *current = value;
    return S_OK;
}

static HRESULT WINAPI interaction_tracker_get_PositionInertiaDecayRate( IInteractionTracker *iface,
        IInspectable **value )
{
    return interaction_tracker_get_decay_rate( impl_from_IInteractionTracker( iface )->position_decay_rate,
                                               value );
}

static HRESULT WINAPI interaction_tracker_put_PositionInertiaDecayRate( IInteractionTracker *iface,
        IInspectable *value )
{
    return interaction_tracker_put_decay_rate(
            &impl_from_IInteractionTracker( iface )->position_decay_rate, value );
}

static HRESULT WINAPI interaction_tracker_get_PositionVelocityInPixelsPerSecond(
        IInteractionTracker *iface, struct Vector3 *value )
{
    if (!value) return E_POINTER;
    memset( value, 0, sizeof(*value) );
    return S_OK;
}

static HRESULT WINAPI interaction_tracker_get_Scale( IInteractionTracker *iface, FLOAT *value )
{
    struct interaction_tracker *impl = impl_from_IInteractionTracker( iface );

    if (!value) return E_POINTER;
    EnterCriticalSection( &impl->animation_cs );
    *value = impl->scale;
    LeaveCriticalSection( &impl->animation_cs );
    return S_OK;
}

static HRESULT WINAPI interaction_tracker_get_ScaleInertiaDecayRate( IInteractionTracker *iface,
        IInspectable **value )
{
    return interaction_tracker_get_decay_rate( impl_from_IInteractionTracker( iface )->scale_decay_rate, value );
}

static HRESULT WINAPI interaction_tracker_put_ScaleInertiaDecayRate( IInteractionTracker *iface,
        IInspectable *value )
{
    return interaction_tracker_put_decay_rate( &impl_from_IInteractionTracker( iface )->scale_decay_rate,
                                               value );
}

static HRESULT WINAPI interaction_tracker_get_ScaleVelocityInPercentPerSecond(
        IInteractionTracker *iface, FLOAT *value )
{
    if (!value) return E_POINTER;
    *value = 0.0f;
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

static HRESULT interaction_tracker_update_position( struct interaction_tracker *impl,
        struct Vector3 value, BOOL relative, BOOL clamp, INT32 *request_id )
{
    struct list changed, discarded;
    HRESULT hr;

    if (clamp && FAILED(hr = interaction_tracker_evaluate_bounds( impl ))) return hr;
    list_init( &changed );
    list_init( &discarded );
    EnterCriticalSection( &tracker_binding_cs );
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
    interaction_tracker_notify_changes( impl, *request_id, &changed, &discarded );
    LeaveCriticalSection( &tracker_binding_cs );
    tracker_notifications_free( &discarded );
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
    if (!request_id) return E_POINTER;
    *request_id = interaction_tracker_next_request_id( impl_from_IInteractionTracker( iface ) );
    return S_OK;
}

static HRESULT WINAPI interaction_tracker_TryUpdatePositionWithAdditionalVelocity(
        IInteractionTracker *iface, struct Vector3 velocity, INT32 *request_id )
{
    if (!request_id) return E_POINTER;
    *request_id = interaction_tracker_next_request_id( impl_from_IInteractionTracker( iface ) );
    return S_OK;
}

static HRESULT WINAPI interaction_tracker_TryUpdateScale( IInteractionTracker *iface, FLOAT value,
        struct Vector3 center_point, INT32 *request_id )
{
    struct interaction_tracker *impl = impl_from_IInteractionTracker( iface );

    struct list changed, discarded;

    if (!request_id) return E_POINTER;
    list_init( &changed );
    list_init( &discarded );
    EnterCriticalSection( &tracker_binding_cs );
    *request_id = interaction_tracker_next_request_id( impl );
    EnterCriticalSection( &impl->animation_cs );
    value = min( max( value, impl->min_scale ), impl->max_scale );
    LeaveCriticalSection( &impl->animation_cs );
    interaction_tracker_update_axis( impl, InteractionBindingAxisModes_Scale, value, &changed );
    interaction_tracker_notify_changes( impl, *request_id, &changed, &discarded );
    LeaveCriticalSection( &tracker_binding_cs );
    tracker_notifications_free( &discarded );
    return S_OK;
}

static HRESULT WINAPI interaction_tracker_TryUpdateScaleWithAnimation( IInteractionTracker *iface,
        IInspectable *animation, struct Vector3 center_point, INT32 *request_id )
{
    if (!request_id) return E_POINTER;
    *request_id = interaction_tracker_next_request_id( impl_from_IInteractionTracker( iface ) );
    return S_OK;
}

static HRESULT WINAPI interaction_tracker_TryUpdateScaleWithAdditionalVelocity(
        IInteractionTracker *iface, FLOAT velocity, struct Vector3 center_point, INT32 *request_id )
{
    if (!request_id) return E_POINTER;
    *request_id = interaction_tracker_next_request_id( impl_from_IInteractionTracker( iface ) );
    return S_OK;
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
    if (!value) return E_POINTER;
    *value = FALSE;
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
    index = tracker_component_index( name );
    if (index == ARRAY_SIZE(tracker_component_names)) return E_NOTIMPL;
    if (FAILED(hr = ICompositionAnimation_QueryInterface( animation, &IID_IExpressionAnimation, (void **)&expression ))) return hr;
    hr = expression_animation_clone( expression, &copy );
    IExpressionAnimation_Release( expression );
    if (FAILED(hr)) return hr;
    EnterCriticalSection( &impl->animation_cs );
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

    TRACE( "iface %p.\n", iface );
    EnterCriticalSection( &impl->animation_cs );
    impl->notifications_closed = TRUE;
    owner = impl->owner;
    impl->owner = NULL;
    cookie = impl->owner_mta_cookie;
    impl->owner_mta_cookie = NULL;
    LeaveCriticalSection( &impl->animation_cs );
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
