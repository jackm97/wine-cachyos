/* WinRT Windows.Data.Text.WordsSegmenter implementation
 *
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

#include "private.h"
#include "winnls.h"

WINE_DEFAULT_DEBUG_CHANNEL(locale);

static const struct vector_iids word_segment_iids =
{
    .vector = &IID_IVector_WordSegment,
    .view = &IID_IVectorView_WordSegment,
    .iterable = &IID_IIterable_WordSegment,
    .iterator = &IID_IIterator_WordSegment,
};

static const struct vector_iids alternate_word_form_iids =
{
    .vector = &IID_IVector_AlternateWordForm,
    .view = &IID_IVectorView_AlternateWordForm,
    .iterable = &IID_IIterable_AlternateWordForm,
    .iterator = &IID_IIterator_AlternateWordForm,
};

struct token
{
    UINT32 start;
    UINT32 length;
};

static BOOL is_letter_or_digit( WCHAR ch )
{
    WORD type;

    if (IS_HIGH_SURROGATE( ch ) || IS_LOW_SURROGATE( ch )) return TRUE;
    if (!GetStringTypeW( CT_CTYPE1, &ch, 1, &type )) return FALSE;
    return !!(type & (C1_ALPHA | C1_DIGIT));
}

/* Combining marks belong to the word they follow, and an apostrophe between
 * two letters keeps a contraction such as "don't" in a single word. */
static BOOL is_word_char( const WCHAR *text, UINT32 length, UINT32 i )
{
    WORD type;

    if (is_letter_or_digit( text[i] )) return TRUE;
    if (GetStringTypeW( CT_CTYPE3, text + i, 1, &type ) && (type & C3_NONSPACING)) return i > 0;
    if (text[i] != '\'' && text[i] != 0x2019) return FALSE;
    return i > 0 && i + 1 < length && is_letter_or_digit( text[i - 1] ) && is_letter_or_digit( text[i + 1] );
}

static HRESULT split_words( const WCHAR *text, UINT32 length, struct token **tokens, UINT32 *count )
{
    struct token *list = NULL, *tmp;
    UINT32 i = 0, capacity = 0;

    *count = 0;
    while (i < length)
    {
        UINT32 start;

        while (i < length && !is_word_char( text, length, i )) i++;
        if (i == length) break;
        for (start = i; i < length && is_word_char( text, length, i ); i++);

        if (*count == capacity)
        {
            capacity = capacity ? capacity * 2 : 8;
            if (!(tmp = realloc( list, capacity * sizeof(*list) )))
            {
                free( list );
                return E_OUTOFMEMORY;
            }
            list = tmp;
        }
        list[*count].start = start;
        list[*count].length = i - start;
        (*count)++;
    }

    *tokens = list;
    return S_OK;
}

struct word_segment
{
    IWordSegment IWordSegment_iface;
    LONG ref;

    HSTRING text;
    TextSegment segment;
};

static inline struct word_segment *impl_from_IWordSegment( IWordSegment *iface )
{
    return CONTAINING_RECORD( iface, struct word_segment, IWordSegment_iface );
}

static HRESULT WINAPI word_segment_QueryInterface( IWordSegment *iface, REFIID iid, void **out )
{
    struct word_segment *impl = impl_from_IWordSegment( iface );

    TRACE( "iface %p, iid %s, out %p.\n", iface, debugstr_guid( iid ), out );

    if (IsEqualGUID( iid, &IID_IUnknown ) ||
        IsEqualGUID( iid, &IID_IInspectable ) ||
        IsEqualGUID( iid, &IID_IAgileObject ) ||
        IsEqualGUID( iid, &IID_IWordSegment ))
    {
        IInspectable_AddRef( (*out = &impl->IWordSegment_iface) );
        return S_OK;
    }

    FIXME( "%s not implemented, returning E_NOINTERFACE.\n", debugstr_guid( iid ) );
    *out = NULL;
    return E_NOINTERFACE;
}

static ULONG WINAPI word_segment_AddRef( IWordSegment *iface )
{
    struct word_segment *impl = impl_from_IWordSegment( iface );
    ULONG ref = InterlockedIncrement( &impl->ref );
    TRACE( "iface %p, ref %lu.\n", iface, ref );
    return ref;
}

static ULONG WINAPI word_segment_Release( IWordSegment *iface )
{
    struct word_segment *impl = impl_from_IWordSegment( iface );
    ULONG ref = InterlockedDecrement( &impl->ref );

    TRACE( "iface %p, ref %lu.\n", iface, ref );

    if (!ref)
    {
        WindowsDeleteString( impl->text );
        free( impl );
    }
    return ref;
}

static HRESULT WINAPI word_segment_GetIids( IWordSegment *iface, ULONG *iid_count, IID **iids )
{
    FIXME( "iface %p, iid_count %p, iids %p stub!\n", iface, iid_count, iids );
    return E_NOTIMPL;
}

static HRESULT WINAPI word_segment_GetRuntimeClassName( IWordSegment *iface, HSTRING *class_name )
{
    TRACE( "iface %p, class_name %p.\n", iface, class_name );
    return WindowsCreateString( RuntimeClass_Windows_Data_Text_WordSegment,
                                wcslen( RuntimeClass_Windows_Data_Text_WordSegment ), class_name );
}

static HRESULT WINAPI word_segment_GetTrustLevel( IWordSegment *iface, TrustLevel *trust_level )
{
    FIXME( "iface %p, trust_level %p stub!\n", iface, trust_level );
    return E_NOTIMPL;
}

static HRESULT WINAPI word_segment_get_Text( IWordSegment *iface, HSTRING *value )
{
    struct word_segment *impl = impl_from_IWordSegment( iface );

    TRACE( "iface %p, value %p.\n", iface, value );

    return WindowsDuplicateString( impl->text, value );
}

static HRESULT WINAPI word_segment_get_SourceTextSegment( IWordSegment *iface, TextSegment *value )
{
    struct word_segment *impl = impl_from_IWordSegment( iface );

    TRACE( "iface %p, value %p.\n", iface, value );

    *value = impl->segment;
    return S_OK;
}

static HRESULT WINAPI word_segment_get_AlternateForms( IWordSegment *iface, IVectorView_AlternateWordForm **value )
{
    IVector_IInspectable *vector;
    HRESULT hr;

    TRACE( "iface %p, value %p.\n", iface, value );

    if (FAILED(hr = vector_create( &alternate_word_form_iids, (void **)&vector ))) return hr;
    hr = IVector_IInspectable_GetView( vector, (IVectorView_IInspectable **)value );
    IVector_IInspectable_Release( vector );
    return hr;
}

static const struct IWordSegmentVtbl word_segment_vtbl =
{
    word_segment_QueryInterface,
    word_segment_AddRef,
    word_segment_Release,
    /* IInspectable methods */
    word_segment_GetIids,
    word_segment_GetRuntimeClassName,
    word_segment_GetTrustLevel,
    /* IWordSegment methods */
    word_segment_get_Text,
    word_segment_get_SourceTextSegment,
    word_segment_get_AlternateForms,
};

static HRESULT word_segment_create( const WCHAR *text, const struct token *token, IWordSegment **out )
{
    struct word_segment *impl;
    HRESULT hr;

    if (!(impl = calloc( 1, sizeof(*impl) ))) return E_OUTOFMEMORY;
    impl->IWordSegment_iface.lpVtbl = &word_segment_vtbl;
    impl->ref = 1;
    impl->segment.StartPosition = token->start;
    impl->segment.Length = token->length;

    if (FAILED(hr = WindowsCreateString( text + token->start, token->length, &impl->text )))
    {
        free( impl );
        return hr;
    }

    *out = &impl->IWordSegment_iface;
    return S_OK;
}

static HRESULT segment_vector_create( const WCHAR *text, const struct token *tokens, UINT32 count,
                                      IVectorView_IInspectable **out )
{
    IVector_IInspectable *vector;
    IWordSegment *segment;
    HRESULT hr;
    UINT32 i;

    if (FAILED(hr = vector_create( &word_segment_iids, (void **)&vector ))) return hr;

    for (i = 0; SUCCEEDED(hr) && i < count; i++)
    {
        if (FAILED(hr = word_segment_create( text, tokens + i, &segment ))) break;
        hr = IVector_IInspectable_Append( vector, (IInspectable *)segment );
        IWordSegment_Release( segment );
    }

    if (SUCCEEDED(hr)) hr = IVector_IInspectable_GetView( vector, out );
    IVector_IInspectable_Release( vector );
    return hr;
}

struct words_segmenter
{
    IWordsSegmenter IWordsSegmenter_iface;
    LONG ref;

    HSTRING language;
};

static inline struct words_segmenter *impl_from_IWordsSegmenter( IWordsSegmenter *iface )
{
    return CONTAINING_RECORD( iface, struct words_segmenter, IWordsSegmenter_iface );
}

static HRESULT WINAPI words_segmenter_QueryInterface( IWordsSegmenter *iface, REFIID iid, void **out )
{
    struct words_segmenter *impl = impl_from_IWordsSegmenter( iface );

    TRACE( "iface %p, iid %s, out %p.\n", iface, debugstr_guid( iid ), out );

    if (IsEqualGUID( iid, &IID_IUnknown ) ||
        IsEqualGUID( iid, &IID_IInspectable ) ||
        IsEqualGUID( iid, &IID_IAgileObject ) ||
        IsEqualGUID( iid, &IID_IWordsSegmenter ))
    {
        IInspectable_AddRef( (*out = &impl->IWordsSegmenter_iface) );
        return S_OK;
    }

    FIXME( "%s not implemented, returning E_NOINTERFACE.\n", debugstr_guid( iid ) );
    *out = NULL;
    return E_NOINTERFACE;
}

static ULONG WINAPI words_segmenter_AddRef( IWordsSegmenter *iface )
{
    struct words_segmenter *impl = impl_from_IWordsSegmenter( iface );
    ULONG ref = InterlockedIncrement( &impl->ref );
    TRACE( "iface %p, ref %lu.\n", iface, ref );
    return ref;
}

static ULONG WINAPI words_segmenter_Release( IWordsSegmenter *iface )
{
    struct words_segmenter *impl = impl_from_IWordsSegmenter( iface );
    ULONG ref = InterlockedDecrement( &impl->ref );

    TRACE( "iface %p, ref %lu.\n", iface, ref );

    if (!ref)
    {
        WindowsDeleteString( impl->language );
        free( impl );
    }
    return ref;
}

static HRESULT WINAPI words_segmenter_GetIids( IWordsSegmenter *iface, ULONG *iid_count, IID **iids )
{
    FIXME( "iface %p, iid_count %p, iids %p stub!\n", iface, iid_count, iids );
    return E_NOTIMPL;
}

static HRESULT WINAPI words_segmenter_GetRuntimeClassName( IWordsSegmenter *iface, HSTRING *class_name )
{
    TRACE( "iface %p, class_name %p.\n", iface, class_name );
    return WindowsCreateString( RuntimeClass_Windows_Data_Text_WordsSegmenter,
                                wcslen( RuntimeClass_Windows_Data_Text_WordsSegmenter ), class_name );
}

static HRESULT WINAPI words_segmenter_GetTrustLevel( IWordsSegmenter *iface, TrustLevel *trust_level )
{
    FIXME( "iface %p, trust_level %p stub!\n", iface, trust_level );
    return E_NOTIMPL;
}

static HRESULT WINAPI words_segmenter_get_ResolvedLanguage( IWordsSegmenter *iface, HSTRING *value )
{
    struct words_segmenter *impl = impl_from_IWordsSegmenter( iface );

    TRACE( "iface %p, value %p.\n", iface, value );

    return WindowsDuplicateString( impl->language, value );
}

static HRESULT WINAPI words_segmenter_GetTokenAt( IWordsSegmenter *iface, HSTRING text, UINT32 index, IWordSegment **result )
{
    const WCHAR *buffer;
    struct token *tokens;
    UINT32 i, length, count;
    HRESULT hr;

    TRACE( "iface %p, text %s, index %u, result %p.\n", iface, debugstr_hstring( text ), index, result );

    *result = NULL;
    buffer = WindowsGetStringRawBuffer( text, &length );
    if (index >= length) return E_BOUNDS;
    if (FAILED(hr = split_words( buffer, length, &tokens, &count ))) return hr;

    for (i = 0; i < count; i++)
    {
        if (tokens[i].start + tokens[i].length <= index) continue;
        hr = word_segment_create( buffer, tokens + i, result );
        break;
    }

    free( tokens );
    return hr;
}

static HRESULT WINAPI words_segmenter_GetTokens( IWordsSegmenter *iface, HSTRING text, IVectorView_WordSegment **result )
{
    const WCHAR *buffer;
    struct token *tokens;
    UINT32 length, count;
    HRESULT hr;

    TRACE( "iface %p, text %s, result %p.\n", iface, debugstr_hstring( text ), result );

    buffer = WindowsGetStringRawBuffer( text, &length );
    if (FAILED(hr = split_words( buffer, length, &tokens, &count ))) return hr;
    hr = segment_vector_create( buffer, tokens, count, (IVectorView_IInspectable **)result );
    free( tokens );
    return hr;
}

static HRESULT WINAPI words_segmenter_Tokenize( IWordsSegmenter *iface, HSTRING text, UINT32 index,
                                                IWordSegmentsTokenizingHandler *handler )
{
    IIterable_WordSegment *preceding = NULL, *words = NULL;
    IVectorView_IInspectable *view;
    const WCHAR *buffer;
    struct token *tokens;
    UINT32 split, length, count;
    HRESULT hr;

    TRACE( "iface %p, text %s, index %u, handler %p.\n", iface, debugstr_hstring( text ), index, handler );

    if (!handler) return E_POINTER;
    buffer = WindowsGetStringRawBuffer( text, &length );
    if (FAILED(hr = split_words( buffer, length, &tokens, &count ))) return hr;
    for (split = 0; split < count && tokens[split].start + tokens[split].length <= index; split++);

    if (SUCCEEDED(hr = segment_vector_create( buffer, tokens, split, &view )))
    {
        hr = IVectorView_IInspectable_QueryInterface( view, &IID_IIterable_WordSegment, (void **)&preceding );
        IVectorView_IInspectable_Release( view );
    }
    if (SUCCEEDED(hr) && SUCCEEDED(hr = segment_vector_create( buffer, tokens + split, count - split, &view )))
    {
        hr = IVectorView_IInspectable_QueryInterface( view, &IID_IIterable_WordSegment, (void **)&words );
        IVectorView_IInspectable_Release( view );
    }
    if (SUCCEEDED(hr)) hr = IWordSegmentsTokenizingHandler_Invoke( handler, preceding, words );

    if (words) IIterable_WordSegment_Release( words );
    if (preceding) IIterable_WordSegment_Release( preceding );
    free( tokens );
    return hr;
}

static const struct IWordsSegmenterVtbl words_segmenter_vtbl =
{
    words_segmenter_QueryInterface,
    words_segmenter_AddRef,
    words_segmenter_Release,
    /* IInspectable methods */
    words_segmenter_GetIids,
    words_segmenter_GetRuntimeClassName,
    words_segmenter_GetTrustLevel,
    /* IWordsSegmenter methods */
    words_segmenter_get_ResolvedLanguage,
    words_segmenter_GetTokenAt,
    words_segmenter_GetTokens,
    words_segmenter_Tokenize,
};

struct words_segmenter_factory
{
    IActivationFactory IActivationFactory_iface;
    IWordsSegmenterFactory IWordsSegmenterFactory_iface;
    LONG ref;
};

static inline struct words_segmenter_factory *impl_from_IActivationFactory( IActivationFactory *iface )
{
    return CONTAINING_RECORD( iface, struct words_segmenter_factory, IActivationFactory_iface );
}

static HRESULT WINAPI activation_factory_QueryInterface( IActivationFactory *iface, REFIID iid, void **out )
{
    struct words_segmenter_factory *factory = impl_from_IActivationFactory( iface );

    TRACE( "iface %p, iid %s, out %p.\n", iface, debugstr_guid( iid ), out );

    if (IsEqualGUID( iid, &IID_IUnknown ) ||
        IsEqualGUID( iid, &IID_IInspectable ) ||
        IsEqualGUID( iid, &IID_IAgileObject ) ||
        IsEqualGUID( iid, &IID_IActivationFactory ))
    {
        IActivationFactory_AddRef( (*out = &factory->IActivationFactory_iface) );
        return S_OK;
    }

    if (IsEqualGUID( iid, &IID_IWordsSegmenterFactory ))
    {
        IWordsSegmenterFactory_AddRef( (*out = &factory->IWordsSegmenterFactory_iface) );
        return S_OK;
    }

    FIXME( "%s not implemented, returning E_NOINTERFACE.\n", debugstr_guid( iid ) );
    *out = NULL;
    return E_NOINTERFACE;
}

static ULONG WINAPI activation_factory_AddRef( IActivationFactory *iface )
{
    struct words_segmenter_factory *impl = impl_from_IActivationFactory( iface );
    ULONG ref = InterlockedIncrement( &impl->ref );
    TRACE( "iface %p, ref %lu.\n", iface, ref );
    return ref;
}

static ULONG WINAPI activation_factory_Release( IActivationFactory *iface )
{
    struct words_segmenter_factory *impl = impl_from_IActivationFactory( iface );
    ULONG ref = InterlockedDecrement( &impl->ref );
    TRACE( "iface %p, ref %lu.\n", iface, ref );
    return ref;
}

static HRESULT WINAPI activation_factory_GetIids( IActivationFactory *iface, ULONG *iid_count, IID **iids )
{
    FIXME( "iface %p, iid_count %p, iids %p stub!\n", iface, iid_count, iids );
    return E_NOTIMPL;
}

static HRESULT WINAPI activation_factory_GetRuntimeClassName( IActivationFactory *iface, HSTRING *class_name )
{
    FIXME( "iface %p, class_name %p stub!\n", iface, class_name );
    return E_NOTIMPL;
}

static HRESULT WINAPI activation_factory_GetTrustLevel( IActivationFactory *iface, TrustLevel *trust_level )
{
    FIXME( "iface %p, trust_level %p stub!\n", iface, trust_level );
    return E_NOTIMPL;
}

static HRESULT WINAPI activation_factory_ActivateInstance( IActivationFactory *iface, IInspectable **out )
{
    TRACE( "iface %p, out %p.\n", iface, out );

    /* WordsSegmenter is only constructed with a language. */
    *out = NULL;
    return E_NOTIMPL;
}

static const struct IActivationFactoryVtbl activation_factory_vtbl =
{
    activation_factory_QueryInterface,
    activation_factory_AddRef,
    activation_factory_Release,
    /* IInspectable methods */
    activation_factory_GetIids,
    activation_factory_GetRuntimeClassName,
    activation_factory_GetTrustLevel,
    /* IActivationFactory methods */
    activation_factory_ActivateInstance,
};

DEFINE_IINSPECTABLE( words_segmenter_factory, IWordsSegmenterFactory, struct words_segmenter_factory, IActivationFactory_iface )

static HRESULT WINAPI words_segmenter_factory_CreateWithLanguage( IWordsSegmenterFactory *iface, HSTRING language,
                                                                 IWordsSegmenter **result )
{
    WCHAR buffer[LOCALE_NAME_MAX_LENGTH];
    struct words_segmenter *impl;
    HRESULT hr;

    TRACE( "iface %p, language %s, result %p.\n", iface, debugstr_hstring( language ), result );

    /* Languages without a known locale fall back to the undetermined tag. */
    if (!ResolveLocaleName( WindowsGetStringRawBuffer( language, NULL ), buffer, ARRAY_SIZE(buffer) ) || !buffer[0])
        wcscpy( buffer, L"und" );

    if (!(impl = calloc( 1, sizeof(*impl) ))) return E_OUTOFMEMORY;
    impl->IWordsSegmenter_iface.lpVtbl = &words_segmenter_vtbl;
    impl->ref = 1;

    if (FAILED(hr = WindowsCreateString( buffer, wcslen( buffer ), &impl->language )))
    {
        free( impl );
        return hr;
    }

    *result = &impl->IWordsSegmenter_iface;
    return S_OK;
}

static const struct IWordsSegmenterFactoryVtbl words_segmenter_factory_vtbl =
{
    words_segmenter_factory_QueryInterface,
    words_segmenter_factory_AddRef,
    words_segmenter_factory_Release,
    /* IInspectable methods */
    words_segmenter_factory_GetIids,
    words_segmenter_factory_GetRuntimeClassName,
    words_segmenter_factory_GetTrustLevel,
    /* IWordsSegmenterFactory methods */
    words_segmenter_factory_CreateWithLanguage,
};

static struct words_segmenter_factory factory =
{
    {&activation_factory_vtbl},
    {&words_segmenter_factory_vtbl},
    1,
};

IActivationFactory *words_segmenter_factory = &factory.IActivationFactory_iface;
