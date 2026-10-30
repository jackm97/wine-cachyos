/* WinRT CoreMessaging Implementation
 *
 * Copyright (C) 2024 Mohamad Al-Jaf
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

#include "initguid.h"
#include "private.h"
#include "dispatcherqueue.h"
#include "weakref.h"
#include "lmcons.h"
#include "sddl.h"
#include "wine/list.h"

WINE_DEFAULT_DEBUG_CHANNEL(messaging);

struct dispatcher_queue
{
    IDispatcherQueue IDispatcherQueue_iface;
    IDispatcherQueue2 IDispatcherQueue2_iface;
    struct weak_reference_source weak_reference_source;
    DWORD thread_id;
    SRWLOCK events_lock;
    struct list shutdown_starting;
    struct list shutdown_completed;
};

struct dispatcher_queue_event
{
    struct list entry;
    EventRegistrationToken token;
    IUnknown *handler;
};

static LONG dispatcher_queue_next_token;

static HRESULT dispatcher_queue_add_event( struct dispatcher_queue *impl, struct list *events,
        IUnknown *handler, EventRegistrationToken *token )
{
    struct dispatcher_queue_event *event;

    if (!handler || !token) return E_INVALIDARG;
    if (!(event = calloc( 1, sizeof(*event) ))) return E_OUTOFMEMORY;
    event->handler = handler;
    IUnknown_AddRef( event->handler );
    event->token.value = InterlockedIncrement( &dispatcher_queue_next_token );

    AcquireSRWLockExclusive( &impl->events_lock );
    list_add_tail( events, &event->entry );
    ReleaseSRWLockExclusive( &impl->events_lock );
    *token = event->token;
    return S_OK;
}

static HRESULT dispatcher_queue_remove_event( struct dispatcher_queue *impl, struct list *events,
        EventRegistrationToken token )
{
    struct dispatcher_queue_event *event, *found = NULL;

    AcquireSRWLockExclusive( &impl->events_lock );
    LIST_FOR_EACH_ENTRY( event, events, struct dispatcher_queue_event, entry )
    {
        if (event->token.value != token.value) continue;
        list_remove( &event->entry );
        found = event;
        break;
    }
    ReleaseSRWLockExclusive( &impl->events_lock );

    if (found)
    {
        IUnknown_Release( found->handler );
        free( found );
    }
    return S_OK;
}

static inline struct dispatcher_queue *dispatcher_queue_from_iface( IDispatcherQueue *iface )
{
    return CONTAINING_RECORD( iface, struct dispatcher_queue, IDispatcherQueue_iface );
}

static inline struct dispatcher_queue *dispatcher_queue_from_iface2( IDispatcherQueue2 *iface )
{
    return CONTAINING_RECORD( iface, struct dispatcher_queue, IDispatcherQueue2_iface );
}

static HRESULT dispatcher_queue_query_interface( struct dispatcher_queue *impl, REFIID iid, void **out )
{
    TRACE( "impl %p, iid %s, out %p.\n", impl, debugstr_guid( iid ), out );
    if (!out) return E_POINTER;
    if (IsEqualGUID( iid, &IID_IUnknown ) || IsEqualGUID( iid, &IID_IInspectable ) ||
        IsEqualGUID( iid, &IID_IAgileObject ) || IsEqualGUID( iid, &IID_IDispatcherQueue ))
        *out = &impl->IDispatcherQueue_iface;
    else if (IsEqualGUID( iid, &IID_IDispatcherQueue2 ))
        *out = &impl->IDispatcherQueue2_iface;
    else if (IsEqualGUID( iid, &IID_IWeakReferenceSource ))
        *out = &impl->weak_reference_source.IWeakReferenceSource_iface;
    else
    {
        *out = NULL;
        return E_NOINTERFACE;
    }
    IInspectable_AddRef( *out );
    return S_OK;
}

static HRESULT WINAPI dispatcher_queue_QueryInterface( IDispatcherQueue *iface, REFIID iid, void **out )
{
    return dispatcher_queue_query_interface( dispatcher_queue_from_iface( iface ), iid, out );
}

static ULONG WINAPI dispatcher_queue_AddRef( IDispatcherQueue *iface )
{
    return weak_reference_strong_add_ref( &dispatcher_queue_from_iface( iface )->weak_reference_source );
}

static ULONG WINAPI dispatcher_queue_Release( IDispatcherQueue *iface )
{
    struct dispatcher_queue *impl = dispatcher_queue_from_iface( iface );
    struct dispatcher_queue_event *event, *next;
    ULONG ref = weak_reference_strong_release( &impl->weak_reference_source );

    if (!ref)
    {
        LIST_FOR_EACH_ENTRY_SAFE( event, next, &impl->shutdown_starting, struct dispatcher_queue_event, entry )
        {
            IUnknown_Release( event->handler );
            free( event );
        }
        LIST_FOR_EACH_ENTRY_SAFE( event, next, &impl->shutdown_completed, struct dispatcher_queue_event, entry )
        {
            IUnknown_Release( event->handler );
            free( event );
        }
        free( impl );
    }
    return ref;
}

static HRESULT WINAPI dispatcher_queue_GetIids( IDispatcherQueue *iface, ULONG *count, IID **iids )
{
    IID *values;

    if (!count || !iids) return E_POINTER;
    *count = 0;
    *iids = NULL;
    if (!(values = CoTaskMemAlloc( 2 * sizeof(*values) ))) return E_OUTOFMEMORY;
    values[0] = IID_IDispatcherQueue;
    values[1] = IID_IDispatcherQueue2;
    *count = 2;
    *iids = values;
    return S_OK;
}

static HRESULT WINAPI dispatcher_queue_GetRuntimeClassName( IDispatcherQueue *iface, HSTRING *name )
{
    if (!name) return E_POINTER;
    return WindowsCreateString( RuntimeClass_Windows_System_DispatcherQueue,
                                ARRAY_SIZE(RuntimeClass_Windows_System_DispatcherQueue) - 1, name );
}

static HRESULT WINAPI dispatcher_queue_GetTrustLevel( IDispatcherQueue *iface, TrustLevel *level )
{
    if (!level) return E_POINTER;
    *level = BaseTrust;
    return S_OK;
}

static HRESULT WINAPI dispatcher_queue_CreateTimer( IDispatcherQueue *iface, IDispatcherQueueTimer **result )
{
    FIXME( "iface %p, result %p stub!\n", iface, result );
    if (!result) return E_POINTER;
    *result = NULL;
    return E_NOTIMPL;
}

static HRESULT WINAPI dispatcher_queue_TryEnqueue( IDispatcherQueue *iface,
        IDispatcherQueueHandler *callback, boolean *result )
{
    TRACE( "iface %p, callback %p, result %p.\n", iface, callback, result );
    if (!callback || !result) return E_POINTER;
    *result = TRUE;
    IDispatcherQueueHandler_Invoke( callback );
    return S_OK;
}

static HRESULT WINAPI dispatcher_queue_TryEnqueueWithPriority( IDispatcherQueue *iface,
        DispatcherQueuePriority priority, IDispatcherQueueHandler *callback, boolean *result )
{
    TRACE( "iface %p, priority %u, callback %p, result %p.\n", iface, priority, callback, result );
    return dispatcher_queue_TryEnqueue( iface, callback, result );
}

static HRESULT WINAPI dispatcher_queue_add_ShutdownStarting( IDispatcherQueue *iface,
        ITypedEventHandler_DispatcherQueue_DispatcherQueueShutdownStartingEventArgs *handler,
        EventRegistrationToken *token )
{
    struct dispatcher_queue *impl = dispatcher_queue_from_iface( iface );

    TRACE( "iface %p, handler %p, token %p.\n", iface, handler, token );
    return dispatcher_queue_add_event( impl, &impl->shutdown_starting, (IUnknown *)handler, token );
}

static HRESULT WINAPI dispatcher_queue_remove_ShutdownStarting( IDispatcherQueue *iface,
        EventRegistrationToken token )
{
    struct dispatcher_queue *impl = dispatcher_queue_from_iface( iface );

    TRACE( "iface %p, token %s.\n", iface, wine_dbgstr_longlong( token.value ) );
    return dispatcher_queue_remove_event( impl, &impl->shutdown_starting, token );
}

static HRESULT WINAPI dispatcher_queue_add_ShutdownCompleted( IDispatcherQueue *iface,
        ITypedEventHandler_DispatcherQueue_IInspectable *handler, EventRegistrationToken *token )
{
    struct dispatcher_queue *impl = dispatcher_queue_from_iface( iface );

    TRACE( "iface %p, handler %p, token %p.\n", iface, handler, token );
    return dispatcher_queue_add_event( impl, &impl->shutdown_completed, (IUnknown *)handler, token );
}

static HRESULT WINAPI dispatcher_queue_remove_ShutdownCompleted( IDispatcherQueue *iface,
        EventRegistrationToken token )
{
    struct dispatcher_queue *impl = dispatcher_queue_from_iface( iface );

    TRACE( "iface %p, token %s.\n", iface, wine_dbgstr_longlong( token.value ) );
    return dispatcher_queue_remove_event( impl, &impl->shutdown_completed, token );
}

static const IDispatcherQueueVtbl dispatcher_queue_vtbl =
{
    dispatcher_queue_QueryInterface,
    dispatcher_queue_AddRef,
    dispatcher_queue_Release,
    dispatcher_queue_GetIids,
    dispatcher_queue_GetRuntimeClassName,
    dispatcher_queue_GetTrustLevel,
    dispatcher_queue_CreateTimer,
    dispatcher_queue_TryEnqueue,
    dispatcher_queue_TryEnqueueWithPriority,
    dispatcher_queue_add_ShutdownStarting,
    dispatcher_queue_remove_ShutdownStarting,
    dispatcher_queue_add_ShutdownCompleted,
    dispatcher_queue_remove_ShutdownCompleted,
};

static HRESULT WINAPI dispatcher_queue2_QueryInterface( IDispatcherQueue2 *iface, REFIID iid, void **out )
{
    return dispatcher_queue_query_interface( dispatcher_queue_from_iface2( iface ), iid, out );
}

static ULONG WINAPI dispatcher_queue2_AddRef( IDispatcherQueue2 *iface )
{
    struct dispatcher_queue *impl = dispatcher_queue_from_iface2( iface );
    return IDispatcherQueue_AddRef( &impl->IDispatcherQueue_iface );
}

static ULONG WINAPI dispatcher_queue2_Release( IDispatcherQueue2 *iface )
{
    struct dispatcher_queue *impl = dispatcher_queue_from_iface2( iface );
    return IDispatcherQueue_Release( &impl->IDispatcherQueue_iface );
}

static HRESULT WINAPI dispatcher_queue2_GetIids( IDispatcherQueue2 *iface, ULONG *count, IID **iids )
{
    struct dispatcher_queue *impl = dispatcher_queue_from_iface2( iface );
    return IDispatcherQueue_GetIids( &impl->IDispatcherQueue_iface, count, iids );
}

static HRESULT WINAPI dispatcher_queue2_GetRuntimeClassName( IDispatcherQueue2 *iface, HSTRING *name )
{
    struct dispatcher_queue *impl = dispatcher_queue_from_iface2( iface );
    return IDispatcherQueue_GetRuntimeClassName( &impl->IDispatcherQueue_iface, name );
}

static HRESULT WINAPI dispatcher_queue2_GetTrustLevel( IDispatcherQueue2 *iface, TrustLevel *level )
{
    struct dispatcher_queue *impl = dispatcher_queue_from_iface2( iface );
    return IDispatcherQueue_GetTrustLevel( &impl->IDispatcherQueue_iface, level );
}

static HRESULT WINAPI dispatcher_queue2_get_HasThreadAccess( IDispatcherQueue2 *iface, boolean *value )
{
    TRACE( "iface %p, value %p.\n", iface, value );
    if (!value) return E_POINTER;
    *value = dispatcher_queue_from_iface2( iface )->thread_id == GetCurrentThreadId();
    return S_OK;
}

static const IDispatcherQueue2Vtbl dispatcher_queue2_vtbl =
{
    dispatcher_queue2_QueryInterface,
    dispatcher_queue2_AddRef,
    dispatcher_queue2_Release,
    dispatcher_queue2_GetIids,
    dispatcher_queue2_GetRuntimeClassName,
    dispatcher_queue2_GetTrustLevel,
    dispatcher_queue2_get_HasThreadAccess,
};

static HRESULT dispatcher_queue_create( IDispatcherQueue **result )
{
    struct dispatcher_queue *impl;

    if (!result) return E_POINTER;
    *result = NULL;
    if (!(impl = calloc( 1, sizeof(*impl) ))) return E_OUTOFMEMORY;
    impl->IDispatcherQueue_iface.lpVtbl = &dispatcher_queue_vtbl;
    impl->IDispatcherQueue2_iface.lpVtbl = &dispatcher_queue2_vtbl;
    impl->thread_id = GetCurrentThreadId();
    InitializeSRWLock( &impl->events_lock );
    list_init( &impl->shutdown_starting );
    list_init( &impl->shutdown_completed );
    if (FAILED(weak_reference_source_init( &impl->weak_reference_source,
                                          (IUnknown *)&impl->IDispatcherQueue_iface )))
    {
        free( impl );
        return E_OUTOFMEMORY;
    }
    *result = &impl->IDispatcherQueue_iface;
    TRACE( "created queue %p for thread %lu.\n", *result, impl->thread_id );
    return S_OK;
}

struct dispatcher_queue_statics
{
    IActivationFactory IActivationFactory_iface;
    IDispatcherQueueStatics IDispatcherQueueStatics_iface;
    LONG ref;
};

static inline struct dispatcher_queue_statics *dispatcher_queue_statics_from_factory( IActivationFactory *iface )
{
    return CONTAINING_RECORD( iface, struct dispatcher_queue_statics, IActivationFactory_iface );
}

static HRESULT WINAPI dispatcher_queue_factory_QueryInterface( IActivationFactory *iface,
        REFIID iid, void **out )
{
    struct dispatcher_queue_statics *impl = dispatcher_queue_statics_from_factory( iface );

    TRACE( "iface %p, iid %s, out %p.\n", iface, debugstr_guid( iid ), out );
    if (!out) return E_POINTER;
    if (IsEqualGUID( iid, &IID_IUnknown ) || IsEqualGUID( iid, &IID_IInspectable ) ||
        IsEqualGUID( iid, &IID_IAgileObject ) || IsEqualGUID( iid, &IID_IActivationFactory ))
        *out = &impl->IActivationFactory_iface;
    else if (IsEqualGUID( iid, &IID_IDispatcherQueueStatics ))
        *out = &impl->IDispatcherQueueStatics_iface;
    else
    {
        *out = NULL;
        return E_NOINTERFACE;
    }
    IInspectable_AddRef( *out );
    return S_OK;
}

static ULONG WINAPI dispatcher_queue_factory_AddRef( IActivationFactory *iface )
{
    return InterlockedIncrement( &dispatcher_queue_statics_from_factory( iface )->ref );
}

static ULONG WINAPI dispatcher_queue_factory_Release( IActivationFactory *iface )
{
    return InterlockedDecrement( &dispatcher_queue_statics_from_factory( iface )->ref );
}

static HRESULT WINAPI dispatcher_queue_factory_GetIids( IActivationFactory *iface,
        ULONG *count, IID **iids )
{
    return E_NOTIMPL;
}

static HRESULT WINAPI dispatcher_queue_factory_GetRuntimeClassName( IActivationFactory *iface,
        HSTRING *name )
{
    return E_NOTIMPL;
}

static HRESULT WINAPI dispatcher_queue_factory_GetTrustLevel( IActivationFactory *iface,
        TrustLevel *level )
{
    return E_NOTIMPL;
}

static HRESULT WINAPI dispatcher_queue_factory_ActivateInstance( IActivationFactory *iface,
        IInspectable **instance )
{
    FIXME( "iface %p, instance %p stub!\n", iface, instance );
    return E_NOTIMPL;
}

static const IActivationFactoryVtbl dispatcher_queue_factory_vtbl =
{
    dispatcher_queue_factory_QueryInterface,
    dispatcher_queue_factory_AddRef,
    dispatcher_queue_factory_Release,
    dispatcher_queue_factory_GetIids,
    dispatcher_queue_factory_GetRuntimeClassName,
    dispatcher_queue_factory_GetTrustLevel,
    dispatcher_queue_factory_ActivateInstance,
};

DEFINE_IINSPECTABLE( dispatcher_queue_statics, IDispatcherQueueStatics,
                     struct dispatcher_queue_statics, IActivationFactory_iface )

static INIT_ONCE dispatcher_queue_fls_once = INIT_ONCE_STATIC_INIT;
static DWORD dispatcher_queue_fls_index = FLS_OUT_OF_INDEXES;

static void WINAPI dispatcher_queue_fls_cleanup( void *value )
{
    if (value) IDispatcherQueue_Release( value );
}

static BOOL CALLBACK dispatcher_queue_init_fls( INIT_ONCE *once, void *param, void **context )
{
    dispatcher_queue_fls_index = FlsAlloc( dispatcher_queue_fls_cleanup );
    return dispatcher_queue_fls_index != FLS_OUT_OF_INDEXES;
}

static HRESULT WINAPI dispatcher_queue_statics_GetForCurrentThread( IDispatcherQueueStatics *iface,
        IDispatcherQueue **result )
{
    IDispatcherQueue *queue;
    HRESULT hr;

    TRACE( "iface %p, result %p.\n", iface, result );
    if (!result) return E_POINTER;
    *result = NULL;
    if (!InitOnceExecuteOnce( &dispatcher_queue_fls_once, dispatcher_queue_init_fls, NULL, NULL ))
        return HRESULT_FROM_WIN32( GetLastError() );
    if ((queue = FlsGetValue( dispatcher_queue_fls_index )))
    {
        IDispatcherQueue_AddRef( queue );
        *result = queue;
        return S_OK;
    }
    if (FAILED(hr = dispatcher_queue_create( &queue ))) return hr;
    if (!FlsSetValue( dispatcher_queue_fls_index, queue ))
    {
        hr = HRESULT_FROM_WIN32( GetLastError() );
        IDispatcherQueue_Release( queue );
        return hr;
    }
    IDispatcherQueue_AddRef( queue );
    *result = queue;
    return S_OK;
}

static const IDispatcherQueueStaticsVtbl dispatcher_queue_statics_vtbl =
{
    dispatcher_queue_statics_QueryInterface,
    dispatcher_queue_statics_AddRef,
    dispatcher_queue_statics_Release,
    dispatcher_queue_statics_GetIids,
    dispatcher_queue_statics_GetRuntimeClassName,
    dispatcher_queue_statics_GetTrustLevel,
    dispatcher_queue_statics_GetForCurrentThread,
};

static struct dispatcher_queue_statics dispatcher_queue_statics_impl =
{
    {&dispatcher_queue_factory_vtbl},
    {&dispatcher_queue_statics_vtbl},
    1,
};

struct dispatcher_queue_controller_statics
{
    IActivationFactory IActivationFactory_iface;
    IDispatcherQueueControllerStatics IDispatcherQueueControllerStatics_iface;
    LONG ref;
};

static inline struct dispatcher_queue_controller_statics *impl_from_IActivationFactory( IActivationFactory *iface )
{
    return CONTAINING_RECORD( iface, struct dispatcher_queue_controller_statics, IActivationFactory_iface );
}

static HRESULT WINAPI factory_QueryInterface( IActivationFactory *iface, REFIID iid, void **out )
{
    struct dispatcher_queue_controller_statics *impl = impl_from_IActivationFactory( iface );

    TRACE( "iface %p, iid %s, out %p.\n", iface, debugstr_guid( iid ), out );

    if (IsEqualGUID( iid, &IID_IUnknown ) ||
        IsEqualGUID( iid, &IID_IInspectable ) ||
        IsEqualGUID( iid, &IID_IAgileObject ) ||
        IsEqualGUID( iid, &IID_IActivationFactory ))
    {
        *out = &impl->IActivationFactory_iface;
        IInspectable_AddRef( *out );
        return S_OK;
    }

    if (IsEqualGUID( iid, &IID_IDispatcherQueueControllerStatics ))
    {
        *out = &impl->IDispatcherQueueControllerStatics_iface;
        IInspectable_AddRef( *out );
        return S_OK;
    }

    FIXME( "%s not implemented, returning E_NOINTERFACE.\n", debugstr_guid( iid ) );
    *out = NULL;
    return E_NOINTERFACE;
}

static ULONG WINAPI factory_AddRef( IActivationFactory *iface )
{
    struct dispatcher_queue_controller_statics *impl = impl_from_IActivationFactory( iface );
    ULONG ref = InterlockedIncrement( &impl->ref );
    TRACE( "iface %p increasing ref to %lu.\n", iface, ref );
    return ref;
}

static ULONG WINAPI factory_Release( IActivationFactory *iface )
{
    struct dispatcher_queue_controller_statics *impl = impl_from_IActivationFactory( iface );
    ULONG ref = InterlockedDecrement( &impl->ref );
    TRACE( "iface %p decreasing ref to %lu.\n", iface, ref );
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

struct dispatcher_queue_controller
{
    IDispatcherQueueController IDispatcherQueueController_iface;
    LONG ref;
};

static inline struct dispatcher_queue_controller *impl_from_IDispatcherQueueController( IDispatcherQueueController *iface )
{
    return CONTAINING_RECORD( iface, struct dispatcher_queue_controller, IDispatcherQueueController_iface );
}

static HRESULT WINAPI dispatcher_queue_controller_QueryInterface( IDispatcherQueueController *iface, REFIID iid, void **out )
{
    struct dispatcher_queue_controller *impl = impl_from_IDispatcherQueueController( iface );

    TRACE( "iface %p, iid %s, out %p.\n", iface, debugstr_guid( iid ), out );

    if (IsEqualGUID( iid, &IID_IUnknown ) ||
        IsEqualGUID( iid, &IID_IInspectable ) ||
        IsEqualGUID( iid, &IID_IAgileObject ) ||
        IsEqualGUID( iid, &IID_IDispatcherQueueController ))
    {
        *out = &impl->IDispatcherQueueController_iface;
        IInspectable_AddRef( *out );
        return S_OK;
    }

    FIXME( "%s not implemented, returning E_NOINTERFACE.\n", debugstr_guid( iid ) );
    *out = NULL;
    return E_NOINTERFACE;
}

static ULONG WINAPI dispatcher_queue_controller_AddRef( IDispatcherQueueController *iface )
{
    struct dispatcher_queue_controller *impl = impl_from_IDispatcherQueueController( iface );
    ULONG ref = InterlockedIncrement( &impl->ref );
    TRACE( "iface %p increasing ref to %lu.\n", iface, ref );
    return ref;
}

static ULONG WINAPI dispatcher_queue_controller_Release( IDispatcherQueueController *iface )
{
    struct dispatcher_queue_controller *impl = impl_from_IDispatcherQueueController( iface );
    ULONG ref = InterlockedDecrement( &impl->ref );

    TRACE( "iface %p decreasing ref to %lu.\n", iface, ref );

    if (!ref) free( impl );
    return ref;
}

static HRESULT WINAPI dispatcher_queue_controller_GetIids( IDispatcherQueueController *iface, ULONG *iid_count, IID **iids )
{
    FIXME( "iface %p, iid_count %p, iids %p stub!\n", iface, iid_count, iids );
    return E_NOTIMPL;
}

static HRESULT WINAPI dispatcher_queue_controller_GetRuntimeClassName( IDispatcherQueueController *iface, HSTRING *class_name )
{
    FIXME( "iface %p, class_name %p stub!\n", iface, class_name );
    return E_NOTIMPL;
}

static HRESULT WINAPI dispatcher_queue_controller_GetTrustLevel( IDispatcherQueueController *iface, TrustLevel *trust_level )
{
    FIXME( "iface %p, trust_level %p stub!\n", iface, trust_level );
    return E_NOTIMPL;
}

static HRESULT WINAPI dispatcher_queue_controller_get_DispatcherQueue( IDispatcherQueueController *iface, IDispatcherQueue **value )
{
    FIXME( "iface %p, value %p stub!\n", iface, value );
    return E_NOTIMPL;
}

static HRESULT shutdown_queue_async( IUnknown *invoker, IUnknown *param, PROPVARIANT *result, BOOL called_async )
{
    if (!called_async) return STATUS_PENDING;
    return S_OK;
}

static HRESULT WINAPI dispatcher_queue_controller_ShutdownQueueAsync( IDispatcherQueueController *iface, IAsyncAction **operation )
{
    FIXME( "iface %p, operation %p stub!\n", iface, operation );

    if (!operation) return E_POINTER;
    *operation = NULL;

    return async_action_create( NULL, shutdown_queue_async, operation );
}

static const struct IDispatcherQueueControllerVtbl dispatcher_queue_controller_vtbl =
{
    dispatcher_queue_controller_QueryInterface,
    dispatcher_queue_controller_AddRef,
    dispatcher_queue_controller_Release,
    /* IInspectable methods */
    dispatcher_queue_controller_GetIids,
    dispatcher_queue_controller_GetRuntimeClassName,
    dispatcher_queue_controller_GetTrustLevel,
    /* IDispatcherQueueController methods */
    dispatcher_queue_controller_get_DispatcherQueue,
    dispatcher_queue_controller_ShutdownQueueAsync,
};

DEFINE_IINSPECTABLE( dispatcher_queue_controller_statics, IDispatcherQueueControllerStatics, struct dispatcher_queue_controller_statics, IActivationFactory_iface )

static HRESULT WINAPI dispatcher_queue_controller_statics_CreateOnDedicatedThread( IDispatcherQueueControllerStatics *iface, IDispatcherQueueController **result )
{
    FIXME( "iface %p, result %p stub!\n", iface, result );
    return E_NOTIMPL;
}

static const struct IDispatcherQueueControllerStaticsVtbl dispatcher_queue_controller_statics_vtbl =
{
    dispatcher_queue_controller_statics_QueryInterface,
    dispatcher_queue_controller_statics_AddRef,
    dispatcher_queue_controller_statics_Release,
    /* IInspectable methods */
    dispatcher_queue_controller_statics_GetIids,
    dispatcher_queue_controller_statics_GetRuntimeClassName,
    dispatcher_queue_controller_statics_GetTrustLevel,
    /* IDispatcherQueueControllerStatics methods */
    dispatcher_queue_controller_statics_CreateOnDedicatedThread,
};

static struct dispatcher_queue_controller_statics dispatcher_queue_controller_statics =
{
    {&factory_vtbl},
    {&dispatcher_queue_controller_statics_vtbl},
    1,
};

static IActivationFactory *dispatcher_queue_controller_factory = &dispatcher_queue_controller_statics.IActivationFactory_iface;

struct user
{
    IUser IUser_iface;
    LONG ref;
    HSTRING id;
};

static inline struct user *impl_from_IUser( IUser *iface )
{
    return CONTAINING_RECORD( iface, struct user, IUser_iface );
}

static HRESULT WINAPI user_QueryInterface( IUser *iface, REFIID iid, void **out )
{
    if (!out) return E_POINTER;
    if (IsEqualGUID( iid, &IID_IUnknown ) || IsEqualGUID( iid, &IID_IInspectable ) ||
        IsEqualGUID( iid, &IID_IAgileObject ) || IsEqualGUID( iid, &IID_IUser ))
    {
        *out = iface;
        IUser_AddRef( iface );
        return S_OK;
    }
    *out = NULL;
    return E_NOINTERFACE;
}

static ULONG WINAPI user_AddRef( IUser *iface )
{
    return InterlockedIncrement( &impl_from_IUser( iface )->ref );
}

static ULONG WINAPI user_Release( IUser *iface )
{
    struct user *impl = impl_from_IUser( iface );
    ULONG ref = InterlockedDecrement( &impl->ref );

    if (!ref)
    {
        WindowsDeleteString( impl->id );
        free( impl );
    }
    return ref;
}

static HRESULT WINAPI user_GetIids( IUser *iface, ULONG *count, IID **iids )
{
    IID *values;

    if (!count || !iids) return E_POINTER;
    *count = 0;
    *iids = NULL;
    if (!(values = CoTaskMemAlloc( sizeof(*values) ))) return E_OUTOFMEMORY;
    values[0] = IID_IUser;
    *count = 1;
    *iids = values;
    return S_OK;
}

static HRESULT WINAPI user_GetRuntimeClassName( IUser *iface, HSTRING *name )
{
    if (!name) return E_POINTER;
    return WindowsCreateString( RuntimeClass_Windows_System_User,
                                ARRAY_SIZE(RuntimeClass_Windows_System_User) - 1, name );
}

static HRESULT WINAPI user_GetTrustLevel( IUser *iface, TrustLevel *level )
{
    if (!level) return E_POINTER;
    *level = BaseTrust;
    return S_OK;
}

static HRESULT WINAPI user_get_NonRoamableId( IUser *iface, HSTRING *value )
{
    if (!value) return E_POINTER;
    return WindowsDuplicateString( impl_from_IUser( iface )->id, value );
}

static HRESULT WINAPI user_get_AuthenticationStatus( IUser *iface, UserAuthenticationStatus *value )
{
    if (!value) return E_POINTER;
    *value = UserAuthenticationStatus_LocallyAuthenticated;
    return S_OK;
}

static HRESULT WINAPI user_get_Type( IUser *iface, UserType *value )
{
    if (!value) return E_POINTER;
    *value = UserType_LocalUser;
    return S_OK;
}

static HRESULT WINAPI user_GetPropertyAsync( IUser *iface, HSTRING value,
                                              IAsyncOperation_IInspectable **operation )
{
    TRACE( "iface %p, property %s, operation %p.\n", iface, debugstr_hstring( value ), operation );
    if (!operation) return E_POINTER;
    *operation = NULL;
    return E_NOTIMPL;
}

static HRESULT user_properties_async_callback( IUnknown *invoker, IUnknown *param,
                                               PROPVARIANT *result, BOOL async )
{
    result->vt = VT_UNKNOWN;
    result->punkVal = param;
    IUnknown_AddRef( param );
    return S_OK;
}

static HRESULT WINAPI user_GetPropertiesAsync( IUser *iface, __FIVectorView_1_HSTRING *values,
        IAsyncOperation_IPropertySet **operation )
{
    static const WCHAR account_property[] = L"System.UserAccountName";
    IPropertyValueStatics *property_value_statics = NULL;
    IMap_HSTRING_IInspectable *map = NULL;
    IInspectable *property_set = NULL, *boxed = NULL;
    HSTRING class_name = NULL, account = NULL, property = NULL;
    WCHAR username[UNLEN + 1];
    UINT32 count = 0, i;
    DWORD username_len = ARRAY_SIZE(username);
    boolean replaced;
    HRESULT hr;

    TRACE( "iface %p, properties %p, operation %p.\n", iface, values, operation );
    if (!values || !operation) return E_POINTER;
    *operation = NULL;

    if (FAILED(hr = WindowsCreateString( RuntimeClass_Windows_Foundation_Collections_PropertySet,
                                        ARRAY_SIZE(RuntimeClass_Windows_Foundation_Collections_PropertySet) - 1,
                                        &class_name ))) goto done;
    if (FAILED(hr = RoActivateInstance( class_name, &property_set ))) goto done;
    WindowsDeleteString( class_name );
    class_name = NULL;
    if (FAILED(hr = IInspectable_QueryInterface( property_set, &IID_IMap_HSTRING_IInspectable,
                                                (void **)&map ))) goto done;
    if (FAILED(hr = __FIVectorView_1_HSTRING_get_Size( values, &count ))) goto done;

    for (i = 0; i < count; ++i)
    {
        const WCHAR *raw;
        UINT32 length;

        if (FAILED(hr = __FIVectorView_1_HSTRING_GetAt( values, i, &property ))) goto done;
        raw = WindowsGetStringRawBuffer( property, &length );
        TRACE( "requested property %s.\n", debugstr_wn( raw, length ) );
        if (length != ARRAY_SIZE(account_property) - 1 ||
            wcsncmp( raw, account_property, length ))
        {
            WindowsDeleteString( property );
            property = NULL;
            continue;
        }

        if (!GetUserNameW( username, &username_len ))
        {
            hr = HRESULT_FROM_WIN32( GetLastError() );
            goto done;
        }
        if (FAILED(hr = WindowsCreateString( username, username_len - 1, &account ))) goto done;
        if (FAILED(hr = WindowsCreateString( RuntimeClass_Windows_Foundation_PropertyValue,
                                            ARRAY_SIZE(RuntimeClass_Windows_Foundation_PropertyValue) - 1,
                                            &class_name ))) goto done;
        if (FAILED(hr = RoGetActivationFactory( class_name, &IID_IPropertyValueStatics,
                                               (void **)&property_value_statics ))) goto done;
        if (FAILED(hr = IPropertyValueStatics_CreateString( property_value_statics, account, &boxed ))) goto done;
        if (FAILED(hr = IMap_HSTRING_IInspectable_Insert( map, property, boxed, &replaced ))) goto done;

        IInspectable_Release( boxed );
        boxed = NULL;
        IPropertyValueStatics_Release( property_value_statics );
        property_value_statics = NULL;
        WindowsDeleteString( class_name );
        class_name = NULL;
        WindowsDeleteString( account );
        account = NULL;
        WindowsDeleteString( property );
        property = NULL;
    }

    hr = async_operation_property_set_create( (IUnknown *)iface, (IUnknown *)property_set,
                                              user_properties_async_callback, operation );

done:
    if (boxed) IInspectable_Release( boxed );
    if (property_value_statics) IPropertyValueStatics_Release( property_value_statics );
    if (map) IMap_HSTRING_IInspectable_Release( map );
    if (property_set) IInspectable_Release( property_set );
    WindowsDeleteString( class_name );
    WindowsDeleteString( account );
    WindowsDeleteString( property );
    return hr;
}

static HRESULT WINAPI user_GetPictureAsync( IUser *iface, UserPictureSize size,
        IAsyncOperation_IRandomAccessStreamReference **operation )
{
    if (!operation) return E_POINTER;
    *operation = NULL;
    return E_NOTIMPL;
}

static const IUserVtbl user_vtbl =
{
    user_QueryInterface,
    user_AddRef,
    user_Release,
    user_GetIids,
    user_GetRuntimeClassName,
    user_GetTrustLevel,
    user_get_NonRoamableId,
    user_get_AuthenticationStatus,
    user_get_Type,
    user_GetPropertyAsync,
    user_GetPropertiesAsync,
    user_GetPictureAsync,
};

static HRESULT user_create( IUser **out )
{
    TOKEN_USER *token_user = NULL;
    struct user *impl = NULL;
    HANDLE token = NULL;
    WCHAR *sid = NULL;
    DWORD size = 0;
    HRESULT hr;

    if (!out) return E_POINTER;
    *out = NULL;
    if (!OpenProcessToken( GetCurrentProcess(), TOKEN_QUERY, &token ))
        return HRESULT_FROM_WIN32( GetLastError() );
    GetTokenInformation( token, TokenUser, NULL, 0, &size );
    if (GetLastError() != ERROR_INSUFFICIENT_BUFFER || !(token_user = malloc( size )))
    {
        hr = HRESULT_FROM_WIN32( GetLastError() );
        goto done;
    }
    if (!GetTokenInformation( token, TokenUser, token_user, size, &size ) ||
        !ConvertSidToStringSidW( token_user->User.Sid, &sid ))
    {
        hr = HRESULT_FROM_WIN32( GetLastError() );
        goto done;
    }
    if (!(impl = calloc( 1, sizeof(*impl) )))
    {
        hr = E_OUTOFMEMORY;
        goto done;
    }
    impl->IUser_iface.lpVtbl = &user_vtbl;
    impl->ref = 1;
    if (FAILED(hr = WindowsCreateString( sid, wcslen( sid ), &impl->id ))) goto done;
    *out = &impl->IUser_iface;
    impl = NULL;
    hr = S_OK;

done:
    if (impl) IUser_Release( &impl->IUser_iface );
    LocalFree( sid );
    free( token_user );
    CloseHandle( token );
    return hr;
}

struct user_statics
{
    IActivationFactory IActivationFactory_iface;
    IUserStatics2 IUserStatics2_iface;
    LONG ref;
};

static inline struct user_statics *user_statics_from_factory( IActivationFactory *iface )
{
    return CONTAINING_RECORD( iface, struct user_statics, IActivationFactory_iface );
}

static HRESULT WINAPI user_factory_QueryInterface( IActivationFactory *iface, REFIID iid, void **out )
{
    struct user_statics *impl = user_statics_from_factory( iface );

    if (!out) return E_POINTER;
    if (IsEqualGUID( iid, &IID_IUnknown ) || IsEqualGUID( iid, &IID_IInspectable ) ||
        IsEqualGUID( iid, &IID_IAgileObject ) || IsEqualGUID( iid, &IID_IActivationFactory ))
        *out = &impl->IActivationFactory_iface;
    else if (IsEqualGUID( iid, &IID_IUserStatics2 ))
        *out = &impl->IUserStatics2_iface;
    else
    {
        *out = NULL;
        return E_NOINTERFACE;
    }
    IInspectable_AddRef( *out );
    return S_OK;
}

static ULONG WINAPI user_factory_AddRef( IActivationFactory *iface )
{
    return InterlockedIncrement( &user_statics_from_factory( iface )->ref );
}

static ULONG WINAPI user_factory_Release( IActivationFactory *iface )
{
    return InterlockedDecrement( &user_statics_from_factory( iface )->ref );
}

static HRESULT WINAPI user_factory_GetIids( IActivationFactory *iface, ULONG *count, IID **iids )
{
    return E_NOTIMPL;
}

static HRESULT WINAPI user_factory_GetRuntimeClassName( IActivationFactory *iface, HSTRING *name )
{
    return E_NOTIMPL;
}

static HRESULT WINAPI user_factory_GetTrustLevel( IActivationFactory *iface, TrustLevel *level )
{
    return E_NOTIMPL;
}

static HRESULT WINAPI user_factory_ActivateInstance( IActivationFactory *iface, IInspectable **instance )
{
    return E_NOTIMPL;
}

static const IActivationFactoryVtbl user_factory_vtbl =
{
    user_factory_QueryInterface,
    user_factory_AddRef,
    user_factory_Release,
    user_factory_GetIids,
    user_factory_GetRuntimeClassName,
    user_factory_GetTrustLevel,
    user_factory_ActivateInstance,
};

DEFINE_IINSPECTABLE( user_statics2, IUserStatics2, struct user_statics, IActivationFactory_iface )

static HRESULT WINAPI user_statics2_GetDefault( IUserStatics2 *iface, IUser **value )
{
    return user_create( value );
}

static const IUserStatics2Vtbl user_statics2_vtbl =
{
    user_statics2_QueryInterface,
    user_statics2_AddRef,
    user_statics2_Release,
    user_statics2_GetIids,
    user_statics2_GetRuntimeClassName,
    user_statics2_GetTrustLevel,
    user_statics2_GetDefault,
};

static struct user_statics user_statics =
{
    {&user_factory_vtbl},
    {&user_statics2_vtbl},
    1,
};

struct known_user_properties_statics
{
    IActivationFactory IActivationFactory_iface;
    IKnownUserPropertiesStatics IKnownUserPropertiesStatics_iface;
    LONG ref;
};

static inline struct known_user_properties_statics *known_properties_from_factory( IActivationFactory *iface )
{
    return CONTAINING_RECORD( iface, struct known_user_properties_statics, IActivationFactory_iface );
}

static HRESULT WINAPI known_properties_factory_QueryInterface( IActivationFactory *iface,
                                                                REFIID iid, void **out )
{
    struct known_user_properties_statics *impl = known_properties_from_factory( iface );

    if (!out) return E_POINTER;
    if (IsEqualGUID( iid, &IID_IUnknown ) || IsEqualGUID( iid, &IID_IInspectable ) ||
        IsEqualGUID( iid, &IID_IAgileObject ) || IsEqualGUID( iid, &IID_IActivationFactory ))
        *out = &impl->IActivationFactory_iface;
    else if (IsEqualGUID( iid, &IID_IKnownUserPropertiesStatics ))
        *out = &impl->IKnownUserPropertiesStatics_iface;
    else
    {
        *out = NULL;
        return E_NOINTERFACE;
    }
    IInspectable_AddRef( *out );
    return S_OK;
}

static ULONG WINAPI known_properties_factory_AddRef( IActivationFactory *iface )
{
    return InterlockedIncrement( &known_properties_from_factory( iface )->ref );
}

static ULONG WINAPI known_properties_factory_Release( IActivationFactory *iface )
{
    return InterlockedDecrement( &known_properties_from_factory( iface )->ref );
}

static HRESULT WINAPI known_properties_factory_GetIids( IActivationFactory *iface,
                                                         ULONG *count, IID **iids )
{
    return E_NOTIMPL;
}

static HRESULT WINAPI known_properties_factory_GetRuntimeClassName( IActivationFactory *iface,
                                                                    HSTRING *name )
{
    return E_NOTIMPL;
}

static HRESULT WINAPI known_properties_factory_GetTrustLevel( IActivationFactory *iface,
                                                               TrustLevel *level )
{
    return E_NOTIMPL;
}

static HRESULT WINAPI known_properties_factory_ActivateInstance( IActivationFactory *iface,
                                                                  IInspectable **instance )
{
    return E_NOTIMPL;
}

static const IActivationFactoryVtbl known_properties_factory_vtbl =
{
    known_properties_factory_QueryInterface,
    known_properties_factory_AddRef,
    known_properties_factory_Release,
    known_properties_factory_GetIids,
    known_properties_factory_GetRuntimeClassName,
    known_properties_factory_GetTrustLevel,
    known_properties_factory_ActivateInstance,
};

DEFINE_IINSPECTABLE( known_properties, IKnownUserPropertiesStatics,
                     struct known_user_properties_statics, IActivationFactory_iface )

static HRESULT known_properties_string( const WCHAR *name, HSTRING *value )
{
    if (!value) return E_POINTER;
    TRACE( "property %s, value %p.\n", debugstr_w( name ), value );
    return WindowsCreateString( name, wcslen( name ), value );
}

#define WIDEN_LITERAL_(value) L##value
#define WIDEN_LITERAL(value) WIDEN_LITERAL_(value)
#define DEFINE_KNOWN_USER_PROPERTY(name) \
    static HRESULT WINAPI known_properties_get_##name( IKnownUserPropertiesStatics *iface, HSTRING *value ) \
    { \
        return known_properties_string( L"System.User" WIDEN_LITERAL(#name), value ); \
    }

DEFINE_KNOWN_USER_PROPERTY(DisplayName)
DEFINE_KNOWN_USER_PROPERTY(FirstName)
DEFINE_KNOWN_USER_PROPERTY(LastName)
DEFINE_KNOWN_USER_PROPERTY(ProviderName)
DEFINE_KNOWN_USER_PROPERTY(AccountName)
DEFINE_KNOWN_USER_PROPERTY(GuestHost)
DEFINE_KNOWN_USER_PROPERTY(PrincipalName)
DEFINE_KNOWN_USER_PROPERTY(DomainName)
DEFINE_KNOWN_USER_PROPERTY(SessionInitiationProtocolUri)

static const IKnownUserPropertiesStaticsVtbl known_properties_vtbl =
{
    known_properties_QueryInterface,
    known_properties_AddRef,
    known_properties_Release,
    known_properties_GetIids,
    known_properties_GetRuntimeClassName,
    known_properties_GetTrustLevel,
    known_properties_get_DisplayName,
    known_properties_get_FirstName,
    known_properties_get_LastName,
    known_properties_get_ProviderName,
    known_properties_get_AccountName,
    known_properties_get_GuestHost,
    known_properties_get_PrincipalName,
    known_properties_get_DomainName,
    known_properties_get_SessionInitiationProtocolUri,
};

static struct known_user_properties_statics known_properties_statics =
{
    {&known_properties_factory_vtbl},
    {&known_properties_vtbl},
    1,
};

HRESULT WINAPI DllGetActivationFactory( HSTRING classid, IActivationFactory **factory )
{
    const WCHAR *name = WindowsGetStringRawBuffer( classid, NULL );

    TRACE( "classid %s, factory %p.\n", debugstr_hstring( classid ), factory );

    *factory = NULL;

    if (!wcscmp( name, RuntimeClass_Windows_System_DispatcherQueue ))
        IActivationFactory_QueryInterface( &dispatcher_queue_statics_impl.IActivationFactory_iface,
                                           &IID_IActivationFactory, (void **)factory );
    if (!wcscmp( name, RuntimeClass_Windows_System_DispatcherQueueController ))
        IActivationFactory_QueryInterface( dispatcher_queue_controller_factory, &IID_IActivationFactory, (void **)factory );
    if (!wcscmp( name, RuntimeClass_Windows_System_User ))
        IActivationFactory_QueryInterface( &user_statics.IActivationFactory_iface,
                                           &IID_IActivationFactory, (void **)factory );
    if (!wcscmp( name, RuntimeClass_Windows_System_KnownUserProperties ))
        IActivationFactory_QueryInterface( &known_properties_statics.IActivationFactory_iface,
                                           &IID_IActivationFactory, (void **)factory );

    if (*factory) return S_OK;
    return CLASS_E_CLASSNOTAVAILABLE;
}

HRESULT WINAPI CreateDispatcherQueueController( DispatcherQueueOptions options, PDISPATCHERQUEUECONTROLLER *queue_controller )
{
    struct dispatcher_queue_controller *impl;

    FIXME( "options.dwSize = %lu, options.threadType = %d, options.apartmentType = %d, queue_controller %p semi-stub!\n",
            options.dwSize, options.threadType, options.apartmentType, queue_controller );

    if (!queue_controller) return E_POINTER;
    if (options.dwSize != sizeof( DispatcherQueueOptions )) return E_INVALIDARG;
    if (options.threadType != DQTYPE_THREAD_DEDICATED && options.threadType != DQTYPE_THREAD_CURRENT) return E_INVALIDARG;
    if (!(impl = calloc( 1, sizeof( *impl ) ))) return E_OUTOFMEMORY;

    impl->IDispatcherQueueController_iface.lpVtbl = &dispatcher_queue_controller_vtbl;
    impl->ref = 1;

    *queue_controller = &impl->IDispatcherQueueController_iface;
    TRACE( "created IDispatcherQueueController %p.\n", *queue_controller );
    return S_OK;
}
