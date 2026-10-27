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
#include "windef.h"
#include "winbase.h"
#include "wincrypt.h"
#include "winnls.h"
#include "winreg.h"
#include "winternl.h"
#include "wine/debug.h"

#include "slpublic.h"
#include "slerror.h"

WINE_DEFAULT_DEBUG_CHANNEL(slc);

#define SLC_CONTEXT_MAGIC 0x534c4343
#define AUTH_MARKER       0x00010000
#define AUTH_RESULT_VER   2
#define AUTH_HMAC_LEN     20
#define AUTH_MAX_KEY_LEN  64

struct slc_context
{
    DWORD magic;
    UINT auth_size;
    BYTE *auth_data;
    BYTE session_key[AUTH_MAX_KEY_LEN];
    DWORD session_key_len;
    WCHAR *last_name;
    SLDATATYPE last_type;
    BYTE *last_value;
    UINT last_size;
    BOOL rights_consumed;
    SLID app;
    SLID product;
    DWORD grace_minutes;
    DWORD grace_days;
};

static BYTE pending_session_key[AUTH_MAX_KEY_LEN];
static DWORD pending_session_key_len;
static BYTE pending_expected_hmac[AUTH_HMAC_LEN];
static DWORD pending_expected_hmac_len;
static CRITICAL_SECTION pending_auth_cs;
static CRITICAL_SECTION_DEBUG pending_auth_cs_debug =
{
    0, 0, &pending_auth_cs,
    { &pending_auth_cs_debug.ProcessLocksList, &pending_auth_cs_debug.ProcessLocksList },
    0, 0, { (DWORD_PTR)(__FILE__ ": pending_auth_cs") }
};
static CRITICAL_SECTION pending_auth_cs = { &pending_auth_cs_debug, -1, 0, 0, 0, 0 };

void CDECL __wine_sppc_set_auth_session_key(const BYTE *key, DWORD length)
{
    if (!key || !length || length > AUTH_MAX_KEY_LEN) return;

    EnterCriticalSection(&pending_auth_cs);
    memcpy(pending_session_key, key, length);
    pending_session_key_len = length;
    LeaveCriticalSection(&pending_auth_cs);
}

void CDECL __wine_sppc_set_expected_hmac(const BYTE *hmac, DWORD length)
{
    if (!hmac || length != AUTH_HMAC_LEN) return;

    EnterCriticalSection(&pending_auth_cs);
    memcpy(pending_expected_hmac, hmac, AUTH_HMAC_LEN);
    pending_expected_hmac_len = AUTH_HMAC_LEN;
    LeaveCriticalSection(&pending_auth_cs);
}

static struct slc_context *get_slc_context(HSLC handle)
{
    struct slc_context *context = handle;

    if (!context || context->magic != SLC_CONTEXT_MAGIC) return NULL;
    return context;
}

static void clear_last_policy(struct slc_context *context)
{
    LocalFree(context->last_name);
    LocalFree(context->last_value);
    context->last_name = NULL;
    context->last_value = NULL;
    context->last_size = 0;
    context->last_type = SL_DATA_NONE;
}

static void remember_policy(struct slc_context *context, const WCHAR *name, SLDATATYPE type,
                            const BYTE *value, UINT size)
{
    WCHAR *name_copy;
    BYTE *value_copy;
    SIZE_T name_size;

    if (!context || !name || !value || !size) return;
    name_size = (wcslen(name) + 1) * sizeof(*name);
    if (!(name_copy = LocalAlloc(LMEM_FIXED, name_size))) return;
    if (!(value_copy = LocalAlloc(LMEM_FIXED, size)))
    {
        LocalFree(name_copy);
        return;
    }

    memcpy(name_copy, name, name_size);
    memcpy(value_copy, value, size);
    clear_last_policy(context);
    context->last_name = name_copy;
    context->last_type = type;
    context->last_value = value_copy;
    context->last_size = size;
}

static BOOL hmac_sha1(const BYTE *key, DWORD key_len, const BYTE *data, DWORD data_len, BYTE *digest)
{
    struct
    {
        BLOBHEADER header;
        DWORD key_len;
        BYTE key[AUTH_MAX_KEY_LEN];
    } blob;
    HCRYPTPROV provider = 0;
    HCRYPTKEY hkey = 0;
    HCRYPTHASH hash = 0;
    HMAC_INFO info;
    DWORD digest_len = AUTH_HMAC_LEN;
    BOOL ret = FALSE;

    if (!key || !key_len || key_len > AUTH_MAX_KEY_LEN) return FALSE;
    if (!CryptAcquireContextW(&provider, NULL, NULL, PROV_RSA_AES, CRYPT_VERIFYCONTEXT)) return FALSE;

    blob.header.bType = PLAINTEXTKEYBLOB;
    blob.header.bVersion = CUR_BLOB_VERSION;
    blob.header.reserved = 0;
    blob.header.aiKeyAlg = key_len == 32 ? CALG_AES_256 : CALG_RC2;
    blob.key_len = key_len;
    memcpy(blob.key, key, key_len);
    if (!CryptImportKey(provider, (BYTE *)&blob, sizeof(blob.header) + sizeof(blob.key_len) + key_len,
                        0, 0, &hkey)) goto done;
    if (!CryptCreateHash(provider, CALG_HMAC, hkey, 0, &hash)) goto done;

    memset(&info, 0, sizeof(info));
    info.HashAlgid = CALG_SHA1;
    if (!CryptSetHashParam(hash, HP_HMAC_INFO, (BYTE *)&info, 0)) goto done;
    if (!CryptHashData(hash, data, data_len, 0)) goto done;
    if (!CryptGetHashParam(hash, HP_HASHVAL, digest, &digest_len, 0)) goto done;
    ret = digest_len == AUTH_HMAC_LEN;

done:
    if (hash) CryptDestroyHash(hash);
    if (hkey) CryptDestroyKey(hkey);
    if (provider) CryptReleaseContext(provider, 0);
    return ret;
}

static HRESULT build_authentication_result(struct slc_context *context, UINT *size, BYTE **value)
{
    BYTE digest[AUTH_HMAC_LEN], *message = NULL, *result, *cursor;
    DWORD marker = AUTH_MARKER, type, name_size, message_size;
    BOOL have_digest = FALSE;
    UINT result_size;

    EnterCriticalSection(&pending_auth_cs);
    if (pending_expected_hmac_len == AUTH_HMAC_LEN)
    {
        memcpy(digest, pending_expected_hmac, AUTH_HMAC_LEN);
        pending_expected_hmac_len = 0;
        have_digest = TRUE;
    }
    LeaveCriticalSection(&pending_auth_cs);

    if (!have_digest && context->session_key_len && context->last_name && context->last_value)
    {
        type = context->last_type;
        name_size = wcslen(context->last_name) * sizeof(*context->last_name);
        if (context->last_size > UINT_MAX - sizeof(marker) - sizeof(type) - name_size)
            return HRESULT_FROM_WIN32(ERROR_ARITHMETIC_OVERFLOW);
        message_size = sizeof(marker) + sizeof(type) + context->last_size + name_size;
        if (!(message = LocalAlloc(LMEM_FIXED, message_size))) return E_OUTOFMEMORY;

        cursor = message;
        memcpy(cursor, &marker, sizeof(marker));
        cursor += sizeof(marker);
        memcpy(cursor, &type, sizeof(type));
        cursor += sizeof(type);
        memcpy(cursor, context->last_value, context->last_size);
        cursor += context->last_size;
        memcpy(cursor, context->last_name, name_size);
        have_digest = hmac_sha1(context->session_key, context->session_key_len, message,
                                message_size, digest);
        LocalFree(message);
    }
    if (!have_digest) return SL_E_AUTHN_CANT_VERIFY;

    result_size = 3 * sizeof(DWORD) + AUTH_HMAC_LEN;
    if (!(result = LocalAlloc(LMEM_FIXED, result_size))) return E_OUTOFMEMORY;
    ((DWORD *)result)[0] = result_size;
    ((DWORD *)result)[1] = AUTH_RESULT_VER;
    ((DWORD *)result)[2] = AUTH_HMAC_LEN;
    memcpy(result + 3 * sizeof(DWORD), digest, AUTH_HMAC_LEN);
    *size = result_size;
    *value = result;
    return S_OK;
}

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

struct grace_license
{
    SLID app;
    SLID product;
    DWORD minutes;
    DWORD days;
    WCHAR path[MAX_PATH];
};

static HRESULT find_xml_value(const BYTE *start, const BYTE *end, const char *name,
                              const BYTE **value, UINT *value_size)
{
    char attribute[64];
    const BYTE *close;

    if (snprintf(attribute, sizeof(attribute), "name=\"%s\"", name) >= sizeof(attribute))
        return E_INVALIDARG;
    if (!(start = find_bytes(start, end, attribute))) return SL_E_VALUE_NOT_FOUND;
    if (!(start = find_bytes(start, end, ">"))) return SL_E_INVALID_LICENSE;
    start++;
    if (!(close = find_bytes(start, end, "<"))) return SL_E_INVALID_LICENSE;
    *value = start;
    *value_size = close - start;
    return S_OK;
}

static HRESULT parse_ascii_guid(const BYTE *value, UINT value_size, SLID *id)
{
    WCHAR string[39];
    UNICODE_STRING stringW;
    UINT i, offset;

    if (value_size == 38 && value[0] == '{' && value[37] == '}') offset = 0;
    else if (value_size == 36) offset = 1;
    else return SL_E_INVALID_LICENSE;

    if (offset) string[0] = '{';
    for (i = 0; i < value_size; i++) string[i + offset] = value[i];
    if (offset) string[37] = '}';
    string[38] = 0;
    RtlInitUnicodeString(&stringW, string);
    return RtlGUIDFromString(&stringW, id) ? SL_E_INVALID_LICENSE : S_OK;
}

static HRESULT parse_xml_guid(const BYTE *start, const BYTE *end, const char *name, SLID *id)
{
    const BYTE *value;
    UINT value_size;
    HRESULT hr;

    if (FAILED(hr = find_xml_value(start, end, name, &value, &value_size))) return hr;
    return parse_ascii_guid(value, value_size, id);
}

static BOOL parse_grace_duration(const BYTE *start, const BYTE *end, DWORD *days, DWORD *minutes)
{
    const BYTE *duration;
    DWORD value = 0;

    if (!(duration = find_bytes(start, end, "<sl:duration>P"))) return FALSE;
    duration += strlen("<sl:duration>P");
    if (duration == end || *duration < '0' || *duration > '9') return FALSE;
    while (duration < end && *duration >= '0' && *duration <= '9')
    {
        if (value > (UINT_MAX - (*duration - '0')) / 10) return FALSE;
        value = value * 10 + (*duration++ - '0');
    }
    if (duration == end || *duration != 'D' || value > UINT_MAX / (24 * 60)) return FALSE;
    *days = value;
    *minutes = value * 24 * 60;
    return TRUE;
}

static HRESULT find_configured_grace_product(SLID *product)
{
    static const WCHAR config_key[] = L"Software\\Microsoft\\Office\\ClickToRun\\Configuration";
    WCHAR product_ids[512], installation[MAX_PATH], map_path[MAX_PATH];
    const BYTE *section, *section_end, *license, *tag_end, *sku, *sku_end, *acid, *acid_end;
    char product_id[256], product_tag[320];
    BYTE *blob;
    DWORD reg_size;
    UINT blob_size, i, length;
    HRESULT hr;

    reg_size = sizeof(product_ids);
    if (RegGetValueW(HKEY_LOCAL_MACHINE, config_key, L"ProductReleaseIds", RRF_RT_REG_SZ,
                     NULL, product_ids, &reg_size)) return SL_E_VALUE_NOT_FOUND;
    reg_size = sizeof(installation);
    if (RegGetValueW(HKEY_LOCAL_MACHINE, config_key, L"InstallationPath", RRF_RT_REG_SZ,
                     NULL, installation, &reg_size)) return SL_E_VALUE_NOT_FOUND;

    length = wcslen(installation);
    while (length && (installation[length - 1] == '\\' || installation[length - 1] == '/'))
        installation[--length] = 0;
    if (length + ARRAY_SIZE(L"\\root\\Licenses16\\c2rpridslicensefiles_auto.xml") >= ARRAY_SIZE(map_path))
        return HRESULT_FROM_WIN32(ERROR_FILENAME_EXCED_RANGE);
    wcscpy(map_path, installation);
    wcscat(map_path, L"\\root\\Licenses16\\c2rpridslicensefiles_auto.xml");
    if (FAILED(hr = read_license_file(map_path, &blob, &blob_size))) return hr;

    for (i = 0; product_ids[i]; i += length)
    {
        while (product_ids[i] == ',' || product_ids[i] == ';' || product_ids[i] == ' ') i++;
        length = 0;
        while (product_ids[i + length] && product_ids[i + length] != ',' &&
               product_ids[i + length] != ';' && product_ids[i + length] != ' ') length++;
        if (!length) break;
        if (length >= ARRAY_SIZE(product_id)) continue;
        if (WideCharToMultiByte(CP_UTF8, 0, product_ids + i, length, product_id,
                                ARRAY_SIZE(product_id) - 1, NULL, NULL) != length) continue;
        product_id[length] = 0;
        if (snprintf(product_tag, sizeof(product_tag), "<ProductReleaseId id=\"%s\">", product_id) >=
            sizeof(product_tag)) continue;

        if (!(section = find_bytes(blob, blob + blob_size, product_tag)) ||
            !(section_end = find_bytes(section, blob + blob_size, "</ProductReleaseId>"))) continue;
        license = section;
        while ((license = find_bytes(license, section_end, "<License Sku=\"")))
        {
            sku = license + strlen("<License Sku=\"");
            if (!(sku_end = find_bytes(sku, section_end, "\"")) ||
                !(tag_end = find_bytes(sku_end, section_end, ">"))) break;
            if (!find_bytes_i(sku, sku_end, "Grace"))
            {
                license = tag_end + 1;
                continue;
            }
            if (!(acid = find_bytes(sku_end, tag_end, "Acid=\"")))
            {
                license = tag_end + 1;
                continue;
            }
            acid += strlen("Acid=\"");
            if (!(acid_end = find_bytes(acid, tag_end, "\""))) break;
            hr = parse_ascii_guid(acid, acid_end - acid, product);
            HeapFree(GetProcessHeap(), 0, blob);
            return hr;
        }
    }

    HeapFree(GetProcessHeap(), 0, blob);
    return SL_E_VALUE_NOT_FOUND;
}

static HRESULT find_grace_license(const SLID *app, const SLID *product, struct grace_license *license)
{
    WIN32_FIND_DATAW data;
    WCHAR search[MAX_PATH], *name;
    HANDLE find;
    UINT length;
    SLID configured_product;

    if (!product && SUCCEEDED(find_configured_grace_product(&configured_product))) product = &configured_product;

    length = GetSystemDirectoryW(search, ARRAY_SIZE(search));
    if (!length) return HRESULT_FROM_WIN32(GetLastError());
    if (length >= ARRAY_SIZE(search) - ARRAY_SIZE(L"\\spp\\store\\2.0\\licenses\\*.xrm-ms"))
        return HRESULT_FROM_WIN32(ERROR_FILENAME_EXCED_RANGE);
    wcscat(search, L"\\spp\\store\\2.0\\licenses\\*.xrm-ms");
    if ((find = FindFirstFileW(search, &data)) == INVALID_HANDLE_VALUE) return SL_E_VALUE_NOT_FOUND;
    name = wcsrchr(search, L'\\') + 1;

    do
    {
        const BYTE *start, *end, *limit;
        BYTE *blob;
        UINT size;

        if (data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;
        if (name - search + wcslen(data.cFileName) >= ARRAY_SIZE(search)) continue;
        wcscpy(name, data.cFileName);
        if (FAILED(read_license_file(search, &blob, &size))) continue;

        limit = blob + size;
        start = blob;
        while ((start = find_bytes(start, limit, "<r:license")))
        {
            SLID candidate_app, candidate_product;
            const BYTE *tag_end;
            DWORD days, minutes;

            if (!(tag_end = find_bytes(start, limit, ">")) ||
                !(end = find_bytes(tag_end + 1, limit, "</r:license>"))) break;
            start = tag_end + 1;
            if (find_bytes(start, end, "msft:sl/grace-period") &&
                SUCCEEDED(parse_xml_guid(start, end, "applicationId", &candidate_app)) &&
                IsEqualGUID(app, &candidate_app) &&
                SUCCEEDED(parse_xml_guid(start, end, "productSkuId", &candidate_product)) &&
                (!product || IsEqualGUID(product, &candidate_product)) &&
                parse_grace_duration(start, end, &days, &minutes))
            {
                license->app = candidate_app;
                license->product = candidate_product;
                license->days = days;
                license->minutes = minutes;
                wcscpy(license->path, search);
                HeapFree(GetProcessHeap(), 0, blob);
                FindClose(find);
                return S_OK;
            }
            start = end + strlen("</r:license>");
        }
        HeapFree(GetProcessHeap(), 0, blob);
    } while (FindNextFileW(find, &data));

    FindClose(find);
    return SL_E_VALUE_NOT_FOUND;
}

static HRESULT get_grace_license_value(const SLID *app, const SLID *product, const char *name,
                                       SLDATATYPE *type, UINT *size, PBYTE *value)
{
    struct grace_license license;
    const BYTE *start, *end, *limit, *data;
    BYTE *blob;
    WCHAR *string;
    UINT blob_size, data_size;
    HRESULT hr;
    int length;

    if (FAILED(hr = find_grace_license(app, product, &license))) return hr;
    if (FAILED(hr = read_license_file(license.path, &blob, &blob_size))) return hr;

    limit = blob + blob_size;
    start = blob;
    hr = SL_E_VALUE_NOT_FOUND;
    while ((start = find_bytes(start, limit, "<r:license")))
    {
        SLID candidate_app, candidate_product;
        const BYTE *tag_end;

        if (!(tag_end = find_bytes(start, limit, ">")) ||
            !(end = find_bytes(tag_end + 1, limit, "</r:license>"))) break;
        start = tag_end + 1;
        if (find_bytes(start, end, "msft:sl/grace-period") &&
            SUCCEEDED(parse_xml_guid(start, end, "applicationId", &candidate_app)) &&
            IsEqualGUID(app, &candidate_app) &&
            SUCCEEDED(parse_xml_guid(start, end, "productSkuId", &candidate_product)) &&
            IsEqualGUID(product, &candidate_product))
        {
            hr = find_xml_value(start, end, name, &data, &data_size);
            break;
        }
        start = end + strlen("</r:license>");
    }
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
    length = data_size ? MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, (const char *)data,
                                              data_size, NULL, 0) : 0;
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

static HRESULT get_product_policy(const struct slc_context *context, PCWSTR name, SLDATATYPE *type,
                                  UINT *size, PBYTE *value)
{
    WIN32_FIND_DATAW data;
    WCHAR search[MAX_PATH], *filename;
    char nameA[192], pattern[256], guid[39];
    HANDLE find;
    UINT length;

    if ((length = WideCharToMultiByte(CP_UTF8, 0, name, -1, nameA, ARRAY_SIZE(nameA), NULL, NULL)) <= 1)
        return SL_E_VALUE_NOT_FOUND;
    format_license_idA(&context->product, guid);

    length = GetSystemDirectoryW(search, ARRAY_SIZE(search));
    if (!length) return HRESULT_FROM_WIN32(GetLastError());
    if (length >= ARRAY_SIZE(search) - ARRAY_SIZE(L"\\spp\\store\\2.0\\licenses\\*.xrm-ms"))
        return HRESULT_FROM_WIN32(ERROR_FILENAME_EXCED_RANGE);
    wcscat(search, L"\\spp\\store\\2.0\\licenses\\*.xrm-ms");
    if ((find = FindFirstFileW(search, &data)) == INVALID_HANDLE_VALUE) return SL_E_VALUE_NOT_FOUND;
    filename = wcsrchr(search, L'\\') + 1;

    do
    {
        const BYTE *start, *end, *limit;
        SLID app;
        BYTE *blob;
        UINT blob_size;

        if (data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;
        if (filename - search + wcslen(data.cFileName) >= ARRAY_SIZE(search)) continue;
        wcscpy(filename, data.cFileName);
        if (FAILED(read_license_file(search, &blob, &blob_size))) continue;
        limit = blob + blob_size;
        if (!find_bytes_i(blob, limit, "msft:sl/PPD") || !find_bytes_i(blob, limit, guid + 1) ||
            FAILED(parse_xml_guid(blob, limit, "applicationId", &app)) || !IsEqualGUID(&app, &context->app))
        {
            HeapFree(GetProcessHeap(), 0, blob);
            continue;
        }

        if (snprintf(pattern, sizeof(pattern), "<sl:policyInt name=\"%s\"", nameA) < sizeof(pattern) &&
            (start = find_bytes(blob, limit, pattern)) && (start = find_bytes(start, limit, ">")) &&
            (end = find_bytes(++start, limit, "<")))
        {
            DWORD number = 0, *copy;

            if (start == end) goto invalid_policy;
            while (start < end && *start >= '0' && *start <= '9')
            {
                if (number > (UINT_MAX - (*start - '0')) / 10) goto invalid_policy;
                number = number * 10 + (*start++ - '0');
            }
            if (start != end) goto invalid_policy;
            if (!(copy = LocalAlloc(LMEM_FIXED, sizeof(*copy))))
            {
                HeapFree(GetProcessHeap(), 0, blob);
                FindClose(find);
                return E_OUTOFMEMORY;
            }
            *copy = number;
            if (type) *type = SL_DATA_DWORD;
            *size = sizeof(*copy);
            *value = (BYTE *)copy;
            HeapFree(GetProcessHeap(), 0, blob);
            FindClose(find);
            return S_OK;
        }

        if (snprintf(pattern, sizeof(pattern), "<sl:policyStr name=\"%s\"", nameA) < sizeof(pattern) &&
            (start = find_bytes(blob, limit, pattern)) && (start = find_bytes(start, limit, ">")) &&
            (end = find_bytes(++start, limit, "<")))
        {
            WCHAR *copy;
            int chars;

            chars = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, (const char *)start,
                                        end - start, NULL, 0);
            if (!chars && start != end) goto invalid_policy;
            if (!(copy = LocalAlloc(LMEM_FIXED, (chars + 1) * sizeof(*copy))))
            {
                HeapFree(GetProcessHeap(), 0, blob);
                FindClose(find);
                return E_OUTOFMEMORY;
            }
            if (chars) MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, (const char *)start,
                                           end - start, copy, chars);
            copy[chars] = 0;
            if (type) *type = SL_DATA_SZ;
            *size = (chars + 1) * sizeof(*copy);
            *value = (BYTE *)copy;
            HeapFree(GetProcessHeap(), 0, blob);
            FindClose(find);
            return S_OK;
        }

        HeapFree(GetProcessHeap(), 0, blob);
        FindClose(find);
        return SL_E_VALUE_NOT_FOUND;

invalid_policy:
        HeapFree(GetProcessHeap(), 0, blob);
        FindClose(find);
        return SL_E_INVALID_LICENSE;
    } while (FindNextFileW(find, &data));

    FindClose(find);
    return SL_E_VALUE_NOT_FOUND;
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
    struct slc_context *context = get_slc_context(handle);
    struct grace_license license;
    SL_LICENSING_STATUS *entry;
    HRESULT hr;

    TRACE("(%p %s %s %s %p %p)\n", handle, wine_dbgstr_guid(app), wine_dbgstr_guid(product),
          debugstr_w(name), count, status);

    if (!context || !count || !status) return E_INVALIDARG;
    *count = 0;
    *status = NULL;

    if (context->rights_consumed)
    {
        license.app = context->app;
        license.product = context->product;
        license.minutes = context->grace_minutes;
        license.days = context->grace_days;
    }
    else
    {
        if (!app) return SL_E_RIGHT_NOT_CONSUMED;
        if (FAILED(hr = find_grace_license(app, product, &license))) return hr;
    }

    if ((app && !IsEqualGUID(app, &license.app)) ||
        (product && !IsEqualGUID(product, &license.product))) return SL_E_VALUE_NOT_FOUND;
    if (!(entry = LocalAlloc(LMEM_FIXED | LMEM_ZEROINIT, sizeof(*entry)))) return E_OUTOFMEMORY;

    entry->SkuId = license.product;
    entry->eStatus = SL_LICENSING_STATUS_IN_GRACE_PERIOD;
    entry->dwGraceTime = license.minutes;
    entry->dwTotalGraceDays = license.days;
    entry->hrReason = SL_I_OOB_GRACE_PERIOD;
    *count = 1;
    *status = entry;
    return S_OK;
}

HRESULT WINAPI SLGetProductSkuInformation(HSLC handle, const SLID *product, PCWSTR name,
                                          SLDATATYPE *type, UINT *size, PBYTE *value)
{
    struct slc_context *context = get_slc_context(handle);
    const char *field;

    TRACE("(%p %s %s %p %p %p)\n", handle, wine_dbgstr_guid(product), debugstr_w(name),
          type, size, value);

    if (!context || !product || !name || !size || !value) return E_INVALIDARG;
    if (type) *type = SL_DATA_NONE;
    *size = 0;
    *value = NULL;
    if (!context->rights_consumed || !IsEqualGUID(product, &context->product)) return SL_E_VALUE_NOT_FOUND;

    if (!wcsicmp(name, L"Name")) field = "productName";
    else if (!wcsicmp(name, L"Description")) field = "productDescription";
    else if (!wcsicmp(name, L"Author")) field = "productAuthor";
    else if (!wcsicmp(name, L"ApplicationBitmap")) field = "ApplicationBitmap";
    else if (!wcsicmp(name, L"UXDifferentiator")) field = "UXDifferentiator";
    else return SL_E_VALUE_NOT_FOUND;

    return get_grace_license_value(&context->app, product, field, type, size, value);
}

HRESULT WINAPI SLGetPolicyInformation(HSLC handle, PCWSTR name, SLDATATYPE *type,
                                      UINT *size, PBYTE *value)
{
    struct slc_context *context = get_slc_context(handle);
    DWORD *policy;
    HRESULT hr;

    TRACE("(%p %s %p %p %p)\n", handle, debugstr_w(name), type, size, value);

    if (!context || !name || !size || !value) return E_INVALIDARG;
    if (type) *type = SL_DATA_NONE;
    *size = 0;
    *value = NULL;
    if (wcscmp(name, L"*"))
    {
        if (!wcsicmp(name, L"office-ParentCode")) return SL_E_VALUE_NOT_FOUND;
        if (!context->rights_consumed) return SL_E_RIGHT_NOT_GRANTED;
        hr = get_product_policy(context, name, type, size, value);
        if (SUCCEEDED(hr)) remember_policy(context, name, type ? *type : SL_DATA_NONE, *value, *size);
        return hr;
    }
    if (!context->rights_consumed) return SL_E_RIGHT_NOT_GRANTED;
    if (!(policy = LocalAlloc(LMEM_FIXED, sizeof(*policy)))) return E_OUTOFMEMORY;

    *policy = 1;
    if (type) *type = SL_DATA_DWORD;
    *size = sizeof(*policy);
    *value = (BYTE *)policy;
    remember_policy(context, name, SL_DATA_DWORD, *value, *size);
    return S_OK;
}

HRESULT WINAPI SLGetServiceInformation(HSLC handle, PCWSTR name, SLDATATYPE *type,
                                       UINT *size, PBYTE *value)
{
    TRACE("(%p %s %p %p %p)\n", handle, debugstr_w(name), type, size, value);

    if (!get_slc_context(handle) || !name || !size || !value) return E_INVALIDARG;
    if (type) *type = SL_DATA_NONE;
    *size = 0;
    *value = NULL;
    return SL_E_VALUE_NOT_FOUND;
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
    struct slc_context *context;

    TRACE("(%p)\n", handle);

    if (!handle) return E_INVALIDARG;
    if (!(context = LocalAlloc(LMEM_FIXED | LMEM_ZEROINIT, sizeof(*context)))) return E_OUTOFMEMORY;

    context->magic = SLC_CONTEXT_MAGIC;
    *handle = context;
    return S_OK;
}

HRESULT WINAPI SLClose(HSLC handle)
{
    struct slc_context *context = get_slc_context(handle);

    TRACE("(%p)\n", handle);

    if (!context) return E_INVALIDARG;
    context->magic = 0;
    clear_last_policy(context);
    if (context->session_key_len) SecureZeroMemory(context->session_key, context->session_key_len);
    LocalFree(context->auth_data);
    LocalFree(context);
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

HRESULT WINAPI SLLoadApplicationPolicies(const SLID *app, const SLID *product, DWORD flags, void **policy)
{
    TRACE("(%s,%s,%lx,%p)\n", wine_dbgstr_guid(app), wine_dbgstr_guid(product), flags, policy);

    if (!app || !policy) return E_INVALIDARG;
    *policy = NULL;
    return SL_E_APPLICATION_POLICIES_MISSING;
}

HRESULT WINAPI SLSetAuthenticationData(HSLC handle, UINT size, const BYTE *value)
{
    struct slc_context *context = get_slc_context(handle);
    BYTE *copy;

    TRACE("(%p,%u,%p)\n", handle, size, value);

    if (!context || (size && !value)) return E_INVALIDARG;
    if (size > 1024) return HRESULT_FROM_WIN32(ERROR_BUFFER_OVERFLOW);
    if (!size)
    {
        LocalFree(context->auth_data);
        context->auth_data = NULL;
        context->auth_size = 0;
        if (context->session_key_len) SecureZeroMemory(context->session_key, context->session_key_len);
        context->session_key_len = 0;
        clear_last_policy(context);
        return S_OK;
    }

    if (!(copy = LocalAlloc(LMEM_FIXED, size))) return E_OUTOFMEMORY;
    memcpy(copy, value, size);
    LocalFree(context->auth_data);
    context->auth_data = copy;
    context->auth_size = size;

    EnterCriticalSection(&pending_auth_cs);
    if (pending_session_key_len)
    {
        memcpy(context->session_key, pending_session_key, pending_session_key_len);
        context->session_key_len = pending_session_key_len;
        SecureZeroMemory(pending_session_key, pending_session_key_len);
        pending_session_key_len = 0;
    }
    LeaveCriticalSection(&pending_auth_cs);
    return S_OK;
}

HRESULT WINAPI SLGetAuthenticationResult(HSLC handle, UINT *size, BYTE **value)
{
    struct slc_context *context = get_slc_context(handle);

    TRACE("(%p,%p,%p)\n", handle, size, value);

    if (!context || !size || !value) return E_INVALIDARG;
    *size = 0;
    *value = NULL;
    if (!context->auth_data) return SL_E_AUTHN_CHALLENGE_NOT_SET;
    return build_authentication_result(context, size, value);
}

HRESULT WINAPI SLConsumeRight(HSLC handle, const SLID *app, const SLID *product, PCWSTR right, void *reserved)
{
    struct slc_context *context = get_slc_context(handle);
    struct grace_license license;
    HRESULT hr;

    TRACE("(%p,%s,%s,%s,%p)\n", handle, wine_dbgstr_guid(app), wine_dbgstr_guid(product),
          debugstr_w(right), reserved);

    if (!context || !app || reserved) return E_INVALIDARG;
    if (FAILED(hr = find_grace_license(app, product, &license))) return hr == SL_E_VALUE_NOT_FOUND ?
        SL_E_RIGHT_NOT_GRANTED : hr;

    context->app = license.app;
    context->product = license.product;
    context->grace_minutes = license.minutes;
    context->grace_days = license.days;
    context->rights_consumed = TRUE;
    return S_OK;
}
