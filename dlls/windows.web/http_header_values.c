#include "private.h"

struct header_value
{
    union
    {
        IHttpMediaTypeHeaderValue media;
        IHttpNameValueHeaderValue pair;
    } iface;
    IStringable stringable;
    LONG ref;
    BOOL media;
    SRWLOCK lock;
    HSTRING name, value;
    IVector_HttpNameValueHeaderValue *parameters;
};

struct string_buffer
{
    WCHAR *data;
    SIZE_T length, capacity;
    HRESULT status;
};

static const IHttpMediaTypeHeaderValueVtbl media_vtbl;
static const IHttpNameValueHeaderValueVtbl pair_vtbl;
static const IStringableVtbl value_string_vtbl;

static void append_span(struct string_buffer *buffer, const WCHAR *text, SIZE_T length)
{
    SIZE_T capacity, required;
    WCHAR *data;

    if (FAILED(buffer->status) || !length) return;
    if (length > (SIZE_T)~(UINT32)0 - buffer->length ||
            length > ~(SIZE_T)0 / sizeof(WCHAR) - buffer->length - 1)
    {
        buffer->status = E_OUTOFMEMORY;
        return;
    }
    required = buffer->length + length + 1;
    if (required > buffer->capacity)
    {
        capacity = required;
        if (buffer->capacity <= ~(SIZE_T)0 / sizeof(WCHAR) / 2 && buffer->capacity * 2 > required)
            capacity = buffer->capacity * 2;
        if (!(data = realloc(buffer->data, capacity * sizeof(WCHAR))))
        {
            buffer->status = E_OUTOFMEMORY;
            return;
        }
        buffer->data = data;
        buffer->capacity = capacity;
    }
    memcpy(buffer->data + buffer->length, text, length * sizeof(WCHAR));
    buffer->length += length;
    buffer->data[buffer->length] = 0;
}

static void append_string(struct string_buffer *buffer, HSTRING value)
{
    UINT32 length;
    const WCHAR *text = WindowsGetStringRawBuffer(value, &length);
    append_span(buffer, text, length);
}

static HRESULT finish_string(struct string_buffer *buffer, HSTRING *value)
{
    HRESULT hr = buffer->status;
    if (SUCCEEDED(hr))
        hr = WindowsCreateString(buffer->data, buffer->length, value);
    free(buffer->data);
    return hr;
}

static BOOL token_char(WCHAR ch)
{
    return (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') || (ch >= '0' && ch <= '9') ||
            (ch && wcschr(L"!#$%&'*+-.^_`|~", ch));
}

static const WCHAR *read_token(const WCHAR *cursor, const WCHAR *end)
{
    while (cursor < end && token_char(*cursor)) ++cursor;
    return cursor;
}

static const WCHAR *skip_space(const WCHAR *cursor, const WCHAR *end)
{
    while (cursor < end && (*cursor == ' ' || *cursor == '\t')) ++cursor;
    return cursor;
}

static BOOL valid_token(HSTRING value)
{
    UINT32 length;
    const WCHAR *text = WindowsGetStringRawBuffer(value, &length);
    return length && read_token(text, text + length) == text + length;
}

static BOOL valid_media(HSTRING value)
{
    UINT32 length;
    const WCHAR *text = WindowsGetStringRawBuffer(value, &length), *end = text + length;
    const WCHAR *cursor = read_token(text, end);
    if (cursor == text || cursor == end || *cursor++ != '/') return FALSE;
    text = cursor;
    cursor = read_token(cursor, end);
    return cursor != text && cursor == end;
}

static ULONG value_addref(struct header_value *impl)
{
    return InterlockedIncrement(&impl->ref);
}

static ULONG value_release(struct header_value *impl)
{
    ULONG ref = InterlockedDecrement(&impl->ref);
    if (!ref)
    {
        WindowsDeleteString(impl->name);
        WindowsDeleteString(impl->value);
        if (impl->parameters) IVector_HttpNameValueHeaderValue_Release(impl->parameters);
        free(impl);
    }
    return ref;
}

static HRESULT value_query(struct header_value *impl, REFIID iid, void **out)
{
    if (!out) return E_POINTER;
    *out = NULL;
    if (IsEqualGUID(iid, &IID_IUnknown) || IsEqualGUID(iid, &IID_IInspectable) ||
            IsEqualGUID(iid, &IID_IAgileObject) ||
            IsEqualGUID(iid, impl->media ? &IID_IHttpMediaTypeHeaderValue : &IID_IHttpNameValueHeaderValue))
        *out = &impl->iface;
    else if (IsEqualGUID(iid, &IID_IStringable)) *out = &impl->stringable;
    else return E_NOINTERFACE;
    value_addref(impl);
    return S_OK;
}

static HRESULT value_get_iids(struct header_value *impl, ULONG *count, IID **iids)
{
    if (!count || !iids) return E_POINTER;
    *count = 0;
    *iids = NULL;
    if (!(*iids = CoTaskMemAlloc(2 * sizeof(**iids)))) return E_OUTOFMEMORY;
    (*iids)[0] = impl->media ? IID_IHttpMediaTypeHeaderValue : IID_IHttpNameValueHeaderValue;
    (*iids)[1] = IID_IStringable;
    *count = 2;
    return S_OK;
}

static HRESULT value_get_name(struct header_value *impl, HSTRING *name)
{
    const WCHAR *text = impl->media ? RuntimeClass_Windows_Web_Http_Headers_HttpMediaTypeHeaderValue
            : RuntimeClass_Windows_Web_Http_Headers_HttpNameValueHeaderValue;
    if (!name) return E_POINTER;
    return WindowsCreateString(text, wcslen(text), name);
}

#define VALUE_BASE(prefix, type, field) \
    static inline struct header_value *value_from_##prefix(type *iface) \
    { return CONTAINING_RECORD(iface, struct header_value, field); } \
    static HRESULT WINAPI prefix##_QueryInterface(type *iface, REFIID iid, void **out) \
    { return value_query(value_from_##prefix(iface), iid, out); } \
    static ULONG WINAPI prefix##_AddRef(type *iface) \
    { return value_addref(value_from_##prefix(iface)); } \
    static ULONG WINAPI prefix##_Release(type *iface) \
    { return value_release(value_from_##prefix(iface)); } \
    static HRESULT WINAPI prefix##_GetIids(type *iface, ULONG *count, IID **iids) \
    { return value_get_iids(value_from_##prefix(iface), count, iids); } \
    static HRESULT WINAPI prefix##_GetRuntimeClassName(type *iface, HSTRING *name) \
    { return value_get_name(value_from_##prefix(iface), name); } \
    static HRESULT WINAPI prefix##_GetTrustLevel(type *iface, TrustLevel *level) \
    { if (!level) return E_POINTER; *level = BaseTrust; return S_OK; }

VALUE_BASE(media, IHttpMediaTypeHeaderValue, iface.media)
VALUE_BASE(pair, IHttpNameValueHeaderValue, iface.pair)
VALUE_BASE(value_string, IStringable, stringable)

static HRESULT value_create(BOOL media, HSTRING name, HSTRING value, struct header_value **out)
{
    static const struct vector_iids iids = {&IID_IVector_HttpNameValueHeaderValue,
            &IID_IVectorView_HttpNameValueHeaderValue, &IID_IIterable_HttpNameValueHeaderValue,
            &IID_IIterator_HttpNameValueHeaderValue};
    struct header_value *impl;
    HRESULT hr;

    *out = NULL;
    if (!media && !valid_token(name)) return E_INVALIDARG;
    if (!(impl = calloc(1, sizeof(*impl)))) return E_OUTOFMEMORY;
    if (media) impl->iface.media.lpVtbl = &media_vtbl;
    else impl->iface.pair.lpVtbl = &pair_vtbl;
    impl->stringable.lpVtbl = &value_string_vtbl;
    impl->media = media;
    impl->ref = 1;
    hr = WindowsDuplicateString(name, &impl->name);
    if (SUCCEEDED(hr)) hr = WindowsDuplicateString(value, &impl->value);
    if (SUCCEEDED(hr) && media) hr = vector_create(&iids, (void **)&impl->parameters);
    if (FAILED(hr))
    {
        value_release(impl);
        return hr;
    }
    *out = impl;
    return S_OK;
}

static HRESULT get_value_string(struct header_value *impl, BOOL name, HSTRING *value)
{
    HRESULT hr;
    if (!value) return E_POINTER;
    *value = NULL;
    AcquireSRWLockShared(&impl->lock);
    hr = WindowsDuplicateString(name ? impl->name : impl->value, value);
    ReleaseSRWLockShared(&impl->lock);
    return hr;
}

static HRESULT set_value_string(struct header_value *impl, HSTRING value)
{
    HSTRING copy, old;
    HRESULT hr;
    if (FAILED(hr = WindowsDuplicateString(value, &copy))) return hr;
    AcquireSRWLockExclusive(&impl->lock);
    old = impl->value;
    impl->value = copy;
    ReleaseSRWLockExclusive(&impl->lock);
    WindowsDeleteString(old);
    return S_OK;
}

static HRESULT WINAPI pair_get_Name(IHttpNameValueHeaderValue *iface, HSTRING *value)
{
    return get_value_string(value_from_pair(iface), TRUE, value);
}

static HRESULT WINAPI pair_get_Value(IHttpNameValueHeaderValue *iface, HSTRING *value)
{
    return get_value_string(value_from_pair(iface), FALSE, value);
}

static HRESULT WINAPI pair_put_Value(IHttpNameValueHeaderValue *iface, HSTRING value)
{
    return set_value_string(value_from_pair(iface), value);
}

static HRESULT WINAPI media_get_MediaType(IHttpMediaTypeHeaderValue *iface, HSTRING *value)
{
    return get_value_string(value_from_media(iface), FALSE, value);
}

static HRESULT WINAPI media_put_MediaType(IHttpMediaTypeHeaderValue *iface, HSTRING value)
{
    if (!valid_media(value)) return E_INVALIDARG;
    return set_value_string(value_from_media(iface), value);
}

static HRESULT WINAPI media_get_Parameters(IHttpMediaTypeHeaderValue *iface, IVector_HttpNameValueHeaderValue **value)
{
    if (!value) return E_POINTER;
    *value = value_from_media(iface)->parameters;
    IVector_HttpNameValueHeaderValue_AddRef(*value);
    return S_OK;
}

static HRESULT find_charset(struct header_value *impl, UINT32 *index, IHttpNameValueHeaderValue **value)
{
    UINT32 count, i;
    HSTRING name;
    HRESULT hr;

    *value = NULL;
    if (FAILED(hr = IVector_HttpNameValueHeaderValue_get_Size(impl->parameters, &count))) return hr;
    for (i = 0; i < count; ++i)
    {
        if (FAILED(hr = IVector_HttpNameValueHeaderValue_GetAt(impl->parameters, i, value))) return hr;
        hr = IHttpNameValueHeaderValue_get_Name(*value, &name);
        if (SUCCEEDED(hr))
        {
            BOOL found = !_wcsicmp(WindowsGetStringRawBuffer(name, NULL), L"charset");
            WindowsDeleteString(name);
            if (found)
            {
                *index = i;
                return S_OK;
            }
        }
        IHttpNameValueHeaderValue_Release(*value);
        *value = NULL;
        if (FAILED(hr)) return hr;
    }
    return S_FALSE;
}

static HRESULT WINAPI media_get_CharSet(IHttpMediaTypeHeaderValue *iface, HSTRING *value)
{
    IHttpNameValueHeaderValue *parameter;
    UINT32 index;
    HRESULT hr;

    if (!value) return E_POINTER;
    *value = NULL;
    hr = find_charset(value_from_media(iface), &index, &parameter);
    if (hr == S_FALSE) return S_OK;
    if (FAILED(hr)) return hr;
    hr = IHttpNameValueHeaderValue_get_Value(parameter, value);
    IHttpNameValueHeaderValue_Release(parameter);
    return hr;
}

static HRESULT WINAPI media_put_CharSet(IHttpMediaTypeHeaderValue *iface, HSTRING value)
{
    struct header_value *impl = value_from_media(iface), *created;
    IHttpNameValueHeaderValue *parameter;
    HSTRING_HEADER header;
    HSTRING name;
    UINT32 index;
    HRESULT hr;

    hr = find_charset(impl, &index, &parameter);
    if (FAILED(hr)) return hr;
    if (parameter)
    {
        if (WindowsGetStringLen(value)) hr = IHttpNameValueHeaderValue_put_Value(parameter, value);
        else hr = IVector_HttpNameValueHeaderValue_RemoveAt(impl->parameters, index);
        IHttpNameValueHeaderValue_Release(parameter);
        return hr;
    }
    if (!WindowsGetStringLen(value)) return S_OK;
    WindowsCreateStringReference(L"charset", 7, &header, &name);
    if (FAILED(hr = value_create(FALSE, name, value, &created))) return hr;
    hr = IVector_HttpNameValueHeaderValue_Append(impl->parameters, &created->iface.pair);
    value_release(created);
    return hr;
}

static void format_pair(struct string_buffer *buffer, HSTRING name, HSTRING value)
{
    static const WCHAR hex[] = L"0123456789ABCDEF";
    const WCHAR *text, *key;
    UINT32 length, name_length, i;
    BOOL quoted;

    key = WindowsGetStringRawBuffer(name, &name_length);
    append_span(buffer, key, name_length);
    text = WindowsGetStringRawBuffer(value, &length);
    if (!length) return;
    append_span(buffer, L"=", 1);
    if (name_length && key[name_length - 1] == '*')
    {
        int bytes;
        char *utf8;
        if (length > INT_MAX) { buffer->status = E_OUTOFMEMORY; return; }
        bytes = WideCharToMultiByte(CP_UTF8, 0, text, length, NULL, 0, NULL, NULL);
        if (!bytes || !(utf8 = malloc(bytes))) { buffer->status = E_OUTOFMEMORY; return; }
        WideCharToMultiByte(CP_UTF8, 0, text, length, utf8, bytes, NULL, NULL);
        append_span(buffer, L"UTF-8''", 7);
        for (i = 0; i < (UINT32)bytes; ++i)
        {
            unsigned char ch = utf8[i];
            if (token_char(ch) && ch != '*' && ch != '\'' && ch != '%')
            {
                WCHAR wide = ch;
                append_span(buffer, &wide, 1);
            }
            else
            {
                WCHAR escaped[] = {'%', hex[ch >> 4], hex[ch & 15]};
                append_span(buffer, escaped, 3);
            }
        }
        free(utf8);
        return;
    }
    quoted = !wcscmp(key, L"name") || !wcscmp(key, L"filename");
    for (i = 0; i < length; ++i) if (!token_char(text[i])) quoted = TRUE;
    if (quoted) append_span(buffer, L"\"", 1);
    for (i = 0; i < length; ++i)
    {
        if (quoted && (text[i] == '\\' || text[i] == '"' || text[i] < 32 || text[i] == 127))
            append_span(buffer, L"\\", 1);
        append_span(buffer, text + i, 1);
    }
    if (quoted) append_span(buffer, L"\"", 1);
}

static HRESULT WINAPI value_string_ToString(IStringable *iface, HSTRING *out)
{
    struct header_value *impl = value_from_value_string(iface);
    struct string_buffer buffer = {0};
    IVectorView_HttpNameValueHeaderValue *view = NULL;
    IHttpNameValueHeaderValue *parameter;
    IStringable *stringable;
    HSTRING name = NULL, value = NULL;
    UINT32 count = 0, i;
    HRESULT hr;

    if (!out) return E_POINTER;
    *out = NULL;
    AcquireSRWLockShared(&impl->lock);
    hr = WindowsDuplicateString(impl->name, &name);
    if (SUCCEEDED(hr)) hr = WindowsDuplicateString(impl->value, &value);
    ReleaseSRWLockShared(&impl->lock);
    if (FAILED(hr)) { WindowsDeleteString(name); return hr; }
    if (!impl->media) format_pair(&buffer, name, value);
    else
    {
        append_string(&buffer, value);
        hr = IVector_HttpNameValueHeaderValue_GetView(impl->parameters, &view);
        if (SUCCEEDED(hr)) hr = IVectorView_HttpNameValueHeaderValue_get_Size(view, &count);
        for (i = 0; SUCCEEDED(hr) && i < count; ++i)
        {
            HSTRING item = NULL;
            hr = IVectorView_HttpNameValueHeaderValue_GetAt(view, i, &parameter);
            if (FAILED(hr)) break;
            hr = IHttpNameValueHeaderValue_QueryInterface(parameter, &IID_IStringable, (void **)&stringable);
            IHttpNameValueHeaderValue_Release(parameter);
            if (FAILED(hr)) break;
            hr = IStringable_ToString(stringable, &item);
            IStringable_Release(stringable);
            if (SUCCEEDED(hr))
            {
                append_span(&buffer, L"; ", 2);
                append_string(&buffer, item);
            }
            WindowsDeleteString(item);
        }
        if (view) IVectorView_HttpNameValueHeaderValue_Release(view);
    }
    WindowsDeleteString(name);
    WindowsDeleteString(value);
    if (FAILED(hr)) buffer.status = hr;
    return finish_string(&buffer, out);
}

static const IHttpMediaTypeHeaderValueVtbl media_vtbl = {
    media_QueryInterface, media_AddRef, media_Release, media_GetIids,
    media_GetRuntimeClassName, media_GetTrustLevel, media_get_CharSet, media_put_CharSet,
    media_get_MediaType, media_put_MediaType, media_get_Parameters
};
static const IHttpNameValueHeaderValueVtbl pair_vtbl = {
    pair_QueryInterface, pair_AddRef, pair_Release, pair_GetIids,
    pair_GetRuntimeClassName, pair_GetTrustLevel, pair_get_Name, pair_get_Value, pair_put_Value
};
static const IStringableVtbl value_string_vtbl = {
    value_string_QueryInterface, value_string_AddRef, value_string_Release, value_string_GetIids,
    value_string_GetRuntimeClassName, value_string_GetTrustLevel, value_string_ToString
};

static HRESULT parse_parameter(const WCHAR **cursor, const WCHAR *end, BOOL require_value,
        struct header_value **out)
{
    const WCHAR *name, *name_end, *position = skip_space(*cursor, end), *start;
    struct string_buffer buffer = {0};
    HSTRING key = NULL, value = NULL;
    HRESULT hr;

    *out = NULL;
    name = position;
    name_end = read_token(position, end);
    if (name_end == name) return E_INVALIDARG;
    position = skip_space(name_end, end);
    if (position < end && *position == '=')
    {
        position = skip_space(position + 1, end);
        if (position == end) return E_INVALIDARG;
        if (*position == '"')
        {
            ++position;
            while (position < end && *position != '"')
            {
                WCHAR ch = *position++;
                if (ch == '\\')
                {
                    if (position == end) { free(buffer.data); return E_INVALIDARG; }
                    ch = *position++;
                }
                if (ch < 32 && ch != '\t') { free(buffer.data); return E_INVALIDARG; }
                append_span(&buffer, &ch, 1);
            }
            if (position == end) { free(buffer.data); return E_INVALIDARG; }
            ++position;
        }
        else
        {
            start = position;
            position = read_token(position, end);
            if (position == start) return E_INVALIDARG;
            append_span(&buffer, start, position - start);
        }
    }
    else if (require_value) return E_INVALIDARG;
    hr = finish_string(&buffer, &value);
    if (SUCCEEDED(hr)) hr = WindowsCreateString(name, name_end - name, &key);
    if (SUCCEEDED(hr)) hr = value_create(FALSE, key, value, out);
    WindowsDeleteString(key);
    WindowsDeleteString(value);
    *cursor = skip_space(position, end);
    return hr;
}

static HRESULT parse_header(BOOL media, HSTRING input, struct header_value **out)
{
    const WCHAR *text, *end, *cursor, *type, *type_end, *subtype, *subtype_end;
    struct header_value *impl = NULL, *parameter;
    struct string_buffer buffer = {0};
    HSTRING value = NULL;
    UINT32 length;
    HRESULT hr;

    *out = NULL;
    text = WindowsGetStringRawBuffer(input, &length);
    end = text + length;
    cursor = skip_space(text, end);
    if (!media)
    {
        hr = parse_parameter(&cursor, end, FALSE, &impl);
        if (SUCCEEDED(hr) && cursor != end) hr = E_INVALIDARG;
    }
    else
    {
        type = cursor;
        type_end = read_token(cursor, end);
        cursor = skip_space(type_end, end);
        if (type_end == type || cursor == end || *cursor++ != '/') return E_INVALIDARG;
        subtype = skip_space(cursor, end);
        subtype_end = read_token(subtype, end);
        if (subtype_end == subtype) return E_INVALIDARG;
        append_span(&buffer, type, type_end - type);
        append_span(&buffer, L"/", 1);
        append_span(&buffer, subtype, subtype_end - subtype);
        if (FAILED(hr = finish_string(&buffer, &value))) return hr;
        hr = value_create(TRUE, NULL, value, &impl);
        WindowsDeleteString(value);
        cursor = skip_space(subtype_end, end);
        while (SUCCEEDED(hr) && cursor < end)
        {
            if (*cursor++ != ';') { hr = E_INVALIDARG; break; }
            hr = parse_parameter(&cursor, end, TRUE, &parameter);
            if (FAILED(hr)) break;
            hr = IVector_HttpNameValueHeaderValue_Append(impl->parameters, &parameter->iface.pair);
            value_release(parameter);
        }
    }
    if (FAILED(hr))
    {
        if (impl) value_release(impl);
        return hr;
    }
    *out = impl;
    return S_OK;
}

struct header_factory
{
    IActivationFactory activation;
    union { IHttpMediaTypeHeaderValueFactory media; IHttpNameValueHeaderValueFactory pair; } factory;
    union { IHttpMediaTypeHeaderValueStatics media; IHttpNameValueHeaderValueStatics pair; } statics;
    BOOL media;
    LONG ref;
};

static HRESULT factory_query(struct header_factory *impl, REFIID iid, void **out)
{
    if (!out) return E_POINTER;
    *out = NULL;
    if (IsEqualGUID(iid, &IID_IUnknown) || IsEqualGUID(iid, &IID_IInspectable) ||
            IsEqualGUID(iid, &IID_IAgileObject) || IsEqualGUID(iid, &IID_IActivationFactory))
        *out = &impl->activation;
    else if (IsEqualGUID(iid, impl->media ? &IID_IHttpMediaTypeHeaderValueFactory : &IID_IHttpNameValueHeaderValueFactory))
        *out = &impl->factory;
    else if (IsEqualGUID(iid, impl->media ? &IID_IHttpMediaTypeHeaderValueStatics : &IID_IHttpNameValueHeaderValueStatics))
        *out = &impl->statics;
    else return E_NOINTERFACE;
    InterlockedIncrement(&impl->ref);
    return S_OK;
}

#define FACTORY_BASE(prefix, type, field) \
    static inline struct header_factory *factory_from_##prefix(type *iface) \
    { return CONTAINING_RECORD(iface, struct header_factory, field); } \
    static HRESULT WINAPI prefix##_QueryInterface(type *iface, REFIID iid, void **out) \
    { return factory_query(factory_from_##prefix(iface), iid, out); } \
    static ULONG WINAPI prefix##_AddRef(type *iface) \
    { return InterlockedIncrement(&factory_from_##prefix(iface)->ref); } \
    static ULONG WINAPI prefix##_Release(type *iface) \
    { return InterlockedDecrement(&factory_from_##prefix(iface)->ref); } \
    static HRESULT WINAPI prefix##_GetIids(type *iface, ULONG *count, IID **iids) \
    { \
        struct header_factory *impl = factory_from_##prefix(iface); \
        if (!count || !iids) return E_POINTER; \
        *count = 0; *iids = NULL; \
        if (!(*iids = CoTaskMemAlloc(2 * sizeof(**iids)))) return E_OUTOFMEMORY; \
        (*iids)[0] = impl->media ? IID_IHttpMediaTypeHeaderValueFactory : IID_IHttpNameValueHeaderValueFactory; \
        (*iids)[1] = impl->media ? IID_IHttpMediaTypeHeaderValueStatics : IID_IHttpNameValueHeaderValueStatics; \
        *count = 2; return S_OK; \
    } \
    static HRESULT WINAPI prefix##_GetRuntimeClassName(type *iface, HSTRING *name) \
    { \
        const WCHAR *text = factory_from_##prefix(iface)->media ? RuntimeClass_Windows_Web_Http_Headers_HttpMediaTypeHeaderValue \
                : RuntimeClass_Windows_Web_Http_Headers_HttpNameValueHeaderValue; \
        if (!name) return E_POINTER; return WindowsCreateString(text, wcslen(text), name); \
    } \
    static HRESULT WINAPI prefix##_GetTrustLevel(type *iface, TrustLevel *level) \
    { if (!level) return E_POINTER; *level = BaseTrust; return S_OK; }

FACTORY_BASE(activation, IActivationFactory, activation)
FACTORY_BASE(media_factory, IHttpMediaTypeHeaderValueFactory, factory.media)
FACTORY_BASE(pair_factory, IHttpNameValueHeaderValueFactory, factory.pair)
FACTORY_BASE(media_statics, IHttpMediaTypeHeaderValueStatics, statics.media)
FACTORY_BASE(pair_statics, IHttpNameValueHeaderValueStatics, statics.pair)

static HRESULT WINAPI activation_ActivateInstance(IActivationFactory *iface, IInspectable **out)
{
    if (!out) return E_POINTER;
    *out = NULL;
    return E_NOTIMPL;
}

static HRESULT WINAPI media_factory_Create(IHttpMediaTypeHeaderValueFactory *iface, HSTRING value,
        IHttpMediaTypeHeaderValue **out)
{
    struct header_value *impl;
    HRESULT hr;
    if (!out) return E_POINTER;
    *out = NULL;
    if (SUCCEEDED(hr = value_create(TRUE, NULL, value, &impl))) *out = &impl->iface.media;
    return hr;
}

static HRESULT WINAPI pair_factory_CreateFromNameWithValue(IHttpNameValueHeaderValueFactory *iface,
        HSTRING name, HSTRING value, IHttpNameValueHeaderValue **out)
{
    struct header_value *impl;
    HRESULT hr;
    if (!out) return E_POINTER;
    *out = NULL;
    if (SUCCEEDED(hr = value_create(FALSE, name, value, &impl))) *out = &impl->iface.pair;
    return hr;
}

static HRESULT WINAPI pair_factory_CreateFromName(IHttpNameValueHeaderValueFactory *iface,
        HSTRING name, IHttpNameValueHeaderValue **out)
{
    return pair_factory_CreateFromNameWithValue(iface, name, NULL, out);
}

#define HEADER_STATICS(prefix, type, value_type, media_value, field) \
    static HRESULT WINAPI prefix##_Parse(type *iface, HSTRING input, value_type **out) \
    { \
        struct header_value *impl; HRESULT hr; \
        if (!out) return E_POINTER; *out = NULL; \
        if (SUCCEEDED(hr = parse_header(media_value, input, &impl))) *out = &impl->iface.field; \
        return hr; \
    } \
    static HRESULT WINAPI prefix##_TryParse(type *iface, HSTRING input, value_type **out, boolean *parsed) \
    { \
        HRESULT hr; \
        if (!out || !parsed) return E_POINTER; *out = NULL; *parsed = FALSE; \
        hr = prefix##_Parse(iface, input, out); \
        if (hr == E_INVALIDARG) return S_OK; \
        if (SUCCEEDED(hr)) *parsed = TRUE; \
        return hr; \
    }

HEADER_STATICS(media_statics, IHttpMediaTypeHeaderValueStatics, IHttpMediaTypeHeaderValue, TRUE, media)
HEADER_STATICS(pair_statics, IHttpNameValueHeaderValueStatics, IHttpNameValueHeaderValue, FALSE, pair)

static const IActivationFactoryVtbl activation_vtbl = {
    activation_QueryInterface, activation_AddRef, activation_Release, activation_GetIids,
    activation_GetRuntimeClassName, activation_GetTrustLevel, activation_ActivateInstance
};
static const IHttpMediaTypeHeaderValueFactoryVtbl media_factory_vtbl = {
    media_factory_QueryInterface, media_factory_AddRef, media_factory_Release, media_factory_GetIids,
    media_factory_GetRuntimeClassName, media_factory_GetTrustLevel, media_factory_Create
};
static const IHttpNameValueHeaderValueFactoryVtbl pair_factory_vtbl = {
    pair_factory_QueryInterface, pair_factory_AddRef, pair_factory_Release, pair_factory_GetIids,
    pair_factory_GetRuntimeClassName, pair_factory_GetTrustLevel, pair_factory_CreateFromName,
    pair_factory_CreateFromNameWithValue
};
static const IHttpMediaTypeHeaderValueStaticsVtbl media_statics_vtbl = {
    media_statics_QueryInterface, media_statics_AddRef, media_statics_Release, media_statics_GetIids,
    media_statics_GetRuntimeClassName, media_statics_GetTrustLevel, media_statics_Parse, media_statics_TryParse
};
static const IHttpNameValueHeaderValueStaticsVtbl pair_statics_vtbl = {
    pair_statics_QueryInterface, pair_statics_AddRef, pair_statics_Release, pair_statics_GetIids,
    pair_statics_GetRuntimeClassName, pair_statics_GetTrustLevel, pair_statics_Parse, pair_statics_TryParse
};
static struct header_factory media_factory = {
    {&activation_vtbl}, {.media = {&media_factory_vtbl}}, {.media = {&media_statics_vtbl}}, TRUE, 1
};
static struct header_factory pair_factory = {
    {&activation_vtbl}, {.pair = {&pair_factory_vtbl}}, {.pair = {&pair_statics_vtbl}}, FALSE, 1
};
IActivationFactory *http_media_value_factory = &media_factory.activation;
IActivationFactory *http_name_value_factory = &pair_factory.activation;
