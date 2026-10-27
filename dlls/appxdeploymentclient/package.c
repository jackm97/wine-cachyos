/* WinRT Windows.Management.Deployment.PackageManager Implementation
 *
 * Copyright (C) 2023 Mohamad Al-Jaf
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
#include "initguid.h"
#include "shlwapi.h"
#include "xmllite.h"
#include "zlib.h"
#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(appx);

struct package_manager
{
    IPackageManager IPackageManager_iface;
    IPackageManager2 IPackageManager2_iface;
    IPackageManager6 IPackageManager6_iface;
    IPackageManager9 IPackageManager9_iface;
    LONG ref;
};

struct registered_package
{
    IPackage IPackage_iface;
    IPackage3 IPackage3_iface;
    IPackage8 IPackage8_iface;
    LONG ref;
    HSTRING family_name;
    HSTRING name;
    HSTRING publisher;
    HSTRING publisher_id;
    HSTRING full_name;
    HSTRING path;
    HSTRING external_path;
    PackageVersion version;
    __x_ABI_CWindows_CSystem_CProcessorArchitecture architecture;
};

struct registered_package_status
{
    IPackageStatus IPackageStatus_iface;
    LONG ref;
};

struct package_registration
{
    HSTRING family_name;
    HSTRING name;
    HSTRING publisher;
    HSTRING publisher_id;
    HSTRING full_name;
    HSTRING path;
    HSTRING external_path;
    HSTRING executable;
    HSTRING application_id;
    HSTRING application_user_model_id;
    PackageVersion version;
    __x_ABI_CWindows_CSystem_CProcessorArchitecture architecture;
};

struct registered_package_id
{
    IPackageId IPackageId_iface;
    LONG ref;
    HSTRING family_name;
    HSTRING name;
    HSTRING publisher;
    HSTRING publisher_id;
    HSTRING full_name;
    PackageVersion version;
    __x_ABI_CWindows_CSystem_CProcessorArchitecture architecture;
};

struct package_iterator
{
    IIterator_Package IIterator_Package_iface;
    LONG ref;
    IPackage *package;
    boolean has_current;
};

struct package_iterable
{
    IIterable_Package IIterable_Package_iface;
    LONG ref;
    IPackage *package;
};

static SRWLOCK package_registration_lock = SRWLOCK_INIT;
static HSTRING pending_package_family;
static struct package_registration *registered_package;

static HRESULT deployment_operation_create(
        IAsyncOperationWithProgress_DeploymentResult_DeploymentProgress **operation );

static void package_registration_free( struct package_registration *registration )
{
    if (!registration) return;
    WindowsDeleteString( registration->family_name );
    WindowsDeleteString( registration->name );
    WindowsDeleteString( registration->publisher );
    WindowsDeleteString( registration->publisher_id );
    WindowsDeleteString( registration->full_name );
    WindowsDeleteString( registration->path );
    WindowsDeleteString( registration->external_path );
    WindowsDeleteString( registration->executable );
    WindowsDeleteString( registration->application_id );
    WindowsDeleteString( registration->application_user_model_id );
    free( registration );
}

static HRESULT set_sparse_package_environment( const struct package_registration *registration )
{
    static const WCHAR *const names[] =
    {
        L"WINE_SPARSE_PACKAGE_FULL_NAME",
        L"WINE_SPARSE_PACKAGE_FAMILY_NAME",
        L"WINE_SPARSE_PACKAGE_PATH",
        L"WINE_SPARSE_PACKAGE_EXECUTABLE",
        L"WINE_SPARSE_PACKAGE_AUMID",
    };
    HSTRING values[] =
    {
        registration ? registration->full_name : NULL,
        registration ? registration->family_name : NULL,
        registration ? registration->external_path : NULL,
        registration ? registration->executable : NULL,
        registration ? registration->application_user_model_id : NULL,
    };
    unsigned int i;

    for (i = 0; i < ARRAY_SIZE(names); ++i)
    {
        const WCHAR *value = values[i] ? WindowsGetStringRawBuffer( values[i], NULL ) : NULL;

        if (!SetEnvironmentVariableW( names[i], value ))
        {
            HRESULT hr = HRESULT_FROM_WIN32( GetLastError() );

            while (i--) SetEnvironmentVariableW( names[i], NULL );
            return hr;
        }
    }
    return S_OK;
}

static HRESULT uri_to_path( IUriRuntimeClass *uri, HSTRING *value )
{
    HSTRING absolute_uri = NULL;
    const WCHAR *buffer;
    WCHAR *path;
    UINT32 length;
    DWORD path_length;
    HRESULT hr;

    if (!value) return E_POINTER;
    *value = NULL;
    if (!uri) return WindowsCreateString( L"", 0, value );
    if (FAILED(hr = IUriRuntimeClass_get_AbsoluteUri( uri, &absolute_uri ))) return hr;
    buffer = WindowsGetStringRawBuffer( absolute_uri, &length );
    path_length = length + 1;
    if (!(path = malloc( path_length * sizeof(*path) )))
    {
        WindowsDeleteString( absolute_uri );
        return E_OUTOFMEMORY;
    }
    hr = PathCreateFromUrlW( buffer, path, &path_length, 0 );
    WindowsDeleteString( absolute_uri );
    if (SUCCEEDED(hr)) hr = WindowsCreateString( path, path_length, value );
    free( path );
    return hr;
}

static HRESULT xml_reader_get_hstring( IXmlReader *reader, const WCHAR *attribute, HSTRING *value )
{
    const WCHAR *buffer;
    UINT length;
    HRESULT hr;

    if (FAILED(hr = IXmlReader_MoveToAttributeByName( reader, attribute, NULL ))) return hr;
    if (FAILED(hr = IXmlReader_GetValue( reader, &buffer, &length ))) return hr;
    return WindowsCreateString( buffer, length, value );
}

static WORD zip_read_word( const BYTE *buffer )
{
    return buffer[0] | (buffer[1] << 8);
}

static DWORD zip_read_dword( const BYTE *buffer )
{
    return zip_read_word( buffer ) | ((DWORD)zip_read_word( buffer + 2 ) << 16);
}

static ULONGLONG zip_read_qword( const BYTE *buffer )
{
    return zip_read_dword( buffer ) | ((ULONGLONG)zip_read_dword( buffer + 4 ) << 32);
}

static HRESULT file_read_at( HANDLE file, ULONGLONG offset, void *buffer, DWORD size )
{
    LARGE_INTEGER position;
    DWORD read;

    position.QuadPart = offset;
    if (!SetFilePointerEx( file, position, NULL, FILE_BEGIN )) return HRESULT_FROM_WIN32( GetLastError() );
    if (!ReadFile( file, buffer, size, &read, NULL )) return HRESULT_FROM_WIN32( GetLastError() );
    return read == size ? S_OK : HRESULT_FROM_WIN32( ERROR_HANDLE_EOF );
}

static voidpf zip_zalloc( voidpf opaque, uInt items, uInt size )
{
    (void)opaque;
    return calloc( items, size );
}

static void zip_zfree( voidpf opaque, voidpf address )
{
    (void)opaque;
    free( address );
}

static HRESULT read_zip_entry( const WCHAR *path, const char *entry_name, BYTE **value,
        DWORD *value_size )
{
    static const ULONGLONG max_directory_size = 64 * 1024 * 1024;
    static const DWORD max_entry_size = 16 * 1024 * 1024;
    HANDLE file = INVALID_HANDLE_VALUE;
    LARGE_INTEGER file_size;
    BYTE zip64_locator[20], zip64_end[56];
    BYTE *tail = NULL, *directory = NULL, *compressed = NULL, *result = NULL;
    DWORD tail_size, directory_size, entry_length = strlen( entry_name );
    DWORD compressed_size = 0, uncompressed_size = 0, local_offset = 0;
    ULONGLONG directory_offset, directory_size64, end_offset, zip64_offset;
    WORD compression = 0, name_length, extra_length, comment_length;
    const BYTE *header = NULL;
    z_stream stream = {0};
    HRESULT hr = HRESULT_FROM_WIN32( ERROR_FILE_NOT_FOUND );
    const char *stage = "open";
    LONG index;
    DWORD offset;

    if (!value || !value_size) return E_POINTER;
    *value = NULL;
    *value_size = 0;
    file = CreateFileW( path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
            NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL );
    if (file == INVALID_HANDLE_VALUE) return HRESULT_FROM_WIN32( GetLastError() );
    stage = "file size";
    if (!GetFileSizeEx( file, &file_size ))
    {
        hr = HRESULT_FROM_WIN32( GetLastError() );
        goto done;
    }
    if (file_size.QuadPart < 0)
    {
        hr = HRESULT_FROM_WIN32( ERROR_INVALID_DATA );
        goto done;
    }
    tail_size = file_size.QuadPart < 0x10015 ? file_size.QuadPart : 0x10015;
    if (tail_size < 22 || !(tail = malloc( tail_size )))
    {
        hr = tail_size < 22 ? HRESULT_FROM_WIN32( ERROR_INVALID_DATA ) : E_OUTOFMEMORY;
        goto done;
    }
    stage = "end record";
    if (FAILED(hr = file_read_at( file, file_size.QuadPart - tail_size, tail, tail_size ))) goto done;
    for (index = tail_size - 22; index >= 0; --index)
        if (zip_read_dword( tail + index ) == 0x06054b50) break;
    if (index < 0)
    {
        hr = HRESULT_FROM_WIN32( ERROR_INVALID_DATA );
        goto done;
    }
    end_offset = file_size.QuadPart - tail_size + index;
    directory_size64 = zip_read_dword( tail + index + 12 );
    directory_offset = zip_read_dword( tail + index + 16 );
    if (directory_size64 == MAXDWORD || directory_offset == MAXDWORD)
    {
        if (end_offset < sizeof(zip64_locator) ||
                FAILED(hr = file_read_at( file, end_offset - sizeof(zip64_locator),
                zip64_locator, sizeof(zip64_locator) )) ||
                zip_read_dword( zip64_locator ) != 0x07064b50)
        {
            hr = HRESULT_FROM_WIN32( ERROR_INVALID_DATA );
            goto done;
        }
        zip64_offset = zip_read_qword( zip64_locator + 8 );
        if (FAILED(hr = file_read_at( file, zip64_offset, zip64_end, sizeof(zip64_end) )) ||
                zip_read_dword( zip64_end ) != 0x06064b50)
        {
            hr = HRESULT_FROM_WIN32( ERROR_INVALID_DATA );
            goto done;
        }
        directory_size64 = zip_read_qword( zip64_end + 40 );
        directory_offset = zip_read_qword( zip64_end + 48 );
    }
    stage = "central directory";
    if (!directory_size64 || directory_size64 > MAXDWORD ||
            directory_size64 > max_directory_size ||
            directory_offset > file_size.QuadPart ||
            directory_size64 > file_size.QuadPart - directory_offset)
    {
        hr = HRESULT_FROM_WIN32( ERROR_INVALID_DATA );
        goto done;
    }
    directory_size = directory_size64;
    if (!(directory = malloc( directory_size )))
    {
        hr = E_OUTOFMEMORY;
        goto done;
    }
    if (FAILED(hr = file_read_at( file, directory_offset, directory, directory_size ))) goto done;

    for (offset = 0; offset + 46 <= directory_size; offset += 46 + name_length + extra_length + comment_length)
    {
        header = directory + offset;
        if (zip_read_dword( header ) != 0x02014b50)
        {
            hr = HRESULT_FROM_WIN32( ERROR_INVALID_DATA );
            goto done;
        }
        name_length = zip_read_word( header + 28 );
        extra_length = zip_read_word( header + 30 );
        comment_length = zip_read_word( header + 32 );
        if (offset + 46 + name_length + extra_length + comment_length > directory_size)
        {
            hr = HRESULT_FROM_WIN32( ERROR_INVALID_DATA );
            goto done;
        }
        if (name_length != entry_length || memcmp( header + 46, entry_name, entry_length )) continue;
        compression = zip_read_word( header + 10 );
        compressed_size = zip_read_dword( header + 20 );
        uncompressed_size = zip_read_dword( header + 24 );
        local_offset = zip_read_dword( header + 42 );
        if (compressed_size == MAXDWORD || uncompressed_size == MAXDWORD ||
                uncompressed_size > max_entry_size)
        {
            hr = HRESULT_FROM_WIN32( ERROR_FILE_TOO_LARGE );
            goto done;
        }
        break;
    }
    if (offset + 46 > directory_size)
    {
        hr = HRESULT_FROM_WIN32( ERROR_FILE_NOT_FOUND );
        goto done;
    }

    stage = "local header";
    if (!(header = malloc( 30 )))
    {
        hr = E_OUTOFMEMORY;
        goto done;
    }
    free( directory );
    directory = (BYTE *)header;
    if (FAILED(hr = file_read_at( file, local_offset, directory, 30 ))) goto done;
    if (zip_read_dword( directory ) != 0x04034b50)
    {
        hr = HRESULT_FROM_WIN32( ERROR_INVALID_DATA );
        goto done;
    }
    name_length = zip_read_word( directory + 26 );
    extra_length = zip_read_word( directory + 28 );
    stage = "entry data";
    if ((ULONGLONG)local_offset + 30 + name_length + extra_length > file_size.QuadPart ||
            compressed_size > file_size.QuadPart - local_offset - 30 - name_length - extra_length)
    {
        hr = HRESULT_FROM_WIN32( ERROR_INVALID_DATA );
        goto done;
    }
    if (!(compressed = malloc( compressed_size )) || !(result = malloc( uncompressed_size + 1 )))
    {
        hr = E_OUTOFMEMORY;
        goto done;
    }
    if (FAILED(hr = file_read_at( file, (ULONGLONG)local_offset + 30 + name_length + extra_length,
            compressed, compressed_size ))) goto done;
    if (!compression)
    {
        if (compressed_size != uncompressed_size)
        {
            hr = HRESULT_FROM_WIN32( ERROR_INVALID_DATA );
            goto done;
        }
        memcpy( result, compressed, uncompressed_size );
    }
    else if (compression == 8)
    {
        stage = "deflate";
        stream.zalloc = zip_zalloc;
        stream.zfree = zip_zfree;
        stream.next_in = compressed;
        stream.avail_in = compressed_size;
        stream.next_out = result;
        stream.avail_out = uncompressed_size;
        if (inflateInit2( &stream, -MAX_WBITS ) != Z_OK)
        {
            hr = E_FAIL;
            goto done;
        }
        if (inflate( &stream, Z_FINISH ) != Z_STREAM_END || stream.total_out != uncompressed_size)
            hr = HRESULT_FROM_WIN32( ERROR_INVALID_DATA );
        else hr = S_OK;
        inflateEnd( &stream );
        if (FAILED(hr)) goto done;
    }
    else
    {
        hr = HRESULT_FROM_WIN32( ERROR_NOT_SUPPORTED );
        goto done;
    }
    result[uncompressed_size] = 0;
    *value = result;
    *value_size = uncompressed_size;
    TRACE( "Read ZIP entry %s from %s, compressed %lu, uncompressed %lu.\n",
            entry_name, debugstr_w(path), compressed_size, uncompressed_size );
    result = NULL;
    hr = S_OK;

done:
    if (FAILED(hr)) WARN( "Failed to read ZIP entry %s from %s at %s, hr %#lx.\n",
            entry_name, debugstr_w(path), stage, hr );
    free( result );
    free( compressed );
    free( directory );
    free( tail );
    if (file != INVALID_HANDLE_VALUE) CloseHandle( file );
    return hr;
}

static HRESULT parse_package_manifest( const WCHAR *path, struct package_registration *registration )
{
    BYTE *manifest = NULL;
    DWORD manifest_size;
    IStream *content = NULL;
    IXmlReader *reader = NULL;
    HSTRING version = NULL, architecture = NULL;
    const WCHAR *version_string, *architecture_string;
    XmlNodeType node_type;
    HRESULT hr;

    if (FAILED(hr = read_zip_entry( path, "AppxManifest.xml", &manifest, &manifest_size )))
    {
        WARN( "Failed to read package manifest from %s, hr %#lx.\n", debugstr_w(path), hr );
        goto done;
    }
    if (!(content = SHCreateMemStream( manifest, manifest_size )))
    {
        hr = E_OUTOFMEMORY;
        goto done;
    }
    if (FAILED(hr = CreateXmlReader( &IID_IXmlReader, (void **)&reader, NULL ))) goto done;
    if (FAILED(hr = IXmlReader_SetInput( reader, (IUnknown *)content ))) goto done;

    while ((hr = IXmlReader_Read( reader, &node_type )) == S_OK)
    {
        const WCHAR *local_name;
        UINT local_name_length;

        if (node_type != XmlNodeType_Element) continue;
        if (FAILED(hr = IXmlReader_GetLocalName( reader, &local_name, &local_name_length ))) goto done;
        if (local_name_length == 8 && !wcsncmp( local_name, L"Identity", 8 ) &&
                !registration->name)
        {
            if (FAILED(hr = xml_reader_get_hstring( reader, L"Name", &registration->name ))) goto done;
            IXmlReader_MoveToElement( reader );
            if (FAILED(hr = xml_reader_get_hstring( reader, L"Publisher", &registration->publisher ))) goto done;
            IXmlReader_MoveToElement( reader );
            if (FAILED(hr = xml_reader_get_hstring( reader, L"Version", &version ))) goto done;
            IXmlReader_MoveToElement( reader );
            if (FAILED(hr = xml_reader_get_hstring( reader, L"ProcessorArchitecture", &architecture ))) goto done;
            IXmlReader_MoveToElement( reader );
        }
        else if (local_name_length == 11 && !wcsncmp( local_name, L"Application", 11 ) &&
                !registration->executable)
        {
            hr = xml_reader_get_hstring( reader, L"Executable", &registration->executable );
            if (FAILED(hr) && hr != S_FALSE) goto done;
            IXmlReader_MoveToElement( reader );
            hr = xml_reader_get_hstring( reader, L"Id", &registration->application_id );
            if (FAILED(hr) && hr != S_FALSE) goto done;
            IXmlReader_MoveToElement( reader );
        }
    }
    if (!registration->name || !registration->publisher || !version || !architecture)
    {
        WARN( "Incomplete package identity in %s, reader hr %#lx.\n", debugstr_w(path), hr );
        hr = HRESULT_FROM_WIN32( ERROR_INVALID_DATA );
        goto done;
    }

    version_string = WindowsGetStringRawBuffer( version, NULL );
    architecture_string = WindowsGetStringRawBuffer( architecture, NULL );
    if (swscanf( version_string, L"%hu.%hu.%hu.%hu", &registration->version.Major,
            &registration->version.Minor, &registration->version.Build,
            &registration->version.Revision ) != 4)
    {
        hr = HRESULT_FROM_WIN32( ERROR_INVALID_DATA );
        goto done;
    }
    if (!wcsicmp( architecture_string, L"x64" ))
        registration->architecture = ProcessorArchitecture_X64;
    else if (!wcsicmp( architecture_string, L"x86" ))
        registration->architecture = ProcessorArchitecture_X86;
    else if (!wcsicmp( architecture_string, L"arm64" ))
        registration->architecture = ProcessorArchitecture_Arm64;
    else if (!wcsicmp( architecture_string, L"arm" ))
        registration->architecture = ProcessorArchitecture_Arm;
    else if (!wcsicmp( architecture_string, L"neutral" ))
        registration->architecture = ProcessorArchitecture_Neutral;
    else
    {
        WARN( "Unsupported package architecture %s for package %s.\n",
                debugstr_hstring(architecture), debugstr_hstring(registration->name) );
        registration->architecture = ProcessorArchitecture_Unknown;
    }
    TRACE( "Parsed package %s version %s architecture %s executable %s application %s.\n",
            debugstr_hstring(registration->name), debugstr_hstring(version),
            debugstr_hstring(architecture), debugstr_hstring(registration->executable),
            debugstr_hstring(registration->application_id) );
    hr = S_OK;

done:
    WindowsDeleteString( architecture );
    WindowsDeleteString( version );
    if (reader) IXmlReader_Release( reader );
    if (content) IStream_Release( content );
    free( manifest );
    return hr;
}

static const WCHAR *architecture_name(
        __x_ABI_CWindows_CSystem_CProcessorArchitecture architecture )
{
    switch (architecture)
    {
    case ProcessorArchitecture_X86: return L"x86";
    case ProcessorArchitecture_Arm: return L"arm";
    case ProcessorArchitecture_X64: return L"x64";
    case ProcessorArchitecture_Arm64: return L"arm64";
    case ProcessorArchitecture_Neutral: return L"neutral";
    default: return L"unknown";
    }
}

static HRESULT package_registration_create( HSTRING family_name, IUriRuntimeClass *uri,
        IAddPackageOptions *options, struct package_registration **value )
{
    struct package_registration *registration;
    IUriRuntimeClass *external_uri = NULL;
    HSTRING package_path = NULL;
    const WCHAR *family, *separator, *name, *publisher_id, *application_id, *arch;
    UINT32 family_length, name_length, publisher_id_length, application_id_length;
    WCHAR *buffer;
    size_t length;
    HRESULT hr;

    if (!value) return E_POINTER;
    *value = NULL;
    if (!(registration = calloc( 1, sizeof(*registration) ))) return E_OUTOFMEMORY;
    if (FAILED(hr = WindowsDuplicateString( family_name, &registration->family_name ))) goto failed;
    if (FAILED(hr = uri_to_path( uri, &package_path ))) goto failed;
    if (FAILED(hr = parse_package_manifest( WindowsGetStringRawBuffer(package_path, NULL), registration ))) goto failed;
    if (options && FAILED(hr = IAddPackageOptions_get_ExternalLocationUri( options, &external_uri ))) goto failed;
    if (FAILED(hr = uri_to_path( external_uri, &registration->external_path ))) goto failed;

    family = WindowsGetStringRawBuffer( family_name, &family_length );
    separator = wcsrchr( family, '_' );
    if (!separator)
    {
        hr = HRESULT_FROM_WIN32( ERROR_INVALID_DATA );
        goto failed;
    }
    publisher_id = separator + 1;
    publisher_id_length = family_length - (publisher_id - family);
    if (FAILED(hr = WindowsCreateString( publisher_id, publisher_id_length,
            &registration->publisher_id ))) goto failed;

    if (registration->application_id)
    {
        application_id = WindowsGetStringRawBuffer( registration->application_id,
                &application_id_length );
        length = family_length + application_id_length + 2;
        if (!(buffer = malloc( length * sizeof(*buffer) )))
        {
            hr = E_OUTOFMEMORY;
            goto failed;
        }
        swprintf( buffer, length, L"%.*ls!%.*ls", family_length, family,
                application_id_length, application_id );
        hr = WindowsCreateString( buffer, length - 1,
                &registration->application_user_model_id );
        free( buffer );
        if (FAILED(hr)) goto failed;
    }

    name = WindowsGetStringRawBuffer( registration->name, &name_length );
    arch = architecture_name( registration->architecture );
    length = name_length + publisher_id_length + wcslen( arch ) + 48;
    if (!(buffer = malloc( length * sizeof(*buffer) )))
    {
        hr = E_OUTOFMEMORY;
        goto failed;
    }
    swprintf( buffer, length, L"%.*ls_%hu.%hu.%hu.%hu_%ls__%.*ls", name_length, name,
            registration->version.Major, registration->version.Minor,
            registration->version.Build, registration->version.Revision, arch,
            publisher_id_length, publisher_id );
    hr = WindowsCreateString( buffer, wcslen(buffer), &registration->full_name );
    if (SUCCEEDED(hr))
    {
        static const WCHAR windows_apps[] = L"C:\\Program Files\\WindowsApps\\";
        size_t prefix_length = ARRAY_SIZE(windows_apps) - 1;
        size_t full_name_length = wcslen( buffer );
        WCHAR *new_buffer;

        length = prefix_length + full_name_length + 1;
        new_buffer = realloc( buffer, length * sizeof(*buffer) );

        if (!new_buffer) hr = E_OUTOFMEMORY;
        else
        {
            buffer = new_buffer;
            memmove( buffer + prefix_length, buffer, (full_name_length + 1) * sizeof(*buffer) );
            memcpy( buffer, windows_apps, prefix_length * sizeof(*buffer) );
            hr = WindowsCreateString( buffer, length - 1, &registration->path );
        }
    }
    free( buffer );
    if (FAILED(hr)) goto failed;

    if (external_uri) IUriRuntimeClass_Release( external_uri );
    WindowsDeleteString( package_path );
    *value = registration;
    return S_OK;

failed:
    if (external_uri) IUriRuntimeClass_Release( external_uri );
    WindowsDeleteString( package_path );
    package_registration_free( registration );
    return hr;
}

static inline struct registered_package_id *impl_from_IPackageId( IPackageId *iface )
{
    return CONTAINING_RECORD( iface, struct registered_package_id, IPackageId_iface );
}

static HRESULT WINAPI registered_package_id_QueryInterface( IPackageId *iface, REFIID iid, void **out )
{
    TRACE( "iface %p, iid %s, out %p.\n", iface, debugstr_guid(iid), out );
    if (!out) return E_POINTER;
    if (IsEqualGUID( iid, &IID_IUnknown ) || IsEqualGUID( iid, &IID_IInspectable ) ||
        IsEqualGUID( iid, &IID_IAgileObject ) || IsEqualGUID( iid, &IID_IPackageId ))
    {
        *out = iface;
        IPackageId_AddRef( iface );
        return S_OK;
    }

    *out = NULL;
    return E_NOINTERFACE;
}

static ULONG WINAPI registered_package_id_AddRef( IPackageId *iface )
{
    return InterlockedIncrement( &impl_from_IPackageId( iface )->ref );
}

static ULONG WINAPI registered_package_id_Release( IPackageId *iface )
{
    struct registered_package_id *impl = impl_from_IPackageId( iface );
    ULONG ref = InterlockedDecrement( &impl->ref );

    if (!ref)
    {
        WindowsDeleteString( impl->family_name );
        WindowsDeleteString( impl->name );
        WindowsDeleteString( impl->publisher );
        WindowsDeleteString( impl->publisher_id );
        WindowsDeleteString( impl->full_name );
        free( impl );
    }
    return ref;
}

static HRESULT WINAPI registered_package_id_GetIids( IPackageId *iface, ULONG *count, IID **iids )
{
    if (!count || !iids) return E_POINTER;
    if (!(*iids = CoTaskMemAlloc( sizeof(**iids) ))) return E_OUTOFMEMORY;
    **iids = IID_IPackageId;
    *count = 1;
    return S_OK;
}

static HRESULT WINAPI registered_package_id_GetRuntimeClassName( IPackageId *iface, HSTRING *name )
{
    static const WCHAR class_name[] = L"Windows.ApplicationModel.PackageId";

    if (!name) return E_POINTER;
    return WindowsCreateString( class_name, ARRAY_SIZE(class_name) - 1, name );
}

static HRESULT WINAPI registered_package_id_GetTrustLevel( IPackageId *iface, TrustLevel *level )
{
    if (!level) return E_POINTER;
    *level = BaseTrust;
    return S_OK;
}

static HRESULT WINAPI registered_package_id_get_Name( IPackageId *iface, HSTRING *value )
{
    TRACE( "iface %p, value %p.\n", iface, value );
    if (!value) return E_POINTER;
    return WindowsDuplicateString( impl_from_IPackageId(iface)->name, value );
}

static HRESULT WINAPI registered_package_id_get_Version( IPackageId *iface, PackageVersion *value )
{
    TRACE( "iface %p, value %p.\n", iface, value );
    if (!value) return E_POINTER;
    *value = impl_from_IPackageId(iface)->version;
    return S_OK;
}

static HRESULT WINAPI registered_package_id_get_Architecture( IPackageId *iface,
        __x_ABI_CWindows_CSystem_CProcessorArchitecture *value )
{
    TRACE( "iface %p, value %p.\n", iface, value );
    if (!value) return E_POINTER;
    *value = impl_from_IPackageId(iface)->architecture;
    return S_OK;
}

static HRESULT WINAPI registered_package_id_get_ResourceId( IPackageId *iface, HSTRING *value )
{
    TRACE( "iface %p, value %p.\n", iface, value );
    if (!value) return E_POINTER;
    return WindowsDuplicateString( impl_from_IPackageId(iface)->publisher, value );
}

static HRESULT WINAPI registered_package_id_get_Publisher( IPackageId *iface, HSTRING *value )
{
    TRACE( "iface %p, value %p.\n", iface, value );
    if (!value) return E_POINTER;
    return WindowsCreateString( L"", 0, value );
}

static HRESULT WINAPI registered_package_id_get_PublisherId( IPackageId *iface, HSTRING *value )
{
    TRACE( "iface %p, value %p.\n", iface, value );
    if (!value) return E_POINTER;
    return WindowsDuplicateString( impl_from_IPackageId(iface)->publisher_id, value );
}

static HRESULT WINAPI registered_package_id_get_FullName( IPackageId *iface, HSTRING *value )
{
    TRACE( "iface %p, value %p.\n", iface, value );
    if (!value) return E_POINTER;
    return WindowsDuplicateString( impl_from_IPackageId(iface)->full_name, value );
}

static HRESULT WINAPI registered_package_id_get_FamilyName( IPackageId *iface, HSTRING *value )
{
    TRACE( "iface %p, value %p.\n", iface, value );
    if (!value) return E_POINTER;
    return WindowsDuplicateString( impl_from_IPackageId(iface)->family_name, value );
}

static const IPackageIdVtbl registered_package_id_vtbl =
{
    registered_package_id_QueryInterface,
    registered_package_id_AddRef,
    registered_package_id_Release,
    registered_package_id_GetIids,
    registered_package_id_GetRuntimeClassName,
    registered_package_id_GetTrustLevel,
    registered_package_id_get_Name,
    registered_package_id_get_Version,
    registered_package_id_get_Architecture,
    registered_package_id_get_ResourceId,
    registered_package_id_get_Publisher,
    registered_package_id_get_PublisherId,
    registered_package_id_get_FullName,
    registered_package_id_get_FamilyName,
};

static HRESULT registered_package_id_create( struct registered_package *package, IPackageId **value )
{
    struct registered_package_id *id;
    HRESULT hr;

    if (!value) return E_POINTER;
    *value = NULL;
    if (!(id = calloc( 1, sizeof(*id) ))) return E_OUTOFMEMORY;
    id->IPackageId_iface.lpVtbl = &registered_package_id_vtbl;
    id->ref = 1;
    if (FAILED(hr = WindowsDuplicateString( package->family_name, &id->family_name ))) goto failed;
    if (FAILED(hr = WindowsDuplicateString( package->name, &id->name ))) goto failed;
    if (FAILED(hr = WindowsDuplicateString( package->publisher, &id->publisher ))) goto failed;
    if (FAILED(hr = WindowsDuplicateString( package->publisher_id, &id->publisher_id ))) goto failed;
    if (FAILED(hr = WindowsDuplicateString( package->full_name, &id->full_name ))) goto failed;
    id->version = package->version;
    id->architecture = package->architecture;

    *value = &id->IPackageId_iface;
    return S_OK;

failed:
    IPackageId_Release( &id->IPackageId_iface );
    return hr;
}

static inline struct registered_package *impl_from_IPackage( IPackage *iface )
{
    return CONTAINING_RECORD( iface, struct registered_package, IPackage_iface );
}

static inline struct registered_package *impl_from_IPackage8( IPackage8 *iface )
{
    return CONTAINING_RECORD( iface, struct registered_package, IPackage8_iface );
}

static inline struct registered_package *impl_from_IPackage3( IPackage3 *iface )
{
    return CONTAINING_RECORD( iface, struct registered_package, IPackage3_iface );
}

static HRESULT WINAPI registered_package_QueryInterface( IPackage *iface, REFIID iid, void **out )
{
    TRACE( "iface %p, iid %s, out %p.\n", iface, debugstr_guid(iid), out );
    if (!out) return E_POINTER;
    if (IsEqualGUID( iid, &IID_IUnknown ) || IsEqualGUID( iid, &IID_IInspectable ) ||
        IsEqualGUID( iid, &IID_IAgileObject ) || IsEqualGUID( iid, &IID_IPackage ))
    {
        *out = iface;
        IPackage_AddRef( iface );
        return S_OK;
    }
    if (IsEqualGUID( iid, &IID_IPackage8 ))
    {
        *out = &impl_from_IPackage(iface)->IPackage8_iface;
        IPackage8_AddRef( *out );
        return S_OK;
    }
    if (IsEqualGUID( iid, &IID_IPackage3 ))
    {
        *out = &impl_from_IPackage(iface)->IPackage3_iface;
        IPackage3_AddRef( *out );
        return S_OK;
    }

    *out = NULL;
    return E_NOINTERFACE;
}

static ULONG WINAPI registered_package_AddRef( IPackage *iface )
{
    return InterlockedIncrement( &impl_from_IPackage( iface )->ref );
}

static ULONG WINAPI registered_package_Release( IPackage *iface )
{
    struct registered_package *impl = impl_from_IPackage( iface );
    ULONG ref = InterlockedDecrement( &impl->ref );

    if (!ref)
    {
        WindowsDeleteString( impl->family_name );
        WindowsDeleteString( impl->name );
        WindowsDeleteString( impl->publisher );
        WindowsDeleteString( impl->publisher_id );
        WindowsDeleteString( impl->full_name );
        WindowsDeleteString( impl->path );
        WindowsDeleteString( impl->external_path );
        free( impl );
    }
    return ref;
}

static HRESULT WINAPI registered_package_GetIids( IPackage *iface, ULONG *count, IID **iids )
{
    if (!count || !iids) return E_POINTER;
    if (!(*iids = CoTaskMemAlloc( 3 * sizeof(**iids) ))) return E_OUTOFMEMORY;
    (*iids)[0] = IID_IPackage;
    (*iids)[1] = IID_IPackage3;
    (*iids)[2] = IID_IPackage8;
    *count = 3;
    return S_OK;
}

static HRESULT WINAPI registered_package_GetRuntimeClassName( IPackage *iface, HSTRING *name )
{
    static const WCHAR class_name[] = L"Windows.ApplicationModel.Package";

    if (!name) return E_POINTER;
    return WindowsCreateString( class_name, ARRAY_SIZE(class_name) - 1, name );
}

static HRESULT WINAPI registered_package_GetTrustLevel( IPackage *iface, TrustLevel *level )
{
    if (!level) return E_POINTER;
    *level = BaseTrust;
    return S_OK;
}

static HRESULT WINAPI registered_package_get_Id( IPackage *iface, IPackageId **value )
{
    TRACE( "iface %p, value %p.\n", iface, value );
    return registered_package_id_create( impl_from_IPackage(iface), value );
}

static HRESULT WINAPI registered_package_get_InstalledLocation( IPackage *iface,
        __x_ABI_CWindows_CStorage_CIStorageFolder **value )
{
    if (value) *value = NULL;
    return E_NOTIMPL;
}

static HRESULT WINAPI registered_package_get_IsFramework( IPackage *iface, boolean *value )
{
    if (!value) return E_POINTER;
    *value = FALSE;
    return S_OK;
}

static HRESULT WINAPI registered_package_get_Dependencies( IPackage *iface, IVectorView_Package **value )
{
    if (value) *value = NULL;
    return E_NOTIMPL;
}

static const IPackageVtbl registered_package_vtbl =
{
    registered_package_QueryInterface,
    registered_package_AddRef,
    registered_package_Release,
    registered_package_GetIids,
    registered_package_GetRuntimeClassName,
    registered_package_GetTrustLevel,
    registered_package_get_Id,
    registered_package_get_InstalledLocation,
    registered_package_get_IsFramework,
    registered_package_get_Dependencies,
};

static inline struct registered_package_status *impl_from_IPackageStatus( IPackageStatus *iface )
{
    return CONTAINING_RECORD( iface, struct registered_package_status, IPackageStatus_iface );
}

static HRESULT WINAPI registered_package_status_QueryInterface( IPackageStatus *iface, REFIID iid,
        void **out )
{
    if (!out) return E_POINTER;
    if (IsEqualGUID( iid, &IID_IUnknown ) || IsEqualGUID( iid, &IID_IInspectable ) ||
        IsEqualGUID( iid, &IID_IAgileObject ) || IsEqualGUID( iid, &IID_IPackageStatus ))
    {
        *out = iface;
        IPackageStatus_AddRef( iface );
        return S_OK;
    }
    *out = NULL;
    return E_NOINTERFACE;
}

static ULONG WINAPI registered_package_status_AddRef( IPackageStatus *iface )
{
    return InterlockedIncrement( &impl_from_IPackageStatus(iface)->ref );
}

static ULONG WINAPI registered_package_status_Release( IPackageStatus *iface )
{
    struct registered_package_status *impl = impl_from_IPackageStatus( iface );
    ULONG ref = InterlockedDecrement( &impl->ref );

    if (!ref) free( impl );
    return ref;
}

static HRESULT WINAPI registered_package_status_GetIids( IPackageStatus *iface, ULONG *count,
        IID **iids )
{
    if (!count || !iids) return E_POINTER;
    if (!(*iids = CoTaskMemAlloc( sizeof(**iids) ))) return E_OUTOFMEMORY;
    **iids = IID_IPackageStatus;
    *count = 1;
    return S_OK;
}

static HRESULT WINAPI registered_package_status_GetRuntimeClassName( IPackageStatus *iface,
        HSTRING *name )
{
    static const WCHAR class_name[] = L"Windows.ApplicationModel.PackageStatus";

    if (!name) return E_POINTER;
    return WindowsCreateString( class_name, ARRAY_SIZE(class_name) - 1, name );
}

static HRESULT WINAPI registered_package_status_GetTrustLevel( IPackageStatus *iface,
        TrustLevel *level )
{
    if (!level) return E_POINTER;
    *level = BaseTrust;
    return S_OK;
}

static HRESULT WINAPI registered_package_status_VerifyIsOK( IPackageStatus *iface, boolean *value )
{
    TRACE( "iface %p, value %p.\n", iface, value );
    if (!value) return E_POINTER;
    *value = TRUE;
    return S_OK;
}

static HRESULT WINAPI registered_package_status_get_false( IPackageStatus *iface, boolean *value )
{
    TRACE( "iface %p, value %p.\n", iface, value );
    if (!value) return E_POINTER;
    *value = FALSE;
    return S_OK;
}

static const IPackageStatusVtbl registered_package_status_vtbl =
{
    registered_package_status_QueryInterface,
    registered_package_status_AddRef,
    registered_package_status_Release,
    registered_package_status_GetIids,
    registered_package_status_GetRuntimeClassName,
    registered_package_status_GetTrustLevel,
    registered_package_status_VerifyIsOK,
    registered_package_status_get_false,
    registered_package_status_get_false,
    registered_package_status_get_false,
    registered_package_status_get_false,
    registered_package_status_get_false,
    registered_package_status_get_false,
    registered_package_status_get_false,
    registered_package_status_get_false,
    registered_package_status_get_false,
    registered_package_status_get_false,
    registered_package_status_get_false,
};

static HRESULT WINAPI registered_package3_QueryInterface( IPackage3 *iface, REFIID iid, void **out )
{
    return registered_package_QueryInterface( &impl_from_IPackage3(iface)->IPackage_iface, iid, out );
}

static ULONG WINAPI registered_package3_AddRef( IPackage3 *iface )
{
    return registered_package_AddRef( &impl_from_IPackage3(iface)->IPackage_iface );
}

static ULONG WINAPI registered_package3_Release( IPackage3 *iface )
{
    return registered_package_Release( &impl_from_IPackage3(iface)->IPackage_iface );
}

static HRESULT WINAPI registered_package3_GetIids( IPackage3 *iface, ULONG *count, IID **iids )
{
    return registered_package_GetIids( &impl_from_IPackage3(iface)->IPackage_iface, count, iids );
}

static HRESULT WINAPI registered_package3_GetRuntimeClassName( IPackage3 *iface, HSTRING *name )
{
    return registered_package_GetRuntimeClassName( &impl_from_IPackage3(iface)->IPackage_iface, name );
}

static HRESULT WINAPI registered_package3_GetTrustLevel( IPackage3 *iface, TrustLevel *level )
{
    return registered_package_GetTrustLevel( &impl_from_IPackage3(iface)->IPackage_iface, level );
}

static HRESULT WINAPI registered_package3_get_Status( IPackage3 *iface, IPackageStatus **value )
{
    struct registered_package_status *status;

    TRACE( "iface %p, value %p.\n", iface, value );
    if (!value) return E_POINTER;
    *value = NULL;
    if (!(status = calloc( 1, sizeof(*status) ))) return E_OUTOFMEMORY;
    status->IPackageStatus_iface.lpVtbl = &registered_package_status_vtbl;
    status->ref = 1;
    *value = &status->IPackageStatus_iface;
    return S_OK;
}

static HRESULT WINAPI registered_package3_get_InstalledDate( IPackage3 *iface,
        __x_ABI_CWindows_CFoundation_CDateTime *value )
{
    TRACE( "iface %p, value %p.\n", iface, value );
    if (!value) return E_POINTER;
    value->UniversalTime = 0;
    return S_OK;
}

static HRESULT WINAPI registered_package3_GetAppListEntriesAsync( IPackage3 *iface,
        __FIAsyncOperation_1___FIVectorView_1_Windows__CApplicationModel__CCore__CAppListEntry **operation )
{
    TRACE( "iface %p, operation %p.\n", iface, operation );
    if (operation) *operation = NULL;
    return E_NOTIMPL;
}

static const IPackage3Vtbl registered_package3_vtbl =
{
    registered_package3_QueryInterface,
    registered_package3_AddRef,
    registered_package3_Release,
    registered_package3_GetIids,
    registered_package3_GetRuntimeClassName,
    registered_package3_GetTrustLevel,
    registered_package3_get_Status,
    registered_package3_get_InstalledDate,
    registered_package3_GetAppListEntriesAsync,
};

static HRESULT WINAPI registered_package8_QueryInterface( IPackage8 *iface, REFIID iid, void **out )
{
    return registered_package_QueryInterface( &impl_from_IPackage8(iface)->IPackage_iface, iid, out );
}

static ULONG WINAPI registered_package8_AddRef( IPackage8 *iface )
{
    return registered_package_AddRef( &impl_from_IPackage8(iface)->IPackage_iface );
}

static ULONG WINAPI registered_package8_Release( IPackage8 *iface )
{
    return registered_package_Release( &impl_from_IPackage8(iface)->IPackage_iface );
}

static HRESULT WINAPI registered_package8_GetIids( IPackage8 *iface, ULONG *count, IID **iids )
{
    return registered_package_GetIids( &impl_from_IPackage8(iface)->IPackage_iface, count, iids );
}

static HRESULT WINAPI registered_package8_GetRuntimeClassName( IPackage8 *iface, HSTRING *name )
{
    return registered_package_GetRuntimeClassName( &impl_from_IPackage8(iface)->IPackage_iface, name );
}

static HRESULT WINAPI registered_package8_GetTrustLevel( IPackage8 *iface, TrustLevel *level )
{
    return registered_package_GetTrustLevel( &impl_from_IPackage8(iface)->IPackage_iface, level );
}

static HRESULT WINAPI registered_package8_get_ExternalLocation( IPackage8 *iface,
        __x_ABI_CWindows_CStorage_CIStorageFolder **value )
{
    TRACE( "iface %p, value %p.\n", iface, value );
    if (!value) return E_POINTER;
    *value = NULL;
    return S_OK;
}

#define DEFINE_PACKAGE8_PATH_GETTER(name, field) \
    static HRESULT WINAPI registered_package8_get_##name( IPackage8 *iface, HSTRING *value ) \
    { \
        struct registered_package *impl = impl_from_IPackage8( iface ); \
        TRACE( "iface %p, value %p, " #name " %s.\n", iface, value, debugstr_hstring(impl->field) ); \
        if (!value) return E_POINTER; \
        return WindowsDuplicateString( impl->field, value ); \
    }

DEFINE_PACKAGE8_PATH_GETTER( InstalledPath, path );
DEFINE_PACKAGE8_PATH_GETTER( EffectivePath, path );
DEFINE_PACKAGE8_PATH_GETTER( EffectiveExternalPath, external_path );
DEFINE_PACKAGE8_PATH_GETTER( UserExternalPath, external_path );

#undef DEFINE_PACKAGE8_PATH_GETTER

#define DEFINE_PACKAGE8_EMPTY_PATH_GETTER(name) \
    static HRESULT WINAPI registered_package8_get_##name( IPackage8 *iface, HSTRING *value ) \
    { \
        TRACE( "iface %p, value %p.\n", iface, value ); \
        if (!value) return E_POINTER; \
        return WindowsCreateString( L"", 0, value ); \
    }

DEFINE_PACKAGE8_EMPTY_PATH_GETTER( MutablePath );
DEFINE_PACKAGE8_EMPTY_PATH_GETTER( MachineExternalPath );

#undef DEFINE_PACKAGE8_EMPTY_PATH_GETTER

static HRESULT WINAPI registered_package8_GetLogoAsRandomAccessStreamReference( IPackage8 *iface,
        __x_ABI_CWindows_CFoundation_CSize size,
        __x_ABI_CWindows_CStorage_CStreams_CIRandomAccessStreamReference **result )
{
    TRACE( "iface %p, size %f x %f, result %p.\n", iface, size.Width, size.Height, result );
    if (!result) return E_POINTER;
    *result = NULL;
    return S_OK;
}

static HRESULT WINAPI registered_package8_GetAppListEntries( IPackage8 *iface,
        __FIVectorView_1_Windows__CApplicationModel__CCore__CAppListEntry **result )
{
    TRACE( "iface %p, result %p.\n", iface, result );
    if (!result) return E_POINTER;
    *result = NULL;
    return S_OK;
}

static const IPackage8Vtbl registered_package8_vtbl =
{
    registered_package8_QueryInterface,
    registered_package8_AddRef,
    registered_package8_Release,
    registered_package8_GetIids,
    registered_package8_GetRuntimeClassName,
    registered_package8_GetTrustLevel,
    registered_package8_get_ExternalLocation,
    registered_package8_get_ExternalLocation,
    registered_package8_get_ExternalLocation,
    registered_package8_get_InstalledPath,
    registered_package8_get_MutablePath,
    registered_package8_get_EffectivePath,
    registered_package8_get_EffectiveExternalPath,
    registered_package8_get_MachineExternalPath,
    registered_package8_get_UserExternalPath,
    registered_package8_GetLogoAsRandomAccessStreamReference,
    registered_package8_GetAppListEntries,
};

static HRESULT registered_package_create( const struct package_registration *registration,
        IPackage **value )
{
    struct registered_package *package;
    HRESULT hr;

    if (!value) return E_POINTER;
    *value = NULL;
    if (!(package = calloc( 1, sizeof(*package) ))) return E_OUTOFMEMORY;
    package->IPackage_iface.lpVtbl = &registered_package_vtbl;
    package->IPackage3_iface.lpVtbl = &registered_package3_vtbl;
    package->IPackage8_iface.lpVtbl = &registered_package8_vtbl;
    package->ref = 1;
    if (FAILED(hr = WindowsDuplicateString( registration->family_name, &package->family_name ))) goto failed;
    if (FAILED(hr = WindowsDuplicateString( registration->name, &package->name ))) goto failed;
    if (FAILED(hr = WindowsDuplicateString( registration->publisher, &package->publisher ))) goto failed;
    if (FAILED(hr = WindowsDuplicateString( registration->publisher_id, &package->publisher_id ))) goto failed;
    if (FAILED(hr = WindowsDuplicateString( registration->full_name, &package->full_name ))) goto failed;
    if (FAILED(hr = WindowsDuplicateString( registration->path, &package->path ))) goto failed;
    if (FAILED(hr = WindowsDuplicateString( registration->external_path, &package->external_path ))) goto failed;
    package->version = registration->version;
    package->architecture = registration->architecture;
    *value = &package->IPackage_iface;
    return S_OK;

failed:
    IPackage_Release( &package->IPackage_iface );
    return hr;
}

static inline struct package_iterator *impl_from_IIterator_Package( IIterator_Package *iface )
{
    return CONTAINING_RECORD( iface, struct package_iterator, IIterator_Package_iface );
}

static HRESULT WINAPI package_iterator_QueryInterface( IIterator_Package *iface, REFIID iid, void **out )
{
    if (!out) return E_POINTER;

    if (IsEqualGUID( iid, &IID_IUnknown ) || IsEqualGUID( iid, &IID_IInspectable ) ||
        IsEqualGUID( iid, &IID_IAgileObject ) || IsEqualGUID( iid, &IID_IIterator_Package ))
    {
        *out = iface;
        IIterator_Package_AddRef( iface );
        return S_OK;
    }

    *out = NULL;
    return E_NOINTERFACE;
}

static ULONG WINAPI package_iterator_AddRef( IIterator_Package *iface )
{
    return InterlockedIncrement( &impl_from_IIterator_Package( iface )->ref );
}

static ULONG WINAPI package_iterator_Release( IIterator_Package *iface )
{
    struct package_iterator *impl = impl_from_IIterator_Package( iface );
    ULONG ref = InterlockedDecrement( &impl->ref );

    if (!ref)
    {
        if (impl->package) IPackage_Release( impl->package );
        free( impl );
    }
    return ref;
}

static HRESULT WINAPI package_iterator_GetIids( IIterator_Package *iface, ULONG *count, IID **iids )
{
    if (!count || !iids) return E_POINTER;
    if (!(*iids = CoTaskMemAlloc( sizeof(**iids) ))) return E_OUTOFMEMORY;
    **iids = IID_IIterator_Package;
    *count = 1;
    return S_OK;
}

static HRESULT WINAPI package_iterator_GetRuntimeClassName( IIterator_Package *iface, HSTRING *name )
{
    if (!name) return E_POINTER;
    return WindowsCreateString( RuntimeClass_Windows_Management_Deployment_PackageManager,
            ARRAY_SIZE(RuntimeClass_Windows_Management_Deployment_PackageManager) - 1, name );
}

static HRESULT WINAPI package_iterator_GetTrustLevel( IIterator_Package *iface, TrustLevel *level )
{
    if (!level) return E_POINTER;
    *level = BaseTrust;
    return S_OK;
}

static HRESULT WINAPI package_iterator_get_Current( IIterator_Package *iface, IPackage **value )
{
    struct package_iterator *impl = impl_from_IIterator_Package( iface );

    TRACE( "iface %p, value %p, has_current %u.\n", iface, value, impl->has_current );
    if (!value) return E_POINTER;
    if (!impl->has_current)
    {
        *value = NULL;
        return E_BOUNDS;
    }
    *value = impl->package;
    IPackage_AddRef( *value );
    return S_OK;
}

static HRESULT WINAPI package_iterator_get_HasCurrent( IIterator_Package *iface, boolean *value )
{
    struct package_iterator *impl = impl_from_IIterator_Package( iface );

    TRACE( "iface %p, value %p, has_current %u.\n", iface, value, impl->has_current );
    if (!value) return E_POINTER;
    *value = impl->has_current;
    return S_OK;
}

static HRESULT WINAPI package_iterator_MoveNext( IIterator_Package *iface, boolean *value )
{
    struct package_iterator *impl = impl_from_IIterator_Package( iface );

    if (!value) return E_POINTER;
    impl->has_current = FALSE;
    *value = FALSE;
    return S_OK;
}

static HRESULT WINAPI package_iterator_GetMany( IIterator_Package *iface, UINT32 capacity,
        IPackage **items, UINT32 *count )
{
    struct package_iterator *impl = impl_from_IIterator_Package( iface );

    if (!count || (capacity && !items)) return E_POINTER;
    if (!capacity || !impl->has_current)
    {
        *count = 0;
        return S_OK;
    }
    items[0] = impl->package;
    IPackage_AddRef( items[0] );
    impl->has_current = FALSE;
    *count = 1;
    return S_OK;
}

static const IIterator_PackageVtbl package_iterator_vtbl =
{
    package_iterator_QueryInterface,
    package_iterator_AddRef,
    package_iterator_Release,
    package_iterator_GetIids,
    package_iterator_GetRuntimeClassName,
    package_iterator_GetTrustLevel,
    package_iterator_get_Current,
    package_iterator_get_HasCurrent,
    package_iterator_MoveNext,
    package_iterator_GetMany,
};

static inline struct package_iterable *impl_from_IIterable_Package( IIterable_Package *iface )
{
    return CONTAINING_RECORD( iface, struct package_iterable, IIterable_Package_iface );
}

static HRESULT WINAPI package_iterable_QueryInterface( IIterable_Package *iface, REFIID iid, void **out )
{
    if (!out) return E_POINTER;

    if (IsEqualGUID( iid, &IID_IUnknown ) || IsEqualGUID( iid, &IID_IInspectable ) ||
        IsEqualGUID( iid, &IID_IAgileObject ) || IsEqualGUID( iid, &IID_IIterable_Package ))
    {
        *out = iface;
        IIterable_Package_AddRef( iface );
        return S_OK;
    }

    *out = NULL;
    return E_NOINTERFACE;
}

static ULONG WINAPI package_iterable_AddRef( IIterable_Package *iface )
{
    return InterlockedIncrement( &impl_from_IIterable_Package( iface )->ref );
}

static ULONG WINAPI package_iterable_Release( IIterable_Package *iface )
{
    struct package_iterable *impl = impl_from_IIterable_Package( iface );
    ULONG ref = InterlockedDecrement( &impl->ref );

    if (!ref)
    {
        if (impl->package) IPackage_Release( impl->package );
        free( impl );
    }
    return ref;
}

static HRESULT WINAPI package_iterable_GetIids( IIterable_Package *iface, ULONG *count, IID **iids )
{
    if (!count || !iids) return E_POINTER;
    if (!(*iids = CoTaskMemAlloc( sizeof(**iids) ))) return E_OUTOFMEMORY;
    **iids = IID_IIterable_Package;
    *count = 1;
    return S_OK;
}

static HRESULT WINAPI package_iterable_GetRuntimeClassName( IIterable_Package *iface, HSTRING *name )
{
    if (!name) return E_POINTER;
    return WindowsCreateString( RuntimeClass_Windows_Management_Deployment_PackageManager,
            ARRAY_SIZE(RuntimeClass_Windows_Management_Deployment_PackageManager) - 1, name );
}

static HRESULT WINAPI package_iterable_GetTrustLevel( IIterable_Package *iface, TrustLevel *level )
{
    if (!level) return E_POINTER;
    *level = BaseTrust;
    return S_OK;
}

static HRESULT WINAPI package_iterable_First( IIterable_Package *iface, IIterator_Package **value )
{
    struct package_iterable *impl = impl_from_IIterable_Package( iface );
    struct package_iterator *iterator;

    TRACE( "iface %p, value %p, package %p.\n", iface, value, impl->package );
    if (!value) return E_POINTER;
    *value = NULL;
    if (!(iterator = calloc( 1, sizeof(*iterator) ))) return E_OUTOFMEMORY;
    iterator->IIterator_Package_iface.lpVtbl = &package_iterator_vtbl;
    iterator->ref = 1;
    iterator->has_current = !!impl->package;
    if ((iterator->package = impl->package)) IPackage_AddRef( iterator->package );
    *value = &iterator->IIterator_Package_iface;
    return S_OK;
}

static const IIterable_PackageVtbl package_iterable_vtbl =
{
    package_iterable_QueryInterface,
    package_iterable_AddRef,
    package_iterable_Release,
    package_iterable_GetIids,
    package_iterable_GetRuntimeClassName,
    package_iterable_GetTrustLevel,
    package_iterable_First,
};

static HRESULT package_iterable_create( const struct package_registration *registration,
        IIterable_Package **value )
{
    struct package_iterable *iterable;
    HRESULT hr;

    if (!value) return E_POINTER;
    *value = NULL;
    if (!(iterable = calloc( 1, sizeof(*iterable) ))) return E_OUTOFMEMORY;
    iterable->IIterable_Package_iface.lpVtbl = &package_iterable_vtbl;
    iterable->ref = 1;
    if (registration && FAILED(hr = registered_package_create( registration, &iterable->package )))
    {
        free( iterable );
        return hr;
    }
    *value = &iterable->IIterable_Package_iface;
    return S_OK;
}

static inline struct package_manager *impl_from_IPackageManager( IPackageManager *iface )
{
    return CONTAINING_RECORD( iface, struct package_manager, IPackageManager_iface );
}

static HRESULT WINAPI package_manager_QueryInterface( IPackageManager *iface, REFIID iid, void **out )
{
    struct package_manager *impl = impl_from_IPackageManager( iface );

    TRACE( "iface %p, iid %s, out %p.\n", iface, debugstr_guid( iid ), out );

    if (IsEqualGUID( iid, &IID_IUnknown ) ||
        IsEqualGUID( iid, &IID_IInspectable ) ||
        IsEqualGUID( iid, &IID_IAgileObject ) ||
        IsEqualGUID( iid, &IID_IPackageManager ))
    {
        *out = &impl->IPackageManager_iface;
        IInspectable_AddRef( *out );
        return S_OK;
    }

    if (IsEqualGUID( iid, &IID_IPackageManager2 ))
    {
        *out = &impl->IPackageManager2_iface;
        IInspectable_AddRef( *out );
        return S_OK;
    }

    if (IsEqualGUID( iid, &IID_IPackageManager6 ))
    {
        *out = &impl->IPackageManager6_iface;
        IInspectable_AddRef( *out );
        return S_OK;
    }

    if (IsEqualGUID( iid, &IID_IPackageManager9 ))
    {
        *out = &impl->IPackageManager9_iface;
        IInspectable_AddRef( *out );
        return S_OK;
    }

    FIXME( "%s not implemented, returning E_NOINTERFACE.\n", debugstr_guid( iid ) );
    *out = NULL;
    return E_NOINTERFACE;
}

static ULONG WINAPI package_manager_AddRef( IPackageManager *iface )
{
    struct package_manager *impl = impl_from_IPackageManager( iface );
    ULONG ref = InterlockedIncrement( &impl->ref );
    TRACE( "iface %p increasing refcount to %lu.\n", iface, ref );
    return ref;
}

static ULONG WINAPI package_manager_Release( IPackageManager *iface )
{
    struct package_manager *impl = impl_from_IPackageManager( iface );
    ULONG ref = InterlockedDecrement( &impl->ref );

    TRACE( "iface %p decreasing refcount to %lu.\n", iface, ref );

    if (!ref) free( impl );
    return ref;
}

static HRESULT WINAPI package_manager_GetIids( IPackageManager *iface, ULONG *iid_count, IID **iids )
{
    FIXME( "iface %p, iid_count %p, iids %p stub!\n", iface, iid_count, iids );
    return E_NOTIMPL;
}

static HRESULT WINAPI package_manager_GetRuntimeClassName( IPackageManager *iface, HSTRING *class_name )
{
    FIXME( "iface %p, class_name %p stub!\n", iface, class_name );
    return E_NOTIMPL;
}

static HRESULT WINAPI package_manager_GetTrustLevel( IPackageManager *iface, TrustLevel *trust_level )
{
    FIXME( "iface %p, trust_level %p stub!\n", iface, trust_level );
    return E_NOTIMPL;
}

static HRESULT WINAPI package_manager_AddPackageAsync( IPackageManager *iface, IUriRuntimeClass *uri,
    IIterable_Uri *dependencies, DeploymentOptions options, IAsyncOperationWithProgress_DeploymentResult_DeploymentProgress **operation )
{
    FIXME( "iface %p, uri %p, dependencies %p, options %d, operation %p stub!\n", iface, uri, dependencies, options, operation );
    return E_NOTIMPL;
}

static HRESULT WINAPI package_manager_UpdatePackageAsync( IPackageManager *iface, IUriRuntimeClass *uri, IIterable_Uri *dependencies,
    DeploymentOptions options, IAsyncOperationWithProgress_DeploymentResult_DeploymentProgress **operation )
{
    FIXME( "iface %p, uri %p, dependencies %p, options %d, operation %p stub!\n", iface, uri, dependencies, options, operation );
    return E_NOTIMPL;
}

static HRESULT WINAPI package_manager_RemovePackageAsync( IPackageManager *iface, HSTRING name,
    IAsyncOperationWithProgress_DeploymentResult_DeploymentProgress **operation )
{
    FIXME( "iface %p, name %s, operation %p stub!\n", iface, debugstr_hstring(name), operation );
    return E_NOTIMPL;
}

static HRESULT WINAPI package_manager_StagePackageAsync( IPackageManager *iface, IUriRuntimeClass *uri, IIterable_Uri *dependencies,
    IAsyncOperationWithProgress_DeploymentResult_DeploymentProgress **operation )
{
    FIXME( "iface %p, uri %p, dependencies %p, operation %p stub!\n", iface, uri, dependencies, operation );
    return E_NOTIMPL;
}

static HRESULT WINAPI package_manager_RegisterPackageAsync( IPackageManager *iface, IUriRuntimeClass *uri, IIterable_Uri *dependencies,
    DeploymentOptions options, IAsyncOperationWithProgress_DeploymentResult_DeploymentProgress **operation )
{
    FIXME( "iface %p, uri %p, dependencies %p, options %d, operation %p stub!\n", iface, uri, dependencies, options, operation );
    return E_NOTIMPL;
}

static HRESULT WINAPI package_manager_FindPackages( IPackageManager *iface, IIterable_Package **packages )
{
    FIXME( "iface %p, packages %p stub!\n", iface, packages );
    return E_NOTIMPL;
}

static HRESULT WINAPI package_manager_FindPackagesByUserSecurityId( IPackageManager *iface, HSTRING sid, IIterable_Package **packages )
{
    FIXME( "iface %p, sid %s, packages %p stub!\n", iface, debugstr_hstring(sid), packages );
    return E_NOTIMPL;
}

static HRESULT WINAPI package_manager_FindPackagesByNamePublisher( IPackageManager *iface, HSTRING name, HSTRING publisher, IIterable_Package **packages )
{
    FIXME( "iface %p, name %s, publisher %s, packages %p stub!\n", iface, debugstr_hstring(name), debugstr_hstring(publisher), packages );

    if (!name || !publisher) return E_INVALIDARG;
    return E_NOTIMPL;
}

static HRESULT WINAPI package_manager_FindPackagesByUserSecurityIdNamePublisher( IPackageManager *iface, HSTRING sid,
    HSTRING name, HSTRING publisher, IIterable_Package **packages )
{
    FIXME( "iface %p, sid %s, name %s, publisher %s, packages %p stub!\n", iface, debugstr_hstring(sid), debugstr_hstring(name), debugstr_hstring(publisher), packages );
    return E_NOTIMPL;
}

static HRESULT WINAPI package_manager_FindUsers( IPackageManager *iface, HSTRING name, IIterable_PackageUserInformation **users )
{
    FIXME( "iface %p, name %s, users %p stub!\n", iface, debugstr_hstring(name), users );
    return E_NOTIMPL;
}

static HRESULT WINAPI package_manager_SetPackageState( IPackageManager *iface, HSTRING name, PackageState state )
{
    FIXME("iface %p, name %s, state %d stub!\n", iface, debugstr_hstring(name), state);
    return E_NOTIMPL;
}

static HRESULT WINAPI package_manager_FindPackageByPackageFullName( IPackageManager *iface, HSTRING name, IPackage **package )
{
    FIXME( "iface %p, name %s, package %p stub!\n", iface, debugstr_hstring(name), package );
    return E_NOTIMPL;
}

static HRESULT WINAPI package_manager_CleanupPackageForUserAsync( IPackageManager *iface, HSTRING name, HSTRING sid,
    IAsyncOperationWithProgress_DeploymentResult_DeploymentProgress **operation )
{
    FIXME( "iface %p, name %s, sid %s, operation %p stub!\n", iface, debugstr_hstring(name), debugstr_hstring(sid), operation );
    return E_NOTIMPL;
}

static HRESULT WINAPI package_manager_FindPackagesByPackageFamilyName( IPackageManager *iface, HSTRING family_name,
    IIterable_Package **packages )
{
    FIXME( "iface %p, family_name %s, packages %p stub!\n", iface, debugstr_hstring(family_name), packages );
    return E_NOTIMPL;
}

static HRESULT WINAPI package_manager_FindPackagesByUserSecurityIdPackageFamilyName( IPackageManager *iface, HSTRING sid,
    HSTRING family_name, IIterable_Package **packages )
{
    FIXME( "iface %p, sid %s, family_name %s, packages %p stub!\n", iface, debugstr_hstring(sid), debugstr_hstring(family_name), packages );
    return E_NOTIMPL;
}

static HRESULT WINAPI package_manager_FindPackageByUserSecurityIdPackageFullName( IPackageManager *iface, HSTRING sid, HSTRING name, IPackage **package )
{
    FIXME( "iface %p, sid %s, name %s, package %p stub!\n", iface, debugstr_hstring(sid), debugstr_hstring(name), package );
    return E_NOTIMPL;
}

static const struct IPackageManagerVtbl package_manager_vtbl =
{
    package_manager_QueryInterface,
    package_manager_AddRef,
    package_manager_Release,
    /* IInspectable methods */
    package_manager_GetIids,
    package_manager_GetRuntimeClassName,
    package_manager_GetTrustLevel,
    /* IPackageManager methods */
    package_manager_AddPackageAsync,
    package_manager_UpdatePackageAsync,
    package_manager_RemovePackageAsync,
    package_manager_StagePackageAsync,
    package_manager_RegisterPackageAsync,
    package_manager_FindPackages,
    package_manager_FindPackagesByUserSecurityId,
    package_manager_FindPackagesByNamePublisher,
    package_manager_FindPackagesByUserSecurityIdNamePublisher,
    package_manager_FindUsers,
    package_manager_SetPackageState,
    package_manager_FindPackageByPackageFullName,
    package_manager_CleanupPackageForUserAsync,
    package_manager_FindPackagesByPackageFamilyName,
    package_manager_FindPackagesByUserSecurityIdPackageFamilyName,
    package_manager_FindPackageByUserSecurityIdPackageFullName
};

DEFINE_IINSPECTABLE( package_manager2, IPackageManager2, struct package_manager, IPackageManager_iface );

static HRESULT WINAPI package_manager2_RemovePackageWithOptionsAsync( IPackageManager2 *iface, HSTRING name, RemovalOptions options,
    IAsyncOperationWithProgress_DeploymentResult_DeploymentProgress **operation )
{
    struct package_registration *old_registration = NULL;
    HSTRING old_pending = NULL, family_copy = NULL;
    HRESULT hr;

    TRACE( "iface %p, name %s, options %d, operation %p.\n", iface, debugstr_hstring(name), options, operation );
    if (FAILED(hr = deployment_operation_create( operation ))) return hr;

    AcquireSRWLockExclusive( &package_registration_lock );
    if (registered_package && SUCCEEDED(hr = WindowsDuplicateString(
            registered_package->family_name, &family_copy )))
    {
        hr = set_sparse_package_environment( NULL );
    }
    if (registered_package && SUCCEEDED(hr))
    {
        old_pending = pending_package_family;
        pending_package_family = family_copy;
        family_copy = NULL;
        old_registration = registered_package;
        registered_package = NULL;
    }
    ReleaseSRWLockExclusive( &package_registration_lock );
    WindowsDeleteString( family_copy );
    WindowsDeleteString( old_pending );
    package_registration_free( old_registration );
    if (FAILED(hr))
    {
        IAsyncOperationWithProgress_DeploymentResult_DeploymentProgress_Release( *operation );
        *operation = NULL;
        return hr;
    }
    return S_OK;
}

static HRESULT WINAPI package_manager2_StagePackageWithOptionsAsync( IPackageManager2 *iface, IUriRuntimeClass *uri, IIterable_Uri *dependencies,
    DeploymentOptions options, IAsyncOperationWithProgress_DeploymentResult_DeploymentProgress **operation )
{
    FIXME( "iface %p, uri %p, dependencies %p, options %d, operation %p stub!\n", iface, uri, dependencies, options, operation );
    return E_NOTIMPL;
}

static HRESULT WINAPI package_manager2_RegisterPackageByFullNameAsync( IPackageManager2 *iface, HSTRING name, IIterable_HSTRING *dependencies,
    DeploymentOptions options, IAsyncOperationWithProgress_DeploymentResult_DeploymentProgress **operation )
{
    FIXME( "iface %p, name %s, dependencies %p, options %d, operation %p stub!\n", iface, debugstr_hstring(name), dependencies, options, operation );
    return E_NOTIMPL;
}

static HRESULT WINAPI package_manager2_FindPackagesWithPackageTypes( IPackageManager2 *iface, PackageTypes types, IIterable_Package **packages )
{
    FIXME( "iface %p, types %d, packages %p stub!\n", iface, types, packages );
    return E_NOTIMPL;
}

static HRESULT WINAPI package_manager2_FindPackagesByUserSecurityIdWithPackageTypes( IPackageManager2 *iface, HSTRING sid,
    PackageTypes types, IIterable_Package **packages )
{
    FIXME( "iface %p, sid %s, types %d, packages %p stub!\n", iface, debugstr_hstring(sid), types, packages );
    return E_NOTIMPL;
}

static HRESULT WINAPI package_manager2_FindPackagesByNamePublisherWithPackageTypes( IPackageManager2 *iface, HSTRING name, HSTRING publisher,
    PackageTypes types, IIterable_Package **packages )
{
    FIXME( "iface %p, name %s, publisher %s, types %d, packages %p stub!\n", iface, debugstr_hstring(name), debugstr_hstring(publisher), types, packages );
    return E_NOTIMPL;
}

static HRESULT WINAPI package_manager2_FindPackagesByUserSecurityIdNamePublisherWithPackageTypes( IPackageManager2 *iface, HSTRING sid, HSTRING name,
    HSTRING publisher, PackageTypes types, IIterable_Package **packages )
{
    FIXME( "iface %p, sid %s, name %s, publisher %s, types %d, packages %p stub!\n", iface, debugstr_hstring(sid), debugstr_hstring(name), debugstr_hstring(publisher), types, packages );
    return E_NOTIMPL;
}

static HRESULT WINAPI package_manager2_FindPackagesByPackageFamilyNameWithPackageTypes( IPackageManager2 *iface, HSTRING family_name, PackageTypes types,
   IIterable_Package **packages )
{
    FIXME( "iface %p, family_name %s, types %d, packages %p stub!\n", iface, debugstr_hstring(family_name), types, packages );
    return E_NOTIMPL;
}

static HRESULT WINAPI package_manager2_FindPackagesByUserSecurityIdPackageFamilyNameWithPackageTypes( IPackageManager2 *iface, HSTRING sid, HSTRING family_name,
    PackageTypes types, IIterable_Package **packages )
{
    HSTRING old_family = NULL, family_copy = NULL;
    INT32 order;
    boolean registered = FALSE;
    HRESULT hr;

    TRACE( "iface %p, sid %s, family_name %s, types %d, packages %p.\n", iface, debugstr_hstring(sid), debugstr_hstring(family_name), types, packages );

    if (!family_name) return E_INVALIDARG;
    AcquireSRWLockShared( &package_registration_lock );
    if (registered_package)
        registered = SUCCEEDED(WindowsCompareStringOrdinal( registered_package->family_name,
                family_name, &order )) && !order;
    if (registered) hr = package_iterable_create( registered_package, packages );
    ReleaseSRWLockShared( &package_registration_lock );
    if (registered) return hr;

    if (SUCCEEDED(WindowsDuplicateString( family_name, &family_copy )))
    {
        AcquireSRWLockExclusive( &package_registration_lock );
        old_family = pending_package_family;
        pending_package_family = family_copy;
        ReleaseSRWLockExclusive( &package_registration_lock );
        WindowsDeleteString( old_family );
    }
    return package_iterable_create( NULL, packages );
}

static HRESULT WINAPI package_manager2_StageUserDataAsync( IPackageManager2 *iface, HSTRING name,
    IAsyncOperationWithProgress_DeploymentResult_DeploymentProgress **operation )
{
    FIXME( "iface %p, name %s, operation %p stub!\n", iface, debugstr_hstring(name), operation );
    return E_NOTIMPL;
}

static const struct IPackageManager2Vtbl package_manager2_vtbl =
{
    package_manager2_QueryInterface,
    package_manager2_AddRef,
    package_manager2_Release,
    /* IInspectable methods */
    package_manager2_GetIids,
    package_manager2_GetRuntimeClassName,
    package_manager2_GetTrustLevel,
    /* IPackageManager2 methods */
    package_manager2_RemovePackageWithOptionsAsync,
    package_manager2_StagePackageWithOptionsAsync,
    package_manager2_RegisterPackageByFullNameAsync,
    package_manager2_FindPackagesWithPackageTypes,
    package_manager2_FindPackagesByUserSecurityIdWithPackageTypes,
    package_manager2_FindPackagesByNamePublisherWithPackageTypes,
    package_manager2_FindPackagesByUserSecurityIdNamePublisherWithPackageTypes,
    package_manager2_FindPackagesByPackageFamilyNameWithPackageTypes,
    package_manager2_FindPackagesByUserSecurityIdPackageFamilyNameWithPackageTypes,
    package_manager2_StageUserDataAsync,
};

DEFINE_IINSPECTABLE( package_manager6, IPackageManager6, struct package_manager, IPackageManager_iface );

static HRESULT WINAPI package_manager6_ProvisionPackageForAllUsersAsync( IPackageManager6 *iface, HSTRING family_name,
    IAsyncOperationWithProgress_DeploymentResult_DeploymentProgress **operation )
{
    FIXME( "iface %p, family_name %s, operation %p stub!\n", iface, debugstr_hstring(family_name), operation );
    if (operation) *operation = NULL;
    return E_NOTIMPL;
}

static HRESULT WINAPI package_manager6_AddPackageByAppInstallerFileAsync( IPackageManager6 *iface, IUriRuntimeClass *uri,
    AddPackageByAppInstallerOptions options, IPackageVolume *volume,
    IAsyncOperationWithProgress_DeploymentResult_DeploymentProgress **operation )
{
    FIXME( "iface %p, uri %p, options %d, volume %p, operation %p stub!\n", iface, uri, options, volume, operation );
    if (operation) *operation = NULL;
    return E_NOTIMPL;
}

static HRESULT WINAPI package_manager6_RequestAddPackageByAppInstallerFileAsync( IPackageManager6 *iface, IUriRuntimeClass *uri,
    AddPackageByAppInstallerOptions options, IPackageVolume *volume,
    IAsyncOperationWithProgress_DeploymentResult_DeploymentProgress **operation )
{
    FIXME( "iface %p, uri %p, options %d, volume %p, operation %p stub!\n", iface, uri, options, volume, operation );
    if (operation) *operation = NULL;
    return E_NOTIMPL;
}

static HRESULT WINAPI package_manager6_AddPackageToVolumeAndRelatedSetAsync( IPackageManager6 *iface, IUriRuntimeClass *uri,
    IIterable_Uri *dependencies, DeploymentOptions options, IPackageVolume *volume, IIterable_HSTRING *optional_families,
    IIterable_Uri *packages, IIterable_Uri *related_packages,
    IAsyncOperationWithProgress_DeploymentResult_DeploymentProgress **operation )
{
    FIXME( "iface %p, uri %p, dependencies %p, options %d, volume %p, optional_families %p, packages %p, related_packages %p, operation %p stub!\n",
           iface, uri, dependencies, options, volume, optional_families, packages, related_packages, operation );
    if (operation) *operation = NULL;
    return E_NOTIMPL;
}

static HRESULT WINAPI package_manager6_StagePackageToVolumeAndRelatedSetAsync( IPackageManager6 *iface, IUriRuntimeClass *uri,
    IIterable_Uri *dependencies, DeploymentOptions options, IPackageVolume *volume, IIterable_HSTRING *optional_families,
    IIterable_Uri *packages, IIterable_Uri *related_packages,
    IAsyncOperationWithProgress_DeploymentResult_DeploymentProgress **operation )
{
    FIXME( "iface %p, uri %p, dependencies %p, options %d, volume %p, optional_families %p, packages %p, related_packages %p, operation %p stub!\n",
           iface, uri, dependencies, options, volume, optional_families, packages, related_packages, operation );
    if (operation) *operation = NULL;
    return E_NOTIMPL;
}

static HRESULT WINAPI package_manager6_RequestAddPackageAsync( IPackageManager6 *iface, IUriRuntimeClass *uri,
    IIterable_Uri *dependencies, DeploymentOptions options, IPackageVolume *volume, IIterable_HSTRING *optional_families,
    IIterable_Uri *related_packages, IAsyncOperationWithProgress_DeploymentResult_DeploymentProgress **operation )
{
    FIXME( "iface %p, uri %p, dependencies %p, options %d, volume %p, optional_families %p, related_packages %p, operation %p stub!\n",
           iface, uri, dependencies, options, volume, optional_families, related_packages, operation );
    if (operation) *operation = NULL;
    return E_NOTIMPL;
}

static const struct IPackageManager6Vtbl package_manager6_vtbl =
{
    package_manager6_QueryInterface,
    package_manager6_AddRef,
    package_manager6_Release,
    /* IInspectable methods */
    package_manager6_GetIids,
    package_manager6_GetRuntimeClassName,
    package_manager6_GetTrustLevel,
    /* IPackageManager6 methods */
    package_manager6_ProvisionPackageForAllUsersAsync,
    package_manager6_AddPackageByAppInstallerFileAsync,
    package_manager6_RequestAddPackageByAppInstallerFileAsync,
    package_manager6_AddPackageToVolumeAndRelatedSetAsync,
    package_manager6_StagePackageToVolumeAndRelatedSetAsync,
    package_manager6_RequestAddPackageAsync,
};

DEFINE_IINSPECTABLE( package_manager9, IPackageManager9, struct package_manager, IPackageManager_iface );

struct deployment_result
{
    IDeploymentResult IDeploymentResult_iface;
    LONG ref;
};

struct deployment_operation
{
    IAsyncOperationWithProgress_DeploymentResult_DeploymentProgress IAsyncOperationWithProgress_iface;
    IAsyncInfo IAsyncInfo_iface;
    IAsyncOperationProgressHandler_DeploymentResult_DeploymentProgress *progress_handler;
    IAsyncOperationWithProgressCompletedHandler_DeploymentResult_DeploymentProgress *completed_handler;
    IDeploymentResult *result;
    LONG ref;
};

static inline struct deployment_result *impl_from_IDeploymentResult( IDeploymentResult *iface )
{
    return CONTAINING_RECORD( iface, struct deployment_result, IDeploymentResult_iface );
}

static HRESULT WINAPI deployment_result_QueryInterface( IDeploymentResult *iface, REFIID iid, void **out )
{
    if (!out) return E_POINTER;
    if (IsEqualGUID( iid, &IID_IUnknown ) || IsEqualGUID( iid, &IID_IInspectable ) ||
        IsEqualGUID( iid, &IID_IAgileObject ) || IsEqualGUID( iid, &IID_IDeploymentResult ))
    {
        *out = iface;
        IDeploymentResult_AddRef( iface );
        return S_OK;
    }
    *out = NULL;
    return E_NOINTERFACE;
}

static ULONG WINAPI deployment_result_AddRef( IDeploymentResult *iface )
{
    return InterlockedIncrement( &impl_from_IDeploymentResult( iface )->ref );
}

static ULONG WINAPI deployment_result_Release( IDeploymentResult *iface )
{
    struct deployment_result *impl = impl_from_IDeploymentResult( iface );
    ULONG ref = InterlockedDecrement( &impl->ref );

    if (!ref) free( impl );
    return ref;
}

static HRESULT WINAPI deployment_result_GetIids( IDeploymentResult *iface, ULONG *count, IID **iids )
{
    if (!count || !iids) return E_POINTER;
    if (!(*iids = CoTaskMemAlloc( sizeof(**iids) ))) return E_OUTOFMEMORY;
    **iids = IID_IDeploymentResult;
    *count = 1;
    return S_OK;
}

static HRESULT WINAPI deployment_result_GetRuntimeClassName( IDeploymentResult *iface, HSTRING *name )
{
    if (!name) return E_POINTER;
    return WindowsCreateString( RuntimeClass_Windows_Management_Deployment_DeploymentResult,
            ARRAY_SIZE(RuntimeClass_Windows_Management_Deployment_DeploymentResult) - 1, name );
}

static HRESULT WINAPI deployment_result_GetTrustLevel( IDeploymentResult *iface, TrustLevel *level )
{
    if (!level) return E_POINTER;
    *level = BaseTrust;
    return S_OK;
}

static HRESULT WINAPI deployment_result_get_ErrorText( IDeploymentResult *iface, HSTRING *value )
{
    if (!value) return E_POINTER;
    return WindowsCreateString( L"", 0, value );
}

static HRESULT WINAPI deployment_result_get_ActivityId( IDeploymentResult *iface, GUID *value )
{
    if (!value) return E_POINTER;
    memset( value, 0, sizeof(*value) );
    return S_OK;
}

static HRESULT WINAPI deployment_result_get_ExtendedErrorCode( IDeploymentResult *iface, HRESULT *value )
{
    if (!value) return E_POINTER;
    *value = S_OK;
    return S_OK;
}

static const IDeploymentResultVtbl deployment_result_vtbl =
{
    deployment_result_QueryInterface,
    deployment_result_AddRef,
    deployment_result_Release,
    deployment_result_GetIids,
    deployment_result_GetRuntimeClassName,
    deployment_result_GetTrustLevel,
    deployment_result_get_ErrorText,
    deployment_result_get_ActivityId,
    deployment_result_get_ExtendedErrorCode,
};

static inline struct deployment_operation *impl_from_deployment_operation(
        IAsyncOperationWithProgress_DeploymentResult_DeploymentProgress *iface )
{
    return CONTAINING_RECORD( iface, struct deployment_operation, IAsyncOperationWithProgress_iface );
}

static HRESULT WINAPI deployment_operation_QueryInterface(
        IAsyncOperationWithProgress_DeploymentResult_DeploymentProgress *iface, REFIID iid, void **out )
{
    struct deployment_operation *impl = impl_from_deployment_operation( iface );

    if (!out) return E_POINTER;
    if (IsEqualGUID( iid, &IID_IUnknown ) || IsEqualGUID( iid, &IID_IInspectable ) ||
        IsEqualGUID( iid, &IID_IAgileObject ) ||
        IsEqualGUID( iid, &IID_IAsyncOperationWithProgress_DeploymentResult_DeploymentProgress ))
        *out = &impl->IAsyncOperationWithProgress_iface;
    else if (IsEqualGUID( iid, &IID_IAsyncInfo ))
        *out = &impl->IAsyncInfo_iface;
    else
    {
        *out = NULL;
        return E_NOINTERFACE;
    }
    IInspectable_AddRef( *out );
    return S_OK;
}

static ULONG WINAPI deployment_operation_AddRef(
        IAsyncOperationWithProgress_DeploymentResult_DeploymentProgress *iface )
{
    return InterlockedIncrement( &impl_from_deployment_operation( iface )->ref );
}

static ULONG WINAPI deployment_operation_Release(
        IAsyncOperationWithProgress_DeploymentResult_DeploymentProgress *iface )
{
    struct deployment_operation *impl = impl_from_deployment_operation( iface );
    ULONG ref = InterlockedDecrement( &impl->ref );

    if (!ref)
    {
        if (impl->progress_handler)
            IAsyncOperationProgressHandler_DeploymentResult_DeploymentProgress_Release( impl->progress_handler );
        if (impl->completed_handler)
            IAsyncOperationWithProgressCompletedHandler_DeploymentResult_DeploymentProgress_Release( impl->completed_handler );
        IDeploymentResult_Release( impl->result );
        free( impl );
    }
    return ref;
}

static HRESULT WINAPI deployment_operation_GetIids(
        IAsyncOperationWithProgress_DeploymentResult_DeploymentProgress *iface, ULONG *count, IID **iids )
{
    if (!count || !iids) return E_POINTER;
    if (!(*iids = CoTaskMemAlloc( sizeof(**iids) ))) return E_OUTOFMEMORY;
    **iids = IID_IAsyncOperationWithProgress_DeploymentResult_DeploymentProgress;
    *count = 1;
    return S_OK;
}

static HRESULT WINAPI deployment_operation_GetRuntimeClassName(
        IAsyncOperationWithProgress_DeploymentResult_DeploymentProgress *iface, HSTRING *name )
{
    static const WCHAR class_name[] =
            L"Windows.Foundation.IAsyncOperationWithProgress`2<Windows.Management.Deployment.DeploymentResult,Windows.Management.Deployment.DeploymentProgress>";

    if (!name) return E_POINTER;
    return WindowsCreateString( class_name, ARRAY_SIZE(class_name) - 1, name );
}

static HRESULT WINAPI deployment_operation_GetTrustLevel(
        IAsyncOperationWithProgress_DeploymentResult_DeploymentProgress *iface, TrustLevel *level )
{
    if (!level) return E_POINTER;
    *level = BaseTrust;
    return S_OK;
}

static HRESULT WINAPI deployment_operation_put_Progress(
        IAsyncOperationWithProgress_DeploymentResult_DeploymentProgress *iface,
        IAsyncOperationProgressHandler_DeploymentResult_DeploymentProgress *handler )
{
    struct deployment_operation *impl = impl_from_deployment_operation( iface );

    if (handler) IAsyncOperationProgressHandler_DeploymentResult_DeploymentProgress_AddRef( handler );
    if (impl->progress_handler)
        IAsyncOperationProgressHandler_DeploymentResult_DeploymentProgress_Release( impl->progress_handler );
    impl->progress_handler = handler;
    return S_OK;
}

static HRESULT WINAPI deployment_operation_get_Progress(
        IAsyncOperationWithProgress_DeploymentResult_DeploymentProgress *iface,
        IAsyncOperationProgressHandler_DeploymentResult_DeploymentProgress **handler )
{
    struct deployment_operation *impl = impl_from_deployment_operation( iface );

    if (!handler) return E_POINTER;
    if ((*handler = impl->progress_handler))
        IAsyncOperationProgressHandler_DeploymentResult_DeploymentProgress_AddRef( *handler );
    return S_OK;
}

static HRESULT WINAPI deployment_operation_put_Completed(
        IAsyncOperationWithProgress_DeploymentResult_DeploymentProgress *iface,
        IAsyncOperationWithProgressCompletedHandler_DeploymentResult_DeploymentProgress *handler )
{
    struct deployment_operation *impl = impl_from_deployment_operation( iface );

    TRACE( "iface %p, handler %p.\n", iface, handler );
    if (handler) IAsyncOperationWithProgressCompletedHandler_DeploymentResult_DeploymentProgress_AddRef( handler );
    if (impl->completed_handler)
        IAsyncOperationWithProgressCompletedHandler_DeploymentResult_DeploymentProgress_Release( impl->completed_handler );
    impl->completed_handler = handler;
    if (handler)
        return IAsyncOperationWithProgressCompletedHandler_DeploymentResult_DeploymentProgress_Invoke( handler, iface, Completed );
    return S_OK;
}

static HRESULT WINAPI deployment_operation_get_Completed(
        IAsyncOperationWithProgress_DeploymentResult_DeploymentProgress *iface,
        IAsyncOperationWithProgressCompletedHandler_DeploymentResult_DeploymentProgress **handler )
{
    struct deployment_operation *impl = impl_from_deployment_operation( iface );

    if (!handler) return E_POINTER;
    if ((*handler = impl->completed_handler))
        IAsyncOperationWithProgressCompletedHandler_DeploymentResult_DeploymentProgress_AddRef( *handler );
    return S_OK;
}

static HRESULT WINAPI deployment_operation_GetResults(
        IAsyncOperationWithProgress_DeploymentResult_DeploymentProgress *iface, IDeploymentResult **result )
{
    struct deployment_operation *impl = impl_from_deployment_operation( iface );

    TRACE( "iface %p, result %p.\n", iface, result );
    if (!result) return E_POINTER;
    *result = impl->result;
    IDeploymentResult_AddRef( *result );
    return S_OK;
}

static const IAsyncOperationWithProgress_DeploymentResult_DeploymentProgressVtbl deployment_operation_vtbl =
{
    deployment_operation_QueryInterface,
    deployment_operation_AddRef,
    deployment_operation_Release,
    deployment_operation_GetIids,
    deployment_operation_GetRuntimeClassName,
    deployment_operation_GetTrustLevel,
    deployment_operation_put_Progress,
    deployment_operation_get_Progress,
    deployment_operation_put_Completed,
    deployment_operation_get_Completed,
    deployment_operation_GetResults,
};

static inline struct deployment_operation *impl_from_IAsyncInfo( IAsyncInfo *iface )
{
    return CONTAINING_RECORD( iface, struct deployment_operation, IAsyncInfo_iface );
}

static HRESULT WINAPI deployment_info_QueryInterface( IAsyncInfo *iface, REFIID iid, void **out )
{
    struct deployment_operation *impl = impl_from_IAsyncInfo( iface );
    return deployment_operation_QueryInterface( &impl->IAsyncOperationWithProgress_iface, iid, out );
}

static ULONG WINAPI deployment_info_AddRef( IAsyncInfo *iface )
{
    struct deployment_operation *impl = impl_from_IAsyncInfo( iface );
    return deployment_operation_AddRef( &impl->IAsyncOperationWithProgress_iface );
}

static ULONG WINAPI deployment_info_Release( IAsyncInfo *iface )
{
    struct deployment_operation *impl = impl_from_IAsyncInfo( iface );
    return deployment_operation_Release( &impl->IAsyncOperationWithProgress_iface );
}

static HRESULT WINAPI deployment_info_GetIids( IAsyncInfo *iface, ULONG *count, IID **iids )
{
    struct deployment_operation *impl = impl_from_IAsyncInfo( iface );
    return deployment_operation_GetIids( &impl->IAsyncOperationWithProgress_iface, count, iids );
}

static HRESULT WINAPI deployment_info_GetRuntimeClassName( IAsyncInfo *iface, HSTRING *name )
{
    struct deployment_operation *impl = impl_from_IAsyncInfo( iface );
    return deployment_operation_GetRuntimeClassName( &impl->IAsyncOperationWithProgress_iface, name );
}

static HRESULT WINAPI deployment_info_GetTrustLevel( IAsyncInfo *iface, TrustLevel *level )
{
    struct deployment_operation *impl = impl_from_IAsyncInfo( iface );
    return deployment_operation_GetTrustLevel( &impl->IAsyncOperationWithProgress_iface, level );
}

static HRESULT WINAPI deployment_info_get_Id( IAsyncInfo *iface, UINT32 *id )
{
    if (!id) return E_POINTER;
    *id = 1;
    return S_OK;
}

static HRESULT WINAPI deployment_info_get_Status( IAsyncInfo *iface, AsyncStatus *status )
{
    if (!status) return E_POINTER;
    *status = Completed;
    return S_OK;
}

static HRESULT WINAPI deployment_info_get_ErrorCode( IAsyncInfo *iface, HRESULT *error )
{
    if (!error) return E_POINTER;
    *error = S_OK;
    return S_OK;
}

static HRESULT WINAPI deployment_info_Cancel( IAsyncInfo *iface )
{
    return S_OK;
}

static HRESULT WINAPI deployment_info_Close( IAsyncInfo *iface )
{
    return S_OK;
}

static const IAsyncInfoVtbl deployment_info_vtbl =
{
    deployment_info_QueryInterface,
    deployment_info_AddRef,
    deployment_info_Release,
    deployment_info_GetIids,
    deployment_info_GetRuntimeClassName,
    deployment_info_GetTrustLevel,
    deployment_info_get_Id,
    deployment_info_get_Status,
    deployment_info_get_ErrorCode,
    deployment_info_Cancel,
    deployment_info_Close,
};

static HRESULT deployment_operation_create(
        IAsyncOperationWithProgress_DeploymentResult_DeploymentProgress **operation )
{
    struct deployment_operation *impl;
    struct deployment_result *result;

    if (!operation) return E_POINTER;
    *operation = NULL;
    if (!(impl = calloc( 1, sizeof(*impl) ))) return E_OUTOFMEMORY;
    if (!(result = calloc( 1, sizeof(*result) )))
    {
        free( impl );
        return E_OUTOFMEMORY;
    }
    result->IDeploymentResult_iface.lpVtbl = &deployment_result_vtbl;
    result->ref = 1;
    impl->IAsyncOperationWithProgress_iface.lpVtbl = &deployment_operation_vtbl;
    impl->IAsyncInfo_iface.lpVtbl = &deployment_info_vtbl;
    impl->result = &result->IDeploymentResult_iface;
    impl->ref = 1;
    *operation = &impl->IAsyncOperationWithProgress_iface;
    return S_OK;
}

static HRESULT WINAPI package_manager9_FindProvisionedPackages( IPackageManager9 *iface, IVector_Package **packages )
{
    FIXME( "iface %p, packages %p stub!\n", iface, packages );
    if (packages) *packages = NULL;
    return E_NOTIMPL;
}

static HRESULT WINAPI package_manager9_AddPackageByUriAsync( IPackageManager9 *iface, IUriRuntimeClass *uri,
    IAddPackageOptions *options, IAsyncOperationWithProgress_DeploymentResult_DeploymentProgress **operation )
{
    struct package_registration *new_registration = NULL, *old_registration = NULL;
    HSTRING uri_string = NULL;
    HSTRING family_name = NULL, old_family = NULL;
    HRESULT hr;

    TRACE( "iface %p, uri %p, options %p, operation %p.\n", iface, uri, options, operation );
    if (uri && SUCCEEDED(IUriRuntimeClass_get_AbsoluteUri( uri, &uri_string )))
    {
        TRACE( "uri %s.\n", debugstr_hstring(uri_string) );
        WindowsDeleteString( uri_string );
    }
    AcquireSRWLockShared( &package_registration_lock );
    if (pending_package_family)
        hr = WindowsDuplicateString( pending_package_family, &family_name );
    else hr = HRESULT_FROM_WIN32( ERROR_NOT_FOUND );
    ReleaseSRWLockShared( &package_registration_lock );
    if (FAILED(hr)) return hr;
    if (FAILED(hr = package_registration_create( family_name, uri, options,
            &new_registration ))) goto done;
    if (FAILED(hr = deployment_operation_create( operation ))) goto done;

    AcquireSRWLockExclusive( &package_registration_lock );
    if (SUCCEEDED(hr = set_sparse_package_environment( new_registration )))
    {
        old_registration = registered_package;
        registered_package = new_registration;
        new_registration = NULL;
        old_family = pending_package_family;
        pending_package_family = NULL;
    }
    else
    {
        set_sparse_package_environment( registered_package );
        IAsyncOperationWithProgress_DeploymentResult_DeploymentProgress_Release( *operation );
        *operation = NULL;
    }
    ReleaseSRWLockExclusive( &package_registration_lock );

done:
    package_registration_free( old_registration );
    package_registration_free( new_registration );
    WindowsDeleteString( old_family );
    WindowsDeleteString( family_name );
    return hr;
}

static HRESULT WINAPI package_manager9_StagePackageByUriAsync( IPackageManager9 *iface, IUriRuntimeClass *uri,
    IStagePackageOptions *options, IAsyncOperationWithProgress_DeploymentResult_DeploymentProgress **operation )
{
    FIXME( "iface %p, uri %p, options %p, operation %p stub!\n", iface, uri, options, operation );
    if (operation) *operation = NULL;
    return E_NOTIMPL;
}

static HRESULT WINAPI package_manager9_RegisterPackageByUriAsync( IPackageManager9 *iface, IUriRuntimeClass *uri,
    IRegisterPackageOptions *options, IAsyncOperationWithProgress_DeploymentResult_DeploymentProgress **operation )
{
    FIXME( "iface %p, uri %p, options %p, operation %p stub!\n", iface, uri, options, operation );
    if (operation) *operation = NULL;
    return E_NOTIMPL;
}

static HRESULT WINAPI package_manager9_RegisterPackagesByFullNameAsync( IPackageManager9 *iface,
    IIterable_HSTRING *package_names, IRegisterPackageOptions *options,
    IAsyncOperationWithProgress_DeploymentResult_DeploymentProgress **operation )
{
    FIXME( "iface %p, package_names %p, options %p, operation %p stub!\n", iface, package_names, options, operation );
    if (operation) *operation = NULL;
    return E_NOTIMPL;
}

static HRESULT WINAPI package_manager9_SetPackageStubPreference( IPackageManager9 *iface, HSTRING family_name,
    PackageStubPreference preference )
{
    FIXME( "iface %p, family_name %s, preference %d stub!\n", iface, debugstr_hstring( family_name ), preference );
    return E_NOTIMPL;
}

static HRESULT WINAPI package_manager9_GetPackageStubPreference( IPackageManager9 *iface, HSTRING family_name,
    PackageStubPreference *preference )
{
    FIXME( "iface %p, family_name %s, preference %p stub!\n", iface, debugstr_hstring( family_name ), preference );
    if (preference) *preference = PackageStubPreference_Full;
    return E_NOTIMPL;
}

static const struct IPackageManager9Vtbl package_manager9_vtbl =
{
    package_manager9_QueryInterface,
    package_manager9_AddRef,
    package_manager9_Release,
    /* IInspectable methods */
    package_manager9_GetIids,
    package_manager9_GetRuntimeClassName,
    package_manager9_GetTrustLevel,
    /* IPackageManager9 methods */
    package_manager9_FindProvisionedPackages,
    package_manager9_AddPackageByUriAsync,
    package_manager9_StagePackageByUriAsync,
    package_manager9_RegisterPackageByUriAsync,
    package_manager9_RegisterPackagesByFullNameAsync,
    package_manager9_SetPackageStubPreference,
    package_manager9_GetPackageStubPreference,
};

struct package_manager_statics
{
    IActivationFactory IActivationFactory_iface;
    LONG ref;
};

static inline struct package_manager_statics *impl_from_IActivationFactory( IActivationFactory *iface )
{
    return CONTAINING_RECORD( iface, struct package_manager_statics, IActivationFactory_iface );
}

static HRESULT WINAPI factory_QueryInterface( IActivationFactory *iface, REFIID iid, void **out )
{
    struct package_manager_statics *impl = impl_from_IActivationFactory( iface );

    TRACE( "iface %p, iid %s, out %p.\n", iface, debugstr_guid( iid ), out );

    if (IsEqualGUID( iid, &IID_IUnknown ) ||
        IsEqualGUID( iid, &IID_IInspectable ) ||
        IsEqualGUID( iid, &IID_IAgileObject ) ||
        IsEqualGUID( iid, &IID_IActivationFactory ))
    {
        *out = &impl->IActivationFactory_iface;
        IInspectable_AddRef( *out );
        return S_OK;
    }

    FIXME( "%s not implemented, returning E_NOINTERFACE.\n", debugstr_guid( iid ) );
    *out = NULL;
    return E_NOINTERFACE;
}

static ULONG WINAPI factory_AddRef( IActivationFactory *iface )
{
    struct package_manager_statics *impl = impl_from_IActivationFactory( iface );
    ULONG ref = InterlockedIncrement( &impl->ref );
    TRACE( "iface %p increasing refcount to %lu.\n", iface, ref );
    return ref;
}

static ULONG WINAPI factory_Release( IActivationFactory *iface )
{
    struct package_manager_statics *impl = impl_from_IActivationFactory( iface );
    ULONG ref = InterlockedDecrement( &impl->ref );
    TRACE( "iface %p decreasing refcount to %lu.\n", iface, ref );
    return ref;
}

static HRESULT WINAPI factory_GetIids( IActivationFactory *iface, ULONG *iid_count, IID **iids )
{
    FIXME( "iface %p, iid_count %p, iids %p stub!\n", iface, iid_count, iids );
    return E_NOTIMPL;
}

static HRESULT WINAPI factory_GetRuntimeClassName( IActivationFactory *iface, HSTRING *class_name )
{
    FIXME( "iface %p, class_name %p stub!\n", iface, class_name );
    return E_NOTIMPL;
}

static HRESULT WINAPI factory_GetTrustLevel( IActivationFactory *iface, TrustLevel *trust_level )
{
    FIXME( "iface %p, trust_level %p stub!\n", iface, trust_level );
    return E_NOTIMPL;
}

static HRESULT WINAPI factory_ActivateInstance( IActivationFactory *iface, IInspectable **instance )
{
    struct package_manager *impl;

    TRACE( "iface %p, instance %p.\n", iface, instance );

    if (!(impl = calloc( 1, sizeof(*impl) )))
    {
        *instance = NULL;
        return E_OUTOFMEMORY;
    }

    impl->IPackageManager_iface.lpVtbl = &package_manager_vtbl;
    impl->IPackageManager2_iface.lpVtbl = &package_manager2_vtbl;
    impl->IPackageManager6_iface.lpVtbl = &package_manager6_vtbl;
    impl->IPackageManager9_iface.lpVtbl = &package_manager9_vtbl;
    impl->ref = 1;

    *instance = (IInspectable *)&impl->IPackageManager_iface;
    return S_OK;
}

static const struct IActivationFactoryVtbl factory_vtbl =
{
    factory_QueryInterface,
    factory_AddRef,
    factory_Release,
    /* IInspectable methods */
    factory_GetIids,
    factory_GetRuntimeClassName,
    factory_GetTrustLevel,
    /* IActivationFactory methods */
    factory_ActivateInstance,
};

static struct package_manager_statics package_manager_statics =
{
    {&factory_vtbl},
    1,
};

IActivationFactory *package_manager_factory = &package_manager_statics.IActivationFactory_iface;
