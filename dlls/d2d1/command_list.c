/*
 * Copyright 2022 Nikolay Sivov for CodeWeavers
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

#include "d2d1_private.h"

WINE_DEFAULT_DEBUG_CHANNEL(d2d);

enum d2d_command_list_flags
{
    D2D_COMMAND_LIST_HAS_NULL_TEXT_RENDERING_PARAMS = 0x1,
};

enum d2d_command_type
{
    D2D_COMMAND_SET_ANTIALIAS_MODE,
    D2D_COMMAND_SET_TAGS,
    D2D_COMMAND_SET_TEXT_ANTIALIAS_MODE,
    D2D_COMMAND_SET_TEXT_RENDERING_PARAMS,
    D2D_COMMAND_SET_TRANSFORM,
    D2D_COMMAND_SET_PRIMITIVE_BLEND,
    D2D_COMMAND_SET_UNIT_MODE,
    D2D_COMMAND_CLEAR,
    D2D_COMMAND_DRAW_GLYPH_RUN,
    D2D_COMMAND_DRAW_LINE,
    D2D_COMMAND_DRAW_GEOMETRY,
    D2D_COMMAND_DRAW_RECTANGLE,
    D2D_COMMAND_DRAW_BITMAP,
    D2D_COMMAND_DRAW_IMAGE,
    D2D_COMMAND_DRAW_SPRITE_BATCH,
    D2D_COMMAND_FILL_MESH,
    D2D_COMMAND_FILL_OPACITY_MASK,
    D2D_COMMAND_FILL_GEOMETRY,
    D2D_COMMAND_FILL_RECTANGLE,
    D2D_COMMAND_PUSH_CLIP,
    D2D_COMMAND_PUSH_LAYER,
    D2D_COMMAND_POP_CLIP,
    D2D_COMMAND_POP_LAYER,
};

struct d2d_command
{
    enum d2d_command_type op;
    size_t size;
};

struct d2d_command_set_antialias_mode
{
    struct d2d_command c;
    D2D1_ANTIALIAS_MODE mode;
};

struct d2d_command_set_tags
{
    struct d2d_command c;
    D2D1_TAG tag1, tag2;
};

struct d2d_command_set_text_antialias_mode
{
    struct d2d_command c;
    D2D1_TEXT_ANTIALIAS_MODE mode;
};

struct d2d_command_set_text_rendering_params
{
    struct d2d_command c;
    IDWriteRenderingParams *params;
};

struct d2d_command_set_transform
{
    struct d2d_command c;
    D2D1_MATRIX_3X2_F transform;
};

struct d2d_command_set_primitive_blend
{
    struct d2d_command c;
    D2D1_PRIMITIVE_BLEND primitive_blend;
};

struct d2d_command_set_unit_mode
{
    struct d2d_command c;
    D2D1_UNIT_MODE mode;
};

struct d2d_command_clear
{
    struct d2d_command c;
    D2D1_COLOR_F color;
};

struct d2d_command_push_clip
{
    struct d2d_command c;
    D2D1_RECT_F rect;
    D2D1_ANTIALIAS_MODE mode;
};

struct d2d_command_push_layer
{
    struct d2d_command c;
    D2D1_LAYER_PARAMETERS1 params;
};

struct d2d_command_draw_line
{
    struct d2d_command c;
    D2D1_POINT_2F p0, p1;
    ID2D1Brush *brush;
    float stroke_width;
    ID2D1StrokeStyle *stroke_style;
};

struct d2d_command_draw_geometry
{
    struct d2d_command c;
    ID2D1Geometry *geometry;
    ID2D1Brush *brush;
    float stroke_width;
    ID2D1StrokeStyle *stroke_style;
};

struct d2d_command_draw_rectangle
{
    struct d2d_command c;
    D2D1_RECT_F rect;
    ID2D1Brush *brush;
    float stroke_width;
    ID2D1StrokeStyle *stroke_style;
};

struct d2d_command_fill_mesh
{
    struct d2d_command c;
    ID2D1Mesh *mesh;
    ID2D1Brush *brush;
};

struct d2d_command_fill_opacity_mask
{
    struct d2d_command c;
    ID2D1Bitmap *bitmap;
    ID2D1Brush *brush;
    D2D1_RECT_F *dst_rect;
    D2D1_RECT_F *src_rect;
};

struct d2d_command_fill_geometry
{
    struct d2d_command c;
    ID2D1Geometry *geometry;
    ID2D1Brush *brush;
    ID2D1Brush *opacity_brush;
};

struct d2d_command_fill_rectangle
{
    struct d2d_command c;
    D2D1_RECT_F rect;
    ID2D1Brush *brush;
};

struct d2d_command_draw_glyph_run
{
    struct d2d_command c;
    D2D1_POINT_2F origin;
    DWRITE_MEASURING_MODE measuring_mode;
    ID2D1Brush *brush;
    DWRITE_GLYPH_RUN run;
    DWRITE_GLYPH_RUN_DESCRIPTION *run_desc;
};

struct d2d_command_draw_bitmap
{
    struct d2d_command c;
    float opacity;
    D2D1_INTERPOLATION_MODE interpolation_mode;
    ID2D1Bitmap *bitmap;
    D2D1_RECT_F *dst_rect;
    D2D1_RECT_F *src_rect;
    D2D1_MATRIX_4X4_F *perspective_transform;
};

struct d2d_command_draw_image
{
    struct d2d_command c;
    ID2D1Image *image;
    D2D1_INTERPOLATION_MODE interpolation_mode;
    D2D1_COMPOSITE_MODE composite_mode;
    D2D1_POINT_2F *target_offset;
    D2D1_RECT_F *image_rect;
};

struct d2d_command_draw_sprite_batch
{
    struct d2d_command c;
    ID2D1SpriteBatch *sprite_batch;
    ID2D1Bitmap *bitmap;
    UINT32 start_index;
    UINT32 sprite_count;
    D2D1_BITMAP_INTERPOLATION_MODE interpolation_mode;
    D2D1_SPRITE_OPTIONS sprite_options;
};

static inline struct d2d_command_list *impl_from_ID2D1CommandList(ID2D1CommandList *iface)
{
    return CONTAINING_RECORD(iface, struct d2d_command_list, ID2D1CommandList_iface);
}

static HRESULT STDMETHODCALLTYPE d2d_command_list_QueryInterface(ID2D1CommandList *iface, REFIID iid, void **out)
{
    TRACE("iface %p, iid %s, out %p.\n", iface, debugstr_guid(iid), out);

    if (IsEqualGUID(iid, &IID_ID2D1CommandList)
            || IsEqualGUID(iid, &IID_ID2D1Image)
            || IsEqualGUID(iid, &IID_ID2D1Resource)
            || IsEqualGUID(iid, &IID_IUnknown))
    {
        ID2D1CommandList_AddRef(iface);
        *out = iface;
        return S_OK;
    }

    WARN("%s not implemented, returning E_NOINTERFACE.\n", debugstr_guid(iid));

    *out = NULL;
    return E_NOINTERFACE;
}

static ULONG STDMETHODCALLTYPE d2d_command_list_AddRef(ID2D1CommandList *iface)
{
    struct d2d_command_list *command_list = impl_from_ID2D1CommandList(iface);
    ULONG refcount = InterlockedIncrement(&command_list->refcount);

    TRACE("%p increasing refcount to %lu.\n", iface, refcount);

    return refcount;
}

static ULONG STDMETHODCALLTYPE d2d_command_list_Release(ID2D1CommandList *iface)
{
    struct d2d_command_list *command_list = impl_from_ID2D1CommandList(iface);
    ULONG refcount = InterlockedDecrement(&command_list->refcount);
    size_t i;

    TRACE("%p decreasing refcount to %lu.\n", iface, refcount);

    if (!refcount)
    {
        ID2D1Factory_Release(command_list->factory);
        for (i = 0; i < command_list->objects_count; ++i)
            IUnknown_Release(command_list->objects[i]);
        free(command_list->objects);
        free(command_list->data);
        free(command_list);
    }

    return refcount;
}

static void STDMETHODCALLTYPE d2d_command_list_GetFactory(ID2D1CommandList *iface, ID2D1Factory **factory)
{
    struct d2d_command_list *command_list = impl_from_ID2D1CommandList(iface);

    TRACE("iface %p, factory %p.\n", iface, factory);

    ID2D1Factory_AddRef(*factory = command_list->factory);
}

static HRESULT STDMETHODCALLTYPE d2d_command_list_Stream(ID2D1CommandList *iface, ID2D1CommandSink *sink)
{
    struct d2d_command_list *command_list = impl_from_ID2D1CommandList(iface);
    const void *data, *end;
    HRESULT hr;

    TRACE("iface %p, sink %p.\n", iface, sink);

    if (command_list->state != D2D_COMMAND_LIST_STATE_CLOSED) return S_OK;

    if (FAILED(hr = ID2D1CommandSink_BeginDraw(sink)))
        return hr;

    data = command_list->data;
    end = (char *)command_list->data + command_list->size;
    while (data < end)
    {
        const struct d2d_command *command = data;

        switch (command->op)
        {
            case D2D_COMMAND_SET_ANTIALIAS_MODE:
            {
                const struct d2d_command_set_antialias_mode *c = data;
                hr = ID2D1CommandSink_SetAntialiasMode(sink, c->mode);
                break;
            }
            case D2D_COMMAND_SET_TAGS:
            {
                const struct d2d_command_set_tags *c = data;
                hr = ID2D1CommandSink_SetTags(sink, c->tag1, c->tag2);
                break;
            }
            case D2D_COMMAND_SET_TEXT_ANTIALIAS_MODE:
            {
                const struct d2d_command_set_text_antialias_mode *c = data;
                hr = ID2D1CommandSink_SetTextAntialiasMode(sink, c->mode);
                break;
            }
            case D2D_COMMAND_SET_TEXT_RENDERING_PARAMS:
            {
                const struct d2d_command_set_text_rendering_params *c = data;
                hr = ID2D1CommandSink_SetTextRenderingParams(sink, c->params);
                break;
            }
            case D2D_COMMAND_SET_TRANSFORM:
            {
                const struct d2d_command_set_transform *c = data;
                hr = ID2D1CommandSink_SetTransform(sink, &c->transform);
                break;
            }
            case D2D_COMMAND_SET_PRIMITIVE_BLEND:
            {
                const struct d2d_command_set_primitive_blend *c = data;
                ID2D1CommandSink1 *sink1;
                ID2D1CommandSink4 *sink4;

                switch (c->primitive_blend)
                {
                    case D2D1_PRIMITIVE_BLEND_SOURCE_OVER:
                    case D2D1_PRIMITIVE_BLEND_COPY:
                        hr = ID2D1CommandSink_SetPrimitiveBlend(sink, c->primitive_blend);
                        break;
                    case D2D1_PRIMITIVE_BLEND_MIN:
                    case D2D1_PRIMITIVE_BLEND_ADD:
                        if (SUCCEEDED(ID2D1CommandSink_QueryInterface(sink, &IID_ID2D1CommandSink1, (void **)&sink1)))
                        {
                            hr = ID2D1CommandSink1_SetPrimitiveBlend1(sink1, c->primitive_blend);
                            ID2D1CommandSink1_Release(sink1);
                        }
                        else
                            hr = ID2D1CommandSink_SetPrimitiveBlend(sink, D2D1_PRIMITIVE_BLEND_SOURCE_OVER);
                        break;
                    case D2D1_PRIMITIVE_BLEND_MAX:
                        if (SUCCEEDED(ID2D1CommandSink_QueryInterface(sink, &IID_ID2D1CommandSink4, (void **)&sink4)))
                        {
                            hr = ID2D1CommandSink4_SetPrimitiveBlend2(sink4, c->primitive_blend);
                            ID2D1CommandSink4_Release(sink4);
                        }
                        else
                            hr = ID2D1CommandSink_SetPrimitiveBlend(sink, D2D1_PRIMITIVE_BLEND_SOURCE_OVER);
                        break;
                    default:
                        FIXME("Unexpected blend mode %u.\n", c->primitive_blend);
                        hr = E_UNEXPECTED;
                }
                break;
            }
            case D2D_COMMAND_SET_UNIT_MODE:
            {
                const struct d2d_command_set_unit_mode *c = data;
                hr = ID2D1CommandSink_SetUnitMode(sink, c->mode);
                break;
            }
            case D2D_COMMAND_CLEAR:
            {
                const struct d2d_command_clear *c = data;
                hr = ID2D1CommandSink_Clear(sink, &c->color);
                break;
            }
            case D2D_COMMAND_DRAW_GLYPH_RUN:
            {
                const struct d2d_command_draw_glyph_run *c = data;
                hr = ID2D1CommandSink_DrawGlyphRun(sink, c->origin, &c->run, c->run_desc, c->brush, c->measuring_mode);
                break;
            }
            case D2D_COMMAND_DRAW_LINE:
            {
                const struct d2d_command_draw_line *c = data;
                hr = ID2D1CommandSink_DrawLine(sink, c->p0, c->p1, c->brush, c->stroke_width,
                        c->stroke_style);
                break;
            }
            case D2D_COMMAND_DRAW_GEOMETRY:
            {
                const struct d2d_command_draw_geometry *c = data;
                hr = ID2D1CommandSink_DrawGeometry(sink, c->geometry, c->brush, c->stroke_width,
                        c->stroke_style);
                break;
            }
            case D2D_COMMAND_DRAW_RECTANGLE:
            {
                const struct d2d_command_draw_rectangle *c = data;
                hr = ID2D1CommandSink_DrawRectangle(sink, &c->rect, c->brush, c->stroke_width,
                        c->stroke_style);
                break;
            }
            case D2D_COMMAND_DRAW_BITMAP:
            {
                const struct d2d_command_draw_bitmap *c = data;
                hr = ID2D1CommandSink_DrawBitmap(sink, c->bitmap, c->dst_rect, c->opacity,
                        c->interpolation_mode, c->src_rect, c->perspective_transform);
                break;
            }
            case D2D_COMMAND_DRAW_IMAGE:
            {
                const struct d2d_command_draw_image *c = data;
                hr = ID2D1CommandSink_DrawImage(sink, c->image, c->target_offset, c->image_rect,
                        c->interpolation_mode, c->composite_mode);
                break;
            }
            case D2D_COMMAND_DRAW_SPRITE_BATCH:
            {
                const struct d2d_command_draw_sprite_batch *c = data;
                ID2D1CommandSink3 *sink3;

                if (SUCCEEDED(ID2D1CommandSink_QueryInterface(sink, &IID_ID2D1CommandSink3, (void **)&sink3)))
                {
                    hr = ID2D1CommandSink3_DrawSpriteBatch(sink3, c->sprite_batch, c->start_index,
                            c->sprite_count, c->bitmap, c->interpolation_mode, c->sprite_options);
                    ID2D1CommandSink3_Release(sink3);
                }
                else
                {
                    FIXME("DrawSpriteBatch() fallback is not implemented.\n");
                    hr = E_NOTIMPL;
                }
                break;
            }
            case D2D_COMMAND_FILL_MESH:
            {
                const struct d2d_command_fill_mesh *c = data;
                hr = ID2D1CommandSink_FillMesh(sink, c->mesh, c->brush);
                break;
            }
            case D2D_COMMAND_FILL_OPACITY_MASK:
            {
                const struct d2d_command_fill_opacity_mask *c = data;
                hr = ID2D1CommandSink_FillOpacityMask(sink, c->bitmap, c->brush, c->dst_rect, c->src_rect);
                break;
            }
            case D2D_COMMAND_FILL_GEOMETRY:
            {
                const struct d2d_command_fill_geometry *c = data;
                hr = ID2D1CommandSink_FillGeometry(sink, c->geometry, c->brush, c->opacity_brush);
                break;
            }
            case D2D_COMMAND_FILL_RECTANGLE:
            {
                const struct d2d_command_fill_rectangle *c = data;
                hr = ID2D1CommandSink_FillRectangle(sink, &c->rect, c->brush);
                break;
            }
            case D2D_COMMAND_PUSH_CLIP:
            {
                const struct d2d_command_push_clip *c = data;
                hr = ID2D1CommandSink_PushAxisAlignedClip(sink, &c->rect, c->mode);
                break;
            }
            case D2D_COMMAND_PUSH_LAYER:
            {
                const struct d2d_command_push_layer *c = data;
                hr = ID2D1CommandSink_PushLayer(sink, &c->params, NULL);
                break;
            }
            case D2D_COMMAND_POP_CLIP:
                hr = ID2D1CommandSink_PopAxisAlignedClip(sink);
                break;
            case D2D_COMMAND_POP_LAYER:
                hr = ID2D1CommandSink_PopLayer(sink);
                break;
            default:
                FIXME("Unhandled command %u.\n", command->op);
                hr = E_UNEXPECTED;
        }

        if (FAILED(hr)) return hr;

        data = (char *)data + command->size;
    }

    return ID2D1CommandSink_EndDraw(sink);
}

static HRESULT d2d_command_list_glyph_bounds(struct d2d_command_list *list,
        const struct d2d_command_draw_glyph_run *command, const D2D1_MATRIX_3X2_F *transform,
        float dpi_x, float dpi_y, D2D1_UNIT_MODE output_mode, D2D1_TEXT_ANTIALIAS_MODE aa_mode,
        IDWriteRenderingParams *params, D2D1_RECT_F *rect)
{
    DWRITE_TEXT_ANTIALIAS_MODE text_aa = DWRITE_TEXT_ANTIALIAS_MODE_GRAYSCALE;
    DWRITE_RENDERING_MODE rendering_mode = DWRITE_RENDERING_MODE_DEFAULT;
    IDWriteRenderingParams *default_params = NULL;
    IDWriteGlyphRunAnalysis *analysis;
    D2D1_MATRIX_3X2_F matrix = *transform;
    IDWriteFactory2 *factory;
    float scale_x = output_mode == D2D1_UNIT_MODE_DIPS ? dpi_x / 96.0f : 1.0f;
    float scale_y = output_mode == D2D1_UNIT_MODE_DIPS ? dpi_y / 96.0f : 1.0f;
    RECT bounds;
    HRESULT hr;

    if (FAILED(hr = DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED,
            &IID_IDWriteFactory2, (IUnknown **)&factory))) return hr;
    if (!params)
    {
        if (FAILED(hr = IDWriteFactory2_CreateRenderingParams(factory, &default_params))) goto done;
        params = default_params;
    }
    if (aa_mode == D2D1_TEXT_ANTIALIAS_MODE_ALIASED)
        rendering_mode = DWRITE_RENDERING_MODE_ALIASED;
    else if (FAILED(hr = IDWriteFontFace_GetRecommendedRenderingMode(command->run.fontFace,
            command->run.fontEmSize, max(dpi_x, dpi_y) / 96.0f,
            command->measuring_mode, params, &rendering_mode))) goto done;
    if (aa_mode == D2D1_TEXT_ANTIALIAS_MODE_CLEARTYPE
            || (aa_mode == D2D1_TEXT_ANTIALIAS_MODE_DEFAULT
            && IDWriteRenderingParams_GetClearTypeLevel(params) > 0.0f))
        text_aa = DWRITE_TEXT_ANTIALIAS_MODE_CLEARTYPE;

    if (rendering_mode == DWRITE_RENDERING_MODE_OUTLINE)
    {
        ID2D1PathGeometry *geometry;
        ID2D1GeometrySink *sink;

        if (FAILED(hr = ID2D1Factory_CreatePathGeometry(list->factory, &geometry))) goto done;
        if (SUCCEEDED(hr = ID2D1PathGeometry_Open(geometry, &sink)))
        {
            hr = IDWriteFontFace_GetGlyphRunOutline(command->run.fontFace, command->run.fontEmSize,
                    command->run.glyphIndices, command->run.glyphAdvances, command->run.glyphOffsets,
                    command->run.glyphCount, command->run.isSideways, command->run.bidiLevel & 1,
                    (IDWriteGeometrySink *)sink);
            if (SUCCEEDED(hr)) hr = ID2D1GeometrySink_Close(sink);
            ID2D1GeometrySink_Release(sink);
            matrix._31 += command->origin.x * matrix._11 + command->origin.y * matrix._21;
            matrix._32 += command->origin.x * matrix._12 + command->origin.y * matrix._22;
            if (SUCCEEDED(hr)) hr = ID2D1PathGeometry_GetBounds(geometry, &matrix, rect);
        }
        ID2D1PathGeometry_Release(geometry);
        goto done;
    }
    matrix._11 *= scale_x; matrix._21 *= scale_x; matrix._31 *= scale_x;
    matrix._12 *= scale_y; matrix._22 *= scale_y; matrix._32 *= scale_y;
    hr = IDWriteFactory2_CreateGlyphRunAnalysis(factory, &command->run, (DWRITE_MATRIX *)&matrix,
            rendering_mode, command->measuring_mode, DWRITE_GRID_FIT_MODE_DEFAULT, text_aa,
            command->origin.x, command->origin.y, &analysis);
    if (FAILED(hr)) goto done;
    hr = IDWriteGlyphRunAnalysis_GetAlphaTextureBounds(analysis,
            rendering_mode == DWRITE_RENDERING_MODE_ALIASED || text_aa == DWRITE_TEXT_ANTIALIAS_MODE_GRAYSCALE
            ? DWRITE_TEXTURE_ALIASED_1x1 : DWRITE_TEXTURE_CLEARTYPE_3x1, &bounds);
    IDWriteGlyphRunAnalysis_Release(analysis);
    if (SUCCEEDED(hr))
    {
        rect->left = bounds.left / scale_x;
        rect->top = bounds.top / scale_y;
        rect->right = bounds.right / scale_x;
        rect->bottom = bounds.bottom / scale_y;
    }
done:
    if (default_params) IDWriteRenderingParams_Release(default_params);
    IDWriteFactory2_Release(factory);
    return hr;
}

struct d2d_command_bounds_sink
{
    ID2D1SimplifiedGeometrySink ID2D1SimplifiedGeometrySink_iface;
    D2D1_RECT_F bounds;
    BOOL empty;
};

static HRESULT STDMETHODCALLTYPE d2d_command_bounds_QueryInterface(ID2D1SimplifiedGeometrySink *iface,
        REFIID iid, void **object)
{
    if (IsEqualGUID(iid, &IID_IUnknown) || IsEqualGUID(iid, &IID_ID2D1SimplifiedGeometrySink))
    {
        *object = iface;
        return S_OK;
    }
    *object = NULL;
    return E_NOINTERFACE;
}

static ULONG STDMETHODCALLTYPE d2d_command_bounds_AddRef(ID2D1SimplifiedGeometrySink *iface)
{
    return 2;
}

static ULONG STDMETHODCALLTYPE d2d_command_bounds_Release(ID2D1SimplifiedGeometrySink *iface)
{
    return 1;
}

static void STDMETHODCALLTYPE d2d_command_bounds_SetFillMode(ID2D1SimplifiedGeometrySink *iface, D2D1_FILL_MODE mode) {}
static void STDMETHODCALLTYPE d2d_command_bounds_SetSegmentFlags(ID2D1SimplifiedGeometrySink *iface, D2D1_PATH_SEGMENT flags) {}

static void STDMETHODCALLTYPE d2d_command_bounds_AddLines(ID2D1SimplifiedGeometrySink *iface,
        const D2D1_POINT_2F *points, UINT32 count)
{
    struct d2d_command_bounds_sink *sink = CONTAINING_RECORD(iface,
            struct d2d_command_bounds_sink, ID2D1SimplifiedGeometrySink_iface);
    UINT32 i;

    for (i = 0; i < count; ++i)
    {
        if (sink->empty)
        {
            sink->bounds.left = sink->bounds.right = points[i].x;
            sink->bounds.top = sink->bounds.bottom = points[i].y;
            sink->empty = FALSE;
        }
        else d2d_rect_expand(&sink->bounds, &points[i]);
    }
}

static void STDMETHODCALLTYPE d2d_command_bounds_BeginFigure(ID2D1SimplifiedGeometrySink *iface,
        D2D1_POINT_2F start, D2D1_FIGURE_BEGIN begin)
{
    d2d_command_bounds_AddLines(iface, &start, 1);
}

static void STDMETHODCALLTYPE d2d_command_bounds_AddBeziers(ID2D1SimplifiedGeometrySink *iface,
        const D2D1_BEZIER_SEGMENT *segments, UINT32 count)
{
    UINT32 i;

    for (i = 0; i < count; ++i)
    {
        d2d_command_bounds_AddLines(iface, &segments[i].point1, 1);
        d2d_command_bounds_AddLines(iface, &segments[i].point2, 1);
        d2d_command_bounds_AddLines(iface, &segments[i].point3, 1);
    }
}

static void STDMETHODCALLTYPE d2d_command_bounds_EndFigure(ID2D1SimplifiedGeometrySink *iface, D2D1_FIGURE_END end) {}
static HRESULT STDMETHODCALLTYPE d2d_command_bounds_Close(ID2D1SimplifiedGeometrySink *iface) {return S_OK;}

static const ID2D1SimplifiedGeometrySinkVtbl d2d_command_bounds_vtbl =
{
    d2d_command_bounds_QueryInterface,
    d2d_command_bounds_AddRef,
    d2d_command_bounds_Release,
    d2d_command_bounds_SetFillMode,
    d2d_command_bounds_SetSegmentFlags,
    d2d_command_bounds_BeginFigure,
    d2d_command_bounds_AddLines,
    d2d_command_bounds_AddBeziers,
    d2d_command_bounds_EndFigure,
    d2d_command_bounds_Close,
};

static float d2d_command_list_stroke_width(float width, ID2D1StrokeStyle *style,
        const D2D1_MATRIX_3X2_F *transform)
{
    struct d2d_stroke_style *impl = unsafe_impl_from_ID2D1StrokeStyle(style);

    if (impl && impl->desc.transformType == D2D1_STROKE_TRANSFORM_TYPE_FIXED)
        width /= transform->_11;
    return width;
}

static HRESULT d2d_command_list_stroke_bounds(ID2D1Geometry *geometry, float width,
        ID2D1StrokeStyle *style, const D2D1_MATRIX_3X2_F *matrix, D2D1_RECT_F *bounds)
{
    struct d2d_command_bounds_sink sink = {{&d2d_command_bounds_vtbl}, {0}, TRUE};
    float radius, x, y;
    HRESULT hr;

    hr = ID2D1Geometry_GetWidenedBounds(geometry, width, style, matrix,
            D2D1_DEFAULT_FLATTENING_TOLERANCE, bounds);
    if (hr != E_NOTIMPL) return hr;
    hr = ID2D1Geometry_Simplify(geometry, D2D1_GEOMETRY_SIMPLIFICATION_OPTION_CUBICS_AND_LINES,
            matrix, D2D1_DEFAULT_FLATTENING_TOLERANCE, &sink.ID2D1SimplifiedGeometrySink_iface);
    if (FAILED(hr)) return hr;
    *bounds = sink.bounds;
    if (sink.empty) return S_OK;
    /* Control points enclose curves; include caps and miter joins around them. */
    radius = fabsf(width) * 0.5f * max(1.414213562f,
            style ? ID2D1StrokeStyle_GetMiterLimit(style) : 10.0f);
    x = radius * hypotf(matrix->_11, matrix->_21);
    y = radius * hypotf(matrix->_12, matrix->_22);
    bounds->left -= x;
    bounds->right += x;
    bounds->top -= y;
    bounds->bottom += y;
    return S_OK;
}

HRESULT d2d_command_list_get_bounds(ID2D1CommandList *iface, struct d2d_device_context *context,
        const struct d2d_effect_frame *parent, D2D1_UNIT_MODE output_mode, D2D1_RECT_F *bounds)
{
    struct d2d_command_list *list = impl_from_ID2D1CommandList(iface);
    D2D1_MATRIX_3X2_F transform = {._11 = 1.0f, ._22 = 1.0f}, matrix;
    D2D1_UNIT_MODE mode = D2D1_UNIT_MODE_DIPS;
    const struct d2d_effect_frame *ancestor;
    struct d2d_effect_frame frame = {NULL, parent, iface};
    struct {D2D1_RECT_F rect; BOOL layer;} *clips = NULL;
    D2D1_RECT_F rect;
    D2D1_TEXT_ANTIALIAS_MODE text_aa = D2D1_TEXT_ANTIALIAS_MODE_DEFAULT;
    IDWriteRenderingParams *text_params = NULL;
    size_t clip_count = 0, clip_capacity = 0;
    ID2D1RectangleGeometry *rectangle;
    const void *data, *end;
    float dpi_x, dpi_y, scale_x, scale_y;
    BOOL have_bounds = FALSE;
    HRESULT hr = S_OK;

    if (list->state != D2D_COMMAND_LIST_STATE_CLOSED) return D2DERR_WRONG_STATE;
    for (ancestor = parent; ancestor; ancestor = ancestor->parent)
        if (ancestor->commands == iface) return D2DERR_CYCLIC_GRAPH;
    dpi_x = context->desc.dpiX;
    dpi_y = context->desc.dpiY;
    memset(bounds, 0, sizeof(*bounds));
    data = list->data;
    end = (char *)list->data + list->size;
    while (data < end)
    {
        const struct d2d_command *command = data;
        BOOL layer = command->op == D2D_COMMAND_PUSH_LAYER;
        BOOL clip = command->op == D2D_COMMAND_PUSH_CLIP || layer;

        matrix = transform;
        scale_x = mode == output_mode ? 1.0f : mode == D2D1_UNIT_MODE_DIPS ? dpi_x / 96.0f : 96.0f / dpi_x;
        scale_y = mode == output_mode ? 1.0f : mode == D2D1_UNIT_MODE_DIPS ? dpi_y / 96.0f : 96.0f / dpi_y;
        matrix._11 *= scale_x; matrix._21 *= scale_x; matrix._31 *= scale_x;
        matrix._12 *= scale_y; matrix._22 *= scale_y; matrix._32 *= scale_y;
        switch (command->op)
        {
            case D2D_COMMAND_SET_TRANSFORM:
                transform = ((const struct d2d_command_set_transform *)data)->transform;
                goto next;
            case D2D_COMMAND_SET_UNIT_MODE:
                mode = ((const struct d2d_command_set_unit_mode *)data)->mode;
                goto next;
            case D2D_COMMAND_SET_ANTIALIAS_MODE:
            case D2D_COMMAND_SET_TAGS:
            case D2D_COMMAND_SET_PRIMITIVE_BLEND:
                goto next;
            case D2D_COMMAND_SET_TEXT_ANTIALIAS_MODE:
                text_aa = ((const struct d2d_command_set_text_antialias_mode *)data)->mode;
                goto next;
            case D2D_COMMAND_SET_TEXT_RENDERING_PARAMS:
                text_params = ((const struct d2d_command_set_text_rendering_params *)data)->params;
                goto next;
            case D2D_COMMAND_DRAW_GLYPH_RUN:
                if (FAILED(hr = d2d_command_list_glyph_bounds(list, data, &matrix,
                        dpi_x, dpi_y, output_mode, text_aa, text_params, &rect))) goto done;
                break;
            case D2D_COMMAND_POP_LAYER:
            case D2D_COMMAND_POP_CLIP:
                if (!clip_count || clips[clip_count - 1].layer != (command->op == D2D_COMMAND_POP_LAYER)) {hr = D2DERR_PUSH_POP_UNBALANCED; goto done;}
                --clip_count;
                goto next;
            case D2D_COMMAND_PUSH_LAYER:
            case D2D_COMMAND_PUSH_CLIP:
            case D2D_COMMAND_FILL_RECTANGLE:
            case D2D_COMMAND_DRAW_RECTANGLE:
            {
                const D2D1_RECT_F *source;
                if (layer) source = &((const struct d2d_command_push_layer *)data)->params.contentBounds;
                else if (clip) source = &((const struct d2d_command_push_clip *)data)->rect;
                else if (command->op == D2D_COMMAND_FILL_RECTANGLE)
                    source = &((const struct d2d_command_fill_rectangle *)data)->rect;
                else source = &((const struct d2d_command_draw_rectangle *)data)->rect;
                if (FAILED(hr = ID2D1Factory_CreateRectangleGeometry(list->factory, source, &rectangle))) goto done;
                if (command->op == D2D_COMMAND_DRAW_RECTANGLE)
                {
                    const struct d2d_command_draw_rectangle *c = data;
                    hr = d2d_command_list_stroke_bounds((ID2D1Geometry *)rectangle,
                            d2d_command_list_stroke_width(c->stroke_width, c->stroke_style, &transform),
                            c->stroke_style, &matrix, &rect);
                }
                else hr = ID2D1RectangleGeometry_GetBounds(rectangle, &matrix, &rect);
                ID2D1RectangleGeometry_Release(rectangle);
                if (FAILED(hr)) goto done;
                break;
            }
            case D2D_COMMAND_DRAW_IMAGE:
            {
                const struct d2d_command_draw_image *c = data;
                ID2D1Bitmap *bitmap;
                D2D1_RECT_F source;
                BOOL is_bitmap;

                if (c->composite_mode != D2D1_COMPOSITE_MODE_SOURCE_OVER)
                {hr = D2DERR_UNSUPPORTED_OPERATION; goto done;}
                if (FAILED(hr = d2d_effect_image_bounds(context, c->image, &frame, mode, &source))) goto done;
                is_bitmap = SUCCEEDED(ID2D1Image_QueryInterface(c->image, &IID_ID2D1Bitmap, (void **)&bitmap));
                if (is_bitmap) ID2D1Bitmap_Release(bitmap);
                if (c->image_rect)
                {
                    source.left = max(source.left, c->image_rect->left);
                    source.top = max(source.top, c->image_rect->top);
                    source.right = min(source.right, c->image_rect->right);
                    source.bottom = min(source.bottom, c->image_rect->bottom);
                }
                if (source.right <= source.left || source.bottom <= source.top) goto next;
                if (is_bitmap)
                {
                    source.right -= source.left;
                    source.bottom -= source.top;
                    source.left = source.top = 0.0f;
                }
                else if (c->image_rect)
                {
                    source.left -= c->image_rect->left;
                    source.right -= c->image_rect->left;
                    source.top -= c->image_rect->top;
                    source.bottom -= c->image_rect->top;
                }
                if (c->target_offset)
                {
                    source.left += c->target_offset->x;
                    source.right += c->target_offset->x;
                    source.top += c->target_offset->y;
                    source.bottom += c->target_offset->y;
                }
                if (FAILED(hr = ID2D1Factory_CreateRectangleGeometry(list->factory, &source, &rectangle))) goto done;
                hr = ID2D1RectangleGeometry_GetBounds(rectangle, &matrix, &rect);
                ID2D1RectangleGeometry_Release(rectangle);
                if (FAILED(hr)) goto done;
                break;
            }
            case D2D_COMMAND_DRAW_BITMAP:
            {
                const struct d2d_command_draw_bitmap *c = data;
                D2D1_SIZE_F size = ID2D1Bitmap_GetSize(c->bitmap);
                D2D1_RECT_F source = {0.0f, 0.0f, size.width, size.height}, destination;

                if (mode == D2D1_UNIT_MODE_PIXELS)
                {
                    D2D1_SIZE_U pixels = ID2D1Bitmap_GetPixelSize(c->bitmap);
                    source.right = pixels.width;
                    source.bottom = pixels.height;
                }
                if (c->perspective_transform) {hr = D2DERR_UNSUPPORTED_OPERATION; goto done;}
                if (c->src_rect && c->src_rect->left <= c->src_rect->right
                        && c->src_rect->top <= c->src_rect->bottom)
                {
                    source.left = max(source.left, c->src_rect->left);
                    source.top = max(source.top, c->src_rect->top);
                    source.right = min(source.right, c->src_rect->right);
                    source.bottom = min(source.bottom, c->src_rect->bottom);
                }
                if (source.left >= source.right || source.top >= source.bottom) goto next;
                if (c->dst_rect)
                {
                    destination.left = min(c->dst_rect->left, c->dst_rect->right);
                    destination.right = max(c->dst_rect->left, c->dst_rect->right);
                    destination.top = min(c->dst_rect->top, c->dst_rect->bottom);
                    destination.bottom = max(c->dst_rect->top, c->dst_rect->bottom);
                }
                else
                {
                    destination.left = destination.top = 0.0f;
                    destination.right = source.right - source.left;
                    destination.bottom = source.bottom - source.top;
                }
                if (FAILED(hr = ID2D1Factory_CreateRectangleGeometry(list->factory, &destination, &rectangle))) goto done;
                hr = ID2D1RectangleGeometry_GetBounds(rectangle, &matrix, &rect);
                ID2D1RectangleGeometry_Release(rectangle);
                if (FAILED(hr)) goto done;
                break;
            }
            case D2D_COMMAND_FILL_GEOMETRY:
            {
                const struct d2d_command_fill_geometry *c = data;
                if (FAILED(hr = ID2D1Geometry_GetBounds(c->geometry, &matrix, &rect))) goto done;
                break;
            }
            case D2D_COMMAND_DRAW_LINE:
            {
                const struct d2d_command_draw_line *c = data;
                D2D1_POINT_2F p0, p1;
                float radius = fabsf(d2d_command_list_stroke_width(c->stroke_width, c->stroke_style,
                        &transform)) * 0.707106781f;
                float x = radius * hypotf(matrix._11, matrix._21);
                float y = radius * hypotf(matrix._12, matrix._22);

                /* Include square and round caps as well as the line segment. */
                d2d_point_transform(&p0, &matrix, c->p0.x, c->p0.y);
                d2d_point_transform(&p1, &matrix, c->p1.x, c->p1.y);
                rect.left = min(p0.x, p1.x) - x;
                rect.right = max(p0.x, p1.x) + x;
                rect.top = min(p0.y, p1.y) - y;
                rect.bottom = max(p0.y, p1.y) + y;
                break;
            }
            case D2D_COMMAND_DRAW_GEOMETRY:
            {
                const struct d2d_command_draw_geometry *c = data;
                if (FAILED(hr = d2d_command_list_stroke_bounds(c->geometry,
                        d2d_command_list_stroke_width(c->stroke_width, c->stroke_style, &transform),
                        c->stroke_style, &matrix, &rect))) goto done;
                break;
            }
            case D2D_COMMAND_CLEAR:
                rect.left = rect.top = -INFINITY;
                rect.right = rect.bottom = INFINITY;
                break;
            default:
                hr = D2DERR_UNSUPPORTED_OPERATION;
                goto done;
        }
        if (clip_count)
        {
            rect.left = max(rect.left, clips[clip_count - 1].rect.left);
            rect.top = max(rect.top, clips[clip_count - 1].rect.top);
            rect.right = min(rect.right, clips[clip_count - 1].rect.right);
            rect.bottom = min(rect.bottom, clips[clip_count - 1].rect.bottom);
        }
        if (clip)
        {
            if (!d2d_array_reserve((void **)&clips, &clip_capacity, clip_count + 1, sizeof(*clips)))
            {hr = E_OUTOFMEMORY; goto done;}
            clips[clip_count].rect = rect;
            clips[clip_count++].layer = layer;
        }
        else if (rect.left < rect.right && rect.top < rect.bottom)
        {
            if (!have_bounds) *bounds = rect;
            else
            {
                bounds->left = min(bounds->left, rect.left);
                bounds->top = min(bounds->top, rect.top);
                bounds->right = max(bounds->right, rect.right);
                bounds->bottom = max(bounds->bottom, rect.bottom);
            }
            have_bounds = TRUE;
        }
next:
        data = (char *)data + command->size;
    }
    if (clip_count) hr = D2DERR_PUSH_POP_UNBALANCED;
done:
    free(clips);
    return hr;
}

HRESULT d2d_command_list_replay(ID2D1CommandList *iface, struct d2d_device_context *target)
{
    ID2D1DeviceContext *context = (ID2D1DeviceContext *)&target->ID2D1DeviceContext6_iface;
    struct d2d_command_list *command_list = impl_from_ID2D1CommandList(iface);
    ID2D1DrawingStateBlock *state;
    D2D1_MATRIX_3X2_F base_transform, recorded_transform = {._11 = 1.0f, ._22 = 1.0f};
    D2D1_UNIT_MODE base_mode, mode;
    float dpi_x, dpi_y;
    ID2D1Factory *factory;
    const void *data, *end;
    BOOL *scopes = NULL;
    size_t scope_count = 0, scope_capacity = 0;
    HRESULT hr;

    if (command_list->state != D2D_COMMAND_LIST_STATE_CLOSED) return D2DERR_WRONG_STATE;

    ID2D1DeviceContext_GetFactory(context, &factory);
    hr = ID2D1Factory_CreateDrawingStateBlock(factory, NULL, NULL, &state);
    ID2D1Factory_Release(factory);
    if (FAILED(hr)) return hr;
    ID2D1DeviceContext_SaveDrawingState(context, state);
    ID2D1DeviceContext_GetTransform(context, &base_transform);
    base_mode = mode = ID2D1DeviceContext_GetUnitMode(context);
    ID2D1DeviceContext_GetDpi(context, &dpi_x, &dpi_y);

    data = command_list->data;
    end = (char *)command_list->data + command_list->size;
    while (data < end)
    {
        const struct d2d_command *command = data;
        BOOL update_transform = FALSE;

        switch (command->op)
        {
            case D2D_COMMAND_SET_ANTIALIAS_MODE:
                ID2D1DeviceContext_SetAntialiasMode(context,
                        ((const struct d2d_command_set_antialias_mode *)data)->mode);
                break;
            case D2D_COMMAND_SET_TEXT_ANTIALIAS_MODE:
                ID2D1DeviceContext_SetTextAntialiasMode(context,
                        ((const struct d2d_command_set_text_antialias_mode *)data)->mode);
                break;
            case D2D_COMMAND_SET_TEXT_RENDERING_PARAMS:
                ID2D1DeviceContext_SetTextRenderingParams(context,
                        ((const struct d2d_command_set_text_rendering_params *)data)->params);
                break;
            case D2D_COMMAND_SET_TRANSFORM:
                recorded_transform = ((const struct d2d_command_set_transform *)data)->transform;
                update_transform = TRUE;
                break;
            case D2D_COMMAND_SET_PRIMITIVE_BLEND:
                ID2D1DeviceContext_SetPrimitiveBlend(context,
                        ((const struct d2d_command_set_primitive_blend *)data)->primitive_blend);
                break;
            case D2D_COMMAND_SET_UNIT_MODE:
                mode = ((const struct d2d_command_set_unit_mode *)data)->mode;
                ID2D1DeviceContext_SetUnitMode(context, mode);
                update_transform = TRUE;
                break;
            case D2D_COMMAND_SET_TAGS:
            {
                const struct d2d_command_set_tags *c = data;
                ID2D1DeviceContext_SetTags(context, c->tag1, c->tag2);
                break;
            }
            case D2D_COMMAND_CLEAR:
                ID2D1DeviceContext_Clear(context, &((const struct d2d_command_clear *)data)->color);
                break;
            case D2D_COMMAND_DRAW_GLYPH_RUN:
            {
                const struct d2d_command_draw_glyph_run *c = data;
                ID2D1DeviceContext_DrawGlyphRun(context, c->origin, &c->run, c->run_desc, c->brush,
                        c->measuring_mode);
                break;
            }
            case D2D_COMMAND_DRAW_LINE:
            {
                const struct d2d_command_draw_line *c = data;
                ID2D1DeviceContext_DrawLine(context, c->p0, c->p1, c->brush, c->stroke_width, c->stroke_style);
                break;
            }
            case D2D_COMMAND_DRAW_GEOMETRY:
            {
                const struct d2d_command_draw_geometry *c = data;
                ID2D1DeviceContext_DrawGeometry(context, c->geometry, c->brush, c->stroke_width, c->stroke_style);
                break;
            }
            case D2D_COMMAND_DRAW_RECTANGLE:
            {
                const struct d2d_command_draw_rectangle *c = data;
                ID2D1DeviceContext_DrawRectangle(context, &c->rect, c->brush, c->stroke_width, c->stroke_style);
                break;
            }
            case D2D_COMMAND_DRAW_BITMAP:
            {
                const struct d2d_command_draw_bitmap *c = data;
                ID2D1DeviceContext_DrawBitmap(context, c->bitmap, c->dst_rect, c->opacity,
                        c->interpolation_mode, c->src_rect, c->perspective_transform);
                break;
            }
            case D2D_COMMAND_DRAW_IMAGE:
            {
                const struct d2d_command_draw_image *c = data;
                ID2D1DeviceContext_DrawImage(context, c->image, c->target_offset, c->image_rect,
                        c->interpolation_mode, c->composite_mode);
                break;
            }
            case D2D_COMMAND_FILL_GEOMETRY:
            {
                const struct d2d_command_fill_geometry *c = data;
                ID2D1DeviceContext_FillGeometry(context, c->geometry, c->brush, c->opacity_brush);
                break;
            }
            case D2D_COMMAND_FILL_RECTANGLE:
            {
                const struct d2d_command_fill_rectangle *c = data;
                ID2D1DeviceContext_FillRectangle(context, &c->rect, c->brush);
                break;
            }
            case D2D_COMMAND_PUSH_LAYER:
            case D2D_COMMAND_PUSH_CLIP:
                if (!d2d_array_reserve((void **)&scopes, &scope_capacity, scope_count + 1, sizeof(*scopes)))
                {hr = E_OUTOFMEMORY; goto done;}
                if (command->op == D2D_COMMAND_PUSH_LAYER)
                {
                    const struct d2d_command_push_layer *c = data;
                    struct d2d_layer_frame *previous = target->layers;

                    ID2D1DeviceContext_PushLayer(context, &c->params, NULL);
                    if (target->layers != previous) scopes[scope_count++] = TRUE;
                    if (FAILED(hr = target->error.code)) goto done;
                }
                else
                {
                    const struct d2d_command_push_clip *c = data;
                    unsigned int previous = target->clip_stack.count;

                    ID2D1DeviceContext_PushAxisAlignedClip(context, &c->rect, c->mode);
                    if (target->clip_stack.count == previous) {hr = E_OUTOFMEMORY; goto done;}
                    scopes[scope_count++] = FALSE;
                }
                break;
            case D2D_COMMAND_POP_LAYER:
            case D2D_COMMAND_POP_CLIP:
                if (!scope_count || scopes[scope_count - 1] != (command->op == D2D_COMMAND_POP_LAYER))
                {hr = D2DERR_PUSH_POP_UNBALANCED; goto done;}
                if (scopes[--scope_count]) ID2D1DeviceContext_PopLayer(context);
                else ID2D1DeviceContext_PopAxisAlignedClip(context);
                break;
            default:
                FIXME("Unhandled command %u.\n", command->op);
                hr = E_NOTIMPL;
                goto done;
        }
        if (update_transform)
        {
            D2D1_MATRIX_3X2_F base = base_transform, transform = recorded_transform;
            float x = base_mode == mode ? 1.0f : base_mode == D2D1_UNIT_MODE_DIPS ? dpi_x / 96.0f : 96.0f / dpi_x;
            float y = base_mode == mode ? 1.0f : base_mode == D2D1_UNIT_MODE_DIPS ? dpi_y / 96.0f : 96.0f / dpi_y;

            base._12 *= y / x;
            base._21 *= x / y;
            base._31 *= x;
            base._32 *= y;
            d2d_matrix_multiply(&transform, &base);
            ID2D1DeviceContext_SetTransform(context, &transform);
        }
        data = (char *)data + command->size;
    }
    hr = scope_count ? D2DERR_PUSH_POP_UNBALANCED : S_OK;
done:
    while (scope_count)
    {
        if (scopes[--scope_count]) ID2D1DeviceContext_PopLayer(context);
        else ID2D1DeviceContext_PopAxisAlignedClip(context);
    }
    free(scopes);
    ID2D1DeviceContext_RestoreDrawingState(context, state);
    ID2D1DeviceContext_SetUnitMode(context, base_mode);
    ID2D1DrawingStateBlock_Release(state);
    return hr;
}

static HRESULT STDMETHODCALLTYPE d2d_command_list_Close(ID2D1CommandList *iface)
{
    struct d2d_command_list *command_list = impl_from_ID2D1CommandList(iface);

    FIXME("iface %p stub.\n", iface);

    if (command_list->state != D2D_COMMAND_LIST_STATE_OPEN)
        return D2DERR_WRONG_STATE;

    command_list->state = D2D_COMMAND_LIST_STATE_CLOSED;

    /* FIXME: reset as a target */

    return S_OK;
}

static const ID2D1CommandListVtbl d2d_command_list_vtbl =
{
    d2d_command_list_QueryInterface,
    d2d_command_list_AddRef,
    d2d_command_list_Release,
    d2d_command_list_GetFactory,
    d2d_command_list_Stream,
    d2d_command_list_Close,
};

HRESULT d2d_command_list_create(ID2D1Factory *factory, struct d2d_command_list **command_list)
{
    if (!(*command_list = calloc(1, sizeof(**command_list))))
        return E_OUTOFMEMORY;

    (*command_list)->ID2D1CommandList_iface.lpVtbl = &d2d_command_list_vtbl;
    (*command_list)->refcount = 1;
    ID2D1Factory_AddRef((*command_list)->factory = factory);

    TRACE("Created command list %p.\n", *command_list);

    return S_OK;
}

struct d2d_command_list *unsafe_impl_from_ID2D1CommandList(ID2D1CommandList *iface)
{
    if (!iface)
        return NULL;
    assert(iface->lpVtbl == (ID2D1CommandListVtbl *)&d2d_command_list_vtbl);
    return CONTAINING_RECORD(iface, struct d2d_command_list, ID2D1CommandList_iface);
}

static void d2d_command_list_rebase_field(void *field, ULONG_PTR offset)
{
    void **ptr = field;

    if (*ptr) *ptr = (void *)((ULONG_PTR)*ptr + offset);
}

static void d2d_command_list_rebase(struct d2d_command_list *command_list, ULONG_PTR offset)
{
    char *data = command_list->data, *end = data + command_list->size;

    while (data < end)
    {
        struct d2d_command *command = (struct d2d_command *)data;

        switch (command->op)
        {
            case D2D_COMMAND_DRAW_GLYPH_RUN:
            {
                struct d2d_command_draw_glyph_run *c = (struct d2d_command_draw_glyph_run *)data;

                d2d_command_list_rebase_field(&c->run.glyphIndices, offset);
                d2d_command_list_rebase_field(&c->run.glyphAdvances, offset);
                d2d_command_list_rebase_field(&c->run.glyphOffsets, offset);
                d2d_command_list_rebase_field(&c->run_desc, offset);
                if (c->run_desc)
                {
                    d2d_command_list_rebase_field(&c->run_desc->localeName, offset);
                    d2d_command_list_rebase_field(&c->run_desc->string, offset);
                    d2d_command_list_rebase_field(&c->run_desc->clusterMap, offset);
                }
                break;
            }
            case D2D_COMMAND_DRAW_BITMAP:
            {
                struct d2d_command_draw_bitmap *c = (struct d2d_command_draw_bitmap *)data;

                d2d_command_list_rebase_field(&c->dst_rect, offset);
                d2d_command_list_rebase_field(&c->src_rect, offset);
                d2d_command_list_rebase_field(&c->perspective_transform, offset);
                break;
            }
            case D2D_COMMAND_DRAW_IMAGE:
            {
                struct d2d_command_draw_image *c = (struct d2d_command_draw_image *)data;

                d2d_command_list_rebase_field(&c->target_offset, offset);
                d2d_command_list_rebase_field(&c->image_rect, offset);
                break;
            }
            case D2D_COMMAND_FILL_OPACITY_MASK:
            {
                struct d2d_command_fill_opacity_mask *c = (struct d2d_command_fill_opacity_mask *)data;

                d2d_command_list_rebase_field(&c->dst_rect, offset);
                d2d_command_list_rebase_field(&c->src_rect, offset);
                break;
            }
            default:
                break;
        }

        data += command->size;
    }
}

static void * d2d_command_list_require_space(struct d2d_command_list *command_list, size_t size)
{
    struct d2d_command *command;
    ULONG_PTR old_data = (ULONG_PTR)command_list->data;

    if (!d2d_array_reserve(&command_list->data, &command_list->capacity, command_list->size + size, 1))
    {
        command_list->state = D2D_COMMAND_LIST_STATE_ERROR;
        return NULL;
    }

    if (old_data != (ULONG_PTR)command_list->data)
        d2d_command_list_rebase(command_list, (ULONG_PTR)command_list->data - old_data);

    command = (struct d2d_command *)((char *)command_list->data + command_list->size);
    command->size = size;

    command_list->size += size;

    return command;
}

static void d2d_command_list_reference_object(struct d2d_command_list *command_list, void *object)
{
    IUnknown *obj = object;

    if (!obj) return;

    if (!d2d_array_reserve((void **)&command_list->objects, &command_list->objects_capacity,
            command_list->objects_count + 1, sizeof(*command_list->objects)))
    {
        return;
    }

    command_list->objects[command_list->objects_count++] = obj;
    IUnknown_AddRef(obj);
}

static bool d2d_command_list_create_brush(struct d2d_command_list *command_list,
        const struct d2d_device_context *ctx, ID2D1Brush *orig_brush, ID2D1Brush **ret)
{
    ID2D1DeviceContext *context = (ID2D1DeviceContext *)&ctx->ID2D1DeviceContext6_iface;
    struct d2d_brush *brush = unsafe_impl_from_ID2D1Brush(orig_brush);
    D2D1_LINEAR_GRADIENT_BRUSH_PROPERTIES linear_properties;
    D2D1_RADIAL_GRADIENT_BRUSH_PROPERTIES radial_properties;
    D2D1_BITMAP_BRUSH_PROPERTIES1 bitmap_properties;
    D2D1_IMAGE_BRUSH_PROPERTIES image_properties;
    D2D1_BRUSH_PROPERTIES properties;
    HRESULT hr;

    properties.opacity = brush->opacity;
    properties.transform = brush->transform;

    switch (brush->type)
    {
        case D2D_BRUSH_TYPE_SOLID:
            hr = ID2D1DeviceContext_CreateSolidColorBrush(context, &brush->u.solid.color,
                    &properties, (ID2D1SolidColorBrush **)ret);
            break;
        case D2D_BRUSH_TYPE_LINEAR:
            linear_properties.startPoint = brush->u.linear.start;
            linear_properties.endPoint = brush->u.linear.end;
            hr = ID2D1DeviceContext_CreateLinearGradientBrush(context, &linear_properties,
                    &properties, &brush->u.linear.gradient->ID2D1GradientStopCollection_iface,
                    (ID2D1LinearGradientBrush **)ret);
            break;
        case D2D_BRUSH_TYPE_RADIAL:
            radial_properties.center = brush->u.radial.centre;
            radial_properties.gradientOriginOffset = brush->u.radial.offset;
            radial_properties.radiusX = brush->u.radial.radius.x;
            radial_properties.radiusY = brush->u.radial.radius.y;
            hr = ID2D1DeviceContext_CreateRadialGradientBrush(context, &radial_properties,
                    &properties, &brush->u.radial.gradient->ID2D1GradientStopCollection_iface,
                    (ID2D1RadialGradientBrush **)ret);
            break;
        case D2D_BRUSH_TYPE_BITMAP:
            bitmap_properties.extendModeX = brush->u.bitmap.extend_mode_x;
            bitmap_properties.extendModeY = brush->u.bitmap.extend_mode_y;
            bitmap_properties.interpolationMode = brush->u.bitmap.interpolation_mode;
            hr = ID2D1DeviceContext_CreateBitmapBrush(context, (ID2D1Bitmap *)&brush->u.bitmap.bitmap->ID2D1Bitmap1_iface,
                    &bitmap_properties, &properties, (ID2D1BitmapBrush1 **)ret);
            break;
        case D2D_BRUSH_TYPE_IMAGE:
            image_properties.sourceRectangle = brush->u.image.source_rect;
            image_properties.extendModeX = brush->u.image.extend_mode_x;
            image_properties.extendModeY = brush->u.image.extend_mode_y;
            image_properties.interpolationMode = brush->u.image.interpolation_mode;
            hr = ID2D1DeviceContext_CreateImageBrush(context, brush->u.image.image,
                    &image_properties, &properties, (ID2D1ImageBrush **)ret);
            break;
        default:
            FIXME("Unsupported brush type %u.\n", brush->type);
            return false;
    }

    if (hr == S_OK)
    {
        d2d_command_list_reference_object(command_list, *ret);
        ID2D1Brush_Release(*ret);
    }
    else
    {
        command_list->state = D2D_COMMAND_LIST_STATE_ERROR;
        WARN("Failed to create a brush, hr %#lx.\n", hr);
    }

    return hr == S_OK;
}

void d2d_command_list_set_antialias_mode(struct d2d_command_list *command_list,
        D2D1_ANTIALIAS_MODE mode)
{
    struct d2d_command_set_antialias_mode *command;

    if ((command = d2d_command_list_require_space(command_list, sizeof(*command))))
    {
        command->c.op = D2D_COMMAND_SET_ANTIALIAS_MODE;
        command->mode = mode;
    }
}

void d2d_command_list_set_primitive_blend(struct d2d_command_list *command_list,
        D2D1_PRIMITIVE_BLEND primitive_blend)
{
    struct d2d_command_set_primitive_blend *command;

    if ((command = d2d_command_list_require_space(command_list, sizeof(*command))))
    {
        command->c.op = D2D_COMMAND_SET_PRIMITIVE_BLEND;
        command->primitive_blend = primitive_blend;
    }
}

void d2d_command_list_set_unit_mode(struct d2d_command_list *command_list, D2D1_UNIT_MODE mode)
{
    struct d2d_command_set_unit_mode *command;

    if ((command = d2d_command_list_require_space(command_list, sizeof(*command))))
    {
        command->c.op = D2D_COMMAND_SET_UNIT_MODE;
        command->mode = mode;
    }
}

void d2d_command_list_set_text_antialias_mode(struct d2d_command_list *command_list,
        D2D1_TEXT_ANTIALIAS_MODE mode)
{
    struct d2d_command_set_text_antialias_mode *command;

    if ((command = d2d_command_list_require_space(command_list, sizeof(*command))))
    {
        command->c.op = D2D_COMMAND_SET_TEXT_ANTIALIAS_MODE;
        command->mode = mode;
    }
}

void d2d_command_list_set_tags(struct d2d_command_list *command_list, D2D1_TAG tag1, D2D1_TAG tag2)
{
    struct d2d_command_set_tags *command;

    if ((command = d2d_command_list_require_space(command_list, sizeof(*command))))
    {
        command->c.op = D2D_COMMAND_SET_TAGS;
        command->tag1 = tag1;
        command->tag2 = tag2;
    }
}

void d2d_command_list_set_transform(struct d2d_command_list *command_list,
        const D2D1_MATRIX_3X2_F *transform)
{
    struct d2d_command_set_transform *command;

    if ((command = d2d_command_list_require_space(command_list, sizeof(*command))))
    {
        command->c.op = D2D_COMMAND_SET_TRANSFORM;
        command->transform = *transform;
    }
}

void d2d_command_list_begin_draw(struct d2d_command_list *command_list,
        const struct d2d_device_context *context)
{
    if (command_list->state != D2D_COMMAND_LIST_STATE_INITIAL)
    {
        command_list->state = D2D_COMMAND_LIST_STATE_ERROR;
        return;
    }

    d2d_command_list_set_antialias_mode(command_list, context->drawing_state.antialiasMode);
    d2d_command_list_set_primitive_blend(command_list, context->drawing_state.primitiveBlend);
    d2d_command_list_set_unit_mode(command_list, context->drawing_state.unitMode);
    d2d_command_list_set_text_antialias_mode(command_list, context->drawing_state.textAntialiasMode);
    d2d_command_list_set_tags(command_list, context->drawing_state.tag1, context->drawing_state.tag2);
    d2d_command_list_set_transform(command_list, &context->drawing_state.transform);
    d2d_command_list_set_text_rendering_params(command_list, context->text_rendering_params);

    command_list->state = D2D_COMMAND_LIST_STATE_OPEN;
}

void d2d_command_list_push_clip(struct d2d_command_list *command_list, const D2D1_RECT_F *rect,
        D2D1_ANTIALIAS_MODE mode)
{
    struct d2d_command_push_clip *command;

    if ((command = d2d_command_list_require_space(command_list, sizeof(*command))))
    {
        command->c.op = D2D_COMMAND_PUSH_CLIP;
        command->rect = *rect;
        command->mode = mode;
    }
}

void d2d_command_list_pop_clip(struct d2d_command_list *command_list)
{
    struct d2d_command *command;

    if ((command = d2d_command_list_require_space(command_list, sizeof(*command))))
        command->op = D2D_COMMAND_POP_CLIP;
}

void d2d_command_list_push_layer(struct d2d_command_list *command_list, const struct d2d_device_context *context,
        const D2D1_LAYER_PARAMETERS1 *params)
{
    struct d2d_command_push_layer *command;
    ID2D1Brush *opacity_brush = NULL;

    if (params->opacityBrush && !d2d_command_list_create_brush(command_list, context,
            params->opacityBrush, &opacity_brush))
    {
        return;
    }

    d2d_command_list_reference_object(command_list, params->geometricMask);

    if ((command = d2d_command_list_require_space(command_list, sizeof(*command))))
    {
        command->c.op = D2D_COMMAND_PUSH_LAYER;
        command->params = *params;
        command->params.opacityBrush = opacity_brush;
    }
}

void d2d_command_list_pop_layer(struct d2d_command_list *command_list)
{
    struct d2d_command *command;

    if ((command = d2d_command_list_require_space(command_list, sizeof(*command))))
        command->op = D2D_COMMAND_POP_LAYER;
}

void d2d_command_list_clear(struct d2d_command_list *command_list, const D2D1_COLOR_F *color)
{
    struct d2d_command_clear *command;

    if ((command = d2d_command_list_require_space(command_list, sizeof(*command))))
    {
        command->c.op = D2D_COMMAND_CLEAR;
        if (color) command->color = *color;
        else memset(&command->color, 0, sizeof(command->color));
    }
}

void d2d_command_list_draw_line(struct d2d_command_list *command_list,
        const struct d2d_device_context *context, D2D1_POINT_2F p0, D2D1_POINT_2F p1,
        ID2D1Brush *orig_brush, float stroke_width, ID2D1StrokeStyle *stroke_style)
{
    struct d2d_command_draw_line *command;
    ID2D1Brush *brush;

    if (!d2d_command_list_create_brush(command_list, context, orig_brush, &brush))
        return;

    d2d_command_list_reference_object(command_list, stroke_style);

    if ((command = d2d_command_list_require_space(command_list, sizeof(*command))))
    {
        command->c.op = D2D_COMMAND_DRAW_LINE;
        command->p0 = p0;
        command->p1 = p1;
        command->brush = brush;
        command->stroke_width = stroke_width;
        command->stroke_style = stroke_style;
    }
}

void d2d_command_list_draw_geometry(struct d2d_command_list *command_list,
        const struct d2d_device_context *context, ID2D1Geometry *geometry, ID2D1Brush *orig_brush,
        float stroke_width, ID2D1StrokeStyle *stroke_style)
{
    struct d2d_command_draw_geometry *command;
    ID2D1Brush *brush;

    if (!d2d_command_list_create_brush(command_list, context, orig_brush, &brush))
        return;

    d2d_command_list_reference_object(command_list, geometry);
    d2d_command_list_reference_object(command_list, stroke_style);

    if ((command = d2d_command_list_require_space(command_list, sizeof(*command))))
    {
        command->c.op = D2D_COMMAND_DRAW_GEOMETRY;
        command->geometry = geometry;
        command->brush = brush;
        command->stroke_width = stroke_width;
        command->stroke_style = stroke_style;
    }
}

void d2d_command_list_draw_rectangle(struct d2d_command_list *command_list, const struct d2d_device_context *context,
        const D2D1_RECT_F *rect, ID2D1Brush *orig_brush, float stroke_width, ID2D1StrokeStyle *stroke_style)
{
    struct d2d_command_draw_rectangle *command;
    ID2D1Brush *brush;

    if (!d2d_command_list_create_brush(command_list, context, orig_brush, &brush))
        return;

    d2d_command_list_reference_object(command_list, stroke_style);

    if ((command = d2d_command_list_require_space(command_list, sizeof(*command))))
    {
        command->c.op = D2D_COMMAND_DRAW_RECTANGLE;
        command->rect = *rect;
        command->brush = brush;
        command->stroke_width = stroke_width;
        command->stroke_style = stroke_style;
    }
}

void d2d_command_list_fill_geometry(struct d2d_command_list *command_list,
        const struct d2d_device_context *context, ID2D1Geometry *geometry, ID2D1Brush *orig_brush,
        ID2D1Brush *orig_opacity_brush)
{
    ID2D1Brush *brush, *opacity_brush = NULL;
    struct d2d_command_fill_geometry *command;

    if (!d2d_command_list_create_brush(command_list, context, orig_brush, &brush))
        return;

    if (orig_opacity_brush && !d2d_command_list_create_brush(command_list, context,
            orig_opacity_brush, &opacity_brush))
    {
        return;
    }

    d2d_command_list_reference_object(command_list, geometry);

    if ((command = d2d_command_list_require_space(command_list, sizeof(*command))))
    {
        command->c.op = D2D_COMMAND_FILL_GEOMETRY;
        command->geometry = geometry;
        command->brush = brush;
        command->opacity_brush = opacity_brush;
    }
}

void d2d_command_list_fill_rectangle(struct d2d_command_list *command_list,
        const struct d2d_device_context *context, const D2D1_RECT_F *rect, ID2D1Brush *orig_brush)
{
    struct d2d_command_fill_rectangle *command;
    ID2D1Brush *brush;

    if (!d2d_command_list_create_brush(command_list, context, orig_brush, &brush))
        return;

    if ((command = d2d_command_list_require_space(command_list, sizeof(*command))))
    {
        command->c.op = D2D_COMMAND_FILL_RECTANGLE;
        command->rect = *rect;
        command->brush = brush;
    }
}

static void d2d_command_list_set_text_rendering_params_internal(struct d2d_command_list *command_list,
        BOOL allow_null, IDWriteRenderingParams *params)
{
    struct d2d_command_set_text_rendering_params *command;

    if (!params && !allow_null) return;

    if ((command_list->flags & D2D_COMMAND_LIST_HAS_NULL_TEXT_RENDERING_PARAMS)
            && !params)
    {
        return;
    }

    d2d_command_list_reference_object(command_list, params);

    if (params)
        command_list->flags &= ~D2D_COMMAND_LIST_HAS_NULL_TEXT_RENDERING_PARAMS;
    else
        command_list->flags |= D2D_COMMAND_LIST_HAS_NULL_TEXT_RENDERING_PARAMS;

    if ((command = d2d_command_list_require_space(command_list, sizeof(*command))))
    {
        command->c.op = D2D_COMMAND_SET_TEXT_RENDERING_PARAMS;
        command->params = params;
    }
}

void d2d_command_list_set_text_rendering_params(struct d2d_command_list *command_list,
        IDWriteRenderingParams *params)
{
    d2d_command_list_set_text_rendering_params_internal(command_list, FALSE, params);
}

static inline void d2d_command_list_write_field(BYTE **data, void *dst, const void *src, size_t size)
{
    void **ptr = dst;

    if (!src)
    {
        *ptr = NULL;
        return;
    }

    *ptr = *data;
    memcpy(*data, src, size);
    *data = *data + size;
}

void d2d_command_list_draw_glyph_run(struct d2d_command_list *command_list,
        const struct d2d_device_context *context, D2D1_POINT_2F origin, const DWRITE_GLYPH_RUN *run,
        const DWRITE_GLYPH_RUN_DESCRIPTION *run_desc, ID2D1Brush *orig_brush,
        DWRITE_MEASURING_MODE measuring_mode)
{
    struct d2d_command_draw_glyph_run *command;
    size_t size, locale_name_size = 0;
    DWRITE_GLYPH_RUN_DESCRIPTION *d;
    DWRITE_GLYPH_RUN *r;
    UINT32 glyph_count;
    ID2D1Brush *brush;
    BYTE *data;

    if (!d2d_command_list_create_brush(command_list, context, orig_brush, &brush))
        return;

    /* Set rendering parameters automatically. Explicitly set null parameters are not recorded,
       either separately or as a part of a restored state block. Forcing parameters update on
       DrawGlyphRun() ensures that state is reset correctly. */

    d2d_command_list_set_text_rendering_params_internal(command_list, TRUE,
            context->text_rendering_params);

    /* Get combined size of variable data. */

    glyph_count = run->glyphCount;
    size = sizeof(*command);
    if (run->glyphIndices) size += glyph_count * sizeof(*run->glyphIndices);
    if (run->glyphAdvances) size += glyph_count * sizeof(*run->glyphAdvances);
    if (run->glyphOffsets) size += glyph_count * sizeof(*run->glyphOffsets);
    if (run_desc)
    {
        size += sizeof(*run_desc);
        if (run_desc->localeName)
        {
            locale_name_size = (wcslen(run_desc->localeName) + 1) * sizeof(*run_desc->localeName);
            size += locale_name_size;
        }
        if (run_desc->string) size += run_desc->stringLength * sizeof(*run_desc->string);
        if (run_desc->clusterMap) size += run_desc->stringLength * sizeof(*run_desc->clusterMap);
        size += sizeof(run_desc->stringLength);
        size += sizeof(run_desc->textPosition);
    }

    d2d_command_list_reference_object(command_list, run->fontFace);

    if (!(command = d2d_command_list_require_space(command_list, size)))
        return;
    command->c.op = D2D_COMMAND_DRAW_GLYPH_RUN;
    r = &command->run;

    r->fontFace = run->fontFace;
    r->fontEmSize = run->fontEmSize;
    r->glyphCount = run->glyphCount;
    r->isSideways = run->isSideways;
    r->bidiLevel = run->bidiLevel;

    data = (BYTE *)(command + 1);

    d2d_command_list_write_field(&data, &r->glyphIndices, run->glyphIndices, glyph_count * sizeof(*r->glyphIndices));
    d2d_command_list_write_field(&data, &r->glyphAdvances, run->glyphAdvances, glyph_count * sizeof(*r->glyphAdvances));
    d2d_command_list_write_field(&data, &r->glyphOffsets, run->glyphOffsets, glyph_count * sizeof(*r->glyphOffsets));

    command->run_desc = NULL;
    if (run_desc)
    {
        d = command->run_desc = (DWRITE_GLYPH_RUN_DESCRIPTION *)data;
        memset(d, 0, sizeof(*d));
        data += sizeof(*d);

        d2d_command_list_write_field(&data, &d->localeName, run_desc->localeName, locale_name_size);
        d2d_command_list_write_field(&data, &d->string, run_desc->string, run_desc->stringLength * sizeof(*run_desc->string));
        d->stringLength = run_desc->stringLength;
        d2d_command_list_write_field(&data, &d->clusterMap, run_desc->clusterMap, run_desc->stringLength * sizeof(*run_desc->clusterMap));
        d->textPosition = run_desc->textPosition;
    }

    command->brush = brush;
    command->origin = origin;
    command->measuring_mode = measuring_mode;
}

void d2d_command_list_draw_bitmap(struct d2d_command_list *command_list, ID2D1Bitmap *bitmap,
        const D2D1_RECT_F *dst_rect, float opacity, D2D1_INTERPOLATION_MODE interpolation_mode,
        const D2D1_RECT_F *src_rect, const D2D1_MATRIX_4X4_F *perspective_transform)
{
    struct d2d_command_draw_bitmap *command;
    size_t size;
    BYTE *data;

    size = sizeof(*command);
    if (dst_rect) size += sizeof(*dst_rect);
    if (src_rect) size += sizeof(*src_rect);
    if (perspective_transform) size += sizeof(*perspective_transform);

    d2d_command_list_reference_object(command_list, bitmap);

    if ((command = d2d_command_list_require_space(command_list, size)))
    {
        command->c.op = D2D_COMMAND_DRAW_BITMAP;
        command->bitmap = bitmap;
        command->opacity = opacity;
        command->interpolation_mode = interpolation_mode;

        data = (BYTE *)(command + 1);

        d2d_command_list_write_field(&data, &command->dst_rect, dst_rect, sizeof(*dst_rect));
        d2d_command_list_write_field(&data, &command->src_rect, src_rect, sizeof(*src_rect));
        d2d_command_list_write_field(&data, &command->perspective_transform, perspective_transform,
                sizeof(*perspective_transform));
    }
}

void d2d_command_list_draw_image(struct d2d_command_list *command_list, ID2D1Image *image,
        const D2D1_POINT_2F *target_offset, const D2D1_RECT_F *image_rect, D2D1_INTERPOLATION_MODE interpolation_mode,
        D2D1_COMPOSITE_MODE composite_mode)
{
    struct d2d_command_draw_image *command;
    size_t size;
    BYTE *data;

    size = sizeof(*command);
    if (target_offset) size += sizeof(*target_offset);
    if (image_rect) size += sizeof(*image_rect);

    d2d_command_list_reference_object(command_list, image);

    if ((command = d2d_command_list_require_space(command_list, size)))
    {
        command->c.op = D2D_COMMAND_DRAW_IMAGE;
        command->image = image;
        command->interpolation_mode = interpolation_mode;
        command->composite_mode = composite_mode;

        data = (BYTE *)(command + 1);

        d2d_command_list_write_field(&data, &command->target_offset, target_offset, sizeof(*target_offset));
        d2d_command_list_write_field(&data, &command->image_rect, image_rect, sizeof(*image_rect));
    }
}

void d2d_command_list_draw_sprite_batch(struct d2d_command_list *command_list, ID2D1SpriteBatch *sprite_batch,
    UINT32 start_index, UINT32 sprite_count, ID2D1Bitmap *bitmap, D2D1_BITMAP_INTERPOLATION_MODE interpolation_mode,
    D2D1_SPRITE_OPTIONS sprite_options)
{
    struct d2d_command_draw_sprite_batch *command;
    size_t size;

    size = sizeof(*command);

    d2d_command_list_reference_object(command_list, sprite_batch);
    d2d_command_list_reference_object(command_list, bitmap);

    if ((command = d2d_command_list_require_space(command_list, size)))
    {
        command->c.op = D2D_COMMAND_DRAW_SPRITE_BATCH;
        command->sprite_batch = sprite_batch;
        command->bitmap = bitmap;
        command->start_index = start_index;
        command->sprite_count = sprite_count;
        command->interpolation_mode = interpolation_mode;
        command->sprite_options = sprite_options;
    }
}

void d2d_command_list_fill_mesh(struct d2d_command_list *command_list, const struct d2d_device_context *context,
        ID2D1Mesh *mesh, ID2D1Brush *orig_brush)
{
    struct d2d_command_fill_mesh *command;
    ID2D1Brush *brush;

    if (!d2d_command_list_create_brush(command_list, context, orig_brush, &brush))
        return;

    d2d_command_list_reference_object(command_list, mesh);

    if ((command = d2d_command_list_require_space(command_list, sizeof(*command))))
    {
        command->c.op = D2D_COMMAND_FILL_MESH;
        command->mesh = mesh;
        command->brush = brush;
    }
}

void d2d_command_list_fill_opacity_mask(struct d2d_command_list *command_list, const struct d2d_device_context *context,
        ID2D1Bitmap *bitmap, ID2D1Brush *orig_brush, const D2D1_RECT_F *dst_rect, const D2D1_RECT_F *src_rect)
{
    struct d2d_command_fill_opacity_mask *command;
    ID2D1Brush *brush;
    size_t size;
    BYTE *data;

    if (!d2d_command_list_create_brush(command_list, context, orig_brush, &brush))
        return;

    size = sizeof(*command);
    if (dst_rect) size += sizeof(*dst_rect);
    if (src_rect) size += sizeof(*src_rect);

    d2d_command_list_reference_object(command_list, bitmap);

    if ((command = d2d_command_list_require_space(command_list, size)))
    {
        command->c.op = D2D_COMMAND_FILL_OPACITY_MASK;
        command->bitmap = bitmap;
        command->brush = brush;

        data = (BYTE *)(command + 1);

        d2d_command_list_write_field(&data, &command->dst_rect, dst_rect, sizeof(*dst_rect));
        d2d_command_list_write_field(&data, &command->src_rect, src_rect, sizeof(*src_rect));
    }
}
