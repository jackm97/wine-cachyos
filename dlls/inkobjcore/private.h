/*
 * Copyright 2023 Mohamad Al-Jaf
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

#ifndef __WINE_INKOBJCORE_PRIVATE_H
#define __WINE_INKOBJCORE_PRIVATE_H

#include <stdarg.h>
#include <stdlib.h>
#define COBJMACROS
#include "windef.h"
#include "winbase.h"
#include "objbase.h"
#include "winstring.h"
#define WIDL_using_Windows_Foundation
#define WIDL_using_Windows_Foundation_Collections
#define WIDL_using_Windows_Foundation_Numerics
#define WIDL_using_Windows_UI
#define WIDL_using_Windows_UI_Input_Inking
#define WIDL_using_Windows_UI_Core
#define WIDL_using_Windows_UI_Composition
#define WIDL_using_Windows_UI_Input_Inking_Core
#include "windows.ui.input.inking.h"
#include "windows.ui.input.inking.core.h"
#include "activation.h"

#include "wine/list.h"

struct ink_events
{
    CRITICAL_SECTION lock;
    struct list handlers;
    LONGLONG next_token;
    BOOL closed;
};

typedef HRESULT (*ink_event_callback)(IUnknown *handler, void *sender, void *args);
enum ink_pointer_event
{
    INK_STROKE_STARTED = 16,
    INK_STROKE_CONTINUED,
    INK_STROKE_ENDED,
    INK_STROKE_CANCELED,
    INK_POINTER_ENTERED = 32,
    INK_POINTER_HOVERED,
    INK_POINTER_EXITED,
    INK_POINTER_PRESSED,
    INK_POINTER_MOVED,
    INK_POINTER_RELEASED,
    INK_POINTER_LOST
};
void ink_events_init(struct ink_events *events);
void ink_events_clear(struct ink_events *events);
HRESULT ink_event_add(struct ink_events *events, UINT id, IUnknown *handler, EventRegistrationToken *token);
HRESULT ink_event_remove(struct ink_events *events, UINT id, EventRegistrationToken token);
HRESULT ink_event_notify(struct ink_events *events, UINT id, void *sender, void *args, ink_event_callback invoke);

struct ink_pending_stroke
{
    struct list entry;
    IInkStroke *stroke;
};

HRESULT ink_stroke_view_take(struct list *strokes, IVectorView_InkStroke **out);
HRESULT ink_synchronizer_create(IInkSynchronizer **out);
HRESULT ink_synchronizer_queue(IInkSynchronizer *iface, IInkStroke *stroke);

extern LONG ink_object_count, ink_factory_refs;
HRESULT ink_point_create(struct Point position, float pressure, float tilt_x, float tilt_y,
        UINT64 timestamp, IInkPoint **out);
HRESULT ink_drawing_attributes_create(IInkDrawingAttributes **out);
HRESULT ink_drawing_attributes_clone(IInkDrawingAttributes *source, IInkDrawingAttributes **out);
HRESULT ink_input_configuration_create(IInkInputProcessingConfiguration **out);
HRESULT ink_presenter_create(IInkPresenter **out);
HRESULT ink_presenter_notify_pointer(IInkPresenter *iface, enum ink_pointer_event id, IPointerEventArgs *args);
HRESULT ink_core_host_create(IInspectable **out);
HRESULT ink_source_get_factory(BOOL wet, IActivationFactory **out);

#define DEFINE_IINSPECTABLE_( pfx, iface_type, impl_type, impl_from, iface_mem, expr )             \
    static inline impl_type *impl_from( iface_type *iface )                                        \
    {                                                                                              \
        return CONTAINING_RECORD( iface, impl_type, iface_mem );                                   \
    }                                                                                              \
    static HRESULT WINAPI pfx##_QueryInterface( iface_type *iface, REFIID iid, void **out )        \
    {                                                                                              \
        impl_type *impl = impl_from( iface );                                                      \
        return IInspectable_QueryInterface( (IInspectable *)(expr), iid, out );                    \
    }                                                                                              \
    static ULONG WINAPI pfx##_AddRef( iface_type *iface )                                          \
    {                                                                                              \
        impl_type *impl = impl_from( iface );                                                      \
        return IInspectable_AddRef( (IInspectable *)(expr) );                                      \
    }                                                                                              \
    static ULONG WINAPI pfx##_Release( iface_type *iface )                                         \
    {                                                                                              \
        impl_type *impl = impl_from( iface );                                                      \
        return IInspectable_Release( (IInspectable *)(expr) );                                     \
    }                                                                                              \
    static HRESULT WINAPI pfx##_GetIids( iface_type *iface, ULONG *iid_count, IID **iids )         \
    {                                                                                              \
        impl_type *impl = impl_from( iface );                                                      \
        return IInspectable_GetIids( (IInspectable *)(expr), iid_count, iids );                    \
    }                                                                                              \
    static HRESULT WINAPI pfx##_GetRuntimeClassName( iface_type *iface, HSTRING *class_name )      \
    {                                                                                              \
        impl_type *impl = impl_from( iface );                                                      \
        return IInspectable_GetRuntimeClassName( (IInspectable *)(expr), class_name );             \
    }                                                                                              \
    static HRESULT WINAPI pfx##_GetTrustLevel( iface_type *iface, TrustLevel *trust_level )        \
    {                                                                                              \
        impl_type *impl = impl_from( iface );                                                      \
        return IInspectable_GetTrustLevel( (IInspectable *)(expr), trust_level );                  \
    }
#define DEFINE_IINSPECTABLE( pfx, iface_type, impl_type, base_iface )                              \
    DEFINE_IINSPECTABLE_( pfx, iface_type, impl_type, impl_from_##iface_type, iface_type##_iface, &impl->base_iface )


#endif
