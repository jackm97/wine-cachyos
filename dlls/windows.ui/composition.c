/* WinRT Windows.UI.Composition implementation
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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA
 */

#include <stdlib.h>
#include <limits.h>
#include <math.h>
#include <stdint.h>
#include "private.h"
#include "initguid.h"
#include "dcomp.h"
#include "d2d1_1.h"
#include "d3d11.h"
#include "dxgi.h"
#include "dxgi1_2.h"
#include "effect_graph.h"

#include "wine/debug.h"
#include "wine/list.h"

WINE_DEFAULT_DEBUG_CHANNEL(ui);

static IDCompositionVisual *dcomp_visual_from_visual( IVisual *iface );
static void surface_brush_bind( ICompositionSurfaceBrush *iface, IDCompositionVisual *visual );
static BOOL composition_brush_contains( ICompositionBrush *brush, ICompositionBrush *target, UINT depth );
static HRESULT scalar_key_frame_animation_get_final_value( ICompositionAnimation *animation,
        FLOAT *value );
static HRESULT boolean_key_frame_animation_get_final_value( ICompositionAnimation *animation,
        boolean *value );
static HRESULT vector3_key_frame_animation_get_final_value( ICompositionAnimation *animation,
        struct Vector3 *value );

static HRESULT expression_animation_get_vector( IExpressionAnimation *animation, struct Vector3 *value );
static HRESULT expression_animation_clone( IExpressionAnimation *animation, IExpressionAnimation **result );

struct compositor2_iface;
struct compositor4_iface;
struct compositor5_iface;
struct compositor6_iface;
struct compositor;
struct compositor_controller;
struct nine_grid_brush_iface;

static void compositor_notify_commit_needed( struct compositor *compositor );
static HRESULT completed_action_create( IAsyncAction **operation );

struct compositor2_vtbl
{
    HRESULT (WINAPI *QueryInterface)( struct compositor2_iface *iface, REFIID iid, void **out );
    ULONG (WINAPI *AddRef)( struct compositor2_iface *iface );
    ULONG (WINAPI *Release)( struct compositor2_iface *iface );
    HRESULT (WINAPI *GetIids)( struct compositor2_iface *iface, ULONG *count, IID **iids );
    HRESULT (WINAPI *GetRuntimeClassName)( struct compositor2_iface *iface, HSTRING *name );
    HRESULT (WINAPI *GetTrustLevel)( struct compositor2_iface *iface, TrustLevel *level );
    HRESULT (WINAPI *CreateAmbientLight)( struct compositor2_iface *iface, IInspectable **result );
    HRESULT (WINAPI *CreateAnimationGroup)( struct compositor2_iface *iface, IInspectable **result );
    HRESULT (WINAPI *CreateBackdropBrush)( struct compositor2_iface *iface, IInspectable **result );
    HRESULT (WINAPI *CreateDistantLight)( struct compositor2_iface *iface, IInspectable **result );
    HRESULT (WINAPI *CreateDropShadow)( struct compositor2_iface *iface, IInspectable **result );
    HRESULT (WINAPI *CreateImplicitAnimationCollection)( struct compositor2_iface *iface, IInspectable **result );
    HRESULT (WINAPI *CreateLayerVisual)( struct compositor2_iface *iface, IInspectable **result );
    HRESULT (WINAPI *CreateMaskBrush)( struct compositor2_iface *iface, IInspectable **result );
    HRESULT (WINAPI *CreateNineGridBrush)( struct compositor2_iface *iface, IInspectable **result );
    HRESULT (WINAPI *CreatePointLight)( struct compositor2_iface *iface, IInspectable **result );
    HRESULT (WINAPI *CreateSpotLight)( struct compositor2_iface *iface, IInspectable **result );
    HRESULT (WINAPI *CreateStepEasingFunction)( struct compositor2_iface *iface, IInspectable **result );
    HRESULT (WINAPI *CreateStepEasingFunctionWithStepCount)( struct compositor2_iface *iface,
            INT32 step_count, IInspectable **result );
};

struct compositor2_iface
{
    const struct compositor2_vtbl *lpVtbl;
};

struct compositor4_vtbl
{
    HRESULT (WINAPI *QueryInterface)( struct compositor4_iface *iface, REFIID iid, void **out );
    ULONG (WINAPI *AddRef)( struct compositor4_iface *iface );
    ULONG (WINAPI *Release)( struct compositor4_iface *iface );
    HRESULT (WINAPI *GetIids)( struct compositor4_iface *iface, ULONG *count, IID **iids );
    HRESULT (WINAPI *GetRuntimeClassName)( struct compositor4_iface *iface, HSTRING *name );
    HRESULT (WINAPI *GetTrustLevel)( struct compositor4_iface *iface, TrustLevel *level );
    HRESULT (WINAPI *CreateColorGradientStop)( struct compositor4_iface *iface, IInspectable **result );
    HRESULT (WINAPI *CreateColorGradientStopWithOffsetAndColor)( struct compositor4_iface *iface,
            FLOAT offset, struct Color color, IInspectable **result );
    HRESULT (WINAPI *CreateLinearGradientBrush)( struct compositor4_iface *iface, IInspectable **result );
    HRESULT (WINAPI *CreateSpringScalarAnimation)( struct compositor4_iface *iface, IInspectable **result );
    HRESULT (WINAPI *CreateSpringVector2Animation)( struct compositor4_iface *iface, IInspectable **result );
    HRESULT (WINAPI *CreateSpringVector3Animation)( struct compositor4_iface *iface, IInspectable **result );
};

struct compositor4_iface
{
    const struct compositor4_vtbl *lpVtbl;
};

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

struct compositor6_vtbl
{
    HRESULT (WINAPI *QueryInterface)( struct compositor6_iface *iface, REFIID iid, void **out );
    ULONG (WINAPI *AddRef)( struct compositor6_iface *iface );
    ULONG (WINAPI *Release)( struct compositor6_iface *iface );
    HRESULT (WINAPI *GetIids)( struct compositor6_iface *iface, ULONG *count, IID **iids );
    HRESULT (WINAPI *GetRuntimeClassName)( struct compositor6_iface *iface, HSTRING *name );
    HRESULT (WINAPI *GetTrustLevel)( struct compositor6_iface *iface, TrustLevel *level );
    HRESULT (WINAPI *CreateGeometricClip)( struct compositor6_iface *iface, IInspectable **result );
    HRESULT (WINAPI *CreateGeometricClipWithGeometry)( struct compositor6_iface *iface,
            IInspectable *geometry, IInspectable **result );
    HRESULT (WINAPI *CreateRedirectVisual)( struct compositor6_iface *iface, IInspectable **result );
    HRESULT (WINAPI *CreateRedirectVisualWithSourceVisual)( struct compositor6_iface *iface,
            IInspectable *visual, IInspectable **result );
    HRESULT (WINAPI *CreateBooleanKeyFrameAnimation)( struct compositor6_iface *iface,
            IInspectable **result );
};

struct compositor6_iface
{
    const struct compositor6_vtbl *lpVtbl;
};

static const GUID compositor6_iid =
    {0x7a38b2bd,0xcec8,0x4eeb,{0x83,0x0f,0xd8,0xd0,0x7a,0xed,0xeb,0xc3}};
static const GUID compositor5_iid =
    {0x48ea31ad,0x7fcd,0x4076,{0xa7,0x9c,0x90,0xcc,0x4b,0x85,0x2c,0x9b}};

struct nine_grid_brush_vtbl
{
    HRESULT (WINAPI *QueryInterface)( struct nine_grid_brush_iface *iface, REFIID iid, void **out );
    ULONG (WINAPI *AddRef)( struct nine_grid_brush_iface *iface );
    ULONG (WINAPI *Release)( struct nine_grid_brush_iface *iface );
    HRESULT (WINAPI *GetIids)( struct nine_grid_brush_iface *iface, ULONG *count, IID **iids );
    HRESULT (WINAPI *GetRuntimeClassName)( struct nine_grid_brush_iface *iface, HSTRING *name );
    HRESULT (WINAPI *GetTrustLevel)( struct nine_grid_brush_iface *iface, TrustLevel *level );
    HRESULT (WINAPI *get_BottomInset)( struct nine_grid_brush_iface *iface, FLOAT *value );
    HRESULT (WINAPI *put_BottomInset)( struct nine_grid_brush_iface *iface, FLOAT value );
    HRESULT (WINAPI *get_BottomInsetScale)( struct nine_grid_brush_iface *iface, FLOAT *value );
    HRESULT (WINAPI *put_BottomInsetScale)( struct nine_grid_brush_iface *iface, FLOAT value );
    HRESULT (WINAPI *get_IsCenterHollow)( struct nine_grid_brush_iface *iface, boolean *value );
    HRESULT (WINAPI *put_IsCenterHollow)( struct nine_grid_brush_iface *iface, boolean value );
    HRESULT (WINAPI *get_LeftInset)( struct nine_grid_brush_iface *iface, FLOAT *value );
    HRESULT (WINAPI *put_LeftInset)( struct nine_grid_brush_iface *iface, FLOAT value );
    HRESULT (WINAPI *get_LeftInsetScale)( struct nine_grid_brush_iface *iface, FLOAT *value );
    HRESULT (WINAPI *put_LeftInsetScale)( struct nine_grid_brush_iface *iface, FLOAT value );
    HRESULT (WINAPI *get_RightInset)( struct nine_grid_brush_iface *iface, FLOAT *value );
    HRESULT (WINAPI *put_RightInset)( struct nine_grid_brush_iface *iface, FLOAT value );
    HRESULT (WINAPI *get_RightInsetScale)( struct nine_grid_brush_iface *iface, FLOAT *value );
    HRESULT (WINAPI *put_RightInsetScale)( struct nine_grid_brush_iface *iface, FLOAT value );
    HRESULT (WINAPI *get_Source)( struct nine_grid_brush_iface *iface, ICompositionBrush **value );
    HRESULT (WINAPI *put_Source)( struct nine_grid_brush_iface *iface, ICompositionBrush *value );
    HRESULT (WINAPI *get_TopInset)( struct nine_grid_brush_iface *iface, FLOAT *value );
    HRESULT (WINAPI *put_TopInset)( struct nine_grid_brush_iface *iface, FLOAT value );
    HRESULT (WINAPI *get_TopInsetScale)( struct nine_grid_brush_iface *iface, FLOAT *value );
    HRESULT (WINAPI *put_TopInsetScale)( struct nine_grid_brush_iface *iface, FLOAT value );
    HRESULT (WINAPI *SetInsets)( struct nine_grid_brush_iface *iface, FLOAT inset );
    HRESULT (WINAPI *SetInsetsWithValues)( struct nine_grid_brush_iface *iface,
            FLOAT left, FLOAT top, FLOAT right, FLOAT bottom );
    HRESULT (WINAPI *SetInsetScales)( struct nine_grid_brush_iface *iface, FLOAT scale );
    HRESULT (WINAPI *SetInsetScalesWithValues)( struct nine_grid_brush_iface *iface,
            FLOAT left, FLOAT top, FLOAT right, FLOAT bottom );
};

struct nine_grid_brush_iface
{
    const struct nine_grid_brush_vtbl *lpVtbl;
};

static const GUID compositor2_iid =
    {0x735081dc, 0x5e24, 0x45da, {0xa3, 0x8f, 0xe3, 0x2c, 0xc3, 0x49, 0xa9, 0xa0}};
static const GUID compositor4_iid =
    {0xae47e78a, 0x7910, 0x4425, {0xa4, 0x82, 0xa0, 0x5b, 0x75, 0x8a, 0xdc, 0xe9}};
static const GUID composition_nine_grid_brush_iid =
    {0xf25154e4, 0xbc8c, 0x4be7, {0xb8, 0x0f, 0x86, 0x85, 0xb8, 0x3c, 0x01, 0x86}};

static void nine_grid_brush_bind( struct nine_grid_brush_iface *iface, IDCompositionVisual *visual );

struct desktop_window_target
{
    IDesktopWindowTarget IDesktopWindowTarget_iface;
    ICompositionTarget ICompositionTarget_iface;
    IDesktopWindowTargetInterop IDesktopWindowTargetInterop_iface;
    IVisual *root;
    IDCompositionTarget *dcomp_target;
    IDXGISwapChain1 *swapchain;
    ID2D1Device *render_device;
    ID2D1DeviceContext *render_context;
    ID2D1Bitmap1 *render_bitmap;
    struct list entry;
    HWND hwnd;
    UINT width;
    UINT height;
    BOOL topmost;
    BOOL linked;
    LONG ref;
};

static inline struct desktop_window_target *impl_from_IDesktopWindowTarget( IDesktopWindowTarget *iface )
{
    return CONTAINING_RECORD( iface, struct desktop_window_target, IDesktopWindowTarget_iface );
}

static inline struct desktop_window_target *target_from_ICompositionTarget( ICompositionTarget *iface )
{
    return CONTAINING_RECORD( iface, struct desktop_window_target, ICompositionTarget_iface );
}

static inline struct desktop_window_target *target_from_IDesktopWindowTargetInterop(
        IDesktopWindowTargetInterop *iface )
{
    return CONTAINING_RECORD( iface, struct desktop_window_target, IDesktopWindowTargetInterop_iface );
}

static HRESULT desktop_window_target_query_interface( struct desktop_window_target *impl,
        REFIID iid, void **out )
{
    FIXME( "impl %p, iid %s, out %p.\n", impl, debugstr_guid( iid ), out );
    if (!out) return E_POINTER;
    if (IsEqualGUID( iid, &IID_IUnknown ) || IsEqualGUID( iid, &IID_IInspectable ) ||
        IsEqualGUID( iid, &IID_IAgileObject ) || IsEqualGUID( iid, &IID_IDesktopWindowTarget ))
        *out = &impl->IDesktopWindowTarget_iface;
    else if (IsEqualGUID( iid, &IID_ICompositionTarget ))
        *out = &impl->ICompositionTarget_iface;
    else if (IsEqualGUID( iid, &IID_IDesktopWindowTargetInterop ))
        *out = &impl->IDesktopWindowTargetInterop_iface;
    else
    {
        *out = NULL;
        return E_NOINTERFACE;
    }
    IUnknown_AddRef( (IUnknown *)*out );
    return S_OK;
}

static ULONG desktop_window_target_add_ref( struct desktop_window_target *impl )
{
    return InterlockedIncrement( &impl->ref );
}

static void desktop_window_target_reset_renderer( struct desktop_window_target *impl )
{
    if (impl->render_bitmap) ID2D1Bitmap1_Release( impl->render_bitmap );
    if (impl->render_context) ID2D1DeviceContext_Release( impl->render_context );
    if (impl->render_device) ID2D1Device_Release( impl->render_device );
    if (impl->swapchain) IDXGISwapChain1_Release( impl->swapchain );
    impl->render_bitmap = NULL;
    impl->render_context = NULL;
    impl->render_device = NULL;
    impl->swapchain = NULL;
    impl->width = 0;
    impl->height = 0;
}

static ULONG desktop_window_target_release( struct desktop_window_target *impl )
{
    ULONG ref = InterlockedDecrement( &impl->ref );

    if (!ref)
    {
        if (impl->linked) list_remove( &impl->entry );
        desktop_window_target_reset_renderer( impl );
        if (impl->root) IVisual_Release( impl->root );
        if (impl->dcomp_target) IDCompositionTarget_Release( impl->dcomp_target );
        free( impl );
    }
    return ref;
}

static HRESULT desktop_window_target_get_iids( ULONG *count, IID **iids )
{
    IID *values;

    if (!count || !iids) return E_POINTER;
    if (!(values = CoTaskMemAlloc( 3 * sizeof(*values) ))) return E_OUTOFMEMORY;
    values[0] = IID_IDesktopWindowTarget;
    values[1] = IID_ICompositionTarget;
    values[2] = IID_IDesktopWindowTargetInterop;
    *count = 3;
    *iids = values;
    return S_OK;
}

static HRESULT desktop_window_target_get_class_name( HSTRING *name )
{
    if (!name) return E_POINTER;
    return WindowsCreateString( RuntimeClass_Windows_UI_Composition_Desktop_DesktopWindowTarget,
                                ARRAY_SIZE(RuntimeClass_Windows_UI_Composition_Desktop_DesktopWindowTarget) - 1,
                                name );
}

static HRESULT desktop_window_target_get_trust_level( TrustLevel *level )
{
    if (!level) return E_POINTER;
    *level = BaseTrust;
    return S_OK;
}

static HRESULT WINAPI desktop_window_target_QueryInterface( IDesktopWindowTarget *iface,
        REFIID iid, void **out )
{
    return desktop_window_target_query_interface( impl_from_IDesktopWindowTarget( iface ), iid, out );
}

static ULONG WINAPI desktop_window_target_AddRef( IDesktopWindowTarget *iface )
{
    return desktop_window_target_add_ref( impl_from_IDesktopWindowTarget( iface ) );
}

static ULONG WINAPI desktop_window_target_Release( IDesktopWindowTarget *iface )
{
    return desktop_window_target_release( impl_from_IDesktopWindowTarget( iface ) );
}

static HRESULT WINAPI desktop_window_target_GetIids( IDesktopWindowTarget *iface,
        ULONG *count, IID **iids )
{
    return desktop_window_target_get_iids( count, iids );
}

static HRESULT WINAPI desktop_window_target_GetRuntimeClassName( IDesktopWindowTarget *iface,
        HSTRING *name )
{
    return desktop_window_target_get_class_name( name );
}

static HRESULT WINAPI desktop_window_target_GetTrustLevel( IDesktopWindowTarget *iface,
        TrustLevel *level )
{
    return desktop_window_target_get_trust_level( level );
}

static HRESULT WINAPI desktop_window_target_get_IsTopmost( IDesktopWindowTarget *iface, boolean *value )
{
    if (!value) return E_POINTER;
    *value = impl_from_IDesktopWindowTarget( iface )->topmost;
    return S_OK;
}

static const IDesktopWindowTargetVtbl desktop_window_target_vtbl =
{
    desktop_window_target_QueryInterface,
    desktop_window_target_AddRef,
    desktop_window_target_Release,
    desktop_window_target_GetIids,
    desktop_window_target_GetRuntimeClassName,
    desktop_window_target_GetTrustLevel,
    desktop_window_target_get_IsTopmost,
};

static HRESULT WINAPI composition_target_QueryInterface( ICompositionTarget *iface,
        REFIID iid, void **out )
{
    return desktop_window_target_query_interface( target_from_ICompositionTarget( iface ), iid, out );
}

static ULONG WINAPI composition_target_AddRef( ICompositionTarget *iface )
{
    return desktop_window_target_add_ref( target_from_ICompositionTarget( iface ) );
}

static ULONG WINAPI composition_target_Release( ICompositionTarget *iface )
{
    return desktop_window_target_release( target_from_ICompositionTarget( iface ) );
}

static HRESULT WINAPI composition_target_GetIids( ICompositionTarget *iface, ULONG *count, IID **iids )
{
    return desktop_window_target_get_iids( count, iids );
}

static HRESULT WINAPI composition_target_GetRuntimeClassName( ICompositionTarget *iface, HSTRING *name )
{
    return desktop_window_target_get_class_name( name );
}

static HRESULT WINAPI composition_target_GetTrustLevel( ICompositionTarget *iface, TrustLevel *level )
{
    return desktop_window_target_get_trust_level( level );
}

static HRESULT WINAPI composition_target_get_Root( ICompositionTarget *iface, IVisual **value )
{
    struct desktop_window_target *impl = target_from_ICompositionTarget( iface );

    if (!value) return E_POINTER;
    if (impl->root) IVisual_AddRef( impl->root );
    *value = impl->root;
    return S_OK;
}

static HRESULT WINAPI composition_target_put_Root( ICompositionTarget *iface, IVisual *value )
{
    struct desktop_window_target *impl = target_from_ICompositionTarget( iface );
    IDCompositionVisual *dcomp_visual = NULL;
    HRESULT hr;

    if (value) dcomp_visual = dcomp_visual_from_visual( value );
    if (impl->dcomp_target && FAILED(hr = IDCompositionTarget_SetRoot( impl->dcomp_target,
            dcomp_visual ))) return hr;

    if (value) IVisual_AddRef( value );
    if (impl->root) IVisual_Release( impl->root );
    impl->root = value;
    return S_OK;
}

static const ICompositionTargetVtbl composition_target_vtbl =
{
    composition_target_QueryInterface,
    composition_target_AddRef,
    composition_target_Release,
    composition_target_GetIids,
    composition_target_GetRuntimeClassName,
    composition_target_GetTrustLevel,
    composition_target_get_Root,
    composition_target_put_Root,
};

static HRESULT WINAPI desktop_target_interop_QueryInterface( IDesktopWindowTargetInterop *iface,
        REFIID iid, void **out )
{
    return desktop_window_target_query_interface( target_from_IDesktopWindowTargetInterop( iface ), iid, out );
}

static ULONG WINAPI desktop_target_interop_AddRef( IDesktopWindowTargetInterop *iface )
{
    return desktop_window_target_add_ref( target_from_IDesktopWindowTargetInterop( iface ) );
}

static ULONG WINAPI desktop_target_interop_Release( IDesktopWindowTargetInterop *iface )
{
    return desktop_window_target_release( target_from_IDesktopWindowTargetInterop( iface ) );
}

static HRESULT WINAPI desktop_target_interop_get_Hwnd( IDesktopWindowTargetInterop *iface, HWND *value )
{
    if (!value) return E_POINTER;
    *value = target_from_IDesktopWindowTargetInterop( iface )->hwnd;
    return S_OK;
}

static const IDesktopWindowTargetInteropVtbl desktop_target_interop_vtbl =
{
    desktop_target_interop_QueryInterface,
    desktop_target_interop_AddRef,
    desktop_target_interop_Release,
    desktop_target_interop_get_Hwnd,
};

static HRESULT desktop_window_target_create( IDCompositionDevice *device, struct list *targets,
        HWND hwnd, BOOL topmost, IDesktopWindowTarget **result )
{
    struct desktop_window_target *impl;
    HRESULT hr;

    if (!result) return E_POINTER;
    *result = NULL;
    if (!(impl = calloc( 1, sizeof(*impl) ))) return E_OUTOFMEMORY;
    impl->IDesktopWindowTarget_iface.lpVtbl = &desktop_window_target_vtbl;
    impl->ICompositionTarget_iface.lpVtbl = &composition_target_vtbl;
    impl->IDesktopWindowTargetInterop_iface.lpVtbl = &desktop_target_interop_vtbl;
    impl->hwnd = hwnd;
    impl->topmost = topmost;
    impl->ref = 2;
    if (device && FAILED(hr = IDCompositionDevice_CreateTargetForHwnd( device, hwnd, topmost,
            &impl->dcomp_target )))
    {
        free( impl );
        return hr;
    }
    list_add_tail( targets, &impl->entry );
    impl->linked = TRUE;
    *result = &impl->IDesktopWindowTarget_iface;
    return S_OK;
}

struct compositor
{
    ICompositor ICompositor_iface;
    struct compositor2_iface ICompositor2_iface;
    struct compositor4_iface ICompositor4_iface;
    struct compositor5_iface ICompositor5_iface;
    struct compositor6_iface ICompositor6_iface;
    ICompositorDesktopInterop ICompositorDesktopInterop_iface;
    ICompositorInterop ICompositorInterop_iface;
    IClosable IClosable_iface;
    IDCompositionDevice *dcomp_device;
    struct compositor_controller *controller;
    struct list targets;
    HSTRING comment;
    FLOAT global_playback_rate;
    LONG ref;
};

struct composition_graphics_device
{
    ICompositionGraphicsDevice ICompositionGraphicsDevice_iface;
    ICompositionGraphicsDevice2 ICompositionGraphicsDevice2_iface;
    IUnknown *device;
    LONG ref;
};

struct composition_surface_tile
{
    struct list entry;
    POINT origin;
    HRGN valid;
    ID2D1Bitmap1 *bitmap;
};

struct composition_drawing_surface
{
    ICompositionDrawingSurface ICompositionDrawingSurface_iface;
    ICompositionVirtualDrawingSurface ICompositionVirtualDrawingSurface_iface;
    ICompositionDrawingSurfaceInterop ICompositionDrawingSurfaceInterop_iface;
    ID2D1Device *device;
    ID2D1DeviceContext *context;
    ID2D1Bitmap1 *bitmap;
    ID2D1Bitmap1 *committed_bitmap;
    RECT update_rect;
    BOOL drawing;
    BOOL suspended;
    Size size;
    enum __x_ABI_CWindows_CGraphics_CDirectX_CDirectXPixelFormat format;
    enum __x_ABI_CWindows_CGraphics_CDirectX_CDirectXAlphaMode mode;
    BOOL virtual;
    POINT bitmap_offset;
    struct list tiles;
    LONG ref;
};

static const GUID redirect_visual_iid =
    {0x8cc6e340,0x8b75,0x5422,{0xb0,0x6f,0x09,0xff,0xe9,0xf8,0x61,0x7e}};
struct redirect_visual_iface;
struct redirect_visual_vtbl
{
    HRESULT (WINAPI *QueryInterface)(struct redirect_visual_iface *, REFIID, void **);
    ULONG (WINAPI *AddRef)(struct redirect_visual_iface *);
    ULONG (WINAPI *Release)(struct redirect_visual_iface *);
    HRESULT (WINAPI *GetIids)(struct redirect_visual_iface *, ULONG *, IID **);
    HRESULT (WINAPI *GetRuntimeClassName)(struct redirect_visual_iface *, HSTRING *);
    HRESULT (WINAPI *GetTrustLevel)(struct redirect_visual_iface *, TrustLevel *);
    HRESULT (WINAPI *get_Source)(struct redirect_visual_iface *, IVisual **);
    HRESULT (WINAPI *put_Source)(struct redirect_visual_iface *, IVisual *);
};
struct redirect_visual_iface { const struct redirect_visual_vtbl *lpVtbl; };

static CRITICAL_SECTION visual_graph_cs;
static CRITICAL_SECTION_DEBUG visual_graph_cs_debug =
{
    0, 0, &visual_graph_cs,
    { &visual_graph_cs_debug.ProcessLocksList, &visual_graph_cs_debug.ProcessLocksList },
    0, 0, { (DWORD_PTR)(__FILE__ ": visual_graph_cs") }
};
static CRITICAL_SECTION visual_graph_cs = { &visual_graph_cs_debug, -1, 0, 0, 0, 0 };
static BOOL visual_contains(IVisual *visual, IVisual *target, UINT depth);

struct visual_collection
{
    IVisualCollection IVisualCollection_iface;
    IIterable_Visual IIterable_Visual_iface;
    IVisual **items;
    IDCompositionVisual *dcomp_parent;
    IVisual *owner;
    UINT32 count;
    UINT32 capacity;
    LONG ref;
};

static inline struct visual_collection *impl_from_IVisualCollection( IVisualCollection *iface )
{
    return CONTAINING_RECORD( iface, struct visual_collection, IVisualCollection_iface );
}

static HRESULT WINAPI visual_collection_QueryInterface( IVisualCollection *iface, REFIID iid, void **out )
{
    struct visual_collection *impl = impl_from_IVisualCollection( iface );

    if (!out) return E_POINTER;
    if (IsEqualGUID( iid, &IID_IUnknown ) || IsEqualGUID( iid, &IID_IInspectable ) ||
        IsEqualGUID( iid, &IID_IAgileObject ) || IsEqualGUID( iid, &IID_IVisualCollection ))
        *out = &impl->IVisualCollection_iface;
    else if (IsEqualGUID( iid, &IID_IIterable_Visual ))
        *out = &impl->IIterable_Visual_iface;
    else
    {
        *out = NULL;
        return E_NOINTERFACE;
    }
    IVisualCollection_AddRef( &impl->IVisualCollection_iface );
    return S_OK;
}

static ULONG WINAPI visual_collection_AddRef( IVisualCollection *iface )
{
    return InterlockedIncrement( &impl_from_IVisualCollection( iface )->ref );
}

static ULONG WINAPI visual_collection_Release( IVisualCollection *iface )
{
    struct visual_collection *impl = impl_from_IVisualCollection( iface );
    ULONG ref = InterlockedDecrement( &impl->ref );
    UINT32 i;

    if (!ref)
    {
        for (i = 0; i < impl->count; ++i) IVisual_Release( impl->items[i] );
        if (impl->dcomp_parent) IDCompositionVisual_Release( impl->dcomp_parent );
        free( impl->items );
        free( impl );
    }
    return ref;
}

static HRESULT WINAPI visual_collection_GetIids( IVisualCollection *iface, ULONG *count, IID **iids )
{
    IID *values;

    if (!count || !iids) return E_POINTER;
    if (!(values = CoTaskMemAlloc( 2 * sizeof(*values) ))) return E_OUTOFMEMORY;
    values[0] = IID_IVisualCollection;
    values[1] = IID_IIterable_Visual;
    *count = 2;
    *iids = values;
    return S_OK;
}

static HRESULT WINAPI visual_collection_GetRuntimeClassName( IVisualCollection *iface, HSTRING *name )
{
    if (!name) return E_POINTER;
    return WindowsCreateString( RuntimeClass_Windows_UI_Composition_VisualCollection,
                                ARRAY_SIZE(RuntimeClass_Windows_UI_Composition_VisualCollection) - 1, name );
}

static HRESULT WINAPI visual_collection_GetTrustLevel( IVisualCollection *iface, TrustLevel *level )
{
    if (!level) return E_POINTER;
    *level = BaseTrust;
    return S_OK;
}

static HRESULT WINAPI visual_collection_get_Count( IVisualCollection *iface, INT32 *value )
{
    if (!value) return E_POINTER;
    *value = impl_from_IVisualCollection( iface )->count;
    return S_OK;
}

static HRESULT visual_collection_reserve( struct visual_collection *impl )
{
    IVisual **items;
    UINT32 capacity;

    if (impl->count < impl->capacity) return S_OK;
    capacity = impl->capacity ? impl->capacity * 2 : 4;
    if (!(items = realloc( impl->items, capacity * sizeof(*items) ))) return E_OUTOFMEMORY;
    impl->items = items;
    impl->capacity = capacity;
    return S_OK;
}

static HRESULT visual_collection_insert(struct visual_collection *impl, UINT32 index, IVisual *child)
{
    IDCompositionVisual *dcomp_child, *dcomp_reference = NULL;
    HRESULT hr = S_OK;

    if (!child) return E_INVALIDARG;
    EnterCriticalSection(&visual_graph_cs);
    if (index > impl->count) hr = E_INVALIDARG;
    else if (impl->owner && visual_contains(child, impl->owner, 0)) hr = E_INVALIDARG;
    else if (FAILED(hr = visual_collection_reserve(impl))) {}
    else
    {
        memmove(impl->items + index + 1, impl->items + index,
                (impl->count - index) * sizeof(*impl->items));
        IVisual_AddRef(child);
        impl->items[index] = child;
        ++impl->count;
        if (impl->dcomp_parent && (dcomp_child = dcomp_visual_from_visual(child)))
        {
            if (index) dcomp_reference = dcomp_visual_from_visual(impl->items[index - 1]);
            hr = IDCompositionVisual_AddVisual(impl->dcomp_parent, dcomp_child, index != 0, dcomp_reference);
        }
    }
    LeaveCriticalSection(&visual_graph_cs);
    return hr;
}

static HRESULT WINAPI visual_collection_InsertAbove_locked( IVisualCollection *iface,
        IVisual *child, IVisual *sibling )
{
    struct visual_collection *impl = impl_from_IVisualCollection( iface );
    UINT32 i;

    for (i = 0; i < impl->count; ++i)
        if (impl->items[i] == sibling) return visual_collection_insert( impl, i + 1, child );
    return E_INVALIDARG;
}

static HRESULT WINAPI visual_collection_InsertAtBottom_locked( IVisualCollection *iface, IVisual *child )
{
    return visual_collection_insert( impl_from_IVisualCollection( iface ), 0, child );
}

static HRESULT WINAPI visual_collection_InsertAtTop_locked( IVisualCollection *iface, IVisual *child )
{
    struct visual_collection *impl = impl_from_IVisualCollection( iface );
    return visual_collection_insert( impl, impl->count, child );
}

static HRESULT WINAPI visual_collection_InsertBelow_locked( IVisualCollection *iface,
        IVisual *child, IVisual *sibling )
{
    struct visual_collection *impl = impl_from_IVisualCollection( iface );
    UINT32 i;

    for (i = 0; i < impl->count; ++i)
        if (impl->items[i] == sibling) return visual_collection_insert( impl, i, child );
    return E_INVALIDARG;
}

static HRESULT WINAPI visual_collection_Remove_locked( IVisualCollection *iface, IVisual *child )
{
    struct visual_collection *impl = impl_from_IVisualCollection( iface );
    UINT32 i;

    for (i = 0; i < impl->count; ++i)
    {
        if (impl->items[i] != child) continue;
        if (impl->dcomp_parent)
        {
            IDCompositionVisual *dcomp_child = dcomp_visual_from_visual( child );
            if (dcomp_child) IDCompositionVisual_RemoveVisual( impl->dcomp_parent, dcomp_child );
        }
        IVisual_Release( impl->items[i] );
        memmove( impl->items + i, impl->items + i + 1,
                 (impl->count - i - 1) * sizeof(*impl->items) );
        --impl->count;
        return S_OK;
    }
    return E_INVALIDARG;
}

static HRESULT WINAPI visual_collection_RemoveAll_locked( IVisualCollection *iface )
{
    struct visual_collection *impl = impl_from_IVisualCollection( iface );

    if (impl->dcomp_parent) IDCompositionVisual_RemoveAllVisuals( impl->dcomp_parent );
    while (impl->count) IVisual_Release( impl->items[--impl->count] );
    return S_OK;
}

struct visual_iterator
{
    IIterator_Visual IIterator_Visual_iface;
    struct visual_collection *collection;
    UINT32 index;
    LONG ref;
};

static inline struct visual_iterator *impl_from_IIterator_Visual( IIterator_Visual *iface )
{
    return CONTAINING_RECORD( iface, struct visual_iterator, IIterator_Visual_iface );
}

static HRESULT WINAPI visual_iterator_QueryInterface( IIterator_Visual *iface, REFIID iid, void **out )
{
    if (!out) return E_POINTER;
    if (IsEqualGUID( iid, &IID_IUnknown ) || IsEqualGUID( iid, &IID_IInspectable ) ||
        IsEqualGUID( iid, &IID_IAgileObject ) || IsEqualGUID( iid, &IID_IIterator_Visual ))
    {
        *out = iface;
        IIterator_Visual_AddRef( iface );
        return S_OK;
    }
    *out = NULL;
    return E_NOINTERFACE;
}

static ULONG WINAPI visual_iterator_AddRef( IIterator_Visual *iface )
{
    return InterlockedIncrement( &impl_from_IIterator_Visual( iface )->ref );
}

static ULONG WINAPI visual_iterator_Release( IIterator_Visual *iface )
{
    struct visual_iterator *impl = impl_from_IIterator_Visual( iface );
    ULONG ref = InterlockedDecrement( &impl->ref );

    if (!ref)
    {
        IVisualCollection_Release( &impl->collection->IVisualCollection_iface );
        free( impl );
    }
    return ref;
}

static HRESULT WINAPI visual_iterator_GetIids( IIterator_Visual *iface, ULONG *count, IID **iids )
{
    if (!count || !iids) return E_POINTER;
    if (!(*iids = CoTaskMemAlloc( sizeof(**iids) ))) return E_OUTOFMEMORY;
    **iids = IID_IIterator_Visual;
    *count = 1;
    return S_OK;
}

static HRESULT WINAPI visual_iterator_GetRuntimeClassName( IIterator_Visual *iface, HSTRING *name )
{
    if (!name) return E_POINTER;
    return WindowsCreateString( RuntimeClass_Windows_UI_Composition_VisualCollection,
                                ARRAY_SIZE(RuntimeClass_Windows_UI_Composition_VisualCollection) - 1, name );
}

static HRESULT WINAPI visual_iterator_GetTrustLevel( IIterator_Visual *iface, TrustLevel *level )
{
    if (!level) return E_POINTER;
    *level = BaseTrust;
    return S_OK;
}

static HRESULT WINAPI visual_iterator_get_Current( IIterator_Visual *iface, IVisual **value )
{
    struct visual_iterator *impl = impl_from_IIterator_Visual( iface );

    if (!value) return E_POINTER;
    *value = NULL;
    if (impl->index >= impl->collection->count) return E_BOUNDS;
    *value = impl->collection->items[impl->index];
    IVisual_AddRef( *value );
    return S_OK;
}

static HRESULT WINAPI visual_iterator_get_HasCurrent( IIterator_Visual *iface, boolean *value )
{
    struct visual_iterator *impl = impl_from_IIterator_Visual( iface );

    if (!value) return E_POINTER;
    *value = impl->index < impl->collection->count;
    return S_OK;
}

static HRESULT WINAPI visual_iterator_MoveNext( IIterator_Visual *iface, boolean *value )
{
    struct visual_iterator *impl = impl_from_IIterator_Visual( iface );

    if (!value) return E_POINTER;
    if (impl->index < impl->collection->count) ++impl->index;
    *value = impl->index < impl->collection->count;
    return S_OK;
}

static HRESULT WINAPI visual_iterator_GetMany( IIterator_Visual *iface, UINT32 capacity,
        IVisual **items, UINT32 *count )
{
    struct visual_iterator *impl = impl_from_IIterator_Visual( iface );
    UINT32 i, available = 0;

    if (!count || (capacity && !items)) return E_POINTER;
    *count = 0;
    if (impl->index < impl->collection->count)
        available = min( capacity, impl->collection->count - impl->index );
    for (i = 0; i < available; ++i)
    {
        items[i] = impl->collection->items[impl->index + i];
        IVisual_AddRef( items[i] );
    }
    impl->index += available;
    *count = available;
    return S_OK;
}

static const IIterator_VisualVtbl visual_iterator_vtbl =
{
    visual_iterator_QueryInterface,
    visual_iterator_AddRef,
    visual_iterator_Release,
    visual_iterator_GetIids,
    visual_iterator_GetRuntimeClassName,
    visual_iterator_GetTrustLevel,
    visual_iterator_get_Current,
    visual_iterator_get_HasCurrent,
    visual_iterator_MoveNext,
    visual_iterator_GetMany,
};

DEFINE_IINSPECTABLE_( visual_collection_iterable, IIterable_Visual, struct visual_collection,
                      impl_from_IIterable_Visual, IIterable_Visual_iface,
                      &impl->IVisualCollection_iface )

static HRESULT WINAPI visual_collection_iterable_First( IIterable_Visual *iface,
        IIterator_Visual **value )
{
    struct visual_collection *collection = impl_from_IIterable_Visual( iface );
    struct visual_iterator *iterator;

    if (!value) return E_POINTER;
    *value = NULL;
    if (!(iterator = calloc( 1, sizeof(*iterator) ))) return E_OUTOFMEMORY;
    iterator->IIterator_Visual_iface.lpVtbl = &visual_iterator_vtbl;
    iterator->collection = collection;
    iterator->ref = 1;
    IVisualCollection_AddRef( &collection->IVisualCollection_iface );
    *value = &iterator->IIterator_Visual_iface;
    return S_OK;
}

static const IIterable_VisualVtbl visual_collection_iterable_vtbl =
{
    visual_collection_iterable_QueryInterface,
    visual_collection_iterable_AddRef,
    visual_collection_iterable_Release,
    visual_collection_iterable_GetIids,
    visual_collection_iterable_GetRuntimeClassName,
    visual_collection_iterable_GetTrustLevel,
    visual_collection_iterable_First,
};

static HRESULT WINAPI visual_collection_InsertAbove( IVisualCollection *iface,
        IVisual *child, IVisual *sibling )
{
    HRESULT hr;

    EnterCriticalSection(&visual_graph_cs);
    hr = visual_collection_InsertAbove_locked(iface, child, sibling);
    LeaveCriticalSection(&visual_graph_cs);
    return hr;
}

static HRESULT WINAPI visual_collection_InsertBelow( IVisualCollection *iface,
        IVisual *child, IVisual *sibling )
{
    HRESULT hr;

    EnterCriticalSection(&visual_graph_cs);
    hr = visual_collection_InsertBelow_locked(iface, child, sibling);
    LeaveCriticalSection(&visual_graph_cs);
    return hr;
}

static HRESULT WINAPI visual_collection_InsertAtTop( IVisualCollection *iface, IVisual *child )
{
    HRESULT hr;

    EnterCriticalSection(&visual_graph_cs);
    hr = visual_collection_InsertAtTop_locked(iface, child);
    LeaveCriticalSection(&visual_graph_cs);
    return hr;
}

static HRESULT WINAPI visual_collection_InsertAtBottom( IVisualCollection *iface, IVisual *child )
{
    HRESULT hr;

    EnterCriticalSection(&visual_graph_cs);
    hr = visual_collection_InsertAtBottom_locked(iface, child);
    LeaveCriticalSection(&visual_graph_cs);
    return hr;
}

static HRESULT WINAPI visual_collection_Remove( IVisualCollection *iface, IVisual *child )
{
    HRESULT hr;

    EnterCriticalSection(&visual_graph_cs);
    hr = visual_collection_Remove_locked(iface, child);
    LeaveCriticalSection(&visual_graph_cs);
    return hr;
}

static HRESULT WINAPI visual_collection_RemoveAll( IVisualCollection *iface )
{
    HRESULT hr;

    EnterCriticalSection(&visual_graph_cs);
    hr = visual_collection_RemoveAll_locked(iface);
    LeaveCriticalSection(&visual_graph_cs);
    return hr;
}

static const IVisualCollectionVtbl visual_collection_vtbl =
{
    visual_collection_QueryInterface,
    visual_collection_AddRef,
    visual_collection_Release,
    visual_collection_GetIids,
    visual_collection_GetRuntimeClassName,
    visual_collection_GetTrustLevel,
    visual_collection_get_Count,
    visual_collection_InsertAbove,
    visual_collection_InsertAtBottom,
    visual_collection_InsertAtTop,
    visual_collection_InsertBelow,
    visual_collection_Remove,
    visual_collection_RemoveAll,
};

static HRESULT visual_collection_create( IDCompositionVisual *parent, IVisualCollection **result )
{
    struct visual_collection *impl;

    if (!result) return E_POINTER;
    *result = NULL;
    if (!(impl = calloc( 1, sizeof(*impl) ))) return E_OUTOFMEMORY;
    impl->IVisualCollection_iface.lpVtbl = &visual_collection_vtbl;
    impl->IIterable_Visual_iface.lpVtbl = &visual_collection_iterable_vtbl;
    impl->dcomp_parent = parent;
    if (parent) IDCompositionVisual_AddRef( parent );
    impl->ref = 1;
    *result = &impl->IVisualCollection_iface;
    return S_OK;
}

struct inset_clip
{
    IInsetClip IInsetClip_iface;
    ICompositionClip ICompositionClip_iface;
    ICompositionObject ICompositionObject_iface;
    ICompositor *compositor;
    FLOAT bottom, left, right, top;
    LONG ref;
};

static inline struct inset_clip *impl_from_IInsetClip( IInsetClip *iface )
{
    return CONTAINING_RECORD( iface, struct inset_clip, IInsetClip_iface );
}

static inline struct inset_clip *impl_from_ICompositionClip( ICompositionClip *iface )
{
    return CONTAINING_RECORD( iface, struct inset_clip, ICompositionClip_iface );
}

static inline struct inset_clip *inset_clip_from_ICompositionObject( ICompositionObject *iface )
{
    return CONTAINING_RECORD( iface, struct inset_clip, ICompositionObject_iface );
}

static HRESULT WINAPI inset_clip_QueryInterface( IInsetClip *iface, REFIID iid, void **out )
{
    struct inset_clip *impl = impl_from_IInsetClip( iface );

    if (!out) return E_POINTER;
    if (IsEqualGUID( iid, &IID_IUnknown ) || IsEqualGUID( iid, &IID_IInspectable ) ||
        IsEqualGUID( iid, &IID_IAgileObject ) || IsEqualGUID( iid, &IID_IInsetClip ))
        *out = iface;
    else if (IsEqualGUID( iid, &IID_ICompositionClip ))
        *out = &impl->ICompositionClip_iface;
    else if (IsEqualGUID( iid, &IID_ICompositionObject ))
        *out = &impl->ICompositionObject_iface;
    else
    {
        *out = NULL;
        return E_NOINTERFACE;
    }
    IInsetClip_AddRef( iface );
    return S_OK;
}

static ULONG WINAPI inset_clip_AddRef( IInsetClip *iface )
{
    return InterlockedIncrement( &impl_from_IInsetClip( iface )->ref );
}

static ULONG WINAPI inset_clip_Release( IInsetClip *iface )
{
    struct inset_clip *impl = impl_from_IInsetClip( iface );
    ULONG ref = InterlockedDecrement( &impl->ref );

    if (!ref)
    {
        ICompositor_Release( impl->compositor );
        free( impl );
    }
    return ref;
}

static HRESULT WINAPI inset_clip_GetIids( IInsetClip *iface, ULONG *count, IID **iids )
{
    IID *values;

    if (!count || !iids) return E_POINTER;
    if (!(values = CoTaskMemAlloc( 3 * sizeof(*values) ))) return E_OUTOFMEMORY;
    values[0] = IID_IInsetClip;
    values[1] = IID_ICompositionClip;
    values[2] = IID_ICompositionObject;
    *count = 3;
    *iids = values;
    return S_OK;
}

static HRESULT WINAPI inset_clip_GetRuntimeClassName( IInsetClip *iface, HSTRING *name )
{
    if (!name) return E_POINTER;
    return WindowsCreateString( RuntimeClass_Windows_UI_Composition_InsetClip,
                                ARRAY_SIZE(RuntimeClass_Windows_UI_Composition_InsetClip) - 1, name );
}

static HRESULT WINAPI inset_clip_GetTrustLevel( IInsetClip *iface, TrustLevel *level )
{
    if (!level) return E_POINTER;
    *level = BaseTrust;
    return S_OK;
}

#define INSET_CLIP_PROPERTY(name, field) \
    static HRESULT WINAPI inset_clip_get_##name( IInsetClip *iface, FLOAT *value ) \
    { \
        if (!value) return E_POINTER; \
        *value = impl_from_IInsetClip( iface )->field; \
        return S_OK; \
    } \
    static HRESULT WINAPI inset_clip_put_##name( IInsetClip *iface, FLOAT value ) \
    { \
        impl_from_IInsetClip( iface )->field = value; \
        return S_OK; \
    }

INSET_CLIP_PROPERTY( BottomInset, bottom )
INSET_CLIP_PROPERTY( LeftInset, left )
INSET_CLIP_PROPERTY( RightInset, right )
INSET_CLIP_PROPERTY( TopInset, top )

static const IInsetClipVtbl inset_clip_vtbl =
{
    inset_clip_QueryInterface,
    inset_clip_AddRef,
    inset_clip_Release,
    inset_clip_GetIids,
    inset_clip_GetRuntimeClassName,
    inset_clip_GetTrustLevel,
    inset_clip_get_BottomInset,
    inset_clip_put_BottomInset,
    inset_clip_get_LeftInset,
    inset_clip_put_LeftInset,
    inset_clip_get_RightInset,
    inset_clip_put_RightInset,
    inset_clip_get_TopInset,
    inset_clip_put_TopInset,
};

static HRESULT WINAPI composition_clip_QueryInterface( ICompositionClip *iface, REFIID iid, void **out )
{
    struct inset_clip *impl = impl_from_ICompositionClip( iface );
    return IInsetClip_QueryInterface( &impl->IInsetClip_iface, iid, out );
}

static ULONG WINAPI composition_clip_AddRef( ICompositionClip *iface )
{
    struct inset_clip *impl = impl_from_ICompositionClip( iface );
    return IInsetClip_AddRef( &impl->IInsetClip_iface );
}

static ULONG WINAPI composition_clip_Release( ICompositionClip *iface )
{
    struct inset_clip *impl = impl_from_ICompositionClip( iface );
    return IInsetClip_Release( &impl->IInsetClip_iface );
}

static HRESULT WINAPI composition_clip_GetIids( ICompositionClip *iface, ULONG *count, IID **iids )
{
    struct inset_clip *impl = impl_from_ICompositionClip( iface );
    return IInsetClip_GetIids( &impl->IInsetClip_iface, count, iids );
}

static HRESULT WINAPI composition_clip_GetRuntimeClassName( ICompositionClip *iface, HSTRING *name )
{
    struct inset_clip *impl = impl_from_ICompositionClip( iface );
    return IInsetClip_GetRuntimeClassName( &impl->IInsetClip_iface, name );
}

static HRESULT WINAPI composition_clip_GetTrustLevel( ICompositionClip *iface, TrustLevel *level )
{
    struct inset_clip *impl = impl_from_ICompositionClip( iface );
    return IInsetClip_GetTrustLevel( &impl->IInsetClip_iface, level );
}

static const ICompositionClipVtbl composition_clip_vtbl =
{
    composition_clip_QueryInterface,
    composition_clip_AddRef,
    composition_clip_Release,
    composition_clip_GetIids,
    composition_clip_GetRuntimeClassName,
    composition_clip_GetTrustLevel,
};

static HRESULT WINAPI inset_clip_composition_object_QueryInterface( ICompositionObject *iface,
        REFIID iid, void **out )
{
    struct inset_clip *impl = inset_clip_from_ICompositionObject( iface );
    return IInsetClip_QueryInterface( &impl->IInsetClip_iface, iid, out );
}

static ULONG WINAPI inset_clip_composition_object_AddRef( ICompositionObject *iface )
{
    struct inset_clip *impl = inset_clip_from_ICompositionObject( iface );
    return IInsetClip_AddRef( &impl->IInsetClip_iface );
}

static ULONG WINAPI inset_clip_composition_object_Release( ICompositionObject *iface )
{
    struct inset_clip *impl = inset_clip_from_ICompositionObject( iface );
    return IInsetClip_Release( &impl->IInsetClip_iface );
}

static HRESULT WINAPI inset_clip_composition_object_GetIids( ICompositionObject *iface,
        ULONG *count, IID **iids )
{
    struct inset_clip *impl = inset_clip_from_ICompositionObject( iface );
    return IInsetClip_GetIids( &impl->IInsetClip_iface, count, iids );
}

static HRESULT WINAPI inset_clip_composition_object_GetRuntimeClassName( ICompositionObject *iface,
        HSTRING *name )
{
    struct inset_clip *impl = inset_clip_from_ICompositionObject( iface );
    return IInsetClip_GetRuntimeClassName( &impl->IInsetClip_iface, name );
}

static HRESULT WINAPI inset_clip_composition_object_GetTrustLevel( ICompositionObject *iface,
        TrustLevel *level )
{
    struct inset_clip *impl = inset_clip_from_ICompositionObject( iface );
    return IInsetClip_GetTrustLevel( &impl->IInsetClip_iface, level );
}

static HRESULT WINAPI inset_clip_composition_object_get_Compositor( ICompositionObject *iface,
        ICompositor **value )
{
    struct inset_clip *impl = inset_clip_from_ICompositionObject( iface );

    if (!value) return E_POINTER;
    ICompositor_AddRef( impl->compositor );
    *value = impl->compositor;
    return S_OK;
}

static HRESULT WINAPI inset_clip_composition_object_get_Dispatcher( ICompositionObject *iface,
        ICoreDispatcher **value )
{
    if (!value) return E_POINTER;
    *value = NULL;
    return S_OK;
}

static HRESULT WINAPI inset_clip_composition_object_get_Properties( ICompositionObject *iface,
        ICompositionPropertySet **value )
{
    if (!value) return E_POINTER;
    *value = NULL;
    return S_OK;
}

static HRESULT WINAPI inset_clip_composition_object_StartAnimation( ICompositionObject *iface,
        HSTRING name, ICompositionAnimation *animation )
{
    TRACE( "iface %p, name %s, animation %p.\n", iface, debugstr_hstring( name ), animation );
    return S_OK;
}

static HRESULT WINAPI inset_clip_composition_object_StopAnimation( ICompositionObject *iface,
        HSTRING name )
{
    TRACE( "iface %p, name %s.\n", iface, debugstr_hstring( name ) );
    return S_OK;
}

static const ICompositionObjectVtbl inset_clip_composition_object_vtbl =
{
    inset_clip_composition_object_QueryInterface,
    inset_clip_composition_object_AddRef,
    inset_clip_composition_object_Release,
    inset_clip_composition_object_GetIids,
    inset_clip_composition_object_GetRuntimeClassName,
    inset_clip_composition_object_GetTrustLevel,
    inset_clip_composition_object_get_Compositor,
    inset_clip_composition_object_get_Dispatcher,
    inset_clip_composition_object_get_Properties,
    inset_clip_composition_object_StartAnimation,
    inset_clip_composition_object_StopAnimation,
};

static HRESULT inset_clip_create( ICompositor *compositor, FLOAT left, FLOAT top, FLOAT right,
        FLOAT bottom, IInsetClip **result )
{
    struct inset_clip *impl;

    if (!result) return E_POINTER;
    *result = NULL;
    if (!(impl = calloc( 1, sizeof(*impl) ))) return E_OUTOFMEMORY;
    impl->IInsetClip_iface.lpVtbl = &inset_clip_vtbl;
    impl->ICompositionClip_iface.lpVtbl = &composition_clip_vtbl;
    impl->ICompositionObject_iface.lpVtbl = &inset_clip_composition_object_vtbl;
    impl->compositor = compositor;
    impl->left = left;
    impl->top = top;
    impl->right = right;
    impl->bottom = bottom;
    impl->ref = 1;
    ICompositor_AddRef( compositor );
    *result = &impl->IInsetClip_iface;
    return S_OK;
}

static const GUID composition_object2_iid = {0xef874ea1,0x5cff,0x4b68,{0x9e,0x30,0xa1,0x51,0x9d,0x08,0xba,0x03}};
static const GUID implicit_collection_iid = {0x0598a3ff,0x0a92,0x4c9d,{0xa4,0x27,0xb2,0x55,0x19,0x25,0x0d,0xbf}};
static const GUID animation_group_iid = {0x5e7cc90c,0xcd14,0x4e07,{0x8a,0x55,0xc7,0x25,0x27,0xaa,0xbd,0xac}};
static const GUID animation2_iid = {0x369b603e,0xa80f,0x4948,{0x93,0xe3,0xed,0x23,0xfb,0x38,0xc6,0xcb}};
static const GUID animation_iterable_iid = {0x0cb59942,0xc6f3,0x585f,{0x9a,0x92,0xb4,0x7e,0x8d,0xd3,0x8e,0xbf}};
static const GUID animation_iterator_iid = {0x98efbe55,0x0a66,0x556a,{0x9a,0xdc,0x4a,0xb5,0x9c,0x69,0x68,0x46}};
static const GUID implicit_map_iid = {0x4ba9aa1d,0x57ac,0x54de,{0xa8,0x07,0xb5,0x2e,0x68,0x9b,0xfc,0x04}};
#define OBJECT2_INSPECTABLE(type) \
    HRESULT (WINAPI *QueryInterface)(type *,REFIID,void **); \
    ULONG (WINAPI *AddRef)(type *); \
    ULONG (WINAPI *Release)(type *); \
    HRESULT (WINAPI *GetIids)(type *,ULONG *,IID **); \
    HRESULT (WINAPI *GetRuntimeClassName)(type *,HSTRING *); \
    HRESULT (WINAPI *GetTrustLevel)(type *,TrustLevel *)
struct composition_object2_iface;
struct composition_object2_vtbl
{
    OBJECT2_INSPECTABLE(struct composition_object2_iface);
    HRESULT (WINAPI *get_Comment)(struct composition_object2_iface *,HSTRING *);
    HRESULT (WINAPI *put_Comment)(struct composition_object2_iface *,HSTRING);
    HRESULT (WINAPI *get_ImplicitAnimations)(struct composition_object2_iface *,IInspectable **);
    HRESULT (WINAPI *put_ImplicitAnimations)(struct composition_object2_iface *,IInspectable *);
    HRESULT (WINAPI *StartAnimationGroup)(struct composition_object2_iface *,ICompositionAnimationBase *);
    HRESULT (WINAPI *StopAnimationGroup)(struct composition_object2_iface *,ICompositionAnimationBase *);
};
struct composition_object2_iface { const struct composition_object2_vtbl *lpVtbl; };
struct animation_group_iface;
struct animation_group_vtbl
{
    OBJECT2_INSPECTABLE(struct animation_group_iface);
    HRESULT (WINAPI *get_Count)(struct animation_group_iface *,INT32 *);
    HRESULT (WINAPI *Add)(struct animation_group_iface *,ICompositionAnimation *);
    HRESULT (WINAPI *Remove)(struct animation_group_iface *,ICompositionAnimation *);
    HRESULT (WINAPI *RemoveAll)(struct animation_group_iface *);
};
struct animation_group_iface { const struct animation_group_vtbl *lpVtbl; };
struct animation2_iface;
struct animation2_vtbl
{
    OBJECT2_INSPECTABLE(struct animation2_iface);
    HRESULT (WINAPI *SetBooleanParameter)(struct animation2_iface *,HSTRING,boolean);
    HRESULT (WINAPI *get_Target)(struct animation2_iface *,HSTRING *);
    HRESULT (WINAPI *put_Target)(struct animation2_iface *,HSTRING);
};
struct animation2_iface { const struct animation2_vtbl *lpVtbl; };
struct animation_iterator_iface;
struct animation_iterator_vtbl
{
    OBJECT2_INSPECTABLE(struct animation_iterator_iface);
    HRESULT (WINAPI *get_Current)(struct animation_iterator_iface *,ICompositionAnimation **);
    HRESULT (WINAPI *get_HasCurrent)(struct animation_iterator_iface *,boolean *);
    HRESULT (WINAPI *MoveNext)(struct animation_iterator_iface *,boolean *);
    HRESULT (WINAPI *GetMany)(struct animation_iterator_iface *,UINT32,ICompositionAnimation **,UINT32 *);
};
struct animation_iterator_iface { const struct animation_iterator_vtbl *lpVtbl; };
struct animation_iterable_iface;
struct animation_iterable_vtbl
{
    OBJECT2_INSPECTABLE(struct animation_iterable_iface);
    HRESULT (WINAPI *First)(struct animation_iterable_iface *,struct animation_iterator_iface **);
};
struct animation_iterable_iface { const struct animation_iterable_vtbl *lpVtbl; };
struct implicit_view_iface;
struct implicit_map_iface;
struct implicit_map_vtbl
{
    OBJECT2_INSPECTABLE(struct implicit_map_iface);
    HRESULT (WINAPI *Lookup)(struct implicit_map_iface *,HSTRING,ICompositionAnimationBase **);
    HRESULT (WINAPI *get_Size)(struct implicit_map_iface *,UINT32 *);
    HRESULT (WINAPI *HasKey)(struct implicit_map_iface *,HSTRING,boolean *);
    HRESULT (WINAPI *GetView)(struct implicit_map_iface *,struct implicit_view_iface **);
    HRESULT (WINAPI *Insert)(struct implicit_map_iface *,HSTRING,ICompositionAnimationBase *,boolean *);
    HRESULT (WINAPI *Remove)(struct implicit_map_iface *,HSTRING);
    HRESULT (WINAPI *Clear)(struct implicit_map_iface *);
};
struct implicit_map_iface { const struct implicit_map_vtbl *lpVtbl; };
#undef OBJECT2_INSPECTABLE
struct sprite_visual;
static HRESULT visual_apply_implicit_animation(struct sprite_visual *impl,const WCHAR *name);

struct sprite_visual
{
    ISpriteVisual ISpriteVisual_iface;
    struct redirect_visual_iface redirect_iface;
    IVisual *source;
    BOOL is_redirect;
    struct composition_object2_iface object2_iface;
    HSTRING comment;
    IInspectable *implicit_animations;
    IVisual IVisual_iface;
    IVisual2 IVisual2_iface;
    IContainerVisual IContainerVisual_iface;
    ICompositionObject ICompositionObject_iface;
    IDCompositionVisual *dcomp_visual;
    IVisualCollection *children;
    ICompositionPropertySet *properties;
    ICompositor *compositor;
    ICompositionBrush *brush;
    ICompositionClip *clip;
    IVisual *parent_for_transform;
    IExpressionAnimation *scale_expression;
    IExpressionAnimation *offset_expression;
    IExpressionAnimation *center_expression;
    struct Vector2 anchor_point;
    struct Vector2 relative_size_adjustment;
    struct Vector3 center_point;
    struct Vector3 offset;
    struct Vector3 relative_offset_adjustment;
    struct Quaternion orientation;
    struct Vector3 rotation_axis;
    struct Vector3 scale;
    struct Vector2 size;
    struct Matrix4x4 transform_matrix;
    enum CompositionBackfaceVisibility backface_visibility;
    enum CompositionBorderMode border_mode;
    enum CompositionCompositeMode composite_mode;
    FLOAT opacity;
    FLOAT rotation_angle;
    FLOAT rotation_angle_degrees;
    boolean visible;
    LONG ref;
};

static inline struct sprite_visual *impl_from_ISpriteVisual( ISpriteVisual *iface )
{
    return CONTAINING_RECORD( iface, struct sprite_visual, ISpriteVisual_iface );
}

static inline struct sprite_visual *sprite_visual_from_IVisual( IVisual *iface )
{
    return CONTAINING_RECORD( iface, struct sprite_visual, IVisual_iface );
}

static IDCompositionVisual *dcomp_visual_from_visual( IVisual *iface )
{
    if (!iface) return NULL;
    return sprite_visual_from_IVisual( iface )->dcomp_visual;
}

static inline struct sprite_visual *sprite_visual_from_IContainerVisual( IContainerVisual *iface )
{
    return CONTAINING_RECORD( iface, struct sprite_visual, IContainerVisual_iface );
}

static inline struct sprite_visual *sprite_visual_from_IVisual2( IVisual2 *iface )
{
    return CONTAINING_RECORD( iface, struct sprite_visual, IVisual2_iface );
}

static inline struct sprite_visual *sprite_visual_from_ICompositionObject( ICompositionObject *iface )
{
    return CONTAINING_RECORD( iface, struct sprite_visual, ICompositionObject_iface );
}

static HRESULT WINAPI sprite_visual_QueryInterface( ISpriteVisual *iface, REFIID iid, void **out )
{
    FIXME( "iface %p, iid %s, out %p.\n", iface, debugstr_guid( iid ), out );
    if (!out) return E_POINTER;
    if (IsEqualGUID( iid, &IID_IUnknown ) || IsEqualGUID( iid, &IID_IInspectable ) ||
        IsEqualGUID( iid, &IID_IAgileObject ))
        *out = impl_from_ISpriteVisual(iface)->is_redirect ?
                (void *)&impl_from_ISpriteVisual(iface)->redirect_iface : (void *)iface;
    else if (IsEqualGUID(iid, &redirect_visual_iid) && impl_from_ISpriteVisual(iface)->is_redirect)
        *out = &impl_from_ISpriteVisual(iface)->redirect_iface;
    else if (IsEqualGUID(iid, &IID_ISpriteVisual) && !impl_from_ISpriteVisual(iface)->is_redirect)
        *out = iface;
    else if (IsEqualGUID(iid,&composition_object2_iid))
        *out = &impl_from_ISpriteVisual(iface)->object2_iface;
    else if (IsEqualGUID( iid, &IID_IVisual ))
        *out = &impl_from_ISpriteVisual( iface )->IVisual_iface;
    else if (IsEqualGUID( iid, &IID_IVisual2 ))
        *out = &impl_from_ISpriteVisual( iface )->IVisual2_iface;
    else if (IsEqualGUID( iid, &IID_IContainerVisual ))
        *out = &impl_from_ISpriteVisual( iface )->IContainerVisual_iface;
    else if (IsEqualGUID( iid, &IID_ICompositionObject ))
        *out = &impl_from_ISpriteVisual( iface )->ICompositionObject_iface;
    else
    {
        *out = NULL;
        return E_NOINTERFACE;
    }
    IUnknown_AddRef( (IUnknown *)*out );
    return S_OK;
}

static ULONG WINAPI sprite_visual_AddRef( ISpriteVisual *iface )
{
    return InterlockedIncrement( &impl_from_ISpriteVisual( iface )->ref );
}

static ULONG WINAPI sprite_visual_Release( ISpriteVisual *iface )
{
    struct sprite_visual *impl = impl_from_ISpriteVisual( iface );
    ULONG ref = InterlockedDecrement( &impl->ref );

    if (!ref)
    {
        if (impl->brush)
        {
            ICompositionSurfaceBrush *surface_brush;
            struct nine_grid_brush_iface *nine_grid_brush;

            if (SUCCEEDED(ICompositionBrush_QueryInterface( impl->brush,
                    &IID_ICompositionSurfaceBrush, (void **)&surface_brush )))
            {
                surface_brush_bind( surface_brush, NULL );
                ICompositionSurfaceBrush_Release( surface_brush );
            }
            if (SUCCEEDED(ICompositionBrush_QueryInterface( impl->brush,
                    &composition_nine_grid_brush_iid, (void **)&nine_grid_brush )))
            {
                nine_grid_brush_bind( nine_grid_brush, NULL );
                IUnknown_Release( (IUnknown *)nine_grid_brush );
            }
            ICompositionBrush_Release( impl->brush );
        }
        if (impl->scale_expression) IExpressionAnimation_Release( impl->scale_expression );
        if (impl->offset_expression) IExpressionAnimation_Release( impl->offset_expression );
        if (impl->center_expression) IExpressionAnimation_Release( impl->center_expression );
        if (impl->clip) ICompositionClip_Release( impl->clip );
        if (impl->parent_for_transform) IVisual_Release( impl->parent_for_transform );
        EnterCriticalSection(&visual_graph_cs);
        if (impl->children) impl_from_IVisualCollection(impl->children)->owner = NULL;
        LeaveCriticalSection(&visual_graph_cs);
        WindowsDeleteString(impl->comment);
        if (impl->implicit_animations) IInspectable_Release(impl->implicit_animations);
        if (impl->source) IVisual_Release(impl->source);
        if (impl->children) IVisualCollection_Release( impl->children );
        if (impl->properties) ICompositionPropertySet_Release( impl->properties );
        if (impl->compositor) ICompositor_Release( impl->compositor );
        if (impl->dcomp_visual) IDCompositionVisual_Release( impl->dcomp_visual );
        free( impl );
    }
    return ref;
}

static HRESULT WINAPI sprite_visual_GetIids( ISpriteVisual *iface, ULONG *count, IID **iids )
{
    IID *values;

    if (!count || !iids) return E_POINTER;
    if (!(values = CoTaskMemAlloc( 6 * sizeof(*values) ))) return E_OUTOFMEMORY;
    values[0] = impl_from_ISpriteVisual(iface)->is_redirect ? redirect_visual_iid : IID_ISpriteVisual;
    values[1] = IID_IVisual;
    values[2] = IID_IContainerVisual;
    values[3] = IID_IVisual2;
    values[4] = IID_ICompositionObject;
    values[5] = composition_object2_iid;
    *count = 6;
    *iids = values;
    return S_OK;
}

static HRESULT WINAPI sprite_visual_GetRuntimeClassName( ISpriteVisual *iface, HSTRING *name )
{
    if (!name) return E_POINTER;
    if (impl_from_ISpriteVisual(iface)->is_redirect)
        return WindowsCreateString(L"Windows.UI.Composition.RedirectVisual", 37, name);
    return WindowsCreateString( RuntimeClass_Windows_UI_Composition_SpriteVisual,
                                ARRAY_SIZE(RuntimeClass_Windows_UI_Composition_SpriteVisual) - 1, name );
}

static HRESULT WINAPI sprite_visual_GetTrustLevel( ISpriteVisual *iface, TrustLevel *level )
{
    if (!level) return E_POINTER;
    *level = BaseTrust;
    return S_OK;
}

static HRESULT WINAPI sprite_visual_composition_object_QueryInterface( ICompositionObject *iface,
        REFIID iid, void **out )
{
    struct sprite_visual *impl = sprite_visual_from_ICompositionObject( iface );
    return ISpriteVisual_QueryInterface( &impl->ISpriteVisual_iface, iid, out );
}

static ULONG WINAPI sprite_visual_composition_object_AddRef( ICompositionObject *iface )
{
    struct sprite_visual *impl = sprite_visual_from_ICompositionObject( iface );
    return ISpriteVisual_AddRef( &impl->ISpriteVisual_iface );
}

static ULONG WINAPI sprite_visual_composition_object_Release( ICompositionObject *iface )
{
    struct sprite_visual *impl = sprite_visual_from_ICompositionObject( iface );
    return ISpriteVisual_Release( &impl->ISpriteVisual_iface );
}

static HRESULT WINAPI sprite_visual_composition_object_GetIids( ICompositionObject *iface,
        ULONG *count, IID **iids )
{
    struct sprite_visual *impl = sprite_visual_from_ICompositionObject( iface );
    return ISpriteVisual_GetIids( &impl->ISpriteVisual_iface, count, iids );
}

static HRESULT WINAPI sprite_visual_composition_object_GetRuntimeClassName( ICompositionObject *iface,
        HSTRING *name )
{
    struct sprite_visual *impl = sprite_visual_from_ICompositionObject( iface );
    return ISpriteVisual_GetRuntimeClassName( &impl->ISpriteVisual_iface, name );
}

static HRESULT WINAPI sprite_visual_composition_object_GetTrustLevel( ICompositionObject *iface,
        TrustLevel *level )
{
    struct sprite_visual *impl = sprite_visual_from_ICompositionObject( iface );
    return ISpriteVisual_GetTrustLevel( &impl->ISpriteVisual_iface, level );
}

static HRESULT WINAPI sprite_visual_composition_object_get_Compositor( ICompositionObject *iface,
        ICompositor **value )
{
    struct sprite_visual *impl = sprite_visual_from_ICompositionObject( iface );

    if (!value) return E_POINTER;
    ICompositor_AddRef( impl->compositor );
    *value = impl->compositor;
    return S_OK;
}

static HRESULT WINAPI sprite_visual_composition_object_get_Dispatcher( ICompositionObject *iface,
        ICoreDispatcher **value )
{
    if (!value) return E_POINTER;
    *value = NULL;
    return S_OK;
}

static HRESULT WINAPI sprite_visual_composition_object_get_Properties( ICompositionObject *iface,
        ICompositionPropertySet **value )
{
    struct sprite_visual *impl = sprite_visual_from_ICompositionObject( iface );

    if (!value) return E_POINTER;
    ICompositionPropertySet_AddRef( impl->properties );
    *value = impl->properties;
    return S_OK;
}

static HRESULT WINAPI sprite_visual_composition_object_StartAnimation( ICompositionObject *iface,
        HSTRING name, ICompositionAnimation *animation )
{
    struct sprite_visual *impl = sprite_visual_from_ICompositionObject( iface );
    const WCHAR *property;
    IExpressionAnimation **binding = NULL, *expression, *copy;
    HRESULT hr;
    struct Vector3 vector;
    boolean visible;
    FLOAT value;

    TRACE( "iface %p, name %s, animation %p.\n", iface, debugstr_hstring( name ), animation );
    if (!name || !animation) return E_INVALIDARG;
    property = WindowsGetStringRawBuffer( name, NULL );
    if (!wcscmp( property, L"Scale" )) binding = &impl->scale_expression;
    else if (!wcscmp( property, L"Offset" )) binding = &impl->offset_expression;
    else if (!wcscmp( property, L"CenterPoint" )) binding = &impl->center_expression;
    if (binding)
    {
        if (*binding) IExpressionAnimation_Release( *binding );
        *binding = NULL;
        if (SUCCEEDED(ICompositionAnimation_QueryInterface( animation,
                &IID_IExpressionAnimation, (void **)&expression )))
        {
            hr = expression_animation_clone( expression, &copy );
            IExpressionAnimation_Release( expression );
            if (FAILED(hr)) return hr;
            *binding = copy;
            return S_OK;
        }
    }

    if (!wcscmp( property, L"Opacity" ) &&
            SUCCEEDED(scalar_key_frame_animation_get_final_value( animation, &value )))
        impl->opacity = value;
    else if (!wcscmp( property, L"IsVisible" ) &&
            SUCCEEDED(boolean_key_frame_animation_get_final_value( animation, &visible )))
        impl->visible = visible;
    else if (SUCCEEDED(vector3_key_frame_animation_get_final_value( animation, &vector )))
    {
        if (!wcscmp( property, L"Offset" )) impl->offset = vector;
        else if (!wcscmp( property, L"Scale" )) impl->scale = vector;
        else if (!wcscmp( property, L"CenterPoint" )) impl->center_point = vector;
        else if (!wcscmp( property, L"RotationAxis" )) impl->rotation_axis = vector;
    }
    return S_OK;
}

static HRESULT WINAPI sprite_visual_composition_object_StopAnimation( ICompositionObject *iface,
        HSTRING name )
{
    struct sprite_visual *impl = sprite_visual_from_ICompositionObject( iface );
    IExpressionAnimation **binding = NULL;
    const WCHAR *property;

    TRACE( "iface %p, name %s.\n", iface, debugstr_hstring( name ) );
    if (!name) return E_INVALIDARG;
    property = WindowsGetStringRawBuffer( name, NULL );
    if (!wcscmp( property, L"Scale" )) binding = &impl->scale_expression;
    else if (!wcscmp( property, L"Offset" )) binding = &impl->offset_expression;
    else if (!wcscmp( property, L"CenterPoint" )) binding = &impl->center_expression;
    if (binding && *binding)
    {
        IExpressionAnimation_Release( *binding );
        *binding = NULL;
    }
    return S_OK;
}

static const ICompositionObjectVtbl sprite_visual_composition_object_vtbl =
{
    sprite_visual_composition_object_QueryInterface,
    sprite_visual_composition_object_AddRef,
    sprite_visual_composition_object_Release,
    sprite_visual_composition_object_GetIids,
    sprite_visual_composition_object_GetRuntimeClassName,
    sprite_visual_composition_object_GetTrustLevel,
    sprite_visual_composition_object_get_Compositor,
    sprite_visual_composition_object_get_Dispatcher,
    sprite_visual_composition_object_get_Properties,
    sprite_visual_composition_object_StartAnimation,
    sprite_visual_composition_object_StopAnimation,
};

static HRESULT WINAPI sprite_visual_get_Brush( ISpriteVisual *iface, ICompositionBrush **value )
{
    struct sprite_visual *impl = impl_from_ISpriteVisual( iface );

    if (!value) return E_POINTER;
    if (impl->brush) ICompositionBrush_AddRef( impl->brush );
    *value = impl->brush;
    return S_OK;
}

static HRESULT WINAPI sprite_visual_put_Brush( ISpriteVisual *iface, ICompositionBrush *value )
{
    struct sprite_visual *impl = impl_from_ISpriteVisual( iface );
    ICompositionSurfaceBrush *surface_brush;
    struct nine_grid_brush_iface *nine_grid_brush;

    if (value) ICompositionBrush_AddRef( value );
    if (impl->brush)
    {
        if (SUCCEEDED(ICompositionBrush_QueryInterface( impl->brush,
                &IID_ICompositionSurfaceBrush, (void **)&surface_brush )))
        {
            surface_brush_bind( surface_brush, NULL );
            ICompositionSurfaceBrush_Release( surface_brush );
        }
        if (SUCCEEDED(ICompositionBrush_QueryInterface( impl->brush,
                &composition_nine_grid_brush_iid, (void **)&nine_grid_brush )))
        {
            nine_grid_brush_bind( nine_grid_brush, NULL );
            IUnknown_Release( (IUnknown *)nine_grid_brush );
        }
        ICompositionBrush_Release( impl->brush );
    }
    impl->brush = value;
    if (impl->dcomp_visual)
    {
        if (value && SUCCEEDED(ICompositionBrush_QueryInterface( value,
                &IID_ICompositionSurfaceBrush, (void **)&surface_brush )))
        {
            surface_brush_bind( surface_brush, impl->dcomp_visual );
            ICompositionSurfaceBrush_Release( surface_brush );
        }
        else if (value && SUCCEEDED(ICompositionBrush_QueryInterface( value,
                &composition_nine_grid_brush_iid, (void **)&nine_grid_brush )))
        {
            nine_grid_brush_bind( nine_grid_brush, impl->dcomp_visual );
            IUnknown_Release( (IUnknown *)nine_grid_brush );
        }
        else IDCompositionVisual_SetContent( impl->dcomp_visual, NULL );
    }
    return S_OK;
}

static const ISpriteVisualVtbl sprite_visual_vtbl =
{
    sprite_visual_QueryInterface,
    sprite_visual_AddRef,
    sprite_visual_Release,
    sprite_visual_GetIids,
    sprite_visual_GetRuntimeClassName,
    sprite_visual_GetTrustLevel,
    sprite_visual_get_Brush,
    sprite_visual_put_Brush,
};

static HRESULT WINAPI visual_QueryInterface( IVisual *iface, REFIID iid, void **out )
{
    struct sprite_visual *impl = sprite_visual_from_IVisual( iface );
    return ISpriteVisual_QueryInterface( &impl->ISpriteVisual_iface, iid, out );
}

static ULONG WINAPI visual_AddRef( IVisual *iface )
{
    struct sprite_visual *impl = sprite_visual_from_IVisual( iface );
    return ISpriteVisual_AddRef( &impl->ISpriteVisual_iface );
}

static ULONG WINAPI visual_Release( IVisual *iface )
{
    struct sprite_visual *impl = sprite_visual_from_IVisual( iface );
    return ISpriteVisual_Release( &impl->ISpriteVisual_iface );
}

static HRESULT WINAPI visual_GetIids( IVisual *iface, ULONG *count, IID **iids )
{
    struct sprite_visual *impl = sprite_visual_from_IVisual( iface );
    return ISpriteVisual_GetIids( &impl->ISpriteVisual_iface, count, iids );
}

static HRESULT WINAPI visual_GetRuntimeClassName( IVisual *iface, HSTRING *name )
{
    struct sprite_visual *impl = sprite_visual_from_IVisual( iface );
    return ISpriteVisual_GetRuntimeClassName( &impl->ISpriteVisual_iface, name );
}

static HRESULT WINAPI visual_GetTrustLevel( IVisual *iface, TrustLevel *level )
{
    struct sprite_visual *impl = sprite_visual_from_IVisual( iface );
    return ISpriteVisual_GetTrustLevel( &impl->ISpriteVisual_iface, level );
}

static BOOL visual_property_equal(const WCHAR *name,const void *a,const void *b,SIZE_T size)
{
    FLOAT previous[16], current[16];
    UINT i;

    if (size % sizeof(FLOAT) || size > sizeof(previous) || !wcscmp(name,L"BackfaceVisibility") ||
            !wcscmp(name,L"BorderMode") || !wcscmp(name,L"CompositeMode"))
        return !memcmp(a,b,size);
    memcpy(previous,a,size);
    memcpy(current,b,size);
    for (i = 0; i < size / sizeof(FLOAT); ++i)
        if (previous[i] != current[i]) return FALSE;
    return TRUE;
}

#define VISUAL_PROPERTY(name, type, field) \
    static HRESULT WINAPI visual_get_##name( IVisual *iface, type *value ) \
    { \
        if (!value) return E_POINTER; \
        *value = sprite_visual_from_IVisual( iface )->field; \
        return S_OK; \
    } \
    static HRESULT WINAPI visual_put_##name( IVisual *iface, type value ) \
    { \
        struct sprite_visual *impl = sprite_visual_from_IVisual(iface); \
        if (visual_property_equal(L## #name,&impl->field,&value,sizeof(value))) return S_OK; \
        impl->field = value; \
        return visual_apply_implicit_animation(impl,L## #name); \
    }

VISUAL_PROPERTY( AnchorPoint, struct Vector2, anchor_point )
VISUAL_PROPERTY( BackfaceVisibility, enum CompositionBackfaceVisibility, backface_visibility )
VISUAL_PROPERTY( BorderMode, enum CompositionBorderMode, border_mode )
VISUAL_PROPERTY( CenterPoint, struct Vector3, center_point )
VISUAL_PROPERTY( CompositeMode, enum CompositionCompositeMode, composite_mode )
VISUAL_PROPERTY( IsVisible, boolean, visible )
VISUAL_PROPERTY( Offset, struct Vector3, offset )
VISUAL_PROPERTY( Opacity, FLOAT, opacity )
VISUAL_PROPERTY( Orientation, struct Quaternion, orientation )
VISUAL_PROPERTY( RotationAngle, FLOAT, rotation_angle )
VISUAL_PROPERTY( RotationAngleInDegrees, FLOAT, rotation_angle_degrees )
VISUAL_PROPERTY( RotationAxis, struct Vector3, rotation_axis )
VISUAL_PROPERTY( Scale, struct Vector3, scale )
VISUAL_PROPERTY( Size, struct Vector2, size )
VISUAL_PROPERTY( TransformMatrix, struct Matrix4x4, transform_matrix )

static HRESULT WINAPI visual_get_Clip( IVisual *iface, ICompositionClip **value )
{
    struct sprite_visual *impl = sprite_visual_from_IVisual( iface );

    if (!value) return E_POINTER;
    if (impl->clip) ICompositionClip_AddRef( impl->clip );
    *value = impl->clip;
    return S_OK;
}

static HRESULT WINAPI visual_put_Clip( IVisual *iface, ICompositionClip *value )
{
    struct sprite_visual *impl = sprite_visual_from_IVisual( iface );

    if (value) ICompositionClip_AddRef( value );
    if (impl->clip) ICompositionClip_Release( impl->clip );
    impl->clip = value;
    return S_OK;
}

static HRESULT WINAPI visual_get_Parent( IVisual *iface, IContainerVisual **value )
{
    if (!value) return E_POINTER;
    *value = NULL;
    return S_OK;
}

static const IVisualVtbl visual_vtbl =
{
    visual_QueryInterface,
    visual_AddRef,
    visual_Release,
    visual_GetIids,
    visual_GetRuntimeClassName,
    visual_GetTrustLevel,
    visual_get_AnchorPoint,
    visual_put_AnchorPoint,
    visual_get_BackfaceVisibility,
    visual_put_BackfaceVisibility,
    visual_get_BorderMode,
    visual_put_BorderMode,
    visual_get_CenterPoint,
    visual_put_CenterPoint,
    visual_get_Clip,
    visual_put_Clip,
    visual_get_CompositeMode,
    visual_put_CompositeMode,
    visual_get_IsVisible,
    visual_put_IsVisible,
    visual_get_Offset,
    visual_put_Offset,
    visual_get_Opacity,
    visual_put_Opacity,
    visual_get_Orientation,
    visual_put_Orientation,
    visual_get_Parent,
    visual_get_RotationAngle,
    visual_put_RotationAngle,
    visual_get_RotationAngleInDegrees,
    visual_put_RotationAngleInDegrees,
    visual_get_RotationAxis,
    visual_put_RotationAxis,
    visual_get_Scale,
    visual_put_Scale,
    visual_get_Size,
    visual_put_Size,
    visual_get_TransformMatrix,
    visual_put_TransformMatrix,
};

static HRESULT WINAPI visual2_QueryInterface( IVisual2 *iface, REFIID iid, void **out )
{
    struct sprite_visual *impl = sprite_visual_from_IVisual2( iface );
    return ISpriteVisual_QueryInterface( &impl->ISpriteVisual_iface, iid, out );
}

static ULONG WINAPI visual2_AddRef( IVisual2 *iface )
{
    struct sprite_visual *impl = sprite_visual_from_IVisual2( iface );
    return ISpriteVisual_AddRef( &impl->ISpriteVisual_iface );
}

static ULONG WINAPI visual2_Release( IVisual2 *iface )
{
    struct sprite_visual *impl = sprite_visual_from_IVisual2( iface );
    return ISpriteVisual_Release( &impl->ISpriteVisual_iface );
}

static HRESULT WINAPI visual2_GetIids( IVisual2 *iface, ULONG *count, IID **iids )
{
    struct sprite_visual *impl = sprite_visual_from_IVisual2( iface );
    return ISpriteVisual_GetIids( &impl->ISpriteVisual_iface, count, iids );
}

static HRESULT WINAPI visual2_GetRuntimeClassName( IVisual2 *iface, HSTRING *name )
{
    struct sprite_visual *impl = sprite_visual_from_IVisual2( iface );
    return ISpriteVisual_GetRuntimeClassName( &impl->ISpriteVisual_iface, name );
}

static HRESULT WINAPI visual2_GetTrustLevel( IVisual2 *iface, TrustLevel *level )
{
    struct sprite_visual *impl = sprite_visual_from_IVisual2( iface );
    return ISpriteVisual_GetTrustLevel( &impl->ISpriteVisual_iface, level );
}

static HRESULT WINAPI visual2_get_ParentForTransform( IVisual2 *iface, IVisual **value )
{
    struct sprite_visual *impl = sprite_visual_from_IVisual2( iface );

    if (!value) return E_POINTER;
    if (impl->parent_for_transform) IVisual_AddRef( impl->parent_for_transform );
    *value = impl->parent_for_transform;
    return S_OK;
}

static HRESULT WINAPI visual2_put_ParentForTransform( IVisual2 *iface, IVisual *value )
{
    struct sprite_visual *impl = sprite_visual_from_IVisual2( iface );

    if (value) IVisual_AddRef( value );
    if (impl->parent_for_transform) IVisual_Release( impl->parent_for_transform );
    impl->parent_for_transform = value;
    return S_OK;
}

#define VISUAL2_PROPERTY(name, type, field) \
    static HRESULT WINAPI visual2_get_##name( IVisual2 *iface, type *value ) \
    { \
        if (!value) return E_POINTER; \
        *value = sprite_visual_from_IVisual2( iface )->field; \
        return S_OK; \
    } \
    static HRESULT WINAPI visual2_put_##name( IVisual2 *iface, type value ) \
    { \
        sprite_visual_from_IVisual2( iface )->field = value; \
        return S_OK; \
    }

VISUAL2_PROPERTY( RelativeOffsetAdjustment, struct Vector3, relative_offset_adjustment )
VISUAL2_PROPERTY( RelativeSizeAdjustment, struct Vector2, relative_size_adjustment )

static const IVisual2Vtbl visual2_vtbl =
{
    visual2_QueryInterface,
    visual2_AddRef,
    visual2_Release,
    visual2_GetIids,
    visual2_GetRuntimeClassName,
    visual2_GetTrustLevel,
    visual2_get_ParentForTransform,
    visual2_put_ParentForTransform,
    visual2_get_RelativeOffsetAdjustment,
    visual2_put_RelativeOffsetAdjustment,
    visual2_get_RelativeSizeAdjustment,
    visual2_put_RelativeSizeAdjustment,
};

static HRESULT WINAPI container_visual_QueryInterface( IContainerVisual *iface, REFIID iid, void **out )
{
    struct sprite_visual *impl = sprite_visual_from_IContainerVisual( iface );
    return ISpriteVisual_QueryInterface( &impl->ISpriteVisual_iface, iid, out );
}

static ULONG WINAPI container_visual_AddRef( IContainerVisual *iface )
{
    struct sprite_visual *impl = sprite_visual_from_IContainerVisual( iface );
    return ISpriteVisual_AddRef( &impl->ISpriteVisual_iface );
}

static ULONG WINAPI container_visual_Release( IContainerVisual *iface )
{
    struct sprite_visual *impl = sprite_visual_from_IContainerVisual( iface );
    return ISpriteVisual_Release( &impl->ISpriteVisual_iface );
}

static HRESULT WINAPI container_visual_GetIids( IContainerVisual *iface, ULONG *count, IID **iids )
{
    struct sprite_visual *impl = sprite_visual_from_IContainerVisual( iface );
    return ISpriteVisual_GetIids( &impl->ISpriteVisual_iface, count, iids );
}

static HRESULT WINAPI container_visual_GetRuntimeClassName( IContainerVisual *iface, HSTRING *name )
{
    struct sprite_visual *impl = sprite_visual_from_IContainerVisual( iface );
    return ISpriteVisual_GetRuntimeClassName( &impl->ISpriteVisual_iface, name );
}

static HRESULT WINAPI container_visual_GetTrustLevel( IContainerVisual *iface, TrustLevel *level )
{
    struct sprite_visual *impl = sprite_visual_from_IContainerVisual( iface );
    return ISpriteVisual_GetTrustLevel( &impl->ISpriteVisual_iface, level );
}

static HRESULT WINAPI container_visual_get_Children( IContainerVisual *iface, IVisualCollection **value )
{
    struct sprite_visual *impl = sprite_visual_from_IContainerVisual( iface );

    if (!value) return E_POINTER;
    IVisualCollection_AddRef( impl->children );
    *value = impl->children;
    return S_OK;
}

static const IContainerVisualVtbl container_visual_vtbl =
{
    container_visual_QueryInterface,
    container_visual_AddRef,
    container_visual_Release,
    container_visual_GetIids,
    container_visual_GetRuntimeClassName,
    container_visual_GetTrustLevel,
    container_visual_get_Children,
};

static inline struct compositor *impl_from_ICompositor( ICompositor *iface )
{
    return CONTAINING_RECORD( iface, struct compositor, ICompositor_iface );
}

static inline struct compositor *impl_from_compositor2( struct compositor2_iface *iface )
{
    return CONTAINING_RECORD( iface, struct compositor, ICompositor2_iface );
}

static inline struct compositor *impl_from_compositor4( struct compositor4_iface *iface )
{
    return CONTAINING_RECORD( iface, struct compositor, ICompositor4_iface );
}

static inline struct compositor *impl_from_compositor5( struct compositor5_iface *iface )
{
    return CONTAINING_RECORD( iface, struct compositor, ICompositor5_iface );
}

static inline struct compositor *impl_from_compositor6( struct compositor6_iface *iface )
{
    return CONTAINING_RECORD( iface, struct compositor, ICompositor6_iface );
}

static inline struct compositor *compositor_from_IClosable( IClosable *iface )
{
    return CONTAINING_RECORD( iface, struct compositor, IClosable_iface );
}

static inline struct compositor *compositor_from_ICompositorDesktopInterop(
        ICompositorDesktopInterop *iface )
{
    return CONTAINING_RECORD( iface, struct compositor, ICompositorDesktopInterop_iface );
}

static inline struct compositor *compositor_from_ICompositorInterop( ICompositorInterop *iface )
{
    return CONTAINING_RECORD( iface, struct compositor, ICompositorInterop_iface );
}

static HRESULT WINAPI compositor_QueryInterface( ICompositor *iface, REFIID iid, void **out )
{
    struct compositor *impl = impl_from_ICompositor( iface );

    FIXME( "iface %p, iid %s, out %p.\n", iface, debugstr_guid( iid ), out );
    if (!out) return E_POINTER;
    if (IsEqualGUID( iid, &IID_IUnknown ) || IsEqualGUID( iid, &IID_IInspectable ) ||
        IsEqualGUID( iid, &IID_IAgileObject ) || IsEqualGUID( iid, &IID_ICompositor ))
        *out = &impl->ICompositor_iface;
    else if (IsEqualGUID( iid, &compositor2_iid ))
        *out = &impl->ICompositor2_iface;
    else if (IsEqualGUID( iid, &compositor4_iid ))
        *out = &impl->ICompositor4_iface;
    else if (IsEqualGUID( iid, &compositor5_iid ))
        *out = &impl->ICompositor5_iface;
    else if (IsEqualGUID( iid, &compositor6_iid ))
        *out = &impl->ICompositor6_iface;
    else if (IsEqualGUID( iid, &IID_IClosable ))
        *out = &impl->IClosable_iface;
    else if (IsEqualGUID( iid, &IID_ICompositorDesktopInterop ))
        *out = &impl->ICompositorDesktopInterop_iface;
    else if (IsEqualGUID( iid, &IID_ICompositorInterop ))
        *out = &impl->ICompositorInterop_iface;
    else
    {
        *out = NULL;
        return E_NOINTERFACE;
    }
    IUnknown_AddRef( (IUnknown *)*out );
    return S_OK;
}

static ULONG WINAPI compositor_AddRef( ICompositor *iface )
{
    return InterlockedIncrement( &impl_from_ICompositor( iface )->ref );
}

static ULONG WINAPI compositor_Release( ICompositor *iface )
{
    struct compositor *impl = impl_from_ICompositor( iface );
    ULONG ref = InterlockedDecrement( &impl->ref );

    if (!ref)
    {
        struct desktop_window_target *target, *next;

        LIST_FOR_EACH_ENTRY_SAFE( target, next, &impl->targets, struct desktop_window_target, entry )
        {
            list_remove( &target->entry );
            target->linked = FALSE;
            desktop_window_target_release( target );
        }
        if (impl->dcomp_device) IDCompositionDevice_Release( impl->dcomp_device );
        WindowsDeleteString( impl->comment );
        free( impl );
    }
    return ref;
}

static HRESULT WINAPI compositor_GetIids( ICompositor *iface, ULONG *count, IID **iids )
{
    IID *values;

    if (!count || !iids) return E_POINTER;
    if (!(values = CoTaskMemAlloc( 6 * sizeof(*values) ))) return E_OUTOFMEMORY;
    values[0] = IID_ICompositor;
    values[1] = compositor2_iid;
    values[2] = compositor4_iid;
    values[3] = compositor5_iid;
    values[4] = compositor6_iid;
    values[5] = IID_IClosable;
    *count = 6;
    *iids = values;
    return S_OK;
}

static HRESULT WINAPI compositor_GetRuntimeClassName( ICompositor *iface, HSTRING *name )
{
    if (!name) return E_POINTER;
    return WindowsCreateString( RuntimeClass_Windows_UI_Composition_Compositor,
                                ARRAY_SIZE(RuntimeClass_Windows_UI_Composition_Compositor) - 1, name );
}

static HRESULT WINAPI compositor_GetTrustLevel( ICompositor *iface, TrustLevel *level )
{
    if (!level) return E_POINTER;
    *level = BaseTrust;
    return S_OK;
}

struct color_brush
{
    ICompositionColorBrush ICompositionColorBrush_iface;
    ICompositionBrush ICompositionBrush_iface;
    ICompositionObject ICompositionObject_iface;
    IClosable IClosable_iface;
    ICompositor *compositor;
    struct Color color;
    LONG ref;
};

static inline struct color_brush *impl_from_ICompositionColorBrush( ICompositionColorBrush *iface )
{
    return CONTAINING_RECORD( iface, struct color_brush, ICompositionColorBrush_iface );
}

static inline struct color_brush *color_brush_from_ICompositionBrush( ICompositionBrush *iface )
{
    return CONTAINING_RECORD( iface, struct color_brush, ICompositionBrush_iface );
}

static inline struct color_brush *color_brush_from_ICompositionObject( ICompositionObject *iface )
{
    return CONTAINING_RECORD( iface, struct color_brush, ICompositionObject_iface );
}

static inline struct color_brush *color_brush_from_IClosable( IClosable *iface )
{
    return CONTAINING_RECORD( iface, struct color_brush, IClosable_iface );
}

static HRESULT color_brush_query_interface( struct color_brush *impl, REFIID iid, void **out )
{
    FIXME( "impl %p, iid %s, out %p.\n", impl, debugstr_guid( iid ), out );
    if (!out) return E_POINTER;
    if (IsEqualGUID( iid, &IID_IUnknown ) || IsEqualGUID( iid, &IID_IInspectable ) ||
        IsEqualGUID( iid, &IID_IAgileObject ) || IsEqualGUID( iid, &IID_ICompositionColorBrush ))
        *out = &impl->ICompositionColorBrush_iface;
    else if (IsEqualGUID( iid, &IID_ICompositionBrush ))
        *out = &impl->ICompositionBrush_iface;
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

static ULONG color_brush_add_ref( struct color_brush *impl )
{
    return InterlockedIncrement( &impl->ref );
}

static ULONG color_brush_release( struct color_brush *impl )
{
    ULONG ref = InterlockedDecrement( &impl->ref );

    if (!ref)
    {
        ICompositor_Release( impl->compositor );
        free( impl );
    }
    return ref;
}

static HRESULT color_brush_get_iids( ULONG *count, IID **iids )
{
    IID *values;

    if (!count || !iids) return E_POINTER;
    if (!(values = CoTaskMemAlloc( 4 * sizeof(*values) ))) return E_OUTOFMEMORY;
    values[0] = IID_ICompositionColorBrush;
    values[1] = IID_ICompositionBrush;
    values[2] = IID_ICompositionObject;
    values[3] = IID_IClosable;
    *count = 4;
    *iids = values;
    return S_OK;
}

static HRESULT color_brush_get_class_name( HSTRING *name )
{
    if (!name) return E_POINTER;
    return WindowsCreateString( RuntimeClass_Windows_UI_Composition_CompositionColorBrush,
                                ARRAY_SIZE(RuntimeClass_Windows_UI_Composition_CompositionColorBrush) - 1,
                                name );
}

static HRESULT color_brush_get_trust_level( TrustLevel *level )
{
    if (!level) return E_POINTER;
    *level = BaseTrust;
    return S_OK;
}

#define COLOR_BRUSH_FORWARD(pfx, iface_type, from) \
    static HRESULT WINAPI pfx##_QueryInterface( iface_type *iface, REFIID iid, void **out ) \
    { \
        return color_brush_query_interface( from( iface ), iid, out ); \
    } \
    static ULONG WINAPI pfx##_AddRef( iface_type *iface ) \
    { \
        return color_brush_add_ref( from( iface ) ); \
    } \
    static ULONG WINAPI pfx##_Release( iface_type *iface ) \
    { \
        return color_brush_release( from( iface ) ); \
    } \
    static HRESULT WINAPI pfx##_GetIids( iface_type *iface, ULONG *count, IID **iids ) \
    { \
        return color_brush_get_iids( count, iids ); \
    } \
    static HRESULT WINAPI pfx##_GetRuntimeClassName( iface_type *iface, HSTRING *name ) \
    { \
        return color_brush_get_class_name( name ); \
    } \
    static HRESULT WINAPI pfx##_GetTrustLevel( iface_type *iface, TrustLevel *level ) \
    { \
        return color_brush_get_trust_level( level ); \
    }

COLOR_BRUSH_FORWARD( color_brush, ICompositionColorBrush, impl_from_ICompositionColorBrush )
COLOR_BRUSH_FORWARD( composition_brush, ICompositionBrush, color_brush_from_ICompositionBrush )
COLOR_BRUSH_FORWARD( brush_composition_object, ICompositionObject, color_brush_from_ICompositionObject )
COLOR_BRUSH_FORWARD( brush_closable, IClosable, color_brush_from_IClosable )

static HRESULT WINAPI color_brush_get_Color( ICompositionColorBrush *iface, struct Color *value )
{
    if (!value) return E_POINTER;
    *value = impl_from_ICompositionColorBrush( iface )->color;
    return S_OK;
}

static HRESULT WINAPI color_brush_put_Color( ICompositionColorBrush *iface, struct Color value )
{
    impl_from_ICompositionColorBrush( iface )->color = value;
    return S_OK;
}

static const ICompositionColorBrushVtbl color_brush_vtbl =
{
    color_brush_QueryInterface,
    color_brush_AddRef,
    color_brush_Release,
    color_brush_GetIids,
    color_brush_GetRuntimeClassName,
    color_brush_GetTrustLevel,
    color_brush_get_Color,
    color_brush_put_Color,
};

static const ICompositionBrushVtbl composition_brush_vtbl =
{
    composition_brush_QueryInterface,
    composition_brush_AddRef,
    composition_brush_Release,
    composition_brush_GetIids,
    composition_brush_GetRuntimeClassName,
    composition_brush_GetTrustLevel,
};

static HRESULT WINAPI brush_composition_object_get_Compositor( ICompositionObject *iface,
        ICompositor **value )
{
    struct color_brush *impl = color_brush_from_ICompositionObject( iface );

    if (!value) return E_POINTER;
    ICompositor_AddRef( impl->compositor );
    *value = impl->compositor;
    return S_OK;
}

static HRESULT WINAPI brush_composition_object_get_Dispatcher( ICompositionObject *iface,
        ICoreDispatcher **value )
{
    if (!value) return E_POINTER;
    *value = NULL;
    return S_OK;
}

static HRESULT WINAPI brush_composition_object_get_Properties( ICompositionObject *iface,
        ICompositionPropertySet **value )
{
    if (!value) return E_POINTER;
    *value = NULL;
    return S_OK;
}

static HRESULT WINAPI brush_composition_object_StartAnimation( ICompositionObject *iface,
        HSTRING name, ICompositionAnimation *animation )
{
    TRACE( "iface %p, name %s, animation %p.\n", iface, debugstr_hstring( name ), animation );
    return S_OK;
}

static HRESULT WINAPI brush_composition_object_StopAnimation( ICompositionObject *iface, HSTRING name )
{
    TRACE( "iface %p, name %s.\n", iface, debugstr_hstring( name ) );
    return S_OK;
}

static const ICompositionObjectVtbl brush_composition_object_vtbl =
{
    brush_composition_object_QueryInterface,
    brush_composition_object_AddRef,
    brush_composition_object_Release,
    brush_composition_object_GetIids,
    brush_composition_object_GetRuntimeClassName,
    brush_composition_object_GetTrustLevel,
    brush_composition_object_get_Compositor,
    brush_composition_object_get_Dispatcher,
    brush_composition_object_get_Properties,
    brush_composition_object_StartAnimation,
    brush_composition_object_StopAnimation,
};

static HRESULT WINAPI brush_closable_Close( IClosable *iface )
{
    TRACE( "iface %p.\n", iface );
    return S_OK;
}

static const IClosableVtbl brush_closable_vtbl =
{
    brush_closable_QueryInterface,
    brush_closable_AddRef,
    brush_closable_Release,
    brush_closable_GetIids,
    brush_closable_GetRuntimeClassName,
    brush_closable_GetTrustLevel,
    brush_closable_Close,
};

static HRESULT color_brush_create( ICompositor *compositor, struct Color color,
        ICompositionColorBrush **result )
{
    struct color_brush *impl;

    if (!result) return E_POINTER;
    *result = NULL;
    if (!(impl = calloc( 1, sizeof(*impl) ))) return E_OUTOFMEMORY;
    impl->ICompositionColorBrush_iface.lpVtbl = &color_brush_vtbl;
    impl->ICompositionBrush_iface.lpVtbl = &composition_brush_vtbl;
    impl->ICompositionObject_iface.lpVtbl = &brush_composition_object_vtbl;
    impl->IClosable_iface.lpVtbl = &brush_closable_vtbl;
    impl->compositor = compositor;
    impl->color = color;
    impl->ref = 1;
    ICompositor_AddRef( compositor );
    *result = &impl->ICompositionColorBrush_iface;
    return S_OK;
}

struct surface_brush
{
    ICompositionSurfaceBrush ICompositionSurfaceBrush_iface;
    ICompositionSurfaceBrush3 ICompositionSurfaceBrush3_iface;
    ICompositionBrush ICompositionBrush_iface;
    IDCompositionVisual *dcomp_visual;
    ICompositionSurface *surface;
    enum CompositionBitmapInterpolationMode interpolation_mode;
    enum CompositionStretch stretch;
    FLOAT horizontal_alignment_ratio;
    FLOAT vertical_alignment_ratio;
    boolean snap_to_pixels;
    LONG ref;
};

static inline struct surface_brush *impl_from_ICompositionSurfaceBrush( ICompositionSurfaceBrush *iface )
{
    return CONTAINING_RECORD( iface, struct surface_brush, ICompositionSurfaceBrush_iface );
}

static inline struct surface_brush *surface_brush_from_ICompositionBrush( ICompositionBrush *iface )
{
    return CONTAINING_RECORD( iface, struct surface_brush, ICompositionBrush_iface );
}

static void surface_brush_bind( ICompositionSurfaceBrush *iface, IDCompositionVisual *visual )
{
    struct surface_brush *impl = impl_from_ICompositionSurfaceBrush( iface );

    impl->dcomp_visual = visual;
    if (visual) IDCompositionVisual_SetContent( visual, (IUnknown *)impl->surface );
    FIXME( "iface %p, visual %p, surface %p.\n", iface, visual, impl->surface );
}

static inline struct surface_brush *surface_brush_from_ICompositionSurfaceBrush3(
        ICompositionSurfaceBrush3 *iface )
{
    return CONTAINING_RECORD( iface, struct surface_brush, ICompositionSurfaceBrush3_iface );
}

static HRESULT surface_brush_query_interface( struct surface_brush *impl, REFIID iid, void **out )
{
    FIXME( "impl %p, iid %s, out %p.\n", impl, debugstr_guid( iid ), out );
    if (!out) return E_POINTER;
    if (IsEqualGUID( iid, &IID_IUnknown ) || IsEqualGUID( iid, &IID_IInspectable ) ||
        IsEqualGUID( iid, &IID_IAgileObject ) || IsEqualGUID( iid, &IID_ICompositionSurfaceBrush ))
        *out = &impl->ICompositionSurfaceBrush_iface;
    else if (IsEqualGUID( iid, &IID_ICompositionBrush ))
        *out = &impl->ICompositionBrush_iface;
    else if (IsEqualGUID( iid, &IID_ICompositionSurfaceBrush3 ))
        *out = &impl->ICompositionSurfaceBrush3_iface;
    else
    {
        *out = NULL;
        return E_NOINTERFACE;
    }
    IUnknown_AddRef( (IUnknown *)*out );
    return S_OK;
}

static ULONG surface_brush_add_ref( struct surface_brush *impl )
{
    return InterlockedIncrement( &impl->ref );
}

static ULONG surface_brush_release( struct surface_brush *impl )
{
    ULONG ref = InterlockedDecrement( &impl->ref );

    if (!ref)
    {
        if (impl->surface) ICompositionSurface_Release( impl->surface );
        free( impl );
    }
    return ref;
}

#define SURFACE_BRUSH_FORWARD(pfx, iface_type, from) \
    static HRESULT WINAPI pfx##_QueryInterface( iface_type *iface, REFIID iid, void **out ) \
    { \
        return surface_brush_query_interface( from( iface ), iid, out ); \
    } \
    static ULONG WINAPI pfx##_AddRef( iface_type *iface ) \
    { \
        return surface_brush_add_ref( from( iface ) ); \
    } \
    static ULONG WINAPI pfx##_Release( iface_type *iface ) \
    { \
        return surface_brush_release( from( iface ) ); \
    } \
    static HRESULT WINAPI pfx##_GetIids( iface_type *iface, ULONG *count, IID **iids ) \
    { \
        IID *values; \
        if (!count || !iids) return E_POINTER; \
        if (!(values = CoTaskMemAlloc( 3 * sizeof(*values) ))) return E_OUTOFMEMORY; \
        values[0] = IID_ICompositionSurfaceBrush; \
        values[1] = IID_ICompositionBrush; \
        values[2] = IID_ICompositionSurfaceBrush3; \
        *count = 3; \
        *iids = values; \
        return S_OK; \
    } \
    static HRESULT WINAPI pfx##_GetRuntimeClassName( iface_type *iface, HSTRING *name ) \
    { \
        if (!name) return E_POINTER; \
        return WindowsCreateString( RuntimeClass_Windows_UI_Composition_CompositionSurfaceBrush, \
                                    ARRAY_SIZE(RuntimeClass_Windows_UI_Composition_CompositionSurfaceBrush) - 1, \
                                    name ); \
    } \
    static HRESULT WINAPI pfx##_GetTrustLevel( iface_type *iface, TrustLevel *level ) \
    { \
        if (!level) return E_POINTER; \
        *level = BaseTrust; \
        return S_OK; \
    }

SURFACE_BRUSH_FORWARD( surface_brush, ICompositionSurfaceBrush, impl_from_ICompositionSurfaceBrush )
SURFACE_BRUSH_FORWARD( surface_composition_brush, ICompositionBrush,
                       surface_brush_from_ICompositionBrush )
SURFACE_BRUSH_FORWARD( surface_brush3, ICompositionSurfaceBrush3,
                       surface_brush_from_ICompositionSurfaceBrush3 )

#define SURFACE_BRUSH_PROPERTY(name, type, field) \
    static HRESULT WINAPI surface_brush_get_##name( ICompositionSurfaceBrush *iface, type *value ) \
    { \
        if (!value) return E_POINTER; \
        *value = impl_from_ICompositionSurfaceBrush( iface )->field; \
        return S_OK; \
    } \
    static HRESULT WINAPI surface_brush_put_##name( ICompositionSurfaceBrush *iface, type value ) \
    { \
        impl_from_ICompositionSurfaceBrush( iface )->field = value; \
        return S_OK; \
    }

SURFACE_BRUSH_PROPERTY( BitmapInterpolationMode, enum CompositionBitmapInterpolationMode,
                        interpolation_mode )
SURFACE_BRUSH_PROPERTY( HorizontalAlignmentRatio, FLOAT, horizontal_alignment_ratio )
SURFACE_BRUSH_PROPERTY( Stretch, enum CompositionStretch, stretch )
SURFACE_BRUSH_PROPERTY( VerticalAlignmentRatio, FLOAT, vertical_alignment_ratio )

static HRESULT WINAPI surface_brush_get_Surface( ICompositionSurfaceBrush *iface,
        ICompositionSurface **value )
{
    struct surface_brush *impl = impl_from_ICompositionSurfaceBrush( iface );

    if (!value) return E_POINTER;
    if (impl->surface) ICompositionSurface_AddRef( impl->surface );
    *value = impl->surface;
    return S_OK;
}

static HRESULT WINAPI surface_brush_put_Surface( ICompositionSurfaceBrush *iface,
        ICompositionSurface *value )
{
    struct surface_brush *impl = impl_from_ICompositionSurfaceBrush( iface );

    if (value) ICompositionSurface_AddRef( value );
    if (impl->surface) ICompositionSurface_Release( impl->surface );
    impl->surface = value;
    if (impl->dcomp_visual) IDCompositionVisual_SetContent( impl->dcomp_visual, (IUnknown *)value );
    FIXME( "iface %p, surface %p, visual %p.\n", iface, value, impl->dcomp_visual );
    return S_OK;
}

static const ICompositionSurfaceBrushVtbl surface_brush_vtbl =
{
    surface_brush_QueryInterface,
    surface_brush_AddRef,
    surface_brush_Release,
    surface_brush_GetIids,
    surface_brush_GetRuntimeClassName,
    surface_brush_GetTrustLevel,
    surface_brush_get_BitmapInterpolationMode,
    surface_brush_put_BitmapInterpolationMode,
    surface_brush_get_HorizontalAlignmentRatio,
    surface_brush_put_HorizontalAlignmentRatio,
    surface_brush_get_Stretch,
    surface_brush_put_Stretch,
    surface_brush_get_Surface,
    surface_brush_put_Surface,
    surface_brush_get_VerticalAlignmentRatio,
    surface_brush_put_VerticalAlignmentRatio,
};

static const ICompositionBrushVtbl surface_composition_brush_vtbl =
{
    surface_composition_brush_QueryInterface,
    surface_composition_brush_AddRef,
    surface_composition_brush_Release,
    surface_composition_brush_GetIids,
    surface_composition_brush_GetRuntimeClassName,
    surface_composition_brush_GetTrustLevel,
};

static HRESULT WINAPI surface_brush3_get_SnapToPixels( ICompositionSurfaceBrush3 *iface,
        boolean *value )
{
    if (!value) return E_POINTER;
    *value = surface_brush_from_ICompositionSurfaceBrush3( iface )->snap_to_pixels;
    return S_OK;
}

static HRESULT WINAPI surface_brush3_put_SnapToPixels( ICompositionSurfaceBrush3 *iface,
        boolean value )
{
    surface_brush_from_ICompositionSurfaceBrush3( iface )->snap_to_pixels = value;
    return S_OK;
}

static const ICompositionSurfaceBrush3Vtbl surface_brush3_vtbl =
{
    surface_brush3_QueryInterface,
    surface_brush3_AddRef,
    surface_brush3_Release,
    surface_brush3_GetIids,
    surface_brush3_GetRuntimeClassName,
    surface_brush3_GetTrustLevel,
    surface_brush3_get_SnapToPixels,
    surface_brush3_put_SnapToPixels,
};

static HRESULT surface_brush_create( ICompositionSurface *surface,
        ICompositionSurfaceBrush **result )
{
    struct surface_brush *impl;

    if (!result) return E_POINTER;
    *result = NULL;
    if (!(impl = calloc( 1, sizeof(*impl) ))) return E_OUTOFMEMORY;
    impl->ICompositionSurfaceBrush_iface.lpVtbl = &surface_brush_vtbl;
    impl->ICompositionSurfaceBrush3_iface.lpVtbl = &surface_brush3_vtbl;
    impl->ICompositionBrush_iface.lpVtbl = &surface_composition_brush_vtbl;
    impl->surface = surface;
    impl->horizontal_alignment_ratio = 0.5f;
    impl->vertical_alignment_ratio = 0.5f;
    impl->ref = 1;
    if (surface) ICompositionSurface_AddRef( surface );
    *result = &impl->ICompositionSurfaceBrush_iface;
    return S_OK;
}

struct nine_grid_brush
{
    struct nine_grid_brush_iface ICompositionNineGridBrush_iface;
    ICompositionBrush ICompositionBrush_iface;
    ICompositionObject ICompositionObject_iface;
    IClosable IClosable_iface;
    ICompositor *compositor;
    ICompositionBrush *source;
    IDCompositionVisual *dcomp_visual;
    FLOAT bottom_inset;
    FLOAT bottom_inset_scale;
    FLOAT left_inset;
    FLOAT left_inset_scale;
    FLOAT right_inset;
    FLOAT right_inset_scale;
    FLOAT top_inset;
    FLOAT top_inset_scale;
    boolean center_hollow;
    LONG ref;
};

static inline struct nine_grid_brush *impl_from_nine_grid_brush( struct nine_grid_brush_iface *iface )
{
    return CONTAINING_RECORD( iface, struct nine_grid_brush, ICompositionNineGridBrush_iface );
}

static inline struct nine_grid_brush *nine_grid_brush_from_ICompositionBrush( ICompositionBrush *iface )
{
    return CONTAINING_RECORD( iface, struct nine_grid_brush, ICompositionBrush_iface );
}

static inline struct nine_grid_brush *nine_grid_brush_from_ICompositionObject( ICompositionObject *iface )
{
    return CONTAINING_RECORD( iface, struct nine_grid_brush, ICompositionObject_iface );
}

static inline struct nine_grid_brush *nine_grid_brush_from_IClosable( IClosable *iface )
{
    return CONTAINING_RECORD( iface, struct nine_grid_brush, IClosable_iface );
}

static HRESULT nine_grid_brush_query_interface( struct nine_grid_brush *impl, REFIID iid, void **out )
{
    FIXME( "impl %p, iid %s, out %p.\n", impl, debugstr_guid( iid ), out );
    if (!out) return E_POINTER;
    if (IsEqualGUID( iid, &IID_IUnknown ) || IsEqualGUID( iid, &IID_IInspectable ) ||
        IsEqualGUID( iid, &IID_IAgileObject ) || IsEqualGUID( iid, &composition_nine_grid_brush_iid ))
        *out = &impl->ICompositionNineGridBrush_iface;
    else if (IsEqualGUID( iid, &IID_ICompositionBrush ))
        *out = &impl->ICompositionBrush_iface;
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

static ULONG nine_grid_brush_add_ref( struct nine_grid_brush *impl )
{
    return InterlockedIncrement( &impl->ref );
}

static ULONG nine_grid_brush_release( struct nine_grid_brush *impl )
{
    ULONG ref = InterlockedDecrement( &impl->ref );

    if (!ref)
    {
        if (impl->source) ICompositionBrush_Release( impl->source );
        ICompositor_Release( impl->compositor );
        free( impl );
    }
    return ref;
}

static HRESULT nine_grid_brush_get_iids( ULONG *count, IID **iids )
{
    IID *values;

    if (!count || !iids) return E_POINTER;
    if (!(values = CoTaskMemAlloc( 4 * sizeof(*values) ))) return E_OUTOFMEMORY;
    values[0] = composition_nine_grid_brush_iid;
    values[1] = IID_ICompositionBrush;
    values[2] = IID_ICompositionObject;
    values[3] = IID_IClosable;
    *count = 4;
    *iids = values;
    return S_OK;
}

static HRESULT nine_grid_brush_get_class_name( HSTRING *name )
{
    static const WCHAR class_name[] = L"Windows.UI.Composition.CompositionNineGridBrush";

    if (!name) return E_POINTER;
    return WindowsCreateString( class_name, ARRAY_SIZE(class_name) - 1, name );
}

static HRESULT nine_grid_brush_get_trust_level( TrustLevel *level )
{
    if (!level) return E_POINTER;
    *level = BaseTrust;
    return S_OK;
}

#define NINE_GRID_BRUSH_FORWARD(pfx, iface_type, from) \
    static HRESULT WINAPI pfx##_QueryInterface( iface_type *iface, REFIID iid, void **out ) \
    { \
        return nine_grid_brush_query_interface( from( iface ), iid, out ); \
    } \
    static ULONG WINAPI pfx##_AddRef( iface_type *iface ) \
    { \
        return nine_grid_brush_add_ref( from( iface ) ); \
    } \
    static ULONG WINAPI pfx##_Release( iface_type *iface ) \
    { \
        return nine_grid_brush_release( from( iface ) ); \
    } \
    static HRESULT WINAPI pfx##_GetIids( iface_type *iface, ULONG *count, IID **iids ) \
    { \
        return nine_grid_brush_get_iids( count, iids ); \
    } \
    static HRESULT WINAPI pfx##_GetRuntimeClassName( iface_type *iface, HSTRING *name ) \
    { \
        return nine_grid_brush_get_class_name( name ); \
    } \
    static HRESULT WINAPI pfx##_GetTrustLevel( iface_type *iface, TrustLevel *level ) \
    { \
        return nine_grid_brush_get_trust_level( level ); \
    }

NINE_GRID_BRUSH_FORWARD( nine_grid_brush, struct nine_grid_brush_iface, impl_from_nine_grid_brush )
NINE_GRID_BRUSH_FORWARD( nine_grid_composition_brush, ICompositionBrush,
                         nine_grid_brush_from_ICompositionBrush )
NINE_GRID_BRUSH_FORWARD( nine_grid_composition_object, ICompositionObject,
                         nine_grid_brush_from_ICompositionObject )
NINE_GRID_BRUSH_FORWARD( nine_grid_closable, IClosable, nine_grid_brush_from_IClosable )

#undef NINE_GRID_BRUSH_FORWARD

#define NINE_GRID_BRUSH_FLOAT_PROPERTY(name, field) \
    static HRESULT WINAPI nine_grid_brush_get_##name( struct nine_grid_brush_iface *iface, FLOAT *value ) \
    { \
        if (!value) return E_POINTER; \
        *value = impl_from_nine_grid_brush( iface )->field; \
        return S_OK; \
    } \
    static HRESULT WINAPI nine_grid_brush_put_##name( struct nine_grid_brush_iface *iface, FLOAT value ) \
    { \
        impl_from_nine_grid_brush( iface )->field = value; \
        return S_OK; \
    }

NINE_GRID_BRUSH_FLOAT_PROPERTY( BottomInset, bottom_inset )
NINE_GRID_BRUSH_FLOAT_PROPERTY( BottomInsetScale, bottom_inset_scale )
NINE_GRID_BRUSH_FLOAT_PROPERTY( LeftInset, left_inset )
NINE_GRID_BRUSH_FLOAT_PROPERTY( LeftInsetScale, left_inset_scale )
NINE_GRID_BRUSH_FLOAT_PROPERTY( RightInset, right_inset )
NINE_GRID_BRUSH_FLOAT_PROPERTY( RightInsetScale, right_inset_scale )
NINE_GRID_BRUSH_FLOAT_PROPERTY( TopInset, top_inset )
NINE_GRID_BRUSH_FLOAT_PROPERTY( TopInsetScale, top_inset_scale )

#undef NINE_GRID_BRUSH_FLOAT_PROPERTY

static HRESULT WINAPI nine_grid_brush_get_IsCenterHollow( struct nine_grid_brush_iface *iface,
        boolean *value )
{
    if (!value) return E_POINTER;
    *value = impl_from_nine_grid_brush( iface )->center_hollow;
    return S_OK;
}

static HRESULT WINAPI nine_grid_brush_put_IsCenterHollow( struct nine_grid_brush_iface *iface,
        boolean value )
{
    impl_from_nine_grid_brush( iface )->center_hollow = value;
    return S_OK;
}

static void nine_grid_brush_bind( struct nine_grid_brush_iface *iface, IDCompositionVisual *visual )
{
    struct nine_grid_brush *impl = impl_from_nine_grid_brush( iface );
    ICompositionSurfaceBrush *surface_brush;

    impl->dcomp_visual = visual;
    if (impl->source && SUCCEEDED(ICompositionBrush_QueryInterface( impl->source,
            &IID_ICompositionSurfaceBrush, (void **)&surface_brush )))
    {
        surface_brush_bind( surface_brush, visual );
        ICompositionSurfaceBrush_Release( surface_brush );
    }
    else if (visual) IDCompositionVisual_SetContent( visual, NULL );
}

static HRESULT WINAPI nine_grid_brush_get_Source( struct nine_grid_brush_iface *iface,
        ICompositionBrush **value )
{
    struct nine_grid_brush *impl = impl_from_nine_grid_brush( iface );

    if (!value) return E_POINTER;
    if (impl->source) ICompositionBrush_AddRef( impl->source );
    *value = impl->source;
    return S_OK;
}

static HRESULT WINAPI nine_grid_brush_put_Source( struct nine_grid_brush_iface *iface,
        ICompositionBrush *value )
{
    struct nine_grid_brush *impl = impl_from_nine_grid_brush( iface );
    IDCompositionVisual *visual = impl->dcomp_visual;

    if (value && composition_brush_contains( value, &impl->ICompositionBrush_iface, 0 )) return E_INVALIDARG;
    if (value) ICompositionBrush_AddRef( value );
    nine_grid_brush_bind( iface, NULL );
    if (impl->source) ICompositionBrush_Release( impl->source );
    impl->source = value;
    nine_grid_brush_bind( iface, visual );
    return S_OK;
}

static HRESULT WINAPI nine_grid_brush_SetInsets( struct nine_grid_brush_iface *iface, FLOAT inset )
{
    struct nine_grid_brush *impl = impl_from_nine_grid_brush( iface );

    impl->left_inset = impl->top_inset = impl->right_inset = impl->bottom_inset = inset;
    return S_OK;
}

static HRESULT WINAPI nine_grid_brush_SetInsetsWithValues( struct nine_grid_brush_iface *iface,
        FLOAT left, FLOAT top, FLOAT right, FLOAT bottom )
{
    struct nine_grid_brush *impl = impl_from_nine_grid_brush( iface );

    impl->left_inset = left;
    impl->top_inset = top;
    impl->right_inset = right;
    impl->bottom_inset = bottom;
    return S_OK;
}

static HRESULT WINAPI nine_grid_brush_SetInsetScales( struct nine_grid_brush_iface *iface, FLOAT scale )
{
    struct nine_grid_brush *impl = impl_from_nine_grid_brush( iface );

    impl->left_inset_scale = impl->top_inset_scale = impl->right_inset_scale =
            impl->bottom_inset_scale = scale;
    return S_OK;
}

static HRESULT WINAPI nine_grid_brush_SetInsetScalesWithValues( struct nine_grid_brush_iface *iface,
        FLOAT left, FLOAT top, FLOAT right, FLOAT bottom )
{
    struct nine_grid_brush *impl = impl_from_nine_grid_brush( iface );

    impl->left_inset_scale = left;
    impl->top_inset_scale = top;
    impl->right_inset_scale = right;
    impl->bottom_inset_scale = bottom;
    return S_OK;
}

static const struct nine_grid_brush_vtbl nine_grid_brush_vtbl =
{
    nine_grid_brush_QueryInterface,
    nine_grid_brush_AddRef,
    nine_grid_brush_Release,
    nine_grid_brush_GetIids,
    nine_grid_brush_GetRuntimeClassName,
    nine_grid_brush_GetTrustLevel,
    nine_grid_brush_get_BottomInset,
    nine_grid_brush_put_BottomInset,
    nine_grid_brush_get_BottomInsetScale,
    nine_grid_brush_put_BottomInsetScale,
    nine_grid_brush_get_IsCenterHollow,
    nine_grid_brush_put_IsCenterHollow,
    nine_grid_brush_get_LeftInset,
    nine_grid_brush_put_LeftInset,
    nine_grid_brush_get_LeftInsetScale,
    nine_grid_brush_put_LeftInsetScale,
    nine_grid_brush_get_RightInset,
    nine_grid_brush_put_RightInset,
    nine_grid_brush_get_RightInsetScale,
    nine_grid_brush_put_RightInsetScale,
    nine_grid_brush_get_Source,
    nine_grid_brush_put_Source,
    nine_grid_brush_get_TopInset,
    nine_grid_brush_put_TopInset,
    nine_grid_brush_get_TopInsetScale,
    nine_grid_brush_put_TopInsetScale,
    nine_grid_brush_SetInsets,
    nine_grid_brush_SetInsetsWithValues,
    nine_grid_brush_SetInsetScales,
    nine_grid_brush_SetInsetScalesWithValues,
};

static const ICompositionBrushVtbl nine_grid_composition_brush_vtbl =
{
    nine_grid_composition_brush_QueryInterface,
    nine_grid_composition_brush_AddRef,
    nine_grid_composition_brush_Release,
    nine_grid_composition_brush_GetIids,
    nine_grid_composition_brush_GetRuntimeClassName,
    nine_grid_composition_brush_GetTrustLevel,
};

static HRESULT WINAPI nine_grid_composition_object_get_Compositor( ICompositionObject *iface,
        ICompositor **value )
{
    struct nine_grid_brush *impl = nine_grid_brush_from_ICompositionObject( iface );

    if (!value) return E_POINTER;
    ICompositor_AddRef( impl->compositor );
    *value = impl->compositor;
    return S_OK;
}

static HRESULT WINAPI nine_grid_composition_object_get_Dispatcher( ICompositionObject *iface,
        ICoreDispatcher **value )
{
    if (!value) return E_POINTER;
    *value = NULL;
    return S_OK;
}

static HRESULT WINAPI nine_grid_composition_object_get_Properties( ICompositionObject *iface,
        ICompositionPropertySet **value )
{
    if (!value) return E_POINTER;
    *value = NULL;
    return S_OK;
}

static HRESULT WINAPI nine_grid_composition_object_StartAnimation( ICompositionObject *iface,
        HSTRING name, ICompositionAnimation *animation )
{
    TRACE( "iface %p, name %s, animation %p.\n", iface, debugstr_hstring( name ), animation );
    return S_OK;
}

static HRESULT WINAPI nine_grid_composition_object_StopAnimation( ICompositionObject *iface,
        HSTRING name )
{
    TRACE( "iface %p, name %s.\n", iface, debugstr_hstring( name ) );
    return S_OK;
}

static const ICompositionObjectVtbl nine_grid_composition_object_vtbl =
{
    nine_grid_composition_object_QueryInterface,
    nine_grid_composition_object_AddRef,
    nine_grid_composition_object_Release,
    nine_grid_composition_object_GetIids,
    nine_grid_composition_object_GetRuntimeClassName,
    nine_grid_composition_object_GetTrustLevel,
    nine_grid_composition_object_get_Compositor,
    nine_grid_composition_object_get_Dispatcher,
    nine_grid_composition_object_get_Properties,
    nine_grid_composition_object_StartAnimation,
    nine_grid_composition_object_StopAnimation,
};

static HRESULT WINAPI nine_grid_closable_Close( IClosable *iface )
{
    TRACE( "iface %p.\n", iface );
    return S_OK;
}

static const IClosableVtbl nine_grid_closable_vtbl =
{
    nine_grid_closable_QueryInterface,
    nine_grid_closable_AddRef,
    nine_grid_closable_Release,
    nine_grid_closable_GetIids,
    nine_grid_closable_GetRuntimeClassName,
    nine_grid_closable_GetTrustLevel,
    nine_grid_closable_Close,
};

static HRESULT nine_grid_brush_create( ICompositor *compositor, IInspectable **result )
{
    struct nine_grid_brush *impl;

    if (!result) return E_POINTER;
    *result = NULL;
    if (!(impl = calloc( 1, sizeof(*impl) ))) return E_OUTOFMEMORY;
    impl->ICompositionNineGridBrush_iface.lpVtbl = &nine_grid_brush_vtbl;
    impl->ICompositionBrush_iface.lpVtbl = &nine_grid_composition_brush_vtbl;
    impl->ICompositionObject_iface.lpVtbl = &nine_grid_composition_object_vtbl;
    impl->IClosable_iface.lpVtbl = &nine_grid_closable_vtbl;
    impl->compositor = compositor;
    impl->bottom_inset_scale = 1.0f;
    impl->left_inset_scale = 1.0f;
    impl->right_inset_scale = 1.0f;
    impl->top_inset_scale = 1.0f;
    impl->ref = 1;
    ICompositor_AddRef( compositor );
    *result = (IInspectable *)&impl->ICompositionNineGridBrush_iface;
    return S_OK;
}

enum property_value_type
{
    PROPERTY_VALUE_COLOR,
    PROPERTY_VALUE_MATRIX3X2,
    PROPERTY_VALUE_MATRIX4X4,
    PROPERTY_VALUE_QUATERNION,
    PROPERTY_VALUE_SCALAR,
    PROPERTY_VALUE_VECTOR2,
    PROPERTY_VALUE_VECTOR3,
    PROPERTY_VALUE_VECTOR4,
};

struct property_value
{
    HSTRING name;
    enum property_value_type type;
    union
    {
        struct Color color;
        struct Matrix3x2 matrix3x2;
        struct Matrix4x4 matrix4x4;
        struct Quaternion quaternion;
        FLOAT scalar;
        struct Vector2 vector2;
        struct Vector3 vector3;
        struct Vector4 vector4;
    } value;
};

struct composition_property_set
{
    ICompositionPropertySet ICompositionPropertySet_iface;
    ICompositionObject ICompositionObject_iface;
    struct property_value *values;
    unsigned int count;
    ICompositor *compositor;
    LONG ref;
};

static inline struct composition_property_set *impl_from_ICompositionPropertySet(
        ICompositionPropertySet *iface )
{
    return CONTAINING_RECORD( iface, struct composition_property_set, ICompositionPropertySet_iface );
}

static inline struct composition_property_set *property_set_from_ICompositionObject(
        ICompositionObject *iface )
{
    return CONTAINING_RECORD( iface, struct composition_property_set, ICompositionObject_iface );
}

static HRESULT WINAPI composition_property_set_QueryInterface( ICompositionPropertySet *iface,
        REFIID iid, void **out )
{
    FIXME( "iface %p, iid %s, out %p.\n", iface, debugstr_guid( iid ), out );
    if (!out) return E_POINTER;
    if (IsEqualGUID( iid, &IID_IUnknown ) || IsEqualGUID( iid, &IID_IInspectable ) ||
        IsEqualGUID( iid, &IID_IAgileObject ) || IsEqualGUID( iid, &IID_ICompositionPropertySet ))
        *out = iface;
    else if (IsEqualGUID( iid, &IID_ICompositionObject ))
        *out = &impl_from_ICompositionPropertySet( iface )->ICompositionObject_iface;
    else
    {
        *out = NULL;
        return E_NOINTERFACE;
    }
    IUnknown_AddRef( (IUnknown *)*out );
    return S_OK;
}

static ULONG WINAPI composition_property_set_AddRef( ICompositionPropertySet *iface )
{
    return InterlockedIncrement( &impl_from_ICompositionPropertySet( iface )->ref );
}

static ULONG WINAPI composition_property_set_Release( ICompositionPropertySet *iface )
{
    struct composition_property_set *impl = impl_from_ICompositionPropertySet( iface );
    ULONG ref = InterlockedDecrement( &impl->ref );
    unsigned int i;

    if (!ref)
    {
        for (i = 0; i < impl->count; ++i) WindowsDeleteString( impl->values[i].name );
        free( impl->values );
        ICompositor_Release( impl->compositor );
        free( impl );
    }
    return ref;
}

static HRESULT WINAPI composition_property_set_GetIids( ICompositionPropertySet *iface,
        ULONG *count, IID **iids )
{
    IID *values;

    if (!count || !iids) return E_POINTER;
    if (!(values = CoTaskMemAlloc( 2 * sizeof(*values) ))) return E_OUTOFMEMORY;
    values[0] = IID_ICompositionPropertySet;
    values[1] = IID_ICompositionObject;
    *count = 2;
    *iids = values;
    return S_OK;
}

static HRESULT WINAPI composition_property_set_GetRuntimeClassName( ICompositionPropertySet *iface,
        HSTRING *name )
{
    if (!name) return E_POINTER;
    return WindowsCreateString( RuntimeClass_Windows_UI_Composition_CompositionPropertySet,
                                ARRAY_SIZE(RuntimeClass_Windows_UI_Composition_CompositionPropertySet) - 1,
                                name );
}

static HRESULT WINAPI composition_property_set_GetTrustLevel( ICompositionPropertySet *iface,
        TrustLevel *level )
{
    if (!level) return E_POINTER;
    *level = BaseTrust;
    return S_OK;
}

static struct property_value *composition_property_set_find( struct composition_property_set *impl,
        HSTRING name )
{
    const WCHAR *buffer = WindowsGetStringRawBuffer( name, NULL );
    unsigned int i;

    for (i = 0; i < impl->count; ++i)
        if (!wcscmp( buffer, WindowsGetStringRawBuffer( impl->values[i].name, NULL ) ))
            return &impl->values[i];
    return NULL;
}

static HRESULT composition_property_set_insert( struct composition_property_set *impl, HSTRING name,
        enum property_value_type type, const void *value, SIZE_T size )
{
    struct property_value *entry, *values;
    HRESULT hr;

    if (!name) return E_INVALIDARG;
    if (!(entry = composition_property_set_find( impl, name )))
    {
        if (!(values = realloc( impl->values, (impl->count + 1) * sizeof(*values) )))
            return E_OUTOFMEMORY;
        impl->values = values;
        entry = &impl->values[impl->count];
        memset( entry, 0, sizeof(*entry) );
        if (FAILED(hr = WindowsDuplicateString( name, &entry->name ))) return hr;
        ++impl->count;
    }
    entry->type = type;
    memcpy( &entry->value, value, size );
    return S_OK;
}

static HRESULT composition_property_set_try_get( struct composition_property_set *impl, HSTRING name,
        enum property_value_type type, void *value, SIZE_T size, enum CompositionGetValueStatus *result )
{
    struct property_value *entry;

    if (!value || !result) return E_POINTER;
    memset( value, 0, size );
    if (!name || !(entry = composition_property_set_find( impl, name )))
    {
        *result = CompositionGetValueStatus_NotFound;
        return S_OK;
    }
    if (entry->type != type)
    {
        *result = CompositionGetValueStatus_TypeMismatch;
        return S_OK;
    }
    memcpy( value, &entry->value, size );
    *result = CompositionGetValueStatus_Succeeded;
    return S_OK;
}

#define PROPERTY_SET_METHODS(name, type, kind, member) \
    static HRESULT WINAPI composition_property_set_Insert##name( ICompositionPropertySet *iface, \
            HSTRING key, type value ) \
    { \
        return composition_property_set_insert( impl_from_ICompositionPropertySet( iface ), key, \
                                                kind, &value, sizeof(value) ); \
    } \
    static HRESULT WINAPI composition_property_set_TryGet##name( ICompositionPropertySet *iface, \
            HSTRING key, type *value, enum CompositionGetValueStatus *result ) \
    { \
        return composition_property_set_try_get( impl_from_ICompositionPropertySet( iface ), key, \
                                                 kind, value, sizeof(*value), result ); \
    }

PROPERTY_SET_METHODS( Color, struct Color, PROPERTY_VALUE_COLOR, color )
PROPERTY_SET_METHODS( Matrix3x2, struct Matrix3x2, PROPERTY_VALUE_MATRIX3X2, matrix3x2 )
PROPERTY_SET_METHODS( Matrix4x4, struct Matrix4x4, PROPERTY_VALUE_MATRIX4X4, matrix4x4 )
PROPERTY_SET_METHODS( Quaternion, struct Quaternion, PROPERTY_VALUE_QUATERNION, quaternion )
PROPERTY_SET_METHODS( Scalar, FLOAT, PROPERTY_VALUE_SCALAR, scalar )
PROPERTY_SET_METHODS( Vector2, struct Vector2, PROPERTY_VALUE_VECTOR2, vector2 )
PROPERTY_SET_METHODS( Vector3, struct Vector3, PROPERTY_VALUE_VECTOR3, vector3 )
PROPERTY_SET_METHODS( Vector4, struct Vector4, PROPERTY_VALUE_VECTOR4, vector4 )

static const ICompositionPropertySetVtbl composition_property_set_vtbl =
{
    composition_property_set_QueryInterface,
    composition_property_set_AddRef,
    composition_property_set_Release,
    composition_property_set_GetIids,
    composition_property_set_GetRuntimeClassName,
    composition_property_set_GetTrustLevel,
    composition_property_set_InsertColor,
    composition_property_set_InsertMatrix3x2,
    composition_property_set_InsertMatrix4x4,
    composition_property_set_InsertQuaternion,
    composition_property_set_InsertScalar,
    composition_property_set_InsertVector2,
    composition_property_set_InsertVector3,
    composition_property_set_InsertVector4,
    composition_property_set_TryGetColor,
    composition_property_set_TryGetMatrix3x2,
    composition_property_set_TryGetMatrix4x4,
    composition_property_set_TryGetQuaternion,
    composition_property_set_TryGetScalar,
    composition_property_set_TryGetVector2,
    composition_property_set_TryGetVector3,
    composition_property_set_TryGetVector4,
};

static HRESULT WINAPI property_set_composition_object_QueryInterface( ICompositionObject *iface,
        REFIID iid, void **out )
{
    struct composition_property_set *impl = property_set_from_ICompositionObject( iface );
    return ICompositionPropertySet_QueryInterface( &impl->ICompositionPropertySet_iface, iid, out );
}

static ULONG WINAPI property_set_composition_object_AddRef( ICompositionObject *iface )
{
    struct composition_property_set *impl = property_set_from_ICompositionObject( iface );
    return ICompositionPropertySet_AddRef( &impl->ICompositionPropertySet_iface );
}

static ULONG WINAPI property_set_composition_object_Release( ICompositionObject *iface )
{
    struct composition_property_set *impl = property_set_from_ICompositionObject( iface );
    return ICompositionPropertySet_Release( &impl->ICompositionPropertySet_iface );
}

static HRESULT WINAPI property_set_composition_object_GetIids( ICompositionObject *iface,
        ULONG *count, IID **iids )
{
    struct composition_property_set *impl = property_set_from_ICompositionObject( iface );
    return ICompositionPropertySet_GetIids( &impl->ICompositionPropertySet_iface, count, iids );
}

static HRESULT WINAPI property_set_composition_object_GetRuntimeClassName( ICompositionObject *iface,
        HSTRING *name )
{
    struct composition_property_set *impl = property_set_from_ICompositionObject( iface );
    return ICompositionPropertySet_GetRuntimeClassName( &impl->ICompositionPropertySet_iface, name );
}

static HRESULT WINAPI property_set_composition_object_GetTrustLevel( ICompositionObject *iface,
        TrustLevel *level )
{
    struct composition_property_set *impl = property_set_from_ICompositionObject( iface );
    return ICompositionPropertySet_GetTrustLevel( &impl->ICompositionPropertySet_iface, level );
}

static HRESULT WINAPI property_set_composition_object_get_Compositor( ICompositionObject *iface,
        ICompositor **value )
{
    struct composition_property_set *impl = property_set_from_ICompositionObject( iface );

    if (!value) return E_POINTER;
    ICompositor_AddRef( impl->compositor );
    *value = impl->compositor;
    return S_OK;
}

static HRESULT WINAPI property_set_composition_object_get_Dispatcher( ICompositionObject *iface,
        ICoreDispatcher **value )
{
    if (!value) return E_POINTER;
    *value = NULL;
    return S_OK;
}

static HRESULT WINAPI property_set_composition_object_get_Properties( ICompositionObject *iface,
        ICompositionPropertySet **value )
{
    struct composition_property_set *impl = property_set_from_ICompositionObject( iface );

    if (!value) return E_POINTER;
    ICompositionPropertySet_AddRef( &impl->ICompositionPropertySet_iface );
    *value = &impl->ICompositionPropertySet_iface;
    return S_OK;
}

static HRESULT WINAPI property_set_composition_object_StartAnimation( ICompositionObject *iface,
        HSTRING name, ICompositionAnimation *animation )
{
    TRACE( "iface %p, name %s, animation %p.\n", iface, debugstr_hstring( name ), animation );
    return S_OK;
}

static HRESULT WINAPI property_set_composition_object_StopAnimation( ICompositionObject *iface,
        HSTRING name )
{
    TRACE( "iface %p, name %s.\n", iface, debugstr_hstring( name ) );
    return S_OK;
}

static const ICompositionObjectVtbl property_set_composition_object_vtbl =
{
    property_set_composition_object_QueryInterface,
    property_set_composition_object_AddRef,
    property_set_composition_object_Release,
    property_set_composition_object_GetIids,
    property_set_composition_object_GetRuntimeClassName,
    property_set_composition_object_GetTrustLevel,
    property_set_composition_object_get_Compositor,
    property_set_composition_object_get_Dispatcher,
    property_set_composition_object_get_Properties,
    property_set_composition_object_StartAnimation,
    property_set_composition_object_StopAnimation,
};

HRESULT composition_property_set_create( ICompositor *compositor, ICompositionPropertySet **result )
{
    struct composition_property_set *impl;

    if (!result) return E_POINTER;
    *result = NULL;
    if (!(impl = calloc( 1, sizeof(*impl) ))) return E_OUTOFMEMORY;
    impl->ICompositionPropertySet_iface.lpVtbl = &composition_property_set_vtbl;
    impl->ICompositionObject_iface.lpVtbl = &property_set_composition_object_vtbl;
    impl->compositor = compositor;
    impl->ref = 1;
    ICompositor_AddRef( compositor );
    *result = &impl->ICompositionPropertySet_iface;
    return S_OK;
}

struct expression_animation_reference
{
    struct list entry;
    HSTRING key;
    ICompositionObject *object;
};

struct expression_animation
{
    IExpressionAnimation IExpressionAnimation_iface;
    ICompositionAnimation ICompositionAnimation_iface;
    struct list references;
    HSTRING expression;
    LONG ref;
};

static inline struct expression_animation *impl_from_IExpressionAnimation( IExpressionAnimation *iface )
{
    return CONTAINING_RECORD( iface, struct expression_animation, IExpressionAnimation_iface );
}

static inline struct expression_animation *impl_from_ICompositionAnimation( ICompositionAnimation *iface )
{
    return CONTAINING_RECORD( iface, struct expression_animation, ICompositionAnimation_iface );
}

static HRESULT expression_animation_query_interface( struct expression_animation *impl,
        REFIID iid, void **out )
{
    if (!out) return E_POINTER;
    if (IsEqualGUID( iid, &IID_IUnknown ) || IsEqualGUID( iid, &IID_IInspectable ) ||
        IsEqualGUID( iid, &IID_IAgileObject ) || IsEqualGUID( iid, &IID_IExpressionAnimation ))
        *out = &impl->IExpressionAnimation_iface;
    else if (IsEqualGUID( iid, &IID_ICompositionAnimation ))
        *out = &impl->ICompositionAnimation_iface;
    else
    {
        *out = NULL;
        return E_NOINTERFACE;
    }
    IUnknown_AddRef( (IUnknown *)*out );
    return S_OK;
}

static ULONG expression_animation_add_ref( struct expression_animation *impl )
{
    return InterlockedIncrement( &impl->ref );
}

static ULONG expression_animation_release( struct expression_animation *impl )
{
    struct expression_animation_reference *reference, *next;
    ULONG ref = InterlockedDecrement( &impl->ref );

    if (!ref)
    {
        LIST_FOR_EACH_ENTRY_SAFE( reference, next, &impl->references,
                struct expression_animation_reference, entry )
        {
            ICompositionObject_Release( reference->object );
            WindowsDeleteString( reference->key );
            free( reference );
        }
        WindowsDeleteString( impl->expression );
        free( impl );
    }
    return ref;
}

static HRESULT expression_animation_get_iids( ULONG *count, IID **iids )
{
    IID *values;

    if (!count || !iids) return E_POINTER;
    if (!(values = CoTaskMemAlloc( 2 * sizeof(*values) ))) return E_OUTOFMEMORY;
    values[0] = IID_IExpressionAnimation;
    values[1] = IID_ICompositionAnimation;
    *count = 2;
    *iids = values;
    return S_OK;
}

static HRESULT expression_animation_get_class_name( HSTRING *name )
{
    if (!name) return E_POINTER;
    return WindowsCreateString( RuntimeClass_Windows_UI_Composition_ExpressionAnimation,
                                ARRAY_SIZE(RuntimeClass_Windows_UI_Composition_ExpressionAnimation) - 1,
                                name );
}

static HRESULT expression_animation_get_trust_level( TrustLevel *level )
{
    if (!level) return E_POINTER;
    *level = BaseTrust;
    return S_OK;
}

static HRESULT WINAPI expression_animation_QueryInterface( IExpressionAnimation *iface,
        REFIID iid, void **out )
{
    FIXME( "iface %p, iid %s, out %p.\n", iface, debugstr_guid( iid ), out );
    return expression_animation_query_interface( impl_from_IExpressionAnimation( iface ), iid, out );
}

static ULONG WINAPI expression_animation_AddRef( IExpressionAnimation *iface )
{
    return expression_animation_add_ref( impl_from_IExpressionAnimation( iface ) );
}

static ULONG WINAPI expression_animation_Release( IExpressionAnimation *iface )
{
    return expression_animation_release( impl_from_IExpressionAnimation( iface ) );
}

static HRESULT WINAPI expression_animation_GetIids( IExpressionAnimation *iface,
        ULONG *count, IID **iids )
{
    return expression_animation_get_iids( count, iids );
}

static HRESULT WINAPI expression_animation_GetRuntimeClassName( IExpressionAnimation *iface,
        HSTRING *name )
{
    return expression_animation_get_class_name( name );
}

static HRESULT WINAPI expression_animation_GetTrustLevel( IExpressionAnimation *iface,
        TrustLevel *level )
{
    return expression_animation_get_trust_level( level );
}

static HRESULT WINAPI expression_animation_get_Expression( IExpressionAnimation *iface, HSTRING *value )
{
    if (!value) return E_POINTER;
    return WindowsDuplicateString( impl_from_IExpressionAnimation( iface )->expression, value );
}

static HRESULT WINAPI expression_animation_put_Expression( IExpressionAnimation *iface, HSTRING value )
{
    struct expression_animation *impl = impl_from_IExpressionAnimation( iface );
    HSTRING copy = NULL;
    HRESULT hr;

    if (value && FAILED(hr = WindowsDuplicateString( value, &copy ))) return hr;
    WindowsDeleteString( impl->expression );
    impl->expression = copy;
    return S_OK;
}

static const IExpressionAnimationVtbl expression_animation_vtbl =
{
    expression_animation_QueryInterface,
    expression_animation_AddRef,
    expression_animation_Release,
    expression_animation_GetIids,
    expression_animation_GetRuntimeClassName,
    expression_animation_GetTrustLevel,
    expression_animation_get_Expression,
    expression_animation_put_Expression,
};

static HRESULT WINAPI composition_animation_QueryInterface( ICompositionAnimation *iface,
        REFIID iid, void **out )
{
    return expression_animation_query_interface( impl_from_ICompositionAnimation( iface ), iid, out );
}

static ULONG WINAPI composition_animation_AddRef( ICompositionAnimation *iface )
{
    return expression_animation_add_ref( impl_from_ICompositionAnimation( iface ) );
}

static ULONG WINAPI composition_animation_Release( ICompositionAnimation *iface )
{
    return expression_animation_release( impl_from_ICompositionAnimation( iface ) );
}

static HRESULT WINAPI composition_animation_GetIids( ICompositionAnimation *iface,
        ULONG *count, IID **iids )
{
    return expression_animation_get_iids( count, iids );
}

static HRESULT WINAPI composition_animation_GetRuntimeClassName( ICompositionAnimation *iface,
        HSTRING *name )
{
    return expression_animation_get_class_name( name );
}

static HRESULT WINAPI composition_animation_GetTrustLevel( ICompositionAnimation *iface,
        TrustLevel *level )
{
    return expression_animation_get_trust_level( level );
}

static HRESULT WINAPI composition_animation_ClearAllParameters( ICompositionAnimation *iface )
{
    struct expression_animation *impl = impl_from_ICompositionAnimation( iface );
    struct expression_animation_reference *reference, *next;

    LIST_FOR_EACH_ENTRY_SAFE( reference, next, &impl->references,
            struct expression_animation_reference, entry )
    {
        list_remove( &reference->entry );
        ICompositionObject_Release( reference->object );
        WindowsDeleteString( reference->key );
        free( reference );
    }
    return S_OK;
}

static HRESULT WINAPI composition_animation_ClearParameter( ICompositionAnimation *iface, HSTRING key )
{
    struct expression_animation *impl = impl_from_ICompositionAnimation( iface );
    struct expression_animation_reference *reference;
    const WCHAR *name;

    if (!key) return E_INVALIDARG;
    name = WindowsGetStringRawBuffer( key, NULL );
    LIST_FOR_EACH_ENTRY( reference, &impl->references, struct expression_animation_reference, entry )
    {
        if (wcscmp( name, WindowsGetStringRawBuffer( reference->key, NULL ) )) continue;
        list_remove( &reference->entry );
        ICompositionObject_Release( reference->object );
        WindowsDeleteString( reference->key );
        free( reference );
        break;
    }
    return S_OK;
}

#define COMPOSITION_ANIMATION_PARAMETER(name, type) \
    static HRESULT WINAPI composition_animation_Set##name##Parameter( ICompositionAnimation *iface, \
            HSTRING key, type value ) \
    { \
        TRACE( "iface %p, key %s.\n", iface, debugstr_hstring( key ) ); \
        return S_OK; \
    }

COMPOSITION_ANIMATION_PARAMETER( Color, struct Color )
COMPOSITION_ANIMATION_PARAMETER( Matrix3x2, struct Matrix3x2 )
COMPOSITION_ANIMATION_PARAMETER( Matrix4x4, struct Matrix4x4 )
COMPOSITION_ANIMATION_PARAMETER( Quaternion, struct Quaternion )
COMPOSITION_ANIMATION_PARAMETER( Scalar, FLOAT )
COMPOSITION_ANIMATION_PARAMETER( Vector2, struct Vector2 )
COMPOSITION_ANIMATION_PARAMETER( Vector3, struct Vector3 )
COMPOSITION_ANIMATION_PARAMETER( Vector4, struct Vector4 )

static HRESULT WINAPI composition_animation_SetReferenceParameter( ICompositionAnimation *iface,
        HSTRING key, ICompositionObject *object )
{
    struct expression_animation *impl = impl_from_ICompositionAnimation( iface );
    struct expression_animation_reference *reference;
    const WCHAR *name;
    HRESULT hr;

    TRACE( "iface %p, key %s, object %p.\n", iface, debugstr_hstring( key ), object );
    if (!key || !object) return E_INVALIDARG;
    name = WindowsGetStringRawBuffer( key, NULL );
    LIST_FOR_EACH_ENTRY( reference, &impl->references, struct expression_animation_reference, entry )
    {
        if (wcscmp( name, WindowsGetStringRawBuffer( reference->key, NULL ) )) continue;
        ICompositionObject_AddRef( object );
        ICompositionObject_Release( reference->object );
        reference->object = object;
        return S_OK;
    }

    if (!(reference = calloc( 1, sizeof(*reference) ))) return E_OUTOFMEMORY;
    if (FAILED(hr = WindowsDuplicateString( key, &reference->key )))
    {
        free( reference );
        return hr;
    }
    if (object) ICompositionObject_AddRef( object );
    reference->object = object;
    list_add_tail( &impl->references, &reference->entry );
    return S_OK;
}

static const ICompositionAnimationVtbl composition_animation_vtbl =
{
    composition_animation_QueryInterface,
    composition_animation_AddRef,
    composition_animation_Release,
    composition_animation_GetIids,
    composition_animation_GetRuntimeClassName,
    composition_animation_GetTrustLevel,
    composition_animation_ClearAllParameters,
    composition_animation_ClearParameter,
    composition_animation_SetColorParameter,
    composition_animation_SetMatrix3x2Parameter,
    composition_animation_SetMatrix4x4Parameter,
    composition_animation_SetQuaternionParameter,
    composition_animation_SetReferenceParameter,
    composition_animation_SetScalarParameter,
    composition_animation_SetVector2Parameter,
    composition_animation_SetVector3Parameter,
    composition_animation_SetVector4Parameter,
};

static HRESULT expression_animation_create( HSTRING expression, IExpressionAnimation **result )
{
    struct expression_animation *impl;
    HRESULT hr;

    if (!result) return E_POINTER;
    *result = NULL;
    if (!(impl = calloc( 1, sizeof(*impl) ))) return E_OUTOFMEMORY;
    impl->IExpressionAnimation_iface.lpVtbl = &expression_animation_vtbl;
    impl->ICompositionAnimation_iface.lpVtbl = &composition_animation_vtbl;
    list_init( &impl->references );
    impl->ref = 1;
    if (expression && FAILED(hr = WindowsDuplicateString( expression, &impl->expression )))
    {
        free( impl );
        return hr;
    }
    *result = &impl->IExpressionAnimation_iface;
    return S_OK;
}

static HRESULT expression_animation_clone( IExpressionAnimation *animation, IExpressionAnimation **result )
{
    struct expression_animation *impl;
    struct expression_animation_reference *reference;
    ICompositionAnimation *copy;
    HRESULT hr;

    if (animation->lpVtbl != &expression_animation_vtbl) return E_NOINTERFACE;
    impl = impl_from_IExpressionAnimation( animation );
    if (FAILED(hr = expression_animation_create( impl->expression, result ))) return hr;
    copy = &impl_from_IExpressionAnimation( *result )->ICompositionAnimation_iface;
    LIST_FOR_EACH_ENTRY( reference, &impl->references, struct expression_animation_reference, entry )
    {
        if (FAILED(hr = ICompositionAnimation_SetReferenceParameter( copy, reference->key, reference->object )))
        {
            IExpressionAnimation_Release( *result );
            *result = NULL;
            return hr;
        }
    }
    return S_OK;
}

struct key_frame_animation_iface;

struct key_frame_animation_vtbl
{
    HRESULT (WINAPI *QueryInterface)( struct key_frame_animation_iface *iface, REFIID iid, void **out );
    ULONG (WINAPI *AddRef)( struct key_frame_animation_iface *iface );
    ULONG (WINAPI *Release)( struct key_frame_animation_iface *iface );
    HRESULT (WINAPI *GetIids)( struct key_frame_animation_iface *iface, ULONG *count, IID **iids );
    HRESULT (WINAPI *GetRuntimeClassName)( struct key_frame_animation_iface *iface, HSTRING *name );
    HRESULT (WINAPI *GetTrustLevel)( struct key_frame_animation_iface *iface, TrustLevel *level );
    HRESULT (WINAPI *get_DelayTime)( struct key_frame_animation_iface *iface, TimeSpan *value );
    HRESULT (WINAPI *put_DelayTime)( struct key_frame_animation_iface *iface, TimeSpan value );
    HRESULT (WINAPI *get_Duration)( struct key_frame_animation_iface *iface, TimeSpan *value );
    HRESULT (WINAPI *put_Duration)( struct key_frame_animation_iface *iface, TimeSpan value );
    HRESULT (WINAPI *get_IterationBehavior)( struct key_frame_animation_iface *iface, INT32 *value );
    HRESULT (WINAPI *put_IterationBehavior)( struct key_frame_animation_iface *iface, INT32 value );
    HRESULT (WINAPI *get_IterationCount)( struct key_frame_animation_iface *iface, INT32 *value );
    HRESULT (WINAPI *put_IterationCount)( struct key_frame_animation_iface *iface, INT32 value );
    HRESULT (WINAPI *get_KeyFrameCount)( struct key_frame_animation_iface *iface, INT32 *value );
    HRESULT (WINAPI *get_StopBehavior)( struct key_frame_animation_iface *iface, INT32 *value );
    HRESULT (WINAPI *put_StopBehavior)( struct key_frame_animation_iface *iface, INT32 value );
    HRESULT (WINAPI *InsertExpressionKeyFrame)( struct key_frame_animation_iface *iface,
            FLOAT key, HSTRING expression );
    HRESULT (WINAPI *InsertExpressionKeyFrameWithEasingFunction)( struct key_frame_animation_iface *iface,
            FLOAT key, HSTRING expression, ICompositionEasingFunction *function );
};

struct key_frame_animation_iface
{
    const struct key_frame_animation_vtbl *lpVtbl;
};

struct boolean_key_frame_animation_iface;

struct boolean_key_frame_animation_vtbl
{
    HRESULT (WINAPI *QueryInterface)( struct boolean_key_frame_animation_iface *iface,
            REFIID iid, void **out );
    ULONG (WINAPI *AddRef)( struct boolean_key_frame_animation_iface *iface );
    ULONG (WINAPI *Release)( struct boolean_key_frame_animation_iface *iface );
    HRESULT (WINAPI *GetIids)( struct boolean_key_frame_animation_iface *iface,
            ULONG *count, IID **iids );
    HRESULT (WINAPI *GetRuntimeClassName)( struct boolean_key_frame_animation_iface *iface,
            HSTRING *name );
    HRESULT (WINAPI *GetTrustLevel)( struct boolean_key_frame_animation_iface *iface,
            TrustLevel *level );
    HRESULT (WINAPI *InsertKeyFrame)( struct boolean_key_frame_animation_iface *iface,
            FLOAT key, boolean value );
};

struct boolean_key_frame_animation_iface
{
    const struct boolean_key_frame_animation_vtbl *lpVtbl;
};

static const GUID key_frame_animation_iid =
    {0x126e7f22,0x3ae9,0x4540,{0x9a,0x8a,0xde,0xae,0x8a,0x4a,0x4a,0x84}};
static const GUID boolean_key_frame_animation_iid =
    {0x95e23a08,0xd1f4,0x4972,{0x97,0x70,0x3e,0xfe,0x68,0xd8,0x2e,0x14}};

static void expression_skip_space( const WCHAR **cursor )
{
    while (**cursor == ' ' || **cursor == '\t' || **cursor == '\r' || **cursor == '\n') ++*cursor;
}

static HRESULT expression_reference_scalar( struct expression_animation *impl,
        const WCHAR *key, UINT key_length, const WCHAR *property, UINT property_length, FLOAT *value )
{
    struct expression_animation_reference *reference;
    ICompositionPropertySet *properties;
    IInteractionTracker *tracker;
    enum CompositionGetValueStatus status;
    struct Vector3 position;
    HSTRING name;
    HRESULT hr;

    LIST_FOR_EACH_ENTRY( reference, &impl->references, struct expression_animation_reference, entry )
    {
        UINT length;
        const WCHAR *stored = WindowsGetStringRawBuffer( reference->key, &length );
        if (length != key_length || wcsncmp( stored, key, length )) continue;
        if (SUCCEEDED(ICompositionObject_QueryInterface( reference->object,
                &IID_IInteractionTracker, (void **)&tracker )))
        {
            hr = E_NOTIMPL;
            if (property_length == 5 && !wcsncmp( property, L"Scale", 5 ))
                hr = IInteractionTracker_get_Scale( tracker, value );
            else if (property_length == 10 && !wcsncmp( property, L"Position.", 9 ))
            {
                hr = IInteractionTracker_get_Position( tracker, &position );
                if (SUCCEEDED(hr))
                {
                    if (property[9] == 'X') *value = position.X;
                    else if (property[9] == 'Y') *value = position.Y;
                    else if (property[9] == 'Z') *value = position.Z;
                    else hr = E_NOTIMPL;
                }
            }
            IInteractionTracker_Release( tracker );
            if (hr != E_NOTIMPL) return hr;
        }
        if (FAILED(hr = ICompositionObject_get_Properties( reference->object, &properties ))) return hr;
        if (!properties) return E_NOTIMPL;
        if (FAILED(hr = WindowsCreateString( property, property_length, &name )))
        {
            ICompositionPropertySet_Release( properties );
            return hr;
        }
        hr = ICompositionPropertySet_TryGetScalar( properties, name, &value[0], &status );
        WindowsDeleteString( name );
        ICompositionPropertySet_Release( properties );
        if (SUCCEEDED(hr) && status != CompositionGetValueStatus_Succeeded) hr = E_NOTIMPL;
        return hr;
    }
    return E_NOTIMPL;
}

static BOOL expression_identifier_char( WCHAR value )
{
    return (value >= 'a' && value <= 'z') || (value >= 'A' && value <= 'Z') ||
            (value >= '0' && value <= '9') || value == '_';
}

static HRESULT expression_scalar( struct expression_animation *impl, const WCHAR **cursor, FLOAT *value )
{
    const WCHAR *key, *property;
    WCHAR *end;
    UINT length;
    FLOAT sign = 1.0f;
    HRESULT hr;

    expression_skip_space( cursor );
    if (**cursor == '+' || **cursor == '-')
    {
        if (**cursor == '-') sign = -1.0f;
        ++*cursor;
        expression_skip_space( cursor );
    }
    if ((**cursor >= '0' && **cursor <= '9') || **cursor == '.')
    {
        *value = wcstod( *cursor, &end ) * sign;
        if (end == *cursor) return E_NOTIMPL;
        *cursor = end;
        return S_OK;
    }
    key = *cursor;
    while (expression_identifier_char( **cursor )) ++*cursor;
    length = *cursor - key;
    if (!length || **cursor != '.') return E_NOTIMPL;
    property = ++*cursor;
    while (expression_identifier_char( **cursor ) || **cursor == '.') ++*cursor;
    if (FAILED(hr = expression_reference_scalar( impl, key, length, property, *cursor - property, value ))) return hr;
    *value *= sign;
    return S_OK;
}

static HRESULT expression_animation_get_vector( IExpressionAnimation *animation, struct Vector3 *value )
{
    struct expression_animation *impl;
    const WCHAR *cursor;
    struct Vector3 result;
    HRESULT hr;

    if (animation->lpVtbl != &expression_animation_vtbl) return E_NOINTERFACE;
    impl = impl_from_IExpressionAnimation( animation );
    cursor = WindowsGetStringRawBuffer( impl->expression, NULL );
    expression_skip_space( &cursor );
    if (wcsncmp( cursor, L"Vector3", 7 )) return E_NOTIMPL;
    cursor += 7;
    expression_skip_space( &cursor );
    if (*cursor++ != '(') return E_NOTIMPL;
    if (FAILED(hr = expression_scalar( impl, &cursor, &result.X ))) return hr;
    expression_skip_space( &cursor );
    if (*cursor++ != ',') return E_NOTIMPL;
    if (FAILED(hr = expression_scalar( impl, &cursor, &result.Y ))) return hr;
    expression_skip_space( &cursor );
    if (*cursor++ != ',') return E_NOTIMPL;
    if (FAILED(hr = expression_scalar( impl, &cursor, &result.Z ))) return hr;
    expression_skip_space( &cursor );
    if (*cursor++ != ')') return E_NOTIMPL;
    expression_skip_space( &cursor );
    if (*cursor) return E_NOTIMPL;
    *value = result;
    return S_OK;
}

struct animation_boolean_parameter
{
    struct animation_boolean_parameter *next;
    HSTRING key;
    boolean value;
};
struct animation2_state
{
    struct animation2_iface iface;
    ICompositionAnimation *animation;
    HSTRING target;
    struct animation_boolean_parameter *parameters;
};

DEFINE_IINSPECTABLE_(animation2, struct animation2_iface, struct animation2_state,
        impl_from_animation2, iface, impl->animation)

static HRESULT WINAPI animation2_get_Target(struct animation2_iface *iface,HSTRING *value)
{
    HRESULT hr;
    if (!value) return E_POINTER;
    *value = NULL;
    EnterCriticalSection(&visual_graph_cs);
    hr = WindowsDuplicateString(impl_from_animation2(iface)->target,value);
    LeaveCriticalSection(&visual_graph_cs);
    return hr;
}

static HRESULT WINAPI animation2_put_Target(struct animation2_iface *iface,HSTRING value)
{
    struct animation2_state *impl = impl_from_animation2(iface);
    HSTRING copy, previous;
    HRESULT hr;

    if (FAILED(hr = WindowsDuplicateString(value,&copy))) return hr;
    EnterCriticalSection(&visual_graph_cs);
    previous = impl->target;
    impl->target = copy;
    LeaveCriticalSection(&visual_graph_cs);
    WindowsDeleteString(previous);
    return S_OK;
}

static void animation2_clear_parameters(struct animation2_state *impl,HSTRING key)
{
    struct animation_boolean_parameter **entry, *removed;
    INT32 comparison;

    EnterCriticalSection(&visual_graph_cs);
    entry = &impl->parameters;
    while (*entry)
    {
        comparison = 1;
        if (key) WindowsCompareStringOrdinal(key,(*entry)->key,&comparison);
        if (!key || !comparison)
        {
            removed = *entry;
            *entry = removed->next;
            WindowsDeleteString(removed->key);
            free(removed);
        }
        else entry = &(*entry)->next;
    }
    LeaveCriticalSection(&visual_graph_cs);
}

static HRESULT WINAPI animation2_SetBooleanParameter(struct animation2_iface *iface,HSTRING key,
        boolean value)
{
    struct animation2_state *impl = impl_from_animation2(iface);
    struct animation_boolean_parameter *entry;
    INT32 comparison;
    HRESULT hr = S_OK;

    if (!WindowsGetStringLen(key)) return E_INVALIDARG;
    EnterCriticalSection(&visual_graph_cs);
    for (entry = impl->parameters; entry; entry = entry->next)
    {
        WindowsCompareStringOrdinal(key,entry->key,&comparison);
        if (!comparison) break;
    }
    if (!entry)
    {
        if (!(entry = calloc(1,sizeof(*entry)))) hr = E_OUTOFMEMORY;
        else if (FAILED(hr = WindowsDuplicateString(key,&entry->key))) free(entry);
        else
        {
            entry->next = impl->parameters;
            impl->parameters = entry;
        }
    }
    if (SUCCEEDED(hr)) entry->value = value;
    LeaveCriticalSection(&visual_graph_cs);
    return hr;
}

static const struct animation2_vtbl animation2_vtbl =
{
    animation2_QueryInterface,
    animation2_AddRef,
    animation2_Release,
    animation2_GetIids,
    animation2_GetRuntimeClassName,
    animation2_GetTrustLevel,
    animation2_SetBooleanParameter,
    animation2_get_Target,
    animation2_put_Target,
};

struct scalar_key_frame_animation
{
    IScalarKeyFrameAnimation IScalarKeyFrameAnimation_iface;
    struct animation2_state animation2;
    IVector3KeyFrameAnimation IVector3KeyFrameAnimation_iface;
    struct boolean_key_frame_animation_iface IBooleanKeyFrameAnimation_iface;
    struct key_frame_animation_iface IKeyFrameAnimation_iface;
    ICompositionAnimation ICompositionAnimation_iface;
    ICompositionAnimationBase ICompositionAnimationBase_iface;
    ICompositionObject ICompositionObject_iface;
    ICompositor *compositor;
    TimeSpan delay_time;
    TimeSpan duration;
    INT32 iteration_behavior;
    INT32 iteration_count;
    INT32 key_frame_count;
    INT32 stop_behavior;
    FLOAT final_key;
    FLOAT final_value;
    struct Vector3 final_vector;
    boolean has_final_value;
    boolean is_boolean;
    boolean is_vector3;
    LONG ref;
};

static const IScalarKeyFrameAnimationVtbl scalar_key_frame_animation_vtbl;
static const IVector3KeyFrameAnimationVtbl vector3_key_frame_animation_vtbl;
static const struct boolean_key_frame_animation_vtbl boolean_key_frame_animation_vtbl;

static inline struct scalar_key_frame_animation *impl_from_IScalarKeyFrameAnimation(
        IScalarKeyFrameAnimation *iface )
{
    return CONTAINING_RECORD( iface, struct scalar_key_frame_animation, IScalarKeyFrameAnimation_iface );
}

static inline struct scalar_key_frame_animation *scalar_animation_from_ICompositionAnimation(
        ICompositionAnimation *iface )
{
    return CONTAINING_RECORD( iface, struct scalar_key_frame_animation, ICompositionAnimation_iface );
}

static inline struct scalar_key_frame_animation *impl_from_IVector3KeyFrameAnimation(
        IVector3KeyFrameAnimation *iface )
{
    return CONTAINING_RECORD( iface, struct scalar_key_frame_animation, IVector3KeyFrameAnimation_iface );
}

static inline struct scalar_key_frame_animation *scalar_animation_from_IBooleanKeyFrameAnimation(
        struct boolean_key_frame_animation_iface *iface )
{
    return CONTAINING_RECORD( iface, struct scalar_key_frame_animation,
                              IBooleanKeyFrameAnimation_iface );
}

static HRESULT scalar_key_frame_animation_get_final_value( ICompositionAnimation *animation,
        FLOAT *value )
{
    struct scalar_key_frame_animation *impl;
    IScalarKeyFrameAnimation *scalar;
    HRESULT hr;

    if (!value) return E_POINTER;
    if (FAILED(hr = ICompositionAnimation_QueryInterface( animation,
            &IID_IScalarKeyFrameAnimation, (void **)&scalar ))) return hr;
    if (scalar->lpVtbl != &scalar_key_frame_animation_vtbl)
    {
        IScalarKeyFrameAnimation_Release( scalar );
        return E_NOINTERFACE;
    }
    impl = impl_from_IScalarKeyFrameAnimation( scalar );
    if (!impl->has_final_value)
        hr = E_BOUNDS;
    else
        *value = impl->final_value;
    IScalarKeyFrameAnimation_Release( scalar );
    return hr;
}

static HRESULT boolean_key_frame_animation_get_final_value( ICompositionAnimation *animation,
        boolean *value )
{
    struct boolean_key_frame_animation_iface *boolean_animation;
    struct scalar_key_frame_animation *impl;
    HRESULT hr;

    if (!value) return E_POINTER;
    if (FAILED(hr = ICompositionAnimation_QueryInterface( animation,
            &boolean_key_frame_animation_iid, (void **)&boolean_animation ))) return hr;
    if (boolean_animation->lpVtbl != &boolean_key_frame_animation_vtbl)
    {
        boolean_animation->lpVtbl->Release( boolean_animation );
        return E_NOINTERFACE;
    }
    impl = scalar_animation_from_IBooleanKeyFrameAnimation( boolean_animation );
    if (!impl->has_final_value)
        hr = E_BOUNDS;
    else
        *value = impl->final_value != 0.0f;
    boolean_animation->lpVtbl->Release( boolean_animation );
    return hr;
}

static HRESULT vector3_key_frame_animation_get_final_value( ICompositionAnimation *animation,
        struct Vector3 *value )
{
    struct scalar_key_frame_animation *impl;
    IVector3KeyFrameAnimation *vector;
    HRESULT hr;

    if (!value) return E_POINTER;
    if (FAILED(hr = ICompositionAnimation_QueryInterface( animation,
            &IID_IVector3KeyFrameAnimation, (void **)&vector ))) return hr;
    if (vector->lpVtbl != &vector3_key_frame_animation_vtbl)
    {
        IVector3KeyFrameAnimation_Release( vector );
        return E_NOINTERFACE;
    }
    impl = impl_from_IVector3KeyFrameAnimation( vector );
    if (!impl->has_final_value) hr = E_BOUNDS;
    else *value = impl->final_vector;
    IVector3KeyFrameAnimation_Release( vector );
    return hr;
}

static inline struct scalar_key_frame_animation *scalar_animation_from_ICompositionAnimationBase(
        ICompositionAnimationBase *iface )
{
    return CONTAINING_RECORD( iface, struct scalar_key_frame_animation,
                              ICompositionAnimationBase_iface );
}

static inline struct scalar_key_frame_animation *scalar_animation_from_IKeyFrameAnimation(
        struct key_frame_animation_iface *iface )
{
    return CONTAINING_RECORD( iface, struct scalar_key_frame_animation, IKeyFrameAnimation_iface );
}

static inline struct scalar_key_frame_animation *scalar_animation_from_ICompositionObject(
        ICompositionObject *iface )
{
    return CONTAINING_RECORD( iface, struct scalar_key_frame_animation, ICompositionObject_iface );
}

static HRESULT scalar_key_frame_animation_query_interface( struct scalar_key_frame_animation *impl,
        REFIID iid, void **out )
{
    if (!out) return E_POINTER;
    *out = NULL;
    if (IsEqualGUID( iid, &IID_IUnknown ) || IsEqualGUID( iid, &IID_IInspectable ) ||
        IsEqualGUID( iid, &IID_IAgileObject ))
        *out = impl->is_vector3 ? (void *)&impl->IVector3KeyFrameAnimation_iface :
               impl->is_boolean ? (void *)&impl->IBooleanKeyFrameAnimation_iface :
                                 (void *)&impl->IScalarKeyFrameAnimation_iface;
    else if (!impl->is_boolean && !impl->is_vector3 && IsEqualGUID( iid, &IID_IScalarKeyFrameAnimation ))
        *out = &impl->IScalarKeyFrameAnimation_iface;
    else if (impl->is_vector3 && IsEqualGUID( iid, &IID_IVector3KeyFrameAnimation ))
        *out = &impl->IVector3KeyFrameAnimation_iface;
    else if (impl->is_boolean && IsEqualGUID( iid, &boolean_key_frame_animation_iid ))
        *out = &impl->IBooleanKeyFrameAnimation_iface;
    else if (IsEqualGUID( iid, &key_frame_animation_iid ))
        *out = &impl->IKeyFrameAnimation_iface;
    else if (IsEqualGUID(iid,&animation2_iid))
        *out = &impl->animation2.iface;
    else if (IsEqualGUID( iid, &IID_ICompositionAnimation ))
        *out = &impl->ICompositionAnimation_iface;
    else if (IsEqualGUID( iid, &IID_ICompositionAnimationBase ))
        *out = &impl->ICompositionAnimationBase_iface;
    else if (IsEqualGUID( iid, &IID_ICompositionObject ))
        *out = &impl->ICompositionObject_iface;
    else
        return E_NOINTERFACE;
    InterlockedIncrement( &impl->ref );
    return S_OK;
}

static ULONG scalar_key_frame_animation_add_ref( struct scalar_key_frame_animation *impl )
{
    return InterlockedIncrement( &impl->ref );
}

static ULONG scalar_key_frame_animation_release( struct scalar_key_frame_animation *impl )
{
    ULONG ref = InterlockedDecrement( &impl->ref );

    if (!ref)
    {
        animation2_clear_parameters(&impl->animation2,NULL);
        WindowsDeleteString(impl->animation2.target);
        ICompositor_Release( impl->compositor );
        free( impl );
    }
    return ref;
}

static HRESULT scalar_key_frame_animation_get_iids( struct scalar_key_frame_animation *impl,
        ULONG *count, IID **iids )
{
    if (!count || !iids) return E_POINTER;
    if (!(*iids = CoTaskMemAlloc( 6 * sizeof(**iids) ))) return E_OUTOFMEMORY;
    (*iids)[0] = impl->is_vector3 ? IID_IVector3KeyFrameAnimation :
                impl->is_boolean ? boolean_key_frame_animation_iid :
                                    IID_IScalarKeyFrameAnimation;
    (*iids)[1] = key_frame_animation_iid;
    (*iids)[2] = IID_ICompositionAnimation;
    (*iids)[3] = IID_ICompositionAnimationBase;
    (*iids)[4] = IID_ICompositionObject;
    (*iids)[5] = animation2_iid;
    *count = 6;
    return S_OK;
}

static HRESULT scalar_key_frame_animation_get_class_name( struct scalar_key_frame_animation *impl,
        HSTRING *name )
{
    static const WCHAR boolean_name[] = L"Windows.UI.Composition.BooleanKeyFrameAnimation";
    static const WCHAR vector_name[] = L"Windows.UI.Composition.Vector3KeyFrameAnimation";

    if (!name) return E_POINTER;
    if (impl->is_vector3)
        return WindowsCreateString( vector_name, ARRAY_SIZE(vector_name) - 1, name );
    if (impl->is_boolean)
        return WindowsCreateString( boolean_name, ARRAY_SIZE(boolean_name) - 1, name );
    return WindowsCreateString( RuntimeClass_Windows_UI_Composition_ScalarKeyFrameAnimation,
                                ARRAY_SIZE(RuntimeClass_Windows_UI_Composition_ScalarKeyFrameAnimation) - 1,
                                name );
}

static HRESULT scalar_key_frame_animation_get_trust_level( TrustLevel *level )
{
    if (!level) return E_POINTER;
    *level = BaseTrust;
    return S_OK;
}

static HRESULT WINAPI scalar_key_frame_animation_QueryInterface( IScalarKeyFrameAnimation *iface,
        REFIID iid, void **out )
{
    TRACE( "iface %p, iid %s, out %p.\n", iface, debugstr_guid( iid ), out );
    return scalar_key_frame_animation_query_interface(
            impl_from_IScalarKeyFrameAnimation( iface ), iid, out );
}

static ULONG WINAPI scalar_key_frame_animation_AddRef( IScalarKeyFrameAnimation *iface )
{
    return scalar_key_frame_animation_add_ref( impl_from_IScalarKeyFrameAnimation( iface ) );
}

static ULONG WINAPI scalar_key_frame_animation_Release( IScalarKeyFrameAnimation *iface )
{
    return scalar_key_frame_animation_release( impl_from_IScalarKeyFrameAnimation( iface ) );
}

static HRESULT WINAPI scalar_key_frame_animation_GetIids( IScalarKeyFrameAnimation *iface,
        ULONG *count, IID **iids )
{
    return scalar_key_frame_animation_get_iids( impl_from_IScalarKeyFrameAnimation( iface ),
            count, iids );
}

static HRESULT WINAPI scalar_key_frame_animation_GetRuntimeClassName(
        IScalarKeyFrameAnimation *iface, HSTRING *name )
{
    return scalar_key_frame_animation_get_class_name( impl_from_IScalarKeyFrameAnimation( iface ),
            name );
}

static HRESULT WINAPI scalar_key_frame_animation_GetTrustLevel( IScalarKeyFrameAnimation *iface,
        TrustLevel *level )
{
    return scalar_key_frame_animation_get_trust_level( level );
}

static HRESULT WINAPI scalar_key_frame_animation_InsertKeyFrame( IScalarKeyFrameAnimation *iface,
        FLOAT key, FLOAT value )
{
    struct scalar_key_frame_animation *impl = impl_from_IScalarKeyFrameAnimation( iface );

    TRACE( "iface %p, key %f, value %f.\n", iface, key, value );
    if (key < 0.0f || key > 1.0f) return E_INVALIDARG;
    if (!impl->has_final_value || key >= impl->final_key)
    {
        impl->final_key = key;
        impl->final_value = value;
        impl->has_final_value = TRUE;
    }
    ++impl->key_frame_count;
    return S_OK;
}

static HRESULT WINAPI scalar_key_frame_animation_InsertKeyFrameWithEasingFunction(
        IScalarKeyFrameAnimation *iface, FLOAT key, FLOAT value,
        ICompositionEasingFunction *function )
{
    struct scalar_key_frame_animation *impl = impl_from_IScalarKeyFrameAnimation( iface );

    TRACE( "iface %p, key %f, value %f, function %p.\n", iface, key, value, function );
    if (key < 0.0f || key > 1.0f) return E_INVALIDARG;
    if (!impl->has_final_value || key >= impl->final_key)
    {
        impl->final_key = key;
        impl->final_value = value;
        impl->has_final_value = TRUE;
    }
    ++impl->key_frame_count;
    return S_OK;
}

static const IScalarKeyFrameAnimationVtbl scalar_key_frame_animation_vtbl =
{
    scalar_key_frame_animation_QueryInterface,
    scalar_key_frame_animation_AddRef,
    scalar_key_frame_animation_Release,
    scalar_key_frame_animation_GetIids,
    scalar_key_frame_animation_GetRuntimeClassName,
    scalar_key_frame_animation_GetTrustLevel,
    scalar_key_frame_animation_InsertKeyFrame,
    scalar_key_frame_animation_InsertKeyFrameWithEasingFunction,
};

static HRESULT WINAPI vector3_key_frame_animation_QueryInterface( IVector3KeyFrameAnimation *iface,
        REFIID iid, void **out )
{
    return scalar_key_frame_animation_query_interface( impl_from_IVector3KeyFrameAnimation( iface ),
                                                       iid, out );
}

static ULONG WINAPI vector3_key_frame_animation_AddRef( IVector3KeyFrameAnimation *iface )
{
    return scalar_key_frame_animation_add_ref( impl_from_IVector3KeyFrameAnimation( iface ) );
}

static ULONG WINAPI vector3_key_frame_animation_Release( IVector3KeyFrameAnimation *iface )
{
    return scalar_key_frame_animation_release( impl_from_IVector3KeyFrameAnimation( iface ) );
}

static HRESULT WINAPI vector3_key_frame_animation_GetIids( IVector3KeyFrameAnimation *iface,
        ULONG *count, IID **iids )
{
    return scalar_key_frame_animation_get_iids( impl_from_IVector3KeyFrameAnimation( iface ), count, iids );
}

static HRESULT WINAPI vector3_key_frame_animation_GetRuntimeClassName( IVector3KeyFrameAnimation *iface,
        HSTRING *name )
{
    return scalar_key_frame_animation_get_class_name( impl_from_IVector3KeyFrameAnimation( iface ), name );
}

static HRESULT WINAPI vector3_key_frame_animation_GetTrustLevel( IVector3KeyFrameAnimation *iface,
        TrustLevel *level )
{
    return scalar_key_frame_animation_get_trust_level( level );
}

static HRESULT WINAPI vector3_key_frame_animation_InsertKeyFrame( IVector3KeyFrameAnimation *iface,
        FLOAT key, struct Vector3 value )
{
    struct scalar_key_frame_animation *impl = impl_from_IVector3KeyFrameAnimation( iface );

    TRACE( "iface %p, key %f, value {%f, %f, %f}.\n", iface, key, value.X, value.Y, value.Z );
    if (!(key >= 0.0f && key <= 1.0f)) return E_INVALIDARG;
    ++impl->key_frame_count;
    if (!impl->has_final_value || key >= impl->final_key)
    {
        impl->final_key = key;
        impl->final_vector = value;
        impl->has_final_value = TRUE;
    }
    return S_OK;
}

static HRESULT WINAPI vector3_key_frame_animation_InsertKeyFrameWithEasingFunction(
        IVector3KeyFrameAnimation *iface, FLOAT key, struct Vector3 value,
        ICompositionEasingFunction *function )
{
    return vector3_key_frame_animation_InsertKeyFrame( iface, key, value );
}

static const IVector3KeyFrameAnimationVtbl vector3_key_frame_animation_vtbl =
{
    vector3_key_frame_animation_QueryInterface,
    vector3_key_frame_animation_AddRef,
    vector3_key_frame_animation_Release,
    vector3_key_frame_animation_GetIids,
    vector3_key_frame_animation_GetRuntimeClassName,
    vector3_key_frame_animation_GetTrustLevel,
    vector3_key_frame_animation_InsertKeyFrame,
    vector3_key_frame_animation_InsertKeyFrameWithEasingFunction,
};

static HRESULT WINAPI boolean_key_frame_animation_QueryInterface(
        struct boolean_key_frame_animation_iface *iface, REFIID iid, void **out )
{
    return scalar_key_frame_animation_query_interface(
            scalar_animation_from_IBooleanKeyFrameAnimation( iface ), iid, out );
}

static ULONG WINAPI boolean_key_frame_animation_AddRef(
        struct boolean_key_frame_animation_iface *iface )
{
    return scalar_key_frame_animation_add_ref(
            scalar_animation_from_IBooleanKeyFrameAnimation( iface ) );
}

static ULONG WINAPI boolean_key_frame_animation_Release(
        struct boolean_key_frame_animation_iface *iface )
{
    return scalar_key_frame_animation_release(
            scalar_animation_from_IBooleanKeyFrameAnimation( iface ) );
}

static HRESULT WINAPI boolean_key_frame_animation_GetIids(
        struct boolean_key_frame_animation_iface *iface, ULONG *count, IID **iids )
{
    return scalar_key_frame_animation_get_iids(
            scalar_animation_from_IBooleanKeyFrameAnimation( iface ), count, iids );
}

static HRESULT WINAPI boolean_key_frame_animation_GetRuntimeClassName(
        struct boolean_key_frame_animation_iface *iface, HSTRING *name )
{
    return scalar_key_frame_animation_get_class_name(
            scalar_animation_from_IBooleanKeyFrameAnimation( iface ), name );
}

static HRESULT WINAPI boolean_key_frame_animation_GetTrustLevel(
        struct boolean_key_frame_animation_iface *iface, TrustLevel *level )
{
    return scalar_key_frame_animation_get_trust_level( level );
}

static HRESULT WINAPI boolean_key_frame_animation_InsertKeyFrame(
        struct boolean_key_frame_animation_iface *iface, FLOAT key, boolean value )
{
    struct scalar_key_frame_animation *impl =
            scalar_animation_from_IBooleanKeyFrameAnimation( iface );

    TRACE( "iface %p, key %f, value %u.\n", iface, key, value );
    if (key < 0.0f || key > 1.0f) return E_INVALIDARG;
    if (!impl->has_final_value || key >= impl->final_key)
    {
        impl->final_key = key;
        impl->final_value = value;
        impl->has_final_value = TRUE;
    }
    ++impl->key_frame_count;
    return S_OK;
}

static const struct boolean_key_frame_animation_vtbl boolean_key_frame_animation_vtbl =
{
    boolean_key_frame_animation_QueryInterface,
    boolean_key_frame_animation_AddRef,
    boolean_key_frame_animation_Release,
    boolean_key_frame_animation_GetIids,
    boolean_key_frame_animation_GetRuntimeClassName,
    boolean_key_frame_animation_GetTrustLevel,
    boolean_key_frame_animation_InsertKeyFrame,
};

static HRESULT WINAPI key_frame_animation_QueryInterface( struct key_frame_animation_iface *iface,
        REFIID iid, void **out )
{
    return scalar_key_frame_animation_query_interface(
            scalar_animation_from_IKeyFrameAnimation( iface ), iid, out );
}

static ULONG WINAPI key_frame_animation_AddRef( struct key_frame_animation_iface *iface )
{
    return scalar_key_frame_animation_add_ref( scalar_animation_from_IKeyFrameAnimation( iface ) );
}

static ULONG WINAPI key_frame_animation_Release( struct key_frame_animation_iface *iface )
{
    return scalar_key_frame_animation_release( scalar_animation_from_IKeyFrameAnimation( iface ) );
}

static HRESULT WINAPI key_frame_animation_GetIids( struct key_frame_animation_iface *iface,
        ULONG *count, IID **iids )
{
    return scalar_key_frame_animation_get_iids( scalar_animation_from_IKeyFrameAnimation( iface ),
            count, iids );
}

static HRESULT WINAPI key_frame_animation_GetRuntimeClassName( struct key_frame_animation_iface *iface,
        HSTRING *name )
{
    return scalar_key_frame_animation_get_class_name(
            scalar_animation_from_IKeyFrameAnimation( iface ), name );
}

static HRESULT WINAPI key_frame_animation_GetTrustLevel( struct key_frame_animation_iface *iface,
        TrustLevel *level )
{
    return scalar_key_frame_animation_get_trust_level( level );
}

#define KEY_FRAME_ANIMATION_PROPERTY(name, type, field) \
    static HRESULT WINAPI key_frame_animation_get_##name( struct key_frame_animation_iface *iface, \
            type *value ) \
    { \
        if (!value) return E_POINTER; \
        *value = scalar_animation_from_IKeyFrameAnimation( iface )->field; \
        return S_OK; \
    } \
    static HRESULT WINAPI key_frame_animation_put_##name( struct key_frame_animation_iface *iface, \
            type value ) \
    { \
        scalar_animation_from_IKeyFrameAnimation( iface )->field = value; \
        return S_OK; \
    }

KEY_FRAME_ANIMATION_PROPERTY( DelayTime, TimeSpan, delay_time )
KEY_FRAME_ANIMATION_PROPERTY( Duration, TimeSpan, duration )
KEY_FRAME_ANIMATION_PROPERTY( IterationBehavior, INT32, iteration_behavior )
KEY_FRAME_ANIMATION_PROPERTY( IterationCount, INT32, iteration_count )
KEY_FRAME_ANIMATION_PROPERTY( StopBehavior, INT32, stop_behavior )

static HRESULT WINAPI key_frame_animation_get_KeyFrameCount( struct key_frame_animation_iface *iface,
        INT32 *value )
{
    if (!value) return E_POINTER;
    *value = scalar_animation_from_IKeyFrameAnimation( iface )->key_frame_count;
    return S_OK;
}

static HRESULT WINAPI key_frame_animation_InsertExpressionKeyFrame(
        struct key_frame_animation_iface *iface, FLOAT key, HSTRING expression )
{
    struct scalar_key_frame_animation *impl = scalar_animation_from_IKeyFrameAnimation( iface );

    if (key < 0.0f || key > 1.0f || !expression) return E_INVALIDARG;
    ++impl->key_frame_count;
    return S_OK;
}

static HRESULT WINAPI key_frame_animation_InsertExpressionKeyFrameWithEasingFunction(
        struct key_frame_animation_iface *iface, FLOAT key, HSTRING expression,
        ICompositionEasingFunction *function )
{
    struct scalar_key_frame_animation *impl = scalar_animation_from_IKeyFrameAnimation( iface );

    if (key < 0.0f || key > 1.0f || !expression) return E_INVALIDARG;
    ++impl->key_frame_count;
    return S_OK;
}

static const struct key_frame_animation_vtbl key_frame_animation_vtbl =
{
    key_frame_animation_QueryInterface,
    key_frame_animation_AddRef,
    key_frame_animation_Release,
    key_frame_animation_GetIids,
    key_frame_animation_GetRuntimeClassName,
    key_frame_animation_GetTrustLevel,
    key_frame_animation_get_DelayTime,
    key_frame_animation_put_DelayTime,
    key_frame_animation_get_Duration,
    key_frame_animation_put_Duration,
    key_frame_animation_get_IterationBehavior,
    key_frame_animation_put_IterationBehavior,
    key_frame_animation_get_IterationCount,
    key_frame_animation_put_IterationCount,
    key_frame_animation_get_KeyFrameCount,
    key_frame_animation_get_StopBehavior,
    key_frame_animation_put_StopBehavior,
    key_frame_animation_InsertExpressionKeyFrame,
    key_frame_animation_InsertExpressionKeyFrameWithEasingFunction,
};

static HRESULT WINAPI scalar_composition_animation_QueryInterface( ICompositionAnimation *iface,
        REFIID iid, void **out )
{
    return scalar_key_frame_animation_query_interface(
            scalar_animation_from_ICompositionAnimation( iface ), iid, out );
}

static ULONG WINAPI scalar_composition_animation_AddRef( ICompositionAnimation *iface )
{
    return scalar_key_frame_animation_add_ref( scalar_animation_from_ICompositionAnimation( iface ) );
}

static ULONG WINAPI scalar_composition_animation_Release( ICompositionAnimation *iface )
{
    return scalar_key_frame_animation_release( scalar_animation_from_ICompositionAnimation( iface ) );
}

static HRESULT WINAPI scalar_composition_animation_GetIids( ICompositionAnimation *iface,
        ULONG *count, IID **iids )
{
    return scalar_key_frame_animation_get_iids( scalar_animation_from_ICompositionAnimation( iface ),
            count, iids );
}

static HRESULT WINAPI scalar_composition_animation_GetRuntimeClassName( ICompositionAnimation *iface,
        HSTRING *name )
{
    return scalar_key_frame_animation_get_class_name(
            scalar_animation_from_ICompositionAnimation( iface ), name );
}

static HRESULT WINAPI scalar_composition_animation_GetTrustLevel( ICompositionAnimation *iface,
        TrustLevel *level )
{
    return scalar_key_frame_animation_get_trust_level( level );
}

static HRESULT WINAPI scalar_composition_animation_ClearAllParameters( ICompositionAnimation *iface )
{
    animation2_clear_parameters(&scalar_animation_from_ICompositionAnimation(iface)->animation2,NULL);
    return S_OK;
}

static HRESULT WINAPI scalar_composition_animation_ClearParameter( ICompositionAnimation *iface,
        HSTRING key )
{
    if (!key) return E_INVALIDARG;
    animation2_clear_parameters(&scalar_animation_from_ICompositionAnimation(iface)->animation2,key);
    return S_OK;
}

#define SCALAR_COMPOSITION_ANIMATION_PARAMETER(name, type) \
    static HRESULT WINAPI scalar_composition_animation_Set##name##Parameter( \
            ICompositionAnimation *iface, HSTRING key, type value ) \
    { \
        return key ? S_OK : E_INVALIDARG; \
    }

SCALAR_COMPOSITION_ANIMATION_PARAMETER( Color, struct Color )
SCALAR_COMPOSITION_ANIMATION_PARAMETER( Matrix3x2, struct Matrix3x2 )
SCALAR_COMPOSITION_ANIMATION_PARAMETER( Matrix4x4, struct Matrix4x4 )
SCALAR_COMPOSITION_ANIMATION_PARAMETER( Quaternion, struct Quaternion )
SCALAR_COMPOSITION_ANIMATION_PARAMETER( Scalar, FLOAT )
SCALAR_COMPOSITION_ANIMATION_PARAMETER( Vector2, struct Vector2 )
SCALAR_COMPOSITION_ANIMATION_PARAMETER( Vector3, struct Vector3 )
SCALAR_COMPOSITION_ANIMATION_PARAMETER( Vector4, struct Vector4 )

static HRESULT WINAPI scalar_composition_animation_SetReferenceParameter( ICompositionAnimation *iface,
        HSTRING key, ICompositionObject *object )
{
    return key && object ? S_OK : E_INVALIDARG;
}

static const ICompositionAnimationVtbl scalar_composition_animation_vtbl =
{
    scalar_composition_animation_QueryInterface,
    scalar_composition_animation_AddRef,
    scalar_composition_animation_Release,
    scalar_composition_animation_GetIids,
    scalar_composition_animation_GetRuntimeClassName,
    scalar_composition_animation_GetTrustLevel,
    scalar_composition_animation_ClearAllParameters,
    scalar_composition_animation_ClearParameter,
    scalar_composition_animation_SetColorParameter,
    scalar_composition_animation_SetMatrix3x2Parameter,
    scalar_composition_animation_SetMatrix4x4Parameter,
    scalar_composition_animation_SetQuaternionParameter,
    scalar_composition_animation_SetReferenceParameter,
    scalar_composition_animation_SetScalarParameter,
    scalar_composition_animation_SetVector2Parameter,
    scalar_composition_animation_SetVector3Parameter,
    scalar_composition_animation_SetVector4Parameter,
};

static HRESULT WINAPI scalar_composition_animation_base_QueryInterface(
        ICompositionAnimationBase *iface, REFIID iid, void **out )
{
    return scalar_key_frame_animation_query_interface(
            scalar_animation_from_ICompositionAnimationBase( iface ), iid, out );
}

static ULONG WINAPI scalar_composition_animation_base_AddRef( ICompositionAnimationBase *iface )
{
    return scalar_key_frame_animation_add_ref(
            scalar_animation_from_ICompositionAnimationBase( iface ) );
}

static ULONG WINAPI scalar_composition_animation_base_Release( ICompositionAnimationBase *iface )
{
    return scalar_key_frame_animation_release(
            scalar_animation_from_ICompositionAnimationBase( iface ) );
}

static HRESULT WINAPI scalar_composition_animation_base_GetIids( ICompositionAnimationBase *iface,
        ULONG *count, IID **iids )
{
    return scalar_key_frame_animation_get_iids(
            scalar_animation_from_ICompositionAnimationBase( iface ), count, iids );
}

static HRESULT WINAPI scalar_composition_animation_base_GetRuntimeClassName(
        ICompositionAnimationBase *iface, HSTRING *name )
{
    return scalar_key_frame_animation_get_class_name(
            scalar_animation_from_ICompositionAnimationBase( iface ), name );
}

static HRESULT WINAPI scalar_composition_animation_base_GetTrustLevel(
        ICompositionAnimationBase *iface, TrustLevel *level )
{
    return scalar_key_frame_animation_get_trust_level( level );
}

static const ICompositionAnimationBaseVtbl scalar_composition_animation_base_vtbl =
{
    scalar_composition_animation_base_QueryInterface,
    scalar_composition_animation_base_AddRef,
    scalar_composition_animation_base_Release,
    scalar_composition_animation_base_GetIids,
    scalar_composition_animation_base_GetRuntimeClassName,
    scalar_composition_animation_base_GetTrustLevel,
};

static HRESULT WINAPI scalar_animation_composition_object_QueryInterface( ICompositionObject *iface,
        REFIID iid, void **out )
{
    return scalar_key_frame_animation_query_interface(
            scalar_animation_from_ICompositionObject( iface ), iid, out );
}

static ULONG WINAPI scalar_animation_composition_object_AddRef( ICompositionObject *iface )
{
    return scalar_key_frame_animation_add_ref( scalar_animation_from_ICompositionObject( iface ) );
}

static ULONG WINAPI scalar_animation_composition_object_Release( ICompositionObject *iface )
{
    return scalar_key_frame_animation_release( scalar_animation_from_ICompositionObject( iface ) );
}

static HRESULT WINAPI scalar_animation_composition_object_GetIids( ICompositionObject *iface,
        ULONG *count, IID **iids )
{
    return scalar_key_frame_animation_get_iids( scalar_animation_from_ICompositionObject( iface ),
            count, iids );
}

static HRESULT WINAPI scalar_animation_composition_object_GetRuntimeClassName(
        ICompositionObject *iface, HSTRING *name )
{
    return scalar_key_frame_animation_get_class_name(
            scalar_animation_from_ICompositionObject( iface ), name );
}

static HRESULT WINAPI scalar_animation_composition_object_GetTrustLevel( ICompositionObject *iface,
        TrustLevel *level )
{
    return scalar_key_frame_animation_get_trust_level( level );
}

static HRESULT WINAPI scalar_animation_composition_object_get_Compositor( ICompositionObject *iface,
        ICompositor **value )
{
    struct scalar_key_frame_animation *impl = scalar_animation_from_ICompositionObject( iface );

    if (!value) return E_POINTER;
    ICompositor_AddRef( impl->compositor );
    *value = impl->compositor;
    return S_OK;
}

static HRESULT WINAPI scalar_animation_composition_object_get_Dispatcher( ICompositionObject *iface,
        ICoreDispatcher **value )
{
    if (!value) return E_POINTER;
    *value = NULL;
    return S_OK;
}

static HRESULT WINAPI scalar_animation_composition_object_get_Properties( ICompositionObject *iface,
        ICompositionPropertySet **value )
{
    if (!value) return E_POINTER;
    *value = NULL;
    return S_OK;
}

static HRESULT WINAPI scalar_animation_composition_object_StartAnimation( ICompositionObject *iface,
        HSTRING name, ICompositionAnimation *animation )
{
    TRACE( "iface %p, name %s, animation %p.\n", iface, debugstr_hstring( name ), animation );
    return S_OK;
}

static HRESULT WINAPI scalar_animation_composition_object_StopAnimation( ICompositionObject *iface,
        HSTRING name )
{
    TRACE( "iface %p, name %s.\n", iface, debugstr_hstring( name ) );
    return S_OK;
}

static const ICompositionObjectVtbl scalar_animation_composition_object_vtbl =
{
    scalar_animation_composition_object_QueryInterface,
    scalar_animation_composition_object_AddRef,
    scalar_animation_composition_object_Release,
    scalar_animation_composition_object_GetIids,
    scalar_animation_composition_object_GetRuntimeClassName,
    scalar_animation_composition_object_GetTrustLevel,
    scalar_animation_composition_object_get_Compositor,
    scalar_animation_composition_object_get_Dispatcher,
    scalar_animation_composition_object_get_Properties,
    scalar_animation_composition_object_StartAnimation,
    scalar_animation_composition_object_StopAnimation,
};

static HRESULT scalar_key_frame_animation_create( ICompositor *compositor,
        IScalarKeyFrameAnimation **result )
{
    struct scalar_key_frame_animation *impl;

    if (!result) return E_POINTER;
    *result = NULL;
    if (!(impl = calloc( 1, sizeof(*impl) ))) return E_OUTOFMEMORY;
    impl->IScalarKeyFrameAnimation_iface.lpVtbl = &scalar_key_frame_animation_vtbl;
    impl->IKeyFrameAnimation_iface.lpVtbl = &key_frame_animation_vtbl;
    impl->ICompositionAnimation_iface.lpVtbl = &scalar_composition_animation_vtbl;
    impl->animation2.iface.lpVtbl = &animation2_vtbl;
    impl->animation2.animation = &impl->ICompositionAnimation_iface;
    impl->ICompositionAnimationBase_iface.lpVtbl = &scalar_composition_animation_base_vtbl;
    impl->ICompositionObject_iface.lpVtbl = &scalar_animation_composition_object_vtbl;
    impl->compositor = compositor;
    impl->duration.Duration = 10000000;
    impl->iteration_count = 1;
    impl->ref = 1;
    ICompositor_AddRef( compositor );
    *result = &impl->IScalarKeyFrameAnimation_iface;
    return S_OK;
}

static HRESULT vector3_key_frame_animation_create( ICompositor *compositor,
        IVector3KeyFrameAnimation **result )
{
    struct scalar_key_frame_animation *impl;
    IScalarKeyFrameAnimation *scalar;
    HRESULT hr;

    if (!result) return E_POINTER;
    *result = NULL;
    if (FAILED(hr = scalar_key_frame_animation_create( compositor, &scalar ))) return hr;
    impl = impl_from_IScalarKeyFrameAnimation( scalar );
    impl->IVector3KeyFrameAnimation_iface.lpVtbl = &vector3_key_frame_animation_vtbl;
    impl->is_vector3 = TRUE;
    *result = &impl->IVector3KeyFrameAnimation_iface;
    return S_OK;
}

static HRESULT boolean_key_frame_animation_create( ICompositor *compositor, IInspectable **result )
{
    struct scalar_key_frame_animation *impl;

    if (!result) return E_POINTER;
    *result = NULL;
    if (!(impl = calloc( 1, sizeof(*impl) ))) return E_OUTOFMEMORY;
    impl->IBooleanKeyFrameAnimation_iface.lpVtbl = &boolean_key_frame_animation_vtbl;
    impl->IKeyFrameAnimation_iface.lpVtbl = &key_frame_animation_vtbl;
    impl->ICompositionAnimation_iface.lpVtbl = &scalar_composition_animation_vtbl;
    impl->animation2.iface.lpVtbl = &animation2_vtbl;
    impl->animation2.animation = &impl->ICompositionAnimation_iface;
    impl->ICompositionAnimationBase_iface.lpVtbl = &scalar_composition_animation_base_vtbl;
    impl->ICompositionObject_iface.lpVtbl = &scalar_animation_composition_object_vtbl;
    impl->compositor = compositor;
    impl->duration.Duration = 10000000;
    impl->iteration_count = 1;
    impl->is_boolean = TRUE;
    impl->ref = 1;
    ICompositor_AddRef( compositor );
    *result = (IInspectable *)&impl->IBooleanKeyFrameAnimation_iface;
    return S_OK;
}

struct linear_easing_function
{
    ILinearEasingFunction ILinearEasingFunction_iface;
    ICompositionEasingFunction ICompositionEasingFunction_iface;
    LONG ref;
};

static inline struct linear_easing_function *impl_from_ILinearEasingFunction(
        ILinearEasingFunction *iface )
{
    return CONTAINING_RECORD( iface, struct linear_easing_function, ILinearEasingFunction_iface );
}

static inline struct linear_easing_function *linear_easing_from_ICompositionEasingFunction(
        ICompositionEasingFunction *iface )
{
    return CONTAINING_RECORD( iface, struct linear_easing_function, ICompositionEasingFunction_iface );
}

static HRESULT linear_easing_function_query_interface( struct linear_easing_function *impl,
        REFIID iid, void **out )
{
    if (!out) return E_POINTER;
    *out = NULL;
    if (IsEqualGUID( iid, &IID_IUnknown ) || IsEqualGUID( iid, &IID_IInspectable ) ||
        IsEqualGUID( iid, &IID_IAgileObject ) || IsEqualGUID( iid, &IID_ILinearEasingFunction ))
        *out = &impl->ILinearEasingFunction_iface;
    else if (IsEqualGUID( iid, &IID_ICompositionEasingFunction ))
        *out = &impl->ICompositionEasingFunction_iface;
    else
        return E_NOINTERFACE;
    InterlockedIncrement( &impl->ref );
    return S_OK;
}

static ULONG linear_easing_function_release( struct linear_easing_function *impl )
{
    ULONG ref = InterlockedDecrement( &impl->ref );

    if (!ref) free( impl );
    return ref;
}

static HRESULT linear_easing_function_get_iids( ULONG *count, IID **iids )
{
    if (!count || !iids) return E_POINTER;
    if (!(*iids = CoTaskMemAlloc( 2 * sizeof(**iids) ))) return E_OUTOFMEMORY;
    (*iids)[0] = IID_ILinearEasingFunction;
    (*iids)[1] = IID_ICompositionEasingFunction;
    *count = 2;
    return S_OK;
}

static HRESULT linear_easing_function_get_class_name( HSTRING *name )
{
    if (!name) return E_POINTER;
    return WindowsCreateString( RuntimeClass_Windows_UI_Composition_LinearEasingFunction,
                                ARRAY_SIZE(RuntimeClass_Windows_UI_Composition_LinearEasingFunction) - 1,
                                name );
}

static HRESULT WINAPI linear_easing_function_QueryInterface( ILinearEasingFunction *iface,
        REFIID iid, void **out )
{
    TRACE( "iface %p, iid %s, out %p.\n", iface, debugstr_guid( iid ), out );
    return linear_easing_function_query_interface( impl_from_ILinearEasingFunction( iface ), iid, out );
}

static ULONG WINAPI linear_easing_function_AddRef( ILinearEasingFunction *iface )
{
    return InterlockedIncrement( &impl_from_ILinearEasingFunction( iface )->ref );
}

static ULONG WINAPI linear_easing_function_Release( ILinearEasingFunction *iface )
{
    return linear_easing_function_release( impl_from_ILinearEasingFunction( iface ) );
}

static HRESULT WINAPI linear_easing_function_GetIids( ILinearEasingFunction *iface,
        ULONG *count, IID **iids )
{
    return linear_easing_function_get_iids( count, iids );
}

static HRESULT WINAPI linear_easing_function_GetRuntimeClassName( ILinearEasingFunction *iface,
        HSTRING *name )
{
    return linear_easing_function_get_class_name( name );
}

static HRESULT WINAPI linear_easing_function_GetTrustLevel( ILinearEasingFunction *iface,
        TrustLevel *level )
{
    if (!level) return E_POINTER;
    *level = BaseTrust;
    return S_OK;
}

static const ILinearEasingFunctionVtbl linear_easing_function_vtbl =
{
    linear_easing_function_QueryInterface,
    linear_easing_function_AddRef,
    linear_easing_function_Release,
    linear_easing_function_GetIids,
    linear_easing_function_GetRuntimeClassName,
    linear_easing_function_GetTrustLevel,
};

static HRESULT WINAPI composition_easing_function_QueryInterface( ICompositionEasingFunction *iface,
        REFIID iid, void **out )
{
    return linear_easing_function_query_interface(
            linear_easing_from_ICompositionEasingFunction( iface ), iid, out );
}

static ULONG WINAPI composition_easing_function_AddRef( ICompositionEasingFunction *iface )
{
    return InterlockedIncrement( &linear_easing_from_ICompositionEasingFunction( iface )->ref );
}

static ULONG WINAPI composition_easing_function_Release( ICompositionEasingFunction *iface )
{
    return linear_easing_function_release( linear_easing_from_ICompositionEasingFunction( iface ) );
}

static HRESULT WINAPI composition_easing_function_GetIids( ICompositionEasingFunction *iface,
        ULONG *count, IID **iids )
{
    return linear_easing_function_get_iids( count, iids );
}

static HRESULT WINAPI composition_easing_function_GetRuntimeClassName(
        ICompositionEasingFunction *iface, HSTRING *name )
{
    return linear_easing_function_get_class_name( name );
}

static HRESULT WINAPI composition_easing_function_GetTrustLevel( ICompositionEasingFunction *iface,
        TrustLevel *level )
{
    if (!level) return E_POINTER;
    *level = BaseTrust;
    return S_OK;
}

static const ICompositionEasingFunctionVtbl composition_easing_function_vtbl =
{
    composition_easing_function_QueryInterface,
    composition_easing_function_AddRef,
    composition_easing_function_Release,
    composition_easing_function_GetIids,
    composition_easing_function_GetRuntimeClassName,
    composition_easing_function_GetTrustLevel,
};

static HRESULT linear_easing_function_create( ILinearEasingFunction **result )
{
    struct linear_easing_function *impl;

    if (!result) return E_POINTER;
    *result = NULL;
    if (!(impl = calloc( 1, sizeof(*impl) ))) return E_OUTOFMEMORY;
    impl->ILinearEasingFunction_iface.lpVtbl = &linear_easing_function_vtbl;
    impl->ICompositionEasingFunction_iface.lpVtbl = &composition_easing_function_vtbl;
    impl->ref = 1;
    *result = &impl->ILinearEasingFunction_iface;
    return S_OK;
}

struct cubic_bezier_easing_function
{
    ICubicBezierEasingFunction ICubicBezierEasingFunction_iface;
    ICompositionEasingFunction ICompositionEasingFunction_iface;
    struct Vector2 point1;
    struct Vector2 point2;
    LONG ref;
};

static inline struct cubic_bezier_easing_function *impl_from_ICubicBezierEasingFunction(
        ICubicBezierEasingFunction *iface )
{
    return CONTAINING_RECORD( iface, struct cubic_bezier_easing_function,
                              ICubicBezierEasingFunction_iface );
}

static inline struct cubic_bezier_easing_function *cubic_from_ICompositionEasingFunction(
        ICompositionEasingFunction *iface )
{
    return CONTAINING_RECORD( iface, struct cubic_bezier_easing_function,
                              ICompositionEasingFunction_iface );
}

static HRESULT cubic_bezier_easing_function_query_interface(
        struct cubic_bezier_easing_function *impl, REFIID iid, void **out )
{
    if (!out) return E_POINTER;
    *out = NULL;
    if (IsEqualGUID( iid, &IID_IUnknown ) || IsEqualGUID( iid, &IID_IInspectable ) ||
        IsEqualGUID( iid, &IID_IAgileObject ) ||
        IsEqualGUID( iid, &IID_ICubicBezierEasingFunction ))
        *out = &impl->ICubicBezierEasingFunction_iface;
    else if (IsEqualGUID( iid, &IID_ICompositionEasingFunction ))
        *out = &impl->ICompositionEasingFunction_iface;
    else
        return E_NOINTERFACE;
    InterlockedIncrement( &impl->ref );
    return S_OK;
}

static ULONG cubic_bezier_easing_function_release( struct cubic_bezier_easing_function *impl )
{
    ULONG ref = InterlockedDecrement( &impl->ref );

    if (!ref) free( impl );
    return ref;
}

static HRESULT cubic_bezier_easing_function_get_iids( ULONG *count, IID **iids )
{
    if (!count || !iids) return E_POINTER;
    if (!(*iids = CoTaskMemAlloc( 2 * sizeof(**iids) ))) return E_OUTOFMEMORY;
    (*iids)[0] = IID_ICubicBezierEasingFunction;
    (*iids)[1] = IID_ICompositionEasingFunction;
    *count = 2;
    return S_OK;
}

static HRESULT cubic_bezier_easing_function_get_class_name( HSTRING *name )
{
    if (!name) return E_POINTER;
    return WindowsCreateString( RuntimeClass_Windows_UI_Composition_CubicBezierEasingFunction,
            ARRAY_SIZE(RuntimeClass_Windows_UI_Composition_CubicBezierEasingFunction) - 1, name );
}

static HRESULT WINAPI cubic_bezier_easing_function_QueryInterface(
        ICubicBezierEasingFunction *iface, REFIID iid, void **out )
{
    return cubic_bezier_easing_function_query_interface(
            impl_from_ICubicBezierEasingFunction( iface ), iid, out );
}

static ULONG WINAPI cubic_bezier_easing_function_AddRef( ICubicBezierEasingFunction *iface )
{
    return InterlockedIncrement( &impl_from_ICubicBezierEasingFunction( iface )->ref );
}

static ULONG WINAPI cubic_bezier_easing_function_Release( ICubicBezierEasingFunction *iface )
{
    return cubic_bezier_easing_function_release( impl_from_ICubicBezierEasingFunction( iface ) );
}

static HRESULT WINAPI cubic_bezier_easing_function_GetIids( ICubicBezierEasingFunction *iface,
        ULONG *count, IID **iids )
{
    return cubic_bezier_easing_function_get_iids( count, iids );
}

static HRESULT WINAPI cubic_bezier_easing_function_GetRuntimeClassName(
        ICubicBezierEasingFunction *iface, HSTRING *name )
{
    return cubic_bezier_easing_function_get_class_name( name );
}

static HRESULT WINAPI cubic_bezier_easing_function_GetTrustLevel(
        ICubicBezierEasingFunction *iface, TrustLevel *level )
{
    if (!level) return E_POINTER;
    *level = BaseTrust;
    return S_OK;
}

static HRESULT WINAPI cubic_bezier_easing_function_get_ControlPoint1(
        ICubicBezierEasingFunction *iface, struct Vector2 *value )
{
    if (!value) return E_POINTER;
    *value = impl_from_ICubicBezierEasingFunction( iface )->point1;
    return S_OK;
}

static HRESULT WINAPI cubic_bezier_easing_function_get_ControlPoint2(
        ICubicBezierEasingFunction *iface, struct Vector2 *value )
{
    if (!value) return E_POINTER;
    *value = impl_from_ICubicBezierEasingFunction( iface )->point2;
    return S_OK;
}

static const ICubicBezierEasingFunctionVtbl cubic_bezier_easing_function_vtbl =
{
    cubic_bezier_easing_function_QueryInterface,
    cubic_bezier_easing_function_AddRef,
    cubic_bezier_easing_function_Release,
    cubic_bezier_easing_function_GetIids,
    cubic_bezier_easing_function_GetRuntimeClassName,
    cubic_bezier_easing_function_GetTrustLevel,
    cubic_bezier_easing_function_get_ControlPoint1,
    cubic_bezier_easing_function_get_ControlPoint2,
};

static HRESULT WINAPI cubic_composition_easing_function_QueryInterface(
        ICompositionEasingFunction *iface, REFIID iid, void **out )
{
    return cubic_bezier_easing_function_query_interface(
            cubic_from_ICompositionEasingFunction( iface ), iid, out );
}

static ULONG WINAPI cubic_composition_easing_function_AddRef( ICompositionEasingFunction *iface )
{
    return InterlockedIncrement( &cubic_from_ICompositionEasingFunction( iface )->ref );
}

static ULONG WINAPI cubic_composition_easing_function_Release( ICompositionEasingFunction *iface )
{
    return cubic_bezier_easing_function_release( cubic_from_ICompositionEasingFunction( iface ) );
}

static HRESULT WINAPI cubic_composition_easing_function_GetIids( ICompositionEasingFunction *iface,
        ULONG *count, IID **iids )
{
    return cubic_bezier_easing_function_get_iids( count, iids );
}

static HRESULT WINAPI cubic_composition_easing_function_GetRuntimeClassName(
        ICompositionEasingFunction *iface, HSTRING *name )
{
    return cubic_bezier_easing_function_get_class_name( name );
}

static HRESULT WINAPI cubic_composition_easing_function_GetTrustLevel(
        ICompositionEasingFunction *iface, TrustLevel *level )
{
    if (!level) return E_POINTER;
    *level = BaseTrust;
    return S_OK;
}

static const ICompositionEasingFunctionVtbl cubic_composition_easing_function_vtbl =
{
    cubic_composition_easing_function_QueryInterface,
    cubic_composition_easing_function_AddRef,
    cubic_composition_easing_function_Release,
    cubic_composition_easing_function_GetIids,
    cubic_composition_easing_function_GetRuntimeClassName,
    cubic_composition_easing_function_GetTrustLevel,
};

static HRESULT cubic_bezier_easing_function_create( struct Vector2 point1, struct Vector2 point2,
        ICubicBezierEasingFunction **result )
{
    struct cubic_bezier_easing_function *impl;

    if (!result) return E_POINTER;
    *result = NULL;
    if (!(impl = calloc( 1, sizeof(*impl) ))) return E_OUTOFMEMORY;
    impl->ICubicBezierEasingFunction_iface.lpVtbl = &cubic_bezier_easing_function_vtbl;
    impl->ICompositionEasingFunction_iface.lpVtbl = &cubic_composition_easing_function_vtbl;
    impl->point1 = point1;
    impl->point2 = point2;
    impl->ref = 1;
    *result = &impl->ICubicBezierEasingFunction_iface;
    return S_OK;
}

struct step_easing_function_iface;

struct step_easing_function_vtbl
{
    HRESULT (WINAPI *QueryInterface)( struct step_easing_function_iface *iface,
            REFIID iid, void **out );
    ULONG (WINAPI *AddRef)( struct step_easing_function_iface *iface );
    ULONG (WINAPI *Release)( struct step_easing_function_iface *iface );
    HRESULT (WINAPI *GetIids)( struct step_easing_function_iface *iface,
            ULONG *count, IID **iids );
    HRESULT (WINAPI *GetRuntimeClassName)( struct step_easing_function_iface *iface,
            HSTRING *name );
    HRESULT (WINAPI *GetTrustLevel)( struct step_easing_function_iface *iface,
            TrustLevel *level );
    HRESULT (WINAPI *get_FinalStep)( struct step_easing_function_iface *iface, INT32 *value );
    HRESULT (WINAPI *put_FinalStep)( struct step_easing_function_iface *iface, INT32 value );
    HRESULT (WINAPI *get_InitialStep)( struct step_easing_function_iface *iface, INT32 *value );
    HRESULT (WINAPI *put_InitialStep)( struct step_easing_function_iface *iface, INT32 value );
    HRESULT (WINAPI *get_IsFinalStepSingleFrame)( struct step_easing_function_iface *iface,
            boolean *value );
    HRESULT (WINAPI *put_IsFinalStepSingleFrame)( struct step_easing_function_iface *iface,
            boolean value );
    HRESULT (WINAPI *get_IsInitialStepSingleFrame)( struct step_easing_function_iface *iface,
            boolean *value );
    HRESULT (WINAPI *put_IsInitialStepSingleFrame)( struct step_easing_function_iface *iface,
            boolean value );
    HRESULT (WINAPI *get_StepCount)( struct step_easing_function_iface *iface, INT32 *value );
    HRESULT (WINAPI *put_StepCount)( struct step_easing_function_iface *iface, INT32 value );
};

struct step_easing_function_iface
{
    const struct step_easing_function_vtbl *lpVtbl;
};

static const GUID step_easing_function_iid =
    {0xd0caa74b,0x560c,0x4a0b,{0xa5,0xf6,0x20,0x6c,0xa8,0xc3,0xec,0xd6}};

struct step_easing_function
{
    struct step_easing_function_iface IStepEasingFunction_iface;
    ICompositionEasingFunction ICompositionEasingFunction_iface;
    INT32 final_step;
    INT32 initial_step;
    INT32 step_count;
    boolean final_single_frame;
    boolean initial_single_frame;
    LONG ref;
};

static inline struct step_easing_function *step_from_IStepEasingFunction(
        struct step_easing_function_iface *iface )
{
    return CONTAINING_RECORD( iface, struct step_easing_function, IStepEasingFunction_iface );
}

static inline struct step_easing_function *step_from_ICompositionEasingFunction(
        ICompositionEasingFunction *iface )
{
    return CONTAINING_RECORD( iface, struct step_easing_function, ICompositionEasingFunction_iface );
}

static HRESULT step_easing_function_query_interface( struct step_easing_function *impl,
        REFIID iid, void **out )
{
    if (!out) return E_POINTER;
    *out = NULL;
    if (IsEqualGUID( iid, &IID_IUnknown ) || IsEqualGUID( iid, &IID_IInspectable ) ||
        IsEqualGUID( iid, &IID_IAgileObject ) || IsEqualGUID( iid, &step_easing_function_iid ))
        *out = &impl->IStepEasingFunction_iface;
    else if (IsEqualGUID( iid, &IID_ICompositionEasingFunction ))
        *out = &impl->ICompositionEasingFunction_iface;
    else
        return E_NOINTERFACE;
    InterlockedIncrement( &impl->ref );
    return S_OK;
}

static ULONG step_easing_function_release( struct step_easing_function *impl )
{
    ULONG ref = InterlockedDecrement( &impl->ref );

    if (!ref) free( impl );
    return ref;
}

static HRESULT step_easing_function_get_iids( ULONG *count, IID **iids )
{
    if (!count || !iids) return E_POINTER;
    if (!(*iids = CoTaskMemAlloc( 2 * sizeof(**iids) ))) return E_OUTOFMEMORY;
    (*iids)[0] = step_easing_function_iid;
    (*iids)[1] = IID_ICompositionEasingFunction;
    *count = 2;
    return S_OK;
}

static HRESULT step_easing_function_get_class_name( HSTRING *name )
{
    static const WCHAR class_name[] = L"Windows.UI.Composition.StepEasingFunction";

    if (!name) return E_POINTER;
    return WindowsCreateString( class_name, ARRAY_SIZE(class_name) - 1, name );
}

static HRESULT WINAPI step_easing_function_QueryInterface( struct step_easing_function_iface *iface,
        REFIID iid, void **out )
{
    return step_easing_function_query_interface( step_from_IStepEasingFunction( iface ), iid, out );
}

static ULONG WINAPI step_easing_function_AddRef( struct step_easing_function_iface *iface )
{
    return InterlockedIncrement( &step_from_IStepEasingFunction( iface )->ref );
}

static ULONG WINAPI step_easing_function_Release( struct step_easing_function_iface *iface )
{
    return step_easing_function_release( step_from_IStepEasingFunction( iface ) );
}

static HRESULT WINAPI step_easing_function_GetIids( struct step_easing_function_iface *iface,
        ULONG *count, IID **iids )
{
    return step_easing_function_get_iids( count, iids );
}

static HRESULT WINAPI step_easing_function_GetRuntimeClassName(
        struct step_easing_function_iface *iface, HSTRING *name )
{
    return step_easing_function_get_class_name( name );
}

static HRESULT WINAPI step_easing_function_GetTrustLevel( struct step_easing_function_iface *iface,
        TrustLevel *level )
{
    if (!level) return E_POINTER;
    *level = BaseTrust;
    return S_OK;
}

#define STEP_EASING_PROPERTY(name, type, field) \
    static HRESULT WINAPI step_easing_function_get_##name( \
            struct step_easing_function_iface *iface, type *value ) \
    { \
        if (!value) return E_POINTER; \
        *value = step_from_IStepEasingFunction( iface )->field; \
        return S_OK; \
    } \
    static HRESULT WINAPI step_easing_function_put_##name( \
            struct step_easing_function_iface *iface, type value ) \
    { \
        step_from_IStepEasingFunction( iface )->field = value; \
        return S_OK; \
    }

STEP_EASING_PROPERTY( FinalStep, INT32, final_step )
STEP_EASING_PROPERTY( InitialStep, INT32, initial_step )
STEP_EASING_PROPERTY( IsFinalStepSingleFrame, boolean, final_single_frame )
STEP_EASING_PROPERTY( IsInitialStepSingleFrame, boolean, initial_single_frame )
STEP_EASING_PROPERTY( StepCount, INT32, step_count )

#undef STEP_EASING_PROPERTY

static const struct step_easing_function_vtbl step_easing_function_vtbl =
{
    step_easing_function_QueryInterface,
    step_easing_function_AddRef,
    step_easing_function_Release,
    step_easing_function_GetIids,
    step_easing_function_GetRuntimeClassName,
    step_easing_function_GetTrustLevel,
    step_easing_function_get_FinalStep,
    step_easing_function_put_FinalStep,
    step_easing_function_get_InitialStep,
    step_easing_function_put_InitialStep,
    step_easing_function_get_IsFinalStepSingleFrame,
    step_easing_function_put_IsFinalStepSingleFrame,
    step_easing_function_get_IsInitialStepSingleFrame,
    step_easing_function_put_IsInitialStepSingleFrame,
    step_easing_function_get_StepCount,
    step_easing_function_put_StepCount,
};

static HRESULT WINAPI step_composition_easing_function_QueryInterface(
        ICompositionEasingFunction *iface, REFIID iid, void **out )
{
    return step_easing_function_query_interface(
            step_from_ICompositionEasingFunction( iface ), iid, out );
}

static ULONG WINAPI step_composition_easing_function_AddRef( ICompositionEasingFunction *iface )
{
    return InterlockedIncrement( &step_from_ICompositionEasingFunction( iface )->ref );
}

static ULONG WINAPI step_composition_easing_function_Release( ICompositionEasingFunction *iface )
{
    return step_easing_function_release( step_from_ICompositionEasingFunction( iface ) );
}

static HRESULT WINAPI step_composition_easing_function_GetIids( ICompositionEasingFunction *iface,
        ULONG *count, IID **iids )
{
    return step_easing_function_get_iids( count, iids );
}

static HRESULT WINAPI step_composition_easing_function_GetRuntimeClassName(
        ICompositionEasingFunction *iface, HSTRING *name )
{
    return step_easing_function_get_class_name( name );
}

static HRESULT WINAPI step_composition_easing_function_GetTrustLevel(
        ICompositionEasingFunction *iface, TrustLevel *level )
{
    if (!level) return E_POINTER;
    *level = BaseTrust;
    return S_OK;
}

static const ICompositionEasingFunctionVtbl step_composition_easing_function_vtbl =
{
    step_composition_easing_function_QueryInterface,
    step_composition_easing_function_AddRef,
    step_composition_easing_function_Release,
    step_composition_easing_function_GetIids,
    step_composition_easing_function_GetRuntimeClassName,
    step_composition_easing_function_GetTrustLevel,
};

static HRESULT step_easing_function_create( INT32 step_count, IInspectable **result )
{
    struct step_easing_function *impl;

    if (!result) return E_POINTER;
    *result = NULL;
    if (step_count < 1) return E_INVALIDARG;
    if (!(impl = calloc( 1, sizeof(*impl) ))) return E_OUTOFMEMORY;
    impl->IStepEasingFunction_iface.lpVtbl = &step_easing_function_vtbl;
    impl->ICompositionEasingFunction_iface.lpVtbl = &step_composition_easing_function_vtbl;
    impl->final_step = step_count;
    impl->step_count = step_count;
    impl->ref = 1;
    *result = (IInspectable *)&impl->IStepEasingFunction_iface;
    return S_OK;
}

#define COMPOSITOR_STUB(name, type) \
    static HRESULT WINAPI compositor_##name( ICompositor *iface, type **result ) \
    { \
        FIXME( "iface %p, result %p stub!\n", iface, result ); \
        if (!result) return E_POINTER; \
        *result = NULL; \
        return E_NOTIMPL; \
    }

COMPOSITOR_STUB( CreateColorKeyFrameAnimation, IColorKeyFrameAnimation )
COMPOSITOR_STUB( CreateColorBrush, ICompositionColorBrush )
COMPOSITOR_STUB( CreateQuaternionKeyFrameAnimation, IQuaternionKeyFrameAnimation )
COMPOSITOR_STUB( CreateTargetForCurrentView, ICompositionTarget )
COMPOSITOR_STUB( CreateVector2KeyFrameAnimation, IVector2KeyFrameAnimation )
COMPOSITOR_STUB( CreateVector4KeyFrameAnimation, IVector4KeyFrameAnimation )

static HRESULT WINAPI compositor_CreateLinearEasingFunction( ICompositor *iface,
        ILinearEasingFunction **result )
{
    TRACE( "iface %p, result %p.\n", iface, result );
    return linear_easing_function_create( result );
}

static HRESULT WINAPI compositor_CreateScalarKeyFrameAnimation( ICompositor *iface,
        IScalarKeyFrameAnimation **result )
{
    TRACE( "iface %p, result %p.\n", iface, result );
    return scalar_key_frame_animation_create( iface, result );
}

static HRESULT WINAPI compositor_CreateVector3KeyFrameAnimation( ICompositor *iface,
        IVector3KeyFrameAnimation **result )
{
    TRACE( "iface %p, result %p.\n", iface, result );
    return vector3_key_frame_animation_create( iface, result );
}

static HRESULT WINAPI compositor_CreateExpressionAnimation( ICompositor *iface,
        IExpressionAnimation **result )
{
    TRACE( "iface %p, result %p.\n", iface, result );
    return expression_animation_create( NULL, result );
}

static HRESULT WINAPI compositor_CreatePropertySet( ICompositor *iface, ICompositionPropertySet **result )
{
    TRACE( "iface %p, result %p.\n", iface, result );
    return composition_property_set_create( iface, result );
}

static HRESULT WINAPI compositor_CreateColorBrushWithColor( ICompositor *iface, struct Color color,
        ICompositionColorBrush **result )
{
    TRACE( "iface %p, color %#x/%#x/%#x/%#x, result %p.\n", iface,
           color.A, color.R, color.G, color.B, result );
    return color_brush_create( iface, color, result );
}

static HRESULT WINAPI compositor_CreateCubicBezierEasingFunction( ICompositor *iface,
        struct Vector2 point1, struct Vector2 point2, ICubicBezierEasingFunction **result )
{
    TRACE( "iface %p, point1 %f/%f, point2 %f/%f, result %p.\n", iface,
           point1.X, point1.Y, point2.X, point2.Y, result );
    return cubic_bezier_easing_function_create( point1, point2, result );
}

struct effect_source_binding
{
    HSTRING name;
    ICompositionBrush *source;
    struct effect_source_binding *next;
};

struct composition_effect
{
    ICompositionEffectFactory ICompositionEffectFactory_iface;
    ICompositionEffectBrush ICompositionEffectBrush_iface;
    ICompositionBrush ICompositionBrush_iface;
    ICompositionObject ICompositionObject_iface;
    IClosable IClosable_iface;
    struct composition_effect_graph *graph;
    ICompositor *compositor;
    ICompositionPropertySet *properties;
    struct effect_source_binding *bindings;
    CRITICAL_SECTION lock;
    BOOL factory;
    LONG closed;
    LONG ref;
};

static const ICompositionBrushVtbl effect_composition_brush_vtbl;

static inline struct composition_effect *effect_from_ICompositionObject( ICompositionObject *iface )
{
    return CONTAINING_RECORD( iface, struct composition_effect, ICompositionObject_iface );
}

static inline struct composition_effect *effect_from_ICompositionBrush( ICompositionBrush *iface )
{
    return CONTAINING_RECORD( iface, struct composition_effect, ICompositionBrush_iface );
}

static struct effect_source_binding *composition_effect_find_binding( struct composition_effect *impl,
        HSTRING name )
{
    struct effect_source_binding *binding;
    INT32 order;

    for (binding = impl->bindings; binding; binding = binding->next)
        if (SUCCEEDED(WindowsCompareStringOrdinal( binding->name, name, &order )) && !order) return binding;
    return NULL;
}

static ICompositionBrush *composition_effect_get_binding( struct composition_effect *impl, UINT index )
{
    struct effect_source_binding *binding;
    ICompositionBrush *source = NULL;

    EnterCriticalSection( &impl->lock );
    for (binding = impl->bindings; binding && index; binding = binding->next) --index;
    if (binding && (source = binding->source)) ICompositionBrush_AddRef( source );
    LeaveCriticalSection( &impl->lock );
    return source;
}

static BOOL composition_brush_contains( ICompositionBrush *brush, ICompositionBrush *target, UINT depth )
{
    struct composition_effect *effect;
    struct effect_source_binding *binding;
    ICompositionBrush *source;
    BOOL found;
    UINT i;

    if (!brush) return FALSE;
    if (brush == target || depth > 64) return TRUE;
    if (brush->lpVtbl == &nine_grid_composition_brush_vtbl)
        return composition_brush_contains( nine_grid_brush_from_ICompositionBrush( brush )->source,
                target, depth + 1 );
    if (brush->lpVtbl != &effect_composition_brush_vtbl) return FALSE;
    effect = effect_from_ICompositionBrush( brush );
    for (i = 0, binding = effect->bindings; binding; binding = binding->next, ++i)
    {
        if (!(source = composition_effect_get_binding( effect, i ))) continue;
        found = composition_brush_contains( source, target, depth + 1 );
        ICompositionBrush_Release( source );
        if (found) return TRUE;
    }
    return FALSE;
}

static HRESULT WINAPI effect_object_QueryInterface( ICompositionObject *iface, REFIID iid, void **out )
{
    struct composition_effect *impl = effect_from_ICompositionObject( iface );

    if (!out) return E_POINTER;
    *out = NULL;
    if (IsEqualGUID( iid, &IID_IUnknown ) || IsEqualGUID( iid, &IID_IInspectable ) ||
            IsEqualGUID( iid, &IID_IAgileObject ))
        *out = impl->factory ? (void *)&impl->ICompositionEffectFactory_iface :
                (void *)&impl->ICompositionEffectBrush_iface;
    else if (IsEqualGUID( iid, &IID_ICompositionObject )) *out = iface;
    else if (IsEqualGUID( iid, &IID_IClosable )) *out = &impl->IClosable_iface;
    else if (impl->factory && IsEqualGUID( iid, &IID_ICompositionEffectFactory ))
        *out = &impl->ICompositionEffectFactory_iface;
    else if (!impl->factory && IsEqualGUID( iid, &IID_ICompositionEffectBrush ))
        *out = &impl->ICompositionEffectBrush_iface;
    else if (!impl->factory && IsEqualGUID( iid, &IID_ICompositionBrush ))
        *out = &impl->ICompositionBrush_iface;
    else return E_NOINTERFACE;
    IUnknown_AddRef( (IUnknown *)*out );
    return S_OK;
}

static ULONG WINAPI effect_object_AddRef( ICompositionObject *iface )
{
    return InterlockedIncrement( &effect_from_ICompositionObject( iface )->ref );
}

static ULONG WINAPI effect_object_Release( ICompositionObject *iface )
{
    struct composition_effect *impl = effect_from_ICompositionObject( iface );
    struct effect_source_binding *binding, *next;
    ULONG ref = InterlockedDecrement( &impl->ref );

    if (!ref)
    {
        for (binding = impl->bindings; binding; binding = next)
        {
            next = binding->next;
            if (binding->source) ICompositionBrush_Release( binding->source );
            WindowsDeleteString( binding->name );
            free( binding );
        }
        if (impl->properties) ICompositionPropertySet_Release( impl->properties );
        if (impl->graph) composition_effect_graph_release( impl->graph );
        ICompositor_Release( impl->compositor );
        DeleteCriticalSection( &impl->lock );
        free( impl );
    }
    return ref;
}

static HRESULT WINAPI effect_object_GetIids( ICompositionObject *iface, ULONG *count, IID **iids )
{
    struct composition_effect *impl = effect_from_ICompositionObject( iface );
    UINT n = impl->factory ? 3 : 4;

    if (!count || !iids) return E_POINTER;
    *count = 0;
    if (!(*iids = CoTaskMemAlloc( n * sizeof(**iids) ))) return E_OUTOFMEMORY;
    (*iids)[0] = impl->factory ? IID_ICompositionEffectFactory : IID_ICompositionEffectBrush;
    (*iids)[1] = IID_ICompositionObject;
    (*iids)[2] = IID_IClosable;
    if (!impl->factory) (*iids)[3] = IID_ICompositionBrush;
    *count = n;
    return S_OK;
}

static HRESULT WINAPI effect_object_GetRuntimeClassName( ICompositionObject *iface, HSTRING *name )
{
    const WCHAR *class_name = effect_from_ICompositionObject( iface )->factory ?
            RuntimeClass_Windows_UI_Composition_CompositionEffectFactory :
            RuntimeClass_Windows_UI_Composition_CompositionEffectBrush;

    if (!name) return E_POINTER;
    return WindowsCreateString( class_name, wcslen(class_name), name );
}

static HRESULT WINAPI effect_object_GetTrustLevel( ICompositionObject *iface, TrustLevel *level )
{
    if (!level) return E_POINTER;
    *level = BaseTrust;
    return S_OK;
}

static HRESULT WINAPI effect_object_get_Compositor( ICompositionObject *iface, ICompositor **value )
{
    struct composition_effect *impl = effect_from_ICompositionObject( iface );

    if (!value) return E_POINTER;
    *value = NULL;
    if (InterlockedCompareExchange( &impl->closed, 0, 0 )) return RO_E_CLOSED;
    ICompositor_AddRef( impl->compositor );
    *value = impl->compositor;
    return S_OK;
}

static HRESULT WINAPI effect_object_get_Dispatcher( ICompositionObject *iface, ICoreDispatcher **value )
{
    if (!value) return E_POINTER;
    *value = NULL;
    return InterlockedCompareExchange( &effect_from_ICompositionObject( iface )->closed, 0, 0 ) ?
            RO_E_CLOSED : S_OK;
}

static HRESULT WINAPI effect_object_get_Properties( ICompositionObject *iface, ICompositionPropertySet **value )
{
    struct composition_effect *impl = effect_from_ICompositionObject( iface );

    if (!value) return E_POINTER;
    *value = NULL;
    if (InterlockedCompareExchange( &impl->closed, 0, 0 )) return RO_E_CLOSED;
    ICompositionPropertySet_AddRef( impl->properties );
    *value = impl->properties;
    return S_OK;
}

static HRESULT WINAPI effect_object_StartAnimation( ICompositionObject *iface, HSTRING name,
        ICompositionAnimation *animation )
{
    if (InterlockedCompareExchange( &effect_from_ICompositionObject( iface )->closed, 0, 0 )) return RO_E_CLOSED;
    if (!name || !animation) return E_INVALIDARG;
    return E_NOTIMPL;
}

static HRESULT WINAPI effect_object_StopAnimation( ICompositionObject *iface, HSTRING name )
{
    if (InterlockedCompareExchange( &effect_from_ICompositionObject( iface )->closed, 0, 0 )) return RO_E_CLOSED;
    if (!name) return E_INVALIDARG;
    return S_OK;
}

static const ICompositionObjectVtbl effect_object_vtbl =
{
    effect_object_QueryInterface,
    effect_object_AddRef,
    effect_object_Release,
    effect_object_GetIids,
    effect_object_GetRuntimeClassName,
    effect_object_GetTrustLevel,
    effect_object_get_Compositor,
    effect_object_get_Dispatcher,
    effect_object_get_Properties,
    effect_object_StartAnimation,
    effect_object_StopAnimation,
};

DEFINE_IINSPECTABLE( effect_factory, ICompositionEffectFactory, struct composition_effect, ICompositionObject_iface )
DEFINE_IINSPECTABLE( effect_brush, ICompositionEffectBrush, struct composition_effect, ICompositionObject_iface )
DEFINE_IINSPECTABLE( effect_composition_brush, ICompositionBrush, struct composition_effect, ICompositionObject_iface )
DEFINE_IINSPECTABLE( effect_closable, IClosable, struct composition_effect, ICompositionObject_iface )

static HRESULT WINAPI effect_closable_Close( IClosable *iface )
{
    struct composition_effect *impl = impl_from_IClosable( iface );
    struct effect_source_binding *binding;
    ICompositionBrush *source;

    if (InterlockedExchange( &impl->closed, TRUE )) return S_OK;
    for (binding = impl->bindings; binding; binding = binding->next)
    {
        EnterCriticalSection( &impl->lock );
        source = binding->source;
        binding->source = NULL;
        LeaveCriticalSection( &impl->lock );
        if (source) ICompositionBrush_Release( source );
    }
    return S_OK;
}

static const IClosableVtbl effect_closable_vtbl =
{
    effect_closable_QueryInterface, effect_closable_AddRef, effect_closable_Release,
    effect_closable_GetIids, effect_closable_GetRuntimeClassName, effect_closable_GetTrustLevel,
    effect_closable_Close,
};

static HRESULT WINAPI effect_brush_GetSourceParameter( ICompositionEffectBrush *iface,
        HSTRING name, ICompositionBrush **result )
{
    struct composition_effect *impl = impl_from_ICompositionEffectBrush( iface );
    struct effect_source_binding *binding;
    HRESULT hr = S_OK;

    if (!result) return E_POINTER;
    *result = NULL;
    EnterCriticalSection( &impl->lock );
    if (impl->closed) hr = RO_E_CLOSED;
    else if (!(binding = composition_effect_find_binding( impl, name ))) hr = E_INVALIDARG;
    else if ((*result = binding->source)) ICompositionBrush_AddRef( *result );
    LeaveCriticalSection( &impl->lock );
    return hr;
}

static HRESULT WINAPI effect_brush_SetSourceParameter( ICompositionEffectBrush *iface,
        HSTRING name, ICompositionBrush *source )
{
    struct composition_effect *impl = impl_from_ICompositionEffectBrush( iface );
    struct effect_source_binding *binding;
    ICompositionObject *object;
    ICompositor *compositor;
    ICompositionBrush *old = NULL;
    HRESULT hr;

    if (InterlockedCompareExchange( &impl->closed, 0, 0 )) return RO_E_CLOSED;
    if (source)
    {
        if (source->lpVtbl != &composition_brush_vtbl && source->lpVtbl != &surface_composition_brush_vtbl &&
                source->lpVtbl != &nine_grid_composition_brush_vtbl &&
                source->lpVtbl != &effect_composition_brush_vtbl) return E_NOTIMPL;
        if (composition_brush_contains( source, &impl->ICompositionBrush_iface, 0 )) return E_INVALIDARG;
        if (FAILED(hr = ICompositionBrush_QueryInterface( source, &IID_ICompositionObject, (void **)&object ))) return hr;
        hr = ICompositionObject_get_Compositor( object, &compositor );
        ICompositionObject_Release( object );
        if (FAILED(hr)) return hr;
        if (compositor != impl->compositor) hr = E_INVALIDARG;
        ICompositor_Release( compositor );
        if (FAILED(hr)) return hr;
        ICompositionBrush_AddRef( source );
    }
    EnterCriticalSection( &impl->lock );
    if (impl->closed) hr = RO_E_CLOSED;
    else if (!(binding = composition_effect_find_binding( impl, name ))) hr = E_INVALIDARG;
    else
    {
        old = binding->source;
        binding->source = source;
        source = NULL;
        hr = S_OK;
    }
    LeaveCriticalSection( &impl->lock );
    if (source) ICompositionBrush_Release( source );
    if (old) ICompositionBrush_Release( old );
    return hr;
}

static const ICompositionEffectBrushVtbl effect_brush_vtbl =
{
    effect_brush_QueryInterface, effect_brush_AddRef, effect_brush_Release,
    effect_brush_GetIids, effect_brush_GetRuntimeClassName, effect_brush_GetTrustLevel,
    effect_brush_GetSourceParameter, effect_brush_SetSourceParameter,
};

static const ICompositionBrushVtbl effect_composition_brush_vtbl =
{
    effect_composition_brush_QueryInterface, effect_composition_brush_AddRef, effect_composition_brush_Release,
    effect_composition_brush_GetIids, effect_composition_brush_GetRuntimeClassName,
    effect_composition_brush_GetTrustLevel,
};

static HRESULT composition_effect_add_bindings( struct composition_effect *impl,
        const struct composition_effect_node *node )
{
    struct effect_source_binding *binding;
    HRESULT hr;

    if (node->parameter)
    {
        if (composition_effect_find_binding( impl, node->source_name )) return S_OK;
        if (!(binding = calloc( 1, sizeof(*binding) ))) return E_OUTOFMEMORY;
        if (FAILED(hr = WindowsDuplicateString( node->source_name, &binding->name )))
        {
            free( binding );
            return hr;
        }
        binding->next = impl->bindings;
        impl->bindings = binding;
        return S_OK;
    }
    if (FAILED(hr = composition_effect_add_bindings( impl, node->sources[0] ))) return hr;
    return composition_effect_add_bindings( impl, node->sources[1] );
}

static HRESULT composition_effect_create( ICompositor *compositor, struct composition_effect_graph *graph,
        BOOL factory, struct composition_effect **result );

static HRESULT WINAPI effect_factory_CreateBrush( ICompositionEffectFactory *iface, ICompositionEffectBrush **result )
{
    struct composition_effect *impl = impl_from_ICompositionEffectFactory( iface ), *brush;
    HRESULT hr;

    if (!result) return E_POINTER;
    *result = NULL;
    if (InterlockedCompareExchange( &impl->closed, 0, 0 )) return RO_E_CLOSED;
    if (FAILED(hr = composition_effect_create( impl->compositor, impl->graph, FALSE, &brush ))) return hr;
    *result = &brush->ICompositionEffectBrush_iface;
    return S_OK;
}

static HRESULT WINAPI effect_factory_get_ExtendedError( ICompositionEffectFactory *iface, HRESULT *value )
{
    if (!value) return E_POINTER;
    if (InterlockedCompareExchange( &impl_from_ICompositionEffectFactory( iface )->closed, 0, 0 )) return RO_E_CLOSED;
    *value = S_OK;
    return S_OK;
}

static HRESULT WINAPI effect_factory_get_LoadStatus( ICompositionEffectFactory *iface,
        enum CompositionEffectFactoryLoadStatus *value )
{
    if (!value) return E_POINTER;
    if (InterlockedCompareExchange( &impl_from_ICompositionEffectFactory( iface )->closed, 0, 0 )) return RO_E_CLOSED;
    *value = CompositionEffectFactoryLoadStatus_Success;
    return S_OK;
}

static const ICompositionEffectFactoryVtbl effect_factory_vtbl =
{
    effect_factory_QueryInterface, effect_factory_AddRef, effect_factory_Release,
    effect_factory_GetIids, effect_factory_GetRuntimeClassName, effect_factory_GetTrustLevel,
    effect_factory_CreateBrush, effect_factory_get_ExtendedError, effect_factory_get_LoadStatus,
};

static HRESULT composition_effect_create( ICompositor *compositor, struct composition_effect_graph *graph,
        BOOL factory, struct composition_effect **result )
{
    struct composition_effect *impl;
    HRESULT hr;

    if (!(impl = calloc( 1, sizeof(*impl) ))) return E_OUTOFMEMORY;
    impl->ICompositionEffectFactory_iface.lpVtbl = &effect_factory_vtbl;
    impl->ICompositionEffectBrush_iface.lpVtbl = &effect_brush_vtbl;
    impl->ICompositionBrush_iface.lpVtbl = &effect_composition_brush_vtbl;
    impl->ICompositionObject_iface.lpVtbl = &effect_object_vtbl;
    impl->IClosable_iface.lpVtbl = &effect_closable_vtbl;
    impl->factory = factory;
    impl->ref = 1;
    impl->compositor = compositor;
    ICompositor_AddRef( compositor );
    InitializeCriticalSection( &impl->lock );
    impl->graph = graph;
    composition_effect_graph_add_ref( graph );
    hr = composition_property_set_create( compositor, &impl->properties );
    if (SUCCEEDED(hr) && !factory) hr = composition_effect_graph_init_properties( graph, impl->properties );
    if (SUCCEEDED(hr) && !factory) hr = composition_effect_add_bindings( impl, graph->root );
    if (FAILED(hr))
    {
        ICompositionObject_Release( &impl->ICompositionObject_iface );
        return hr;
    }
    *result = impl;
    return S_OK;
}

static HRESULT composition_effect_factory_create( ICompositor *compositor, IGraphicsEffect *effect,
        __FIIterable_1_HSTRING *properties, ICompositionEffectFactory **result )
{
    struct composition_effect_graph *graph;
    struct composition_effect *factory;
    HRESULT hr;

    if (!result) return E_POINTER;
    *result = NULL;
    if (FAILED(hr = composition_effect_graph_compile_properties( effect, properties, &graph ))) return hr;
    hr = composition_effect_create( compositor, graph, TRUE, &factory );
    composition_effect_graph_release( graph );
    if (SUCCEEDED(hr)) *result = &factory->ICompositionEffectFactory_iface;
    return hr;
}

static HRESULT WINAPI compositor_CreateEffectFactory( ICompositor *iface, IGraphicsEffect *effect,
        ICompositionEffectFactory **result )
{
    return composition_effect_factory_create( iface, effect, NULL, result );
}

static HRESULT WINAPI compositor_CreateEffectFactoryWithProperties( ICompositor *iface,
        IGraphicsEffect *effect, __FIIterable_1_HSTRING *animatable, ICompositionEffectFactory **result )
{
    return composition_effect_factory_create( iface, effect, animatable, result );
}

static HRESULT WINAPI compositor_CreateExpressionAnimationWithExpression( ICompositor *iface,
        HSTRING expression, IExpressionAnimation **result )
{
    TRACE( "iface %p, expression %s, result %p.\n", iface, debugstr_hstring( expression ), result );
    return expression_animation_create( expression, result );
}

static HRESULT WINAPI compositor_CreateInsetClipWithInsets( ICompositor *iface, FLOAT left,
        FLOAT top, FLOAT right, FLOAT bottom, IInsetClip **result )
{
    TRACE( "iface %p, insets %f/%f/%f/%f, result %p.\n", iface,
           left, top, right, bottom, result );
    return inset_clip_create( iface, left, top, right, bottom, result );
}

static HRESULT WINAPI compositor_CreateInsetClip( ICompositor *iface, IInsetClip **result )
{
    TRACE( "iface %p, result %p.\n", iface, result );
    return inset_clip_create( iface, 0.0f, 0.0f, 0.0f, 0.0f, result );
}

struct composition_scoped_batch_handler
{
    struct list entry;
    ITypedEventHandler_IInspectable_CompositionBatchCompletedEventArgs *handler;
    EventRegistrationToken token;
};

struct composition_scoped_batch
{
    ICompositionScopedBatch ICompositionScopedBatch_iface;
    struct list handlers;
    CRITICAL_SECTION cs;
    boolean active;
    boolean ended;
    LONG ref;
};

static inline struct composition_scoped_batch *impl_from_ICompositionScopedBatch(
        ICompositionScopedBatch *iface )
{
    return CONTAINING_RECORD( iface, struct composition_scoped_batch, ICompositionScopedBatch_iface );
}

static HRESULT WINAPI composition_scoped_batch_QueryInterface( ICompositionScopedBatch *iface,
        REFIID iid, void **out )
{
    if (!out) return E_POINTER;
    *out = NULL;
    if (IsEqualGUID( iid, &IID_IUnknown ) || IsEqualGUID( iid, &IID_IInspectable ) ||
        IsEqualGUID( iid, &IID_IAgileObject ) || IsEqualGUID( iid, &IID_ICompositionScopedBatch ))
        *out = iface;
    else
        return E_NOINTERFACE;
    ICompositionScopedBatch_AddRef( iface );
    return S_OK;
}

static ULONG WINAPI composition_scoped_batch_AddRef( ICompositionScopedBatch *iface )
{
    return InterlockedIncrement( &impl_from_ICompositionScopedBatch( iface )->ref );
}

static ULONG WINAPI composition_scoped_batch_Release( ICompositionScopedBatch *iface )
{
    struct composition_scoped_batch *impl = impl_from_ICompositionScopedBatch( iface );
    ULONG ref = InterlockedDecrement( &impl->ref );

    if (!ref)
    {
        struct composition_scoped_batch_handler *handler, *next;

        LIST_FOR_EACH_ENTRY_SAFE( handler, next, &impl->handlers,
                struct composition_scoped_batch_handler, entry )
        {
            list_remove( &handler->entry );
            ITypedEventHandler_IInspectable_CompositionBatchCompletedEventArgs_Release( handler->handler );
            free( handler );
        }
        DeleteCriticalSection( &impl->cs );
        free( impl );
    }
    return ref;
}

static HRESULT WINAPI composition_scoped_batch_GetIids( ICompositionScopedBatch *iface,
        ULONG *count, IID **iids )
{
    if (!count || !iids) return E_POINTER;
    if (!(*iids = CoTaskMemAlloc( sizeof(**iids) ))) return E_OUTOFMEMORY;
    **iids = IID_ICompositionScopedBatch;
    *count = 1;
    return S_OK;
}

static HRESULT WINAPI composition_scoped_batch_GetRuntimeClassName( ICompositionScopedBatch *iface,
        HSTRING *name )
{
    if (!name) return E_POINTER;
    return WindowsCreateString( RuntimeClass_Windows_UI_Composition_CompositionScopedBatch,
                                ARRAY_SIZE(RuntimeClass_Windows_UI_Composition_CompositionScopedBatch) - 1,
                                name );
}

static HRESULT WINAPI composition_scoped_batch_GetTrustLevel( ICompositionScopedBatch *iface,
        TrustLevel *level )
{
    if (!level) return E_POINTER;
    *level = BaseTrust;
    return S_OK;
}

static HRESULT WINAPI composition_scoped_batch_get_IsActive( ICompositionScopedBatch *iface,
        boolean *value )
{
    struct composition_scoped_batch *impl = impl_from_ICompositionScopedBatch( iface );

    if (!value) return E_POINTER;
    EnterCriticalSection( &impl->cs );
    *value = impl->active;
    LeaveCriticalSection( &impl->cs );
    return S_OK;
}

static HRESULT WINAPI composition_scoped_batch_get_IsEnded( ICompositionScopedBatch *iface,
        boolean *value )
{
    struct composition_scoped_batch *impl = impl_from_ICompositionScopedBatch( iface );

    if (!value) return E_POINTER;
    EnterCriticalSection( &impl->cs );
    *value = impl->ended;
    LeaveCriticalSection( &impl->cs );
    return S_OK;
}

static HRESULT WINAPI composition_scoped_batch_End( ICompositionScopedBatch *iface )
{
    struct composition_scoped_batch *impl = impl_from_ICompositionScopedBatch( iface );
    struct composition_scoped_batch_handler *entry;
    ITypedEventHandler_IInspectable_CompositionBatchCompletedEventArgs **handlers = NULL;
    unsigned int count = 0, i = 0;
    HRESULT hr;

    TRACE( "batch %p End begin.\n", iface );
    EnterCriticalSection( &impl->cs );
    if (impl->ended)
    {
        LeaveCriticalSection( &impl->cs );
        return E_ILLEGAL_METHOD_CALL;
    }
    LIST_FOR_EACH_ENTRY( entry, &impl->handlers, struct composition_scoped_batch_handler, entry ) ++count;
    if (count && !(handlers = malloc( count * sizeof(*handlers) )))
    {
        LeaveCriticalSection( &impl->cs );
        return E_OUTOFMEMORY;
    }
    LIST_FOR_EACH_ENTRY( entry, &impl->handlers, struct composition_scoped_batch_handler, entry )
    {
        handlers[i] = entry->handler;
        ITypedEventHandler_IInspectable_CompositionBatchCompletedEventArgs_AddRef( handlers[i++] );
    }
    impl->active = FALSE;
    impl->ended = TRUE;
    LeaveCriticalSection( &impl->cs );

    ICompositionScopedBatch_AddRef( iface );
    for (i = 0; i < count; ++i)
    {
        TRACE( "batch %p Completed handler %u/%u begin.\n", iface, i, count );
        hr = ITypedEventHandler_IInspectable_CompositionBatchCompletedEventArgs_Invoke(
                handlers[i], (IInspectable *)iface, NULL );
        TRACE( "batch %p Completed handler %u/%u end hr %#lx.\n", iface, i, count, hr );
        if (FAILED(hr))
            WARN( "CompositionScopedBatch Completed handler returned %#lx.\n", hr );
        ITypedEventHandler_IInspectable_CompositionBatchCompletedEventArgs_Release( handlers[i] );
    }
    ICompositionScopedBatch_Release( iface );
    free( handlers );
    TRACE( "batch %p End return.\n", iface );
    return S_OK;
}

static HRESULT WINAPI composition_scoped_batch_Resume( ICompositionScopedBatch *iface )
{
    struct composition_scoped_batch *impl = impl_from_ICompositionScopedBatch( iface );
    HRESULT hr = S_OK;

    EnterCriticalSection( &impl->cs );
    if (impl->ended) hr = E_ILLEGAL_METHOD_CALL;
    else impl->active = TRUE;
    LeaveCriticalSection( &impl->cs );
    return hr;
}

static HRESULT WINAPI composition_scoped_batch_Suspend( ICompositionScopedBatch *iface )
{
    struct composition_scoped_batch *impl = impl_from_ICompositionScopedBatch( iface );
    HRESULT hr = S_OK;

    EnterCriticalSection( &impl->cs );
    if (impl->ended) hr = E_ILLEGAL_METHOD_CALL;
    else impl->active = FALSE;
    LeaveCriticalSection( &impl->cs );
    return hr;
}

static HRESULT WINAPI composition_scoped_batch_add_Completed( ICompositionScopedBatch *iface,
        ITypedEventHandler_IInspectable_CompositionBatchCompletedEventArgs *handler,
        EventRegistrationToken *token )
{
    struct composition_scoped_batch *impl = impl_from_ICompositionScopedBatch( iface );
    struct composition_scoped_batch_handler *entry;
    static LONG next_token;

    if (!handler || !token) return E_INVALIDARG;
    if (!(entry = calloc( 1, sizeof(*entry) ))) return E_OUTOFMEMORY;
    entry->handler = handler;
    entry->token.value = InterlockedIncrement( &next_token );
    ITypedEventHandler_IInspectable_CompositionBatchCompletedEventArgs_AddRef( handler );
    EnterCriticalSection( &impl->cs );
    list_add_tail( &impl->handlers, &entry->entry );
    LeaveCriticalSection( &impl->cs );
    *token = entry->token;
    return S_OK;
}

static HRESULT WINAPI composition_scoped_batch_remove_Completed( ICompositionScopedBatch *iface,
        EventRegistrationToken token )
{
    struct composition_scoped_batch *impl = impl_from_ICompositionScopedBatch( iface );
    struct composition_scoped_batch_handler *handler;

    EnterCriticalSection( &impl->cs );
    LIST_FOR_EACH_ENTRY( handler, &impl->handlers, struct composition_scoped_batch_handler, entry )
    {
        if (handler->token.value != token.value) continue;
        list_remove( &handler->entry );
        LeaveCriticalSection( &impl->cs );
        ITypedEventHandler_IInspectable_CompositionBatchCompletedEventArgs_Release( handler->handler );
        free( handler );
        return S_OK;
    }
    LeaveCriticalSection( &impl->cs );
    return S_OK;
}

static const ICompositionScopedBatchVtbl composition_scoped_batch_vtbl =
{
    composition_scoped_batch_QueryInterface,
    composition_scoped_batch_AddRef,
    composition_scoped_batch_Release,
    composition_scoped_batch_GetIids,
    composition_scoped_batch_GetRuntimeClassName,
    composition_scoped_batch_GetTrustLevel,
    composition_scoped_batch_get_IsActive,
    composition_scoped_batch_get_IsEnded,
    composition_scoped_batch_End,
    composition_scoped_batch_Resume,
    composition_scoped_batch_Suspend,
    composition_scoped_batch_add_Completed,
    composition_scoped_batch_remove_Completed,
};

static HRESULT composition_scoped_batch_create( ICompositionScopedBatch **result )
{
    struct composition_scoped_batch *impl;

    if (!result) return E_POINTER;
    *result = NULL;
    if (!(impl = calloc( 1, sizeof(*impl) ))) return E_OUTOFMEMORY;
    impl->ICompositionScopedBatch_iface.lpVtbl = &composition_scoped_batch_vtbl;
    list_init( &impl->handlers );
    InitializeCriticalSection( &impl->cs );
    impl->active = TRUE;
    impl->ref = 1;
    *result = &impl->ICompositionScopedBatch_iface;
    return S_OK;
}

static HRESULT WINAPI compositor_CreateScopedBatch( ICompositor *iface,
        enum CompositionBatchTypes type, ICompositionScopedBatch **result )
{
    TRACE( "iface %p, type %u, result %p.\n", iface, type, result );
    return composition_scoped_batch_create( result );
}

static HRESULT WINAPI compositor_CreateSurfaceBrushWithSurface( ICompositor *iface,
        ICompositionSurface *surface, ICompositionSurfaceBrush **result )
{
    TRACE( "iface %p, surface %p, result %p.\n", iface, surface, result );
    return surface_brush_create( surface, result );
}

static HRESULT WINAPI compositor_CreateSurfaceBrush( ICompositor *iface,
        ICompositionSurfaceBrush **result )
{
    TRACE( "iface %p, result %p.\n", iface, result );
    return surface_brush_create( NULL, result );
}

DEFINE_IINSPECTABLE_(redirect_visual, struct redirect_visual_iface, struct sprite_visual,
        impl_from_redirect_visual, redirect_iface, &impl->ISpriteVisual_iface)

static IVisual *visual_get_source(struct sprite_visual *impl)
{
    IVisual *source;

    EnterCriticalSection(&visual_graph_cs);
    source = impl->source;
    if (source) IVisual_AddRef(source);
    LeaveCriticalSection(&visual_graph_cs);
    return source;
}

static BOOL visual_contains(IVisual *visual, IVisual *target, UINT depth)
{
    struct sprite_visual *impl;
    struct visual_collection *children;
    UINT i;

    if (!visual) return FALSE;
    if (visual == target || depth > 64) return TRUE;
    if (visual->lpVtbl != &visual_vtbl) return TRUE;
    impl = sprite_visual_from_IVisual(visual);
    if (visual_contains(impl->source, target, depth + 1)) return TRUE;
    children = impl_from_IVisualCollection(impl->children);
    for (i = 0; i < children->count; ++i)
        if (visual_contains(children->items[i], target, depth + 1)) return TRUE;
    return FALSE;
}

static HRESULT WINAPI redirect_visual_get_Source(struct redirect_visual_iface *iface, IVisual **value)
{
    if (!value) return E_POINTER;
    *value = visual_get_source(impl_from_redirect_visual(iface));
    return S_OK;
}

static HRESULT WINAPI redirect_visual_put_Source(struct redirect_visual_iface *iface, IVisual *value)
{
    struct sprite_visual *impl = impl_from_redirect_visual(iface), *source;
    IVisual *previous;

    if (value)
    {
        if (value->lpVtbl != &visual_vtbl) return E_INVALIDARG;
        source = sprite_visual_from_IVisual(value);
        if (source->compositor != impl->compositor) return E_INVALIDARG;
    }
    EnterCriticalSection(&visual_graph_cs);
    if (visual_contains(value, &impl->IVisual_iface, 0))
    {
        LeaveCriticalSection(&visual_graph_cs);
        return E_INVALIDARG;
    }
    if (value) IVisual_AddRef(value);
    previous = impl->source;
    impl->source = value;
    LeaveCriticalSection(&visual_graph_cs);
    if (previous) IVisual_Release(previous);
    compositor_notify_commit_needed(impl_from_ICompositor(impl->compositor));
    return S_OK;
}

static const struct redirect_visual_vtbl redirect_visual_vtbl =
{
    redirect_visual_QueryInterface,
    redirect_visual_AddRef,
    redirect_visual_Release,
    redirect_visual_GetIids,
    redirect_visual_GetRuntimeClassName,
    redirect_visual_GetTrustLevel,
    redirect_visual_get_Source,
    redirect_visual_put_Source,
};

DEFINE_IINSPECTABLE_(visual_object2, struct composition_object2_iface, struct sprite_visual,
        impl_from_visual_object2, object2_iface, &impl->ISpriteVisual_iface)

static HRESULT WINAPI visual_object2_get_Comment(struct composition_object2_iface *iface,HSTRING *value)
{
    struct sprite_visual *impl = impl_from_visual_object2(iface);
    HRESULT hr;

    if (!value) return E_POINTER;
    *value = NULL;
    EnterCriticalSection(&visual_graph_cs);
    hr = WindowsDuplicateString(impl->comment,value);
    LeaveCriticalSection(&visual_graph_cs);
    return hr;
}

static HRESULT WINAPI visual_object2_put_Comment(struct composition_object2_iface *iface,HSTRING value)
{
    struct sprite_visual *impl = impl_from_visual_object2(iface);
    HSTRING copy, previous;
    HRESULT hr;

    if (FAILED(hr = WindowsDuplicateString(value,&copy))) return hr;
    EnterCriticalSection(&visual_graph_cs);
    previous = impl->comment;
    impl->comment = copy;
    LeaveCriticalSection(&visual_graph_cs);
    WindowsDeleteString(previous);
    return S_OK;
}

static HRESULT WINAPI visual_object2_get_ImplicitAnimations(struct composition_object2_iface *iface,
        IInspectable **value)
{
    struct sprite_visual *impl = impl_from_visual_object2(iface);

    if (!value) return E_POINTER;
    EnterCriticalSection(&visual_graph_cs);
    *value = impl->implicit_animations;
    if (*value) IInspectable_AddRef(*value);
    LeaveCriticalSection(&visual_graph_cs);
    return S_OK;
}

static HRESULT WINAPI visual_object2_put_ImplicitAnimations(struct composition_object2_iface *iface,
        IInspectable *value)
{
    struct sprite_visual *impl = impl_from_visual_object2(iface);
    IInspectable *copy = NULL, *previous;
    HRESULT hr;

    if (value && FAILED(hr = IInspectable_QueryInterface(value,&implicit_collection_iid,(void **)&copy)))
        return hr;
    EnterCriticalSection(&visual_graph_cs);
    previous = impl->implicit_animations;
    impl->implicit_animations = copy;
    LeaveCriticalSection(&visual_graph_cs);
    if (previous) IInspectable_Release(previous);
    return S_OK;
}

struct group_animation_binding
{
    ICompositionAnimation *animation;
    HSTRING target;
};

static HRESULT visual_validate_animation(ICompositionAnimation *animation,HSTRING name)
{
    const WCHAR *target = WindowsGetStringRawBuffer(name,NULL);
    IExpressionAnimation *expression;
    struct Vector3 vector;
    FLOAT scalar;
    boolean boolean_value;
    HRESULT hr;

    if (!wcscmp(target,L"Opacity")) return scalar_key_frame_animation_get_final_value(animation,&scalar);
    if (!wcscmp(target,L"IsVisible")) return boolean_key_frame_animation_get_final_value(animation,&boolean_value);
    if (!wcscmp(target,L"Offset") || !wcscmp(target,L"Scale") || !wcscmp(target,L"CenterPoint"))
    {
        if (SUCCEEDED(ICompositionAnimation_QueryInterface(animation,&IID_IExpressionAnimation,(void **)&expression)))
        {
            hr = expression_animation_get_vector(expression,&vector);
            IExpressionAnimation_Release(expression);
            return hr;
        }
        return vector3_key_frame_animation_get_final_value(animation,&vector);
    }
    if (!wcscmp(target,L"RotationAxis")) return vector3_key_frame_animation_get_final_value(animation,&vector);
    return E_NOTIMPL;
}

static HRESULT visual_run_animation_group(struct sprite_visual *impl,
        ICompositionAnimationBase *value,BOOL stop)
{
    struct animation_group_iface *group = NULL;
    struct animation_iterable_iface *iterable = NULL;
    struct animation_iterator_iface *iterator = NULL;
    struct animation2_iface *animation2;
    struct group_animation_binding *bindings = NULL;
    INT32 count = 0, i;
    boolean current;
    HRESULT hr;

    if (!value) return E_INVALIDARG;
    if (FAILED(hr = ICompositionAnimationBase_QueryInterface(value,&animation_group_iid,(void **)&group)))
        return hr;
    if (FAILED(hr = group->lpVtbl->get_Count(group,&count))) goto done;
    if (count < 0) { hr = E_INVALIDARG; goto done; }
    if (count && !(bindings = calloc(count,sizeof(*bindings)))) { hr = E_OUTOFMEMORY; goto done; }
    if (FAILED(hr = ICompositionAnimationBase_QueryInterface(value,&animation_iterable_iid,(void **)&iterable)))
        goto done;
    if (FAILED(hr = iterable->lpVtbl->First(iterable,&iterator))) goto done;
    if (!iterator) { hr = E_UNEXPECTED; goto done; }
    /* Validate every target before changing any binding on the visual. */
    for (i = 0; i < count; ++i)
    {
        if (FAILED(hr = iterator->lpVtbl->get_HasCurrent(iterator,&current))) goto done;
        if (!current) { hr = E_CHANGED_STATE; goto done; }
        if (FAILED(hr = iterator->lpVtbl->get_Current(iterator,&bindings[i].animation))) goto done;
        if (!bindings[i].animation) { hr = E_INVALIDARG; goto done; }
        if (FAILED(hr = ICompositionAnimation_QueryInterface(bindings[i].animation,&animation2_iid,
                (void **)&animation2))) goto done;
        hr = animation2->lpVtbl->get_Target(animation2,&bindings[i].target);
        animation2->lpVtbl->Release(animation2);
        if (FAILED(hr)) goto done;
        if (!WindowsGetStringLen(bindings[i].target)) { hr = E_INVALIDARG; goto done; }
        if (!stop && FAILED(hr = visual_validate_animation(bindings[i].animation,bindings[i].target)))
            goto done;
        if (FAILED(hr = iterator->lpVtbl->MoveNext(iterator,&current))) goto done;
    }
    if (FAILED(hr = iterator->lpVtbl->get_HasCurrent(iterator,&current))) goto done;
    if (current) { hr = E_CHANGED_STATE; goto done; }
    for (i = 0; i < count; ++i)
    {
        if (stop) hr = ICompositionObject_StopAnimation(&impl->ICompositionObject_iface,bindings[i].target);
        else hr = ICompositionObject_StartAnimation(&impl->ICompositionObject_iface,
                bindings[i].target,bindings[i].animation);
        if (FAILED(hr)) goto done;
    }
    hr = S_OK;
done:
    if (bindings)
    {
        for (i = 0; i < count; ++i)
        {
            if (bindings[i].animation) ICompositionAnimation_Release(bindings[i].animation);
            WindowsDeleteString(bindings[i].target);
        }
        free(bindings);
    }
    if (iterator) iterator->lpVtbl->Release(iterator);
    if (iterable) iterable->lpVtbl->Release(iterable);
    if (group) group->lpVtbl->Release(group);
    return hr;
}

static HRESULT WINAPI visual_object2_StartAnimationGroup(struct composition_object2_iface *iface,
        ICompositionAnimationBase *value)
{
    TRACE("iface %p, value %p.\n",iface,value);
    return visual_run_animation_group(impl_from_visual_object2(iface),value,FALSE);
}

static HRESULT WINAPI visual_object2_StopAnimationGroup(struct composition_object2_iface *iface,
        ICompositionAnimationBase *value)
{
    TRACE("iface %p, value %p.\n",iface,value);
    return visual_run_animation_group(impl_from_visual_object2(iface),value,TRUE);
}

static HRESULT visual_apply_implicit_animation(struct sprite_visual *impl,const WCHAR *property)
{
    IInspectable *collection;
    struct implicit_map_iface *map;
    struct animation2_iface *animation2;
    ICompositionAnimationBase *value = NULL;
    ICompositionAnimation *animation;
    HSTRING name, target = NULL;
    HRESULT hr;

    visual_object2_get_ImplicitAnimations(&impl->object2_iface,&collection);
    if (!collection) return S_OK;
    hr = IInspectable_QueryInterface(collection,&implicit_map_iid,(void **)&map);
    IInspectable_Release(collection);
    if (FAILED(hr)) return hr;
    hr = WindowsCreateString(property,wcslen(property),&name);
    if (SUCCEEDED(hr))
    {
        hr = map->lpVtbl->Lookup(map,name,&value);
        WindowsDeleteString(name);
    }
    map->lpVtbl->Release(map);
    if (hr == E_BOUNDS) return S_OK;
    if (FAILED(hr)) return hr;
    if (!value) return E_INVALIDARG;
    if (SUCCEEDED(hr = ICompositionAnimationBase_QueryInterface(value,&IID_ICompositionAnimation,
            (void **)&animation)))
    {
        hr = ICompositionAnimation_QueryInterface(animation,&animation2_iid,(void **)&animation2);
        if (SUCCEEDED(hr))
        {
            hr = animation2->lpVtbl->get_Target(animation2,&target);
            animation2->lpVtbl->Release(animation2);
            if (SUCCEEDED(hr) && !WindowsGetStringLen(target)) hr = E_INVALIDARG;
            if (SUCCEEDED(hr)) hr = visual_validate_animation(animation,target);
            if (SUCCEEDED(hr)) hr = ICompositionObject_StartAnimation(&impl->ICompositionObject_iface,target,animation);
            WindowsDeleteString(target);
        }
        ICompositionAnimation_Release(animation);
    }
    else hr = visual_run_animation_group(impl,value,FALSE);
    ICompositionAnimationBase_Release(value);
    return hr;
}

static const struct composition_object2_vtbl visual_object2_vtbl =
{
    visual_object2_QueryInterface,
    visual_object2_AddRef,
    visual_object2_Release,
    visual_object2_GetIids,
    visual_object2_GetRuntimeClassName,
    visual_object2_GetTrustLevel,
    visual_object2_get_Comment,
    visual_object2_put_Comment,
    visual_object2_get_ImplicitAnimations,
    visual_object2_put_ImplicitAnimations,
    visual_object2_StartAnimationGroup,
    visual_object2_StopAnimationGroup,
};

static HRESULT WINAPI compositor_CreateSpriteVisual( ICompositor *iface, ISpriteVisual **result )
{
    struct sprite_visual *impl;
    HRESULT hr;

    FIXME( "iface %p, result %p.\n", iface, result );
    if (!result) return E_POINTER;
    *result = NULL;
    if (!(impl = calloc( 1, sizeof(*impl) ))) return E_OUTOFMEMORY;
    impl->ISpriteVisual_iface.lpVtbl = &sprite_visual_vtbl;
    impl->IVisual_iface.lpVtbl = &visual_vtbl;
    impl->redirect_iface.lpVtbl = &redirect_visual_vtbl;
    impl->object2_iface.lpVtbl = &visual_object2_vtbl;
    impl->IVisual2_iface.lpVtbl = &visual2_vtbl;
    impl->IContainerVisual_iface.lpVtbl = &container_visual_vtbl;
    impl->ICompositionObject_iface.lpVtbl = &sprite_visual_composition_object_vtbl;
    if (impl_from_ICompositor( iface )->dcomp_device && FAILED(hr =
            IDCompositionDevice_CreateVisual( impl_from_ICompositor( iface )->dcomp_device,
            &impl->dcomp_visual )))
    {
        free( impl );
        return hr;
    }
    if (FAILED(hr = visual_collection_create( impl->dcomp_visual, &impl->children )))
    {
        if (impl->dcomp_visual) IDCompositionVisual_Release( impl->dcomp_visual );
        free( impl );
        return hr;
    }
    if (FAILED(hr = composition_property_set_create( iface, &impl->properties )))
    {
        IVisualCollection_Release( impl->children );
        if (impl->dcomp_visual) IDCompositionVisual_Release( impl->dcomp_visual );
        free( impl );
        return hr;
    }
    impl->compositor = iface;
    impl_from_IVisualCollection(impl->children)->owner = &impl->IVisual_iface;
    ICompositor_AddRef( iface );
    impl->visible = TRUE;
    impl->opacity = 1.0f;
    impl->orientation.W = 1.0f;
    impl->scale.X = impl->scale.Y = impl->scale.Z = 1.0f;
    impl->transform_matrix.M11 = 1.0f;
    impl->transform_matrix.M22 = 1.0f;
    impl->transform_matrix.M33 = 1.0f;
    impl->transform_matrix.M44 = 1.0f;
    impl->ref = 1;
    *result = &impl->ISpriteVisual_iface;
    compositor_notify_commit_needed( impl_from_ICompositor( iface ) );
    return S_OK;
}

static HRESULT WINAPI compositor_CreateContainerVisual( ICompositor *iface, IContainerVisual **result )
{
    ISpriteVisual *visual;
    HRESULT hr;

    if (!result) return E_POINTER;
    *result = NULL;
    if (FAILED(hr = compositor_CreateSpriteVisual( iface, &visual ))) return hr;
    hr = ISpriteVisual_QueryInterface( visual, &IID_IContainerVisual, (void **)result );
    ISpriteVisual_Release( visual );
    return hr;
}

static HRESULT WINAPI compositor_GetCommitBatch( ICompositor *iface,
        enum CompositionBatchTypes type, ICompositionCommitBatch **result )
{
    FIXME( "iface %p, type %u, result %p stub!\n", iface, type, result );
    if (!result) return E_POINTER;
    *result = NULL;
    return E_NOTIMPL;
}

static const ICompositorVtbl compositor_vtbl =
{
    compositor_QueryInterface,
    compositor_AddRef,
    compositor_Release,
    compositor_GetIids,
    compositor_GetRuntimeClassName,
    compositor_GetTrustLevel,
    compositor_CreateColorKeyFrameAnimation,
    compositor_CreateColorBrush,
    compositor_CreateColorBrushWithColor,
    compositor_CreateContainerVisual,
    compositor_CreateCubicBezierEasingFunction,
    compositor_CreateEffectFactory,
    compositor_CreateEffectFactoryWithProperties,
    compositor_CreateExpressionAnimation,
    compositor_CreateExpressionAnimationWithExpression,
    compositor_CreateInsetClip,
    compositor_CreateInsetClipWithInsets,
    compositor_CreateLinearEasingFunction,
    compositor_CreatePropertySet,
    compositor_CreateQuaternionKeyFrameAnimation,
    compositor_CreateScalarKeyFrameAnimation,
    compositor_CreateScopedBatch,
    compositor_CreateSpriteVisual,
    compositor_CreateSurfaceBrush,
    compositor_CreateSurfaceBrushWithSurface,
    compositor_CreateTargetForCurrentView,
    compositor_CreateVector2KeyFrameAnimation,
    compositor_CreateVector3KeyFrameAnimation,
    compositor_CreateVector4KeyFrameAnimation,
    compositor_GetCommitBatch,
};

static HRESULT WINAPI compositor2_QueryInterface( struct compositor2_iface *iface, REFIID iid, void **out )
{
    struct compositor *impl = impl_from_compositor2( iface );
    return ICompositor_QueryInterface( &impl->ICompositor_iface, iid, out );
}

static ULONG WINAPI compositor2_AddRef( struct compositor2_iface *iface )
{
    struct compositor *impl = impl_from_compositor2( iface );
    return ICompositor_AddRef( &impl->ICompositor_iface );
}

static ULONG WINAPI compositor2_Release( struct compositor2_iface *iface )
{
    struct compositor *impl = impl_from_compositor2( iface );
    return ICompositor_Release( &impl->ICompositor_iface );
}

static HRESULT WINAPI compositor2_GetIids( struct compositor2_iface *iface, ULONG *count, IID **iids )
{
    struct compositor *impl = impl_from_compositor2( iface );
    return ICompositor_GetIids( &impl->ICompositor_iface, count, iids );
}

static HRESULT WINAPI compositor2_GetRuntimeClassName( struct compositor2_iface *iface, HSTRING *name )
{
    struct compositor *impl = impl_from_compositor2( iface );
    return ICompositor_GetRuntimeClassName( &impl->ICompositor_iface, name );
}

static HRESULT WINAPI compositor2_GetTrustLevel( struct compositor2_iface *iface, TrustLevel *level )
{
    struct compositor *impl = impl_from_compositor2( iface );
    return ICompositor_GetTrustLevel( &impl->ICompositor_iface, level );
}

#define DEFINE_COMPOSITOR2_CREATE_METHOD(name) \
    static HRESULT WINAPI compositor2_##name( struct compositor2_iface *iface, IInspectable **result ) \
    { \
        FIXME( "iface %p, result %p stub!\n", iface, result ); \
        if (!result) return E_POINTER; \
        *result = NULL; \
        return E_NOTIMPL; \
    }

DEFINE_COMPOSITOR2_CREATE_METHOD(CreateAmbientLight)
struct collection_object2_state
{
    struct composition_object2_iface iface;
    ICompositionObject *object;
    CRITICAL_SECTION *cs;
    BOOL *closed;
    HSTRING comment;
    IInspectable *implicit_animations;
};

static struct collection_object2_state *impl_from_collection_object2( struct composition_object2_iface *iface )
{
    return CONTAINING_RECORD( iface, struct collection_object2_state, iface );
}

static HRESULT WINAPI collection_object2_QueryInterface( struct composition_object2_iface *iface,
        REFIID iid, void **out )
{
    return ICompositionObject_QueryInterface( impl_from_collection_object2( iface )->object, iid, out );
}

static ULONG WINAPI collection_object2_AddRef( struct composition_object2_iface *iface )
{
    return ICompositionObject_AddRef( impl_from_collection_object2( iface )->object );
}

static ULONG WINAPI collection_object2_Release( struct composition_object2_iface *iface )
{
    return ICompositionObject_Release( impl_from_collection_object2( iface )->object );
}

static HRESULT WINAPI collection_object2_GetIids( struct composition_object2_iface *iface, ULONG *count, IID **iids )
{
    return ICompositionObject_GetIids( impl_from_collection_object2( iface )->object, count, iids );
}

static HRESULT WINAPI collection_object2_GetRuntimeClassName( struct composition_object2_iface *iface, HSTRING *name )
{
    return ICompositionObject_GetRuntimeClassName( impl_from_collection_object2( iface )->object, name );
}

static HRESULT WINAPI collection_object2_GetTrustLevel( struct composition_object2_iface *iface, TrustLevel *level )
{
    return ICompositionObject_GetTrustLevel( impl_from_collection_object2( iface )->object, level );
}

static HRESULT WINAPI collection_object2_get_Comment( struct composition_object2_iface *iface, HSTRING *value )
{
    struct collection_object2_state *impl = impl_from_collection_object2( iface );
    HRESULT hr;

    if (!value) return E_POINTER;
    *value = NULL;
    EnterCriticalSection( impl->cs );
    hr = *impl->closed ? RO_E_CLOSED : WindowsDuplicateString( impl->comment, value );
    LeaveCriticalSection( impl->cs );
    return hr;
}

static HRESULT WINAPI collection_object2_put_Comment( struct composition_object2_iface *iface, HSTRING value )
{
    struct collection_object2_state *impl = impl_from_collection_object2( iface );
    HSTRING copy, previous;
    HRESULT hr;

    if (FAILED(hr = WindowsDuplicateString( value, &copy ))) return hr;
    EnterCriticalSection( impl->cs );
    if (*impl->closed)
    {
        LeaveCriticalSection( impl->cs );
        WindowsDeleteString( copy );
        return RO_E_CLOSED;
    }
    previous = impl->comment;
    impl->comment = copy;
    LeaveCriticalSection( impl->cs );
    WindowsDeleteString( previous );
    return S_OK;
}

static HRESULT WINAPI collection_object2_get_ImplicitAnimations( struct composition_object2_iface *iface,
        IInspectable **value )
{
    struct collection_object2_state *impl = impl_from_collection_object2( iface );

    if (!value) return E_POINTER;
    *value = NULL;
    EnterCriticalSection( impl->cs );
    if (*impl->closed) { LeaveCriticalSection( impl->cs ); return RO_E_CLOSED; }
    *value = impl->implicit_animations;
    if (*value) IInspectable_AddRef( *value );
    LeaveCriticalSection( impl->cs );
    return S_OK;
}

static HRESULT WINAPI collection_object2_put_ImplicitAnimations( struct composition_object2_iface *iface,
        IInspectable *value )
{
    struct collection_object2_state *impl = impl_from_collection_object2( iface );
    IInspectable *copy = NULL, *previous;
    ICompositionObject *object;
    ICompositor *owner, *compositor;
    IUnknown *identity, *expected;
    HRESULT hr;

    if (value)
    {
        if (FAILED(hr = ICompositionObject_get_Compositor( impl->object, &owner ))) return hr;
        hr = IInspectable_QueryInterface( value, &implicit_collection_iid, (void **)&copy );
        if (SUCCEEDED(hr))
        {
            hr = IInspectable_QueryInterface( copy, &IID_ICompositionObject, (void **)&object );
            if (SUCCEEDED(hr))
            {
                hr = ICompositionObject_get_Compositor( object, &compositor );
                ICompositionObject_Release( object );
                if (SUCCEEDED(hr))
                {
                    if (!compositor) hr = E_INVALIDARG;
                    else
                    {
                        hr = ICompositor_QueryInterface( compositor, &IID_IUnknown, (void **)&identity );
                        ICompositor_Release( compositor );
                        if (SUCCEEDED(hr))
                        {
                            hr = ICompositor_QueryInterface( owner, &IID_IUnknown, (void **)&expected );
                            if (SUCCEEDED(hr))
                            {
                                hr = identity == expected ? S_OK : E_INVALIDARG;
                                IUnknown_Release( expected );
                            }
                            IUnknown_Release( identity );
                        }
                    }
                }
            }
        }
        ICompositor_Release( owner );
        if (FAILED(hr)) { if (copy) IInspectable_Release( copy ); return hr; }
    }
    EnterCriticalSection( impl->cs );
    if (*impl->closed)
    {
        LeaveCriticalSection( impl->cs );
        if (copy) IInspectable_Release( copy );
        return RO_E_CLOSED;
    }
    previous = impl->implicit_animations;
    impl->implicit_animations = copy;
    LeaveCriticalSection( impl->cs );
    if (previous) IInspectable_Release( previous );
    return S_OK;
}

static HRESULT collection_object2_run_group( struct collection_object2_state *impl,
        ICompositionAnimationBase *value, BOOL stop )
{
    struct animation_group_iface *group;
    INT32 count;
    HRESULT hr;

    EnterCriticalSection( impl->cs );
    hr = *impl->closed ? RO_E_CLOSED : S_OK;
    LeaveCriticalSection( impl->cs );
    if (FAILED(hr)) return hr;
    if (!value) return E_INVALIDARG;
    if (FAILED(hr = ICompositionAnimationBase_QueryInterface( value, &animation_group_iid, (void **)&group ))) return hr;
    hr = group->lpVtbl->get_Count( group, &count );
    group->lpVtbl->Release( group );
    if (FAILED(hr)) return hr;
    if (count < 0) return E_INVALIDARG;
    return count ? E_NOTIMPL : S_OK;
}

static HRESULT WINAPI collection_object2_StartAnimationGroup( struct composition_object2_iface *iface,
        ICompositionAnimationBase *value )
{
    return collection_object2_run_group( impl_from_collection_object2( iface ), value, FALSE );
}

static HRESULT WINAPI collection_object2_StopAnimationGroup( struct composition_object2_iface *iface,
        ICompositionAnimationBase *value )
{
    return collection_object2_run_group( impl_from_collection_object2( iface ), value, TRUE );
}

static const struct composition_object2_vtbl collection_object2_vtbl =
{
    collection_object2_QueryInterface, collection_object2_AddRef, collection_object2_Release,
    collection_object2_GetIids, collection_object2_GetRuntimeClassName, collection_object2_GetTrustLevel,
    collection_object2_get_Comment, collection_object2_put_Comment,
    collection_object2_get_ImplicitAnimations, collection_object2_put_ImplicitAnimations,
    collection_object2_StartAnimationGroup, collection_object2_StopAnimationGroup,
};

static void collection_object2_release_state( HSTRING comment, IInspectable *implicit_animations )
{
    WindowsDeleteString( comment );
    if (implicit_animations) IInspectable_Release( implicit_animations );
}

struct animation_group_entry
{
    ICompositionAnimation *animation;
    IUnknown *identity;
};

struct native_animation_group
{
    struct animation_group_iface iface;
    struct animation_iterable_iface iterable;
    ICompositionObject ICompositionObject_iface;
    IClosable closable;
    struct collection_object2_state object2;
    BOOL closed;
    LONG ref;
    CRITICAL_SECTION cs;
    ICompositor *compositor;
    ICompositionPropertySet *properties;
    struct animation_group_entry *entries;
    UINT32 count, generation;
};

struct native_animation_iterator
{
    struct animation_iterator_iface iface;
    LONG ref;
    struct native_animation_group *group;
    UINT32 index, generation;
};

static struct native_animation_group *impl_from_animation_group( struct animation_group_iface *iface )
{
    return CONTAINING_RECORD( iface, struct native_animation_group, iface );
}

static HRESULT WINAPI animation_group_QueryInterface( struct animation_group_iface *iface,
        REFIID iid, void **out )
{
    struct native_animation_group *impl = impl_from_animation_group( iface );

    if (!out) return E_POINTER;
    *out = NULL;
    if (IsEqualGUID( iid, &IID_IUnknown ) || IsEqualGUID( iid, &IID_IInspectable ) ||
            IsEqualGUID( iid, &animation_group_iid ) || IsEqualGUID( iid, &IID_ICompositionAnimationBase ))
        *out = &impl->iface;
    else if (IsEqualGUID( iid, &animation_iterable_iid )) *out = &impl->iterable;
    else if (IsEqualGUID( iid, &IID_ICompositionObject )) *out = &impl->ICompositionObject_iface;
    else if (IsEqualGUID( iid, &IID_IClosable )) *out = &impl->closable;
    else if (IsEqualGUID( iid, &composition_object2_iid )) *out = &impl->object2.iface;
    else return E_NOINTERFACE;
    InterlockedIncrement( &impl->ref );
    return S_OK;
}

static ULONG WINAPI animation_group_AddRef( struct animation_group_iface *iface )
{
    return InterlockedIncrement( &impl_from_animation_group( iface )->ref );
}

static void animation_group_release_entries( struct animation_group_entry *entries, UINT32 count )
{
    UINT32 i;

    for (i = 0; i < count; ++i)
    {
        ICompositionAnimation_Release( entries[i].animation );
        IUnknown_Release( entries[i].identity );
    }
    free( entries );
}

static ULONG WINAPI animation_group_Release( struct animation_group_iface *iface )
{
    struct native_animation_group *impl = impl_from_animation_group( iface );
    ULONG ref = InterlockedDecrement( &impl->ref );

    if (!ref)
    {
        animation_group_release_entries( impl->entries, impl->count );
        if (impl->properties) ICompositionPropertySet_Release( impl->properties );
        if (impl->compositor) ICompositor_Release( impl->compositor );
        collection_object2_release_state( impl->object2.comment, impl->object2.implicit_animations );
        DeleteCriticalSection( &impl->cs );
        free( impl );
    }
    return ref;
}

static HRESULT WINAPI animation_group_GetIids( struct animation_group_iface *iface,
        ULONG *count, IID **iids )
{
    if (!count || !iids) return E_POINTER;
    *count = 0;
    *iids = CoTaskMemAlloc( 6 * sizeof(**iids) );
    if (!*iids) return E_OUTOFMEMORY;
    (*iids)[0] = animation_group_iid;
    (*iids)[1] = IID_ICompositionAnimationBase;
    (*iids)[2] = animation_iterable_iid;
    (*iids)[3] = IID_ICompositionObject;
    (*iids)[4] = IID_IClosable;
    (*iids)[5] = composition_object2_iid;
    *count = 6;
    return S_OK;
}

static HRESULT WINAPI animation_group_GetRuntimeClassName( struct animation_group_iface *iface,
        HSTRING *name )
{
    static const WCHAR class_name[] = L"Windows.UI.Composition.CompositionAnimationGroup";

    if (!name) return E_POINTER;
    return WindowsCreateString( class_name, ARRAY_SIZE(class_name) - 1, name );
}

static HRESULT WINAPI animation_group_GetTrustLevel( struct animation_group_iface *iface, TrustLevel *level )
{
    if (!level) return E_POINTER;
    *level = BaseTrust;
    return S_OK;
}

static HRESULT WINAPI animation_group_get_Count( struct animation_group_iface *iface, INT32 *count )
{
    struct native_animation_group *impl = impl_from_animation_group( iface );

    if (!count) return E_POINTER;
    EnterCriticalSection( &impl->cs );
    *count = 0;
    if (impl->closed) { LeaveCriticalSection( &impl->cs ); return RO_E_CLOSED; }
    *count = impl->count;
    LeaveCriticalSection( &impl->cs );
    return S_OK;
}

static HRESULT WINAPI animation_group_Add( struct animation_group_iface *iface, ICompositionAnimation *animation )
{
    struct native_animation_group *impl = impl_from_animation_group( iface );
    struct animation_group_entry *entries;
    ICompositionObject *object;
    ICompositor *compositor, *parent;
    IUnknown *identity, *owner, *expected;
    UINT32 i;
    HRESULT hr;

    if (!animation) return E_INVALIDARG;
    if (FAILED(hr = ICompositionAnimation_QueryInterface( animation, &IID_ICompositionObject, (void **)&object )))
        return hr;
    hr = ICompositionObject_get_Compositor( object, &compositor );
    ICompositionObject_Release( object );
    if (FAILED(hr)) return hr;
    if (!compositor) return E_INVALIDARG;
    hr = ICompositor_QueryInterface( compositor, &IID_IUnknown, (void **)&owner );
    ICompositor_Release( compositor );
    if (FAILED(hr)) return hr;
    EnterCriticalSection( &impl->cs );
    parent = impl->compositor;
    if (parent) ICompositor_AddRef( parent );
    LeaveCriticalSection( &impl->cs );
    if (!parent) { IUnknown_Release( owner ); return RO_E_CLOSED; }
    hr = ICompositor_QueryInterface( parent, &IID_IUnknown, (void **)&expected );
    ICompositor_Release( parent );
    if (FAILED(hr)) { IUnknown_Release( owner ); return hr; }
    hr = owner == expected ? S_OK : E_INVALIDARG;
    IUnknown_Release( owner );
    IUnknown_Release( expected );
    if (FAILED(hr)) return hr;
    if (FAILED(hr = ICompositionAnimation_QueryInterface( animation, &IID_IUnknown, (void **)&identity )))
        return hr;
    ICompositionAnimation_AddRef( animation );
    EnterCriticalSection( &impl->cs );
    if (impl->closed) { LeaveCriticalSection( &impl->cs ); ICompositionAnimation_Release( animation ); IUnknown_Release( identity ); return RO_E_CLOSED; }
    for (i = 0; i < impl->count; ++i) if (impl->entries[i].identity == identity) break;
    if (i != impl->count) hr = S_OK;
    else if (impl->count == INT_MAX) hr = E_OUTOFMEMORY;
    else if (!(entries = realloc( impl->entries, ((size_t)impl->count + 1) * sizeof(*entries) ))) hr = E_OUTOFMEMORY;
    else
    {
        impl->entries = entries;
        impl->entries[impl->count].animation = animation;
        impl->entries[impl->count++].identity = identity;
        ++impl->generation;
        LeaveCriticalSection( &impl->cs );
        return S_OK;
    }
    LeaveCriticalSection( &impl->cs );
    ICompositionAnimation_Release( animation );
    IUnknown_Release( identity );
    return hr;
}

static HRESULT WINAPI animation_group_Remove( struct animation_group_iface *iface, ICompositionAnimation *animation )
{
    struct native_animation_group *impl = impl_from_animation_group( iface );
    struct animation_group_entry removed;
    IUnknown *identity;
    UINT32 i;
    HRESULT hr;

    if (!animation) return E_INVALIDARG;
    if (FAILED(hr = ICompositionAnimation_QueryInterface( animation, &IID_IUnknown, (void **)&identity ))) return hr;
    EnterCriticalSection( &impl->cs );
    if (impl->closed) { LeaveCriticalSection( &impl->cs ); IUnknown_Release( identity ); return RO_E_CLOSED; }
    for (i = 0; i < impl->count; ++i) if (impl->entries[i].identity == identity) break;
    if (i != impl->count)
    {
        removed = impl->entries[i];
        memmove( impl->entries + i, impl->entries + i + 1, (--impl->count - i) * sizeof(*impl->entries) );
        ++impl->generation;
    }
    else removed.animation = NULL;
    LeaveCriticalSection( &impl->cs );
    IUnknown_Release( identity );
    if (removed.animation)
    {
        ICompositionAnimation_Release( removed.animation );
        IUnknown_Release( removed.identity );
    }
    return S_OK;
}

static HRESULT WINAPI animation_group_RemoveAll( struct animation_group_iface *iface )
{
    struct native_animation_group *impl = impl_from_animation_group( iface );
    struct animation_group_entry *entries;
    UINT32 count;

    EnterCriticalSection( &impl->cs );
    if (impl->closed) { LeaveCriticalSection( &impl->cs ); return RO_E_CLOSED; }
    entries = impl->entries;
    count = impl->count;
    impl->entries = NULL;
    impl->count = 0;
    if (count) ++impl->generation;
    LeaveCriticalSection( &impl->cs );
    animation_group_release_entries( entries, count );
    return S_OK;
}

static const struct animation_group_vtbl animation_group_vtbl =
{
    animation_group_QueryInterface, animation_group_AddRef, animation_group_Release,
    animation_group_GetIids, animation_group_GetRuntimeClassName, animation_group_GetTrustLevel,
    animation_group_get_Count, animation_group_Add, animation_group_Remove, animation_group_RemoveAll,
};

static HRESULT WINAPI animation_iterator_QueryInterface( struct animation_iterator_iface *iface,
        REFIID iid, void **out )
{
    if (!out) return E_POINTER;
    *out = NULL;
    if (!IsEqualGUID( iid, &IID_IUnknown ) && !IsEqualGUID( iid, &IID_IInspectable ) &&
            !IsEqualGUID( iid, &animation_iterator_iid )) return E_NOINTERFACE;
    *out = iface;
    iface->lpVtbl->AddRef( iface );
    return S_OK;
}

static ULONG WINAPI animation_iterator_AddRef( struct animation_iterator_iface *iface )
{
    return InterlockedIncrement( &CONTAINING_RECORD( iface, struct native_animation_iterator, iface )->ref );
}

static ULONG WINAPI animation_iterator_Release( struct animation_iterator_iface *iface )
{
    struct native_animation_iterator *impl = CONTAINING_RECORD( iface, struct native_animation_iterator, iface );
    ULONG ref = InterlockedDecrement( &impl->ref );

    if (!ref)
    {
        animation_group_Release( &impl->group->iface );
        free( impl );
    }
    return ref;
}

static HRESULT WINAPI animation_iterator_GetIids( struct animation_iterator_iface *iface, ULONG *count, IID **iids )
{
    if (!count || !iids) return E_POINTER;
    *count = 0;
    *iids = CoTaskMemAlloc( sizeof(**iids) );
    if (!*iids) return E_OUTOFMEMORY;
    **iids = animation_iterator_iid;
    *count = 1;
    return S_OK;
}

static HRESULT WINAPI animation_iterator_GetRuntimeClassName( struct animation_iterator_iface *iface, HSTRING *name )
{
    if (!name) return E_POINTER;
    *name = NULL;
    return S_OK;
}

static HRESULT WINAPI animation_iterator_GetTrustLevel( struct animation_iterator_iface *iface, TrustLevel *level )
{
    if (!level) return E_POINTER;
    *level = BaseTrust;
    return S_OK;
}

static HRESULT WINAPI animation_iterator_get_Current( struct animation_iterator_iface *iface, ICompositionAnimation **value )
{
    struct native_animation_iterator *impl = CONTAINING_RECORD( iface, struct native_animation_iterator, iface );
    HRESULT hr = S_OK;

    if (!value) return E_POINTER;
    *value = NULL;
    EnterCriticalSection( &impl->group->cs );
    if (impl->group->closed) hr = RO_E_CLOSED;
    else if (impl->generation != impl->group->generation) hr = E_CHANGED_STATE;
    else if (impl->index >= impl->group->count) hr = E_BOUNDS;
    else
    {
        *value = impl->group->entries[impl->index].animation;
        ICompositionAnimation_AddRef( *value );
    }
    LeaveCriticalSection( &impl->group->cs );
    return hr;
}

static HRESULT WINAPI animation_iterator_get_HasCurrent( struct animation_iterator_iface *iface, boolean *value )
{
    struct native_animation_iterator *impl = CONTAINING_RECORD( iface, struct native_animation_iterator, iface );
    HRESULT hr = S_OK;

    if (!value) return E_POINTER;
    *value = FALSE;
    EnterCriticalSection( &impl->group->cs );
    if (impl->group->closed) hr = RO_E_CLOSED;
    else if (impl->generation != impl->group->generation) hr = E_CHANGED_STATE;
    else *value = impl->index < impl->group->count;
    LeaveCriticalSection( &impl->group->cs );
    return hr;
}

static HRESULT WINAPI animation_iterator_MoveNext( struct animation_iterator_iface *iface, boolean *value )
{
    struct native_animation_iterator *impl = CONTAINING_RECORD( iface, struct native_animation_iterator, iface );
    HRESULT hr = S_OK;

    if (!value) return E_POINTER;
    *value = FALSE;
    EnterCriticalSection( &impl->group->cs );
    if (impl->group->closed) hr = RO_E_CLOSED;
    else if (impl->generation != impl->group->generation) hr = E_CHANGED_STATE;
    else
    {
        if (impl->index < impl->group->count) ++impl->index;
        *value = impl->index < impl->group->count;
    }
    LeaveCriticalSection( &impl->group->cs );
    return hr;
}

static HRESULT WINAPI animation_iterator_GetMany( struct animation_iterator_iface *iface, UINT32 capacity,
        ICompositionAnimation **values, UINT32 *count )
{
    struct native_animation_iterator *impl = CONTAINING_RECORD( iface, struct native_animation_iterator, iface );
    HRESULT hr = S_OK;

    if (!count) return E_POINTER;
    *count = 0;
    if (capacity && !values) return E_POINTER;
    EnterCriticalSection( &impl->group->cs );
    if (impl->group->closed) hr = RO_E_CLOSED;
    else if (impl->generation != impl->group->generation) hr = E_CHANGED_STATE;
    else while (*count < capacity && impl->index < impl->group->count)
    {
        values[*count] = impl->group->entries[impl->index++].animation;
        ICompositionAnimation_AddRef( values[(*count)++] );
    }
    LeaveCriticalSection( &impl->group->cs );
    return hr;
}

static const struct animation_iterator_vtbl animation_iterator_vtbl =
{
    animation_iterator_QueryInterface, animation_iterator_AddRef, animation_iterator_Release,
    animation_iterator_GetIids, animation_iterator_GetRuntimeClassName, animation_iterator_GetTrustLevel,
    animation_iterator_get_Current, animation_iterator_get_HasCurrent, animation_iterator_MoveNext,
    animation_iterator_GetMany,
};

static HRESULT WINAPI animation_iterable_First( struct animation_iterable_iface *iface, struct animation_iterator_iface **value )
{
    struct native_animation_group *group = CONTAINING_RECORD( iface, struct native_animation_group, iterable );
    struct native_animation_iterator *impl;

    if (!value) return E_POINTER;
    *value = NULL;
    if (!(impl = calloc( 1, sizeof(*impl) ))) return E_OUTOFMEMORY;
    impl->iface.lpVtbl = &animation_iterator_vtbl;
    impl->ref = 1;
    impl->group = group;
    animation_group_AddRef( &group->iface );
    EnterCriticalSection( &group->cs );
    if (group->closed)
    {
        LeaveCriticalSection( &group->cs );
        animation_group_Release( &group->iface );
        free( impl );
        return RO_E_CLOSED;
    }
    impl->generation = group->generation;
    LeaveCriticalSection( &group->cs );
    *value = &impl->iface;
    return S_OK;
}

static HRESULT WINAPI animation_iterable_QueryInterface( struct animation_iterable_iface *iface, REFIID iid, void **out)
{
    struct native_animation_group *impl = CONTAINING_RECORD( iface, struct native_animation_group, iterable );

    return animation_group_QueryInterface( &impl->iface, iid, out );
}

static ULONG WINAPI animation_iterable_AddRef( struct animation_iterable_iface *iface)
{
    struct native_animation_group *impl = CONTAINING_RECORD( iface, struct native_animation_group, iterable );

    return animation_group_AddRef( &impl->iface );
}

static ULONG WINAPI animation_iterable_Release( struct animation_iterable_iface *iface)
{
    struct native_animation_group *impl = CONTAINING_RECORD( iface, struct native_animation_group, iterable );

    return animation_group_Release( &impl->iface );
}

static HRESULT WINAPI animation_iterable_GetIids( struct animation_iterable_iface *iface, ULONG *count, IID **iids)
{
    struct native_animation_group *impl = CONTAINING_RECORD( iface, struct native_animation_group, iterable );

    return animation_group_GetIids( &impl->iface, count, iids );
}

static HRESULT WINAPI animation_iterable_GetRuntimeClassName( struct animation_iterable_iface *iface, HSTRING *name)
{
    struct native_animation_group *impl = CONTAINING_RECORD( iface, struct native_animation_group, iterable );

    return animation_group_GetRuntimeClassName( &impl->iface, name );
}

static HRESULT WINAPI animation_iterable_GetTrustLevel( struct animation_iterable_iface *iface, TrustLevel *level)
{
    struct native_animation_group *impl = CONTAINING_RECORD( iface, struct native_animation_group, iterable );

    return animation_group_GetTrustLevel( &impl->iface, level );
}

static HRESULT WINAPI animation_group_object_QueryInterface( ICompositionObject *iface, REFIID iid, void **out)
{
    struct native_animation_group *impl = CONTAINING_RECORD( iface, struct native_animation_group, ICompositionObject_iface );

    return animation_group_QueryInterface( &impl->iface, iid, out );
}

static ULONG WINAPI animation_group_object_AddRef( ICompositionObject *iface)
{
    struct native_animation_group *impl = CONTAINING_RECORD( iface, struct native_animation_group, ICompositionObject_iface );

    return animation_group_AddRef( &impl->iface );
}

static ULONG WINAPI animation_group_object_Release( ICompositionObject *iface)
{
    struct native_animation_group *impl = CONTAINING_RECORD( iface, struct native_animation_group, ICompositionObject_iface );

    return animation_group_Release( &impl->iface );
}

static HRESULT WINAPI animation_group_object_GetIids( ICompositionObject *iface, ULONG *count, IID **iids)
{
    struct native_animation_group *impl = CONTAINING_RECORD( iface, struct native_animation_group, ICompositionObject_iface );

    return animation_group_GetIids( &impl->iface, count, iids );
}

static HRESULT WINAPI animation_group_object_GetRuntimeClassName( ICompositionObject *iface, HSTRING *name)
{
    struct native_animation_group *impl = CONTAINING_RECORD( iface, struct native_animation_group, ICompositionObject_iface );

    return animation_group_GetRuntimeClassName( &impl->iface, name );
}

static HRESULT WINAPI animation_group_object_GetTrustLevel( ICompositionObject *iface, TrustLevel *level)
{
    struct native_animation_group *impl = CONTAINING_RECORD( iface, struct native_animation_group, ICompositionObject_iface );

    return animation_group_GetTrustLevel( &impl->iface, level );
}

static const struct animation_iterable_vtbl animation_iterable_vtbl =
{
    animation_iterable_QueryInterface, animation_iterable_AddRef, animation_iterable_Release,
    animation_iterable_GetIids, animation_iterable_GetRuntimeClassName, animation_iterable_GetTrustLevel,
    animation_iterable_First,
};

static HRESULT WINAPI animation_group_object_get_Compositor( ICompositionObject *iface, ICompositor **value )
{
    struct native_animation_group *impl = CONTAINING_RECORD( iface, struct native_animation_group, ICompositionObject_iface );

    if (!value) return E_POINTER;
    *value = NULL;
    EnterCriticalSection( &impl->cs );
    if (impl->closed) { LeaveCriticalSection( &impl->cs ); return RO_E_CLOSED; }
    *value = impl->compositor;
    ICompositor_AddRef( *value );
    LeaveCriticalSection( &impl->cs );
    return S_OK;
}

static HRESULT WINAPI animation_group_object_get_Dispatcher( ICompositionObject *iface, ICoreDispatcher **value )
{
    struct native_animation_group *impl = CONTAINING_RECORD( iface, struct native_animation_group, ICompositionObject_iface );
    HRESULT hr;

    if (!value) return E_POINTER;
    *value = NULL;
    EnterCriticalSection( &impl->cs );
    hr = impl->closed ? RO_E_CLOSED : S_OK;
    LeaveCriticalSection( &impl->cs );
    return hr;
}

static HRESULT WINAPI animation_group_object_get_Properties( ICompositionObject *iface, ICompositionPropertySet **value )
{
    struct native_animation_group *impl = CONTAINING_RECORD( iface, struct native_animation_group, ICompositionObject_iface );

    if (!value) return E_POINTER;
    *value = NULL;
    EnterCriticalSection( &impl->cs );
    if (impl->closed) { LeaveCriticalSection( &impl->cs ); return RO_E_CLOSED; }
    *value = impl->properties;
    ICompositionPropertySet_AddRef( *value );
    LeaveCriticalSection( &impl->cs );
    return S_OK;
}

static HRESULT WINAPI animation_group_object_StartAnimation( ICompositionObject *iface, HSTRING name, ICompositionAnimation *animation )
{
    struct native_animation_group *impl = CONTAINING_RECORD( iface, struct native_animation_group, ICompositionObject_iface );
    HRESULT hr;

    EnterCriticalSection( &impl->cs );
    hr = impl->closed ? RO_E_CLOSED : E_NOTIMPL;
    LeaveCriticalSection( &impl->cs );
    return hr;
}

static HRESULT WINAPI animation_group_object_StopAnimation( ICompositionObject *iface, HSTRING name )
{
    struct native_animation_group *impl = CONTAINING_RECORD( iface, struct native_animation_group, ICompositionObject_iface );
    HRESULT hr;

    EnterCriticalSection( &impl->cs );
    hr = impl->closed ? RO_E_CLOSED : E_NOTIMPL;
    LeaveCriticalSection( &impl->cs );
    return hr;
}

static const ICompositionObjectVtbl animation_group_object_vtbl =
{
    animation_group_object_QueryInterface, animation_group_object_AddRef, animation_group_object_Release,
    animation_group_object_GetIids, animation_group_object_GetRuntimeClassName, animation_group_object_GetTrustLevel,
    animation_group_object_get_Compositor, animation_group_object_get_Dispatcher, animation_group_object_get_Properties,
    animation_group_object_StartAnimation, animation_group_object_StopAnimation,
};


static HRESULT WINAPI animation_group_closable_QueryInterface( IClosable *iface, REFIID iid, void **out)
{
    struct native_animation_group *impl = CONTAINING_RECORD( iface, struct native_animation_group, closable );

    return animation_group_QueryInterface( &impl->iface, iid, out );
}

static ULONG WINAPI animation_group_closable_AddRef( IClosable *iface)
{
    struct native_animation_group *impl = CONTAINING_RECORD( iface, struct native_animation_group, closable );

    return animation_group_AddRef( &impl->iface );
}

static ULONG WINAPI animation_group_closable_Release( IClosable *iface)
{
    struct native_animation_group *impl = CONTAINING_RECORD( iface, struct native_animation_group, closable );

    return animation_group_Release( &impl->iface );
}

static HRESULT WINAPI animation_group_closable_GetIids( IClosable *iface, ULONG *count, IID **iids)
{
    struct native_animation_group *impl = CONTAINING_RECORD( iface, struct native_animation_group, closable );

    return animation_group_GetIids( &impl->iface, count, iids );
}

static HRESULT WINAPI animation_group_closable_GetRuntimeClassName( IClosable *iface, HSTRING *name)
{
    struct native_animation_group *impl = CONTAINING_RECORD( iface, struct native_animation_group, closable );

    return animation_group_GetRuntimeClassName( &impl->iface, name );
}

static HRESULT WINAPI animation_group_closable_GetTrustLevel( IClosable *iface, TrustLevel *level)
{
    struct native_animation_group *impl = CONTAINING_RECORD( iface, struct native_animation_group, closable );

    return animation_group_GetTrustLevel( &impl->iface, level );
}

static HRESULT WINAPI animation_group_closable_Close( IClosable *iface )
{
    struct native_animation_group *impl = CONTAINING_RECORD( iface, struct native_animation_group, closable );
    struct animation_group_entry *entries;
    ICompositionPropertySet *properties;
    ICompositor *compositor;
    IClosable *child;
    HSTRING comment;
    IInspectable *implicit_animations;
    UINT32 count, i;

    EnterCriticalSection( &impl->cs );
    if (impl->closed) { LeaveCriticalSection( &impl->cs ); return S_OK; }
    impl->closed = TRUE;
    comment = impl->object2.comment;
    implicit_animations = impl->object2.implicit_animations;
    impl->object2.comment = NULL;
    impl->object2.implicit_animations = NULL;
    ++impl->generation;
    entries = impl->entries;
    count = impl->count;
    properties = impl->properties;
    compositor = impl->compositor;
    impl->entries = NULL;
    impl->count = 0;
    impl->properties = NULL;
    impl->compositor = NULL;
    LeaveCriticalSection( &impl->cs );
    for (i = 0; i < count; ++i)
    {
        if (SUCCEEDED(IUnknown_QueryInterface( (IUnknown *)entries[i].animation, &IID_IClosable, (void **)&child )))
        {
            IClosable_Close( child );
            IClosable_Release( child );
        }
    }
    animation_group_release_entries( entries, count );
    collection_object2_release_state( comment, implicit_animations );
    ICompositionPropertySet_Release( properties );
    ICompositor_Release( compositor );
    return S_OK;
}

static const IClosableVtbl animation_group_closable_vtbl =
{
    animation_group_closable_QueryInterface, animation_group_closable_AddRef, animation_group_closable_Release,
    animation_group_closable_GetIids, animation_group_closable_GetRuntimeClassName, animation_group_closable_GetTrustLevel, animation_group_closable_Close,
};

static HRESULT WINAPI compositor2_CreateAnimationGroup( struct compositor2_iface *iface, IInspectable **value )
{
    struct compositor *compositor = impl_from_compositor2( iface );
    struct native_animation_group *impl;
    HRESULT hr;

    if (!value) return E_POINTER;
    *value = NULL;
    if (!(impl = calloc( 1, sizeof(*impl) ))) return E_OUTOFMEMORY;
    hr = composition_property_set_create( &compositor->ICompositor_iface, &impl->properties );
    if (FAILED(hr)) { free( impl ); return hr; }
    impl->iface.lpVtbl = &animation_group_vtbl;
    impl->iterable.lpVtbl = &animation_iterable_vtbl;
    impl->ICompositionObject_iface.lpVtbl = &animation_group_object_vtbl;
    impl->compositor = &compositor->ICompositor_iface;
    ICompositor_AddRef( impl->compositor );
    impl->closable.lpVtbl = &animation_group_closable_vtbl;
    impl->object2.iface.lpVtbl = &collection_object2_vtbl;
    impl->object2.object = &impl->ICompositionObject_iface;
    impl->object2.cs = &impl->cs;
    impl->object2.closed = &impl->closed;
    impl->ref = 1;
    InitializeCriticalSection( &impl->cs );
    *value = (IInspectable *)&impl->iface;
    return S_OK;
}

DEFINE_COMPOSITOR2_CREATE_METHOD(CreateBackdropBrush)
DEFINE_COMPOSITOR2_CREATE_METHOD(CreateDistantLight)
DEFINE_COMPOSITOR2_CREATE_METHOD(CreateDropShadow)
static const GUID implicit_pair_iid = {0x3b9a274f,0x891d,0x5208,{0x96,0xbb,0xe3,0xf4,0x0c,0x80,0x9e,0x9b}};
static const GUID implicit_iterator_iid = {0x85924e77,0xfc46,0x5a0d,{0xbd,0xaf,0x46,0x3c,0x4f,0x86,0xc1,0x72}};
static const GUID implicit_iterable_iid = {0xc739c420,0x64b2,0x53f2,{0x89,0xb9,0x05,0x90,0x6a,0xba,0x7c,0xb0}};
static const GUID implicit_view_iid = {0xb65e6ece,0xd9af,0x5d4a,{0xab,0xaa,0xc0,0x8e,0x34,0xa5,0xf8,0x15}};
#define IMPLICIT_INSPECTABLE(type) \
    HRESULT (WINAPI *QueryInterface)(type *,REFIID,void **); \
    ULONG (WINAPI *AddRef)(type *); \
    ULONG (WINAPI *Release)(type *); \
    HRESULT (WINAPI *GetIids)(type *,ULONG *,IID **); \
    HRESULT (WINAPI *GetRuntimeClassName)(type *,HSTRING *); \
    HRESULT (WINAPI *GetTrustLevel)(type *,TrustLevel *)
struct implicit_pair_iface;
struct implicit_pair_vtbl
{
    IMPLICIT_INSPECTABLE(struct implicit_pair_iface);
    HRESULT (WINAPI *get_Key)(struct implicit_pair_iface *,HSTRING *);
    HRESULT (WINAPI *get_Value)(struct implicit_pair_iface *,ICompositionAnimationBase **);
};
struct implicit_pair_iface { const struct implicit_pair_vtbl *lpVtbl; };
struct implicit_iterator_iface;
struct implicit_iterator_vtbl
{
    IMPLICIT_INSPECTABLE(struct implicit_iterator_iface);
    HRESULT (WINAPI *get_Current)(struct implicit_iterator_iface *,struct implicit_pair_iface **);
    HRESULT (WINAPI *get_HasCurrent)(struct implicit_iterator_iface *,boolean *);
    HRESULT (WINAPI *MoveNext)(struct implicit_iterator_iface *,boolean *);
    HRESULT (WINAPI *GetMany)(struct implicit_iterator_iface *,UINT32,struct implicit_pair_iface **,UINT32 *);
};
struct implicit_iterator_iface { const struct implicit_iterator_vtbl *lpVtbl; };
struct implicit_iterable_iface;
struct implicit_iterable_vtbl
{
    IMPLICIT_INSPECTABLE(struct implicit_iterable_iface);
    HRESULT (WINAPI *First)(struct implicit_iterable_iface *,struct implicit_iterator_iface **);
};
struct implicit_iterable_iface { const struct implicit_iterable_vtbl *lpVtbl; };
struct implicit_view_iface;
struct implicit_view_vtbl
{
    IMPLICIT_INSPECTABLE(struct implicit_view_iface);
    HRESULT (WINAPI *Lookup)(struct implicit_view_iface *,HSTRING,ICompositionAnimationBase **);
    HRESULT (WINAPI *get_Size)(struct implicit_view_iface *,UINT32 *);
    HRESULT (WINAPI *HasKey)(struct implicit_view_iface *,HSTRING,boolean *);
    HRESULT (WINAPI *Split)(struct implicit_view_iface *,struct implicit_view_iface **,struct implicit_view_iface **);
};
struct implicit_view_iface { const struct implicit_view_vtbl *lpVtbl; };
#undef IMPLICIT_INSPECTABLE

struct implicit_animation_pair
{
    struct implicit_pair_iface iface;
    LONG ref;
    HSTRING key;
    ICompositionAnimationBase *value;
};

struct implicit_animation_collection
{
    IInspectable iface;
    struct implicit_map_iface map;
    struct implicit_iterable_iface iterable;
    ICompositionObject object;
    IClosable closable;
    struct collection_object2_state object2;
    BOOL closed;
    LONG ref;
    CRITICAL_SECTION cs;
    ICompositor *compositor;
    ICompositionPropertySet *properties;
    struct implicit_animation_pair **entries;
    UINT32 count, generation;
};

struct implicit_animation_view
{
    struct implicit_view_iface iface;
    struct implicit_iterable_iface iterable;
    LONG ref;
    struct implicit_animation_pair **entries;
    UINT32 count;
};

struct implicit_animation_iterator
{
    struct implicit_iterator_iface iface;
    LONG ref;
    struct implicit_animation_collection *collection;
    struct implicit_animation_view *view;
    UINT32 index, generation;
};

static ULONG WINAPI implicit_pair_AddRef( struct implicit_pair_iface *iface )
{
    return InterlockedIncrement( &CONTAINING_RECORD( iface, struct implicit_animation_pair, iface )->ref );
}

static ULONG WINAPI implicit_pair_Release( struct implicit_pair_iface *iface )
{
    struct implicit_animation_pair *impl = CONTAINING_RECORD( iface, struct implicit_animation_pair, iface );
    ULONG ref = InterlockedDecrement( &impl->ref );

    if (!ref)
    {
        WindowsDeleteString( impl->key );
        ICompositionAnimationBase_Release( impl->value );
        free( impl );
    }
    return ref;
}

static HRESULT WINAPI implicit_pair_QueryInterface( struct implicit_pair_iface *iface, REFIID iid, void **out )
{
    if (!out) return E_POINTER;
    *out = NULL;
    if (!IsEqualGUID( iid, &IID_IUnknown ) && !IsEqualGUID( iid, &IID_IInspectable ) &&
            !IsEqualGUID( iid, &implicit_pair_iid )) return E_NOINTERFACE;
    *out = iface;
    implicit_pair_AddRef( iface );
    return S_OK;
}

static HRESULT implicit_single_iid( REFIID iid, ULONG *count, IID **iids )
{
    if (!count || !iids) return E_POINTER;
    *count = 0;
    *iids = CoTaskMemAlloc( sizeof(**iids) );
    if (!*iids) return E_OUTOFMEMORY;
    **iids = *iid;
    *count = 1;
    return S_OK;
}

static HRESULT WINAPI implicit_pair_GetIids( struct implicit_pair_iface *iface, ULONG *count, IID **iids )
{
    return implicit_single_iid( &implicit_pair_iid, count, iids );
}

static HRESULT WINAPI implicit_pair_GetRuntimeClassName( struct implicit_pair_iface *iface, HSTRING *name )
{
    if (!name) return E_POINTER;
    *name = NULL;
    return S_OK;
}

static HRESULT WINAPI implicit_pair_GetTrustLevel( struct implicit_pair_iface *iface, TrustLevel *level )
{
    if (!level) return E_POINTER;
    *level = BaseTrust;
    return S_OK;
}

static HRESULT WINAPI implicit_pair_get_Key( struct implicit_pair_iface *iface, HSTRING *key )
{
    struct implicit_animation_pair *impl = CONTAINING_RECORD( iface, struct implicit_animation_pair, iface );

    if (!key) return E_POINTER;
    return WindowsDuplicateString( impl->key, key );
}

static HRESULT WINAPI implicit_pair_get_Value( struct implicit_pair_iface *iface, ICompositionAnimationBase **value )
{
    struct implicit_animation_pair *impl = CONTAINING_RECORD( iface, struct implicit_animation_pair, iface );

    if (!value) return E_POINTER;
    *value = impl->value;
    ICompositionAnimationBase_AddRef( *value );
    return S_OK;
}

static const struct implicit_pair_vtbl implicit_pair_vtbl =
{
    implicit_pair_QueryInterface, implicit_pair_AddRef, implicit_pair_Release,
    implicit_pair_GetIids, implicit_pair_GetRuntimeClassName, implicit_pair_GetTrustLevel,
    implicit_pair_get_Key, implicit_pair_get_Value,
};

static HRESULT implicit_pair_create( HSTRING key, ICompositionAnimationBase *value,
        struct implicit_animation_pair **out )
{
    struct implicit_animation_pair *impl;
    HRESULT hr;

    *out = NULL;
    if (!(impl = calloc( 1, sizeof(*impl) ))) return E_OUTOFMEMORY;
    if (FAILED(hr = WindowsDuplicateString( key, &impl->key ))) { free( impl ); return hr; }
    impl->iface.lpVtbl = &implicit_pair_vtbl;
    impl->ref = 1;
    impl->value = value;
    ICompositionAnimationBase_AddRef( value );
    *out = impl;
    return S_OK;
}

static void implicit_entries_release( struct implicit_animation_pair **entries, UINT32 count )
{
    UINT32 i;

    for (i = 0; i < count; ++i) implicit_pair_Release( &entries[i]->iface );
    free( entries );
}

static UINT32 implicit_entries_find( struct implicit_animation_pair **entries, UINT32 count, HSTRING key )
{
    UINT32 i;
    INT32 order;

    for (i = 0; i < count; ++i)
        if (SUCCEEDED(WindowsCompareStringOrdinal( entries[i]->key, key, &order )) && !order) break;
    return i;
}

static HRESULT WINAPI implicit_collection_QueryInterface( IInspectable *iface, REFIID iid, void **out )
{
    struct implicit_animation_collection *impl = CONTAINING_RECORD( iface, struct implicit_animation_collection, iface );

    if (!out) return E_POINTER;
    *out = NULL;
    if (IsEqualGUID( iid, &IID_IUnknown ) || IsEqualGUID( iid, &IID_IInspectable ) ||
            IsEqualGUID( iid, &implicit_collection_iid )) *out = &impl->iface;
    else if (IsEqualGUID( iid, &implicit_map_iid )) *out = &impl->map;
    else if (IsEqualGUID( iid, &implicit_iterable_iid )) *out = &impl->iterable;
    else if (IsEqualGUID( iid, &IID_ICompositionObject )) *out = &impl->object;
    else if (IsEqualGUID( iid, &IID_IClosable )) *out = &impl->closable;
    else if (IsEqualGUID( iid, &composition_object2_iid )) *out = &impl->object2.iface;
    else return E_NOINTERFACE;
    InterlockedIncrement( &impl->ref );
    return S_OK;
}

static ULONG WINAPI implicit_collection_AddRef( IInspectable *iface )
{
    return InterlockedIncrement( &CONTAINING_RECORD( iface, struct implicit_animation_collection, iface )->ref );
}

static ULONG WINAPI implicit_collection_Release( IInspectable *iface )
{
    struct implicit_animation_collection *impl = CONTAINING_RECORD( iface, struct implicit_animation_collection, iface );
    ULONG ref = InterlockedDecrement( &impl->ref );

    if (!ref)
    {
        implicit_entries_release( impl->entries, impl->count );
        if (impl->properties) ICompositionPropertySet_Release( impl->properties );
        if (impl->compositor) ICompositor_Release( impl->compositor );
        collection_object2_release_state( impl->object2.comment, impl->object2.implicit_animations );
        DeleteCriticalSection( &impl->cs );
        free( impl );
    }
    return ref;
}

static HRESULT WINAPI implicit_collection_GetIids( IInspectable *iface, ULONG *count, IID **iids )
{
    if (!count || !iids) return E_POINTER;
    *count = 0;
    *iids = CoTaskMemAlloc( 6 * sizeof(**iids) );
    if (!*iids) return E_OUTOFMEMORY;
    (*iids)[0] = implicit_collection_iid;
    (*iids)[1] = implicit_map_iid;
    (*iids)[2] = implicit_iterable_iid;
    (*iids)[3] = IID_ICompositionObject;
    (*iids)[4] = IID_IClosable;
    (*iids)[5] = composition_object2_iid;
    *count = 6;
    return S_OK;
}

static HRESULT WINAPI implicit_collection_GetRuntimeClassName( IInspectable *iface, HSTRING *name )
{
    static const WCHAR class_name[] = L"Windows.UI.Composition.ImplicitAnimationCollection";

    if (!name) return E_POINTER;
    return WindowsCreateString( class_name, ARRAY_SIZE(class_name) - 1, name );
}

static HRESULT WINAPI implicit_collection_GetTrustLevel( IInspectable *iface, TrustLevel *level )
{
    if (!level) return E_POINTER;
    *level = BaseTrust;
    return S_OK;
}

static const IInspectableVtbl implicit_collection_vtbl =
{
    implicit_collection_QueryInterface, implicit_collection_AddRef, implicit_collection_Release,
    implicit_collection_GetIids, implicit_collection_GetRuntimeClassName, implicit_collection_GetTrustLevel,
};

static HRESULT WINAPI implicit_map_Lookup( struct implicit_map_iface *iface, HSTRING key, ICompositionAnimationBase **value )
{
    struct implicit_animation_collection *impl = CONTAINING_RECORD( iface, struct implicit_animation_collection, map );
    UINT32 index;
    HRESULT hr;

    if (!value) return E_POINTER;
    *value = NULL;
    EnterCriticalSection( &impl->cs );
    *value = NULL;
    if (impl->closed) { LeaveCriticalSection( &impl->cs ); return RO_E_CLOSED; }
    index = implicit_entries_find( impl->entries, impl->count, key );
    hr = index < impl->count ? implicit_pair_get_Value( &impl->entries[index]->iface, value ) : E_BOUNDS;
    LeaveCriticalSection( &impl->cs );
    return hr;
}

static HRESULT WINAPI implicit_map_get_Size( struct implicit_map_iface *iface, UINT32 *count )
{
    struct implicit_animation_collection *impl = CONTAINING_RECORD( iface, struct implicit_animation_collection, map );

    if (!count) return E_POINTER;
    EnterCriticalSection( &impl->cs );
    *count = 0;
    if (impl->closed) { LeaveCriticalSection( &impl->cs ); return RO_E_CLOSED; }
    *count = impl->count;
    LeaveCriticalSection( &impl->cs );
    return S_OK;
}

static HRESULT WINAPI implicit_map_HasKey( struct implicit_map_iface *iface, HSTRING key, boolean *value )
{
    struct implicit_animation_collection *impl = CONTAINING_RECORD( iface, struct implicit_animation_collection, map );

    if (!value) return E_POINTER;
    EnterCriticalSection( &impl->cs );
    *value = FALSE;
    if (impl->closed) { LeaveCriticalSection( &impl->cs ); return RO_E_CLOSED; }
    *value = implicit_entries_find( impl->entries, impl->count, key ) < impl->count;
    LeaveCriticalSection( &impl->cs );
    return S_OK;
}

static HRESULT WINAPI implicit_map_Insert( struct implicit_map_iface *iface, HSTRING key,
        ICompositionAnimationBase *value, boolean *replaced )
{
    struct implicit_animation_collection *impl = CONTAINING_RECORD( iface, struct implicit_animation_collection, map );
    struct implicit_animation_pair *pair, *old = NULL, **entries;
    ICompositionObject *object;
    ICompositor *compositor, *parent;
    IUnknown *identity, *expected;
    UINT32 index;
    SIZE_T entry_count;
    HRESULT hr;

    if (!replaced) return E_POINTER;
    *replaced = FALSE;
    if (!value || !WindowsGetStringLen( key )) return E_INVALIDARG;
    if (FAILED(hr = ICompositionAnimationBase_QueryInterface( value, &IID_ICompositionObject, (void **)&object ))) return hr;
    hr = ICompositionObject_get_Compositor( object, &compositor );
    ICompositionObject_Release( object );
    if (FAILED(hr)) return hr;
    if (!compositor) return E_INVALIDARG;
    hr = ICompositor_QueryInterface( compositor, &IID_IUnknown, (void **)&identity );
    ICompositor_Release( compositor );
    if (FAILED(hr)) return hr;
    EnterCriticalSection( &impl->cs );
    parent = impl->compositor;
    if (parent) ICompositor_AddRef( parent );
    LeaveCriticalSection( &impl->cs );
    if (!parent) { IUnknown_Release( identity ); return RO_E_CLOSED; }
    hr = ICompositor_QueryInterface( parent, &IID_IUnknown, (void **)&expected );
    ICompositor_Release( parent );
    if (FAILED(hr)) { IUnknown_Release( identity ); return hr; }
    hr = identity == expected ? S_OK : E_INVALIDARG;
    IUnknown_Release( identity );
    IUnknown_Release( expected );
    if (FAILED(hr)) return hr;
    if (FAILED(hr = implicit_pair_create( key, value, &pair ))) return hr;
    EnterCriticalSection( &impl->cs );
    if (impl->closed) { LeaveCriticalSection( &impl->cs ); implicit_pair_Release( &pair->iface ); return RO_E_CLOSED; }
    index = implicit_entries_find( impl->entries, impl->count, key );
    if (index < impl->count)
    {
        old = impl->entries[index];
        impl->entries[index] = pair;
        *replaced = TRUE;
        ++impl->generation;
    }
    else if (impl->count == UINT_MAX ||
            (entry_count = (SIZE_T)impl->count + 1) > SIZE_MAX / sizeof(*entries)) hr = E_OUTOFMEMORY;
    else if (!(entries = realloc( impl->entries, entry_count * sizeof(*entries) ))) hr = E_OUTOFMEMORY;
    else
    {
        impl->entries = entries;
        impl->entries[impl->count++] = pair;
        ++impl->generation;
    }
    LeaveCriticalSection( &impl->cs );
    if (old) implicit_pair_Release( &old->iface );
    if (FAILED(hr)) implicit_pair_Release( &pair->iface );
    return hr;
}

static HRESULT WINAPI implicit_map_Remove( struct implicit_map_iface *iface, HSTRING key )
{
    struct implicit_animation_collection *impl = CONTAINING_RECORD( iface, struct implicit_animation_collection, map );
    struct implicit_animation_pair *old = NULL;
    UINT32 index;

    EnterCriticalSection( &impl->cs );
    if (impl->closed) { LeaveCriticalSection( &impl->cs ); return RO_E_CLOSED; }
    index = implicit_entries_find( impl->entries, impl->count, key );
    if (index < impl->count)
    {
        old = impl->entries[index];
        memmove( impl->entries + index, impl->entries + index + 1, (--impl->count - index) * sizeof(*impl->entries) );
        ++impl->generation;
    }
    LeaveCriticalSection( &impl->cs );
    if (!old) return E_BOUNDS;
    implicit_pair_Release( &old->iface );
    return S_OK;
}

static HRESULT WINAPI implicit_map_Clear( struct implicit_map_iface *iface )
{
    struct implicit_animation_collection *impl = CONTAINING_RECORD( iface, struct implicit_animation_collection, map );
    struct implicit_animation_pair **entries;
    UINT32 count;

    EnterCriticalSection( &impl->cs );
    if (impl->closed) { LeaveCriticalSection( &impl->cs ); return RO_E_CLOSED; }
    entries = impl->entries;
    count = impl->count;
    impl->entries = NULL;
    impl->count = 0;
    if (count) ++impl->generation;
    LeaveCriticalSection( &impl->cs );
    implicit_entries_release( entries, count );
    return S_OK;
}
static ULONG WINAPI implicit_view_AddRef( struct implicit_view_iface *iface )
{
    return InterlockedIncrement( &CONTAINING_RECORD( iface, struct implicit_animation_view, iface )->ref );
}

static ULONG WINAPI implicit_view_Release( struct implicit_view_iface *iface )
{
    struct implicit_animation_view *impl = CONTAINING_RECORD( iface, struct implicit_animation_view, iface );
    ULONG ref = InterlockedDecrement( &impl->ref );

    if (!ref)
    {
        implicit_entries_release( impl->entries, impl->count );
        free( impl );
    }
    return ref;
}

static HRESULT WINAPI implicit_view_QueryInterface( struct implicit_view_iface *iface, REFIID iid, void **out )
{
    struct implicit_animation_view *impl = CONTAINING_RECORD( iface, struct implicit_animation_view, iface );

    if (!out) return E_POINTER;
    *out = NULL;
    if (IsEqualGUID( iid, &IID_IUnknown ) || IsEqualGUID( iid, &IID_IInspectable ) ||
            IsEqualGUID( iid, &implicit_view_iid )) *out = &impl->iface;
    else if (IsEqualGUID( iid, &implicit_iterable_iid )) *out = &impl->iterable;
    else return E_NOINTERFACE;
    implicit_view_AddRef( iface );
    return S_OK;
}

static HRESULT WINAPI implicit_view_GetIids( struct implicit_view_iface *iface, ULONG *count, IID **iids )
{
    if (!count || !iids) return E_POINTER;
    *count = 0;
    *iids = CoTaskMemAlloc( 2 * sizeof(**iids) );
    if (!*iids) return E_OUTOFMEMORY;
    (*iids)[0] = implicit_view_iid;
    (*iids)[1] = implicit_iterable_iid;
    *count = 2;
    return S_OK;
}

static HRESULT WINAPI implicit_view_GetRuntimeClassName( struct implicit_view_iface *iface, HSTRING *name )
{
    if (!name) return E_POINTER;
    *name = NULL;
    return S_OK;
}

static HRESULT WINAPI implicit_view_GetTrustLevel( struct implicit_view_iface *iface, TrustLevel *level )
{
    if (!level) return E_POINTER;
    *level = BaseTrust;
    return S_OK;
}

static HRESULT WINAPI implicit_view_Lookup( struct implicit_view_iface *iface, HSTRING key, ICompositionAnimationBase **value )
{
    struct implicit_animation_view *impl = CONTAINING_RECORD( iface, struct implicit_animation_view, iface );
    UINT32 index;

    if (!value) return E_POINTER;
    *value = NULL;
    index = implicit_entries_find( impl->entries, impl->count, key );
    return index < impl->count ? implicit_pair_get_Value( &impl->entries[index]->iface, value ) : E_BOUNDS;
}

static HRESULT WINAPI implicit_view_get_Size( struct implicit_view_iface *iface, UINT32 *count )
{
    if (!count) return E_POINTER;
    *count = CONTAINING_RECORD( iface, struct implicit_animation_view, iface )->count;
    return S_OK;
}

static HRESULT WINAPI implicit_view_HasKey( struct implicit_view_iface *iface, HSTRING key, boolean *value )
{
    struct implicit_animation_view *impl = CONTAINING_RECORD( iface, struct implicit_animation_view, iface );

    if (!value) return E_POINTER;
    *value = implicit_entries_find( impl->entries, impl->count, key ) < impl->count;
    return S_OK;
}

static HRESULT implicit_view_create( struct implicit_animation_pair **entries, UINT32 count,
        struct implicit_view_iface **out );

static HRESULT WINAPI implicit_view_Split( struct implicit_view_iface *iface,
        struct implicit_view_iface **first, struct implicit_view_iface **second )
{
    struct implicit_animation_view *impl = CONTAINING_RECORD( iface, struct implicit_animation_view, iface );
    HRESULT hr;
    UINT32 middle;

    if (!first || !second) return E_POINTER;
    *first = *second = NULL;
    if (first == second) return E_INVALIDARG;
    if (impl->count < 2) return S_OK;
    middle = impl->count / 2;
    if (FAILED(hr = implicit_view_create( impl->entries, middle, first ))) return hr;
    if (FAILED(hr = implicit_view_create( impl->entries + middle, impl->count - middle, second )))
    {
        (*first)->lpVtbl->Release( *first );
        *first = NULL;
    }
    return hr;
}

static const struct implicit_view_vtbl implicit_view_vtbl =
{
    implicit_view_QueryInterface, implicit_view_AddRef, implicit_view_Release,
    implicit_view_GetIids, implicit_view_GetRuntimeClassName, implicit_view_GetTrustLevel,
    implicit_view_Lookup, implicit_view_get_Size, implicit_view_HasKey, implicit_view_Split,
};

static ULONG WINAPI implicit_iterator_AddRef( struct implicit_iterator_iface *iface )
{
    return InterlockedIncrement( &CONTAINING_RECORD( iface, struct implicit_animation_iterator, iface )->ref );
}

static ULONG WINAPI implicit_iterator_Release( struct implicit_iterator_iface *iface )
{
    struct implicit_animation_iterator *impl = CONTAINING_RECORD( iface, struct implicit_animation_iterator, iface );
    ULONG ref = InterlockedDecrement( &impl->ref );

    if (!ref)
    {
        if (impl->collection) implicit_collection_Release( &impl->collection->iface );
        else implicit_view_Release( &impl->view->iface );
        free( impl );
    }
    return ref;
}

static HRESULT WINAPI implicit_iterator_QueryInterface( struct implicit_iterator_iface *iface, REFIID iid, void **out )
{
    if (!out) return E_POINTER;
    *out = NULL;
    if (!IsEqualGUID( iid, &IID_IUnknown ) && !IsEqualGUID( iid, &IID_IInspectable ) &&
            !IsEqualGUID( iid, &implicit_iterator_iid )) return E_NOINTERFACE;
    *out = iface;
    implicit_iterator_AddRef( iface );
    return S_OK;
}

static HRESULT WINAPI implicit_iterator_GetIids( struct implicit_iterator_iface *iface, ULONG *count, IID **iids )
{
    return implicit_single_iid( &implicit_iterator_iid, count, iids );
}

static HRESULT WINAPI implicit_iterator_GetRuntimeClassName( struct implicit_iterator_iface *iface, HSTRING *name )
{
    if (!name) return E_POINTER;
    *name = NULL;
    return S_OK;
}

static HRESULT WINAPI implicit_iterator_GetTrustLevel( struct implicit_iterator_iface *iface, TrustLevel *level )
{
    if (!level) return E_POINTER;
    *level = BaseTrust;
    return S_OK;
}

static HRESULT implicit_iterator_lock( struct implicit_animation_iterator *impl,
        struct implicit_animation_pair ***entries, UINT32 *count )
{
    if (impl->collection)
    {
        EnterCriticalSection( &impl->collection->cs );
        if (impl->collection->closed)
        {
            LeaveCriticalSection( &impl->collection->cs );
            return RO_E_CLOSED;
        }
        if (impl->generation != impl->collection->generation)
        {
            LeaveCriticalSection( &impl->collection->cs );
            return E_CHANGED_STATE;
        }
        *entries = impl->collection->entries;
        *count = impl->collection->count;
    }
    else
    {
        *entries = impl->view->entries;
        *count = impl->view->count;
    }
    return S_OK;
}

static void implicit_iterator_unlock( struct implicit_animation_iterator *impl )
{
    if (impl->collection) LeaveCriticalSection( &impl->collection->cs );
}

static HRESULT WINAPI implicit_iterator_get_Current( struct implicit_iterator_iface *iface, struct implicit_pair_iface **value )
{
    struct implicit_animation_iterator *impl = CONTAINING_RECORD( iface, struct implicit_animation_iterator, iface );
    struct implicit_animation_pair **entries;
    UINT32 count;
    HRESULT hr;

    if (!value) return E_POINTER;
    *value = NULL;
    if (FAILED(hr = implicit_iterator_lock( impl, &entries, &count ))) return hr;
    if (impl->index >= count) hr = E_BOUNDS;
    else
    {
        *value = &entries[impl->index]->iface;
        implicit_pair_AddRef( *value );
    }
    implicit_iterator_unlock( impl );
    return hr;
}

static HRESULT WINAPI implicit_iterator_get_HasCurrent( struct implicit_iterator_iface *iface, boolean *value )
{
    struct implicit_animation_iterator *impl = CONTAINING_RECORD( iface, struct implicit_animation_iterator, iface );
    struct implicit_animation_pair **entries;
    UINT32 count;
    HRESULT hr;

    if (!value) return E_POINTER;
    *value = FALSE;
    if (FAILED(hr = implicit_iterator_lock( impl, &entries, &count ))) return hr;
    *value = impl->index < count;
    implicit_iterator_unlock( impl );
    return S_OK;
}

static HRESULT WINAPI implicit_iterator_MoveNext( struct implicit_iterator_iface *iface, boolean *value )
{
    struct implicit_animation_iterator *impl = CONTAINING_RECORD( iface, struct implicit_animation_iterator, iface );
    struct implicit_animation_pair **entries;
    UINT32 count;
    HRESULT hr;

    if (!value) return E_POINTER;
    *value = FALSE;
    if (FAILED(hr = implicit_iterator_lock( impl, &entries, &count ))) return hr;
    if (impl->index < count) ++impl->index;
    *value = impl->index < count;
    implicit_iterator_unlock( impl );
    return S_OK;
}

static HRESULT WINAPI implicit_iterator_GetMany( struct implicit_iterator_iface *iface, UINT32 capacity,
        struct implicit_pair_iface **values, UINT32 *fetched )
{
    struct implicit_animation_iterator *impl = CONTAINING_RECORD( iface, struct implicit_animation_iterator, iface );
    struct implicit_animation_pair **entries;
    UINT32 count;
    HRESULT hr;

    if (!fetched) return E_POINTER;
    *fetched = 0;
    if (capacity && !values) return E_POINTER;
    if (FAILED(hr = implicit_iterator_lock( impl, &entries, &count ))) return hr;
    while (*fetched < capacity && impl->index < count)
    {
        values[*fetched] = &entries[impl->index++]->iface;
        implicit_pair_AddRef( values[(*fetched)++] );
    }
    implicit_iterator_unlock( impl );
    return S_OK;
}

static const struct implicit_iterator_vtbl implicit_iterator_vtbl =
{
    implicit_iterator_QueryInterface, implicit_iterator_AddRef, implicit_iterator_Release,
    implicit_iterator_GetIids, implicit_iterator_GetRuntimeClassName, implicit_iterator_GetTrustLevel,
    implicit_iterator_get_Current, implicit_iterator_get_HasCurrent, implicit_iterator_MoveNext, implicit_iterator_GetMany,
};

static HRESULT implicit_iterator_create( struct implicit_animation_collection *collection,
        struct implicit_animation_view *view, struct implicit_iterator_iface **value )
{
    struct implicit_animation_iterator *impl;

    if (!value) return E_POINTER;
    *value = NULL;
    if (!(impl = calloc( 1, sizeof(*impl) ))) return E_OUTOFMEMORY;
    impl->iface.lpVtbl = &implicit_iterator_vtbl;
    impl->ref = 1;
    impl->collection = collection;
    impl->view = view;
    if (collection)
    {
        implicit_collection_AddRef( &collection->iface );
        EnterCriticalSection( &collection->cs );
        if (collection->closed)
        {
            LeaveCriticalSection( &collection->cs );
            implicit_collection_Release( &collection->iface );
            free( impl );
            return RO_E_CLOSED;
        }
        impl->generation = collection->generation;
        LeaveCriticalSection( &collection->cs );
    }
    else implicit_view_AddRef( &view->iface );
    *value = &impl->iface;
    return S_OK;
}

static HRESULT WINAPI implicit_iterable_First( struct implicit_iterable_iface *iface, struct implicit_iterator_iface **value )
{
    return implicit_iterator_create( CONTAINING_RECORD( iface, struct implicit_animation_collection, iterable ), NULL, value );
}

static HRESULT WINAPI implicit_view_iterable_First( struct implicit_iterable_iface *iface, struct implicit_iterator_iface **value )
{
    return implicit_iterator_create( NULL, CONTAINING_RECORD( iface, struct implicit_animation_view, iterable ), value );
}

static HRESULT WINAPI implicit_map_QueryInterface( struct implicit_map_iface *iface, REFIID iid, void **out)
{
    struct implicit_animation_collection *impl = CONTAINING_RECORD( iface, struct implicit_animation_collection, map );

    return implicit_collection_QueryInterface( &impl->iface, iid, out );
}

static ULONG WINAPI implicit_map_AddRef( struct implicit_map_iface *iface)
{
    struct implicit_animation_collection *impl = CONTAINING_RECORD( iface, struct implicit_animation_collection, map );

    return implicit_collection_AddRef( &impl->iface );
}

static ULONG WINAPI implicit_map_Release( struct implicit_map_iface *iface)
{
    struct implicit_animation_collection *impl = CONTAINING_RECORD( iface, struct implicit_animation_collection, map );

    return implicit_collection_Release( &impl->iface );
}

static HRESULT WINAPI implicit_map_GetIids( struct implicit_map_iface *iface, ULONG *count, IID **iids)
{
    struct implicit_animation_collection *impl = CONTAINING_RECORD( iface, struct implicit_animation_collection, map );

    return implicit_collection_GetIids( &impl->iface, count, iids );
}

static HRESULT WINAPI implicit_map_GetRuntimeClassName( struct implicit_map_iface *iface, HSTRING *name)
{
    struct implicit_animation_collection *impl = CONTAINING_RECORD( iface, struct implicit_animation_collection, map );

    return implicit_collection_GetRuntimeClassName( &impl->iface, name );
}

static HRESULT WINAPI implicit_map_GetTrustLevel( struct implicit_map_iface *iface, TrustLevel *level)
{
    struct implicit_animation_collection *impl = CONTAINING_RECORD( iface, struct implicit_animation_collection, map );

    return implicit_collection_GetTrustLevel( &impl->iface, level );
}

static HRESULT WINAPI implicit_iterable_QueryInterface( struct implicit_iterable_iface *iface, REFIID iid, void **out)
{
    struct implicit_animation_collection *impl = CONTAINING_RECORD( iface, struct implicit_animation_collection, iterable );

    return implicit_collection_QueryInterface( &impl->iface, iid, out );
}

static ULONG WINAPI implicit_iterable_AddRef( struct implicit_iterable_iface *iface)
{
    struct implicit_animation_collection *impl = CONTAINING_RECORD( iface, struct implicit_animation_collection, iterable );

    return implicit_collection_AddRef( &impl->iface );
}

static ULONG WINAPI implicit_iterable_Release( struct implicit_iterable_iface *iface)
{
    struct implicit_animation_collection *impl = CONTAINING_RECORD( iface, struct implicit_animation_collection, iterable );

    return implicit_collection_Release( &impl->iface );
}

static HRESULT WINAPI implicit_iterable_GetIids( struct implicit_iterable_iface *iface, ULONG *count, IID **iids)
{
    struct implicit_animation_collection *impl = CONTAINING_RECORD( iface, struct implicit_animation_collection, iterable );

    return implicit_collection_GetIids( &impl->iface, count, iids );
}

static HRESULT WINAPI implicit_iterable_GetRuntimeClassName( struct implicit_iterable_iface *iface, HSTRING *name)
{
    struct implicit_animation_collection *impl = CONTAINING_RECORD( iface, struct implicit_animation_collection, iterable );

    return implicit_collection_GetRuntimeClassName( &impl->iface, name );
}

static HRESULT WINAPI implicit_iterable_GetTrustLevel( struct implicit_iterable_iface *iface, TrustLevel *level)
{
    struct implicit_animation_collection *impl = CONTAINING_RECORD( iface, struct implicit_animation_collection, iterable );

    return implicit_collection_GetTrustLevel( &impl->iface, level );
}

static HRESULT WINAPI implicit_object_QueryInterface( ICompositionObject *iface, REFIID iid, void **out)
{
    struct implicit_animation_collection *impl = CONTAINING_RECORD( iface, struct implicit_animation_collection, object );

    return implicit_collection_QueryInterface( &impl->iface, iid, out );
}

static ULONG WINAPI implicit_object_AddRef( ICompositionObject *iface)
{
    struct implicit_animation_collection *impl = CONTAINING_RECORD( iface, struct implicit_animation_collection, object );

    return implicit_collection_AddRef( &impl->iface );
}

static ULONG WINAPI implicit_object_Release( ICompositionObject *iface)
{
    struct implicit_animation_collection *impl = CONTAINING_RECORD( iface, struct implicit_animation_collection, object );

    return implicit_collection_Release( &impl->iface );
}

static HRESULT WINAPI implicit_object_GetIids( ICompositionObject *iface, ULONG *count, IID **iids)
{
    struct implicit_animation_collection *impl = CONTAINING_RECORD( iface, struct implicit_animation_collection, object );

    return implicit_collection_GetIids( &impl->iface, count, iids );
}

static HRESULT WINAPI implicit_object_GetRuntimeClassName( ICompositionObject *iface, HSTRING *name)
{
    struct implicit_animation_collection *impl = CONTAINING_RECORD( iface, struct implicit_animation_collection, object );

    return implicit_collection_GetRuntimeClassName( &impl->iface, name );
}

static HRESULT WINAPI implicit_object_GetTrustLevel( ICompositionObject *iface, TrustLevel *level)
{
    struct implicit_animation_collection *impl = CONTAINING_RECORD( iface, struct implicit_animation_collection, object );

    return implicit_collection_GetTrustLevel( &impl->iface, level );
}

static HRESULT WINAPI implicit_view_iterable_QueryInterface( struct implicit_iterable_iface *iface, REFIID iid, void **out)
{
    struct implicit_animation_view *impl = CONTAINING_RECORD( iface, struct implicit_animation_view, iterable );

    return implicit_view_QueryInterface( &impl->iface, iid, out );
}

static ULONG WINAPI implicit_view_iterable_AddRef( struct implicit_iterable_iface *iface)
{
    struct implicit_animation_view *impl = CONTAINING_RECORD( iface, struct implicit_animation_view, iterable );

    return implicit_view_AddRef( &impl->iface );
}

static ULONG WINAPI implicit_view_iterable_Release( struct implicit_iterable_iface *iface)
{
    struct implicit_animation_view *impl = CONTAINING_RECORD( iface, struct implicit_animation_view, iterable );

    return implicit_view_Release( &impl->iface );
}

static HRESULT WINAPI implicit_view_iterable_GetIids( struct implicit_iterable_iface *iface, ULONG *count, IID **iids)
{
    struct implicit_animation_view *impl = CONTAINING_RECORD( iface, struct implicit_animation_view, iterable );

    return implicit_view_GetIids( &impl->iface, count, iids );
}

static HRESULT WINAPI implicit_view_iterable_GetRuntimeClassName( struct implicit_iterable_iface *iface, HSTRING *name)
{
    struct implicit_animation_view *impl = CONTAINING_RECORD( iface, struct implicit_animation_view, iterable );

    return implicit_view_GetRuntimeClassName( &impl->iface, name );
}

static HRESULT WINAPI implicit_view_iterable_GetTrustLevel( struct implicit_iterable_iface *iface, TrustLevel *level)
{
    struct implicit_animation_view *impl = CONTAINING_RECORD( iface, struct implicit_animation_view, iterable );

    return implicit_view_GetTrustLevel( &impl->iface, level );
}

static const struct implicit_iterable_vtbl implicit_iterable_vtbl =
{
    implicit_iterable_QueryInterface, implicit_iterable_AddRef, implicit_iterable_Release,
    implicit_iterable_GetIids, implicit_iterable_GetRuntimeClassName, implicit_iterable_GetTrustLevel, implicit_iterable_First,
};

static const struct implicit_iterable_vtbl implicit_view_iterable_vtbl =
{
    implicit_view_iterable_QueryInterface, implicit_view_iterable_AddRef, implicit_view_iterable_Release,
    implicit_view_iterable_GetIids, implicit_view_iterable_GetRuntimeClassName, implicit_view_iterable_GetTrustLevel,
    implicit_view_iterable_First,
};

static HRESULT implicit_view_create( struct implicit_animation_pair **entries, UINT32 count,
        struct implicit_view_iface **out )
{
    struct implicit_animation_view *impl;
    UINT32 i;

    *out = NULL;
    if (!(impl = calloc( 1, sizeof(*impl) ))) return E_OUTOFMEMORY;
    if (count && !(impl->entries = calloc( count, sizeof(*entries) ))) { free( impl ); return E_OUTOFMEMORY; }
    impl->iface.lpVtbl = &implicit_view_vtbl;
    impl->iterable.lpVtbl = &implicit_view_iterable_vtbl;
    impl->ref = 1;
    impl->count = count;
    for (i = 0; i < count; ++i)
    {
        impl->entries[i] = entries[i];
        implicit_pair_AddRef( &entries[i]->iface );
    }
    *out = &impl->iface;
    return S_OK;
}

static HRESULT WINAPI implicit_map_GetView( struct implicit_map_iface *iface, struct implicit_view_iface **value )
{
    struct implicit_animation_collection *impl = CONTAINING_RECORD( iface, struct implicit_animation_collection, map );
    HRESULT hr;

    if (!value) return E_POINTER;
    *value = NULL;
    EnterCriticalSection( &impl->cs );
    *value = NULL;
    if (impl->closed) { LeaveCriticalSection( &impl->cs ); return RO_E_CLOSED; }
    hr = implicit_view_create( impl->entries, impl->count, value );
    LeaveCriticalSection( &impl->cs );
    return hr;
}

static const struct implicit_map_vtbl implicit_map_vtbl =
{
    implicit_map_QueryInterface, implicit_map_AddRef, implicit_map_Release,
    implicit_map_GetIids, implicit_map_GetRuntimeClassName, implicit_map_GetTrustLevel,
    implicit_map_Lookup, implicit_map_get_Size, implicit_map_HasKey, implicit_map_GetView,
    implicit_map_Insert, implicit_map_Remove, implicit_map_Clear,
};
static HRESULT WINAPI implicit_object_get_Compositor( ICompositionObject *iface, ICompositor **value )
{
    struct implicit_animation_collection *impl = CONTAINING_RECORD( iface, struct implicit_animation_collection, object );

    if (!value) return E_POINTER;
    *value = NULL;
    EnterCriticalSection( &impl->cs );
    if (impl->closed) { LeaveCriticalSection( &impl->cs ); return RO_E_CLOSED; }
    *value = impl->compositor;
    ICompositor_AddRef( *value );
    LeaveCriticalSection( &impl->cs );
    return S_OK;
}

static HRESULT WINAPI implicit_object_get_Dispatcher( ICompositionObject *iface, ICoreDispatcher **value )
{
    struct implicit_animation_collection *impl = CONTAINING_RECORD( iface, struct implicit_animation_collection, object );
    HRESULT hr;

    if (!value) return E_POINTER;
    *value = NULL;
    EnterCriticalSection( &impl->cs );
    hr = impl->closed ? RO_E_CLOSED : S_OK;
    LeaveCriticalSection( &impl->cs );
    return hr;
}

static HRESULT WINAPI implicit_object_get_Properties( ICompositionObject *iface, ICompositionPropertySet **value )
{
    struct implicit_animation_collection *impl = CONTAINING_RECORD( iface, struct implicit_animation_collection, object );

    if (!value) return E_POINTER;
    *value = NULL;
    EnterCriticalSection( &impl->cs );
    if (impl->closed) { LeaveCriticalSection( &impl->cs ); return RO_E_CLOSED; }
    *value = impl->properties;
    ICompositionPropertySet_AddRef( *value );
    LeaveCriticalSection( &impl->cs );
    return S_OK;
}

static HRESULT WINAPI implicit_object_StartAnimation( ICompositionObject *iface, HSTRING name, ICompositionAnimation *animation )
{
    struct implicit_animation_collection *impl = CONTAINING_RECORD( iface, struct implicit_animation_collection, object );
    HRESULT hr;

    EnterCriticalSection( &impl->cs );
    hr = impl->closed ? RO_E_CLOSED : E_NOTIMPL;
    LeaveCriticalSection( &impl->cs );
    return hr;
}

static HRESULT WINAPI implicit_object_StopAnimation( ICompositionObject *iface, HSTRING name )
{
    struct implicit_animation_collection *impl = CONTAINING_RECORD( iface, struct implicit_animation_collection, object );
    HRESULT hr;

    EnterCriticalSection( &impl->cs );
    hr = impl->closed ? RO_E_CLOSED : E_NOTIMPL;
    LeaveCriticalSection( &impl->cs );
    return hr;
}

static const ICompositionObjectVtbl implicit_object_vtbl =
{
    implicit_object_QueryInterface, implicit_object_AddRef, implicit_object_Release,
    implicit_object_GetIids, implicit_object_GetRuntimeClassName, implicit_object_GetTrustLevel,
    implicit_object_get_Compositor, implicit_object_get_Dispatcher, implicit_object_get_Properties,
    implicit_object_StartAnimation, implicit_object_StopAnimation,
};



static HRESULT WINAPI implicit_collection_closable_QueryInterface( IClosable *iface, REFIID iid, void **out)
{
    struct implicit_animation_collection *impl = CONTAINING_RECORD( iface, struct implicit_animation_collection, closable );

    return implicit_collection_QueryInterface( &impl->iface, iid, out );
}

static ULONG WINAPI implicit_collection_closable_AddRef( IClosable *iface)
{
    struct implicit_animation_collection *impl = CONTAINING_RECORD( iface, struct implicit_animation_collection, closable );

    return implicit_collection_AddRef( &impl->iface );
}

static ULONG WINAPI implicit_collection_closable_Release( IClosable *iface)
{
    struct implicit_animation_collection *impl = CONTAINING_RECORD( iface, struct implicit_animation_collection, closable );

    return implicit_collection_Release( &impl->iface );
}

static HRESULT WINAPI implicit_collection_closable_GetIids( IClosable *iface, ULONG *count, IID **iids)
{
    struct implicit_animation_collection *impl = CONTAINING_RECORD( iface, struct implicit_animation_collection, closable );

    return implicit_collection_GetIids( &impl->iface, count, iids );
}

static HRESULT WINAPI implicit_collection_closable_GetRuntimeClassName( IClosable *iface, HSTRING *name)
{
    struct implicit_animation_collection *impl = CONTAINING_RECORD( iface, struct implicit_animation_collection, closable );

    return implicit_collection_GetRuntimeClassName( &impl->iface, name );
}

static HRESULT WINAPI implicit_collection_closable_GetTrustLevel( IClosable *iface, TrustLevel *level)
{
    struct implicit_animation_collection *impl = CONTAINING_RECORD( iface, struct implicit_animation_collection, closable );

    return implicit_collection_GetTrustLevel( &impl->iface, level );
}

static HRESULT WINAPI implicit_collection_closable_Close( IClosable *iface )
{
    struct implicit_animation_collection *impl = CONTAINING_RECORD( iface, struct implicit_animation_collection, closable );
    struct implicit_animation_pair **entries;
    ICompositionPropertySet *properties;
    ICompositor *compositor;
    IClosable *child;
    HSTRING comment;
    IInspectable *implicit_animations;
    UINT32 count, i;

    EnterCriticalSection( &impl->cs );
    if (impl->closed) { LeaveCriticalSection( &impl->cs ); return S_OK; }
    impl->closed = TRUE;
    comment = impl->object2.comment;
    implicit_animations = impl->object2.implicit_animations;
    impl->object2.comment = NULL;
    impl->object2.implicit_animations = NULL;
    ++impl->generation;
    entries = impl->entries;
    count = impl->count;
    properties = impl->properties;
    compositor = impl->compositor;
    impl->entries = NULL;
    impl->count = 0;
    impl->properties = NULL;
    impl->compositor = NULL;
    LeaveCriticalSection( &impl->cs );
    for (i = 0; i < count; ++i)
    {
        if (SUCCEEDED(IUnknown_QueryInterface( (IUnknown *)entries[i]->value, &IID_IClosable, (void **)&child )))
        {
            IClosable_Close( child );
            IClosable_Release( child );
        }
    }
    implicit_entries_release( entries, count );
    collection_object2_release_state( comment, implicit_animations );
    ICompositionPropertySet_Release( properties );
    ICompositor_Release( compositor );
    return S_OK;
}

static const IClosableVtbl implicit_collection_closable_vtbl =
{
    implicit_collection_closable_QueryInterface, implicit_collection_closable_AddRef, implicit_collection_closable_Release,
    implicit_collection_closable_GetIids, implicit_collection_closable_GetRuntimeClassName, implicit_collection_closable_GetTrustLevel, implicit_collection_closable_Close,
};

static HRESULT WINAPI compositor2_CreateImplicitAnimationCollection( struct compositor2_iface *iface, IInspectable **value )
{
    struct compositor *compositor = impl_from_compositor2( iface );
    struct implicit_animation_collection *impl;
    HRESULT hr;

    if (!value) return E_POINTER;
    *value = NULL;
    if (!(impl = calloc( 1, sizeof(*impl) ))) return E_OUTOFMEMORY;
    hr = composition_property_set_create( &compositor->ICompositor_iface, &impl->properties );
    if (FAILED(hr)) { free( impl ); return hr; }
    impl->iface.lpVtbl = &implicit_collection_vtbl;
    impl->map.lpVtbl = &implicit_map_vtbl;
    impl->iterable.lpVtbl = &implicit_iterable_vtbl;
    impl->object.lpVtbl = &implicit_object_vtbl;
    impl->compositor = &compositor->ICompositor_iface;
    ICompositor_AddRef( impl->compositor );
    impl->closable.lpVtbl = &implicit_collection_closable_vtbl;
    impl->object2.iface.lpVtbl = &collection_object2_vtbl;
    impl->object2.object = &impl->object;
    impl->object2.cs = &impl->cs;
    impl->object2.closed = &impl->closed;
    impl->ref = 1;
    InitializeCriticalSection( &impl->cs );
    *value = &impl->iface;
    return S_OK;
}

DEFINE_COMPOSITOR2_CREATE_METHOD(CreateLayerVisual)
DEFINE_COMPOSITOR2_CREATE_METHOD(CreateMaskBrush)
DEFINE_COMPOSITOR2_CREATE_METHOD(CreatePointLight)
DEFINE_COMPOSITOR2_CREATE_METHOD(CreateSpotLight)

#undef DEFINE_COMPOSITOR2_CREATE_METHOD

static HRESULT WINAPI compositor2_CreateStepEasingFunction( struct compositor2_iface *iface,
        IInspectable **result )
{
    TRACE( "iface %p, result %p.\n", iface, result );
    return step_easing_function_create( 1, result );
}

static HRESULT WINAPI compositor2_CreateNineGridBrush( struct compositor2_iface *iface,
        IInspectable **result )
{
    struct compositor *impl = impl_from_compositor2( iface );

    TRACE( "iface %p, result %p.\n", iface, result );
    return nine_grid_brush_create( &impl->ICompositor_iface, result );
}

static HRESULT WINAPI compositor2_CreateStepEasingFunctionWithStepCount( struct compositor2_iface *iface,
        INT32 step_count, IInspectable **result )
{
    TRACE( "iface %p, step_count %d, result %p.\n", iface, step_count, result );
    return step_easing_function_create( step_count, result );
}

static const struct compositor2_vtbl compositor2_vtbl =
{
    compositor2_QueryInterface,
    compositor2_AddRef,
    compositor2_Release,
    compositor2_GetIids,
    compositor2_GetRuntimeClassName,
    compositor2_GetTrustLevel,
    compositor2_CreateAmbientLight,
    compositor2_CreateAnimationGroup,
    compositor2_CreateBackdropBrush,
    compositor2_CreateDistantLight,
    compositor2_CreateDropShadow,
    compositor2_CreateImplicitAnimationCollection,
    compositor2_CreateLayerVisual,
    compositor2_CreateMaskBrush,
    compositor2_CreateNineGridBrush,
    compositor2_CreatePointLight,
    compositor2_CreateSpotLight,
    compositor2_CreateStepEasingFunction,
    compositor2_CreateStepEasingFunctionWithStepCount,
};

static HRESULT WINAPI compositor4_QueryInterface( struct compositor4_iface *iface, REFIID iid, void **out )
{
    struct compositor *impl = impl_from_compositor4( iface );
    return ICompositor_QueryInterface( &impl->ICompositor_iface, iid, out );
}

static ULONG WINAPI compositor4_AddRef( struct compositor4_iface *iface )
{
    struct compositor *impl = impl_from_compositor4( iface );
    return ICompositor_AddRef( &impl->ICompositor_iface );
}

static ULONG WINAPI compositor4_Release( struct compositor4_iface *iface )
{
    struct compositor *impl = impl_from_compositor4( iface );
    return ICompositor_Release( &impl->ICompositor_iface );
}

static HRESULT WINAPI compositor4_GetIids( struct compositor4_iface *iface, ULONG *count, IID **iids )
{
    struct compositor *impl = impl_from_compositor4( iface );
    return ICompositor_GetIids( &impl->ICompositor_iface, count, iids );
}

static HRESULT WINAPI compositor4_GetRuntimeClassName( struct compositor4_iface *iface, HSTRING *name )
{
    struct compositor *impl = impl_from_compositor4( iface );
    return ICompositor_GetRuntimeClassName( &impl->ICompositor_iface, name );
}

static HRESULT WINAPI compositor4_GetTrustLevel( struct compositor4_iface *iface, TrustLevel *level )
{
    struct compositor *impl = impl_from_compositor4( iface );
    return ICompositor_GetTrustLevel( &impl->ICompositor_iface, level );
}

struct gradient_object;
struct gradient_iface { const void *lpVtbl; struct gradient_object *owner; };
#define GRADIENT_INSPECTABLE \
    HRESULT (WINAPI *QueryInterface)( struct gradient_iface *, REFIID, void ** ); \
    ULONG (WINAPI *AddRef)( struct gradient_iface * ); \
    ULONG (WINAPI *Release)( struct gradient_iface * ); \
    HRESULT (WINAPI *GetIids)( struct gradient_iface *, ULONG *, IID ** ); \
    HRESULT (WINAPI *GetRuntimeClassName)( struct gradient_iface *, HSTRING * ); \
    HRESULT (WINAPI *GetTrustLevel)( struct gradient_iface *, TrustLevel * )
static const GUID gradient_linear_iid = {0x983bc519, 0xa9db, 0x413c, {0xa2, 0xd8, 0x2a, 0x90, 0x56, 0xfc, 0x52, 0x5e}};
struct gradient_linear_vtbl
{
    GRADIENT_INSPECTABLE;
    HRESULT (WINAPI *get_EndPoint)( struct gradient_iface *iface, struct Vector2* value );
    HRESULT (WINAPI *put_EndPoint)( struct gradient_iface *iface, struct Vector2 value );
    HRESULT (WINAPI *get_StartPoint)( struct gradient_iface *iface, struct Vector2* value );
    HRESULT (WINAPI *put_StartPoint)( struct gradient_iface *iface, struct Vector2 value );
};
static const GUID gradient_gradient_iid = {0x1d9709e0, 0xffc6, 0x4c0e, {0xa9, 0xab, 0x34, 0x14, 0x4d, 0x4c, 0x90, 0x98}};
struct gradient_gradient_vtbl
{
    GRADIENT_INSPECTABLE;
    HRESULT (WINAPI *get_AnchorPoint)( struct gradient_iface *iface, struct Vector2* value );
    HRESULT (WINAPI *put_AnchorPoint)( struct gradient_iface *iface, struct Vector2 value );
    HRESULT (WINAPI *get_CenterPoint)( struct gradient_iface *iface, struct Vector2* value );
    HRESULT (WINAPI *put_CenterPoint)( struct gradient_iface *iface, struct Vector2 value );
    HRESULT (WINAPI *get_ColorStops)( struct gradient_iface *iface, struct gradient_iface** value );
    HRESULT (WINAPI *get_ExtendMode)( struct gradient_iface *iface, INT32* value );
    HRESULT (WINAPI *put_ExtendMode)( struct gradient_iface *iface, INT32 value );
    HRESULT (WINAPI *get_InterpolationSpace)( struct gradient_iface *iface, INT32* value );
    HRESULT (WINAPI *put_InterpolationSpace)( struct gradient_iface *iface, INT32 value );
    HRESULT (WINAPI *get_Offset)( struct gradient_iface *iface, struct Vector2* value );
    HRESULT (WINAPI *put_Offset)( struct gradient_iface *iface, struct Vector2 value );
    HRESULT (WINAPI *get_RotationAngle)( struct gradient_iface *iface, FLOAT* value );
    HRESULT (WINAPI *put_RotationAngle)( struct gradient_iface *iface, FLOAT value );
    HRESULT (WINAPI *get_RotationAngleInDegrees)( struct gradient_iface *iface, FLOAT* value );
    HRESULT (WINAPI *put_RotationAngleInDegrees)( struct gradient_iface *iface, FLOAT value );
    HRESULT (WINAPI *get_Scale)( struct gradient_iface *iface, struct Vector2* value );
    HRESULT (WINAPI *put_Scale)( struct gradient_iface *iface, struct Vector2 value );
    HRESULT (WINAPI *get_TransformMatrix)( struct gradient_iface *iface, struct Matrix3x2* value );
    HRESULT (WINAPI *put_TransformMatrix)( struct gradient_iface *iface, struct Matrix3x2 value );
};
static const GUID gradient_mapping_iid = {0x899dd5a1, 0xb4c7, 0x4b33, {0xa1, 0xb6, 0x26, 0x4a, 0xdd, 0xc2, 0x6d, 0x10}};
struct gradient_mapping_vtbl
{
    GRADIENT_INSPECTABLE;
    HRESULT (WINAPI *get_MappingMode)( struct gradient_iface *iface, INT32* value );
    HRESULT (WINAPI *put_MappingMode)( struct gradient_iface *iface, INT32 value );
};
static const GUID gradient_stop_iid = {0x6f00ca92, 0xc801, 0x4e41, {0x9a, 0x8f, 0xa5, 0x3e, 0x20, 0xf5, 0x77, 0x78}};
struct gradient_stop_vtbl
{
    GRADIENT_INSPECTABLE;
    HRESULT (WINAPI *get_Color)( struct gradient_iface *iface, struct Color* value );
    HRESULT (WINAPI *put_Color)( struct gradient_iface *iface, struct Color value );
    HRESULT (WINAPI *get_Offset)( struct gradient_iface *iface, FLOAT* value );
    HRESULT (WINAPI *put_Offset)( struct gradient_iface *iface, FLOAT value );
};
static const GUID gradient_vector_iid = {0xbf2e107e, 0xf3db, 0x56cd, {0x91, 0xed, 0xc1, 0x12, 0x94, 0x06, 0xd5, 0x52}};
static const GUID gradient_view_iid = {0x801d95c7, 0x7ce2, 0x55d7, {0xa6, 0x2f, 0x2f, 0xf9, 0x4c, 0x54, 0x40, 0x34}};
static const GUID gradient_iterable_iid = {0x0f2cfac1, 0xcdbc, 0x5c4e, {0xa7, 0xd2, 0xa3, 0x91, 0x4f, 0xb6, 0x34, 0x26}};
static const GUID gradient_iterator_iid = {0x746a0f51, 0x47f4, 0x5395, {0x8e, 0xdd, 0x8a, 0xe1, 0xe9, 0x3a, 0x81, 0x83}};
static const GUID gradient_collection_iid = {0x9f1d20ec,0x7b04,0x4b1d,{0x90,0xbc,0x9f,0xa3,0x2c,0x0c,0xfd,0x26}};

struct gradient_vector_vtbl
{
    GRADIENT_INSPECTABLE;
    HRESULT (WINAPI *GetAt)( struct gradient_iface *, UINT32, struct gradient_iface ** );
    HRESULT (WINAPI *get_Size)( struct gradient_iface *, UINT32 * );
    HRESULT (WINAPI *GetView)( struct gradient_iface *, struct gradient_iface ** );
    HRESULT (WINAPI *IndexOf)( struct gradient_iface *, struct gradient_iface *, UINT32 *, boolean * );
    HRESULT (WINAPI *SetAt)( struct gradient_iface *, UINT32, struct gradient_iface * );
    HRESULT (WINAPI *InsertAt)( struct gradient_iface *, UINT32, struct gradient_iface * );
    HRESULT (WINAPI *RemoveAt)( struct gradient_iface *, UINT32 );
    HRESULT (WINAPI *Append)( struct gradient_iface *, struct gradient_iface * );
    HRESULT (WINAPI *RemoveAtEnd)( struct gradient_iface * );
    HRESULT (WINAPI *Clear)( struct gradient_iface * );
    HRESULT (WINAPI *GetMany)( struct gradient_iface *, UINT32, UINT32, struct gradient_iface **, UINT32 * );
    HRESULT (WINAPI *ReplaceAll)( struct gradient_iface *, UINT32, struct gradient_iface ** );
};
struct gradient_view_vtbl
{
    GRADIENT_INSPECTABLE;
    HRESULT (WINAPI *GetAt)( struct gradient_iface *, UINT32, struct gradient_iface ** );
    HRESULT (WINAPI *get_Size)( struct gradient_iface *, UINT32 * );
    HRESULT (WINAPI *IndexOf)( struct gradient_iface *, struct gradient_iface *, UINT32 *, boolean * );
    HRESULT (WINAPI *GetMany)( struct gradient_iface *, UINT32, UINT32, struct gradient_iface **, UINT32 * );
};
struct gradient_iterable_vtbl { GRADIENT_INSPECTABLE; HRESULT (WINAPI *First)( struct gradient_iface *, struct gradient_iface ** ); };
struct gradient_iterator_vtbl
{
    GRADIENT_INSPECTABLE;
    HRESULT (WINAPI *get_Current)( struct gradient_iface *, struct gradient_iface ** );
    HRESULT (WINAPI *get_HasCurrent)( struct gradient_iface *, boolean * );
    HRESULT (WINAPI *MoveNext)( struct gradient_iface *, boolean * );
    HRESULT (WINAPI *GetMany)( struct gradient_iface *, UINT32, struct gradient_iface **, UINT32 * );
};
struct gradient_object_vtbl
{
    GRADIENT_INSPECTABLE;
    HRESULT (WINAPI *get_Compositor)( struct gradient_iface *, ICompositor ** );
    HRESULT (WINAPI *get_Dispatcher)( struct gradient_iface *, ICoreDispatcher ** );
    HRESULT (WINAPI *get_Properties)( struct gradient_iface *, ICompositionPropertySet ** );
    HRESULT (WINAPI *StartAnimation)( struct gradient_iface *, HSTRING, ICompositionAnimation * );
    HRESULT (WINAPI *StopAnimation)( struct gradient_iface *, HSTRING );
};
struct gradient_closable_vtbl { GRADIENT_INSPECTABLE; HRESULT (WINAPI *Close)( struct gradient_iface * ); };
enum gradient_kind { GRADIENT_BRUSH, GRADIENT_STOP, GRADIENT_COLLECTION, GRADIENT_VIEW, GRADIENT_ITERATOR };
struct gradient_object
{
    struct gradient_iface primary, base, mapping, brush, object, closable, iterable;
    enum gradient_kind kind;
    LONG ref, closed;
    CRITICAL_SECTION lock;
    ICompositor *compositor;
    ICompositionPropertySet *properties;
    struct gradient_object *stops;
    struct gradient_iface **items;
    UINT32 count, capacity, position, version, observed_version;
    struct Color color;
    FLOAT offset, rotation;
    struct Vector2 start, end, anchor, center, translation, scale;
    struct Matrix3x2 transform;
    INT32 extend, interpolation, mapping_mode;
};
static const struct gradient_linear_vtbl gradient_linear_vtbl;
static const struct gradient_stop_vtbl gradient_stop_vtbl;
static const struct gradient_vector_vtbl gradient_vector_vtbl;
static const struct gradient_view_vtbl gradient_view_vtbl;
static const struct gradient_iterator_vtbl gradient_iterator_vtbl;
static const struct gradient_iterable_vtbl gradient_iterable_vtbl;
static const struct gradient_gradient_vtbl gradient_gradient_vtbl;
static const struct gradient_mapping_vtbl gradient_mapping_vtbl;
static const struct gradient_object_vtbl gradient_object_vtbl;
static const struct gradient_closable_vtbl gradient_closable_vtbl;
struct gradient_inspectable_vtbl { GRADIENT_INSPECTABLE; };
static const struct gradient_inspectable_vtbl gradient_brush_vtbl;
static HRESULT gradient_create( ICompositor *, enum gradient_kind, struct gradient_object ** );
static ULONG WINAPI gradient_addref( struct gradient_iface *iface ) { return InterlockedIncrement( &iface->owner->ref ); }
static ULONG WINAPI gradient_release( struct gradient_iface *iface )
{
    struct gradient_object *impl = iface->owner;
    ULONG ref = InterlockedDecrement( &impl->ref );
    UINT32 i;
    if (!ref)
    {
        for (i = 0; i < impl->count; ++i) IUnknown_Release( (IUnknown *)impl->items[i] );
        free( impl->items );
        if (impl->stops) gradient_release( &impl->stops->primary );
        if (impl->properties) ICompositionPropertySet_Release( impl->properties );
        ICompositor_Release( impl->compositor );
        DeleteCriticalSection( &impl->lock );
        free( impl );
    }
    return ref;
}
static HRESULT WINAPI gradient_query( struct gradient_iface *iface, REFIID iid, void **out )
{
    struct gradient_object *impl = iface->owner;
    if (!out) return E_POINTER;
    *out = NULL;
    if (IsEqualGUID( iid, &IID_IUnknown ) || IsEqualGUID( iid, &IID_IInspectable ) || IsEqualGUID( iid, &IID_IAgileObject )) *out = &impl->primary;
    else if (impl->kind == GRADIENT_BRUSH && IsEqualGUID( iid, &gradient_linear_iid )) *out = &impl->primary;
    else if (impl->kind == GRADIENT_BRUSH && IsEqualGUID( iid, &gradient_gradient_iid )) *out = &impl->base;
    else if (impl->kind == GRADIENT_BRUSH && IsEqualGUID( iid, &gradient_mapping_iid )) *out = &impl->mapping;
    else if (impl->kind == GRADIENT_BRUSH && IsEqualGUID( iid, &IID_ICompositionBrush )) *out = &impl->brush;
    else if (impl->kind == GRADIENT_STOP && IsEqualGUID( iid, &gradient_stop_iid )) *out = &impl->primary;
    else if (impl->kind == GRADIENT_COLLECTION && (IsEqualGUID( iid, &gradient_collection_iid ) || IsEqualGUID( iid, &gradient_vector_iid ))) *out = &impl->primary;
    else if (impl->kind == GRADIENT_VIEW && IsEqualGUID( iid, &gradient_view_iid )) *out = &impl->primary;
    else if ((impl->kind == GRADIENT_COLLECTION || impl->kind == GRADIENT_VIEW) && IsEqualGUID( iid, &gradient_iterable_iid )) *out = &impl->iterable;
    else if (impl->kind == GRADIENT_ITERATOR && IsEqualGUID( iid, &gradient_iterator_iid )) *out = &impl->primary;
    else if (impl->kind <= GRADIENT_COLLECTION && IsEqualGUID( iid, &IID_ICompositionObject )) *out = &impl->object;
    else if (impl->kind <= GRADIENT_COLLECTION && IsEqualGUID( iid, &IID_IClosable )) *out = &impl->closable;
    else return E_NOINTERFACE;
    gradient_addref( iface );
    return S_OK;
}
static HRESULT WINAPI gradient_iids( struct gradient_iface *iface, ULONG *count, IID **ids )
{
    struct gradient_object *impl = iface->owner;
    IID supported[7];
    ULONG n = 0;
    if (!count || !ids) return E_POINTER;
    *count = 0; *ids = NULL;
    if (impl->kind == GRADIENT_BRUSH) { supported[n++] = gradient_linear_iid; supported[n++] = gradient_gradient_iid; supported[n++] = gradient_mapping_iid; supported[n++] = IID_ICompositionBrush; }
    else if (impl->kind == GRADIENT_STOP) supported[n++] = gradient_stop_iid;
    else if (impl->kind == GRADIENT_COLLECTION) { supported[n++] = gradient_collection_iid; supported[n++] = gradient_vector_iid; supported[n++] = gradient_iterable_iid; }
    else if (impl->kind == GRADIENT_VIEW) { supported[n++] = gradient_view_iid; supported[n++] = gradient_iterable_iid; }
    else supported[n++] = gradient_iterator_iid;
    if (impl->kind <= GRADIENT_COLLECTION) { supported[n++] = IID_ICompositionObject; supported[n++] = IID_IClosable; }
    if (!(*ids = CoTaskMemAlloc( n * sizeof(**ids) ))) return E_OUTOFMEMORY;
    memcpy( *ids, supported, n * sizeof(**ids) ); *count = n;
    return S_OK;
}
static HRESULT WINAPI gradient_name( struct gradient_iface *iface, HSTRING *name )
{
    static const WCHAR *names[] = {L"Windows.UI.Composition.CompositionLinearGradientBrush", L"Windows.UI.Composition.CompositionColorGradientStop", L"Windows.UI.Composition.CompositionColorGradientStopCollection", L"Windows.Foundation.Collections.IVectorView`1<Windows.UI.Composition.CompositionColorGradientStop>", L"Windows.Foundation.Collections.IIterator`1<Windows.UI.Composition.CompositionColorGradientStop>"};
    if (!name) return E_POINTER;
    return WindowsCreateString( names[iface->owner->kind], wcslen( names[iface->owner->kind] ), name );
}
static HRESULT WINAPI gradient_trust( struct gradient_iface *iface, TrustLevel *level )
{ if (!level) return E_POINTER; *level = BaseTrust; return S_OK; }
#define GRADIENT_FORWARD gradient_query, gradient_addref, gradient_release, gradient_iids, gradient_name, gradient_trust
static void gradient_changed( struct gradient_object *impl ) { compositor_notify_commit_needed( impl_from_ICompositor( impl->compositor ) ); }
#define GRADIENT_PROPERTY(name, type, field) \
static HRESULT WINAPI gradient_get_##name( struct gradient_iface *iface, type *value ) \
{ struct gradient_object *impl = iface->owner; if (!value) return E_POINTER; if (impl->closed) return RO_E_CLOSED; EnterCriticalSection( &impl->lock ); *value = impl->field; LeaveCriticalSection( &impl->lock ); return S_OK; } \
static HRESULT WINAPI gradient_put_##name( struct gradient_iface *iface, type value ) \
{ struct gradient_object *impl = iface->owner; if (impl->closed) return RO_E_CLOSED; EnterCriticalSection( &impl->lock ); impl->field = value; LeaveCriticalSection( &impl->lock ); gradient_changed( impl ); return S_OK; }
GRADIENT_PROPERTY( StartPoint, struct Vector2, start )
GRADIENT_PROPERTY( EndPoint, struct Vector2, end )
GRADIENT_PROPERTY( AnchorPoint, struct Vector2, anchor )
GRADIENT_PROPERTY( CenterPoint, struct Vector2, center )
GRADIENT_PROPERTY( Offset, struct Vector2, translation )
GRADIENT_PROPERTY( Scale, struct Vector2, scale )
GRADIENT_PROPERTY( TransformMatrix, struct Matrix3x2, transform )
GRADIENT_PROPERTY( RotationAngle, FLOAT, rotation )
GRADIENT_PROPERTY( Color, struct Color, color )
GRADIENT_PROPERTY( StopOffset, FLOAT, offset )
GRADIENT_PROPERTY( ExtendMode, INT32, extend )
GRADIENT_PROPERTY( InterpolationSpace, INT32, interpolation )
GRADIENT_PROPERTY( MappingMode, INT32, mapping_mode )
static HRESULT WINAPI gradient_get_RotationAngleInDegrees( struct gradient_iface *iface, FLOAT *value )
{ HRESULT hr = gradient_get_RotationAngle( iface, value ); if (SUCCEEDED(hr)) *value *= 57.29577951308232f; return hr; }
static HRESULT WINAPI gradient_put_RotationAngleInDegrees( struct gradient_iface *iface, FLOAT value )
{ return gradient_put_RotationAngle( iface, value * 0.017453292519943295f ); }
static HRESULT WINAPI gradient_get_ColorStops( struct gradient_iface *iface, struct gradient_iface **value )
{ if (!value) return E_POINTER; *value = NULL; if (iface->owner->closed) return RO_E_CLOSED; *value = &iface->owner->stops->primary; gradient_addref( *value ); return S_OK; }
static HRESULT WINAPI gradient_get_Compositor( struct gradient_iface *iface, ICompositor **value )
{ if (!value) return E_POINTER; *value = iface->owner->compositor; ICompositor_AddRef( *value ); return S_OK; }
static HRESULT WINAPI gradient_get_Dispatcher( struct gradient_iface *iface, ICoreDispatcher **value )
{ if (!value) return E_POINTER; *value = NULL; return S_OK; }
static HRESULT WINAPI gradient_get_Properties( struct gradient_iface *iface, ICompositionPropertySet **value )
{ if (!value) return E_POINTER; *value = NULL; if (iface->owner->closed) return RO_E_CLOSED; *value = iface->owner->properties; ICompositionPropertySet_AddRef( *value ); return S_OK; }
static HRESULT WINAPI gradient_StartAnimation( struct gradient_iface *iface, HSTRING name, ICompositionAnimation *animation )
{ return iface->owner->closed ? RO_E_CLOSED : E_NOTIMPL; }
static HRESULT WINAPI gradient_StopAnimation( struct gradient_iface *iface, HSTRING name )
{ return iface->owner->closed ? RO_E_CLOSED : E_NOTIMPL; }
static HRESULT WINAPI gradient_Close( struct gradient_iface *iface )
{ InterlockedExchange( &iface->owner->closed, TRUE ); return S_OK; }
static HRESULT WINAPI gradient_vector_GetAt( struct gradient_iface *iface, UINT32 index, struct gradient_iface **value )
{
    struct gradient_object *impl = iface->owner;
    if (!value) return E_POINTER;
    *value = NULL;
    if (impl->closed) return RO_E_CLOSED;
    EnterCriticalSection( &impl->lock );
    if (index >= impl->count) { LeaveCriticalSection( &impl->lock ); return E_BOUNDS; }
    *value = impl->items[index]; IUnknown_AddRef( (IUnknown *)*value );
    LeaveCriticalSection( &impl->lock ); return S_OK;
}
static HRESULT WINAPI gradient_vector_Size( struct gradient_iface *iface, UINT32 *value )
{ if (!value) return E_POINTER; if (iface->owner->closed) return RO_E_CLOSED; EnterCriticalSection( &iface->owner->lock ); *value = iface->owner->count; LeaveCriticalSection( &iface->owner->lock ); return S_OK; }
static HRESULT WINAPI gradient_vector_GetMany( struct gradient_iface *iface, UINT32 start, UINT32 capacity, struct gradient_iface **values, UINT32 *actual )
{
    struct gradient_object *impl = iface->owner;
    UINT32 i;
    if (!actual || (capacity && !values)) return E_POINTER;
    *actual = 0;
    if (impl->closed) return RO_E_CLOSED;
    EnterCriticalSection( &impl->lock );
    if (start > impl->count) { LeaveCriticalSection( &impl->lock ); return E_BOUNDS; }
    *actual = min( capacity, impl->count - start );
    for (i = 0; i < *actual; ++i) { values[i] = impl->items[start + i]; IUnknown_AddRef( (IUnknown *)values[i] ); }
    LeaveCriticalSection( &impl->lock ); return S_OK;
}
static HRESULT WINAPI gradient_vector_IndexOf( struct gradient_iface *iface, struct gradient_iface *value, UINT32 *index, boolean *found )
{
    struct gradient_object *impl = iface->owner;
    IUnknown *identity, *current;
    UINT32 i;
    if (!index || !found) return E_POINTER;
    *index = 0; *found = FALSE;
    if (impl->closed) return RO_E_CLOSED;
    if (!value || FAILED(IUnknown_QueryInterface( (IUnknown *)value, &IID_IUnknown, (void **)&identity ))) return S_OK;
    EnterCriticalSection( &impl->lock );
    for (i = 0; i < impl->count; ++i)
    {
        if (FAILED(IUnknown_QueryInterface( (IUnknown *)impl->items[i], &IID_IUnknown, (void **)&current ))) continue;
        if (current == identity) { *index = i; *found = TRUE; }
        IUnknown_Release( current ); if (*found) break;
    }
    LeaveCriticalSection( &impl->lock ); IUnknown_Release( identity ); return S_OK;
}
static HRESULT gradient_vector_set( struct gradient_iface *iface, UINT32 index, struct gradient_iface *input, BOOL insert, BOOL append )
{
    struct gradient_object *impl = iface->owner;
    struct gradient_iface *value, *old = NULL, **items;
    HRESULT hr;
    UINT32 capacity;
    if (impl->closed) return RO_E_CLOSED;
    if (!input) return E_POINTER;
    if (FAILED(hr = IUnknown_QueryInterface( (IUnknown *)input, &gradient_stop_iid, (void **)&value ))) return hr;
    EnterCriticalSection( &impl->lock );
    if (append) index = impl->count;
    if (index > impl->count || (!insert && index == impl->count)) hr = E_BOUNDS;
    else if (insert && impl->count == UINT_MAX) hr = E_OUTOFMEMORY;
    else if (insert && impl->count == impl->capacity)
    {
        capacity = impl->capacity ? impl->capacity * 2 : 4;
        if (capacity <= impl->count || (SIZE_T)capacity > SIZE_MAX / sizeof(*items)) hr = E_OUTOFMEMORY;
        else if (!(items = realloc( impl->items, capacity * sizeof(*items) ))) hr = E_OUTOFMEMORY;
        else { impl->items = items; impl->capacity = capacity; }
    }
    if (SUCCEEDED(hr))
    {
        if (insert) { memmove( impl->items + index + 1, impl->items + index, (impl->count - index) * sizeof(*impl->items) ); ++impl->count; }
        else old = impl->items[index];
        impl->items[index] = value; ++impl->version;
    }
    LeaveCriticalSection( &impl->lock );
    if (old) IUnknown_Release( (IUnknown *)old );
    if (FAILED(hr)) IUnknown_Release( (IUnknown *)value ); else gradient_changed( impl );
    return hr;
}
static HRESULT WINAPI gradient_vector_SetAt( struct gradient_iface *iface, UINT32 index, struct gradient_iface *value ) { return gradient_vector_set( iface, index, value, FALSE, FALSE ); }
static HRESULT WINAPI gradient_vector_InsertAt( struct gradient_iface *iface, UINT32 index, struct gradient_iface *value ) { return gradient_vector_set( iface, index, value, TRUE, FALSE ); }
static HRESULT WINAPI gradient_vector_Append( struct gradient_iface *iface, struct gradient_iface *value ) { return gradient_vector_set( iface, 0, value, TRUE, TRUE ); }
static HRESULT gradient_vector_remove( struct gradient_iface *iface, UINT32 index, BOOL last )
{
    struct gradient_object *impl = iface->owner;
    struct gradient_iface *old;
    if (impl->closed) return RO_E_CLOSED;
    EnterCriticalSection( &impl->lock );
    if (last) index = impl->count - 1;
    if (index >= impl->count) { LeaveCriticalSection( &impl->lock ); return E_BOUNDS; }
    old = impl->items[index]; --impl->count;
    memmove( impl->items + index, impl->items + index + 1, (impl->count - index) * sizeof(*impl->items) ); ++impl->version;
    LeaveCriticalSection( &impl->lock ); IUnknown_Release( (IUnknown *)old ); gradient_changed( impl ); return S_OK;
}
static HRESULT WINAPI gradient_vector_RemoveAt( struct gradient_iface *iface, UINT32 index ) { return gradient_vector_remove( iface, index, FALSE ); }
static HRESULT WINAPI gradient_vector_RemoveAtEnd( struct gradient_iface *iface ) { return gradient_vector_remove( iface, 0, TRUE ); }
static HRESULT WINAPI gradient_vector_ReplaceAll( struct gradient_iface *iface, UINT32 count, struct gradient_iface **values )
{
    struct gradient_object *impl = iface->owner;
    struct gradient_iface **items = NULL, **old;
    UINT32 i, old_count;
    HRESULT hr = S_OK;
    if (impl->closed) return RO_E_CLOSED;
    if (count && !values) return E_POINTER;
    if ((SIZE_T)count > SIZE_MAX / sizeof(*items)) return E_OUTOFMEMORY;
    if (count && !(items = calloc( count, sizeof(*items) ))) return E_OUTOFMEMORY;
    for (i = 0; i < count; ++i)
    {
        if (!values[i]) { hr = E_POINTER; break; }
        if (FAILED(hr = IUnknown_QueryInterface( (IUnknown *)values[i], &gradient_stop_iid, (void **)&items[i] ))) break;
    }
    if (FAILED(hr)) { while (i) { --i; IUnknown_Release( (IUnknown *)items[i] ); } free( items ); return hr; }
    EnterCriticalSection( &impl->lock ); old = impl->items; old_count = impl->count;
    impl->items = items; impl->count = impl->capacity = count; ++impl->version;
    LeaveCriticalSection( &impl->lock );
    for (i = 0; i < old_count; ++i) IUnknown_Release( (IUnknown *)old[i] );
    free( old ); gradient_changed( impl ); return S_OK;
}
static HRESULT WINAPI gradient_vector_Clear( struct gradient_iface *iface ) { return gradient_vector_ReplaceAll( iface, 0, NULL ); }
static HRESULT WINAPI gradient_vector_GetView( struct gradient_iface *iface, struct gradient_iface **value )
{
    struct gradient_object *source = iface->owner, *view;
    HRESULT hr;
    UINT32 i;
    if (!value) return E_POINTER;
    *value = NULL;
    if (source->closed) return RO_E_CLOSED;
    if (FAILED(hr = gradient_create( source->compositor, GRADIENT_VIEW, &view ))) return hr;
    EnterCriticalSection( &source->lock );
    if (source->count && !(view->items = calloc( source->count, sizeof(*view->items) ))) hr = E_OUTOFMEMORY;
    else for (i = 0; i < source->count; ++i) { view->items[view->count++] = source->items[i]; IUnknown_AddRef( (IUnknown *)source->items[i] ); }
    LeaveCriticalSection( &source->lock );
    if (FAILED(hr)) { gradient_release( &view->primary ); return hr; }
    *value = &view->primary; return S_OK;
}
static HRESULT WINAPI gradient_iterable_First( struct gradient_iface *iface, struct gradient_iface **value )
{
    struct gradient_object *iter;
    HRESULT hr;
    if (!value) return E_POINTER;
    *value = NULL;
    if (iface->owner->closed) return RO_E_CLOSED;
    if (FAILED(hr = gradient_create( iface->owner->compositor, GRADIENT_ITERATOR, &iter ))) return hr;
    iter->stops = iface->owner; gradient_addref( &iter->stops->primary );
    EnterCriticalSection( &iter->stops->lock ); iter->observed_version = iter->stops->version; LeaveCriticalSection( &iter->stops->lock );
    *value = &iter->primary; return S_OK;
}
static HRESULT WINAPI gradient_iterator_Current( struct gradient_iface *iface, struct gradient_iface **value )
{
    struct gradient_object *iter = iface->owner;
    HRESULT hr;
    if (!value) return E_POINTER;
    *value = NULL; EnterCriticalSection( &iter->stops->lock );
    hr = iter->observed_version == iter->stops->version ? gradient_vector_GetAt( &iter->stops->primary, iter->position, value ) : E_CHANGED_STATE;
    LeaveCriticalSection( &iter->stops->lock ); return hr;
}
static HRESULT WINAPI gradient_iterator_HasCurrent( struct gradient_iface *iface, boolean *value )
{
    struct gradient_object *iter = iface->owner;
    HRESULT hr = S_OK;
    if (!value) return E_POINTER;
    *value = FALSE; EnterCriticalSection( &iter->stops->lock );
    if (iter->observed_version != iter->stops->version) hr = E_CHANGED_STATE;
    else *value = iter->position < iter->stops->count;
    LeaveCriticalSection( &iter->stops->lock ); return hr;
}
static HRESULT WINAPI gradient_iterator_MoveNext( struct gradient_iface *iface, boolean *value )
{
    struct gradient_object *iter = iface->owner;
    HRESULT hr;
    EnterCriticalSection( &iter->stops->lock );
    hr = gradient_iterator_HasCurrent( iface, value );
    if (SUCCEEDED(hr) && *value) { ++iter->position; *value = iter->position < iter->stops->count; }
    LeaveCriticalSection( &iter->stops->lock ); return hr;
}
static HRESULT WINAPI gradient_iterator_GetMany( struct gradient_iface *iface, UINT32 capacity, struct gradient_iface **values, UINT32 *actual )
{
    struct gradient_object *iter = iface->owner;
    HRESULT hr;
    if (!actual) return E_POINTER;
    *actual = 0; EnterCriticalSection( &iter->stops->lock );
    if (iter->observed_version != iter->stops->version) hr = E_CHANGED_STATE;
    else { hr = gradient_vector_GetMany( &iter->stops->primary, iter->position, capacity, values, actual ); if (SUCCEEDED(hr)) iter->position += *actual; }
    LeaveCriticalSection( &iter->stops->lock ); return hr;
}
static const struct gradient_linear_vtbl gradient_linear_vtbl = {GRADIENT_FORWARD, gradient_get_EndPoint, gradient_put_EndPoint, gradient_get_StartPoint, gradient_put_StartPoint};
static const struct gradient_stop_vtbl gradient_stop_vtbl = {GRADIENT_FORWARD, gradient_get_Color, gradient_put_Color, gradient_get_StopOffset, gradient_put_StopOffset};
static const struct gradient_gradient_vtbl gradient_gradient_vtbl = {GRADIENT_FORWARD, gradient_get_AnchorPoint, gradient_put_AnchorPoint, gradient_get_CenterPoint, gradient_put_CenterPoint, gradient_get_ColorStops, gradient_get_ExtendMode, gradient_put_ExtendMode, gradient_get_InterpolationSpace, gradient_put_InterpolationSpace, gradient_get_Offset, gradient_put_Offset, gradient_get_RotationAngle, gradient_put_RotationAngle, gradient_get_RotationAngleInDegrees, gradient_put_RotationAngleInDegrees, gradient_get_Scale, gradient_put_Scale, gradient_get_TransformMatrix, gradient_put_TransformMatrix};
static const struct gradient_mapping_vtbl gradient_mapping_vtbl = {GRADIENT_FORWARD, gradient_get_MappingMode, gradient_put_MappingMode};
static const struct gradient_inspectable_vtbl gradient_brush_vtbl = {GRADIENT_FORWARD};
static const struct gradient_object_vtbl gradient_object_vtbl = {GRADIENT_FORWARD, gradient_get_Compositor, gradient_get_Dispatcher, gradient_get_Properties, gradient_StartAnimation, gradient_StopAnimation};
static const struct gradient_closable_vtbl gradient_closable_vtbl = {GRADIENT_FORWARD, gradient_Close};
static const struct gradient_vector_vtbl gradient_vector_vtbl = {GRADIENT_FORWARD, gradient_vector_GetAt, gradient_vector_Size, gradient_vector_GetView, gradient_vector_IndexOf, gradient_vector_SetAt, gradient_vector_InsertAt, gradient_vector_RemoveAt, gradient_vector_Append, gradient_vector_RemoveAtEnd, gradient_vector_Clear, gradient_vector_GetMany, gradient_vector_ReplaceAll};
static const struct gradient_view_vtbl gradient_view_vtbl = {GRADIENT_FORWARD, gradient_vector_GetAt, gradient_vector_Size, gradient_vector_IndexOf, gradient_vector_GetMany};
static const struct gradient_iterable_vtbl gradient_iterable_vtbl = {GRADIENT_FORWARD, gradient_iterable_First};
static const struct gradient_iterator_vtbl gradient_iterator_vtbl = {GRADIENT_FORWARD, gradient_iterator_Current, gradient_iterator_HasCurrent, gradient_iterator_MoveNext, gradient_iterator_GetMany};
static HRESULT gradient_create( ICompositor *compositor, enum gradient_kind kind, struct gradient_object **out )
{
    struct gradient_object *impl;
    HRESULT hr;
    *out = NULL;
    if (!(impl = calloc( 1, sizeof(*impl) ))) return E_OUTOFMEMORY;
    impl->kind = kind; impl->ref = 1; impl->compositor = compositor; ICompositor_AddRef( compositor );
    InitializeCriticalSection( &impl->lock );
    impl->primary.owner = impl->base.owner = impl->mapping.owner = impl->brush.owner = impl->object.owner = impl->closable.owner = impl->iterable.owner = impl;
    impl->primary.lpVtbl = kind == GRADIENT_BRUSH ? (const void *)&gradient_linear_vtbl : kind == GRADIENT_STOP ? (const void *)&gradient_stop_vtbl : kind == GRADIENT_COLLECTION ? (const void *)&gradient_vector_vtbl : kind == GRADIENT_VIEW ? (const void *)&gradient_view_vtbl : (const void *)&gradient_iterator_vtbl;
    impl->base.lpVtbl = &gradient_gradient_vtbl; impl->mapping.lpVtbl = &gradient_mapping_vtbl; impl->brush.lpVtbl = &gradient_brush_vtbl;
    impl->object.lpVtbl = &gradient_object_vtbl; impl->closable.lpVtbl = &gradient_closable_vtbl; impl->iterable.lpVtbl = &gradient_iterable_vtbl;
    impl->end.X = impl->end.Y = impl->scale.X = impl->scale.Y = 1;
    impl->transform.M11 = impl->transform.M22 = 1; impl->interpolation = 2; impl->mapping_mode = 1;
    hr = composition_property_set_create( compositor, &impl->properties );
    if (SUCCEEDED(hr) && kind == GRADIENT_BRUSH) hr = gradient_create( compositor, GRADIENT_COLLECTION, &impl->stops );
    if (FAILED(hr)) { gradient_release( &impl->primary ); return hr; }
    *out = impl; return S_OK;
}

#define DEFINE_COMPOSITOR4_CREATE_METHOD(name) \
    static HRESULT WINAPI compositor4_##name( struct compositor4_iface *iface, IInspectable **result ) \
    { \
        FIXME( "iface %p, result %p stub!\n", iface, result ); \
        if (!result) return E_POINTER; \
        *result = NULL; \
        return E_NOTIMPL; \
    }

static HRESULT WINAPI compositor4_CreateColorGradientStop( struct compositor4_iface *iface, IInspectable **result )
{
    struct gradient_object *object;
    HRESULT hr;
    if (!result) return E_POINTER;
    *result = NULL;
    hr = gradient_create( &impl_from_compositor4( iface )->ICompositor_iface, GRADIENT_STOP, &object );
    if (SUCCEEDED(hr)) *result = (IInspectable *)&object->primary;
    return hr;
}
static HRESULT WINAPI compositor4_CreateLinearGradientBrush( struct compositor4_iface *iface, IInspectable **result )
{
    struct gradient_object *object;
    HRESULT hr;
    if (!result) return E_POINTER;
    *result = NULL;
    hr = gradient_create( &impl_from_compositor4( iface )->ICompositor_iface, GRADIENT_BRUSH, &object );
    if (SUCCEEDED(hr)) *result = (IInspectable *)&object->primary;
    return hr;
}
DEFINE_COMPOSITOR4_CREATE_METHOD(CreateSpringScalarAnimation)
DEFINE_COMPOSITOR4_CREATE_METHOD(CreateSpringVector2Animation)
DEFINE_COMPOSITOR4_CREATE_METHOD(CreateSpringVector3Animation)

#undef DEFINE_COMPOSITOR4_CREATE_METHOD

static HRESULT WINAPI compositor4_CreateColorGradientStopWithOffsetAndColor(
        struct compositor4_iface *iface, FLOAT offset, struct Color color, IInspectable **result )
{
    struct gradient_object *object;
    HRESULT hr;
    if (!result) return E_POINTER;
    *result = NULL;
    hr = gradient_create( &impl_from_compositor4( iface )->ICompositor_iface, GRADIENT_STOP, &object );
    if (FAILED(hr)) return hr;
    object->offset = offset;
    object->color = color;
    *result = (IInspectable *)&object->primary;
    return S_OK;
}

static const struct compositor4_vtbl compositor4_vtbl =
{
    compositor4_QueryInterface,
    compositor4_AddRef,
    compositor4_Release,
    compositor4_GetIids,
    compositor4_GetRuntimeClassName,
    compositor4_GetTrustLevel,
    compositor4_CreateColorGradientStop,
    compositor4_CreateColorGradientStopWithOffsetAndColor,
    compositor4_CreateLinearGradientBrush,
    compositor4_CreateSpringScalarAnimation,
    compositor4_CreateSpringVector2Animation,
    compositor4_CreateSpringVector3Animation,
};

static HRESULT WINAPI compositor5_QueryInterface( struct compositor5_iface *iface, REFIID iid,
        void **out )
{
    struct compositor *impl = impl_from_compositor5( iface );
    return ICompositor_QueryInterface( &impl->ICompositor_iface, iid, out );
}

static ULONG WINAPI compositor5_AddRef( struct compositor5_iface *iface )
{
    struct compositor *impl = impl_from_compositor5( iface );
    return ICompositor_AddRef( &impl->ICompositor_iface );
}

static ULONG WINAPI compositor5_Release( struct compositor5_iface *iface )
{
    struct compositor *impl = impl_from_compositor5( iface );
    return ICompositor_Release( &impl->ICompositor_iface );
}

static HRESULT WINAPI compositor5_GetIids( struct compositor5_iface *iface, ULONG *count, IID **iids )
{
    struct compositor *impl = impl_from_compositor5( iface );
    return ICompositor_GetIids( &impl->ICompositor_iface, count, iids );
}

static HRESULT WINAPI compositor5_GetRuntimeClassName( struct compositor5_iface *iface, HSTRING *name )
{
    struct compositor *impl = impl_from_compositor5( iface );
    return ICompositor_GetRuntimeClassName( &impl->ICompositor_iface, name );
}

static HRESULT WINAPI compositor5_GetTrustLevel( struct compositor5_iface *iface, TrustLevel *level )
{
    struct compositor *impl = impl_from_compositor5( iface );
    return ICompositor_GetTrustLevel( &impl->ICompositor_iface, level );
}

static HRESULT WINAPI compositor5_get_Comment( struct compositor5_iface *iface, HSTRING *value )
{
    struct compositor *impl = impl_from_compositor5( iface );

    if (!value) return E_POINTER;
    *value = NULL;
    if (!impl->comment) return S_OK;
    return WindowsDuplicateString( impl->comment, value );
}

static HRESULT WINAPI compositor5_put_Comment( struct compositor5_iface *iface, HSTRING value )
{
    struct compositor *impl = impl_from_compositor5( iface );
    HSTRING copy = NULL;
    HRESULT hr;

    if (value && FAILED(hr = WindowsDuplicateString( value, &copy ))) return hr;
    WindowsDeleteString( impl->comment );
    impl->comment = copy;
    return S_OK;
}

static HRESULT WINAPI compositor5_get_GlobalPlaybackRate( struct compositor5_iface *iface, FLOAT *value )
{
    if (!value) return E_POINTER;
    *value = impl_from_compositor5( iface )->global_playback_rate;
    return S_OK;
}

static HRESULT WINAPI compositor5_put_GlobalPlaybackRate( struct compositor5_iface *iface, FLOAT value )
{
    impl_from_compositor5( iface )->global_playback_rate = value;
    return S_OK;
}

#define DEFINE_COMPOSITOR5_CREATE_METHOD(name) \
    static HRESULT WINAPI compositor5_##name( struct compositor5_iface *iface, \
            IInspectable **result ) \
    { \
        FIXME( "iface %p, result %p stub!\n", iface, result ); \
        if (!result) return E_POINTER; \
        *result = NULL; \
        return E_NOTIMPL; \
    }

DEFINE_COMPOSITOR5_CREATE_METHOD(CreateBounceScalarAnimation)
DEFINE_COMPOSITOR5_CREATE_METHOD(CreateBounceVector2Animation)
DEFINE_COMPOSITOR5_CREATE_METHOD(CreateBounceVector3Animation)
DEFINE_COMPOSITOR5_CREATE_METHOD(CreateContainerShape)
DEFINE_COMPOSITOR5_CREATE_METHOD(CreateEllipseGeometry)
DEFINE_COMPOSITOR5_CREATE_METHOD(CreateLineGeometry)
DEFINE_COMPOSITOR5_CREATE_METHOD(CreatePathGeometry)
DEFINE_COMPOSITOR5_CREATE_METHOD(CreatePathKeyFrameAnimation)
DEFINE_COMPOSITOR5_CREATE_METHOD(CreateRectangleGeometry)
DEFINE_COMPOSITOR5_CREATE_METHOD(CreateRoundedRectangleGeometry)
DEFINE_COMPOSITOR5_CREATE_METHOD(CreateShapeVisual)
DEFINE_COMPOSITOR5_CREATE_METHOD(CreateSpriteShape)
DEFINE_COMPOSITOR5_CREATE_METHOD(CreateViewBox)

#undef DEFINE_COMPOSITOR5_CREATE_METHOD

static HRESULT WINAPI compositor5_CreatePathGeometryWithPath( struct compositor5_iface *iface,
        IInspectable *path, IInspectable **result )
{
    FIXME( "iface %p, path %p, result %p stub!\n", iface, path, result );
    if (!result) return E_POINTER;
    *result = NULL;
    return E_NOTIMPL;
}

static HRESULT WINAPI compositor5_CreateSpriteShapeWithGeometry( struct compositor5_iface *iface,
        IInspectable *geometry, IInspectable **result )
{
    FIXME( "iface %p, geometry %p, result %p stub!\n", iface, geometry, result );
    if (!result) return E_POINTER;
    *result = NULL;
    return E_NOTIMPL;
}

static HRESULT WINAPI compositor5_RequestCommitAsync( struct compositor5_iface *iface,
        IAsyncAction **operation )
{
    struct compositor *impl = impl_from_compositor5( iface );

    compositor_notify_commit_needed( impl );
    return completed_action_create( operation );
}

static const struct compositor5_vtbl compositor5_vtbl =
{
    compositor5_QueryInterface,
    compositor5_AddRef,
    compositor5_Release,
    compositor5_GetIids,
    compositor5_GetRuntimeClassName,
    compositor5_GetTrustLevel,
    compositor5_get_Comment,
    compositor5_put_Comment,
    compositor5_get_GlobalPlaybackRate,
    compositor5_put_GlobalPlaybackRate,
    compositor5_CreateBounceScalarAnimation,
    compositor5_CreateBounceVector2Animation,
    compositor5_CreateBounceVector3Animation,
    compositor5_CreateContainerShape,
    compositor5_CreateEllipseGeometry,
    compositor5_CreateLineGeometry,
    compositor5_CreatePathGeometry,
    compositor5_CreatePathGeometryWithPath,
    compositor5_CreatePathKeyFrameAnimation,
    compositor5_CreateRectangleGeometry,
    compositor5_CreateRoundedRectangleGeometry,
    compositor5_CreateShapeVisual,
    compositor5_CreateSpriteShape,
    compositor5_CreateSpriteShapeWithGeometry,
    compositor5_CreateViewBox,
    compositor5_RequestCommitAsync,
};

static HRESULT WINAPI compositor6_QueryInterface( struct compositor6_iface *iface, REFIID iid,
        void **out )
{
    struct compositor *impl = impl_from_compositor6( iface );
    return ICompositor_QueryInterface( &impl->ICompositor_iface, iid, out );
}

static ULONG WINAPI compositor6_AddRef( struct compositor6_iface *iface )
{
    struct compositor *impl = impl_from_compositor6( iface );
    return ICompositor_AddRef( &impl->ICompositor_iface );
}

static ULONG WINAPI compositor6_Release( struct compositor6_iface *iface )
{
    struct compositor *impl = impl_from_compositor6( iface );
    return ICompositor_Release( &impl->ICompositor_iface );
}

static HRESULT WINAPI compositor6_GetIids( struct compositor6_iface *iface, ULONG *count, IID **iids )
{
    struct compositor *impl = impl_from_compositor6( iface );
    return ICompositor_GetIids( &impl->ICompositor_iface, count, iids );
}

static HRESULT WINAPI compositor6_GetRuntimeClassName( struct compositor6_iface *iface, HSTRING *name )
{
    struct compositor *impl = impl_from_compositor6( iface );
    return ICompositor_GetRuntimeClassName( &impl->ICompositor_iface, name );
}

static HRESULT WINAPI compositor6_GetTrustLevel( struct compositor6_iface *iface, TrustLevel *level )
{
    struct compositor *impl = impl_from_compositor6( iface );
    return ICompositor_GetTrustLevel( &impl->ICompositor_iface, level );
}

#define DEFINE_COMPOSITOR6_CREATE_METHOD(name) \
    static HRESULT WINAPI compositor6_##name( struct compositor6_iface *iface, \
            IInspectable **result ) \
    { \
        FIXME( "iface %p, result %p stub!\n", iface, result ); \
        if (!result) return E_POINTER; \
        *result = NULL; \
        return E_NOTIMPL; \
    }

DEFINE_COMPOSITOR6_CREATE_METHOD(CreateGeometricClip)


#undef DEFINE_COMPOSITOR6_CREATE_METHOD

static HRESULT WINAPI compositor6_CreateBooleanKeyFrameAnimation( struct compositor6_iface *iface,
        IInspectable **result )
{
    TRACE( "iface %p, result %p.\n", iface, result );
    return boolean_key_frame_animation_create( &impl_from_compositor6( iface )->ICompositor_iface,
                                               result );
}

static HRESULT WINAPI compositor6_CreateGeometricClipWithGeometry( struct compositor6_iface *iface,
        IInspectable *geometry, IInspectable **result )
{
    FIXME( "iface %p, geometry %p, result %p stub!\n", iface, geometry, result );
    if (!result) return E_POINTER;
    *result = NULL;
    return E_NOTIMPL;
}

static HRESULT WINAPI compositor6_CreateRedirectVisualWithSourceVisual( struct compositor6_iface *iface,
        IInspectable *visual, IInspectable **result )
{
    struct sprite_visual *impl;
    ISpriteVisual *sprite;
    IVisual *source = NULL;
    HRESULT hr;

    TRACE("iface %p, visual %p, result %p.\n", iface, visual, result);
    if (!result) return E_POINTER;
    *result = NULL;
    if (visual && FAILED(hr = IInspectable_QueryInterface(visual, &IID_IVisual, (void **)&source)))
        return hr;
    hr = compositor_CreateSpriteVisual(&impl_from_compositor6(iface)->ICompositor_iface, &sprite);
    if (SUCCEEDED(hr))
    {
        impl = impl_from_ISpriteVisual(sprite);
        impl->is_redirect = TRUE;
        hr = redirect_visual_put_Source(&impl->redirect_iface, source);
        if (SUCCEEDED(hr)) *result = (IInspectable *)&impl->redirect_iface;
        else ISpriteVisual_Release(sprite);
    }
    if (source) IVisual_Release(source);
    return hr;
}

static HRESULT WINAPI compositor6_CreateRedirectVisual(struct compositor6_iface *iface,
        IInspectable **result)
{
    return compositor6_CreateRedirectVisualWithSourceVisual(iface, NULL, result);
}

static const struct compositor6_vtbl compositor6_vtbl =
{
    compositor6_QueryInterface,
    compositor6_AddRef,
    compositor6_Release,
    compositor6_GetIids,
    compositor6_GetRuntimeClassName,
    compositor6_GetTrustLevel,
    compositor6_CreateGeometricClip,
    compositor6_CreateGeometricClipWithGeometry,
    compositor6_CreateRedirectVisual,
    compositor6_CreateRedirectVisualWithSourceVisual,
    compositor6_CreateBooleanKeyFrameAnimation,
};

static HRESULT WINAPI compositor_closable_QueryInterface( IClosable *iface, REFIID iid, void **out )
{
    struct compositor *impl = compositor_from_IClosable( iface );
    return ICompositor_QueryInterface( &impl->ICompositor_iface, iid, out );
}

static ULONG WINAPI compositor_closable_AddRef( IClosable *iface )
{
    struct compositor *impl = compositor_from_IClosable( iface );
    return ICompositor_AddRef( &impl->ICompositor_iface );
}

static ULONG WINAPI compositor_closable_Release( IClosable *iface )
{
    struct compositor *impl = compositor_from_IClosable( iface );
    return ICompositor_Release( &impl->ICompositor_iface );
}

static HRESULT WINAPI compositor_closable_GetIids( IClosable *iface, ULONG *count, IID **iids )
{
    struct compositor *impl = compositor_from_IClosable( iface );
    return ICompositor_GetIids( &impl->ICompositor_iface, count, iids );
}

static HRESULT WINAPI compositor_closable_GetRuntimeClassName( IClosable *iface, HSTRING *name )
{
    struct compositor *impl = compositor_from_IClosable( iface );
    return ICompositor_GetRuntimeClassName( &impl->ICompositor_iface, name );
}

static HRESULT WINAPI compositor_closable_GetTrustLevel( IClosable *iface, TrustLevel *level )
{
    struct compositor *impl = compositor_from_IClosable( iface );
    return ICompositor_GetTrustLevel( &impl->ICompositor_iface, level );
}

static HRESULT WINAPI compositor_closable_Close( IClosable *iface )
{
    TRACE( "iface %p.\n", iface );
    return S_OK;
}

static const IClosableVtbl compositor_closable_vtbl =
{
    compositor_closable_QueryInterface,
    compositor_closable_AddRef,
    compositor_closable_Release,
    compositor_closable_GetIids,
    compositor_closable_GetRuntimeClassName,
    compositor_closable_GetTrustLevel,
    compositor_closable_Close,
};

static HRESULT WINAPI compositor_desktop_QueryInterface( ICompositorDesktopInterop *iface,
        REFIID iid, void **out )
{
    struct compositor *impl = compositor_from_ICompositorDesktopInterop( iface );
    return ICompositor_QueryInterface( &impl->ICompositor_iface, iid, out );
}

static ULONG WINAPI compositor_desktop_AddRef( ICompositorDesktopInterop *iface )
{
    struct compositor *impl = compositor_from_ICompositorDesktopInterop( iface );
    return ICompositor_AddRef( &impl->ICompositor_iface );
}

static ULONG WINAPI compositor_desktop_Release( ICompositorDesktopInterop *iface )
{
    struct compositor *impl = compositor_from_ICompositorDesktopInterop( iface );
    return ICompositor_Release( &impl->ICompositor_iface );
}

static HRESULT WINAPI compositor_desktop_CreateDesktopWindowTarget( ICompositorDesktopInterop *iface,
        HWND hwnd, BOOL topmost, IDesktopWindowTarget **result )
{
    struct compositor *impl = compositor_from_ICompositorDesktopInterop( iface );
    HRESULT hr;

    FIXME( "iface %p, hwnd %p, topmost %u, result %p.\n", iface, hwnd, topmost, result );
    hr = desktop_window_target_create( impl->dcomp_device, &impl->targets, hwnd, topmost, result );
    TRACE( "desktop target hwnd %p create returned %#lx.\n", hwnd, hr );
    if (SUCCEEDED(hr))
    {
        TRACE( "desktop target hwnd %p notify begin.\n", hwnd );
        compositor_notify_commit_needed( impl );
        TRACE( "desktop target hwnd %p notify end.\n", hwnd );
    }
    return hr;
}

static HRESULT WINAPI compositor_desktop_EnsureOnThread( ICompositorDesktopInterop *iface,
        DWORD thread_id )
{
    FIXME( "iface %p, thread_id %lu.\n", iface, thread_id );
    return S_OK;
}

static const ICompositorDesktopInteropVtbl compositor_desktop_vtbl =
{
    compositor_desktop_QueryInterface,
    compositor_desktop_AddRef,
    compositor_desktop_Release,
    compositor_desktop_CreateDesktopWindowTarget,
    compositor_desktop_EnsureOnThread,
};

static HRESULT WINAPI compositor_interop_QueryInterface( ICompositorInterop *iface, REFIID iid, void **out )
{
    return compositor_QueryInterface( &compositor_from_ICompositorInterop( iface )->ICompositor_iface, iid, out );
}

static ULONG WINAPI compositor_interop_AddRef( ICompositorInterop *iface )
{
    return compositor_AddRef( &compositor_from_ICompositorInterop( iface )->ICompositor_iface );
}

static ULONG WINAPI compositor_interop_Release( ICompositorInterop *iface )
{
    return compositor_Release( &compositor_from_ICompositorInterop( iface )->ICompositor_iface );
}

static inline struct composition_graphics_device *impl_from_ICompositionGraphicsDevice(
        ICompositionGraphicsDevice *iface )
{
    return CONTAINING_RECORD( iface, struct composition_graphics_device, ICompositionGraphicsDevice_iface );
}

static inline struct composition_graphics_device *impl_from_ICompositionGraphicsDevice2(
        ICompositionGraphicsDevice2 *iface )
{
    return CONTAINING_RECORD( iface, struct composition_graphics_device, ICompositionGraphicsDevice2_iface );
}

static inline struct composition_drawing_surface *impl_from_ICompositionDrawingSurface(
        ICompositionDrawingSurface *iface )
{
    return CONTAINING_RECORD( iface, struct composition_drawing_surface, ICompositionDrawingSurface_iface );
}

static inline struct composition_drawing_surface *impl_from_ICompositionDrawingSurfaceInterop(
        ICompositionDrawingSurfaceInterop *iface )
{
    return CONTAINING_RECORD( iface, struct composition_drawing_surface, ICompositionDrawingSurfaceInterop_iface );
}

static inline struct composition_drawing_surface *impl_from_ICompositionVirtualDrawingSurface(
        ICompositionVirtualDrawingSurface *iface )
{
    return CONTAINING_RECORD( iface, struct composition_drawing_surface,
            ICompositionVirtualDrawingSurface_iface );
}

static HRESULT WINAPI composition_drawing_surface_QueryInterface( ICompositionDrawingSurface *iface,
        REFIID iid, void **out )
{
    FIXME( "iface %p, iid %s, out %p.\n", iface, debugstr_guid( iid ), out );
    if (!out) return E_POINTER;
    *out = NULL;
    if (IsEqualGUID( iid, &IID_ICompositionDrawingSurfaceInterop ))
    {
        *out = &impl_from_ICompositionDrawingSurface( iface )->ICompositionDrawingSurfaceInterop_iface;
        ICompositionDrawingSurface_AddRef( iface );
        return S_OK;
    }
    if (IsEqualGUID( iid, &IID_ICompositionVirtualDrawingSurface ) &&
        impl_from_ICompositionDrawingSurface( iface )->virtual)
    {
        *out = &impl_from_ICompositionDrawingSurface( iface )->ICompositionVirtualDrawingSurface_iface;
        ICompositionDrawingSurface_AddRef( iface );
        return S_OK;
    }
    if (!IsEqualGUID( iid, &IID_IUnknown ) && !IsEqualGUID( iid, &IID_IInspectable ) &&
        !IsEqualGUID( iid, &IID_IAgileObject ) && !IsEqualGUID( iid, &IID_ICompositionSurface ) &&
        !IsEqualGUID( iid, &IID_ICompositionDrawingSurface ))
        return E_NOINTERFACE;
    *out = iface;
    ICompositionDrawingSurface_AddRef( iface );
    return S_OK;
}

static ULONG WINAPI composition_drawing_surface_AddRef( ICompositionDrawingSurface *iface )
{
    return InterlockedIncrement( &impl_from_ICompositionDrawingSurface( iface )->ref );
}

static void drawing_surface_free_tile( struct composition_surface_tile *tile )
{
    ULONG ref;

    list_remove( &tile->entry );
    DeleteObject( tile->valid );
    ref = ID2D1Bitmap1_Release( tile->bitmap );
    TRACE( "Released virtual tile {%ld,%ld}, bitmap ref %lu.\n", tile->origin.x, tile->origin.y, ref );
    free( tile );
}

static void drawing_surface_clear_tiles( struct composition_drawing_surface *surface )
{
    struct composition_surface_tile *tile, *next;

    LIST_FOR_EACH_ENTRY_SAFE( tile, next, &surface->tiles, struct composition_surface_tile, entry )
    {
        drawing_surface_free_tile( tile );
    }
}

static ULONG WINAPI composition_drawing_surface_Release( ICompositionDrawingSurface *iface )
{
    struct composition_drawing_surface *impl = impl_from_ICompositionDrawingSurface( iface );
    ULONG ref = InterlockedDecrement( &impl->ref );

    if (!ref)
    {
        drawing_surface_clear_tiles( impl );
        if (impl->bitmap) ID2D1Bitmap1_Release( impl->bitmap );
        if (impl->committed_bitmap) ID2D1Bitmap1_Release( impl->committed_bitmap );
        if (impl->context) ID2D1DeviceContext_Release( impl->context );
        ID2D1Device_Release( impl->device );
        free( impl );
    }
    return ref;
}

static HRESULT WINAPI composition_drawing_surface_GetIids( ICompositionDrawingSurface *iface,
        ULONG *count, IID **iids )
{
    struct composition_drawing_surface *impl = impl_from_ICompositionDrawingSurface( iface );
    ULONG iid_count = impl->virtual ? 3 : 2;

    if (!count || !iids) return E_POINTER;
    if (!(*iids = CoTaskMemAlloc( iid_count * sizeof(**iids) ))) return E_OUTOFMEMORY;
    (*iids)[0] = IID_ICompositionDrawingSurface;
    (*iids)[1] = IID_ICompositionSurface;
    if (impl->virtual) (*iids)[2] = IID_ICompositionVirtualDrawingSurface;
    *count = iid_count;
    return S_OK;
}

static HRESULT WINAPI composition_drawing_surface_GetRuntimeClassName( ICompositionDrawingSurface *iface,
        HSTRING *name )
{
    if (!name) return E_POINTER;
    if (impl_from_ICompositionDrawingSurface( iface )->virtual)
        return WindowsCreateString( RuntimeClass_Windows_UI_Composition_CompositionVirtualDrawingSurface,
                ARRAY_SIZE(RuntimeClass_Windows_UI_Composition_CompositionVirtualDrawingSurface) - 1, name );
    return WindowsCreateString( RuntimeClass_Windows_UI_Composition_CompositionDrawingSurface,
            ARRAY_SIZE(RuntimeClass_Windows_UI_Composition_CompositionDrawingSurface) - 1, name );
}

static HRESULT WINAPI composition_drawing_surface_GetTrustLevel( ICompositionDrawingSurface *iface,
        TrustLevel *level )
{
    if (!level) return E_POINTER;
    *level = BaseTrust;
    return S_OK;
}

static HRESULT WINAPI composition_drawing_surface_get_AlphaMode( ICompositionDrawingSurface *iface,
        enum __x_ABI_CWindows_CGraphics_CDirectX_CDirectXAlphaMode *value )
{
    if (!value) return E_POINTER;
    *value = impl_from_ICompositionDrawingSurface( iface )->mode;
    return S_OK;
}

static HRESULT WINAPI composition_drawing_surface_get_PixelFormat( ICompositionDrawingSurface *iface,
        enum __x_ABI_CWindows_CGraphics_CDirectX_CDirectXPixelFormat *value )
{
    if (!value) return E_POINTER;
    *value = impl_from_ICompositionDrawingSurface( iface )->format;
    return S_OK;
}

static HRESULT WINAPI composition_drawing_surface_get_Size( ICompositionDrawingSurface *iface, Size *value )
{
    if (!value) return E_POINTER;
    *value = impl_from_ICompositionDrawingSurface( iface )->size;
    return S_OK;
}

static const ICompositionDrawingSurfaceVtbl composition_drawing_surface_vtbl =
{
    composition_drawing_surface_QueryInterface,
    composition_drawing_surface_AddRef,
    composition_drawing_surface_Release,
    composition_drawing_surface_GetIids,
    composition_drawing_surface_GetRuntimeClassName,
    composition_drawing_surface_GetTrustLevel,
    composition_drawing_surface_get_AlphaMode,
    composition_drawing_surface_get_PixelFormat,
    composition_drawing_surface_get_Size,
};

static HRESULT WINAPI composition_virtual_drawing_surface_QueryInterface(
        ICompositionVirtualDrawingSurface *iface, REFIID iid, void **out )
{
    return composition_drawing_surface_QueryInterface(
            &impl_from_ICompositionVirtualDrawingSurface( iface )->ICompositionDrawingSurface_iface,
            iid, out );
}

static ULONG WINAPI composition_virtual_drawing_surface_AddRef( ICompositionVirtualDrawingSurface *iface )
{
    return composition_drawing_surface_AddRef(
            &impl_from_ICompositionVirtualDrawingSurface( iface )->ICompositionDrawingSurface_iface );
}

static ULONG WINAPI composition_virtual_drawing_surface_Release( ICompositionVirtualDrawingSurface *iface )
{
    return composition_drawing_surface_Release(
            &impl_from_ICompositionVirtualDrawingSurface( iface )->ICompositionDrawingSurface_iface );
}

static HRESULT WINAPI composition_virtual_drawing_surface_GetIids( ICompositionVirtualDrawingSurface *iface,
        ULONG *count, IID **iids )
{
    return composition_drawing_surface_GetIids(
            &impl_from_ICompositionVirtualDrawingSurface( iface )->ICompositionDrawingSurface_iface,
            count, iids );
}

static HRESULT WINAPI composition_virtual_drawing_surface_GetRuntimeClassName(
        ICompositionVirtualDrawingSurface *iface, HSTRING *name )
{
    return composition_drawing_surface_GetRuntimeClassName(
            &impl_from_ICompositionVirtualDrawingSurface( iface )->ICompositionDrawingSurface_iface,
            name );
}

static HRESULT WINAPI composition_virtual_drawing_surface_GetTrustLevel(
        ICompositionVirtualDrawingSurface *iface, TrustLevel *level )
{
    return composition_drawing_surface_GetTrustLevel(
            &impl_from_ICompositionVirtualDrawingSurface( iface )->ICompositionDrawingSurface_iface,
            level );
}

static HRESULT WINAPI composition_virtual_drawing_surface_Trim( ICompositionVirtualDrawingSurface *iface,
        UINT32 count, struct RectInt32 *rects )
{
    struct composition_drawing_surface *surface = impl_from_ICompositionVirtualDrawingSurface( iface );
    struct composition_surface_tile *tile, *next;
    HRGN keep, region;
    UINT32 i;
    int type;

    TRACE( "iface %p, count %u, rects %p.\n", iface, count, rects );
    if (count && !rects) return E_POINTER;
    if (surface->drawing) return D2DERR_WRONG_STATE;
    for (i = 0; i < count; ++i)
        if (rects[i].X < 0 || rects[i].Y < 0 || rects[i].Width < 0 || rects[i].Height < 0 ||
                (LONGLONG)rects[i].X + rects[i].Width > surface->size.Width ||
                (LONGLONG)rects[i].Y + rects[i].Height > surface->size.Height) return E_INVALIDARG;
    if (!(keep = CreateRectRgn( 0, 0, 0, 0 ))) return E_OUTOFMEMORY;
    for (i = 0; i < count; ++i)
    {
        if (!(region = CreateRectRgn( rects[i].X, rects[i].Y,
                rects[i].X + rects[i].Width, rects[i].Y + rects[i].Height )))
        {
            DeleteObject( keep );
            return E_OUTOFMEMORY;
        }
        type = CombineRgn( keep, keep, region, RGN_OR );
        DeleteObject( region );
        if (type == ERROR) { DeleteObject( keep ); return E_OUTOFMEMORY; }
    }
    LIST_FOR_EACH_ENTRY_SAFE( tile, next, &surface->tiles, struct composition_surface_tile, entry )
    {
        OffsetRgn( keep, -tile->origin.x, -tile->origin.y );
        type = CombineRgn( tile->valid, tile->valid, keep, RGN_AND );
        OffsetRgn( keep, tile->origin.x, tile->origin.y );
        if (type == ERROR) { DeleteObject( keep ); return E_OUTOFMEMORY; }
        if (type == NULLREGION) drawing_surface_free_tile( tile );
    }
    DeleteObject( keep );
    if (!count && surface->bitmap)
    {
        ID2D1DeviceContext_SetTarget( surface->context, NULL );
        ID2D1Bitmap1_Release( surface->bitmap );
        surface->bitmap = NULL;
    }
    return S_OK;
}

static const ICompositionVirtualDrawingSurfaceVtbl composition_virtual_drawing_surface_vtbl =
{
    composition_virtual_drawing_surface_QueryInterface,
    composition_virtual_drawing_surface_AddRef,
    composition_virtual_drawing_surface_Release,
    composition_virtual_drawing_surface_GetIids,
    composition_virtual_drawing_surface_GetRuntimeClassName,
    composition_virtual_drawing_surface_GetTrustLevel,
    composition_virtual_drawing_surface_Trim,
};

static HRESULT WINAPI composition_drawing_surface_interop_QueryInterface(
        ICompositionDrawingSurfaceInterop *iface, REFIID iid, void **out )
{
    return composition_drawing_surface_QueryInterface(
            &impl_from_ICompositionDrawingSurfaceInterop( iface )->ICompositionDrawingSurface_iface, iid, out );
}

static ULONG WINAPI composition_drawing_surface_interop_AddRef( ICompositionDrawingSurfaceInterop *iface )
{
    return composition_drawing_surface_AddRef(
            &impl_from_ICompositionDrawingSurfaceInterop( iface )->ICompositionDrawingSurface_iface );
}

static ULONG WINAPI composition_drawing_surface_interop_Release( ICompositionDrawingSurfaceInterop *iface )
{
    return composition_drawing_surface_Release(
            &impl_from_ICompositionDrawingSurfaceInterop( iface )->ICompositionDrawingSurface_iface );
}

static HRESULT WINAPI composition_drawing_surface_interop_BeginDraw( ICompositionDrawingSurfaceInterop *iface,
        const RECT *update_rect, REFIID iid, void **update_object, POINT *update_offset )
{
    struct composition_drawing_surface *impl = impl_from_ICompositionDrawingSurfaceInterop( iface );
    D2D1_BITMAP_PROPERTIES1 desc = {0};
    D2D1_COLOR_F clear = {0.0f, 0.0f, 0.0f, 0.0f};
    D2D1_SIZE_U bitmap_size, size;
    IDXGISurface *surface;
    HRESULT hr;

    FIXME( "iface %p, update_rect %s, iid %s, update_object %p, update_offset %p.\n",
            iface, wine_dbgstr_rect( update_rect ), debugstr_guid( iid ), update_object, update_offset );
    if (!update_object || !update_offset) return E_POINTER;
    *update_object = NULL;
    if (update_offset) update_offset->x = update_offset->y = 0;
    if (!IsEqualGUID( iid, &IID_IDXGISurface ) && !IsEqualGUID( iid, &IID_ID2D1DeviceContext ))
        return E_NOINTERFACE;
    if (impl->drawing) return D2DERR_WRONG_STATE;
    if (update_rect && (update_rect->left < 0 || update_rect->top < 0 ||
            update_rect->right <= update_rect->left || update_rect->bottom <= update_rect->top ||
            update_rect->right > impl->size.Width || update_rect->bottom > impl->size.Height))
        return E_INVALIDARG;
    if (impl->virtual && !update_rect) return E_INVALIDARG;
    if (!impl->context && FAILED(hr = ID2D1Device_CreateDeviceContext( impl->device,
            D2D1_DEVICE_CONTEXT_OPTIONS_NONE, &impl->context ))) return hr;
    if (impl->virtual)
    {
        size.width = update_rect->right - update_rect->left;
        size.height = update_rect->bottom - update_rect->top;
        if (impl->bitmap)
        {
            bitmap_size = ID2D1Bitmap1_GetPixelSize( impl->bitmap );
            if (bitmap_size.width != size.width || bitmap_size.height != size.height)
            {
                ID2D1Bitmap1_Release( impl->bitmap );
                impl->bitmap = NULL;
            }
        }
    }
    else
    {
        size.width = impl->size.Width;
        size.height = impl->size.Height;
    }
    if (!impl->bitmap)
    {
        desc.pixelFormat.format = (DXGI_FORMAT)impl->format;
        desc.pixelFormat.alphaMode = (D2D1_ALPHA_MODE)impl->mode;
        desc.dpiX = desc.dpiY = 96.0f;
        desc.bitmapOptions = D2D1_BITMAP_OPTIONS_TARGET;
        if (FAILED(hr = ID2D1DeviceContext_CreateBitmap( impl->context, size, NULL, 0,
                &desc, &impl->bitmap ))) return hr;
        ID2D1DeviceContext_SetTarget( impl->context, (ID2D1Image *)impl->bitmap );
        ID2D1DeviceContext_BeginDraw( impl->context );
        ID2D1DeviceContext_Clear( impl->context, &clear );
        if (FAILED(hr = ID2D1DeviceContext_EndDraw( impl->context, NULL, NULL ))) return hr;
    }
    if (IsEqualGUID( iid, &IID_ID2D1DeviceContext ))
    {
        ID2D1DeviceContext_SetTarget( impl->context, (ID2D1Image *)impl->bitmap );
        ID2D1DeviceContext_BeginDraw( impl->context );
        impl->drawing = TRUE;
        ID2D1DeviceContext_AddRef( impl->context );
        *update_object = impl->context;
    }
    else
    {
        if (FAILED(hr = ID2D1Bitmap1_GetSurface( impl->bitmap, &surface ))) return hr;
        *update_object = surface;
    }
    impl->update_rect = update_rect ? *update_rect : (RECT){0, 0, size.width, size.height};
    if (impl->virtual)
        impl->bitmap_offset = (POINT){update_rect->left, update_rect->top};
    else
        *update_offset = (POINT){impl->update_rect.left, impl->update_rect.top};
    return S_OK;
}

static struct composition_drawing_surface *drawing_surface_from_composition_surface(
        ICompositionSurface *surface )
{
    ICompositionDrawingSurface *drawing_surface;
    struct composition_drawing_surface *impl;

    if (!surface || FAILED(ICompositionSurface_QueryInterface( surface,
            &IID_ICompositionDrawingSurface, (void **)&drawing_surface ))) return NULL;
    impl = impl_from_ICompositionDrawingSurface( drawing_surface );
    ICompositionDrawingSurface_Release( drawing_surface );
    return impl;
}

static struct composition_drawing_surface *drawing_surface_from_surface_brush(
        struct surface_brush *brush )
{
    return drawing_surface_from_composition_surface( brush->surface );
}

static void draw_surface_brush( ID2D1DeviceContext *context, struct surface_brush *brush,
        const D2D1_RECT_F *bounds, FLOAT opacity )
{
    struct composition_drawing_surface *surface = drawing_surface_from_surface_brush( brush );
    D2D1_INTERPOLATION_MODE interpolation;
    ID2D1Bitmap1 *bitmap;
    struct composition_surface_tile *tile;
    D2D1_RECT_F source_rect, dest_rect, tile_rect;
    D2D1_SIZE_F size, bitmap_size;
    FLOAT scale, scale_x, scale_y;
    RGNDATA *data;
    D2D1_RECT_F clip;
    RECT *rects;
    DWORD bytes, i;

    if (!surface) return;
    bitmap = surface->virtual ? surface->bitmap : surface->committed_bitmap;
    if (!surface->virtual && !bitmap) return;
    bitmap_size = bitmap ? ID2D1Bitmap1_GetSize( bitmap ) : (D2D1_SIZE_F){0.0f, 0.0f};
    size = surface->virtual ? (D2D1_SIZE_F){surface->size.Width, surface->size.Height} : bitmap_size;
    if (size.width <= 0.0f || size.height <= 0.0f) return;
    source_rect = (D2D1_RECT_F){0.0f, 0.0f, bitmap_size.width, bitmap_size.height};
    dest_rect = *bounds;

    if (brush->stretch == CompositionStretch_None)
    {
        dest_rect.left += (bounds->right - bounds->left - size.width) * brush->horizontal_alignment_ratio;
        dest_rect.top += (bounds->bottom - bounds->top - size.height) * brush->vertical_alignment_ratio;
        dest_rect.right = dest_rect.left + size.width;
        dest_rect.bottom = dest_rect.top + size.height;
    }
    else if (brush->stretch == CompositionStretch_Uniform ||
            brush->stretch == CompositionStretch_UniformToFill)
    {
        FLOAT scale_x = (bounds->right - bounds->left) / size.width;
        FLOAT scale_y = (bounds->bottom - bounds->top) / size.height;

        scale = brush->stretch == CompositionStretch_Uniform ? min( scale_x, scale_y ) :
                max( scale_x, scale_y );
        dest_rect.left += (bounds->right - bounds->left - size.width * scale) *
                brush->horizontal_alignment_ratio;
        dest_rect.top += (bounds->bottom - bounds->top - size.height * scale) *
                brush->vertical_alignment_ratio;
        dest_rect.right = dest_rect.left + size.width * scale;
        dest_rect.bottom = dest_rect.top + size.height * scale;
    }

    interpolation = brush->interpolation_mode == CompositionBitmapInterpolationMode_NearestNeighbor ?
            D2D1_INTERPOLATION_MODE_NEAREST_NEIGHBOR : D2D1_INTERPOLATION_MODE_LINEAR;
    ID2D1DeviceContext_PushAxisAlignedClip( context, bounds, D2D1_ANTIALIAS_MODE_ALIASED );
    if (surface->virtual)
    {
        scale_x = (dest_rect.right - dest_rect.left) / size.width;
        scale_y = (dest_rect.bottom - dest_rect.top) / size.height;
        LIST_FOR_EACH_ENTRY( tile, &surface->tiles, struct composition_surface_tile, entry )
        {
            bitmap_size = ID2D1Bitmap1_GetSize( tile->bitmap );
            source_rect = (D2D1_RECT_F){0.0f, 0.0f, bitmap_size.width, bitmap_size.height};
            tile_rect.left = dest_rect.left + tile->origin.x * scale_x;
            tile_rect.top = dest_rect.top + tile->origin.y * scale_y;
            tile_rect.right = tile_rect.left + bitmap_size.width * scale_x;
            tile_rect.bottom = tile_rect.top + bitmap_size.height * scale_y;
            if (!(bytes = GetRegionData( tile->valid, 0, NULL )) || !(data = malloc( bytes ))) continue;
            if (!GetRegionData( tile->valid, bytes, data )) { free( data ); continue; }
            rects = (RECT *)data->Buffer;
            for (i = 0; i < data->rdh.nCount; ++i)
            {
                clip = (D2D1_RECT_F){tile_rect.left + rects[i].left * scale_x,
                        tile_rect.top + rects[i].top * scale_y, tile_rect.left + rects[i].right * scale_x,
                        tile_rect.top + rects[i].bottom * scale_y};
                ID2D1DeviceContext_PushAxisAlignedClip( context, &clip, D2D1_ANTIALIAS_MODE_ALIASED );
                ID2D1DeviceContext_DrawBitmap( context, (ID2D1Bitmap *)tile->bitmap, &tile_rect,
                        opacity, interpolation, &source_rect, NULL );
                ID2D1DeviceContext_PopAxisAlignedClip( context );
            }
            free( data );
        }
    }
    else
        ID2D1DeviceContext_DrawBitmap( context, (ID2D1Bitmap *)bitmap, &dest_rect,
                opacity, interpolation, &source_rect, NULL );
    ID2D1DeviceContext_PopAxisAlignedClip( context );
}

static void draw_nine_grid_brush( ID2D1DeviceContext *context, struct nine_grid_brush *brush,
        const D2D1_RECT_F *bounds, FLOAT opacity )
{
    struct surface_brush *source_brush;
    struct composition_drawing_surface *surface;
    D2D1_INTERPOLATION_MODE interpolation;
    ID2D1Bitmap1 *bitmap;
    D2D1_RECT_F source_rect, dest_rect;
    D2D1_SIZE_F size;
    FLOAT source_x[4], source_y[4], dest_x[4], dest_y[4];
    FLOAT left, top, right, bottom, dest_left, dest_top, dest_right, dest_bottom;
    UINT x, y;

    if (!brush->source || brush->source->lpVtbl != &surface_composition_brush_vtbl) return;
    source_brush = surface_brush_from_ICompositionBrush( brush->source );
    if (!(surface = drawing_surface_from_surface_brush( source_brush ))) return;
    bitmap = surface->virtual ? surface->bitmap : surface->committed_bitmap;
    if (!bitmap) return;
    size = ID2D1Bitmap1_GetSize( bitmap );
    if (size.width <= 0.0f || size.height <= 0.0f) return;

    left = min( max( brush->left_inset, 0.0f ), size.width );
    right = min( max( brush->right_inset, 0.0f ), size.width - left );
    top = min( max( brush->top_inset, 0.0f ), size.height );
    bottom = min( max( brush->bottom_inset, 0.0f ), size.height - top );
    dest_left = min( left * brush->left_inset_scale, (bounds->right - bounds->left) / 2.0f );
    dest_right = min( right * brush->right_inset_scale,
            bounds->right - bounds->left - dest_left );
    dest_top = min( top * brush->top_inset_scale, (bounds->bottom - bounds->top) / 2.0f );
    dest_bottom = min( bottom * brush->bottom_inset_scale,
            bounds->bottom - bounds->top - dest_top );
    source_x[0] = 0.0f;
    source_x[1] = left;
    source_x[2] = size.width - right;
    source_x[3] = size.width;
    source_y[0] = 0.0f;
    source_y[1] = top;
    source_y[2] = size.height - bottom;
    source_y[3] = size.height;
    dest_x[0] = bounds->left;
    dest_x[1] = bounds->left + dest_left;
    dest_x[2] = bounds->right - dest_right;
    dest_x[3] = bounds->right;
    dest_y[0] = bounds->top;
    dest_y[1] = bounds->top + dest_top;
    dest_y[2] = bounds->bottom - dest_bottom;
    dest_y[3] = bounds->bottom;
    interpolation = source_brush->interpolation_mode == CompositionBitmapInterpolationMode_NearestNeighbor ?
            D2D1_INTERPOLATION_MODE_NEAREST_NEIGHBOR : D2D1_INTERPOLATION_MODE_LINEAR;

    ID2D1DeviceContext_PushAxisAlignedClip( context, bounds, D2D1_ANTIALIAS_MODE_ALIASED );
    for (y = 0; y < 3; ++y)
    {
        for (x = 0; x < 3; ++x)
        {
            if (brush->center_hollow && x == 1 && y == 1) continue;
            if (source_x[x] >= source_x[x + 1] || source_y[y] >= source_y[y + 1] ||
                    dest_x[x] >= dest_x[x + 1] || dest_y[y] >= dest_y[y + 1]) continue;
            source_rect = (D2D1_RECT_F){source_x[x], source_y[y], source_x[x + 1], source_y[y + 1]};
            dest_rect = (D2D1_RECT_F){dest_x[x], dest_y[y], dest_x[x + 1], dest_y[y + 1]};
            ID2D1DeviceContext_DrawBitmap( context, (ID2D1Bitmap *)bitmap, &dest_rect,
                    opacity, interpolation, &source_rect, NULL );
        }
    }
    ID2D1DeviceContext_PopAxisAlignedClip( context );
}

struct gradient_render_stop
{
    D2D1_GRADIENT_STOP value;
    UINT32 order;
};

static int gradient_compare_stops( const void *a, const void *b )
{
    const struct gradient_render_stop *left = a, *right = b;
    if (left->value.position < right->value.position) return -1;
    if (left->value.position > right->value.position) return 1;
    return left->order < right->order ? -1 : left->order > right->order;
}

static HRESULT draw_gradient_brush( ID2D1DeviceContext *context, struct gradient_object *impl,
        const D2D1_RECT_F *bounds, FLOAT opacity )
{
    struct gradient_render_stop *sorted = NULL;
    struct gradient_iface *view = NULL;
    struct gradient_object state = {0};
    D2D1_GRADIENT_STOP *stops = NULL;
    ID2D1GradientStopCollection *collection = NULL;
    ID2D1LinearGradientBrush *brush = NULL;
    D2D1_LINEAR_GRADIENT_BRUSH_PROPERTIES points;
    D2D1_BRUSH_PROPERTIES properties = {0};
    struct Color color;
    struct Matrix3x2 *matrix;
    FLOAT width, height, sine, cosine, x, y, cx, cy;
    UINT32 i, count;
    HRESULT hr;

    EnterCriticalSection( &impl->lock );
    if (impl->closed) hr = RO_E_CLOSED;
    else
    {
        state.start = impl->start; state.end = impl->end;
        state.anchor = impl->anchor; state.center = impl->center;
        state.scale = impl->scale; state.translation = impl->translation;
        state.transform = impl->transform; state.rotation = impl->rotation;
        state.mapping_mode = impl->mapping_mode; state.extend = impl->extend;
        state.interpolation = impl->interpolation;
        hr = gradient_vector_GetView( &impl->stops->primary, &view );
    }
    LeaveCriticalSection( &impl->lock );
    if (FAILED(hr)) return hr;
    count = view->owner->count;
    if (!count) goto done;
    if ((SIZE_T)count > SIZE_MAX / sizeof(*sorted) || (SIZE_T)count > SIZE_MAX / sizeof(*stops))
    { hr = E_OUTOFMEMORY; goto done; }
    if (!(sorted = calloc( count, sizeof(*sorted) )) || !(stops = calloc( count, sizeof(*stops) )))
    { hr = E_OUTOFMEMORY; goto done; }
    for (i = 0; i < count; ++i)
    {
        struct gradient_iface *stop = view->owner->items[i];
        const struct gradient_stop_vtbl *vtbl = stop->lpVtbl;
        if (FAILED(hr = vtbl->get_Color( stop, &color ))) goto done;
        if (FAILED(hr = vtbl->get_Offset( stop, &sorted[i].value.position ))) goto done;
        if (!isfinite( sorted[i].value.position )) { hr = E_INVALIDARG; goto done; }
        sorted[i].value.color = (D2D1_COLOR_F){color.R / 255.0f, color.G / 255.0f,
                color.B / 255.0f, color.A / 255.0f};
        sorted[i].order = i;
    }
    qsort( sorted, count, sizeof(*sorted), gradient_compare_stops );
    for (i = 0; i < count; ++i) stops[i] = sorted[i].value;
    if ((state.interpolation != 2 && state.interpolation != 4) || state.extend < 0 || state.extend > 2 ||
            state.mapping_mode < 0 || state.mapping_mode > 1)
    { hr = E_INVALIDARG; goto done; }
    hr = ID2D1RenderTarget_CreateGradientStopCollection( (ID2D1RenderTarget *)context, stops, count,
            state.interpolation == 2 ? D2D1_GAMMA_2_2 : D2D1_GAMMA_1_0,
            state.extend, &collection );
    if (FAILED(hr)) goto done;
    width = bounds->right - bounds->left;
    height = bounds->bottom - bounds->top;
    points.startPoint = (D2D1_POINT_2F){state.start.X, state.start.Y};
    points.endPoint = (D2D1_POINT_2F){state.end.X, state.end.Y};
    if (state.mapping_mode)
    {
        points.startPoint.x *= width; points.startPoint.y *= height;
        points.endPoint.x *= width; points.endPoint.y *= height;
    }
    sine = sinf( state.rotation ); cosine = cosf( state.rotation );
    cx = state.center.X; cy = state.center.Y;
    x = cx - cx * state.scale.X * cosine + cy * state.scale.Y * sine;
    y = cy - cx * state.scale.X * sine - cy * state.scale.Y * cosine;
    matrix = &state.transform;
    properties.transform._11 = state.scale.X * (cosine * matrix->M11 + sine * matrix->M21);
    properties.transform._12 = state.scale.X * (cosine * matrix->M12 + sine * matrix->M22);
    properties.transform._21 = state.scale.Y * (-sine * matrix->M11 + cosine * matrix->M21);
    properties.transform._22 = state.scale.Y * (-sine * matrix->M12 + cosine * matrix->M22);
    properties.transform._31 = x * matrix->M11 + y * matrix->M21 + matrix->M31 +
            state.translation.X - state.anchor.X * width + bounds->left;
    properties.transform._32 = x * matrix->M12 + y * matrix->M22 + matrix->M32 +
            state.translation.Y - state.anchor.Y * height + bounds->top;
    properties.opacity = opacity;
    hr = ID2D1DeviceContext_CreateLinearGradientBrush( context, &points, &properties, collection, &brush );
    if (FAILED(hr)) goto done;
    ID2D1DeviceContext_FillRectangle( context, bounds, (ID2D1Brush *)brush );

done:
    if (brush) ID2D1LinearGradientBrush_Release( brush );
    if (collection) ID2D1GradientStopCollection_Release( collection );
    if (view) gradient_release( view );
    free( stops ); free( sorted );
    return hr;
}

static HRESULT draw_composition_brush( ID2D1DeviceContext *context, ICompositionBrush *brush,
        const D2D1_RECT_F *bounds, FLOAT opacity, UINT depth );

struct effect_render_sources
{
    struct composition_effect *effect;
    ID2D1DeviceContext *context;
    D2D1_SIZE_F size;
    UINT depth;
};

static HRESULT effect_render_source( void *data, HSTRING name, ID2D1Image **image )
{
    struct effect_render_sources *sources = data;
    D2D1_PIXEL_FORMAT format = {DXGI_FORMAT_R32G32B32A32_FLOAT, D2D1_ALPHA_MODE_PREMULTIPLIED};
    D2D1_COLOR_F clear = {0.0f, 0.0f, 0.0f, 0.0f};
    D2D1_RECT_F bounds = {0.0f, 0.0f, sources->size.width, sources->size.height};
    ID2D1BitmapRenderTarget *target = NULL;
    ID2D1DeviceContext *context = NULL;
    ID2D1Bitmap *bitmap = NULL;
    ICompositionBrush *brush = NULL;
    HRESULT hr, end;

    *image = NULL;
    if (FAILED(hr = effect_brush_GetSourceParameter( &sources->effect->ICompositionEffectBrush_iface,
            name, &brush ))) return hr;
    if (!brush) return E_INVALIDARG;
    if (FAILED(hr = ID2D1DeviceContext_CreateCompatibleRenderTarget( sources->context, &sources->size,
            NULL, &format, D2D1_COMPATIBLE_RENDER_TARGET_OPTIONS_NONE, &target ))) goto done;
    if (FAILED(hr = ID2D1BitmapRenderTarget_QueryInterface( target, &IID_ID2D1DeviceContext,
            (void **)&context ))) goto done;
    ID2D1DeviceContext_BeginDraw( context );
    ID2D1DeviceContext_Clear( context, &clear );
    hr = draw_composition_brush( context, brush, &bounds, 1.0f, sources->depth + 1 );
    end = ID2D1DeviceContext_EndDraw( context, NULL, NULL );
    if (SUCCEEDED(hr)) hr = end;
    if (FAILED(hr)) goto done;
    if (SUCCEEDED(hr = ID2D1BitmapRenderTarget_GetBitmap( target, &bitmap )))
    {
        *image = (ID2D1Image *)bitmap;
        bitmap = NULL;
    }
done:
    if (bitmap) ID2D1Bitmap_Release( bitmap );
    if (context) ID2D1DeviceContext_Release( context );
    if (target) ID2D1BitmapRenderTarget_Release( target );
    ICompositionBrush_Release( brush );
    return hr;
}

static HRESULT draw_effect_brush( ID2D1DeviceContext *context, struct composition_effect *effect,
        const D2D1_RECT_F *bounds, FLOAT opacity, UINT depth )
{
    D2D1_PIXEL_FORMAT format = {DXGI_FORMAT_R32G32B32A32_FLOAT, D2D1_ALPHA_MODE_PREMULTIPLIED};
    D2D1_COLOR_F clear = {0.0f, 0.0f, 0.0f, 0.0f};
    struct effect_render_sources sources = {effect, NULL,
            {bounds->right - bounds->left, bounds->bottom - bounds->top}, depth};
    ID2D1BitmapRenderTarget *target = NULL;
    ID2D1DeviceContext *output = NULL;
    ID2D1Bitmap *bitmap = NULL;
    ID2D1Image *image = NULL;
    HRESULT hr, end;

    if (InterlockedCompareExchange( &effect->closed, 0, 0 )) return RO_E_CLOSED;
    if (FAILED(hr = ID2D1DeviceContext_CreateCompatibleRenderTarget( context, &sources.size, NULL,
            &format, D2D1_COMPATIBLE_RENDER_TARGET_OPTIONS_NONE, &target ))) goto done;
    if (FAILED(hr = ID2D1BitmapRenderTarget_QueryInterface( target, &IID_ID2D1DeviceContext,
            (void **)&output ))) goto done;
    sources.context = output;
    hr = composition_effect_graph_create_image_properties( effect->graph, output, effect->properties,
            effect_render_source, &sources, &image );
    if (FAILED(hr)) goto done;
    ID2D1DeviceContext_BeginDraw( output );
    ID2D1DeviceContext_Clear( output, &clear );
    ID2D1DeviceContext_DrawImage( output, image, NULL, NULL, D2D1_INTERPOLATION_MODE_LINEAR,
            D2D1_COMPOSITE_MODE_SOURCE_OVER );
    end = ID2D1DeviceContext_EndDraw( output, NULL, NULL );
    if (FAILED(hr = end)) goto done;
    if (FAILED(hr = ID2D1BitmapRenderTarget_GetBitmap( target, &bitmap ))) goto done;
    ID2D1DeviceContext_DrawBitmap( context, bitmap, bounds, opacity,
            D2D1_INTERPOLATION_MODE_LINEAR, NULL, NULL );
done:
    if (bitmap) ID2D1Bitmap_Release( bitmap );
    if (image) ID2D1Image_Release( image );
    if (output) ID2D1DeviceContext_Release( output );
    if (target) ID2D1BitmapRenderTarget_Release( target );
    return hr;
}

static HRESULT draw_composition_brush( ID2D1DeviceContext *context, ICompositionBrush *brush,
        const D2D1_RECT_F *bounds, FLOAT opacity, UINT depth )
{
    if (depth > 64) return D2DERR_CYCLIC_GRAPH;
    if (brush->lpVtbl == &composition_brush_vtbl)
    {
        struct Color value = color_brush_from_ICompositionBrush( brush )->color;
        D2D1_COLOR_F color = {value.R / 255.0f, value.G / 255.0f,
                value.B / 255.0f, opacity * value.A / 255.0f};
        ID2D1SolidColorBrush *solid;
        HRESULT hr;

        if (FAILED(hr = ID2D1DeviceContext_CreateSolidColorBrush( context, &color, NULL, &solid ))) return hr;
        else
        {
            ID2D1DeviceContext_FillRectangle( context, bounds, (ID2D1Brush *)solid );
            ID2D1SolidColorBrush_Release( solid );
        }
    }
    else if ((const void *)brush->lpVtbl == &gradient_brush_vtbl)
        return draw_gradient_brush( context, ((struct gradient_iface *)brush)->owner, bounds, opacity );
    else if (brush->lpVtbl == &surface_composition_brush_vtbl)
        draw_surface_brush( context, surface_brush_from_ICompositionBrush( brush ), bounds, opacity );
    else if (brush->lpVtbl == &nine_grid_composition_brush_vtbl)
        draw_nine_grid_brush( context, nine_grid_brush_from_ICompositionBrush( brush ), bounds, opacity );
    else if (brush->lpVtbl == &effect_composition_brush_vtbl)
        return draw_effect_brush( context, effect_from_ICompositionBrush( brush ), bounds, opacity, depth );
    else return E_NOTIMPL;
    return S_OK;
}

static HRESULT draw_composition_visual( ID2D1DeviceContext *context, IVisual *visual,
        FLOAT x, FLOAT y, FLOAT parent_width, FLOAT parent_height, FLOAT opacity, UINT depth )
{
    struct sprite_visual *impl;
    struct visual_collection *children;
    IInsetClip *clip = NULL;
    D2D1_RECT_F bounds, clip_bounds;
    D2D1_MATRIX_3X2_F parent, local, transform;
    FLOAT center_x, center_y;
    FLOAT width, height;
    UINT i;
    HRESULT hr = S_OK;

    if (!visual) return S_OK;
    if (depth > 64) return E_INVALIDARG;
    impl = sprite_visual_from_IVisual( visual );
    if (!impl->visible) return S_OK;
    if (impl->scale_expression)
        expression_animation_get_vector( impl->scale_expression, &impl->scale );
    if (impl->offset_expression)
        expression_animation_get_vector( impl->offset_expression, &impl->offset );
    if (impl->center_expression)
        expression_animation_get_vector( impl->center_expression, &impl->center_point );
    children = impl_from_IVisualCollection( impl->children );
    width = impl->size.X + parent_width * impl->relative_size_adjustment.X;
    height = impl->size.Y + parent_height * impl->relative_size_adjustment.Y;
    x += impl->offset.X + parent_width * impl->relative_offset_adjustment.X -
            impl->anchor_point.X * width;
    y += impl->offset.Y + parent_height * impl->relative_offset_adjustment.Y -
            impl->anchor_point.Y * height;
    opacity *= impl->opacity;
    ID2D1DeviceContext_GetTransform( context, &parent );
    center_x = impl->center_point.X * (1.0f - impl->scale.X);
    center_y = impl->center_point.Y * (1.0f - impl->scale.Y);
    local = (D2D1_MATRIX_3X2_F){impl->scale.X * impl->transform_matrix.M11,
            impl->scale.X * impl->transform_matrix.M12,
            impl->scale.Y * impl->transform_matrix.M21,
            impl->scale.Y * impl->transform_matrix.M22,
            center_x * impl->transform_matrix.M11 + center_y * impl->transform_matrix.M21 +
                    impl->transform_matrix.M41 + x,
            center_x * impl->transform_matrix.M12 + center_y * impl->transform_matrix.M22 +
                    impl->transform_matrix.M42 + y};
    transform = (D2D1_MATRIX_3X2_F){local._11 * parent._11 + local._12 * parent._21,
            local._11 * parent._12 + local._12 * parent._22,
            local._21 * parent._11 + local._22 * parent._21,
            local._21 * parent._12 + local._22 * parent._22,
            local._31 * parent._11 + local._32 * parent._21 + parent._31,
            local._31 * parent._12 + local._32 * parent._22 + parent._32};
    ID2D1DeviceContext_SetTransform( context, &transform );
    bounds = (D2D1_RECT_F){0.0f, 0.0f, width, height};

    if (impl->clip && SUCCEEDED(ICompositionClip_QueryInterface( impl->clip,
            &IID_IInsetClip, (void **)&clip )))
    {
        struct inset_clip *inset = impl_from_IInsetClip( clip );
        clip_bounds = (D2D1_RECT_F){bounds.left + inset->left, bounds.top + inset->top,
                bounds.right - inset->right, bounds.bottom - inset->bottom};
        ID2D1DeviceContext_PushAxisAlignedClip( context, &clip_bounds, D2D1_ANTIALIAS_MODE_ALIASED );
    }
    if (impl->is_redirect)
    {
        IVisual *source = visual_get_source(impl);
        if (source)
        {
            hr = draw_composition_visual(context, source, 0.0f, 0.0f, width, height, opacity, depth + 1);
            IVisual_Release(source);
        }
    }
    if (SUCCEEDED(hr) && impl->brush && bounds.right > bounds.left && bounds.bottom > bounds.top)
        hr = draw_composition_brush( context, impl->brush, &bounds, opacity, 0 );
    for (i = 0; SUCCEEDED(hr) && i < children->count; ++i)
        hr = draw_composition_visual( context, children->items[i], 0.0f, 0.0f, width, height,
                opacity, depth + 1 );
    if (clip)
    {
        ID2D1DeviceContext_PopAxisAlignedClip( context );
        IInsetClip_Release( clip );
    }
    ID2D1DeviceContext_SetTransform( context, &parent );
    return hr;
}

static struct composition_drawing_surface *find_brush_drawing_surface( ICompositionBrush *brush, UINT depth )
{
    struct composition_drawing_surface *surface;
    struct composition_effect *effect;
    struct effect_source_binding *binding;
    ICompositionBrush *source;
    UINT i;

    if (!brush || depth > 64) return NULL;
    if (brush->lpVtbl == &surface_composition_brush_vtbl)
    {
        surface = drawing_surface_from_surface_brush( surface_brush_from_ICompositionBrush( brush ) );
        if (surface && (surface->committed_bitmap || !list_empty( &surface->tiles )))
        {
            ICompositionDrawingSurface_AddRef( &surface->ICompositionDrawingSurface_iface );
            return surface;
        }
    }
    else if (brush->lpVtbl == &nine_grid_composition_brush_vtbl)
        return find_brush_drawing_surface( nine_grid_brush_from_ICompositionBrush( brush )->source, depth + 1 );
    else if (brush->lpVtbl == &effect_composition_brush_vtbl)
    {
        effect = effect_from_ICompositionBrush( brush );
        for (i = 0, binding = effect->bindings; binding; binding = binding->next, ++i)
        {
            if (!(source = composition_effect_get_binding( effect, i ))) continue;
            surface = find_brush_drawing_surface( source, depth + 1 );
            ICompositionBrush_Release( source );
            if (surface) return surface;
        }
    }
    return NULL;
}

static struct composition_drawing_surface *find_visual_drawing_surface( IVisual *visual, UINT depth )
{
    struct sprite_visual *impl;
    struct visual_collection *children;
    struct composition_drawing_surface *surface;
    UINT i;

    if (!visual || depth > 64) return NULL;
    impl = sprite_visual_from_IVisual( visual );
    if (impl->is_redirect)
    {
        IVisual *source = visual_get_source(impl);
        surface = find_visual_drawing_surface(source, depth + 1);
        if (source) IVisual_Release(source);
        if (surface) return surface;
    }
    if ((surface = find_brush_drawing_surface( impl->brush, 0 ))) return surface;
    children = impl_from_IVisualCollection( impl->children );
    for (i = 0; i < children->count; ++i)
        if ((surface = find_visual_drawing_surface( children->items[i], depth + 1 ))) return surface;
    return NULL;
}

static HRESULT desktop_window_target_render( struct desktop_window_target *target )
{
    struct composition_drawing_surface *source;
    ID2D1Bitmap1 *source_bitmap;
    D2D1_BITMAP_PROPERTIES1 properties = {0};
    D2D1_MATRIX_3X2_F identity = {1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f};
    DXGI_SWAP_CHAIN_DESC1 desc = {0};
    D2D1_COLOR_F white = {1.0f, 1.0f, 1.0f, 1.0f};
    IDXGISurface *surface = NULL, *backbuffer = NULL;
    ID3D11Device *d3d_device = NULL;
    IDXGIDevice *dxgi_device = NULL;
    IDXGIAdapter *adapter = NULL;
    IDXGIFactory2 *factory = NULL;
    RECT client;
    UINT width, height;
    const char *stage = "initialization";
    HRESULT hr, draw;

    if (!target->root || !IsWindow( target->hwnd )) return S_FALSE;
    source = find_visual_drawing_surface( target->root, 0 );
    GetClientRect( target->hwnd, &client );
    width = max( 1, client.right - client.left );
    height = max( 1, client.bottom - client.top );
    if ((target->width && (target->width != width || target->height != height)) ||
            (source && target->render_device && target->render_device != source->device))
        desktop_window_target_reset_renderer( target );

    if (!target->swapchain)
    {
        if (source)
        {
            source_bitmap = source->virtual ? source->bitmap : source->committed_bitmap;
            if (!source_bitmap && !list_empty( &source->tiles ))
                source_bitmap = LIST_ENTRY( list_head( &source->tiles ), struct composition_surface_tile, entry )->bitmap;
            if (!source_bitmap) {hr = S_FALSE; goto done;}
            stage = "get source surface";
            if (FAILED(hr = ID2D1Bitmap1_GetSurface( source_bitmap, &surface ))) goto done;
            stage = "get d3d device";
            if (FAILED(hr = IDXGISurface_GetDevice( surface, &IID_ID3D11Device,
                    (void **)&d3d_device ))) goto done;
            stage = "get dxgi device";
            if (FAILED(hr = IDXGISurface_GetDevice( surface, &IID_IDXGIDevice,
                    (void **)&dxgi_device ))) goto done;
        }
        else
        {
            stage = "create d3d device";
            if (FAILED(hr = D3D11CreateDevice( NULL, D3D_DRIVER_TYPE_HARDWARE, NULL,
                    D3D11_CREATE_DEVICE_BGRA_SUPPORT, NULL, 0, D3D11_SDK_VERSION,
                    &d3d_device, NULL, NULL ))) goto done;
            if (FAILED(hr = ID3D11Device_QueryInterface( d3d_device, &IID_IDXGIDevice,
                    (void **)&dxgi_device ))) goto done;
            stage = "create d2d device";
            if (FAILED(hr = D2D1CreateDevice( dxgi_device, NULL, &target->render_device ))) goto done;
        }
        stage = "get adapter";
        if (FAILED(hr = IDXGIDevice_GetAdapter( dxgi_device, &adapter ))) goto done;
        stage = "get factory";
        if (FAILED(hr = IDXGIAdapter_GetParent( adapter, &IID_IDXGIFactory2,
                (void **)&factory ))) goto done;
        desc.Width = width;
        desc.Height = height;
        desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
        desc.SampleDesc.Count = 1;
        desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        desc.BufferCount = 1;
        desc.Scaling = DXGI_SCALING_STRETCH;
        desc.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
        desc.AlphaMode = DXGI_ALPHA_MODE_IGNORE;
        stage = "create swapchain";
        if (FAILED(hr = IDXGIFactory2_CreateSwapChainForHwnd( factory, (IUnknown *)d3d_device,
                target->hwnd, &desc, NULL, NULL, &target->swapchain ))) goto done;
        target->width = width;
        target->height = height;
    }
    if (!target->render_context)
    {
        if (!target->render_device)
        {
            target->render_device = source->device;
            ID2D1Device_AddRef( target->render_device );
        }
        stage = "create render context";
        if (FAILED(hr = ID2D1Device_CreateDeviceContext( target->render_device,
                D2D1_DEVICE_CONTEXT_OPTIONS_NONE, &target->render_context ))) goto done;
    }
    if (!target->render_bitmap)
    {
        stage = "get backbuffer";
        if (FAILED(hr = IDXGISwapChain1_GetBuffer( target->swapchain, 0,
                &IID_IDXGISurface, (void **)&backbuffer ))) goto done;
        properties.pixelFormat.format = DXGI_FORMAT_B8G8R8A8_UNORM;
        properties.pixelFormat.alphaMode = D2D1_ALPHA_MODE_IGNORE;
        properties.bitmapOptions = D2D1_BITMAP_OPTIONS_TARGET | D2D1_BITMAP_OPTIONS_CANNOT_DRAW;
        properties.dpiX = properties.dpiY = 96.0f;
        stage = "create render bitmap";
        if (FAILED(hr = ID2D1DeviceContext_CreateBitmapFromDxgiSurface( target->render_context,
                backbuffer, &properties, &target->render_bitmap ))) goto done;
    }

    ID2D1DeviceContext_SetTarget( target->render_context, (ID2D1Image *)target->render_bitmap );
    ID2D1DeviceContext_SetTransform( target->render_context, &identity );
    ID2D1DeviceContext_BeginDraw( target->render_context );
    ID2D1DeviceContext_Clear( target->render_context, &white );
    draw = draw_composition_visual( target->render_context, target->root, 0.0f, 0.0f,
            width, height, 1.0f, 0 );
    stage = "end draw";
    hr = ID2D1DeviceContext_EndDraw( target->render_context, NULL, NULL );
    if (SUCCEEDED(hr)) hr = draw;
    if (FAILED(hr)) goto done;
    stage = "present";
    hr = IDXGISwapChain1_Present( target->swapchain, 0, 0 );

done:
    if (source) ICompositionDrawingSurface_Release( &source->ICompositionDrawingSurface_iface );
    if (factory) IDXGIFactory2_Release( factory );
    if (adapter) IDXGIAdapter_Release( adapter );
    if (dxgi_device) IDXGIDevice_Release( dxgi_device );
    if (d3d_device) ID3D11Device_Release( d3d_device );
    if (backbuffer) IDXGISurface_Release( backbuffer );
    if (surface) IDXGISurface_Release( surface );
    if (FAILED(hr))
    {
        ERR( "render failed at %s, hr %#lx.\n", stage, hr );
        desktop_window_target_reset_renderer( target );
    }
    return hr;
}

static HRESULT drawing_surface_update_tiles( struct composition_drawing_surface *surface )
{
    struct composition_surface_tile *tile, *found;
    D2D1_BITMAP_PROPERTIES1 properties = {0};
    D2D1_COLOR_F clear = {0.0f, 0.0f, 0.0f, 0.0f};
    D2D1_SIZE_U update_size, tile_size;
    D2D1_POINT_2U destination;
    D2D1_RECT_U source_rect;
    RECT update, bounds, overlap;
    LONG x, y;
    HRESULT hr = S_OK;
    HRGN region;
    int type;

    update_size = ID2D1Bitmap1_GetPixelSize( surface->bitmap );
    update = (RECT){surface->bitmap_offset.x, surface->bitmap_offset.y,
            surface->bitmap_offset.x + update_size.width, surface->bitmap_offset.y + update_size.height};
    properties.pixelFormat.format = (DXGI_FORMAT)surface->format;
    properties.pixelFormat.alphaMode = (D2D1_ALPHA_MODE)surface->mode;
    properties.dpiX = properties.dpiY = 96.0f;
    properties.bitmapOptions = D2D1_BITMAP_OPTIONS_TARGET;

    for (y = update.top / 256 * 256; y < update.bottom; y += 256)
    {
        for (x = update.left / 256 * 256; x < update.right; x += 256)
        {
            bounds = (RECT){x, y, x + 256, y + 256};
            bounds.right = min( bounds.right, (LONG)surface->size.Width );
            bounds.bottom = min( bounds.bottom, (LONG)surface->size.Height );
            if (!IntersectRect( &overlap, &update, &bounds )) continue;
            found = NULL;
            LIST_FOR_EACH_ENTRY( tile, &surface->tiles, struct composition_surface_tile, entry )
            {
                if (tile->origin.x != x || tile->origin.y != y) continue;
                found = tile;
                break;
            }
            if (!found)
            {
                if (!(found = calloc( 1, sizeof(*found) ))) { hr = E_OUTOFMEMORY; goto done; }
                tile_size = (D2D1_SIZE_U){256, 256};
                hr = ID2D1DeviceContext_CreateBitmap( surface->context, tile_size, NULL, 0,
                        &properties, &found->bitmap );
                if (FAILED(hr)) { free( found ); goto done; }
                found->origin = (POINT){x, y};
                ID2D1DeviceContext_SetTarget( surface->context, (ID2D1Image *)found->bitmap );
                ID2D1DeviceContext_BeginDraw( surface->context );
                ID2D1DeviceContext_Clear( surface->context, &clear );
                hr = ID2D1DeviceContext_EndDraw( surface->context, NULL, NULL );
                if (FAILED(hr))
                {
                    ID2D1Bitmap1_Release( found->bitmap );
                    free( found );
                    goto done;
                }
                if (!(found->valid = CreateRectRgn( 0, 0, 0, 0 )))
                {
                    ID2D1Bitmap1_Release( found->bitmap );
                    free( found );
                    hr = E_OUTOFMEMORY;
                    goto done;
                }
                list_add_tail( &surface->tiles, &found->entry );
                TRACE( "Allocated virtual tile {%ld,%ld}.\n", x, y );
            }
            destination = (D2D1_POINT_2U){overlap.left - x, overlap.top - y};
            source_rect = (D2D1_RECT_U){overlap.left - update.left, overlap.top - update.top,
                    overlap.right - update.left, overlap.bottom - update.top};
            hr = ID2D1Bitmap1_CopyFromBitmap( found->bitmap, &destination,
                    (ID2D1Bitmap *)surface->bitmap, &source_rect );
            if (FAILED(hr)) goto done;
            if (!(region = CreateRectRgn( destination.x, destination.y,
                    destination.x + overlap.right - overlap.left, destination.y + overlap.bottom - overlap.top )))
            {
                hr = E_OUTOFMEMORY;
                goto done;
            }
            type = CombineRgn( found->valid, found->valid, region, RGN_OR );
            DeleteObject( region );
            if (type == ERROR) { hr = E_OUTOFMEMORY; goto done; }
        }
    }
done:
    ID2D1DeviceContext_SetTarget( surface->context, (ID2D1Image *)surface->bitmap );
    return hr;
}

static HRESULT drawing_surface_publish_bitmap( struct composition_drawing_surface *surface )
{
    D2D1_BITMAP_PROPERTIES1 properties = {0};
    D2D1_COLOR_F clear = {0.0f, 0.0f, 0.0f, 0.0f};
    ID2D1Bitmap1 *bitmap = surface->committed_bitmap;
    D2D1_POINT_2U destination = {surface->update_rect.left, surface->update_rect.top};
    D2D1_RECT_U rect = {surface->update_rect.left, surface->update_rect.top,
            surface->update_rect.right, surface->update_rect.bottom};
    HRESULT hr;

    if (!bitmap)
    {
        properties.pixelFormat.format = (DXGI_FORMAT)surface->format;
        properties.pixelFormat.alphaMode = (D2D1_ALPHA_MODE)surface->mode;
        properties.dpiX = properties.dpiY = 96.0f;
        properties.bitmapOptions = D2D1_BITMAP_OPTIONS_TARGET;
        if (FAILED(hr = ID2D1DeviceContext_CreateBitmap( surface->context,
                ID2D1Bitmap1_GetPixelSize( surface->bitmap ), NULL, 0, &properties, &bitmap ))) return hr;
        ID2D1DeviceContext_SetTarget( surface->context, (ID2D1Image *)bitmap );
        ID2D1DeviceContext_BeginDraw( surface->context );
        ID2D1DeviceContext_Clear( surface->context, &clear );
        hr = ID2D1DeviceContext_EndDraw( surface->context, NULL, NULL );
        ID2D1DeviceContext_SetTarget( surface->context, (ID2D1Image *)surface->bitmap );
        if (FAILED(hr)) { ID2D1Bitmap1_Release( bitmap ); return hr; }
    }
    hr = ID2D1Bitmap1_CopyFromBitmap( bitmap, &destination, (ID2D1Bitmap *)surface->bitmap, &rect );
    if (FAILED(hr))
    {
        if (!surface->committed_bitmap) ID2D1Bitmap1_Release( bitmap );
        return hr;
    }
    surface->committed_bitmap = bitmap;
    return S_OK;
}

static HRESULT WINAPI composition_drawing_surface_interop_EndDraw( ICompositionDrawingSurfaceInterop *iface )
{
    struct composition_drawing_surface *impl = impl_from_ICompositionDrawingSurfaceInterop( iface );
    HRESULT hr;

    FIXME( "iface %p.\n", iface );
    if (impl->drawing)
    {
        impl->drawing = FALSE;
        if (!impl->suspended && FAILED(hr = ID2D1DeviceContext_EndDraw( impl->context, NULL, NULL ))) return hr;
        impl->suspended = FALSE;
    }
    if (!impl->bitmap) return S_OK;
    if (impl->virtual) return drawing_surface_update_tiles( impl );
    return drawing_surface_publish_bitmap( impl );
}

static HRESULT WINAPI composition_drawing_surface_interop_Resize( ICompositionDrawingSurfaceInterop *iface,
        SIZE size )
{
    struct composition_drawing_surface *impl = impl_from_ICompositionDrawingSurfaceInterop( iface );
    struct composition_surface_tile *tile, *next;
    HRGN bounds;
    int type;

    TRACE( "iface %p, size {%ld, %ld}.\n", iface, size.cx, size.cy );
    if (impl->virtual)
    {
        if (size.cx < 0 || size.cy < 0 || size.cx > 0x1000000 || size.cy > 0x1000000) return E_INVALIDARG;
    }
    else if (size.cx <= 0 || size.cy <= 0) return E_INVALIDARG;
    if (impl->drawing) return D2DERR_WRONG_STATE;
    if (impl->virtual)
    {
        if (!(bounds = CreateRectRgn( 0, 0, size.cx, size.cy ))) return E_OUTOFMEMORY;
        LIST_FOR_EACH_ENTRY_SAFE( tile, next, &impl->tiles, struct composition_surface_tile, entry )
        {
            OffsetRgn( bounds, -tile->origin.x, -tile->origin.y );
            type = CombineRgn( tile->valid, tile->valid, bounds, RGN_AND );
            OffsetRgn( bounds, tile->origin.x, tile->origin.y );
            if (type == ERROR) { DeleteObject( bounds ); return E_OUTOFMEMORY; }
            if (type == NULLREGION) drawing_surface_free_tile( tile );
        }
        DeleteObject( bounds );
    }
    else drawing_surface_clear_tiles( impl );
    if (impl->bitmap)
    {
        ID2D1DeviceContext_SetTarget( impl->context, NULL );
        ID2D1Bitmap1_Release( impl->bitmap );
        impl->bitmap = NULL;
    }
    if (impl->committed_bitmap)
    {
        ID2D1Bitmap1_Release( impl->committed_bitmap );
        impl->committed_bitmap = NULL;
    }
    impl->size.Width = size.cx;
    impl->size.Height = size.cy;
    return S_OK;
}

static HRESULT WINAPI composition_drawing_surface_interop_Scroll( ICompositionDrawingSurfaceInterop *iface,
        const RECT *scroll_rect, const RECT *clip_rect, INT offset_x, INT offset_y )
{
    FIXME( "iface %p, scroll_rect %s, clip_rect %s, offset {%d, %d}.\n",
            iface, wine_dbgstr_rect( scroll_rect ), wine_dbgstr_rect( clip_rect ), offset_x, offset_y );
    return E_NOTIMPL;
}

static HRESULT WINAPI composition_drawing_surface_interop_ResumeDraw( ICompositionDrawingSurfaceInterop *iface )
{
    struct composition_drawing_surface *impl = impl_from_ICompositionDrawingSurfaceInterop( iface );

    FIXME( "iface %p.\n", iface );
    if (!impl->drawing || !impl->suspended) return D2DERR_WRONG_STATE;
    ID2D1DeviceContext_SetTarget( impl->context, (ID2D1Image *)impl->bitmap );
    ID2D1DeviceContext_BeginDraw( impl->context );
    impl->suspended = FALSE;
    return S_OK;
}

static HRESULT WINAPI composition_drawing_surface_interop_SuspendDraw( ICompositionDrawingSurfaceInterop *iface )
{
    struct composition_drawing_surface *impl = impl_from_ICompositionDrawingSurfaceInterop( iface );
    HRESULT hr;

    FIXME( "iface %p.\n", iface );
    if (!impl->drawing || impl->suspended) return D2DERR_WRONG_STATE;
    if (FAILED(hr = ID2D1DeviceContext_EndDraw( impl->context, NULL, NULL ))) return hr;
    impl->suspended = TRUE;
    return S_OK;
}

static const ICompositionDrawingSurfaceInteropVtbl composition_drawing_surface_interop_vtbl =
{
    composition_drawing_surface_interop_QueryInterface,
    composition_drawing_surface_interop_AddRef,
    composition_drawing_surface_interop_Release,
    composition_drawing_surface_interop_BeginDraw,
    composition_drawing_surface_interop_EndDraw,
    composition_drawing_surface_interop_Resize,
    composition_drawing_surface_interop_Scroll,
    composition_drawing_surface_interop_ResumeDraw,
    composition_drawing_surface_interop_SuspendDraw,
};

static HRESULT WINAPI composition_graphics_device_QueryInterface( ICompositionGraphicsDevice *iface,
        REFIID iid, void **out )
{
    FIXME( "iface %p, iid %s, out %p.\n", iface, debugstr_guid( iid ), out );
    if (!out) return E_POINTER;
    *out = NULL;
    if (IsEqualGUID( iid, &IID_ICompositionGraphicsDevice2 ))
        *out = &impl_from_ICompositionGraphicsDevice( iface )->ICompositionGraphicsDevice2_iface;
    else
    if (!IsEqualGUID( iid, &IID_IUnknown ) && !IsEqualGUID( iid, &IID_IInspectable ) &&
        !IsEqualGUID( iid, &IID_IAgileObject ) && !IsEqualGUID( iid, &IID_ICompositionGraphicsDevice ))
        return E_NOINTERFACE;
    else
        *out = iface;
    ICompositionGraphicsDevice_AddRef( iface );
    return S_OK;
}

static ULONG WINAPI composition_graphics_device_AddRef( ICompositionGraphicsDevice *iface )
{
    return InterlockedIncrement( &impl_from_ICompositionGraphicsDevice( iface )->ref );
}

static ULONG WINAPI composition_graphics_device_Release( ICompositionGraphicsDevice *iface )
{
    struct composition_graphics_device *impl = impl_from_ICompositionGraphicsDevice( iface );
    ULONG ref = InterlockedDecrement( &impl->ref );

    if (!ref)
    {
        IUnknown_Release( impl->device );
        free( impl );
    }
    return ref;
}

static HRESULT WINAPI composition_graphics_device_GetIids( ICompositionGraphicsDevice *iface,
        ULONG *count, IID **iids )
{
    if (!count || !iids) return E_POINTER;
    if (!(*iids = CoTaskMemAlloc( 2 * sizeof(**iids) ))) return E_OUTOFMEMORY;
    (*iids)[0] = IID_ICompositionGraphicsDevice;
    (*iids)[1] = IID_ICompositionGraphicsDevice2;
    *count = 2;
    return S_OK;
}

static HRESULT WINAPI composition_graphics_device_GetRuntimeClassName( ICompositionGraphicsDevice *iface,
        HSTRING *name )
{
    if (!name) return E_POINTER;
    return WindowsCreateString( RuntimeClass_Windows_UI_Composition_CompositionGraphicsDevice,
            ARRAY_SIZE(RuntimeClass_Windows_UI_Composition_CompositionGraphicsDevice) - 1, name );
}

static HRESULT WINAPI composition_graphics_device_GetTrustLevel( ICompositionGraphicsDevice *iface,
        TrustLevel *level )
{
    if (!level) return E_POINTER;
    *level = BaseTrust;
    return S_OK;
}

static HRESULT WINAPI composition_graphics_device_CreateDrawingSurface( ICompositionGraphicsDevice *iface,
        Size pixels, enum __x_ABI_CWindows_CGraphics_CDirectX_CDirectXPixelFormat format,
        enum __x_ABI_CWindows_CGraphics_CDirectX_CDirectXAlphaMode mode,
        ICompositionDrawingSurface **result )
{
    struct composition_drawing_surface *impl;

    FIXME( "iface %p, pixels {%f, %f}, format %u, mode %u, result %p.\n",
            iface, pixels.Width, pixels.Height, format, mode, result );
    if (!result) return E_POINTER;
    *result = NULL;
    if (!(impl = calloc( 1, sizeof(*impl) ))) return E_OUTOFMEMORY;
    impl->ICompositionDrawingSurface_iface.lpVtbl = &composition_drawing_surface_vtbl;
    impl->ICompositionDrawingSurfaceInterop_iface.lpVtbl = &composition_drawing_surface_interop_vtbl;
    if (FAILED(IUnknown_QueryInterface( impl_from_ICompositionGraphicsDevice( iface )->device,
            &IID_ID2D1Device, (void **)&impl->device )))
    {
        free( impl );
        return E_NOINTERFACE;
    }
    list_init( &impl->tiles );
    impl->size = pixels;
    impl->format = format;
    impl->mode = mode;
    impl->ref = 1;
    *result = &impl->ICompositionDrawingSurface_iface;
    return S_OK;
}

static HRESULT WINAPI composition_graphics_device_add_RenderingDeviceReplaced( ICompositionGraphicsDevice *iface,
        ITypedEventHandler_CompositionGraphicsDevice_RenderingDeviceReplacedEventArgs *handler,
        EventRegistrationToken *token )
{
    FIXME( "iface %p, handler %p, token %p.\n", iface, handler, token );
    return E_NOTIMPL;
}

static HRESULT WINAPI composition_graphics_device_remove_RenderingDeviceReplaced( ICompositionGraphicsDevice *iface,
        EventRegistrationToken token )
{
    FIXME( "iface %p.\n", iface );
    return E_NOTIMPL;
}

static const ICompositionGraphicsDeviceVtbl composition_graphics_device_vtbl =
{
    composition_graphics_device_QueryInterface,
    composition_graphics_device_AddRef,
    composition_graphics_device_Release,
    composition_graphics_device_GetIids,
    composition_graphics_device_GetRuntimeClassName,
    composition_graphics_device_GetTrustLevel,
    composition_graphics_device_CreateDrawingSurface,
    composition_graphics_device_add_RenderingDeviceReplaced,
    composition_graphics_device_remove_RenderingDeviceReplaced,
};

static HRESULT WINAPI composition_graphics_device2_QueryInterface( ICompositionGraphicsDevice2 *iface,
        REFIID iid, void **out )
{
    return composition_graphics_device_QueryInterface(
            &impl_from_ICompositionGraphicsDevice2( iface )->ICompositionGraphicsDevice_iface, iid, out );
}

static ULONG WINAPI composition_graphics_device2_AddRef( ICompositionGraphicsDevice2 *iface )
{
    return composition_graphics_device_AddRef(
            &impl_from_ICompositionGraphicsDevice2( iface )->ICompositionGraphicsDevice_iface );
}

static ULONG WINAPI composition_graphics_device2_Release( ICompositionGraphicsDevice2 *iface )
{
    return composition_graphics_device_Release(
            &impl_from_ICompositionGraphicsDevice2( iface )->ICompositionGraphicsDevice_iface );
}

static HRESULT WINAPI composition_graphics_device2_GetIids( ICompositionGraphicsDevice2 *iface,
        ULONG *count, IID **iids )
{
    return composition_graphics_device_GetIids(
            &impl_from_ICompositionGraphicsDevice2( iface )->ICompositionGraphicsDevice_iface, count, iids );
}

static HRESULT WINAPI composition_graphics_device2_GetRuntimeClassName( ICompositionGraphicsDevice2 *iface,
        HSTRING *name )
{
    return composition_graphics_device_GetRuntimeClassName(
            &impl_from_ICompositionGraphicsDevice2( iface )->ICompositionGraphicsDevice_iface, name );
}

static HRESULT WINAPI composition_graphics_device2_GetTrustLevel( ICompositionGraphicsDevice2 *iface,
        TrustLevel *level )
{
    return composition_graphics_device_GetTrustLevel(
            &impl_from_ICompositionGraphicsDevice2( iface )->ICompositionGraphicsDevice_iface, level );
}

static HRESULT WINAPI composition_graphics_device2_CreateDrawingSurface2( ICompositionGraphicsDevice2 *iface,
        struct SizeInt32 pixels, enum __x_ABI_CWindows_CGraphics_CDirectX_CDirectXPixelFormat format,
        enum __x_ABI_CWindows_CGraphics_CDirectX_CDirectXAlphaMode mode,
        ICompositionDrawingSurface **result )
{
    Size size = {pixels.Width, pixels.Height};

    return composition_graphics_device_CreateDrawingSurface(
            &impl_from_ICompositionGraphicsDevice2( iface )->ICompositionGraphicsDevice_iface,
            size, format, mode, result );
}

static HRESULT WINAPI composition_graphics_device2_CreateVirtualDrawingSurface(
        ICompositionGraphicsDevice2 *iface, struct SizeInt32 pixels,
        enum __x_ABI_CWindows_CGraphics_CDirectX_CDirectXPixelFormat format,
        enum __x_ABI_CWindows_CGraphics_CDirectX_CDirectXAlphaMode mode,
        ICompositionVirtualDrawingSurface **result )
{
    struct composition_graphics_device *device = impl_from_ICompositionGraphicsDevice2( iface );
    struct composition_drawing_surface *impl;

    FIXME( "iface %p, pixels {%d, %d}, format %u, mode %u, result %p.\n",
            iface, pixels.Width, pixels.Height, format, mode, result );
    if (!result) return E_POINTER;
    *result = NULL;
    if (pixels.Width < 0 || pixels.Height < 0 || pixels.Width > 0x1000000 || pixels.Height > 0x1000000)
        return E_INVALIDARG;
    if (!(impl = calloc( 1, sizeof(*impl) ))) return E_OUTOFMEMORY;
    impl->ICompositionDrawingSurface_iface.lpVtbl = &composition_drawing_surface_vtbl;
    impl->ICompositionVirtualDrawingSurface_iface.lpVtbl = &composition_virtual_drawing_surface_vtbl;
    impl->ICompositionDrawingSurfaceInterop_iface.lpVtbl = &composition_drawing_surface_interop_vtbl;
    if (FAILED(IUnknown_QueryInterface( device->device, &IID_ID2D1Device, (void **)&impl->device )))
    {
        free( impl );
        return E_NOINTERFACE;
    }
    list_init( &impl->tiles );
    impl->size.Width = pixels.Width;
    impl->size.Height = pixels.Height;
    impl->format = format;
    impl->mode = mode;
    impl->virtual = TRUE;
    impl->ref = 1;
    *result = &impl->ICompositionVirtualDrawingSurface_iface;
    return S_OK;
}

static const ICompositionGraphicsDevice2Vtbl composition_graphics_device2_vtbl =
{
    composition_graphics_device2_QueryInterface,
    composition_graphics_device2_AddRef,
    composition_graphics_device2_Release,
    composition_graphics_device2_GetIids,
    composition_graphics_device2_GetRuntimeClassName,
    composition_graphics_device2_GetTrustLevel,
    composition_graphics_device2_CreateDrawingSurface2,
    composition_graphics_device2_CreateVirtualDrawingSurface,
};

static HRESULT WINAPI compositor_interop_CreateCompositionSurfaceForHandle( ICompositorInterop *iface,
        HANDLE handle, ICompositionSurface **result )
{
    FIXME( "iface %p, handle %p, result %p.\n", iface, handle, result );
    if (result) *result = NULL;
    return E_NOTIMPL;
}

static HRESULT WINAPI compositor_interop_CreateCompositionSurfaceForSwapChain( ICompositorInterop *iface,
        IUnknown *swapchain, ICompositionSurface **result )
{
    FIXME( "iface %p, swapchain %p, result %p.\n", iface, swapchain, result );
    if (result) *result = NULL;
    return E_NOTIMPL;
}

static HRESULT WINAPI compositor_interop_CreateGraphicsDevice( ICompositorInterop *iface,
        IUnknown *device, ICompositionGraphicsDevice **result )
{
    struct composition_graphics_device *impl;

    FIXME( "iface %p, device %p, result %p.\n", iface, device, result );
    if (!result) return E_POINTER;
    *result = NULL;
    if (!device) return E_INVALIDARG;
    if (!(impl = calloc( 1, sizeof(*impl) ))) return E_OUTOFMEMORY;
    impl->ICompositionGraphicsDevice_iface.lpVtbl = &composition_graphics_device_vtbl;
    impl->ICompositionGraphicsDevice2_iface.lpVtbl = &composition_graphics_device2_vtbl;
    impl->device = device;
    IUnknown_AddRef( device );
    impl->ref = 1;
    *result = &impl->ICompositionGraphicsDevice_iface;
    return S_OK;
}

static const ICompositorInteropVtbl compositor_interop_vtbl =
{
    compositor_interop_QueryInterface,
    compositor_interop_AddRef,
    compositor_interop_Release,
    compositor_interop_CreateCompositionSurfaceForHandle,
    compositor_interop_CreateCompositionSurfaceForSwapChain,
    compositor_interop_CreateGraphicsDevice,
};

static HRESULT compositor_create( IInspectable **result )
{
    struct compositor *impl;
    HRESULT hr;

    if (!result) return E_POINTER;
    *result = NULL;
    if (!(impl = calloc( 1, sizeof(*impl) ))) return E_OUTOFMEMORY;
    impl->ICompositor_iface.lpVtbl = &compositor_vtbl;
    impl->ICompositor2_iface.lpVtbl = &compositor2_vtbl;
    impl->ICompositor4_iface.lpVtbl = &compositor4_vtbl;
    impl->ICompositor5_iface.lpVtbl = &compositor5_vtbl;
    impl->ICompositor6_iface.lpVtbl = &compositor6_vtbl;
    impl->ICompositorDesktopInterop_iface.lpVtbl = &compositor_desktop_vtbl;
    impl->ICompositorInterop_iface.lpVtbl = &compositor_interop_vtbl;
    impl->IClosable_iface.lpVtbl = &compositor_closable_vtbl;
    list_init( &impl->targets );
    impl->global_playback_rate = 1.0f;
    impl->ref = 1;
    if (FAILED(hr = DCompositionCreateDevice( NULL, &IID_IDCompositionDevice,
            (void **)&impl->dcomp_device )))
    {
        free( impl );
        return hr;
    }
    *result = (IInspectable *)&impl->ICompositor_iface;
    return S_OK;
}

struct compositor_controller
{
    ICompositorController ICompositorController_iface;
    IClosable IClosable_iface;
    ICompositor *compositor;
    struct list commit_handlers;
    LONG ref;
};

struct compositor_commit_handler
{
    struct list entry;
    ITypedEventHandler_CompositorController_IInspectable *handler;
    EventRegistrationToken token;
};

static inline struct compositor_controller *impl_from_ICompositorController( ICompositorController *iface )
{
    return CONTAINING_RECORD( iface, struct compositor_controller, ICompositorController_iface );
}

static inline struct compositor_controller *controller_from_IClosable( IClosable *iface )
{
    return CONTAINING_RECORD( iface, struct compositor_controller, IClosable_iface );
}

static HRESULT WINAPI compositor_controller_QueryInterface( ICompositorController *iface,
        REFIID iid, void **out )
{
    struct compositor_controller *impl = impl_from_ICompositorController( iface );

    FIXME( "iface %p, iid %s, out %p.\n", iface, debugstr_guid( iid ), out );
    if (!out) return E_POINTER;
    if (IsEqualGUID( iid, &IID_IUnknown ) || IsEqualGUID( iid, &IID_IInspectable ) ||
        IsEqualGUID( iid, &IID_IAgileObject ) || IsEqualGUID( iid, &IID_ICompositorController ))
        *out = &impl->ICompositorController_iface;
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

static ULONG WINAPI compositor_controller_AddRef( ICompositorController *iface )
{
    return InterlockedIncrement( &impl_from_ICompositorController( iface )->ref );
}

static ULONG WINAPI compositor_controller_Release( ICompositorController *iface )
{
    struct compositor_controller *impl = impl_from_ICompositorController( iface );
    ULONG ref = InterlockedDecrement( &impl->ref );

    if (!ref)
    {
        struct compositor_commit_handler *handler, *next;

        impl_from_ICompositor( impl->compositor )->controller = NULL;
        LIST_FOR_EACH_ENTRY_SAFE( handler, next, &impl->commit_handlers,
                struct compositor_commit_handler, entry )
        {
            list_remove( &handler->entry );
            ITypedEventHandler_CompositorController_IInspectable_Release( handler->handler );
            free( handler );
        }
        ICompositor_Release( impl->compositor );
        free( impl );
    }
    return ref;
}

static HRESULT WINAPI compositor_controller_GetIids( ICompositorController *iface,
        ULONG *count, IID **iids )
{
    IID *values;

    if (!count || !iids) return E_POINTER;
    if (!(values = CoTaskMemAlloc( 2 * sizeof(*values) ))) return E_OUTOFMEMORY;
    values[0] = IID_ICompositorController;
    values[1] = IID_IClosable;
    *count = 2;
    *iids = values;
    return S_OK;
}

static HRESULT WINAPI compositor_controller_GetRuntimeClassName( ICompositorController *iface,
        HSTRING *name )
{
    if (!name) return E_POINTER;
    return WindowsCreateString( RuntimeClass_Windows_UI_Composition_Core_CompositorController,
                                ARRAY_SIZE(RuntimeClass_Windows_UI_Composition_Core_CompositorController) - 1,
                                name );
}

static HRESULT WINAPI compositor_controller_GetTrustLevel( ICompositorController *iface,
        TrustLevel *level )
{
    if (!level) return E_POINTER;
    *level = BaseTrust;
    return S_OK;
}

static HRESULT WINAPI compositor_controller_get_Compositor( ICompositorController *iface,
        ICompositor **value )
{
    struct compositor_controller *impl = impl_from_ICompositorController( iface );

    FIXME( "iface %p, value %p.\n", iface, value );
    if (!value) return E_POINTER;
    ICompositor_AddRef( impl->compositor );
    *value = impl->compositor;
    return S_OK;
}

static HRESULT WINAPI compositor_controller_Commit( ICompositorController *iface )
{
    struct compositor *compositor = impl_from_ICompositor(
            impl_from_ICompositorController( iface )->compositor );
    struct desktop_window_target *target;
    BOOL rendered = FALSE;
    HRESULT hr;

    FIXME( "iface %p.\n", iface );
    LIST_FOR_EACH_ENTRY( target, &compositor->targets, struct desktop_window_target, entry )
    {
        hr = desktop_window_target_render( target );
        if (hr == S_OK) rendered = TRUE;
        else if (hr != S_FALSE) return hr;
    }
    if (rendered) return S_OK;
    if (compositor->dcomp_device) return IDCompositionDevice_Commit( compositor->dcomp_device );
    return S_OK;
}

struct completed_action
{
    IAsyncAction IAsyncAction_iface;
    IAsyncInfo IAsyncInfo_iface;
    IAsyncActionCompletedHandler *handler;
    CRITICAL_SECTION cs;
    BOOL closed;
    LONG ref;
};

static inline struct completed_action *completed_action_from_IAsyncAction( IAsyncAction *iface )
{
    return CONTAINING_RECORD( iface, struct completed_action, IAsyncAction_iface );
}

static inline struct completed_action *completed_action_from_IAsyncInfo( IAsyncInfo *iface )
{
    return CONTAINING_RECORD( iface, struct completed_action, IAsyncInfo_iface );
}

static HRESULT completed_action_query_interface( struct completed_action *impl, REFIID iid, void **out )
{
    if (!out) return E_POINTER;
    *out = NULL;
    if (IsEqualGUID( iid, &IID_IUnknown ) || IsEqualGUID( iid, &IID_IInspectable ) ||
        IsEqualGUID( iid, &IID_IAgileObject ) || IsEqualGUID( iid, &IID_IAsyncAction ))
        *out = &impl->IAsyncAction_iface;
    else if (IsEqualGUID( iid, &IID_IAsyncInfo ))
        *out = &impl->IAsyncInfo_iface;
    else
        return E_NOINTERFACE;
    InterlockedIncrement( &impl->ref );
    return S_OK;
}

static ULONG completed_action_add_ref( struct completed_action *impl )
{
    return InterlockedIncrement( &impl->ref );
}

static ULONG completed_action_release( struct completed_action *impl )
{
    ULONG ref = InterlockedDecrement( &impl->ref );

    if (!ref)
    {
        if (impl->handler) IAsyncActionCompletedHandler_Release( impl->handler );
        DeleteCriticalSection( &impl->cs );
        free( impl );
    }
    return ref;
}

static HRESULT completed_action_get_iids( ULONG *count, IID **iids )
{
    if (!count || !iids) return E_POINTER;
    if (!(*iids = CoTaskMemAlloc( 2 * sizeof(**iids) ))) return E_OUTOFMEMORY;
    (*iids)[0] = IID_IAsyncAction;
    (*iids)[1] = IID_IAsyncInfo;
    *count = 2;
    return S_OK;
}

static HRESULT completed_action_get_class_name( HSTRING *name )
{
    static const WCHAR class_name[] = L"Windows.Foundation.IAsyncAction";

    if (!name) return E_POINTER;
    return WindowsCreateString( class_name, ARRAY_SIZE(class_name) - 1, name );
}

static HRESULT completed_action_get_trust_level( TrustLevel *level )
{
    if (!level) return E_POINTER;
    *level = BaseTrust;
    return S_OK;
}

static HRESULT WINAPI completed_action_QueryInterface( IAsyncAction *iface, REFIID iid, void **out )
{
    return completed_action_query_interface( completed_action_from_IAsyncAction( iface ), iid, out );
}

static ULONG WINAPI completed_action_AddRef( IAsyncAction *iface )
{
    return completed_action_add_ref( completed_action_from_IAsyncAction( iface ) );
}

static ULONG WINAPI completed_action_Release( IAsyncAction *iface )
{
    return completed_action_release( completed_action_from_IAsyncAction( iface ) );
}

static HRESULT WINAPI completed_action_GetIids( IAsyncAction *iface, ULONG *count, IID **iids )
{
    return completed_action_get_iids( count, iids );
}

static HRESULT WINAPI completed_action_GetRuntimeClassName( IAsyncAction *iface, HSTRING *name )
{
    return completed_action_get_class_name( name );
}

static HRESULT WINAPI completed_action_GetTrustLevel( IAsyncAction *iface, TrustLevel *level )
{
    return completed_action_get_trust_level( level );
}

static HRESULT WINAPI completed_action_put_Completed( IAsyncAction *iface,
        IAsyncActionCompletedHandler *handler )
{
    struct completed_action *impl = completed_action_from_IAsyncAction( iface );
    HRESULT hr = S_OK;

    if (!handler) return E_POINTER;
    EnterCriticalSection( &impl->cs );
    if (impl->closed) hr = E_ILLEGAL_METHOD_CALL;
    else if (impl->handler) hr = E_ILLEGAL_DELEGATE_ASSIGNMENT;
    else
    {
        IAsyncActionCompletedHandler_AddRef( handler );
        impl->handler = handler;
    }
    LeaveCriticalSection( &impl->cs );
    if (FAILED(hr)) return hr;

    IAsyncAction_AddRef( iface );
    hr = IAsyncActionCompletedHandler_Invoke( handler, iface, Completed );
    IAsyncAction_Release( iface );
    return hr;
}

static HRESULT WINAPI completed_action_get_Completed( IAsyncAction *iface,
        IAsyncActionCompletedHandler **handler )
{
    struct completed_action *impl = completed_action_from_IAsyncAction( iface );
    HRESULT hr = S_OK;

    if (!handler) return E_POINTER;
    *handler = NULL;
    EnterCriticalSection( &impl->cs );
    if (impl->closed) hr = E_ILLEGAL_METHOD_CALL;
    else if ((*handler = impl->handler)) IAsyncActionCompletedHandler_AddRef( *handler );
    LeaveCriticalSection( &impl->cs );
    return hr;
}

static HRESULT WINAPI completed_action_GetResults( IAsyncAction *iface )
{
    struct completed_action *impl = completed_action_from_IAsyncAction( iface );
    HRESULT hr = S_OK;

    EnterCriticalSection( &impl->cs );
    if (impl->closed) hr = E_ILLEGAL_METHOD_CALL;
    LeaveCriticalSection( &impl->cs );
    return hr;
}

static const IAsyncActionVtbl completed_action_vtbl =
{
    completed_action_QueryInterface,
    completed_action_AddRef,
    completed_action_Release,
    completed_action_GetIids,
    completed_action_GetRuntimeClassName,
    completed_action_GetTrustLevel,
    completed_action_put_Completed,
    completed_action_get_Completed,
    completed_action_GetResults,
};

static HRESULT WINAPI completed_info_QueryInterface( IAsyncInfo *iface, REFIID iid, void **out )
{
    return completed_action_query_interface( completed_action_from_IAsyncInfo( iface ), iid, out );
}

static ULONG WINAPI completed_info_AddRef( IAsyncInfo *iface )
{
    return completed_action_add_ref( completed_action_from_IAsyncInfo( iface ) );
}

static ULONG WINAPI completed_info_Release( IAsyncInfo *iface )
{
    return completed_action_release( completed_action_from_IAsyncInfo( iface ) );
}

static HRESULT WINAPI completed_info_GetIids( IAsyncInfo *iface, ULONG *count, IID **iids )
{
    return completed_action_get_iids( count, iids );
}

static HRESULT WINAPI completed_info_GetRuntimeClassName( IAsyncInfo *iface, HSTRING *name )
{
    return completed_action_get_class_name( name );
}

static HRESULT WINAPI completed_info_GetTrustLevel( IAsyncInfo *iface, TrustLevel *level )
{
    return completed_action_get_trust_level( level );
}

static HRESULT WINAPI completed_info_get_Id( IAsyncInfo *iface, UINT32 *id )
{
    if (!id) return E_POINTER;
    *id = 1;
    return S_OK;
}

static HRESULT WINAPI completed_info_get_Status( IAsyncInfo *iface, AsyncStatus *status )
{
    struct completed_action *impl = completed_action_from_IAsyncInfo( iface );
    HRESULT hr = S_OK;

    if (!status) return E_POINTER;
    *status = Started;
    EnterCriticalSection( &impl->cs );
    if (impl->closed) hr = E_ILLEGAL_METHOD_CALL;
    else *status = Completed;
    LeaveCriticalSection( &impl->cs );
    return hr;
}

static HRESULT WINAPI completed_info_get_ErrorCode( IAsyncInfo *iface, HRESULT *error )
{
    struct completed_action *impl = completed_action_from_IAsyncInfo( iface );
    HRESULT hr = S_OK;

    if (!error) return E_POINTER;
    *error = S_OK;
    EnterCriticalSection( &impl->cs );
    if (impl->closed) hr = E_ILLEGAL_METHOD_CALL;
    LeaveCriticalSection( &impl->cs );
    return hr;
}

static HRESULT WINAPI completed_info_Cancel( IAsyncInfo *iface )
{
    struct completed_action *impl = completed_action_from_IAsyncInfo( iface );
    HRESULT hr = S_OK;

    EnterCriticalSection( &impl->cs );
    if (impl->closed) hr = E_ILLEGAL_METHOD_CALL;
    LeaveCriticalSection( &impl->cs );
    return hr;
}

static HRESULT WINAPI completed_info_Close( IAsyncInfo *iface )
{
    struct completed_action *impl = completed_action_from_IAsyncInfo( iface );
    IAsyncActionCompletedHandler *handler;

    EnterCriticalSection( &impl->cs );
    if (impl->closed)
    {
        LeaveCriticalSection( &impl->cs );
        return S_OK;
    }
    impl->closed = TRUE;
    handler = impl->handler;
    impl->handler = NULL;
    LeaveCriticalSection( &impl->cs );
    if (handler) IAsyncActionCompletedHandler_Release( handler );
    return S_OK;
}

static const IAsyncInfoVtbl completed_info_vtbl =
{
    completed_info_QueryInterface,
    completed_info_AddRef,
    completed_info_Release,
    completed_info_GetIids,
    completed_info_GetRuntimeClassName,
    completed_info_GetTrustLevel,
    completed_info_get_Id,
    completed_info_get_Status,
    completed_info_get_ErrorCode,
    completed_info_Cancel,
    completed_info_Close,
};

static HRESULT completed_action_create( IAsyncAction **operation )
{
    struct completed_action *impl;

    if (!operation) return E_POINTER;
    *operation = NULL;
    if (!(impl = calloc( 1, sizeof(*impl) ))) return E_OUTOFMEMORY;
    impl->IAsyncAction_iface.lpVtbl = &completed_action_vtbl;
    impl->IAsyncInfo_iface.lpVtbl = &completed_info_vtbl;
    impl->ref = 1;
    InitializeCriticalSection( &impl->cs );
    *operation = &impl->IAsyncAction_iface;
    return S_OK;
}

static HRESULT WINAPI compositor_controller_EnsurePreviousCommitCompletedAsync(
        ICompositorController *iface, IAsyncAction **operation )
{
    TRACE( "iface %p, operation %p.\n", iface, operation );
    return completed_action_create( operation );
}

static HRESULT WINAPI compositor_controller_add_CommitNeeded( ICompositorController *iface,
        ITypedEventHandler_CompositorController_IInspectable *handler, EventRegistrationToken *token )
{
    struct compositor_controller *impl = impl_from_ICompositorController( iface );
    struct compositor_commit_handler *entry;
    static LONG next_token;

    TRACE( "iface %p, handler %p, token %p.\n", iface, handler, token );
    if (!handler || !token) return E_INVALIDARG;
    if (!(entry = calloc( 1, sizeof(*entry) ))) return E_OUTOFMEMORY;
    entry->handler = handler;
    entry->token.value = InterlockedIncrement( &next_token );
    ITypedEventHandler_CompositorController_IInspectable_AddRef( handler );
    list_add_tail( &impl->commit_handlers, &entry->entry );
    *token = entry->token;
    return S_OK;
}

static HRESULT WINAPI compositor_controller_remove_CommitNeeded( ICompositorController *iface,
        EventRegistrationToken token )
{
    struct compositor_controller *impl = impl_from_ICompositorController( iface );
    struct compositor_commit_handler *handler;

    TRACE( "iface %p, token %s.\n", iface, wine_dbgstr_longlong( token.value ) );
    LIST_FOR_EACH_ENTRY( handler, &impl->commit_handlers, struct compositor_commit_handler, entry )
    {
        if (handler->token.value != token.value) continue;
        list_remove( &handler->entry );
        ITypedEventHandler_CompositorController_IInspectable_Release( handler->handler );
        free( handler );
        break;
    }
    return S_OK;
}

static void compositor_notify_commit_needed( struct compositor *compositor )
{
    struct compositor_controller *controller = compositor->controller;
    struct compositor_commit_handler *entry;
    ITypedEventHandler_CompositorController_IInspectable **handlers;
    unsigned int count = 0, i = 0;
    HRESULT hr;

    if (!controller) return;
    ICompositorController_AddRef( &controller->ICompositorController_iface );
    LIST_FOR_EACH_ENTRY( entry, &controller->commit_handlers,
            struct compositor_commit_handler, entry ) ++count;
    if (!count || !(handlers = malloc( count * sizeof(*handlers) )))
    {
        ICompositorController_Release( &controller->ICompositorController_iface );
        return;
    }
    LIST_FOR_EACH_ENTRY( entry, &controller->commit_handlers,
            struct compositor_commit_handler, entry )
    {
        handlers[i] = entry->handler;
        ITypedEventHandler_CompositorController_IInspectable_AddRef( handlers[i++] );
    }
    for (i = 0; i < count; ++i)
    {
        TRACE( "controller %p CommitNeeded handler %u/%u begin.\n", controller, i, count );
        hr = ITypedEventHandler_CompositorController_IInspectable_Invoke(
                handlers[i], &controller->ICompositorController_iface, NULL );
        TRACE( "controller %p CommitNeeded handler %u/%u end hr %#lx.\n", controller, i, count, hr );
        if (FAILED(hr)) WARN( "CommitNeeded handler returned %#lx.\n", hr );
        ITypedEventHandler_CompositorController_IInspectable_Release( handlers[i] );
    }
    free( handlers );
    ICompositorController_Release( &controller->ICompositorController_iface );
}

static const ICompositorControllerVtbl compositor_controller_vtbl =
{
    compositor_controller_QueryInterface,
    compositor_controller_AddRef,
    compositor_controller_Release,
    compositor_controller_GetIids,
    compositor_controller_GetRuntimeClassName,
    compositor_controller_GetTrustLevel,
    compositor_controller_get_Compositor,
    compositor_controller_Commit,
    compositor_controller_EnsurePreviousCommitCompletedAsync,
    compositor_controller_add_CommitNeeded,
    compositor_controller_remove_CommitNeeded,
};

static HRESULT WINAPI controller_closable_QueryInterface( IClosable *iface, REFIID iid, void **out )
{
    struct compositor_controller *impl = controller_from_IClosable( iface );
    return ICompositorController_QueryInterface( &impl->ICompositorController_iface, iid, out );
}

static ULONG WINAPI controller_closable_AddRef( IClosable *iface )
{
    struct compositor_controller *impl = controller_from_IClosable( iface );
    return ICompositorController_AddRef( &impl->ICompositorController_iface );
}

static ULONG WINAPI controller_closable_Release( IClosable *iface )
{
    struct compositor_controller *impl = controller_from_IClosable( iface );
    return ICompositorController_Release( &impl->ICompositorController_iface );
}

static HRESULT WINAPI controller_closable_GetIids( IClosable *iface, ULONG *count, IID **iids )
{
    struct compositor_controller *impl = controller_from_IClosable( iface );
    return ICompositorController_GetIids( &impl->ICompositorController_iface, count, iids );
}

static HRESULT WINAPI controller_closable_GetRuntimeClassName( IClosable *iface, HSTRING *name )
{
    struct compositor_controller *impl = controller_from_IClosable( iface );
    return ICompositorController_GetRuntimeClassName( &impl->ICompositorController_iface, name );
}

static HRESULT WINAPI controller_closable_GetTrustLevel( IClosable *iface, TrustLevel *level )
{
    struct compositor_controller *impl = controller_from_IClosable( iface );
    return ICompositorController_GetTrustLevel( &impl->ICompositorController_iface, level );
}

static HRESULT WINAPI controller_closable_Close( IClosable *iface )
{
    TRACE( "iface %p.\n", iface );
    return S_OK;
}

static const IClosableVtbl controller_closable_vtbl =
{
    controller_closable_QueryInterface,
    controller_closable_AddRef,
    controller_closable_Release,
    controller_closable_GetIids,
    controller_closable_GetRuntimeClassName,
    controller_closable_GetTrustLevel,
    controller_closable_Close,
};

static HRESULT compositor_controller_create( IInspectable **result )
{
    struct compositor_controller *impl;
    IInspectable *compositor;
    HRESULT hr;

    if (!result) return E_POINTER;
    *result = NULL;
    if (!(impl = calloc( 1, sizeof(*impl) ))) return E_OUTOFMEMORY;
    if (FAILED(hr = compositor_create( &compositor )))
    {
        free( impl );
        return hr;
    }
    impl->ICompositorController_iface.lpVtbl = &compositor_controller_vtbl;
    impl->IClosable_iface.lpVtbl = &controller_closable_vtbl;
    impl->compositor = (ICompositor *)compositor;
    impl_from_ICompositor( impl->compositor )->controller = impl;
    list_init( &impl->commit_handlers );
    impl->ref = 1;
    *result = (IInspectable *)&impl->ICompositorController_iface;
    return S_OK;
}

struct activation_factory
{
    IActivationFactory IActivationFactory_iface;
    HRESULT (*create)( IInspectable **result );
    LONG ref;
};

static inline struct activation_factory *impl_from_IActivationFactory( IActivationFactory *iface )
{
    return CONTAINING_RECORD( iface, struct activation_factory, IActivationFactory_iface );
}

static HRESULT WINAPI activation_factory_QueryInterface( IActivationFactory *iface,
        REFIID iid, void **out )
{
    FIXME( "iface %p, iid %s, out %p.\n", iface, debugstr_guid( iid ), out );
    if (!out) return E_POINTER;
    if (IsEqualGUID( iid, &IID_IUnknown ) || IsEqualGUID( iid, &IID_IInspectable ) ||
        IsEqualGUID( iid, &IID_IAgileObject ) || IsEqualGUID( iid, &IID_IActivationFactory ))
    {
        *out = iface;
        IActivationFactory_AddRef( iface );
        return S_OK;
    }
    *out = NULL;
    return E_NOINTERFACE;
}

static ULONG WINAPI activation_factory_AddRef( IActivationFactory *iface )
{
    return InterlockedIncrement( &impl_from_IActivationFactory( iface )->ref );
}

static ULONG WINAPI activation_factory_Release( IActivationFactory *iface )
{
    return InterlockedDecrement( &impl_from_IActivationFactory( iface )->ref );
}

static HRESULT WINAPI activation_factory_GetIids( IActivationFactory *iface, ULONG *count, IID **iids )
{
    return E_NOTIMPL;
}

static HRESULT WINAPI activation_factory_GetRuntimeClassName( IActivationFactory *iface, HSTRING *name )
{
    return E_NOTIMPL;
}

static HRESULT WINAPI activation_factory_GetTrustLevel( IActivationFactory *iface, TrustLevel *level )
{
    return E_NOTIMPL;
}

static HRESULT WINAPI activation_factory_ActivateInstance( IActivationFactory *iface, IInspectable **instance )
{
    struct activation_factory *impl = impl_from_IActivationFactory( iface );
    FIXME( "iface %p, instance %p.\n", iface, instance );
    return impl->create( instance );
}

static const IActivationFactoryVtbl activation_factory_vtbl =
{
    activation_factory_QueryInterface,
    activation_factory_AddRef,
    activation_factory_Release,
    activation_factory_GetIids,
    activation_factory_GetRuntimeClassName,
    activation_factory_GetTrustLevel,
    activation_factory_ActivateInstance,
};

static struct activation_factory compositor_factory_impl =
{
    {&activation_factory_vtbl},
    compositor_create,
    1,
};

static struct activation_factory compositor_controller_factory_impl =
{
    {&activation_factory_vtbl},
    compositor_controller_create,
    1,
};

IActivationFactory *compositor_factory = &compositor_factory_impl.IActivationFactory_iface;
IActivationFactory *compositor_controller_factory = &compositor_controller_factory_impl.IActivationFactory_iface;
