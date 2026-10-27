/* WinRT Windows.Security.Authentication.Onlineid Implementation
 *
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

#include "initguid.h"
#include "private.h"
#include "webauthenticationcoremanagerinterop.h"

#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(onlineid);

HRESULT WINAPI DllGetClassObject( REFCLSID clsid, REFIID riid, void **out )
{
    FIXME( "clsid %s, riid %s, out %p stub!\n", debugstr_guid( clsid ), debugstr_guid( riid ), out );
    return CLASS_E_CLASSNOTAVAILABLE;
}

HRESULT WINAPI DllGetActivationFactory( HSTRING classid, IActivationFactory **factory )
{
    const WCHAR *buffer = WindowsGetStringRawBuffer( classid, NULL );

    TRACE( "class %s, factory %p.\n", debugstr_hstring( classid ), factory );

    *factory = NULL;

    if (!wcscmp( buffer, RuntimeClass_Windows_Security_Authentication_OnlineId_OnlineIdSystemAuthenticator ))
        IActivationFactory_QueryInterface( authenticator_factory, &IID_IActivationFactory, (void **)factory );
    if (!wcscmp( buffer, RuntimeClass_Windows_Security_Authentication_OnlineId_OnlineIdServiceTicketRequest ))
        IActivationFactory_QueryInterface( ticket_factory, &IID_IActivationFactory, (void **)factory );
    if (!wcscmp( buffer, RuntimeClass_Windows_Security_Authentication_Web_Core_WebAuthenticationCoreManager ))
        IActivationFactory_QueryInterface( web_authentication_core_manager_factory, &IID_IActivationFactory,
                                           (void **)factory );
    if (!wcscmp( buffer, RuntimeClass_Windows_Security_Authentication_Web_Core_WebTokenRequest ))
        IActivationFactory_QueryInterface( web_token_request_factory, &IID_IActivationFactory, (void **)factory );
    if (!wcscmp( buffer, RuntimeClass_Windows_Security_Authentication_Web_Core_WebTokenResponse ))
        IActivationFactory_QueryInterface( web_token_response_factory, &IID_IActivationFactory, (void **)factory );
    if (!wcscmp( buffer, RuntimeClass_Windows_Security_Credentials_WebAccount ))
        IActivationFactory_QueryInterface( web_account_factory, &IID_IActivationFactory, (void **)factory );
    if (!wcscmp( buffer, RuntimeClass_Windows_Security_Credentials_WebAccountProvider ))
        IActivationFactory_QueryInterface( web_account_provider_factory, &IID_IActivationFactory, (void **)factory );

    if (*factory) return S_OK;
    return CLASS_E_CLASSNOTAVAILABLE;
}
