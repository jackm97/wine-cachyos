/* Windows.UI.Composition effect graphs
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

#ifndef __WINE_COMPOSITION_EFFECT_GRAPH_H
#define __WINE_COMPOSITION_EFFECT_GRAPH_H

#include "private.h"
#include "d2d1_1.h"

struct graphics_effect_interop;
struct graphics_effect_interop_vtbl
{
    HRESULT (WINAPI *QueryInterface)( struct graphics_effect_interop *, REFIID, void ** );
    ULONG (WINAPI *AddRef)( struct graphics_effect_interop * );
    ULONG (WINAPI *Release)( struct graphics_effect_interop * );
    HRESULT (WINAPI *GetEffectId)( struct graphics_effect_interop *, GUID * );
    HRESULT (WINAPI *GetNamedPropertyMapping)( struct graphics_effect_interop *, const WCHAR *, UINT *, UINT * );
    HRESULT (WINAPI *GetPropertyCount)( struct graphics_effect_interop *, UINT * );
    HRESULT (WINAPI *GetProperty)( struct graphics_effect_interop *, UINT, IPropertyValue ** );
    HRESULT (WINAPI *GetSource)( struct graphics_effect_interop *, UINT, IGraphicsEffectSource ** );
    HRESULT (WINAPI *GetSourceCount)( struct graphics_effect_interop *, UINT * );
};
struct graphics_effect_interop
{
    const struct graphics_effect_interop_vtbl *lpVtbl;
};

extern const GUID composition_effect_interop_iid;

enum composition_effect_property_mapping
{
    EFFECT_PROPERTY_DIRECT = 1,
    EFFECT_PROPERTY_VECTOR_X,
    EFFECT_PROPERTY_VECTOR_Y,
    EFFECT_PROPERTY_VECTOR_Z,
    EFFECT_PROPERTY_VECTOR_W,
};

struct composition_effect_property
{
    HSTRING name;
    UINT mapping;
};

struct composition_effect_node
{
    HSTRING name;
    HSTRING source_name;
    BOOL parameter;
    D2D1_VECTOR_4F coefficients;
    BOOL clamp_output;
    UINT property_count;
    struct composition_effect_property *properties;
    struct composition_effect_node *sources[2];
};

struct composition_effect_graph
{
    LONG ref;
    struct composition_effect_node *root;
};

typedef HRESULT (*composition_effect_source_lookup)( void *, HSTRING, ID2D1Image ** );
HRESULT composition_effect_graph_compile( IGraphicsEffect *, struct composition_effect_graph ** );
HRESULT composition_effect_graph_compile_properties( IGraphicsEffect *, __FIIterable_1_HSTRING *,
        struct composition_effect_graph ** );
HRESULT composition_effect_graph_init_properties( struct composition_effect_graph *, ICompositionPropertySet * );
ULONG composition_effect_graph_add_ref( struct composition_effect_graph * );
ULONG composition_effect_graph_release( struct composition_effect_graph * );
HRESULT composition_effect_graph_create_image( struct composition_effect_graph *, ID2D1DeviceContext *,
        composition_effect_source_lookup, void *, ID2D1Image ** );
HRESULT composition_effect_graph_create_image_properties( struct composition_effect_graph *, ID2D1DeviceContext *,
        ICompositionPropertySet *, composition_effect_source_lookup, void *, ID2D1Image ** );

#endif
