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

#ifndef __WINE_ONLINEID_PRIVATE_H
#define __WINE_ONLINEID_PRIVATE_H

#include <stdarg.h>

#define COBJMACROS
#include "windef.h"
#include "winbase.h"
#include "winstring.h"

#include "activation.h"

#define WIDL_using_Windows_Foundation
#define WIDL_using_Windows_Foundation_Collections
#include "windows.foundation.h"
#define WIDL_using_Windows_System
#include "windows.system.h"
#define WIDL_using_Windows_Security_Authentication_OnlineId
#include "windows.security.authentication.onlineid.h"
#define WIDL_using_Windows_Security_Credentials
#include "windows.security.credentials.h"
#define WIDL_using_Windows_Security_Authentication_Web_Core
#include "windows.security.authentication.web.core.h"

extern IActivationFactory *authenticator_factory;
extern IActivationFactory *ticket_factory;
extern IActivationFactory *web_account_factory;
extern IActivationFactory *web_account_provider_factory;
extern IActivationFactory *web_authentication_core_manager_factory;
extern IActivationFactory *web_token_request_factory;
extern IActivationFactory *web_token_response_factory;

struct identity_reply
{
    HSTRING access_token;
    HSTRING id_token;
    HSTRING account_id;
    HSTRING username;
    HSTRING display_name;
    HSTRING tenant_id;
    HSTRING client_info;
    HSTRING scopes;
    UINT64 expires_at;
    HSTRING error_message;
    HRESULT error;
    BYTE *accounts;
    UINT32 accounts_size;
};

extern HRESULT identity_bridge_authorize( const GUID *application_id, HSTRING service, HSTRING policy,
                                          HANDLE cancel, struct identity_reply *reply );
extern HRESULT identity_bridge_authorize_client( HSTRING client_id, HSTRING scopes, HSTRING authority,
                                                 HSTRING login_hint, HSTRING policy, BOOL force, HANDLE cancel, struct identity_reply *reply );
extern HRESULT identity_bridge_get_token( HSTRING client_id, HSTRING scopes, HSTRING authority,
                                          HSTRING account_id, HANDLE cancel, struct identity_reply *reply );
extern HRESULT identity_device_authorize( const GUID *application_id, HSTRING service, HSTRING authority, HSTRING login_hint,
                                          BOOL force, HANDLE cancel, struct identity_reply *reply );
extern HRESULT identity_device_get_token( const GUID *application_id, HSTRING service, HSTRING authority,
                                          HSTRING account_id, HSTRING login_hint, HANDLE cancel, struct identity_reply *reply );
extern HRESULT identity_native_credentials( UINT operation, const char *client_id, HSTRING authority,
                                            char *data, DWORD capacity, DWORD *size );
extern HRESULT identity_authority_tenant( HSTRING authority, WCHAR tenant[37] );
extern HRESULT identity_device_list_accounts( HSTRING client_id, HSTRING authority, HANDLE cancel,
                                              struct identity_reply *reply );
extern void identity_reply_clear( struct identity_reply *reply );
extern HRESULT identity_bridge_list_accounts( HSTRING client_id, HSTRING authority, HANDLE cancel,
                                              struct identity_reply *reply );
extern HRESULT web_accounts_result_create( IWebAccountProvider *provider, const struct identity_reply *reply,
                                           IFindAllAccountsResult **out );
extern HRESULT string_map_create( IMap_HSTRING_HSTRING **out );
extern HRESULT web_account_create( IWebAccountProvider *provider, HSTRING id, HSTRING username,
                                   WebAccountState state, IWebAccount **out );
extern HRESULT web_account_set_customer_id( IWebAccount *account, HSTRING authority );
extern HRESULT web_account_set_property( IWebAccount *account, const WCHAR *key, HSTRING value );
extern HRESULT web_account_provider_create( HSTRING id, HSTRING display_name, HSTRING authority,
                                            boolean system_provider, IWebAccountProvider **out );
extern HRESULT web_token_request_create( IWebAccountProvider *provider, HSTRING scope, HSTRING client_id,
                                         WebTokenRequestPromptType prompt_type, IWebTokenRequest **out );
extern HRESULT web_token_response_create( HSTRING token, IWebAccount *account, IWebProviderError *error,
                                          IWebTokenResponse **out );
extern HRESULT web_provider_error_create( HRESULT error, HSTRING message, IWebProviderError **out );
extern HRESULT web_token_result_create( IWebTokenRequest *request, IWebAccountProvider *provider, const struct identity_reply *reply,
                                        WebTokenRequestStatus status, IWebTokenRequestResult **out );
extern HRESULT web_async_token_create( IWebTokenRequest *request, IWebAccount *account, boolean silent,
                                       IAsyncOperation_WebTokenRequestResult **out );
extern HRESULT web_async_provider_create( HSTRING provider_id, HSTRING authority, boolean system_provider,
                                          IAsyncOperation_WebAccountProvider **out );
extern HRESULT web_async_account_create( IWebAccountProvider *provider, HSTRING account_id,
                                         IAsyncOperation_WebAccount **out );
extern HRESULT web_async_accounts_create( IWebAccountProvider *provider, HSTRING client_id,
                                          IAsyncOperation_FindAllAccountsResult **out );
extern HRESULT ticket_request_create( HSTRING service, HSTRING policy, IOnlineIdServiceTicketRequest **out );
extern HRESULT ticket_result_create( IOnlineIdServiceTicketRequest *request, const struct identity_reply *reply,
                                     IOnlineIdSystemTicketResult **out );
extern HRESULT async_operation_ticket_result_create( IUnknown *invoker, IOnlineIdServiceTicketRequest *request,
                                                      const GUID *application_id,
                                                      IAsyncOperation_OnlineIdSystemTicketResult **out );

#define DEFINE_IINSPECTABLE_( pfx, iface_type, impl_type, impl_from, iface_mem, expr )             \
    static inline impl_type *impl_from( iface_type *iface )                                        \
    {                                                                                              \
        return CONTAINING_RECORD( iface, impl_type, iface_mem );                                   \
    }                                                                                              \
    static HRESULT WINAPI pfx##_QueryInterface( iface_type *iface, REFIID iid, void **out )        \
    {                                                                                              \
        impl_type *impl = impl_from( iface );                                                      \
        return IInspectable_QueryInterface( (IInspectable *)(expr), iid, out );                    \
    }                                                                                              \
    static ULONG WINAPI pfx##_AddRef( iface_type *iface )                                          \
    {                                                                                              \
        impl_type *impl = impl_from( iface );                                                      \
        return IInspectable_AddRef( (IInspectable *)(expr) );                                      \
    }                                                                                              \
    static ULONG WINAPI pfx##_Release( iface_type *iface )                                         \
    {                                                                                              \
        impl_type *impl = impl_from( iface );                                                      \
        return IInspectable_Release( (IInspectable *)(expr) );                                     \
    }                                                                                              \
    static HRESULT WINAPI pfx##_GetIids( iface_type *iface, ULONG *iid_count, IID **iids )         \
    {                                                                                              \
        impl_type *impl = impl_from( iface );                                                      \
        return IInspectable_GetIids( (IInspectable *)(expr), iid_count, iids );                    \
    }                                                                                              \
    static HRESULT WINAPI pfx##_GetRuntimeClassName( iface_type *iface, HSTRING *class_name )      \
    {                                                                                              \
        impl_type *impl = impl_from( iface );                                                      \
        return IInspectable_GetRuntimeClassName( (IInspectable *)(expr), class_name );             \
    }                                                                                              \
    static HRESULT WINAPI pfx##_GetTrustLevel( iface_type *iface, TrustLevel *trust_level )        \
    {                                                                                              \
        impl_type *impl = impl_from( iface );                                                      \
        return IInspectable_GetTrustLevel( (IInspectable *)(expr), trust_level );                  \
    }
#define DEFINE_IINSPECTABLE( pfx, iface_type, impl_type, base_iface )                              \
    DEFINE_IINSPECTABLE_( pfx, iface_type, impl_type, impl_from_##iface_type, iface_type##_iface, &impl->base_iface )

#endif
