/*
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
#define COBJMACROS
#include "initguid.h"
#include <stdarg.h>

#include "windef.h"
#include "winbase.h"
#include "winstring.h"

#include "roapi.h"

#define WIDL_using_Windows_Foundation
#define WIDL_using_Windows_Foundation_Collections
#include "windows.foundation.h"
#define WIDL_using_Windows_Storage
#include "windows.storage.h"
#define WIDL_using_Windows_Management_Core
#include "windows.management.h"

#include "wine/test.h"

#define check_interface( obj, iid ) check_interface_( __LINE__, obj, iid )
static void check_interface_( unsigned int line, void *obj, const IID *iid )
{
    IUnknown *iface = obj;
    IUnknown *unk;
    HRESULT hr;

    hr = IUnknown_QueryInterface( iface, iid, (void **)&unk );
    ok_(__FILE__, line)( hr == S_OK, "got hr %#lx.\n", hr );
    IUnknown_Release( unk );
}

static void test_ApplicationDataStatics(void)
{
    static const WCHAR *application_data_statics_name = L"Windows.Storage.ApplicationData";
    IApplicationDataStatics *application_data_statics;
    IApplicationData *application_data = NULL;
    IActivationFactory *factory;
    HSTRING str;
    HRESULT hr;
    LONG ref;

    hr = WindowsCreateString( application_data_statics_name, wcslen( application_data_statics_name ), &str );
    ok( hr == S_OK, "got hr %#lx.\n", hr );

    hr = RoGetActivationFactory( str, &IID_IActivationFactory, (void **)&factory );
    WindowsDeleteString( str );
    ok( hr == S_OK || broken( hr == REGDB_E_CLASSNOTREG ), "got hr %#lx.\n", hr );
    if (hr == REGDB_E_CLASSNOTREG)
    {
        win_skip( "%s runtimeclass not registered, skipping tests.\n", wine_dbgstr_w( application_data_statics_name ) );
        return;
    }

    check_interface( factory, &IID_IUnknown );
    check_interface( factory, &IID_IInspectable );
    check_interface( factory, &IID_IAgileObject );

    hr = IActivationFactory_QueryInterface( factory, &IID_IApplicationDataStatics, (void **)&application_data_statics );
    ok( hr == S_OK, "got hr %#lx.\n", hr );

    hr = IApplicationDataStatics_get_Current( application_data_statics, NULL );
    ok( hr == E_INVALIDARG, "got hr %#lx.\n", hr );
    hr = IApplicationDataStatics_get_Current( application_data_statics, &application_data );
    todo_wine ok( hr == 0x80073d54, "got hr %#lx.\n", hr );
    todo_wine ok( !application_data, "got application_data %p.\n", application_data );
    if (application_data) IApplicationData_Release( application_data );

    ref = IApplicationDataStatics_Release( application_data_statics );
    ok( ref == 2, "got ref %ld.\n", ref );
    ref = IActivationFactory_Release( factory );
    ok( ref == 1, "got ref %ld.\n", ref );
}

static void test_ApplicationDataManagerStatics(void)
{
    static const WCHAR class_name[] = L"Windows.Management.Core.ApplicationDataManager";
    static const WCHAR package_family[] = L"wine.applicationdata.test_8wekyb3d8bbwe";
    IApplicationDataManagerStatics *statics;
    IApplicationData *application_data = NULL;
    IStorageFolder *folder = NULL;
    IStorageItem *item = NULL;
    IActivationFactory *factory;
    const WCHAR *path;
    HSTRING str, package, name;
    FileAttributes attributes;
    boolean value;
    HRESULT hr;

    hr = WindowsCreateString( class_name, ARRAY_SIZE( class_name ) - 1, &str );
    ok( hr == S_OK, "got hr %#lx.\n", hr );
    hr = RoGetActivationFactory( str, &IID_IActivationFactory, (void **)&factory );
    WindowsDeleteString( str );
    ok( hr == S_OK || broken( hr == REGDB_E_CLASSNOTREG ), "got hr %#lx.\n", hr );
    if (hr == REGDB_E_CLASSNOTREG)
    {
        win_skip( "%s runtimeclass not registered, skipping tests.\n", wine_dbgstr_w( class_name ) );
        return;
    }

    check_interface( factory, &IID_IUnknown );
    check_interface( factory, &IID_IInspectable );
    check_interface( factory, &IID_IAgileObject );

    hr = IActivationFactory_QueryInterface( factory, &IID_IApplicationDataManagerStatics, (void **)&statics );
    ok( hr == S_OK, "got hr %#lx.\n", hr );

    application_data = (IApplicationData *)0xdeadbeef;
    hr = IApplicationDataManagerStatics_CreateForPackageFamily( statics, NULL, &application_data );
    ok( hr == E_INVALIDARG, "got hr %#lx.\n", hr );
    ok( application_data == (IApplicationData *)0xdeadbeef, "got application_data %p.\n", application_data );

    hr = WindowsCreateString( package_family, ARRAY_SIZE( package_family ) - 1, &package );
    ok( hr == S_OK, "got hr %#lx.\n", hr );
    hr = IApplicationDataManagerStatics_CreateForPackageFamily( statics, package, NULL );
    ok( hr == E_INVALIDARG, "got hr %#lx.\n", hr );
    hr = IApplicationDataManagerStatics_CreateForPackageFamily( statics, package, &application_data );
    ok( hr == S_OK, "got hr %#lx.\n", hr );
    ok( application_data != NULL, "got application_data %p.\n", application_data );
    WindowsDeleteString( package );

    hr = IApplicationData_get_LocalFolder( application_data, NULL );
    ok( hr == E_POINTER, "got hr %#lx.\n", hr );
    hr = IApplicationData_get_LocalFolder( application_data, &folder );
    ok( hr == S_OK, "got hr %#lx.\n", hr );
    ok( folder != NULL, "got folder %p.\n", folder );

    check_interface( folder, &IID_IUnknown );
    check_interface( folder, &IID_IInspectable );
    check_interface( folder, &IID_IAgileObject );
    hr = IStorageFolder_QueryInterface( folder, &IID_IStorageItem, (void **)&item );
    ok( hr == S_OK, "got hr %#lx.\n", hr );

    hr = IStorageItem_get_Path( item, &str );
    ok( hr == S_OK, "got hr %#lx.\n", hr );
    path = WindowsGetStringRawBuffer( str, NULL );
    ok( wcsstr( path, L"\\AppData\\Local\\Packages\\wine.applicationdata.test_8wekyb3d8bbwe\\LocalState" ) != NULL,
            "got path %s.\n", wine_dbgstr_w( path ) );
    ok( GetFileAttributesW( path ) & FILE_ATTRIBUTE_DIRECTORY, "folder %s was not created.\n", wine_dbgstr_w( path ) );
    WindowsDeleteString( str );

    hr = IStorageItem_get_Name( item, &name );
    ok( hr == S_OK, "got hr %#lx.\n", hr );
    ok( !wcscmp( WindowsGetStringRawBuffer( name, NULL ), L"LocalState" ), "got name %s.\n",
            wine_dbgstr_hstring( name ) );
    WindowsDeleteString( name );
    hr = IStorageItem_get_Attributes( item, &attributes );
    ok( hr == S_OK, "got hr %#lx.\n", hr );
    ok( attributes == FileAttributes_Directory, "got attributes %#x.\n", attributes );
    hr = IStorageItem_IsOfType( item, StorageItemTypes_Folder, &value );
    ok( hr == S_OK, "got hr %#lx.\n", hr );
    ok( value, "got value %u.\n", value );
    hr = IStorageItem_IsOfType( item, StorageItemTypes_File, &value );
    ok( hr == S_OK, "got hr %#lx.\n", hr );
    ok( !value, "got value %u.\n", value );

    IStorageItem_Release( item );
    IStorageFolder_Release( folder );
    IApplicationData_Release( application_data );
    IApplicationDataManagerStatics_Release( statics );
    IActivationFactory_Release( factory );
}

START_TEST(data)
{
    HRESULT hr;

    hr = RoInitialize( RO_INIT_MULTITHREADED );
    ok( hr == S_OK, "RoInitialize failed, hr %#lx\n", hr );

    test_ApplicationDataStatics();
    test_ApplicationDataManagerStatics();

    RoUninitialize();
}
