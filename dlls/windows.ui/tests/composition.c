/*
 * Copyright (C) 2026 Mirko Brombin
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

#define COBJMACROS
#include <stdarg.h>

#include "windef.h"
#include "winbase.h"
#include "winstring.h"

#include "roapi.h"

#define WIDL_using_Windows_Foundation
#define WIDL_using_Windows_Foundation_Collections
#include "windows.foundation.h"
#define WIDL_using_Windows_Graphics_Effects
#include "windows.graphics.effects.h"
#define WIDL_using_Windows_Foundation_Numerics
#include "windows.foundation.numerics.h"
#define WIDL_using_Windows_Graphics
#include "windows.graphics.h"
#define WIDL_using_Windows_UI
#include "windows.ui.h"
#define WIDL_using_Windows_UI_Core
#define WIDL_using_Windows_UI_Composition
#include "windows.ui.composition.h"
#include "windows.ui.composition.interop.h"
#define WIDL_using_Windows_UI_Composition_Core
#include "windows.ui.composition.core.h"
#define WIDL_using_Windows_UI_Composition_Interactions
#include "windows.ui.composition.interactions.h"

#include "wine/test.h"

static const GUID compositor4_iid =
    {0xae47e78a, 0x7910, 0x4425, {0xa4, 0x82, 0xa0, 0x5b, 0x75, 0x8a, 0xdc, 0xe9}};
static const GUID compositor5_iid =
    {0x48ea31ad, 0x7fcd, 0x4076, {0xa7, 0x9c, 0x90, 0xcc, 0x4b, 0x85, 0x2c, 0x9b}};
static const GUID compositor_controller_iid =
    {0x2d75f35a, 0x70a7, 0x4395, {0xba, 0x2d, 0xce, 0xf0, 0xb1, 0x83, 0x99, 0xf9}};
static const GUID compositor_interop_iid =
    {0x25297d5c, 0x3ad4, 0x4c9c, {0xb5, 0xcf, 0xe3, 0x6a, 0x38, 0x51, 0x23, 0x30}};
static const GUID interaction_tracker_statics_iid =
    {0xbba5d7b7, 0x6590, 0x4498, {0x8d, 0x6c, 0xeb, 0x62, 0xb5, 0x14, 0xc9, 0x2a}};
static const GUID interaction_tracker_statics2_iid =
    {0x35e53720, 0x46b7, 0x5cb0, {0xb5, 0x05, 0xf3, 0xd6, 0x88, 0x4a, 0x61, 0x63}};

struct compositor5_iface;

struct compositor5_vtbl
{
    HRESULT (WINAPI *QueryInterface)( struct compositor5_iface *iface, REFIID iid, void **out );
    ULONG (WINAPI *AddRef)( struct compositor5_iface *iface );
    ULONG (WINAPI *Release)( struct compositor5_iface *iface );
    HRESULT (WINAPI *GetIids)( struct compositor5_iface *iface, ULONG *count, IID **iids );
    HRESULT (WINAPI *GetRuntimeClassName)( struct compositor5_iface *iface, HSTRING *name );
    HRESULT (WINAPI *GetTrustLevel)( struct compositor5_iface *iface, TrustLevel *level );
    HRESULT (WINAPI *get_Comment)( struct compositor5_iface *iface, HSTRING *value );
    HRESULT (WINAPI *put_Comment)( struct compositor5_iface *iface, HSTRING value );
    HRESULT (WINAPI *get_GlobalPlaybackRate)( struct compositor5_iface *iface, FLOAT *value );
    HRESULT (WINAPI *put_GlobalPlaybackRate)( struct compositor5_iface *iface, FLOAT value );
    HRESULT (WINAPI *CreateBounceScalarAnimation)( struct compositor5_iface *iface, IInspectable **result );
    HRESULT (WINAPI *CreateBounceVector2Animation)( struct compositor5_iface *iface, IInspectable **result );
    HRESULT (WINAPI *CreateBounceVector3Animation)( struct compositor5_iface *iface, IInspectable **result );
    HRESULT (WINAPI *CreateContainerShape)( struct compositor5_iface *iface, IInspectable **result );
    HRESULT (WINAPI *CreateEllipseGeometry)( struct compositor5_iface *iface, IInspectable **result );
    HRESULT (WINAPI *CreateLineGeometry)( struct compositor5_iface *iface, IInspectable **result );
    HRESULT (WINAPI *CreatePathGeometry)( struct compositor5_iface *iface, IInspectable **result );
    HRESULT (WINAPI *CreatePathGeometryWithPath)( struct compositor5_iface *iface,
            IInspectable *path, IInspectable **result );
    HRESULT (WINAPI *CreatePathKeyFrameAnimation)( struct compositor5_iface *iface, IInspectable **result );
    HRESULT (WINAPI *CreateRectangleGeometry)( struct compositor5_iface *iface, IInspectable **result );
    HRESULT (WINAPI *CreateRoundedRectangleGeometry)( struct compositor5_iface *iface, IInspectable **result );
    HRESULT (WINAPI *CreateShapeVisual)( struct compositor5_iface *iface, IInspectable **result );
    HRESULT (WINAPI *CreateSpriteShape)( struct compositor5_iface *iface, IInspectable **result );
    HRESULT (WINAPI *CreateSpriteShapeWithGeometry)( struct compositor5_iface *iface,
            IInspectable *geometry, IInspectable **result );
    HRESULT (WINAPI *CreateViewBox)( struct compositor5_iface *iface, IInspectable **result );
    HRESULT (WINAPI *RequestCommitAsync)( struct compositor5_iface *iface, IAsyncAction **operation );
};

struct compositor5_iface
{
    const struct compositor5_vtbl *lpVtbl;
};

static ICompositor *create_compositor(void)
{
    IInspectable *inspectable;
    ICompositor *compositor;
    HSTRING class_name;
    HRESULT hr;

    hr = WindowsCreateString( RuntimeClass_Windows_UI_Composition_Compositor,
                              ARRAY_SIZE(RuntimeClass_Windows_UI_Composition_Compositor) - 1,
                              &class_name );
    ok( hr == S_OK, "Got hr %#lx.\n", hr );
    hr = RoActivateInstance( class_name, &inspectable );
    ok( hr == S_OK, "Got hr %#lx.\n", hr );
    WindowsDeleteString( class_name );
    if (FAILED(hr)) return NULL;

    hr = IInspectable_QueryInterface( inspectable, &IID_ICompositor, (void **)&compositor );
    ok( hr == S_OK, "Got hr %#lx.\n", hr );
    IInspectable_Release( inspectable );
    return SUCCEEDED(hr) ? compositor : NULL;
}

static void test_compositor_interfaces(void)
{
    struct compositor5_iface *compositor5 = NULL;
    IAsyncAction *action = NULL;
    ICompositor *compositor;
    IUnknown *unknown;
    HSTRING comment = NULL, value = NULL;
    FLOAT rate = 0.0f;
    HRESULT hr;

    if (!(compositor = create_compositor())) return;
    hr = ICompositor_QueryInterface( compositor, &compositor4_iid, (void **)&unknown );
    ok( hr == S_OK, "Got hr %#lx.\n", hr );
    if (SUCCEEDED(hr)) IUnknown_Release( unknown );

    hr = ICompositor_QueryInterface( compositor, &compositor5_iid, (void **)&compositor5 );
    ok( hr == S_OK, "Got hr %#lx.\n", hr );
    if (FAILED(hr)) goto done;

    hr = compositor5->lpVtbl->get_GlobalPlaybackRate( compositor5, &rate );
    ok( hr == S_OK, "Got hr %#lx.\n", hr );
    ok( rate == 1.0f, "Got rate %f.\n", rate );
    hr = compositor5->lpVtbl->put_GlobalPlaybackRate( compositor5, 0.5f );
    ok( hr == S_OK, "Got hr %#lx.\n", hr );
    hr = compositor5->lpVtbl->get_GlobalPlaybackRate( compositor5, &rate );
    ok( hr == S_OK, "Got hr %#lx.\n", hr );
    ok( rate == 0.5f, "Got rate %f.\n", rate );

    hr = WindowsCreateString( L"Office", 6, &comment );
    ok( hr == S_OK, "Got hr %#lx.\n", hr );
    hr = compositor5->lpVtbl->put_Comment( compositor5, comment );
    ok( hr == S_OK, "Got hr %#lx.\n", hr );
    hr = compositor5->lpVtbl->get_Comment( compositor5, &value );
    ok( hr == S_OK, "Got hr %#lx.\n", hr );
    ok( value && !wcscmp( WindowsGetStringRawBuffer( value, NULL ), L"Office" ),
        "Got comment %s.\n", wine_dbgstr_w( WindowsGetStringRawBuffer( value, NULL ) ) );

    hr = compositor5->lpVtbl->RequestCommitAsync( compositor5, &action );
    ok( hr == S_OK, "Got hr %#lx.\n", hr );
    ok( action != NULL, "Got action %p.\n", action );
    if (action)
    {
        hr = IAsyncAction_GetResults( action );
        ok( hr == S_OK, "Got hr %#lx.\n", hr );
    }

done:
    if (action) IAsyncAction_Release( action );
    WindowsDeleteString( value );
    WindowsDeleteString( comment );
    if (compositor5) compositor5->lpVtbl->Release( compositor5 );
    ICompositor_Release( compositor );
}

static void test_composition_graphics_device2(void)
{
    struct SizeInt32 size = {640, 480};
    ICompositionVirtualDrawingSurface *virtual_surface = (void *)0xdeadbeef;
    ICompositionDrawingSurface *surface = (void *)0xdeadbeef;
    ICompositionGraphicsDevice2 *device2 = NULL;
    ICompositionGraphicsDevice *device = NULL;
    ICompositorInterop *interop = NULL;
    ICompositor *compositor;
    IID *iids = NULL;
    ULONG count = 0;
    HRESULT hr;

    if (!(compositor = create_compositor())) return;
    hr = ICompositor_QueryInterface( compositor, &compositor_interop_iid, (void **)&interop );
    ok( hr == S_OK, "Got hr %#lx.\n", hr );
    if (FAILED(hr)) goto done;

    hr = ICompositorInterop_CreateGraphicsDevice( interop, (IUnknown *)compositor, &device );
    ok( hr == S_OK, "Got hr %#lx.\n", hr );
    if (FAILED(hr)) goto done;

    hr = ICompositionGraphicsDevice_QueryInterface( device, &IID_ICompositionGraphicsDevice2,
            (void **)&device2 );
    ok( hr == S_OK, "Got hr %#lx.\n", hr );
    if (FAILED(hr)) goto done;

    hr = ICompositionGraphicsDevice2_GetIids( device2, &count, &iids );
    ok( hr == S_OK, "Got hr %#lx.\n", hr );
    ok( count == 2, "Got count %lu.\n", count );
    ok( IsEqualGUID( &iids[0], &IID_ICompositionGraphicsDevice ), "Unexpected first IID.\n" );
    ok( IsEqualGUID( &iids[1], &IID_ICompositionGraphicsDevice2 ), "Unexpected second IID.\n" );

    hr = ICompositionGraphicsDevice2_CreateDrawingSurface2( device2, size,
            DirectXPixelFormat_B8G8R8A8UIntNormalized, DirectXAlphaMode_Premultiplied, &surface );
    ok( hr == E_NOINTERFACE, "Got hr %#lx.\n", hr );
    ok( surface == NULL, "Got surface %p.\n", surface );

    hr = ICompositionGraphicsDevice2_CreateVirtualDrawingSurface( device2, size,
            DirectXPixelFormat_B8G8R8A8UIntNormalized, DirectXAlphaMode_Premultiplied,
            &virtual_surface );
    ok( hr == E_NOINTERFACE, "Got hr %#lx.\n", hr );
    ok( virtual_surface == NULL, "Got surface %p.\n", virtual_surface );

done:
    CoTaskMemFree( iids );
    if (device2) ICompositionGraphicsDevice2_Release( device2 );
    if (device) ICompositionGraphicsDevice_Release( device );
    if (interop) ICompositorInterop_Release( interop );
    ICompositor_Release( compositor );
}

struct async_action_handler
{
    IAsyncActionCompletedHandler IAsyncActionCompletedHandler_iface;
    LONG ref;
    IAsyncAction *sender;
    AsyncStatus status;
    LONG invoked;
};

static inline struct async_action_handler *impl_from_IAsyncActionCompletedHandler(
        IAsyncActionCompletedHandler *iface )
{
    return CONTAINING_RECORD( iface, struct async_action_handler,
                              IAsyncActionCompletedHandler_iface );
}

static HRESULT WINAPI async_action_handler_QueryInterface( IAsyncActionCompletedHandler *iface,
        REFIID iid, void **out )
{
    if (!out) return E_POINTER;
    *out = NULL;
    if (!IsEqualGUID( iid, &IID_IUnknown ) &&
        !IsEqualGUID( iid, &IID_IAsyncActionCompletedHandler ))
        return E_NOINTERFACE;
    *out = iface;
    IAsyncActionCompletedHandler_AddRef( iface );
    return S_OK;
}

static ULONG WINAPI async_action_handler_AddRef( IAsyncActionCompletedHandler *iface )
{
    return InterlockedIncrement( &impl_from_IAsyncActionCompletedHandler( iface )->ref );
}

static ULONG WINAPI async_action_handler_Release( IAsyncActionCompletedHandler *iface )
{
    return InterlockedDecrement( &impl_from_IAsyncActionCompletedHandler( iface )->ref );
}

static HRESULT WINAPI async_action_handler_Invoke( IAsyncActionCompletedHandler *iface,
        IAsyncAction *sender, AsyncStatus status )
{
    struct async_action_handler *handler = impl_from_IAsyncActionCompletedHandler( iface );

    handler->sender = sender;
    handler->status = status;
    InterlockedIncrement( &handler->invoked );
    return S_OK;
}

static const IAsyncActionCompletedHandlerVtbl async_action_handler_vtbl =
{
    async_action_handler_QueryInterface,
    async_action_handler_AddRef,
    async_action_handler_Release,
    async_action_handler_Invoke,
};

static void test_compositor_controller_async(void)
{
    struct async_action_handler handler = {{&async_action_handler_vtbl}, 1};
    IAsyncActionCompletedHandler *completed_handler = NULL;
    ICompositorController *controller = NULL;
    IInspectable *inspectable = NULL;
    IAsyncAction *action = NULL;
    IAsyncInfo *info = NULL;
    AsyncStatus status = Started;
    HSTRING class_name;
    HRESULT hr;

    hr = WindowsCreateString( RuntimeClass_Windows_UI_Composition_Core_CompositorController,
                              ARRAY_SIZE(RuntimeClass_Windows_UI_Composition_Core_CompositorController) - 1,
                              &class_name );
    ok( hr == S_OK, "Got hr %#lx.\n", hr );
    hr = RoActivateInstance( class_name, &inspectable );
    ok( hr == S_OK, "Got hr %#lx.\n", hr );
    WindowsDeleteString( class_name );
    if (FAILED(hr)) return;

    hr = IInspectable_QueryInterface( inspectable, &compositor_controller_iid, (void **)&controller );
    ok( hr == S_OK, "Got hr %#lx.\n", hr );
    if (FAILED(hr)) goto done;

    hr = ICompositorController_EnsurePreviousCommitCompletedAsync( controller, &action );
    ok( hr == S_OK, "Got hr %#lx.\n", hr );
    ok( action != NULL, "Got action %p.\n", action );
    if (FAILED(hr) || !action) goto done;

    hr = IAsyncAction_QueryInterface( action, &IID_IAsyncInfo, (void **)&info );
    ok( hr == S_OK, "Got hr %#lx.\n", hr );
    hr = IAsyncInfo_get_Status( info, &status );
    ok( hr == S_OK && status == Completed, "Got hr %#lx, status %u.\n", hr, status );
    hr = IAsyncAction_GetResults( action );
    ok( hr == S_OK, "Got hr %#lx.\n", hr );

    hr = IAsyncAction_put_Completed( action, &handler.IAsyncActionCompletedHandler_iface );
    ok( hr == S_OK, "Got hr %#lx.\n", hr );
    ok( handler.invoked == 1, "Handler invoked %ld times.\n", handler.invoked );
    ok( handler.sender == action, "Got sender %p.\n", handler.sender );
    ok( handler.status == Completed, "Got status %u.\n", handler.status );

    hr = IAsyncAction_get_Completed( action, &completed_handler );
    ok( hr == S_OK, "Got hr %#lx.\n", hr );
    ok( completed_handler == &handler.IAsyncActionCompletedHandler_iface,
        "Got handler %p.\n", completed_handler );
    if (completed_handler) IAsyncActionCompletedHandler_Release( completed_handler );

    hr = IAsyncInfo_Close( info );
    ok( hr == S_OK, "Got hr %#lx.\n", hr );
    ok( handler.ref == 1, "Got ref %ld.\n", handler.ref );

done:
    if (info) IAsyncInfo_Release( info );
    if (action) IAsyncAction_Release( action );
    if (controller) ICompositorController_Release( controller );
    IInspectable_Release( inspectable );
}

static void test_compositor_animation(void)
{
    ICubicBezierEasingFunction *cubic_easing = NULL;
    ILinearEasingFunction *linear_easing = NULL;
    ICompositionEasingFunction *easing = NULL;
    ICompositionScopedBatch *batch = NULL;
    IScalarKeyFrameAnimation *scalar = NULL;
    ICompositionAnimation *animation = NULL;
    ICompositionObject *animation_object = NULL;
    ICompositionObject *clip_object = NULL;
    ICompositionObject *object = NULL;
    IInsetClip *clip = NULL;
    ISpriteVisual *sprite = NULL;
    IVisual *visual = NULL;
    ICompositor *compositor;
    ICompositor *animation_compositor = NULL;
    ICompositor *clip_compositor = NULL;
    struct Vector2 point1 = {0.25f, 0.1f}, point2 = {0.25f, 1.0f};
    HSTRING property = NULL;
    boolean value;
    FLOAT opacity;
    HRESULT hr;

    if (!(compositor = create_compositor())) return;
    hr = ICompositor_CreateLinearEasingFunction( compositor, &linear_easing );
    ok( hr == S_OK, "Got hr %#lx.\n", hr );
    hr = ICompositor_CreateCubicBezierEasingFunction( compositor, point1, point2, &cubic_easing );
    ok( hr == S_OK, "Got hr %#lx.\n", hr );
    hr = ILinearEasingFunction_QueryInterface( linear_easing, &IID_ICompositionEasingFunction,
                                                (void **)&easing );
    ok( hr == S_OK, "Got hr %#lx.\n", hr );

    hr = ICompositor_CreateScopedBatch( compositor, CompositionBatchTypes_Animation, &batch );
    ok( hr == S_OK, "Got hr %#lx.\n", hr );
    hr = ICompositionScopedBatch_get_IsActive( batch, &value );
    ok( hr == S_OK && value, "Got hr %#lx, value %u.\n", hr, value );
    hr = ICompositionScopedBatch_get_IsEnded( batch, &value );
    ok( hr == S_OK && !value, "Got hr %#lx, value %u.\n", hr, value );
    hr = ICompositionScopedBatch_Suspend( batch );
    ok( hr == S_OK, "Got hr %#lx.\n", hr );
    hr = ICompositionScopedBatch_get_IsActive( batch, &value );
    ok( hr == S_OK && !value, "Got hr %#lx, value %u.\n", hr, value );
    hr = ICompositionScopedBatch_Resume( batch );
    ok( hr == S_OK, "Got hr %#lx.\n", hr );
    hr = ICompositionScopedBatch_End( batch );
    ok( hr == S_OK, "Got hr %#lx.\n", hr );
    hr = ICompositionScopedBatch_get_IsEnded( batch, &value );
    ok( hr == S_OK && value, "Got hr %#lx, value %u.\n", hr, value );
    hr = ICompositionScopedBatch_End( batch );
    ok( hr == E_ILLEGAL_METHOD_CALL, "Got hr %#lx.\n", hr );

    hr = ICompositor_CreateSpriteVisual( compositor, &sprite );
    ok( hr == S_OK, "Got hr %#lx.\n", hr );
    hr = ISpriteVisual_QueryInterface( sprite, &IID_IVisual, (void **)&visual );
    ok( hr == S_OK, "Got hr %#lx.\n", hr );
    hr = ISpriteVisual_QueryInterface( sprite, &IID_ICompositionObject, (void **)&object );
    ok( hr == S_OK, "Got hr %#lx.\n", hr );
    hr = ICompositor_CreateInsetClip( compositor, &clip );
    ok( hr == S_OK, "Got hr %#lx.\n", hr );
    hr = IInsetClip_QueryInterface( clip, &IID_ICompositionObject, (void **)&clip_object );
    ok( hr == S_OK, "Got hr %#lx.\n", hr );
    hr = ICompositionObject_get_Compositor( clip_object, &clip_compositor );
    ok( hr == S_OK && clip_compositor == compositor,
        "Got hr %#lx, compositor %p.\n", hr, clip_compositor );
    hr = ICompositor_CreateScalarKeyFrameAnimation( compositor, &scalar );
    ok( hr == S_OK, "Got hr %#lx.\n", hr );
    hr = IScalarKeyFrameAnimation_QueryInterface( scalar, &IID_ICompositionObject,
                                                   (void **)&animation_object );
    ok( hr == S_OK, "Got hr %#lx.\n", hr );
    hr = ICompositionObject_get_Compositor( animation_object, &animation_compositor );
    ok( hr == S_OK && animation_compositor == compositor,
        "Got hr %#lx, compositor %p.\n", hr, animation_compositor );
    hr = IScalarKeyFrameAnimation_InsertKeyFrame( scalar, 0.0f, 0.75f );
    ok( hr == S_OK, "Got hr %#lx.\n", hr );
    hr = IScalarKeyFrameAnimation_InsertKeyFrameWithEasingFunction( scalar, 1.0f, 0.25f,
                                                                    easing );
    ok( hr == S_OK, "Got hr %#lx.\n", hr );
    hr = IScalarKeyFrameAnimation_QueryInterface( scalar, &IID_ICompositionAnimation,
                                                   (void **)&animation );
    ok( hr == S_OK, "Got hr %#lx.\n", hr );
    hr = WindowsCreateString( L"Opacity", 7, &property );
    ok( hr == S_OK, "Got hr %#lx.\n", hr );
    hr = ICompositionObject_StartAnimation( object, property, animation );
    ok( hr == S_OK, "Got hr %#lx.\n", hr );
    hr = IVisual_get_Opacity( visual, &opacity );
    ok( hr == S_OK && opacity == 0.25f, "Got hr %#lx, opacity %f.\n", hr, opacity );

    WindowsDeleteString( property );
    if (clip_compositor) ICompositor_Release( clip_compositor );
    if (clip_object) ICompositionObject_Release( clip_object );
    if (clip) IInsetClip_Release( clip );
    if (animation_compositor) ICompositor_Release( animation_compositor );
    if (animation_object) ICompositionObject_Release( animation_object );
    ICompositionAnimation_Release( animation );
    IScalarKeyFrameAnimation_Release( scalar );
    ICompositionObject_Release( object );
    IVisual_Release( visual );
    ISpriteVisual_Release( sprite );
    ICompositionScopedBatch_Release( batch );
    ICompositionEasingFunction_Release( easing );
    ICubicBezierEasingFunction_Release( cubic_easing );
    ILinearEasingFunction_Release( linear_easing );
    ICompositor_Release( compositor );
}

static void test_vector3_animation(void)
{
    IVector3KeyFrameAnimation *vector = NULL;
    IScalarKeyFrameAnimation *scalar = (void *)0xdeadbeef;
    ICompositionAnimation *animation = NULL;
    ICompositionObject *object = NULL;
    ISpriteVisual *sprite = NULL;
    IVisual *visual = NULL;
    ICompositor *compositor;
    struct Vector3 first = {0.0f, 0.0f, 0.0f}, final = {30.0f, 40.0f, 5.0f}, value;
    HSTRING property = NULL;
    HRESULT hr;

    if (!(compositor = create_compositor())) return;
    hr = ICompositor_CreateVector3KeyFrameAnimation( compositor, NULL );
    ok( hr == E_POINTER, "Got hr %#lx.\n", hr );
    hr = ICompositor_CreateVector3KeyFrameAnimation( compositor, &vector );
    ok( hr == S_OK && vector, "Got hr %#lx, vector %p.\n", hr, vector );
    if (FAILED(hr) || !vector) goto done;
    hr = IVector3KeyFrameAnimation_QueryInterface( vector, &IID_IScalarKeyFrameAnimation,
                                                   (void **)&scalar );
    ok( hr == E_NOINTERFACE && !scalar, "Got hr %#lx, scalar %p.\n", hr, scalar );
    hr = IVector3KeyFrameAnimation_InsertKeyFrame( vector, -0.1f, first );
    ok( hr == E_INVALIDARG, "Got hr %#lx.\n", hr );
    hr = IVector3KeyFrameAnimation_InsertKeyFrame( vector, 1.1f, first );
    ok( hr == E_INVALIDARG, "Got hr %#lx.\n", hr );
    hr = IVector3KeyFrameAnimation_InsertKeyFrameWithEasingFunction( vector, 1.0f, final, NULL );
    ok( hr == S_OK, "Got hr %#lx.\n", hr );
    hr = IVector3KeyFrameAnimation_InsertKeyFrame( vector, 0.0f, first );
    ok( hr == S_OK, "Got hr %#lx.\n", hr );
    hr = IVector3KeyFrameAnimation_QueryInterface( vector, &IID_ICompositionAnimation,
                                                   (void **)&animation );
    ok( hr == S_OK, "Got hr %#lx.\n", hr );
    hr = ICompositor_CreateSpriteVisual( compositor, &sprite );
    ok( hr == S_OK, "Got hr %#lx.\n", hr );
    hr = ISpriteVisual_QueryInterface( sprite, &IID_IVisual, (void **)&visual );
    ok( hr == S_OK, "Got hr %#lx.\n", hr );
    hr = ISpriteVisual_QueryInterface( sprite, &IID_ICompositionObject, (void **)&object );
    ok( hr == S_OK, "Got hr %#lx.\n", hr );
    hr = WindowsCreateString( L"Offset", 6, &property );
    ok( hr == S_OK, "Got hr %#lx.\n", hr );
    hr = ICompositionObject_StartAnimation( object, property, animation );
    ok( hr == S_OK, "Got hr %#lx.\n", hr );
    hr = IVisual_get_Offset( visual, &value );
    ok( hr == S_OK && value.X == final.X && value.Y == final.Y && value.Z == final.Z,
        "Got hr %#lx, offset {%f, %f, %f}.\n", hr, value.X, value.Y, value.Z );

done:
    WindowsDeleteString( property );
    if (object) ICompositionObject_Release( object );
    if (visual) IVisual_Release( visual );
    if (sprite) ISpriteVisual_Release( sprite );
    if (animation) ICompositionAnimation_Release( animation );
    if (vector) IVector3KeyFrameAnimation_Release( vector );
    ICompositor_Release( compositor );
}

static void test_visual_collection_iterable(void)
{
    IVisual *child1 = NULL, *child2 = NULL, *current = NULL, *items[2] = {0};
    ISpriteVisual *sprite1 = NULL, *sprite2 = NULL;
    IContainerVisual *container = NULL;
    IVisualCollection *collection = NULL;
    IIterable_Visual *iterable = NULL;
    IIterator_Visual *iterator = NULL;
    ICompositor *compositor;
    boolean has_current;
    UINT32 count;
    INT32 size;
    HRESULT hr;

    if (!(compositor = create_compositor())) return;
    hr = ICompositor_CreateContainerVisual( compositor, &container );
    ok( hr == S_OK, "Got hr %#lx.\n", hr );
    hr = ICompositor_CreateSpriteVisual( compositor, &sprite1 );
    ok( hr == S_OK, "Got hr %#lx.\n", hr );
    hr = ICompositor_CreateSpriteVisual( compositor, &sprite2 );
    ok( hr == S_OK, "Got hr %#lx.\n", hr );
    if (FAILED(hr) || !container || !sprite1 || !sprite2) goto done;

    hr = ISpriteVisual_QueryInterface( sprite1, &IID_IVisual, (void **)&child1 );
    ok( hr == S_OK, "Got hr %#lx.\n", hr );
    hr = ISpriteVisual_QueryInterface( sprite2, &IID_IVisual, (void **)&child2 );
    ok( hr == S_OK, "Got hr %#lx.\n", hr );
    hr = IContainerVisual_get_Children( container, &collection );
    ok( hr == S_OK, "Got hr %#lx.\n", hr );
    if (FAILED(hr) || !child1 || !child2) goto done;

    hr = IVisualCollection_QueryInterface( collection, &IID_IIterable_Visual, (void **)&iterable );
    ok( hr == S_OK, "Got hr %#lx.\n", hr );
    hr = IVisualCollection_InsertAtBottom( collection, child1 );
    ok( hr == S_OK, "Got hr %#lx.\n", hr );
    hr = IVisualCollection_InsertAtTop( collection, child2 );
    ok( hr == S_OK, "Got hr %#lx.\n", hr );
    hr = IVisualCollection_get_Count( collection, &size );
    ok( hr == S_OK && size == 2, "Got hr %#lx, size %d.\n", hr, size );

    hr = IIterable_Visual_First( iterable, &iterator );
    ok( hr == S_OK, "Got hr %#lx.\n", hr );
    hr = IIterator_Visual_get_HasCurrent( iterator, &has_current );
    ok( hr == S_OK && has_current, "Got hr %#lx, has_current %u.\n", hr, has_current );
    hr = IIterator_Visual_get_Current( iterator, &current );
    ok( hr == S_OK && current == child1, "Got hr %#lx, current %p.\n", hr, current );
    if (current) IVisual_Release( current );
    current = NULL;
    hr = IIterator_Visual_MoveNext( iterator, &has_current );
    ok( hr == S_OK && has_current, "Got hr %#lx, has_current %u.\n", hr, has_current );
    hr = IIterator_Visual_get_Current( iterator, &current );
    ok( hr == S_OK && current == child2, "Got hr %#lx, current %p.\n", hr, current );
    if (current) IVisual_Release( current );
    current = NULL;
    hr = IIterator_Visual_MoveNext( iterator, &has_current );
    ok( hr == S_OK && !has_current, "Got hr %#lx, has_current %u.\n", hr, has_current );
    hr = IIterator_Visual_get_Current( iterator, &current );
    ok( hr == E_BOUNDS && !current, "Got hr %#lx, current %p.\n", hr, current );
    IIterator_Visual_Release( iterator );
    iterator = NULL;

    hr = IIterable_Visual_First( iterable, &iterator );
    ok( hr == S_OK, "Got hr %#lx.\n", hr );
    hr = IIterator_Visual_GetMany( iterator, ARRAY_SIZE(items), items, &count );
    ok( hr == S_OK && count == 2, "Got hr %#lx, count %u.\n", hr, count );
    ok( items[0] == child1, "Got item %p.\n", items[0] );
    ok( items[1] == child2, "Got item %p.\n", items[1] );
    if (items[0]) IVisual_Release( items[0] );
    if (items[1]) IVisual_Release( items[1] );

done:
    if (iterator) IIterator_Visual_Release( iterator );
    if (iterable) IIterable_Visual_Release( iterable );
    if (collection) IVisualCollection_Release( collection );
    if (child2) IVisual_Release( child2 );
    if (child1) IVisual_Release( child1 );
    if (sprite2) ISpriteVisual_Release( sprite2 );
    if (sprite1) ISpriteVisual_Release( sprite1 );
    if (container) IContainerVisual_Release( container );
    ICompositor_Release( compositor );
}

static void check_binding_mode( IInteractionTrackerStatics2 *statics, IInteractionTracker *tracker1,
        IInteractionTracker *tracker2, enum InteractionBindingAxisModes expected )
{
    enum InteractionBindingAxisModes mode = 0xdead;
    HRESULT hr;

    hr = IInteractionTrackerStatics2_GetBindingMode( statics, tracker1, tracker2, &mode );
    ok( hr == S_OK && mode == expected, "Got hr %#lx, mode %#x.\n", hr, mode );
}

static void test_interaction_tracker_bindings(void)
{
    IInteractionTracker *tracker1 = NULL, *tracker2 = NULL, *tracker3 = NULL, *tracker4 = NULL;
    IInteractionTrackerStatics2 *statics2 = NULL;
    IInteractionTrackerStatics *statics = NULL;
    IActivationFactory *factory = NULL;
    ICompositor *compositor;
    HSTRING class_name;
    HRESULT hr;

    if (!(compositor = create_compositor())) return;
    hr = WindowsCreateString( RuntimeClass_Windows_UI_Composition_Interactions_InteractionTracker,
                              ARRAY_SIZE(RuntimeClass_Windows_UI_Composition_Interactions_InteractionTracker) - 1,
                              &class_name );
    ok( hr == S_OK, "Got hr %#lx.\n", hr );
    hr = RoGetActivationFactory( class_name, &IID_IActivationFactory, (void **)&factory );
    ok( hr == S_OK, "Got hr %#lx.\n", hr );
    WindowsDeleteString( class_name );
    if (FAILED(hr)) goto done;

    hr = IActivationFactory_QueryInterface( factory, &interaction_tracker_statics_iid,
                                             (void **)&statics );
    ok( hr == S_OK, "Got hr %#lx.\n", hr );
    hr = IActivationFactory_QueryInterface( factory, &interaction_tracker_statics2_iid,
                                             (void **)&statics2 );
    ok( hr == S_OK, "Got hr %#lx.\n", hr );
    if (!statics || !statics2) goto done;

    hr = IInteractionTrackerStatics_Create( statics, compositor, &tracker1 );
    ok( hr == S_OK, "Got hr %#lx.\n", hr );
    hr = IInteractionTrackerStatics_Create( statics, compositor, &tracker2 );
    ok( hr == S_OK, "Got hr %#lx.\n", hr );
    hr = IInteractionTrackerStatics_Create( statics, compositor, &tracker3 );
    ok( hr == S_OK, "Got hr %#lx.\n", hr );
    hr = IInteractionTrackerStatics_Create( statics, compositor, &tracker4 );
    ok( hr == S_OK, "Got hr %#lx.\n", hr );
    if (!tracker1 || !tracker2 || !tracker3 || !tracker4) goto done;

    hr = IInteractionTrackerStatics2_SetBindingMode( statics2, tracker1, tracker2,
                                                      InteractionBindingAxisModes_PositionX );
    ok( hr == S_OK, "Got hr %#lx.\n", hr );
    hr = IInteractionTrackerStatics2_SetBindingMode( statics2, tracker1, tracker3,
                                                      InteractionBindingAxisModes_PositionY );
    ok( hr == S_OK, "Got hr %#lx.\n", hr );
    hr = IInteractionTrackerStatics2_SetBindingMode( statics2, tracker1, tracker4,
                                                      InteractionBindingAxisModes_Scale );
    ok( hr == S_OK, "Got hr %#lx.\n", hr );
    check_binding_mode( statics2, tracker1, tracker2, InteractionBindingAxisModes_PositionX );
    check_binding_mode( statics2, tracker1, tracker3, InteractionBindingAxisModes_PositionY );
    check_binding_mode( statics2, tracker1, tracker4, InteractionBindingAxisModes_Scale );
    check_binding_mode( statics2, tracker2, tracker1, InteractionBindingAxisModes_PositionX );

    hr = IInteractionTrackerStatics2_SetBindingMode( statics2, tracker1, tracker2,
            InteractionBindingAxisModes_PositionX | InteractionBindingAxisModes_Scale );
    ok( hr == S_OK, "Got hr %#lx.\n", hr );
    check_binding_mode( statics2, tracker1, tracker2,
                        InteractionBindingAxisModes_PositionX | InteractionBindingAxisModes_Scale );
    check_binding_mode( statics2, tracker1, tracker3, InteractionBindingAxisModes_PositionY );

    hr = IInteractionTrackerStatics2_SetBindingMode( statics2, tracker1, tracker3,
                                                      InteractionBindingAxisModes_None );
    ok( hr == S_OK, "Got hr %#lx.\n", hr );
    check_binding_mode( statics2, tracker1, tracker3, InteractionBindingAxisModes_None );
    check_binding_mode( statics2, tracker1, tracker2,
                        InteractionBindingAxisModes_PositionX | InteractionBindingAxisModes_Scale );
    check_binding_mode( statics2, tracker1, tracker4, InteractionBindingAxisModes_Scale );

done:
    if (tracker4) IInteractionTracker_Release( tracker4 );
    if (tracker3) IInteractionTracker_Release( tracker3 );
    if (tracker2) IInteractionTracker_Release( tracker2 );
    if (tracker1) IInteractionTracker_Release( tracker1 );
    if (statics2) IInteractionTrackerStatics2_Release( statics2 );
    if (statics) IInteractionTrackerStatics_Release( statics );
    if (factory) IActivationFactory_Release( factory );
    ICompositor_Release( compositor );
}

static void test_effect_source_parameter(void)
{
    ICompositionEffectSourceParameterFactory *constructor = NULL;
    ICompositionEffectSourceParameter *parameter = NULL;
    IGraphicsEffectSource *source = NULL;
    IActivationFactory *factory = NULL;
    IUnknown *identity = NULL, *other = NULL;
    HSTRING class_name, name, value;
    HSTRING_HEADER header;
    WCHAR buffer[] = L"Backdrop";
    IID *iids;
    ULONG count;
    HRESULT hr;

    WindowsCreateString( RuntimeClass_Windows_UI_Composition_CompositionEffectSourceParameter,
                        wcslen(RuntimeClass_Windows_UI_Composition_CompositionEffectSourceParameter), &class_name );
    hr = RoGetActivationFactory( class_name, &IID_IActivationFactory, (void **)&factory );
    WindowsDeleteString( class_name );
    ok( hr == S_OK, "Got hr %#lx.\n", hr );
    if (FAILED(hr)) return;
    hr = IActivationFactory_QueryInterface( factory, &IID_ICompositionEffectSourceParameterFactory,
                                           (void **)&constructor );
    ok( hr == S_OK, "Got hr %#lx.\n", hr );
    if (FAILED(hr)) goto done;

    hr = ICompositionEffectSourceParameterFactory_Create( constructor, NULL, NULL );
    ok( hr == E_POINTER, "Got hr %#lx.\n", hr );
    WindowsCreateStringReference( buffer, wcslen(buffer), &header, &name );
    hr = ICompositionEffectSourceParameterFactory_Create( constructor, name, &parameter );
    ok( hr == S_OK, "Got hr %#lx.\n", hr );
    if (FAILED(hr)) goto done;
    buffer[0] = L'X';
    hr = ICompositionEffectSourceParameter_get_Name( parameter, &value );
    ok( hr == S_OK, "Got hr %#lx.\n", hr );
    ok( !wcscmp( WindowsGetStringRawBuffer( value, NULL ), L"Backdrop" ), "Name was not copied.\n" );
    WindowsDeleteString( value );
    hr = ICompositionEffectSourceParameter_get_Name( parameter, NULL );
    ok( hr == E_POINTER, "Got hr %#lx.\n", hr );
    hr = ICompositionEffectSourceParameter_GetRuntimeClassName( parameter, &value );
    ok( hr == S_OK, "Got hr %#lx.\n", hr );
    ok( !wcscmp( WindowsGetStringRawBuffer( value, NULL ),
                 RuntimeClass_Windows_UI_Composition_CompositionEffectSourceParameter ), "Wrong class name.\n" );
    WindowsDeleteString( value );
    hr = ICompositionEffectSourceParameter_GetIids( parameter, &count, &iids );
    ok( hr == S_OK, "Got hr %#lx.\n", hr );
    ok( count == 2, "Got count %lu.\n", count );
    CoTaskMemFree( iids );
    hr = ICompositionEffectSourceParameter_QueryInterface( parameter, &IID_IGraphicsEffectSource, (void **)&source );
    ok( hr == S_OK, "Got hr %#lx.\n", hr );
    if (source)
    {
        ICompositionEffectSourceParameter_QueryInterface( parameter, &IID_IUnknown, (void **)&identity );
        IGraphicsEffectSource_QueryInterface( source, &IID_IUnknown, (void **)&other );
        ok( identity == other, "COM identity differs.\n" );
        IUnknown_Release( identity );
        IUnknown_Release( other );
        IGraphicsEffectSource_Release( source );
    }
    hr = ICompositionEffectSourceParameter_QueryInterface( parameter, &IID_IAgileObject, (void **)&other );
    ok( hr == S_OK, "Got hr %#lx.\n", hr );
    if (SUCCEEDED(hr)) IUnknown_Release( other );
    other = (IUnknown *)0xdeadbeef;
    hr = ICompositionEffectSourceParameter_QueryInterface( parameter, &IID_IGraphicsEffect, (void **)&other );
    ok( hr == E_NOINTERFACE, "Got hr %#lx.\n", hr );
    ok( !other, "Unsupported interface was not cleared.\n" );
    ICompositionEffectSourceParameter_Release( parameter );
    parameter = NULL;

    hr = ICompositionEffectSourceParameterFactory_Create( constructor, NULL, &parameter );
    ok( hr == S_OK, "Got hr %#lx.\n", hr );
    if (parameter)
    {
        hr = ICompositionEffectSourceParameter_get_Name( parameter, &value );
        ok( hr == S_OK, "Got hr %#lx.\n", hr );
        ok( !WindowsGetStringLen( value ), "Expected an empty name.\n" );
        WindowsDeleteString( value );
        ICompositionEffectSourceParameter_Release( parameter );
    }

done:
    if (constructor) ICompositionEffectSourceParameterFactory_Release( constructor );
    IActivationFactory_Release( factory );
}

START_TEST(composition)
{
    HRESULT hr;

    hr = RoInitialize( RO_INIT_MULTITHREADED );
    ok( hr == S_OK, "RoInitialize failed, hr %#lx.\n", hr );

    test_compositor_interfaces();
    test_composition_graphics_device2();
    test_compositor_controller_async();
    test_compositor_animation();
    test_vector3_animation();
    test_visual_collection_iterable();
    test_interaction_tracker_bindings();
    test_effect_source_parameter();

    RoUninitialize();
}
