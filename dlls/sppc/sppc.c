/*
 *
 * Copyright 2008 Alistair Leslie-Hughes
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

#include <stdarg.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>
#include <wchar.h>

#include "ntstatus.h"
#define WIN32_NO_STATUS
#include "windef.h"
#include "winbase.h"
#include "winnls.h"
#include "winternl.h"
#include "wine/debug.h"

#include "slpublic.h"
#include "slerror.h"

WINE_DEFAULT_DEBUG_CHANNEL(slc);

static HRESULT find_license_id(UINT size, const BYTE *blob, UINT *offset, SLID *id)
{
    static const char attribute[] = "licenseId";
    WCHAR string[39];
    UNICODE_STRING stringW;
    UINT i, j, length;
    char quote;

    for (i = *offset; i + sizeof(attribute) < size; i++)
    {
        if (memcmp(blob + i, attribute, sizeof(attribute) - 1)) continue;

        j = i + sizeof(attribute) - 1;
        while (j < size && (blob[j] == ' ' || blob[j] == '\t' || blob[j] == '\r' || blob[j] == '\n')) j++;
        if (j == size || blob[j++] != '=') continue;
        while (j < size && (blob[j] == ' ' || blob[j] == '\t' || blob[j] == '\r' || blob[j] == '\n')) j++;
        if (j == size || (blob[j] != '\'' && blob[j] != '"')) continue;

        quote = blob[j++];
        if (j < size && blob[j] == '{')
        {
            if (size - j < 39 || blob[j + 38] != quote) continue;
            length = 38;
        }
        else
        {
            if (size - j < 37 || blob[j + 36] != quote) continue;
            string[0] = '{';
            length = 36;
        }

        for (i = 0; i < length; i++) string[i + (length == 36)] = blob[j + i];
        string[38] = 0;
        RtlInitUnicodeString(&stringW, string);
        if (!RtlGUIDFromString(&stringW, id))
        {
            *offset = j + length + 1;
            return S_OK;
        }
        return E_INVALIDARG;
    }

    return SL_E_VALUE_NOT_FOUND;
}

static HRESULT append_store_directory(WCHAR *path, const WCHAR *name)
{
    DWORD error;

    if (wcslen(path) + wcslen(name) >= MAX_PATH) return HRESULT_FROM_WIN32(ERROR_FILENAME_EXCED_RANGE);
    wcscat(path, name);
    if (CreateDirectoryW(path, NULL) || (error = GetLastError()) == ERROR_ALREADY_EXISTS) return S_OK;
    return HRESULT_FROM_WIN32(error);
}

static void format_license_id(const SLID *id, WCHAR *string)
{
    swprintf(string, 39, L"{%08X-%04X-%04X-%02X%02X-%02X%02X%02X%02X%02X%02X}", id->Data1,
             id->Data2, id->Data3, id->Data4[0], id->Data4[1], id->Data4[2], id->Data4[3], id->Data4[4],
             id->Data4[5], id->Data4[6], id->Data4[7]);
}

static void format_license_idA(const SLID *id, char *string)
{
    snprintf(string, 39, "{%08X-%04X-%04X-%02X%02X-%02X%02X%02X%02X%02X%02X}", (unsigned int)id->Data1,
             id->Data2, id->Data3, id->Data4[0], id->Data4[1], id->Data4[2], id->Data4[3], id->Data4[4],
             id->Data4[5], id->Data4[6], id->Data4[7]);
}

static const BYTE *find_bytes(const BYTE *start, const BYTE *end, const char *string)
{
    size_t length = strlen(string);

    if (length > end - start) return NULL;
    for (; start <= end - length; start++)
        if (!memcmp(start, string, length)) return start;
    return NULL;
}

static char ascii_tolower(char ch)
{
    if (ch >= 'A' && ch <= 'Z') return ch + ('a' - 'A');
    return ch;
}

static const BYTE *find_bytes_i(const BYTE *start, const BYTE *end, const char *string)
{
    size_t length = strlen(string), i;

    if (length > end - start) return NULL;
    for (; start <= end - length; start++)
    {
        for (i = 0; i < length; i++)
            if (ascii_tolower(start[i]) != ascii_tolower(string[i])) break;
        if (i == length) return start;
    }
    return NULL;
}

static const BYTE *find_license_element(UINT size, const BYTE *blob, const SLID *id, const BYTE **end)
{
    const BYTE *limit = blob + size, *start = blob, *tag_end, *close;
    char guid[39];

    format_license_idA(id, guid);
    while ((start = find_bytes(start, limit, "<r:license")))
    {
        if (!(tag_end = find_bytes(start, limit, ">"))) break;
        if (find_bytes_i(start, tag_end, guid))
        {
            if (!(close = find_bytes(tag_end + 1, limit, "</r:license>"))) return NULL;
            *end = close;
            return tag_end + 1;
        }
        start = tag_end + 1;
    }
    return NULL;
}

static HRESULT read_license_file(const WCHAR *filename, BYTE **blob, UINT *size)
{
    LARGE_INTEGER file_size;
    DWORD error, read;
    HANDLE file;

    file = CreateFileW(filename, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    if (file == INVALID_HANDLE_VALUE) return HRESULT_FROM_WIN32(GetLastError());
    if (!GetFileSizeEx(file, &file_size))
    {
        error = GetLastError();
        CloseHandle(file);
        return HRESULT_FROM_WIN32(error);
    }
    if (!file_size.QuadPart || file_size.QuadPart > UINT_MAX)
    {
        CloseHandle(file);
        return SL_E_INVALID_LICENSE;
    }
    if (!(*blob = HeapAlloc(GetProcessHeap(), 0, file_size.LowPart)))
    {
        CloseHandle(file);
        return E_OUTOFMEMORY;
    }
    if (!ReadFile(file, *blob, file_size.LowPart, &read, NULL) || read != file_size.LowPart)
    {
        error = GetLastError();
        if (!error) error = ERROR_READ_FAULT;
        HeapFree(GetProcessHeap(), 0, *blob);
        CloseHandle(file);
        return HRESULT_FROM_WIN32(error);
    }
    CloseHandle(file);
    *size = read;
    return S_OK;
}

static HRESULT get_license_path(const SLID *id, WCHAR *filename);

static HRESULT load_license_blob(const SLID *id, BYTE **blob, UINT *size)
{
    const BYTE *license_end;
    WIN32_FIND_DATAW data;
    WCHAR filename[MAX_PATH], search[MAX_PATH], *name;
    HANDLE find;
    HRESULT hr;
    UINT length;

    if (FAILED(hr = get_license_path(id, filename))) return hr;
    if (SUCCEEDED(hr = read_license_file(filename, blob, size)))
    {
        if (find_license_element(*size, *blob, id, &license_end)) return S_OK;
        HeapFree(GetProcessHeap(), 0, *blob);
    }
    else if (HRESULT_CODE(hr) != ERROR_FILE_NOT_FOUND && HRESULT_CODE(hr) != ERROR_PATH_NOT_FOUND) return hr;

    length = GetSystemDirectoryW(search, ARRAY_SIZE(search));
    if (!length) return HRESULT_FROM_WIN32(GetLastError());
    if (length >= ARRAY_SIZE(search) - ARRAY_SIZE(L"\\spp\\store\\2.0\\licenses\\*.xrm-ms"))
        return HRESULT_FROM_WIN32(ERROR_FILENAME_EXCED_RANGE);
    wcscat(search, L"\\spp\\store\\2.0\\licenses\\*.xrm-ms");
    if ((find = FindFirstFileW(search, &data)) == INVALID_HANDLE_VALUE) return SL_E_VALUE_NOT_FOUND;
    name = wcsrchr(search, L'\\') + 1;

    do
    {
        const BYTE *end;

        if (data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;
        if (name - search + wcslen(data.cFileName) >= ARRAY_SIZE(search)) continue;
        wcscpy(name, data.cFileName);
        if (FAILED(hr = read_license_file(search, blob, size))) continue;
        if (find_license_element(*size, *blob, id, &end))
        {
            FindClose(find);
            return S_OK;
        }
        HeapFree(GetProcessHeap(), 0, *blob);
    } while (FindNextFileW(find, &data));

    FindClose(find);
    return SL_E_VALUE_NOT_FOUND;
}

static HRESULT find_license_value(UINT size, const BYTE *blob, const SLID *id, const char *name,
                                  const BYTE **value, UINT *value_size)
{
    const BYTE *license_end, *start, *end;
    char attribute[64];

    if (!(start = find_license_element(size, blob, id, &license_end))) return SL_E_VALUE_NOT_FOUND;
    if (snprintf(attribute, sizeof(attribute), "name=\"%s\"", name) >= sizeof(attribute)) return E_INVALIDARG;
    if (!(start = find_bytes(start, license_end, attribute))) return SL_E_VALUE_NOT_FOUND;
    if (!(start = find_bytes(start, license_end, ">"))) return SL_E_INVALID_LICENSE;
    start++;
    if (!(end = find_bytes(start, license_end, "<"))) return SL_E_INVALID_LICENSE;
    *value = start;
    *value_size = end - start;
    return S_OK;
}

static HRESULT get_license_path(const SLID *id, WCHAR *filename)
{
    WCHAR guid[39];
    UINT length;

    length = GetSystemDirectoryW(filename, MAX_PATH);
    if (!length) return HRESULT_FROM_WIN32(GetLastError());
    if (length >= MAX_PATH - ARRAY_SIZE(L"\\spp\\store\\2.0\\licenses\\") - ARRAY_SIZE(guid) -
                  ARRAY_SIZE(L".xrm-ms"))
        return HRESULT_FROM_WIN32(ERROR_FILENAME_EXCED_RANGE);

    format_license_id(id, guid);
    wcscat(filename, L"\\spp\\store\\2.0\\licenses\\");
    wcscat(filename, guid);
    wcscat(filename, L".xrm-ms");
    return S_OK;
}

static HRESULT store_license(UINT size, const BYTE *blob, const SLID *id)
{
    WCHAR directory[MAX_PATH], filename[MAX_PATH], temporary[MAX_PATH];
    DWORD error, written;
    HANDLE file;
    HRESULT hr;
    UINT length;

    length = GetSystemDirectoryW(directory, ARRAY_SIZE(directory));
    if (!length) return HRESULT_FROM_WIN32(GetLastError());
    if (length >= ARRAY_SIZE(directory)) return HRESULT_FROM_WIN32(ERROR_FILENAME_EXCED_RANGE);

    if (FAILED(hr = append_store_directory(directory, L"\\spp")) ||
        FAILED(hr = append_store_directory(directory, L"\\store")) ||
        FAILED(hr = append_store_directory(directory, L"\\2.0")) ||
        FAILED(hr = append_store_directory(directory, L"\\licenses"))) return hr;

    if (FAILED(hr = get_license_path(id, filename))) return hr;

    if (!GetTempFileNameW(directory, L"slc", 0, temporary)) return HRESULT_FROM_WIN32(GetLastError());
    file = CreateFileW(temporary, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE)
    {
        error = GetLastError();
        DeleteFileW(temporary);
        return HRESULT_FROM_WIN32(error);
    }

    if (!WriteFile(file, blob, size, &written, NULL) || written != size || !FlushFileBuffers(file))
    {
        error = GetLastError();
        if (!error) error = ERROR_WRITE_FAULT;
        CloseHandle(file);
        DeleteFileW(temporary);
        return HRESULT_FROM_WIN32(error);
    }
    CloseHandle(file);

    if (!MoveFileExW(temporary, filename, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
    {
        error = GetLastError();
        DeleteFileW(temporary);
        return HRESULT_FROM_WIN32(error);
    }

    return S_OK;
}

HRESULT WINAPI SLGetLicensingStatusInformation(HSLC handle, const SLID *app, const SLID *product,
                                               LPCWSTR name, UINT *count, SL_LICENSING_STATUS **status)
{
    FIXME("(%p %p %p %s %p %p) stub\n", handle, app, product, debugstr_w(name), count, status );

    return SL_E_RIGHT_NOT_CONSUMED;
}

HRESULT WINAPI SLGetLicenseInformation(HSLC handle, const SLID *id, PCWSTR name, SLDATATYPE *type,
                                       UINT *size, PBYTE *value)
{
    const BYTE *data;
    const char *field;
    BYTE *blob;
    WCHAR *string;
    UINT blob_size, data_size;
    HRESULT hr;
    int length;

    TRACE("(%p %s %s %p %p %p)\n", handle, wine_dbgstr_guid(id), debugstr_w(name), type, size, value);

    if (!handle || !id || !name || !size || !value) return E_INVALIDARG;
    if (type) *type = SL_DATA_NONE;
    *size = 0;
    *value = NULL;

    if (!wcscmp(name, L"LicenseType")) field = "licenseType";
    else if (!wcscmp(name, L"Version")) field = "licenseVersion";
    else if (!wcscmp(name, L"Description")) field = "description";
    else return SL_E_VALUE_NOT_FOUND;

    if (FAILED(hr = load_license_blob(id, &blob, &blob_size))) return hr;
    hr = find_license_value(blob_size, blob, id, field, &data, &data_size);
    if (hr == SL_E_VALUE_NOT_FOUND && !wcscmp(name, L"Description"))
        hr = find_license_value(blob_size, blob, id, "productDescription", &data, &data_size);
    if (FAILED(hr))
    {
        HeapFree(GetProcessHeap(), 0, blob);
        return hr;
    }
    if (data_size > INT_MAX)
    {
        HeapFree(GetProcessHeap(), 0, blob);
        return SL_E_INVALID_LICENSE;
    }
    length = data_size ? MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, (const char *)data, data_size,
                                              NULL, 0) : 0;
    if (data_size && !length)
    {
        hr = HRESULT_FROM_WIN32(GetLastError());
        HeapFree(GetProcessHeap(), 0, blob);
        return hr;
    }
    if (!(string = LocalAlloc(LMEM_FIXED, (length + 1) * sizeof(*string))))
    {
        HeapFree(GetProcessHeap(), 0, blob);
        return E_OUTOFMEMORY;
    }
    if (length) MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, (const char *)data, data_size, string, length);
    string[length] = 0;
    HeapFree(GetProcessHeap(), 0, blob);

    if (type) *type = SL_DATA_SZ;
    *size = (length + 1) * sizeof(*string);
    *value = (BYTE *)string;
    return S_OK;
}

HRESULT WINAPI SLGetSLIDList(HSLC handle, SLIDTYPE query_type, const SLID *query_id, SLIDTYPE return_type,
                             UINT *count, SLID **ids)
{
    LARGE_INTEGER file_size;
    WCHAR filename[MAX_PATH];
    UINT offset, found, i;
    BYTE *blob;
    DWORD read;
    HANDLE file;
    SLID parsed, *list;
    HRESULT hr;

    TRACE("(%p %u %s %u %p %p)\n", handle, query_type, wine_dbgstr_guid(query_id), return_type, count, ids);

    if (!handle || query_type >= SL_ID_LAST || return_type >= SL_ID_LAST || !count || !ids) return E_INVALIDARG;
    *count = 0;
    *ids = NULL;
    if (query_type != SL_ID_LICENSE_FILE || !query_id || return_type != SL_ID_LICENSE) return SL_E_NOT_SUPPORTED;

    if (FAILED(hr = get_license_path(query_id, filename))) return hr;
    file = CreateFileW(filename, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    if (file == INVALID_HANDLE_VALUE)
    {
        if (GetLastError() == ERROR_FILE_NOT_FOUND || GetLastError() == ERROR_PATH_NOT_FOUND)
            return SL_E_LICENSE_FILE_NOT_INSTALLED;
        return HRESULT_FROM_WIN32(GetLastError());
    }
    if (!GetFileSizeEx(file, &file_size))
    {
        hr = HRESULT_FROM_WIN32(GetLastError());
        CloseHandle(file);
        return hr;
    }
    if (!file_size.QuadPart || file_size.QuadPart > UINT_MAX)
    {
        CloseHandle(file);
        return SL_E_INVALID_LICENSE;
    }

    if (!(blob = HeapAlloc(GetProcessHeap(), 0, file_size.LowPart)))
    {
        CloseHandle(file);
        return E_OUTOFMEMORY;
    }
    if (!ReadFile(file, blob, file_size.LowPart, &read, NULL) || read != file_size.LowPart)
    {
        hr = HRESULT_FROM_WIN32(GetLastError());
        if (SUCCEEDED(hr)) hr = HRESULT_FROM_WIN32(ERROR_READ_FAULT);
        HeapFree(GetProcessHeap(), 0, blob);
        CloseHandle(file);
        return hr;
    }
    CloseHandle(file);

    for (offset = found = 0; SUCCEEDED(find_license_id(read, blob, &offset, &parsed)); found++);
    if (!found)
    {
        HeapFree(GetProcessHeap(), 0, blob);
        return SL_E_VALUE_NOT_FOUND;
    }
    if (!(list = LocalAlloc(LMEM_FIXED, found * sizeof(*list))))
    {
        HeapFree(GetProcessHeap(), 0, blob);
        return E_OUTOFMEMORY;
    }

    offset = 0;
    for (i = 0; i < found; i++) find_license_id(read, blob, &offset, list + i);
    HeapFree(GetProcessHeap(), 0, blob);
    *count = found;
    *ids = list;
    return S_OK;
}

HRESULT WINAPI SLOpen(HSLC *handle)
{
    FIXME("(%p) stub\n", handle );

    if (!handle)
        return E_INVALIDARG;

    *handle = (HSLC)0xdeadbeef;

    return S_OK;
}

HRESULT WINAPI SLClose(HSLC handle)
{
    FIXME("(%p) stub\n", handle );

    return S_OK;
}

HRESULT WINAPI SLInstallLicense(HSLC handle, UINT size, const BYTE *blob, SLID *file_id)
{
    SLID id;
    HRESULT hr;
    UINT offset = 0;

    TRACE("(%p %u %p %p)\n", handle, size, blob, file_id);

    if (!handle || !size || !blob || !file_id) return E_INVALIDARG;
    if ((hr = find_license_id(size, blob, &offset, &id)) == SL_E_VALUE_NOT_FOUND) return SL_E_INVALID_LICENSE;
    if (FAILED(hr)) return hr;
    if (FAILED(hr = store_license(size, blob, &id))) return hr;

    *file_id = id;
    return S_OK;
}

HRESULT WINAPI SLPersistApplicationPolicies(const SLID *app, const SLID *product, DWORD flags)
{
    FIXME("(%s,%s,%lx) stub\n", wine_dbgstr_guid(app), wine_dbgstr_guid(product), flags);

    if (!app)
        return E_INVALIDARG;

    return S_OK;
}
