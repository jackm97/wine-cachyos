/*
 * Copyright (C) 2024 Mohamad Al-Jaf
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
#include <stdio.h>

#include "windef.h"
#include "winbase.h"
#include "winstring.h"

#include "roapi.h"

#define WIDL_using_Windows_Foundation
#define WIDL_using_Windows_Foundation_Collections
#include "windows.foundation.h"
#define WIDL_using_Windows_Security_Authentication_OnlineId
#include "windows.security.authentication.onlineid.h"
#define WIDL_using_Windows_Security_Credentials
#include "windows.security.credentials.h"
#define WIDL_using_Windows_Security_Authentication_Web_Core
#include "windows.security.authentication.web.core.h"
#include "webauthenticationcoremanagerinterop.h"

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

static void test_AuthenticatorStatics(void)
{
    static const WCHAR *authenticator_statics_name = L"Windows.Security.Authentication.OnlineId.OnlineIdSystemAuthenticator";
    IOnlineIdSystemAuthenticatorForUser *authenticator_for_user = (void *)0xdeadbeef;
    IOnlineIdSystemAuthenticatorStatics *authenticator_statics = (void *)0xdeadbeef;
    IActivationFactory *factory = (void *)0xdeadbeef;
    HSTRING str;
    GUID application_id = {0}, value = {0};
    HRESULT hr;
    LONG ref;

    hr = WindowsCreateString( authenticator_statics_name, wcslen( authenticator_statics_name ), &str );
    ok( hr == S_OK, "got hr %#lx.\n", hr );

    hr = RoGetActivationFactory( str, &IID_IActivationFactory, (void **)&factory );
    WindowsDeleteString( str );
    ok( hr == S_OK || broken( hr == REGDB_E_CLASSNOTREG ), "got hr %#lx.\n", hr );
    if (hr == REGDB_E_CLASSNOTREG)
    {
        win_skip( "%s runtimeclass not registered, skipping tests.\n", wine_dbgstr_w( authenticator_statics_name ) );
        return;
    }

    check_interface( factory, &IID_IUnknown );
    check_interface( factory, &IID_IInspectable );
    check_interface( factory, &IID_IAgileObject );

    hr = IActivationFactory_QueryInterface( factory, &IID_IOnlineIdSystemAuthenticatorStatics, (void **)&authenticator_statics );
    ok( hr == S_OK, "got hr %#lx.\n", hr );

    hr = IOnlineIdSystemAuthenticatorStatics_get_Default( authenticator_statics, NULL );
    ok( hr == E_POINTER, "got hr %#lx.\n", hr );
    hr = IOnlineIdSystemAuthenticatorStatics_get_Default( authenticator_statics, &authenticator_for_user );
    ok( hr == S_OK, "got hr %#lx.\n", hr );

    check_interface( authenticator_for_user, &IID_IAgileObject );

    hr = IOnlineIdSystemAuthenticatorForUser_get_ApplicationId( authenticator_for_user, &value );
    ok( hr == S_OK, "got hr %#lx.\n", hr );
    ok( IsEqualGUID( &value, &application_id ), "got application id %s.\n", wine_dbgstr_guid( &value ) );
    application_id.Data1 = 0xd3590ed6;
    application_id.Data2 = 0x52b3;
    application_id.Data3 = 0x4102;
    application_id.Data4[0] = 0xae;
    application_id.Data4[1] = 0xff;
    application_id.Data4[2] = 0xaa;
    application_id.Data4[3] = 0xd2;
    application_id.Data4[4] = 0x29;
    application_id.Data4[5] = 0x2a;
    application_id.Data4[6] = 0xb0;
    application_id.Data4[7] = 0x1c;
    hr = IOnlineIdSystemAuthenticatorForUser_put_ApplicationId( authenticator_for_user, application_id );
    ok( hr == S_OK, "got hr %#lx.\n", hr );
    hr = IOnlineIdSystemAuthenticatorForUser_get_ApplicationId( authenticator_for_user, &value );
    ok( hr == S_OK, "got hr %#lx.\n", hr );
    ok( IsEqualGUID( &value, &application_id ), "got application id %s.\n", wine_dbgstr_guid( &value ) );

    IOnlineIdSystemAuthenticatorForUser_Release( authenticator_for_user );
    ref = IOnlineIdSystemAuthenticatorStatics_Release( authenticator_statics );
    ok( ref == 2, "got ref %ld.\n", ref );
    ref = IActivationFactory_Release( factory );
    ok( ref == 1, "got ref %ld.\n", ref );
}

static void test_TicketStatics(void)
{
    static const WCHAR *ticket_statics_name = L"Windows.Security.Authentication.OnlineId.OnlineIdServiceTicketRequest";
    IOnlineIdServiceTicketRequestFactory *ticket_statics = (void *)0xdeadbeef;
    IOnlineIdServiceTicketRequest *request = NULL;
    IActivationFactory *factory = (void *)0xdeadbeef;
    HSTRING str, service, policy, value;
    HRESULT hr;
    LONG ref;

    hr = WindowsCreateString( ticket_statics_name, wcslen( ticket_statics_name ), &str );
    ok( hr == S_OK, "got hr %#lx.\n", hr );

    hr = RoGetActivationFactory( str, &IID_IActivationFactory, (void **)&factory );
    WindowsDeleteString( str );
    ok( hr == S_OK || broken( hr == REGDB_E_CLASSNOTREG ), "got hr %#lx.\n", hr );
    if (hr == REGDB_E_CLASSNOTREG)
    {
        win_skip( "%s runtimeclass not registered, skipping tests.\n", wine_dbgstr_w( ticket_statics_name ) );
        return;
    }

    check_interface( factory, &IID_IUnknown );
    check_interface( factory, &IID_IInspectable );

    hr = IActivationFactory_QueryInterface( factory, &IID_IOnlineIdServiceTicketRequestFactory, (void **)&ticket_statics );
    ok( hr == S_OK, "got hr %#lx.\n", hr );

    WindowsCreateString( L"openid profile", 14, &service );
    WindowsCreateString( L"JWT", 3, &policy );
    hr = IOnlineIdServiceTicketRequestFactory_CreateOnlineIdServiceTicketRequest( ticket_statics, service, policy, &request );
    ok( hr == S_OK, "got hr %#lx.\n", hr );
    check_interface( request, &IID_IAgileObject );
    hr = IOnlineIdServiceTicketRequest_get_Service( request, &value );
    ok( hr == S_OK, "got hr %#lx.\n", hr );
    ok( !wcscmp( WindowsGetStringRawBuffer( value, NULL ), L"openid profile" ), "got service %s.\n",
        wine_dbgstr_w( WindowsGetStringRawBuffer( value, NULL ) ) );
    WindowsDeleteString( value );
    hr = IOnlineIdServiceTicketRequest_get_Policy( request, &value );
    ok( hr == S_OK, "got hr %#lx.\n", hr );
    ok( !wcscmp( WindowsGetStringRawBuffer( value, NULL ), L"JWT" ), "got policy %s.\n",
        wine_dbgstr_w( WindowsGetStringRawBuffer( value, NULL ) ) );
    WindowsDeleteString( value );
    WindowsDeleteString( service );
    WindowsDeleteString( policy );
    IOnlineIdServiceTicketRequest_Release( request );

    ref = IOnlineIdServiceTicketRequestFactory_Release( ticket_statics );
    ok( ref == 2, "got ref %ld.\n", ref );
    ref = IActivationFactory_Release( factory );
    ok( ref == 1, "got ref %ld.\n", ref );
}

static void test_ticket_operation(void)
{
    static const WCHAR *authenticator_name = L"Windows.Security.Authentication.OnlineId.OnlineIdSystemAuthenticator";
    static const WCHAR *request_name = L"Windows.Security.Authentication.OnlineId.OnlineIdServiceTicketRequest";
    IAsyncOperation_OnlineIdSystemTicketResult *operation = NULL;
    IOnlineIdSystemAuthenticatorForUser *authenticator = NULL;
    IOnlineIdSystemAuthenticatorStatics *statics = NULL;
    IOnlineIdServiceTicketRequestFactory *request_factory = NULL;
    IOnlineIdServiceTicketRequest *request = NULL;
    IOnlineIdSystemTicketResult *result = NULL;
    IOnlineIdSystemIdentity *identity = NULL;
    IOnlineIdServiceTicket *ticket = NULL;
    OnlineIdSystemTicketStatus ticket_status;
    IAsyncInfo *info = NULL;
    AsyncStatus status;
    HSTRING name, service, policy, value;
    GUID application_id = {0x2b379600, 0xb42b, 0x4fe9, {0xa5, 0x9c, 0xa3, 0x12, 0xfb, 0x93, 0x49, 0x35}};
    HRESULT hr;
    unsigned int i;

    if (!GetEnvironmentVariableA( "SODA_IDENTITY_BRIDGE_SOCKET", NULL, 0 ))
    {
        win_skip( "host identity bridge unavailable.\n" );
        return;
    }
    WindowsCreateString( request_name, wcslen(request_name), &name );
    hr = RoGetActivationFactory( name, &IID_IOnlineIdServiceTicketRequestFactory, (void **)&request_factory );
    WindowsDeleteString( name );
    ok( hr == S_OK, "got hr %#lx.\n", hr );
    WindowsCreateString( L"openid", 6, &service );
    WindowsCreateString( L"JWT", 3, &policy );
    hr = IOnlineIdServiceTicketRequestFactory_CreateOnlineIdServiceTicketRequest( request_factory, service, policy, &request );
    ok( hr == S_OK, "got hr %#lx.\n", hr );
    WindowsDeleteString( service );
    WindowsDeleteString( policy );

    WindowsCreateString( authenticator_name, wcslen(authenticator_name), &name );
    hr = RoGetActivationFactory( name, &IID_IOnlineIdSystemAuthenticatorStatics, (void **)&statics );
    WindowsDeleteString( name );
    ok( hr == S_OK, "got hr %#lx.\n", hr );
    hr = IOnlineIdSystemAuthenticatorStatics_get_Default( statics, &authenticator );
    ok( hr == S_OK, "got hr %#lx.\n", hr );
    hr = IOnlineIdSystemAuthenticatorForUser_put_ApplicationId( authenticator, application_id );
    ok( hr == S_OK, "got hr %#lx.\n", hr );
    hr = IOnlineIdSystemAuthenticatorForUser_GetTicketAsync( authenticator, request, &operation );
    ok( hr == S_OK, "got hr %#lx.\n", hr );
    hr = IAsyncOperation_OnlineIdSystemTicketResult_QueryInterface( operation, &IID_IAsyncInfo, (void **)&info );
    ok( hr == S_OK, "got hr %#lx.\n", hr );
    for (i = 0; i < 100; ++i)
    {
        hr = IAsyncInfo_get_Status( info, &status );
        if (status != Started) break;
        Sleep( 10 );
    }
    ok( status == Completed, "got status %u, hr %#lx.\n", status, hr );
    hr = IAsyncOperation_OnlineIdSystemTicketResult_GetResults( operation, &result );
    ok( hr == S_OK, "got hr %#lx.\n", hr );
    hr = IOnlineIdSystemTicketResult_get_Status( result, &ticket_status );
    ok( hr == S_OK, "got hr %#lx.\n", hr );
    ok( ticket_status == OnlineIdSystemTicketStatus_Success, "got ticket status %u.\n", ticket_status );
    hr = IOnlineIdSystemTicketResult_get_Identity( result, &identity );
    ok( hr == S_OK, "got hr %#lx.\n", hr );
    hr = IOnlineIdSystemIdentity_get_Ticket( identity, &ticket );
    ok( hr == S_OK, "got hr %#lx.\n", hr );
    hr = IOnlineIdServiceTicket_get_Value( ticket, &value );
    ok( hr == S_OK, "got hr %#lx.\n", hr );
    ok( !wcscmp( WindowsGetStringRawBuffer( value, NULL ), L"access-token" ), "got ticket %s.\n",
        wine_dbgstr_w( WindowsGetStringRawBuffer( value, NULL ) ) );
    WindowsDeleteString( value );

    IOnlineIdServiceTicket_Release( ticket );
    IOnlineIdSystemIdentity_Release( identity );
    IOnlineIdSystemTicketResult_Release( result );
    IAsyncInfo_Release( info );
    IAsyncOperation_OnlineIdSystemTicketResult_Release( operation );
    IOnlineIdSystemAuthenticatorForUser_Release( authenticator );
    IOnlineIdSystemAuthenticatorStatics_Release( statics );
    IOnlineIdServiceTicketRequest_Release( request );
    IOnlineIdServiceTicketRequestFactory_Release( request_factory );
}

static AsyncStatus wait_operation( IUnknown *operation )
{
    IAsyncInfo *info = NULL;
    AsyncStatus status = Started;
    HRESULT hr;
    unsigned int i;

    hr = IUnknown_QueryInterface( operation, &IID_IAsyncInfo, (void **)&info );
    ok( hr == S_OK, "got hr %#lx.\n", hr );
    if (FAILED(hr)) return Error;
    for (i = 0; i < 100 && status == Started; ++i)
    {
        hr = IAsyncInfo_get_Status( info, &status );
        ok( hr == S_OK, "got hr %#lx.\n", hr );
        if (status == Started) Sleep( 10 );
    }
    IAsyncInfo_Release( info );
    return status;
}

static void test_response_properties( IWebTokenResponse *response )
{
    static const struct { const WCHAR *key, *value; } expected[] =
    {
        {L"Authority", L"https://login.microsoftonline.com/00000000-0000-0000-0000-000000000001/"},
        {L"TokenExpiresOn", L"15746918400"},
        {L"wamcompat_id_token", L"id-token"},
        {L"wamcompat_client_info", L"client-info"},
        {L"wamcompat_scopes", L"openid profile email"},
    };
    IMap_HSTRING_HSTRING *properties;
    IWebAccount *account;
    IWebAccount2 *account2;
    IMapView_HSTRING_HSTRING *view;
    HSTRING key, value;
    unsigned int i;
    HRESULT hr;

    hr = IWebTokenResponse_get_Properties( response, &properties );
    ok( hr == S_OK, "got hr %#lx.\n", hr );
    if (FAILED(hr)) return;
    for (i = 0; i < ARRAY_SIZE(expected); ++i)
    {
        WindowsCreateString( expected[i].key, wcslen(expected[i].key), &key );
        value = NULL;
        hr = IMap_HSTRING_HSTRING_Lookup( properties, key, &value );
        ok( hr == S_OK, "%s: got hr %#lx.\n", wine_dbgstr_w(expected[i].key), hr );
        if (SUCCEEDED(hr))
            ok( !wcscmp( WindowsGetStringRawBuffer(value, NULL), expected[i].value ),
                "%s: unexpected value.\n", wine_dbgstr_w(expected[i].key) );
        WindowsDeleteString( value );
        WindowsDeleteString( key );
    }
    IMap_HSTRING_HSTRING_Release( properties );
    hr = IWebTokenResponse_get_WebAccount( response, &account );
    ok( hr == S_OK, "got hr %#lx.\n", hr );
    if (FAILED(hr)) return;
    hr = IWebAccount_QueryInterface( account, &IID_IWebAccount2, (void **)&account2 );
    ok( hr == S_OK, "got hr %#lx.\n", hr );
    if (SUCCEEDED(hr))
    {
        hr = IWebAccount2_get_Properties( account2, &view );
        ok( hr == S_OK, "got hr %#lx.\n", hr );
        if (SUCCEEDED(hr))
        {
            WindowsCreateString( L"OID", 3, &key );
            value = NULL;
            hr = IMapView_HSTRING_HSTRING_Lookup( view, key, &value );
            ok( hr == S_OK, "got hr %#lx.\n", hr );
            if (SUCCEEDED(hr))
                ok( !wcscmp( WindowsGetStringRawBuffer(value, NULL), L"account-id" ), "unexpected account ID.\n" );
            WindowsDeleteString( value );
            WindowsDeleteString( key );
            IMapView_HSTRING_HSTRING_Release( view );
        }
        IWebAccount2_Release( account2 );
    }
    IWebAccount_Release( account );
}

static void test_web_authentication_core(void)
{
    static const WCHAR manager_name[] = L"Windows.Security.Authentication.Web.Core.WebAuthenticationCoreManager";
    static const WCHAR request_name[] = L"Windows.Security.Authentication.Web.Core.WebTokenRequest";
    IWebAuthenticationCoreManagerStatics *manager = NULL;
    IWebAuthenticationCoreManagerStatics4 *manager4 = NULL;
    IWebAuthenticationCoreManagerInterop *interop = NULL;
    IWebTokenRequestFactory *request_factory = NULL;
    IAsyncOperation_WebAccountProvider *provider_operation = NULL;
    IAsyncOperation_WebTokenRequestResult *token_operation = NULL;
    IWebAccountProvider *provider = NULL;
    IWebAccountProvider *response_provider = NULL;
    IWebAccountProvider4 *provider4 = NULL;
    IWebAccountProvider2 *response_provider2 = NULL;
    IWebAccount *account = NULL;
    IWebTokenRequest *request = NULL;
    IMap_HSTRING_HSTRING *properties;
    IIterable_IKeyValuePair_HSTRING_HSTRING *property_iterable = NULL;
    IWebProviderError *response_error = NULL;
    IWebTokenRequestResult *result = NULL;
    IVectorView_WebTokenResponse *responses = NULL;
    IWebTokenResponse *response = NULL;
    WebTokenRequestStatus response_status;
    IActivationFactory *factory = NULL;
    HSTRING name = NULL, provider_id = NULL, authority = NULL, scope = NULL, client_id = NULL, token = NULL;
    HSTRING response_authority = NULL, error_message = NULL;
    AsyncStatus status;
    UINT32 size, error_code;
    boolean is_system_provider;
    HRESULT hr;

    WindowsCreateString( manager_name, ARRAY_SIZE(manager_name) - 1, &name );
    hr = RoGetActivationFactory( name, &IID_IActivationFactory, (void **)&factory );
    ok( hr == S_OK, "got hr %#lx.\n", hr );
    WindowsDeleteString( name );
    if (FAILED(hr)) return;
    check_interface( factory, &IID_IAgileObject );
    hr = IActivationFactory_QueryInterface( factory, &IID_IWebAuthenticationCoreManagerStatics,
                                            (void **)&manager );
    ok( hr == S_OK, "got hr %#lx.\n", hr );
    IActivationFactory_Release( factory );
    if (FAILED(hr)) return;

    hr = IWebAuthenticationCoreManagerStatics_QueryInterface( manager,
        &IID_IWebAuthenticationCoreManagerInterop, (void **)&interop );
    ok( hr == S_OK, "got hr %#lx.\n", hr );
    if (interop)
    {
        check_interface( interop, &IID_IActivationFactory );
        hr = IWebAuthenticationCoreManagerInterop_RequestTokenForWindowAsync( interop,
            NULL, NULL, &IID_IAsyncOperation_WebTokenRequestResult, NULL );
        ok( hr == E_POINTER, "got hr %#lx.\n", hr );
        token_operation = (void *)0xdeadbeef;
        hr = IWebAuthenticationCoreManagerInterop_RequestTokenForWindowAsync( interop,
            NULL, NULL, &IID_IAsyncOperation_WebTokenRequestResult, (void **)&token_operation );
        ok( hr == E_POINTER && !token_operation, "got hr %#lx, operation %p.\n", hr, token_operation );
        token_operation = (void *)0xdeadbeef;
        hr = IWebAuthenticationCoreManagerInterop_RequestTokenForWindowAsync( interop,
            NULL, (IInspectable *)manager, &IID_IAsyncOperation_WebTokenRequestResult,
            (void **)&token_operation );
        ok( hr == E_NOINTERFACE && !token_operation,
            "got hr %#lx, operation %p.\n", hr, token_operation );
        token_operation = (void *)0xdeadbeef;
        hr = IWebAuthenticationCoreManagerInterop_RequestTokenWithWebAccountForWindowAsync( interop,
            NULL, NULL, NULL, &IID_IAsyncOperation_WebTokenRequestResult, (void **)&token_operation );
        ok( hr == E_POINTER && !token_operation,
            "got hr %#lx, operation %p.\n", hr, token_operation );
    }

    WindowsCreateString( L"https://login.microsoft.com", 27, &provider_id );
    WindowsCreateString( L"common", 6, &authority );
    hr = IWebAuthenticationCoreManagerStatics_QueryInterface( manager,
        &IID_IWebAuthenticationCoreManagerStatics4, (void **)&manager4 );
    ok( hr == S_OK, "got hr %#lx.\n", hr );
    hr = IWebAuthenticationCoreManagerStatics4_FindSystemAccountProviderWithAuthorityAsync(
        manager4, provider_id, authority, &provider_operation );
    ok( hr == S_OK, "got hr %#lx.\n", hr );
    status = wait_operation( (IUnknown *)provider_operation );
    ok( status == Completed, "got status %u.\n", status );
    hr = IAsyncOperation_WebAccountProvider_GetResults( provider_operation, &provider );
    ok( hr == S_OK, "got hr %#lx.\n", hr );
    hr = IWebAccountProvider_QueryInterface( provider, &IID_IWebAccountProvider4, (void **)&provider4 );
    ok( hr == S_OK, "got hr %#lx.\n", hr );
    hr = IWebAccountProvider4_get_IsSystemProvider( provider4, &is_system_provider );
    ok( hr == S_OK, "got hr %#lx.\n", hr );
    ok( is_system_provider, "provider is not a system provider.\n" );
    IWebAccountProvider4_Release( provider4 );
    IWebAccountProvider_Release( provider );
    IAsyncOperation_WebAccountProvider_Release( provider_operation );

    hr = IWebAuthenticationCoreManagerStatics_FindAccountProviderWithAuthorityAsync(
        manager, provider_id, authority, &provider_operation );
    ok( hr == S_OK, "got hr %#lx.\n", hr );
    status = wait_operation( (IUnknown *)provider_operation );
    ok( status == Completed, "got status %u.\n", status );
    hr = IAsyncOperation_WebAccountProvider_GetResults( provider_operation, &provider );
    ok( hr == S_OK, "got hr %#lx.\n", hr );
    check_interface( provider, &IID_IWebAccountProvider2 );
    IAsyncOperation_WebAccountProvider_Release( provider_operation );
    WindowsDeleteString( provider_id );
    WindowsDeleteString( authority );

    WindowsCreateString( request_name, ARRAY_SIZE(request_name) - 1, &name );
    hr = RoGetActivationFactory( name, &IID_IWebTokenRequestFactory, (void **)&request_factory );
    ok( hr == S_OK, "got hr %#lx.\n", hr );
    WindowsDeleteString( name );
    WindowsCreateString( L"openid profile", 14, &scope );
    WindowsCreateString( L"d3590ed6-52b3-4102-aeff-aad2292ab01c", 36, &client_id );
    hr = IWebTokenRequestFactory_Create( request_factory, provider, scope, client_id, &request );
    ok( hr == S_OK, "got hr %#lx.\n", hr );
    check_interface( request, &IID_IWebTokenRequest2 );
    check_interface( request, &IID_IWebTokenRequest3 );
    hr = IWebTokenRequest_get_Properties( request, &properties );
    ok( hr == S_OK, "got hr %#lx.\n", hr );
    hr = IMap_HSTRING_HSTRING_QueryInterface( properties,
        &IID_IIterable_IKeyValuePair_HSTRING_HSTRING, (void **)&property_iterable );
    ok( hr == S_OK, "got hr %#lx.\n", hr );
    if (property_iterable)
    {
        IIterator_IKeyValuePair_HSTRING_HSTRING *iterator;
        IKeyValuePair_HSTRING_HSTRING *pair, *items[2];
        IMapView_HSTRING_HSTRING *snapshot;
        HSTRING key, value, actual;
        UINT32 count;
        boolean current, replaced;

        hr = IIterable_IKeyValuePair_HSTRING_HSTRING_First( property_iterable, &iterator );
        ok( hr == S_OK, "got hr %#lx.\n", hr );
        hr = IIterator_IKeyValuePair_HSTRING_HSTRING_get_HasCurrent( iterator, &current );
        ok( hr == S_OK && !current, "got hr %#lx, current %u.\n", hr, current );
        pair = (void *)0xdeadbeef;
        hr = IIterator_IKeyValuePair_HSTRING_HSTRING_get_Current( iterator, &pair );
        ok( hr == E_BOUNDS && !pair, "got hr %#lx, pair %p.\n", hr, pair );
        WindowsCreateString( L"test-property", 13, &key );
        WindowsCreateString( L"test-value", 10, &value );
        hr = IMap_HSTRING_HSTRING_Insert( properties, key, value, &replaced );
        ok( hr == S_OK && !replaced, "got hr %#lx, replaced %u.\n", hr, replaced );
        hr = IIterator_IKeyValuePair_HSTRING_HSTRING_get_HasCurrent( iterator, &current );
        ok( hr == E_CHANGED_STATE, "got hr %#lx.\n", hr );
        IIterator_IKeyValuePair_HSTRING_HSTRING_Release( iterator );
        hr = IIterable_IKeyValuePair_HSTRING_HSTRING_First( property_iterable, &iterator );
        ok( hr == S_OK, "got hr %#lx.\n", hr );
        hr = IIterator_IKeyValuePair_HSTRING_HSTRING_get_Current( iterator, &pair );
        ok( hr == S_OK, "got hr %#lx.\n", hr );
        check_interface( pair, &IID_IAgileObject );
        hr = IKeyValuePair_HSTRING_HSTRING_get_Key( pair, &actual );
        ok( hr == S_OK && !wcscmp( WindowsGetStringRawBuffer( actual, NULL ), L"test-property" ),
            "unexpected key, hr %#lx.\n", hr );
        WindowsDeleteString( actual );
        hr = IKeyValuePair_HSTRING_HSTRING_get_Value( pair, &actual );
        ok( hr == S_OK && !wcscmp( WindowsGetStringRawBuffer( actual, NULL ), L"test-value" ),
            "unexpected value, hr %#lx.\n", hr );
        WindowsDeleteString( actual );
        hr = IIterator_IKeyValuePair_HSTRING_HSTRING_GetMany( iterator, 2, items, &count );
        ok( hr == S_OK && count == 1, "got hr %#lx, count %u.\n", hr, count );
        if (count) IKeyValuePair_HSTRING_HSTRING_Release( items[0] );
        hr = IIterator_IKeyValuePair_HSTRING_HSTRING_MoveNext( iterator, &current );
        ok( hr == S_OK && !current, "got hr %#lx, current %u.\n", hr, current );
        hr = IMap_HSTRING_HSTRING_GetView( properties, &snapshot );
        ok( hr == S_OK, "got hr %#lx.\n", hr );
        hr = IMap_HSTRING_HSTRING_Clear( properties );
        ok( hr == S_OK, "got hr %#lx.\n", hr );
        hr = IMapView_HSTRING_HSTRING_Lookup( snapshot, key, &actual );
        ok( hr == S_OK && !wcscmp( WindowsGetStringRawBuffer( actual, NULL ), L"test-value" ),
            "snapshot changed after clear, hr %#lx.\n", hr );
        WindowsDeleteString( actual );
        IMapView_HSTRING_HSTRING_Release( snapshot );
        hr = IIterator_IKeyValuePair_HSTRING_HSTRING_get_Current( iterator, &items[0] );
        ok( hr == E_CHANGED_STATE && !items[0], "got hr %#lx.\n", hr );
        hr = IKeyValuePair_HSTRING_HSTRING_get_Value( pair, &actual );
        ok( hr == S_OK && !wcscmp( WindowsGetStringRawBuffer( actual, NULL ), L"test-value" ),
            "pair changed after clear, hr %#lx.\n", hr );
        WindowsDeleteString( actual );
        IKeyValuePair_HSTRING_HSTRING_Release( pair );
        IIterator_IKeyValuePair_HSTRING_HSTRING_Release( iterator );
        WindowsDeleteString( key );
        WindowsDeleteString( value );
        IIterable_IKeyValuePair_HSTRING_HSTRING_Release( property_iterable );
    }
    IMap_HSTRING_HSTRING_Release( properties );
    WindowsDeleteString( scope );
    WindowsDeleteString( client_id );
    IWebTokenRequestFactory_Release( request_factory );

    hr = IWebAuthenticationCoreManagerStatics_GetTokenSilentlyAsync( manager, request, &token_operation );
    ok( hr == S_OK, "got hr %#lx.\n", hr );
    status = wait_operation( (IUnknown *)token_operation );
    ok( status == Completed, "got status %u.\n", status );
    hr = IAsyncOperation_WebTokenRequestResult_GetResults( token_operation, &result );
    ok( hr == S_OK, "got hr %#lx.\n", hr );
    hr = IWebTokenRequestResult_get_ResponseStatus( result, &response_status );
    ok( hr == S_OK, "got hr %#lx.\n", hr );
    if (GetEnvironmentVariableA( "SODA_IDENTITY_BRIDGE_SOCKET", NULL, 0 ))
        ok( response_status == WebTokenRequestStatus_Success, "got response status %u.\n", response_status );
    else
        ok( response_status == WebTokenRequestStatus_UserInteractionRequired,
            "got response status %u.\n", response_status );
    hr = IWebTokenRequestResult_get_ResponseError( result, &response_error );
    ok( hr == S_OK, "got hr %#lx.\n", hr );
    if (response_status == WebTokenRequestStatus_Success)
        ok( !response_error, "got unexpected provider error %p.\n", response_error );
    else
    {
        ok( !!response_error, "missing provider error.\n" );
        if (response_error)
        {
            hr = IWebProviderError_get_ErrorCode( response_error, &error_code );
            ok( hr == S_OK && error_code, "got hr %#lx, error code %#x.\n", hr, error_code );
            hr = IWebProviderError_get_ErrorMessage( response_error, &error_message );
            ok( hr == S_OK && !WindowsIsStringEmpty(error_message), "got hr %#lx.\n", hr );
            WindowsDeleteString( error_message );
            IWebProviderError_Release( response_error );
        }
    }
    IWebTokenRequestResult_Release( result );
    IAsyncOperation_WebTokenRequestResult_Release( token_operation );

    if (GetEnvironmentVariableA( "SODA_IDENTITY_BRIDGE_SOCKET", NULL, 0 ))
    {
        if (interop)
        {
            token_operation = (void *)0xdeadbeef;
            hr = IWebAuthenticationCoreManagerInterop_RequestTokenForWindowAsync( interop,
                NULL, (IInspectable *)request, &IID_IWebAccount, (void **)&token_operation );
            ok( hr == E_NOINTERFACE && !token_operation,
                "got hr %#lx, operation %p.\n", hr, token_operation );
            hr = IWebAuthenticationCoreManagerInterop_RequestTokenForWindowAsync( interop,
                NULL, (IInspectable *)request, &IID_IAsyncOperation_WebTokenRequestResult,
                (void **)&token_operation );
            ok( hr == S_OK, "got hr %#lx.\n", hr );
            if (SUCCEEDED(hr))
            {
                status = wait_operation( (IUnknown *)token_operation );
                ok( status == Completed, "got status %u.\n", status );
                hr = IAsyncOperation_WebTokenRequestResult_GetResults( token_operation, &result );
                ok( hr == S_OK, "got hr %#lx.\n", hr );
                if (SUCCEEDED(hr))
                {
                    IWebTokenRequestResult_get_ResponseStatus( result, &response_status );
                    ok( response_status == WebTokenRequestStatus_Success,
                        "got response status %u.\n", response_status );
                    IWebTokenRequestResult_Release( result );
                }
                IAsyncOperation_WebTokenRequestResult_Release( token_operation );
            }
        }
        hr = IWebAuthenticationCoreManagerStatics_RequestTokenAsync( manager, request, &token_operation );
        ok( hr == S_OK, "got hr %#lx.\n", hr );
        status = wait_operation( (IUnknown *)token_operation );
        ok( status == Completed, "got status %u.\n", status );
        hr = IAsyncOperation_WebTokenRequestResult_GetResults( token_operation, &result );
        ok( hr == S_OK, "got hr %#lx.\n", hr );
        hr = IWebTokenRequestResult_get_ResponseStatus( result, &response_status );
        ok( hr == S_OK, "got hr %#lx.\n", hr );
        ok( response_status == WebTokenRequestStatus_Success, "got response status %u.\n", response_status );
        hr = IWebTokenRequestResult_get_ResponseData( result, &responses );
        ok( hr == S_OK, "got hr %#lx.\n", hr );
        hr = IVectorView_WebTokenResponse_get_Size( responses, &size );
        ok( hr == S_OK && size == 1, "got hr %#lx, size %u.\n", hr, size );
        hr = IVectorView_WebTokenResponse_GetAt( responses, 0, &response );
        ok( hr == S_OK, "got hr %#lx.\n", hr );
        if (SUCCEEDED(hr)) test_response_properties( response );
        hr = IWebTokenResponse_get_Token( response, &token );
        ok( hr == S_OK, "got hr %#lx.\n", hr );
        ok( !wcscmp( WindowsGetStringRawBuffer( token, NULL ), L"access-token" ), "got token %s.\n",
            wine_dbgstr_w( WindowsGetStringRawBuffer( token, NULL ) ) );
        WindowsDeleteString( token );
        hr = IWebTokenResponse_get_WebAccount( response, &account );
        ok( hr == S_OK, "got hr %#lx.\n", hr );
        hr = IWebAccount_get_WebAccountProvider( account, &response_provider );
        ok( hr == S_OK, "got hr %#lx.\n", hr );
        hr = IWebAccountProvider_QueryInterface( response_provider, &IID_IWebAccountProvider2,
                                                 (void **)&response_provider2 );
        ok( hr == S_OK, "got hr %#lx.\n", hr );
        hr = IWebAccountProvider2_get_Authority( response_provider2, &response_authority );
        ok( hr == S_OK, "got hr %#lx.\n", hr );
        ok( !wcscmp( WindowsGetStringRawBuffer( response_authority, NULL ), L"common" ),
            "got authority %s.\n", wine_dbgstr_w( WindowsGetStringRawBuffer( response_authority, NULL ) ) );
        WindowsDeleteString( response_authority );
        IWebAccountProvider2_Release( response_provider2 );
        IWebAccountProvider_Release( response_provider );
        if (interop)
        {
            IAsyncOperation_WebTokenRequestResult *account_operation;
            IWebTokenRequestResult *account_result;

            account_operation = (void *)0xdeadbeef;
            hr = IWebAuthenticationCoreManagerInterop_RequestTokenWithWebAccountForWindowAsync( interop,
                NULL, (IInspectable *)request, (IInspectable *)request,
                &IID_IAsyncOperation_WebTokenRequestResult, (void **)&account_operation );
            ok( hr == E_NOINTERFACE && !account_operation,
                "got hr %#lx, operation %p.\n", hr, account_operation );
            hr = IWebAuthenticationCoreManagerInterop_RequestTokenWithWebAccountForWindowAsync( interop,
                NULL, (IInspectable *)request, (IInspectable *)account,
                &IID_IAsyncOperation_WebTokenRequestResult, (void **)&account_operation );
            ok( hr == S_OK, "got hr %#lx.\n", hr );
            if (SUCCEEDED(hr))
            {
                status = wait_operation( (IUnknown *)account_operation );
                ok( status == Completed, "got status %u.\n", status );
                hr = IAsyncOperation_WebTokenRequestResult_GetResults( account_operation, &account_result );
                ok( hr == S_OK, "got hr %#lx.\n", hr );
                if (SUCCEEDED(hr))
                {
                    IWebTokenRequestResult_get_ResponseStatus( account_result, &response_status );
                    ok( response_status == WebTokenRequestStatus_Success,
                        "got response status %u.\n", response_status );
                    IWebTokenRequestResult_Release( account_result );
                }
                IAsyncOperation_WebTokenRequestResult_Release( account_operation );
            }
        }
        IWebAccount_Release( account );
        IWebTokenResponse_Release( response );
        IVectorView_WebTokenResponse_Release( responses );
        IWebTokenRequestResult_Release( result );
        IAsyncOperation_WebTokenRequestResult_Release( token_operation );
    }
    IWebTokenRequest_Release( request );
    IWebAccountProvider_Release( provider );
    IWebAuthenticationCoreManagerStatics4_Release( manager4 );
    if (interop) IWebAuthenticationCoreManagerInterop_Release( interop );
    IWebAuthenticationCoreManagerStatics_Release( manager );
}

static void test_find_all_accounts(void)
{
    static const WCHAR class_name[] = L"Windows.Security.Authentication.Web.Core.WebAuthenticationCoreManager";
    IWebAuthenticationCoreManagerStatics *manager;
    IWebAuthenticationCoreManagerStatics4 *manager4;
    IAsyncOperation_WebAccountProvider *provider_operation;
    IAsyncOperation_FindAllAccountsResult *operation;
    IWebAccountProvider *provider;
    IFindAllAccountsResult *result;
    IVectorView_WebAccount *accounts;
    IIterable_WebAccount *iterable;
    IIterator_WebAccount *iterator;
    IWebProviderError *error;
    IWebAccount *account, *items[2];
    IWebAccount2 *account2;
    FindAllWebAccountsStatus result_status;
    HSTRING name, provider_id, authority, client_id, missing_client, value;
    AsyncStatus status;
    UINT32 size, index, count;
    boolean found;
    HRESULT hr;
    unsigned int i;

    WindowsCreateString( class_name, ARRAY_SIZE(class_name) - 1, &name );
    hr = RoGetActivationFactory( name, &IID_IWebAuthenticationCoreManagerStatics, (void **)&manager );
    WindowsDeleteString( name );
    ok( hr == S_OK, "got hr %#lx.\n", hr );
    if (FAILED(hr)) return;
    hr = IWebAuthenticationCoreManagerStatics_QueryInterface( manager,
        &IID_IWebAuthenticationCoreManagerStatics4, (void **)&manager4 );
    ok( hr == S_OK, "got hr %#lx.\n", hr );
    WindowsCreateString( L"https://login.microsoft.com", 27, &provider_id );
    WindowsCreateString( L"common", 6, &authority );
    WindowsCreateString( L"d3590ed6-52b3-4102-aeff-aad2292ab01c", 36, &client_id );
    WindowsCreateString( L"missing-client", 14, &missing_client );
    hr = IWebAuthenticationCoreManagerStatics_FindAccountProviderWithAuthorityAsync(
        manager, provider_id, authority, &provider_operation );
    ok( hr == S_OK, "got hr %#lx.\n", hr );
    status = wait_operation( (IUnknown *)provider_operation );
    ok( status == Completed, "got status %u.\n", status );
    hr = IAsyncOperation_WebAccountProvider_GetResults( provider_operation, &provider );
    ok( hr == S_OK, "got hr %#lx.\n", hr );
    IAsyncOperation_WebAccountProvider_Release( provider_operation );

    hr = IWebAuthenticationCoreManagerStatics4_FindAllAccountsWithClientIdAsync(
        manager4, provider, client_id, NULL );
    ok( hr == E_POINTER, "got hr %#lx.\n", hr );
    operation = (void *)0xdeadbeef;
    hr = IWebAuthenticationCoreManagerStatics4_FindAllAccountsAsync( manager4, NULL, &operation );
    ok( hr == E_POINTER && !operation, "got hr %#lx, operation %p.\n", hr, operation );
    for (i = 0; i < 3; ++i)
    {
        operation = NULL;
        if (i == 1)
            hr = IWebAuthenticationCoreManagerStatics4_FindAllAccountsAsync( manager4, provider, &operation );
        else
            hr = IWebAuthenticationCoreManagerStatics4_FindAllAccountsWithClientIdAsync(
                manager4, provider, i == 2 ? missing_client : client_id, &operation );
        ok( hr == S_OK, "case %u got hr %#lx.\n", i, hr );
        if (FAILED(hr)) continue;
        check_interface( operation, &IID_IAsyncInfo );
        status = wait_operation( (IUnknown *)operation );
        ok( status == Completed, "case %u got status %u.\n", i, status );
        result = NULL;
        hr = IAsyncOperation_FindAllAccountsResult_GetResults( operation, &result );
        ok( hr == S_OK, "got hr %#lx.\n", hr );
        if (SUCCEEDED(hr))
        {
            check_interface( result, &IID_IFindAllAccountsResult );
            hr = IFindAllAccountsResult_get_Status( result, &result_status );
            ok( hr == S_OK, "got hr %#lx.\n", hr );
            ok( result_status == FindAllWebAccountsStatus_Success, "got status %u.\n", result_status );
            hr = IFindAllAccountsResult_get_Accounts( result, &accounts );
            ok( hr == S_OK, "got hr %#lx.\n", hr );
            if (SUCCEEDED(hr))
            {
                hr = IVectorView_WebAccount_get_Size( accounts, &size );
                ok( hr == S_OK, "got hr %#lx.\n", hr );
                ok( size == (i == 2 ? 0 : 1), "case %u got size %u.\n", i, size );
                hr = IVectorView_WebAccount_QueryInterface( accounts, &IID_IIterable_WebAccount, (void **)&iterable );
                ok( hr == S_OK, "got hr %#lx.\n", hr );
                hr = IIterable_WebAccount_First( iterable, &iterator );
                ok( hr == S_OK, "got hr %#lx.\n", hr );
                IIterable_WebAccount_Release( iterable );
                hr = IIterator_WebAccount_get_HasCurrent( iterator, &found );
                ok( hr == S_OK && found == !!size, "got hr %#lx, found %u.\n", hr, found );
                if (size)
                {
                    hr = IIterator_WebAccount_get_Current( iterator, &account );
                    ok( hr == S_OK, "got hr %#lx.\n", hr );
                    IWebAccount_Release( account );
                }
                hr = IIterator_WebAccount_GetMany( iterator, 2, items, &count );
                ok( hr == S_OK && count == size, "got hr %#lx, count %u.\n", hr, count );
                if (count) IWebAccount_Release( items[0] );
                hr = IIterator_WebAccount_get_Current( iterator, &account );
                ok( hr == E_BOUNDS && !account, "got hr %#lx, account %p.\n", hr, account );
                hr = IIterator_WebAccount_MoveNext( iterator, &found );
                ok( hr == S_OK && !found, "got hr %#lx, found %u.\n", hr, found );
                IIterator_WebAccount_Release( iterator );
                account = (void *)0xdeadbeef;
                hr = IVectorView_WebAccount_GetAt( accounts, size, &account );
                ok( hr == E_BOUNDS && !account, "got hr %#lx, account %p.\n", hr, account );
                hr = IVectorView_WebAccount_GetMany( accounts, size, 2, items, &count );
                ok( hr == S_OK && !count, "got hr %#lx, count %u.\n", hr, count );
                hr = IVectorView_WebAccount_GetMany( accounts, size + 1, 2, items, &count );
                ok( hr == E_BOUNDS && !count, "got hr %#lx, count %u.\n", hr, count );
                if (size)
                {
                    hr = IVectorView_WebAccount_GetAt( accounts, 0, &account );
                    ok( hr == S_OK, "got hr %#lx.\n", hr );
                    hr = IWebAccount_get_UserName( account, &value );
                    ok( hr == S_OK, "got hr %#lx.\n", hr );
                    ok( !wcscmp( WindowsGetStringRawBuffer( value, NULL ), L"test@example.com" ),
                        "unexpected username.\n" );
                    WindowsDeleteString( value );
                    hr = IWebAccount_QueryInterface( account, &IID_IWebAccount2, (void **)&account2 );
                    ok( hr == S_OK, "got hr %#lx.\n", hr );
                    hr = IWebAccount2_get_Id( account2, &value );
                    ok( hr == S_OK, "got hr %#lx.\n", hr );
                    ok( !wcscmp( WindowsGetStringRawBuffer( value, NULL ), L"account-id" ),
                        "unexpected account ID.\n" );
                    WindowsDeleteString( value );
                    IWebAccount2_Release( account2 );
                    hr = IVectorView_WebAccount_IndexOf( accounts, account, &index, &found );
                    ok( hr == S_OK && found && !index, "got hr %#lx, index %u, found %u.\n", hr, index, found );
                    hr = IVectorView_WebAccount_GetMany( accounts, 0, 2, items, &count );
                    ok( hr == S_OK && count == 1 && items[0] == account, "got hr %#lx, count %u.\n", hr, count );
                    if (count) IWebAccount_Release( items[0] );
                    IWebAccount_Release( account );
                }
                IVectorView_WebAccount_Release( accounts );
            }
            error = (void *)0xdeadbeef;
            hr = IFindAllAccountsResult_get_ProviderError( result, &error );
            ok( hr == S_OK && !error, "got hr %#lx, error %p.\n", hr, error );
            IFindAllAccountsResult_Release( result );
        }
        IAsyncOperation_FindAllAccountsResult_Release( operation );
    }
    IWebAccountProvider_Release( provider );
    IWebAuthenticationCoreManagerStatics4_Release( manager4 );
    IWebAuthenticationCoreManagerStatics_Release( manager );
    WindowsDeleteString( provider_id );
    WindowsDeleteString( authority );
    WindowsDeleteString( client_id );
    WindowsDeleteString( missing_client );
}

START_TEST(onlineid)
{
    HRESULT hr;

    hr = RoInitialize( RO_INIT_MULTITHREADED );
    ok( hr == S_OK, "RoInitialize failed, hr %#lx\n", hr );

    test_AuthenticatorStatics();
    test_TicketStatics();
    test_ticket_operation();
    test_web_authentication_core();
    test_find_all_accounts();

    RoUninitialize();
}
