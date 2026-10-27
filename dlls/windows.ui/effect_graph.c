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

#include "effect_graph.h"
#include "d2d1effects.h"

const GUID composition_effect_interop_iid =
        {0x2fc57384, 0xa068, 0x44d7, {0xa3, 0x31, 0x30, 0x98, 0x2f, 0xcf, 0x71, 0x77}};

struct effect_graph_frame
{
    IUnknown *identity;
    const struct effect_graph_frame *parent;
};

struct effect_property_request
{
    HSTRING name;
    BOOL matched;
};

static void composition_effect_node_free( struct composition_effect_node *node )
{
    UINT i;

    if (!node) return;
    composition_effect_node_free( node->sources[0] );
    composition_effect_node_free( node->sources[1] );
    WindowsDeleteString( node->source_name );
    WindowsDeleteString( node->name );
    for (i = 0; i < node->property_count; ++i) WindowsDeleteString( node->properties[i].name );
    free( node->properties );
    free( node );
}

static HRESULT composition_effect_node_properties( struct composition_effect_node *node,
        struct graphics_effect_interop *metadata, struct effect_property_request *requests, UINT count )
{
    struct composition_effect_property *property;
    const WCHAR *name, *key;
    UINT length, key_length, index, mapping, i;
    HRESULT hr;

    name = WindowsGetStringRawBuffer( node->name, &length );
    if (!length || !count) return S_OK;
    if (!(node->properties = calloc( count, sizeof(*node->properties) ))) return E_OUTOFMEMORY;
    for (i = 0; i < count; ++i)
    {
        key = WindowsGetStringRawBuffer( requests[i].name, &key_length );
        if (key_length <= length + 1 || memcmp( key, name, length * sizeof(*key) ) || key[length] != '.') continue;
        if (FAILED(hr = metadata->lpVtbl->GetNamedPropertyMapping( metadata, key + length + 1,
                &index, &mapping ))) return hr;
        if (index || mapping < EFFECT_PROPERTY_DIRECT || mapping > EFFECT_PROPERTY_VECTOR_W)
            return D2DERR_UNSUPPORTED_OPERATION;
        property = &node->properties[node->property_count];
        if (FAILED(hr = WindowsDuplicateString( requests[i].name, &property->name ))) return hr;
        property->mapping = mapping;
        ++node->property_count;
        requests[i].matched = TRUE;
    }
    return S_OK;
}

static HRESULT composition_effect_node_compile( IGraphicsEffectSource *source,
        const struct effect_graph_frame *parent, struct effect_property_request *requests, UINT request_count,
        struct composition_effect_node **result )
{
    ICompositionEffectSourceParameter *parameter = NULL;
    struct graphics_effect_interop *metadata = NULL;
    struct composition_effect_node *node = NULL;
    const struct effect_graph_frame *ancestor;
    struct effect_graph_frame frame = {NULL, parent};
    IGraphicsEffect *named = NULL;
    IGraphicsEffectSource *input = NULL;
    IPropertyValue *value = NULL;
    FLOAT *coefficients = NULL;
    enum PropertyType type;
    boolean clamp;
    UINT count, i;
    GUID effect_id;
    HRESULT hr;

    *result = NULL;
    if (!source) return E_INVALIDARG;
    if (FAILED(hr = IGraphicsEffectSource_QueryInterface( source, &IID_IUnknown,
            (void **)&frame.identity ))) return hr;
    if (!frame.identity) return E_INVALIDARG;
    for (ancestor = parent; ancestor; ancestor = ancestor->parent)
        if (ancestor->identity == frame.identity)
        {
            IUnknown_Release( frame.identity );
            return D2DERR_CYCLIC_GRAPH;
        }
    if (!(node = calloc( 1, sizeof(*node) ))) {hr = E_OUTOFMEMORY; goto done;}
    hr = IGraphicsEffectSource_QueryInterface( source, &IID_ICompositionEffectSourceParameter,
            (void **)&parameter );
    if (SUCCEEDED(hr))
    {
        if (!parameter) {hr = E_INVALIDARG; goto done;}
        node->parameter = TRUE;
        hr = ICompositionEffectSourceParameter_get_Name( parameter, &node->source_name );
        goto done;
    }
    if (hr != E_NOINTERFACE) goto done;
    if (FAILED(hr = IGraphicsEffectSource_QueryInterface( source, &composition_effect_interop_iid,
            (void **)&metadata ))) goto done;
    if (!metadata) {hr = E_INVALIDARG; goto done;}
    if (FAILED(hr = metadata->lpVtbl->GetEffectId( metadata, &effect_id ))) goto done;
    if (!IsEqualGUID( &effect_id, &CLSID_D2D1ArithmeticComposite ))
    {
        hr = D2DERR_UNSUPPORTED_OPERATION;
        goto done;
    }
    hr = IGraphicsEffectSource_QueryInterface( source, &IID_IGraphicsEffect, (void **)&named );
    if (SUCCEEDED(hr))
    {
        if (!named) {hr = E_INVALIDARG; goto done;}
        if (FAILED(hr = IGraphicsEffect_get_Name( named, &node->name ))) goto done;
    }
    else if (hr != E_NOINTERFACE) goto done;
    if (FAILED(hr = composition_effect_node_properties( node, metadata, requests, request_count ))) goto done;
    if (FAILED(hr = metadata->lpVtbl->GetPropertyCount( metadata, &count ))) goto done;
    if (count < 2) {hr = E_INVALIDARG; goto done;}
    if (FAILED(hr = metadata->lpVtbl->GetProperty( metadata, 0, &value ))) goto done;
    if (!value) {hr = E_INVALIDARG; goto done;}
    if (FAILED(hr = IPropertyValue_get_Type( value, &type ))) goto done;
    if (type != PropertyType_SingleArray) {hr = E_INVALIDARG; goto done;}
    if (FAILED(hr = IPropertyValue_GetSingleArray( value, &count, &coefficients ))) goto done;
    if (count != 4 || !coefficients) {hr = E_INVALIDARG; goto done;}
    memcpy( &node->coefficients, coefficients, sizeof(node->coefficients) );
    CoTaskMemFree( coefficients );
    coefficients = NULL;
    IPropertyValue_Release( value );
    value = NULL;
    if (FAILED(hr = metadata->lpVtbl->GetProperty( metadata, 1, &value ))) goto done;
    if (!value) {hr = E_INVALIDARG; goto done;}
    if (FAILED(hr = IPropertyValue_get_Type( value, &type ))) goto done;
    if (type != PropertyType_Boolean) {hr = E_INVALIDARG; goto done;}
    if (FAILED(hr = IPropertyValue_GetBoolean( value, &clamp ))) goto done;
    node->clamp_output = clamp;
    IPropertyValue_Release( value );
    value = NULL;
    if (FAILED(hr = metadata->lpVtbl->GetSourceCount( metadata, &count ))) goto done;
    if (count != 2) {hr = E_INVALIDARG; goto done;}
    for (i = 0; i < count; ++i)
    {
        if (FAILED(hr = metadata->lpVtbl->GetSource( metadata, i, &input ))) goto done;
        if (!input) {hr = E_INVALIDARG; goto done;}
        hr = composition_effect_node_compile( input, &frame, requests, request_count, &node->sources[i] );
        IGraphicsEffectSource_Release( input );
        input = NULL;
        if (FAILED(hr)) goto done;
    }

done:
    if (coefficients) CoTaskMemFree( coefficients );
    if (value) IPropertyValue_Release( value );
    if (input) IGraphicsEffectSource_Release( input );
    if (metadata) metadata->lpVtbl->Release( metadata );
    if (named) IGraphicsEffect_Release( named );
    if (parameter) ICompositionEffectSourceParameter_Release( parameter );
    IUnknown_Release( frame.identity );
    if (FAILED(hr)) composition_effect_node_free( node );
    else *result = node;
    return hr;
}

HRESULT composition_effect_graph_compile_properties( IGraphicsEffect *effect, __FIIterable_1_HSTRING *properties,
        struct composition_effect_graph **result )
{
    struct composition_effect_graph *graph = NULL;
    struct effect_property_request *requests = NULL, *expanded;
    __FIIterator_1_HSTRING *iterator = NULL;
    IGraphicsEffectSource *source = NULL;
    HSTRING name = NULL;
    boolean current;
    UINT count = 0, i;
    HRESULT hr;

    if (!result) return E_POINTER;
    *result = NULL;
    if (!effect) return E_INVALIDARG;
    if (properties)
    {
        if (FAILED(hr = __FIIterable_1_HSTRING_First( properties, &iterator ))) goto done;
        if (!iterator) {hr = E_INVALIDARG; goto done;}
        if (FAILED(hr = __FIIterator_1_HSTRING_get_HasCurrent( iterator, &current ))) goto done;
        while (current)
        {
            if (FAILED(hr = __FIIterator_1_HSTRING_get_Current( iterator, &name ))) goto done;
            if (!WindowsGetStringLen( name )) {hr = E_INVALIDARG; goto done;}
            if (!(expanded = realloc( requests, (count + 1) * sizeof(*requests) )))
            {hr = E_OUTOFMEMORY; goto done;}
            requests = expanded;
            requests[count++] = (struct effect_property_request){name, FALSE};
            name = NULL;
            if (FAILED(hr = __FIIterator_1_HSTRING_MoveNext( iterator, &current ))) goto done;
        }
    }
    if (FAILED(hr = IGraphicsEffect_QueryInterface( effect, &IID_IGraphicsEffectSource,
            (void **)&source ))) goto done;
    if (!(graph = calloc( 1, sizeof(*graph) )))
    {
        hr = E_OUTOFMEMORY;
        goto done;
    }
    hr = composition_effect_node_compile( source, NULL, requests, count, &graph->root );
    if (SUCCEEDED(hr) && graph->root->parameter) hr = E_INVALIDARG;
    for (i = 0; SUCCEEDED(hr) && i < count; ++i) if (!requests[i].matched) hr = E_INVALIDARG;
    if (SUCCEEDED(hr))
    {
        graph->ref = 1;
        *result = graph;
        graph = NULL;
    }
done:
    if (graph)
    {
        composition_effect_node_free( graph->root );
        free( graph );
    }
    if (source) IGraphicsEffectSource_Release( source );
    if (iterator) __FIIterator_1_HSTRING_Release( iterator );
    WindowsDeleteString( name );
    for (i = 0; i < count; ++i) WindowsDeleteString( requests[i].name );
    free( requests );
    return hr;
}

HRESULT composition_effect_graph_compile( IGraphicsEffect *effect, struct composition_effect_graph **result )
{
    return composition_effect_graph_compile_properties( effect, NULL, result );
}

ULONG composition_effect_graph_add_ref( struct composition_effect_graph *graph )
{
    return InterlockedIncrement( &graph->ref );
}

ULONG composition_effect_graph_release( struct composition_effect_graph *graph )
{
    ULONG ref = InterlockedDecrement( &graph->ref );

    if (!ref)
    {
        composition_effect_node_free( graph->root );
        free( graph );
    }
    return ref;
}

static HRESULT composition_effect_node_create_image( const struct composition_effect_node *node,
        ID2D1DeviceContext *context, ICompositionPropertySet *properties,
        composition_effect_source_lookup lookup, void *data, ID2D1Image **image )
{
    ID2D1Effect *effect;
    ID2D1Image *input = NULL;
    HRESULT hr;
    UINT i;
    D2D1_VECTOR_4F coefficients = node->coefficients;
    struct Vector4 vector;
    FLOAT scalar;
    enum CompositionGetValueStatus status;
    const struct composition_effect_property *property;

    *image = NULL;
    if (node->parameter)
    {
        hr = lookup( data, node->source_name, image );
        if (SUCCEEDED(hr) && !*image) hr = E_INVALIDARG;
        return hr;
    }
    for (i = 0; properties && i < node->property_count; ++i)
    {
        property = &node->properties[i];
        if (property->mapping == EFFECT_PROPERTY_DIRECT)
        {
            hr = ICompositionPropertySet_TryGetVector4( properties, property->name, &vector, &status );
            if (SUCCEEDED(hr) && status == CompositionGetValueStatus_Succeeded)
                coefficients = (D2D1_VECTOR_4F){vector.X, vector.Y, vector.Z, vector.W};
        }
        else
        {
            hr = ICompositionPropertySet_TryGetScalar( properties, property->name, &scalar, &status );
            if (SUCCEEDED(hr) && status == CompositionGetValueStatus_Succeeded)
                ((FLOAT *)&coefficients)[property->mapping - EFFECT_PROPERTY_VECTOR_X] = scalar;
        }
        if (FAILED(hr)) return hr;
        if (status != CompositionGetValueStatus_Succeeded) return E_INVALIDARG;
    }
    if (FAILED(hr = ID2D1DeviceContext_CreateEffect( context, &CLSID_D2D1ArithmeticComposite, &effect ))) return hr;
    if (FAILED(hr = ID2D1Effect_SetValue( effect, D2D1_ARITHMETICCOMPOSITE_PROP_COEFFICIENTS,
            D2D1_PROPERTY_TYPE_VECTOR4, (const BYTE *)&coefficients, sizeof(coefficients) ))) goto done;
    if (FAILED(hr = ID2D1Effect_SetValue( effect, D2D1_ARITHMETICCOMPOSITE_PROP_CLAMP_OUTPUT,
            D2D1_PROPERTY_TYPE_BOOL, (const BYTE *)&node->clamp_output, sizeof(node->clamp_output) ))) goto done;
    for (i = 0; i < 2; ++i)
    {
        if (FAILED(hr = composition_effect_node_create_image( node->sources[i], context,
                properties, lookup, data, &input ))) goto done;
        ID2D1Effect_SetInput( effect, i, input, TRUE );
        ID2D1Image_Release( input );
        input = NULL;
    }
    ID2D1Effect_GetOutput( effect, image );
    if (!*image) hr = E_UNEXPECTED;
done:
    if (input) ID2D1Image_Release( input );
    ID2D1Effect_Release( effect );
    return hr;
}

HRESULT composition_effect_graph_create_image_properties( struct composition_effect_graph *graph,
        ID2D1DeviceContext *context, ICompositionPropertySet *properties,
        composition_effect_source_lookup lookup, void *data, ID2D1Image **image )
{
    if (!image) return E_POINTER;
    *image = NULL;
    if (!graph || !context || !lookup) return E_INVALIDARG;
    return composition_effect_node_create_image( graph->root, context, properties, lookup, data, image );
}

HRESULT composition_effect_graph_create_image( struct composition_effect_graph *graph, ID2D1DeviceContext *context,
        composition_effect_source_lookup lookup, void *data, ID2D1Image **image )
{
    return composition_effect_graph_create_image_properties( graph, context, NULL, lookup, data, image );
}

static HRESULT composition_effect_node_init_properties( const struct composition_effect_node *node,
        ICompositionPropertySet *properties )
{
    const struct composition_effect_property *property;
    struct Vector4 vector;
    HRESULT hr;
    UINT i;

    if (node->parameter) return S_OK;
    vector = (struct Vector4){node->coefficients.x, node->coefficients.y,
            node->coefficients.z, node->coefficients.w};
    for (i = 0; i < node->property_count; ++i)
    {
        property = &node->properties[i];
        if (property->mapping == EFFECT_PROPERTY_DIRECT)
            hr = ICompositionPropertySet_InsertVector4( properties, property->name, vector );
        else
            hr = ICompositionPropertySet_InsertScalar( properties, property->name,
                    ((const FLOAT *)&node->coefficients)[property->mapping - EFFECT_PROPERTY_VECTOR_X] );
        if (FAILED(hr)) return hr;
    }
    if (FAILED(hr = composition_effect_node_init_properties( node->sources[0], properties ))) return hr;
    return composition_effect_node_init_properties( node->sources[1], properties );
}

HRESULT composition_effect_graph_init_properties( struct composition_effect_graph *graph,
        ICompositionPropertySet *properties )
{
    return composition_effect_node_init_properties( graph->root, properties );
}
