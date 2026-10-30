/*
 * Copyright (C) 2026 Mirko Brombin
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
 */

#define COBJMACROS
#include "initguid.h"
#include <stdarg.h>

#include "windef.h"
#include "winbase.h"
#include "winstring.h"

#include "roapi.h"

#define WIDL_using_Windows_Devices_Input
#include "windows.devices.input.h"

#include "wine/test.h"

static void test_pen_device(void)
{
    static const WCHAR class_name[] = L"Windows.Devices.Input.PenDevice";
    IPenDeviceStatics *statics = NULL;
    IActivationFactory *factory = NULL;
    IPenDevice *device = (void *)0xdeadbeef;
    HSTRING str;
    HRESULT hr;

    hr = WindowsCreateString( class_name, ARRAY_SIZE(class_name) - 1, &str );
    ok( hr == S_OK, "got hr %#lx.\n", hr );
    hr = RoGetActivationFactory( str, &IID_IActivationFactory, (void **)&factory );
    WindowsDeleteString( str );
    ok( hr == S_OK || broken(hr == REGDB_E_CLASSNOTREG), "got hr %#lx.\n", hr );
    if (hr == REGDB_E_CLASSNOTREG)
    {
        win_skip( "PenDevice runtimeclass not registered.\n" );
        return;
    }

    hr = IActivationFactory_QueryInterface( factory, &IID_IPenDeviceStatics, (void **)&statics );
    ok( hr == S_OK, "got hr %#lx.\n", hr );

    hr = IPenDeviceStatics_GetFromPointerId( statics, 0, NULL );
    ok( hr == E_POINTER, "got hr %#lx.\n", hr );
    hr = IPenDeviceStatics_GetFromPointerId( statics, 0, &device );
    ok( hr == S_OK, "got hr %#lx.\n", hr );
    ok( device == NULL, "got device %p.\n", device );

    IPenDeviceStatics_Release( statics );
    IActivationFactory_Release( factory );
}

START_TEST(input)
{
    RoInitialize( RO_INIT_MULTITHREADED );
    test_pen_device();
    RoUninitialize();
}
