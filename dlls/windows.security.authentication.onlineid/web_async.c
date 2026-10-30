/*
 * WinRT web authentication asynchronous operations
 *
 * Copyright (C) 2026 Mirko Brombin
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 */

#include "private.h"

#define ASYNC_CLOSED ((AsyncStatus)4)
#define HANDLER_ASSIGNED ((IUnknown *)~(ULONG_PTR)0)

enum web_async_kind
{
    WEB_ASYNC_TOKEN,
    WEB_ASYNC_PROVIDER,
    WEB_ASYNC_ACCOUNT,
    WEB_ASYNC_ACCOUNTS,
};

struct completed_handler_vtbl
{
    HRESULT (WINAPI *QueryInterface)( IUnknown *, REFIID, void ** );
    ULONG (WINAPI *AddRef)( IUnknown * );
    ULONG (WINAPI *Release)( IUnknown * );
    HRESULT (WINAPI *Invoke)( IUnknown *, IInspectable *, AsyncStatus );
};

struct completed_handler
{
    const struct completed_handler_vtbl *lpVtbl;
};

struct web_async_operation
{
    IAsyncOperation_WebTokenRequestResult IAsyncOperation_iface;
    IAsyncInfo IAsyncInfo_iface;
    LONG ref;
    CRITICAL_SECTION cs;
    TP_WORK *work;
    HANDLE cancel;
    IUnknown *handler;
    IInspectable *result;
    const IID *operation_iid;
    const WCHAR *runtime_name;
    enum web_async_kind kind;
    AsyncStatus status;
    HRESULT error;
    union
    {
        struct
        {
            IWebTokenRequest *request;
            IWebAccount *account;
            boolean silent;
        } token;
        struct
        {
            HSTRING provider_id;
            HSTRING authority;
            boolean system_provider;
        } provider;
        struct
        {
            IWebAccountProvider *provider;
            HSTRING account_id;
        } account;
        struct
        {
            IWebAccountProvider *provider;
            HSTRING client_id;
        } accounts;
    } u;
};

static inline struct web_async_operation *impl_from_operation( IAsyncOperation_WebTokenRequestResult *iface )
{
    return CONTAINING_RECORD( iface, struct web_async_operation, IAsyncOperation_iface );
}

static inline struct web_async_operation *impl_from_info( IAsyncInfo *iface )
{
    return CONTAINING_RECORD( iface, struct web_async_operation, IAsyncInfo_iface );
}

static HRESULT operation_query_interface( struct web_async_operation *impl, REFIID iid, void **out )
{
    if (!out) return E_POINTER;
    *out = NULL;
    if (IsEqualGUID( iid, &IID_IUnknown ) || IsEqualGUID( iid, &IID_IInspectable ) ||
        IsEqualGUID( iid, &IID_IAgileObject ) || IsEqualGUID( iid, impl->operation_iid ))
        *out = &impl->IAsyncOperation_iface;
    else if (IsEqualGUID( iid, &IID_IAsyncInfo ))
        *out = &impl->IAsyncInfo_iface;
    else
        return E_NOINTERFACE;
    InterlockedIncrement( &impl->ref );
    return S_OK;
}

static void operation_destroy( struct web_async_operation *impl )
{
    if (impl->handler && impl->handler != HANDLER_ASSIGNED) IUnknown_Release( impl->handler );
    if (impl->result) IInspectable_Release( impl->result );
    switch (impl->kind)
    {
    case WEB_ASYNC_TOKEN:
        if (impl->u.token.request) IWebTokenRequest_Release( impl->u.token.request );
        if (impl->u.token.account) IWebAccount_Release( impl->u.token.account );
        break;
    case WEB_ASYNC_PROVIDER:
        WindowsDeleteString( impl->u.provider.provider_id );
        WindowsDeleteString( impl->u.provider.authority );
        break;
    case WEB_ASYNC_ACCOUNT:
        if (impl->u.account.provider) IWebAccountProvider_Release( impl->u.account.provider );
        WindowsDeleteString( impl->u.account.account_id );
        break;
    case WEB_ASYNC_ACCOUNTS:
        if (impl->u.accounts.provider) IWebAccountProvider_Release( impl->u.accounts.provider );
        WindowsDeleteString( impl->u.accounts.client_id );
        break;
    }
    if (impl->work) CloseThreadpoolWork( impl->work );
    if (impl->cancel) CloseHandle( impl->cancel );
    impl->cs.DebugInfo->Spare[0] = 0;
    DeleteCriticalSection( &impl->cs );
    free( impl );
}

static ULONG operation_release( struct web_async_operation *impl )
{
    ULONG ref = InterlockedDecrement( &impl->ref );
    if (!ref) operation_destroy( impl );
    return ref;
}

static HRESULT WINAPI operation_QueryInterface( IAsyncOperation_WebTokenRequestResult *iface, REFIID iid, void **out )
{
    return operation_query_interface( impl_from_operation( iface ), iid, out );
}

static ULONG WINAPI operation_AddRef( IAsyncOperation_WebTokenRequestResult *iface )
{
    return InterlockedIncrement( &impl_from_operation( iface )->ref );
}

static ULONG WINAPI operation_Release( IAsyncOperation_WebTokenRequestResult *iface )
{
    return operation_release( impl_from_operation( iface ) );
}

static HRESULT WINAPI operation_GetIids( IAsyncOperation_WebTokenRequestResult *iface, ULONG *count, IID **iids )
{
    struct web_async_operation *impl = impl_from_operation( iface );
    if (!count || !iids) return E_POINTER;
    *count = 1;
    if (!(*iids = CoTaskMemAlloc( sizeof(**iids) ))) return E_OUTOFMEMORY;
    **iids = *impl->operation_iid;
    return S_OK;
}

static HRESULT WINAPI operation_GetRuntimeClassName( IAsyncOperation_WebTokenRequestResult *iface, HSTRING *name )
{
    struct web_async_operation *impl = impl_from_operation( iface );
    if (!name) return E_POINTER;
    return WindowsCreateString( impl->runtime_name, wcslen( impl->runtime_name ), name );
}

static HRESULT WINAPI operation_GetTrustLevel( IAsyncOperation_WebTokenRequestResult *iface, TrustLevel *level )
{
    if (!level) return E_POINTER;
    *level = BaseTrust;
    return S_OK;
}

static HRESULT WINAPI operation_put_Completed( IAsyncOperation_WebTokenRequestResult *iface,
                                               IAsyncOperationCompletedHandler_WebTokenRequestResult *handler )
{
    struct web_async_operation *impl = impl_from_operation( iface );
    IUnknown *unknown = (IUnknown *)handler;
    AsyncStatus status;

    EnterCriticalSection( &impl->cs );
    if (impl->status == ASYNC_CLOSED || impl->handler != HANDLER_ASSIGNED)
    {
        LeaveCriticalSection( &impl->cs );
        return impl->status == ASYNC_CLOSED ? E_ILLEGAL_METHOD_CALL : E_ILLEGAL_DELEGATE_ASSIGNMENT;
    }
    if (unknown) IUnknown_AddRef( unknown );
    impl->handler = unknown;
    status = impl->status;
    if (status == Started)
    {
        LeaveCriticalSection( &impl->cs );
        return S_OK;
    }
    impl->handler = NULL;
    LeaveCriticalSection( &impl->cs );
    if (unknown)
    {
        ((struct completed_handler *)unknown)->lpVtbl->Invoke( unknown, (IInspectable *)iface, status );
        IUnknown_Release( unknown );
    }
    return S_OK;
}

static HRESULT WINAPI operation_get_Completed( IAsyncOperation_WebTokenRequestResult *iface,
                                               IAsyncOperationCompletedHandler_WebTokenRequestResult **handler )
{
    struct web_async_operation *impl = impl_from_operation( iface );
    if (!handler) return E_POINTER;
    EnterCriticalSection( &impl->cs );
    if (impl->status == ASYNC_CLOSED)
    {
        LeaveCriticalSection( &impl->cs );
        return E_ILLEGAL_METHOD_CALL;
    }
    if (!impl->handler || impl->handler == HANDLER_ASSIGNED)
        *handler = NULL;
    else
    {
        *handler = (IAsyncOperationCompletedHandler_WebTokenRequestResult *)impl->handler;
        IUnknown_AddRef( impl->handler );
    }
    LeaveCriticalSection( &impl->cs );
    return S_OK;
}

static HRESULT WINAPI operation_GetResults( IAsyncOperation_WebTokenRequestResult *iface,
                                            IWebTokenRequestResult **result )
{
    struct web_async_operation *impl = impl_from_operation( iface );
    HRESULT hr;
    if (!result) return E_POINTER;
    *result = NULL;
    EnterCriticalSection( &impl->cs );
    if (impl->status == Completed)
    {
        *result = (IWebTokenRequestResult *)impl->result;
        if (*result) IInspectable_AddRef( impl->result );
        hr = S_OK;
    }
    else if (impl->status == Error)
        hr = impl->error;
    else
        hr = E_ILLEGAL_METHOD_CALL;
    LeaveCriticalSection( &impl->cs );
    return hr;
}

static const IAsyncOperation_WebTokenRequestResultVtbl operation_vtbl =
{
    operation_QueryInterface,
    operation_AddRef,
    operation_Release,
    operation_GetIids,
    operation_GetRuntimeClassName,
    operation_GetTrustLevel,
    operation_put_Completed,
    operation_get_Completed,
    operation_GetResults,
};

static HRESULT WINAPI info_QueryInterface( IAsyncInfo *iface, REFIID iid, void **out )
{
    return operation_query_interface( impl_from_info( iface ), iid, out );
}

static ULONG WINAPI info_AddRef( IAsyncInfo *iface )
{
    return InterlockedIncrement( &impl_from_info( iface )->ref );
}

static ULONG WINAPI info_Release( IAsyncInfo *iface )
{
    return operation_release( impl_from_info( iface ) );
}

static HRESULT WINAPI info_GetIids( IAsyncInfo *iface, ULONG *count, IID **iids )
{
    if (!count || !iids) return E_POINTER;
    *count = 1;
    if (!(*iids = CoTaskMemAlloc( sizeof(**iids) ))) return E_OUTOFMEMORY;
    **iids = IID_IAsyncInfo;
    return S_OK;
}

static HRESULT WINAPI info_GetRuntimeClassName( IAsyncInfo *iface, HSTRING *name )
{
    return operation_GetRuntimeClassName( &impl_from_info( iface )->IAsyncOperation_iface, name );
}

static HRESULT WINAPI info_GetTrustLevel( IAsyncInfo *iface, TrustLevel *level )
{
    return operation_GetTrustLevel( &impl_from_info( iface )->IAsyncOperation_iface, level );
}

static HRESULT WINAPI info_get_Id( IAsyncInfo *iface, UINT32 *id )
{
    if (!id) return E_POINTER;
    *id = 1;
    return S_OK;
}

static HRESULT WINAPI info_get_Status( IAsyncInfo *iface, AsyncStatus *status )
{
    struct web_async_operation *impl = impl_from_info( iface );
    if (!status) return E_POINTER;
    EnterCriticalSection( &impl->cs );
    *status = impl->status;
    LeaveCriticalSection( &impl->cs );
    return *status == ASYNC_CLOSED ? E_ILLEGAL_METHOD_CALL : S_OK;
}

static HRESULT WINAPI info_get_ErrorCode( IAsyncInfo *iface, HRESULT *error )
{
    struct web_async_operation *impl = impl_from_info( iface );
    HRESULT hr = S_OK;

    if (!error) return E_POINTER;
    EnterCriticalSection( &impl->cs );
    if (impl->status == ASYNC_CLOSED)
        *error = hr = E_ILLEGAL_METHOD_CALL;
    else
        *error = impl->error;
    LeaveCriticalSection( &impl->cs );
    return hr;
}

static HRESULT WINAPI info_Cancel( IAsyncInfo *iface )
{
    struct web_async_operation *impl = impl_from_info( iface );
    IUnknown *handler = NULL;
    HRESULT hr = S_OK;

    EnterCriticalSection( &impl->cs );
    if (impl->status == ASYNC_CLOSED)
        hr = E_ILLEGAL_METHOD_CALL;
    else if (impl->status == Started)
    {
        impl->status = Canceled;
        impl->error = E_ABORT;
        SetEvent( impl->cancel );
        if (impl->handler && impl->handler != HANDLER_ASSIGNED)
        {
            handler = impl->handler;
            impl->handler = NULL;
        }
    }
    LeaveCriticalSection( &impl->cs );
    if (handler)
    {
        ((struct completed_handler *)handler)->lpVtbl->Invoke( handler,
                                                               (IInspectable *)&impl->IAsyncOperation_iface,
                                                               Canceled );
        IUnknown_Release( handler );
    }
    return hr;
}

static HRESULT WINAPI info_Close( IAsyncInfo *iface )
{
    struct web_async_operation *impl = impl_from_info( iface );
    HRESULT hr = S_OK;

    EnterCriticalSection( &impl->cs );
    if (impl->status == Started)
        hr = E_ILLEGAL_STATE_CHANGE;
    else if (impl->status != ASYNC_CLOSED)
        impl->status = ASYNC_CLOSED;
    LeaveCriticalSection( &impl->cs );
    return hr;
}

static const IAsyncInfoVtbl info_vtbl =
{
    info_QueryInterface,
    info_AddRef,
    info_Release,
    info_GetIids,
    info_GetRuntimeClassName,
    info_GetTrustLevel,
    info_get_Id,
    info_get_Status,
    info_get_ErrorCode,
    info_Cancel,
    info_Close,
};

static HRESULT token_worker( struct web_async_operation *impl, IInspectable **result )
{
    IWebAccountProvider *provider = NULL;
    IWebAccountProvider2 *provider2 = NULL;
    IWebAccount2 *account2 = NULL;
    struct identity_reply reply = {0};
    IWebTokenRequestResult *token_result = NULL;
    HSTRING client_id = NULL, scopes = NULL, authority = NULL, login_hint = NULL;
    HSTRING account_id = NULL, policy = NULL;
    WebTokenRequestPromptType prompt;
    HRESULT hr;

    if (FAILED(hr = IWebTokenRequest_get_PromptType( impl->u.token.request, &prompt )) ||
        FAILED(hr = IWebTokenRequest_get_ClientId( impl->u.token.request, &client_id )) ||
        FAILED(hr = IWebTokenRequest_get_Scope( impl->u.token.request, &scopes )) ||
        FAILED(hr = IWebTokenRequest_get_WebAccountProvider( impl->u.token.request, &provider )) ||
        FAILED(hr = IWebAccountProvider_QueryInterface( provider, &IID_IWebAccountProvider2, (void **)&provider2 )) ||
        FAILED(hr = IWebAccountProvider2_get_Authority( provider2, &authority )) ||
        FAILED(hr = WindowsCreateString( L"", 0, &policy )))
        goto done;
    if (prompt != WebTokenRequestPromptType_Default && prompt != WebTokenRequestPromptType_ForceAuthentication)
    {
        hr = E_INVALIDARG;
        goto done;
    }
    if (impl->u.token.silent && prompt == WebTokenRequestPromptType_ForceAuthentication)
    {
        hr = web_token_result_create( impl->u.token.request, provider, NULL, WebTokenRequestStatus_UserInteractionRequired, &token_result );
        goto done;
    }
    if (impl->u.token.account && impl->u.token.silent)
    {
        if (FAILED(hr = IWebAccount_QueryInterface( impl->u.token.account, &IID_IWebAccount2,
                                                    (void **)&account2 )) ||
            FAILED(hr = IWebAccount2_get_Id( account2, &account_id )))
            goto done;
    }
    else if (impl->u.token.account)
        hr = IWebAccount_get_UserName( impl->u.token.account, &login_hint );
    else
        hr = WindowsCreateString( L"", 0, &login_hint );
    if (FAILED(hr)) goto done;
    if (impl->u.token.silent)
    {
        if (!account_id && FAILED(hr = WindowsCreateString( L"", 0, &account_id ))) goto done;
        hr = identity_bridge_get_token( client_id, scopes, authority, account_id, impl->cancel, &reply );
    }
    else
        hr = identity_bridge_authorize_client( client_id, scopes, authority, login_hint, policy, prompt == WebTokenRequestPromptType_ForceAuthentication, impl->cancel, &reply );
    if (SUCCEEDED(hr))
        hr = web_token_result_create( impl->u.token.request, provider, &reply, WebTokenRequestStatus_Success, &token_result );
    else if (hr == HRESULT_FROM_WIN32( ERROR_CANCELLED ))
        hr = web_token_result_create( impl->u.token.request, provider, &reply, WebTokenRequestStatus_UserCancel, &token_result );
    else if (impl->u.token.silent)
        hr = web_token_result_create( impl->u.token.request, provider, &reply, WebTokenRequestStatus_UserInteractionRequired, &token_result );
    else
        hr = web_token_result_create( impl->u.token.request, provider, &reply, WebTokenRequestStatus_ProviderError, &token_result );

done:
    if (account2) IWebAccount2_Release( account2 );
    if (provider2) IWebAccountProvider2_Release( provider2 );
    if (provider) IWebAccountProvider_Release( provider );
    WindowsDeleteString( client_id );
    WindowsDeleteString( scopes );
    WindowsDeleteString( authority );
    WindowsDeleteString( login_hint );
    WindowsDeleteString( account_id );
    WindowsDeleteString( policy );
    identity_reply_clear( &reply );
    if (SUCCEEDED(hr)) *result = (IInspectable *)token_result;
    return hr;
}

static HRESULT provider_worker( struct web_async_operation *impl, IInspectable **result )
{
    IWebAccountProvider *provider;
    HSTRING display_name = NULL, authority = impl->u.provider.authority;
    HRESULT hr;
    if (FAILED(hr = WindowsCreateString( L"Microsoft", 9, &display_name ))) return hr;
    if (!WindowsGetStringLen( authority ) &&
        FAILED(hr = WindowsCreateString( L"organizations", 13, &authority )))
    {
        WindowsDeleteString( display_name );
        return hr;
    }
    hr = web_account_provider_create( impl->u.provider.provider_id, display_name,
                                      authority, impl->u.provider.system_provider,
                                      &provider );
    if (authority != impl->u.provider.authority) WindowsDeleteString( authority );
    WindowsDeleteString( display_name );
    if (SUCCEEDED(hr)) *result = (IInspectable *)provider;
    return hr;
}

static HRESULT account_worker( struct web_async_operation *impl, IInspectable **result )
{
    IWebAccount *account;
    HRESULT hr = web_account_create( impl->u.account.provider, impl->u.account.account_id,
                                     impl->u.account.account_id, WebAccountState_Connected, &account );
    if (SUCCEEDED(hr)) *result = (IInspectable *)account;
    return hr;
}

static HRESULT accounts_worker( struct web_async_operation *impl, IInspectable **result )
{
    IWebAccountProvider2 *provider2 = NULL;
    IFindAllAccountsResult *accounts_result = NULL;
    struct identity_reply reply = {0};
    HSTRING authority = NULL, provider_id = NULL;
    const WCHAR *id;
    HRESULT hr;

    if (FAILED(hr = IWebAccountProvider_get_Id( impl->u.accounts.provider, &provider_id ))) goto done;
    id = WindowsGetStringRawBuffer( provider_id, NULL );
    if (wcscmp( id, L"https://login.microsoft.com" ) && wcscmp( id, L"https://login.microsoftonline.com" ) &&
        wcscmp( id, L"https://login.windows.net" ) && wcscmp( id, L"https://login.live.com" ))
    {
        hr = E_INVALIDARG;
        goto done;
    }
    if (FAILED(hr = IWebAccountProvider_QueryInterface( impl->u.accounts.provider,
        &IID_IWebAccountProvider2, (void **)&provider2 )) ||
        FAILED(hr = IWebAccountProvider2_get_Authority( provider2, &authority ))) goto done;
    hr = identity_bridge_list_accounts( impl->u.accounts.client_id, authority, impl->cancel, &reply );
    if (hr == HRESULT_FROM_WIN32( ERROR_CANCELLED )) goto done;
    reply.error = hr;
    hr = web_accounts_result_create( impl->u.accounts.provider, &reply, &accounts_result );
    if (SUCCEEDED(hr)) *result = (IInspectable *)accounts_result;

done:
    if (provider2) IWebAccountProvider2_Release( provider2 );
    WindowsDeleteString( authority );
    WindowsDeleteString( provider_id );
    identity_reply_clear( &reply );
    return hr;
}

static void CALLBACK operation_callback( TP_CALLBACK_INSTANCE *instance, void *context, TP_WORK *work )
{
    struct web_async_operation *impl = context;
    IUnknown *handler = NULL;
    IInspectable *result = NULL;
    AsyncStatus status;
    HRESULT hr;

    switch (impl->kind)
    {
    case WEB_ASYNC_TOKEN: hr = token_worker( impl, &result ); break;
    case WEB_ASYNC_PROVIDER: hr = provider_worker( impl, &result ); break;
    case WEB_ASYNC_ACCOUNT: hr = account_worker( impl, &result ); break;
    case WEB_ASYNC_ACCOUNTS: hr = accounts_worker( impl, &result ); break;
    default: hr = E_UNEXPECTED; break;
    }
    EnterCriticalSection( &impl->cs );
    if (impl->status == Started)
    {
        impl->result = result;
        impl->error = hr;
        impl->status = SUCCEEDED(hr) ? Completed : Error;
        result = NULL;
    }
    status = impl->status;
    if (impl->handler && impl->handler != HANDLER_ASSIGNED)
    {
        handler = impl->handler;
        impl->handler = NULL;
    }
    LeaveCriticalSection( &impl->cs );
    if (result) IInspectable_Release( result );
    if (handler)
    {
        ((struct completed_handler *)handler)->lpVtbl->Invoke( handler,
                                                               (IInspectable *)&impl->IAsyncOperation_iface,
                                                               status );
        IUnknown_Release( handler );
    }
    operation_release( impl );
}

static HRESULT operation_create( enum web_async_kind kind, const IID *iid, const WCHAR *runtime_name,
                                 struct web_async_operation **out )
{
    struct web_async_operation *impl;
    if (!out) return E_POINTER;
    *out = NULL;
    if (!(impl = calloc( 1, sizeof(*impl) ))) return E_OUTOFMEMORY;
    impl->IAsyncOperation_iface.lpVtbl = &operation_vtbl;
    impl->IAsyncInfo_iface.lpVtbl = &info_vtbl;
    impl->ref = 1;
    impl->handler = HANDLER_ASSIGNED;
    impl->operation_iid = iid;
    impl->runtime_name = runtime_name;
    impl->kind = kind;
    impl->status = Started;
    InitializeCriticalSectionEx( &impl->cs, 0, RTL_CRITICAL_SECTION_FLAG_FORCE_DEBUG_INFO );
    impl->cs.DebugInfo->Spare[0] = (DWORD_PTR)(__FILE__ ": web_async_operation.cs");
    if (!(impl->cancel = CreateEventW( NULL, TRUE, FALSE, NULL )))
    {
        HRESULT hr = HRESULT_FROM_WIN32( GetLastError() );
        operation_destroy( impl );
        return hr;
    }
    if (!(impl->work = CreateThreadpoolWork( operation_callback, impl, NULL )))
    {
        HRESULT hr = HRESULT_FROM_WIN32( GetLastError() );
        operation_destroy( impl );
        return hr;
    }
    *out = impl;
    return S_OK;
}

static void operation_submit( struct web_async_operation *impl )
{
    InterlockedIncrement( &impl->ref );
    SubmitThreadpoolWork( impl->work );
}

HRESULT web_async_token_create( IWebTokenRequest *request, IWebAccount *account, boolean silent,
                                IAsyncOperation_WebTokenRequestResult **out )
{
    static const WCHAR runtime_name[] = L"Windows.Foundation.IAsyncOperation`1<Windows.Security.Authentication.Web.Core.WebTokenRequestResult>";
    struct web_async_operation *impl;
    HRESULT hr;
    if (!request || !out) return E_POINTER;
    if (FAILED(hr = operation_create( WEB_ASYNC_TOKEN, &IID_IAsyncOperation_WebTokenRequestResult,
                                      runtime_name, &impl ))) return hr;
    impl->u.token.request = request;
    impl->u.token.account = account;
    impl->u.token.silent = silent;
    IWebTokenRequest_AddRef( request );
    if (account) IWebAccount_AddRef( account );
    *out = &impl->IAsyncOperation_iface;
    operation_submit( impl );
    return S_OK;
}

HRESULT web_async_provider_create( HSTRING provider_id, HSTRING authority, boolean system_provider,
                                   IAsyncOperation_WebAccountProvider **out )
{
    static const WCHAR runtime_name[] = L"Windows.Foundation.IAsyncOperation`1<Windows.Security.Credentials.WebAccountProvider>";
    struct web_async_operation *impl = NULL;
    HRESULT hr;
    if (!out) return E_POINTER;
    *out = NULL;
    if (FAILED(hr = operation_create( WEB_ASYNC_PROVIDER, &IID_IAsyncOperation_WebAccountProvider,
                                      runtime_name, &impl )) ||
        FAILED(hr = WindowsDuplicateString( provider_id, &impl->u.provider.provider_id )) ||
        FAILED(hr = WindowsDuplicateString( authority, &impl->u.provider.authority )))
    {
        if (impl) operation_release( impl );
        return hr;
    }
    impl->u.provider.system_provider = system_provider;
    *out = (IAsyncOperation_WebAccountProvider *)&impl->IAsyncOperation_iface;
    operation_submit( impl );
    return S_OK;
}

HRESULT web_async_account_create( IWebAccountProvider *provider, HSTRING account_id,
                                  IAsyncOperation_WebAccount **out )
{
    static const WCHAR runtime_name[] = L"Windows.Foundation.IAsyncOperation`1<Windows.Security.Credentials.WebAccount>";
    struct web_async_operation *impl = NULL;
    HRESULT hr;
    if (!provider || !out) return E_POINTER;
    *out = NULL;
    if (FAILED(hr = operation_create( WEB_ASYNC_ACCOUNT, &IID_IAsyncOperation_WebAccount,
                                      runtime_name, &impl )) ||
        FAILED(hr = WindowsDuplicateString( account_id, &impl->u.account.account_id )))
    {
        if (impl) operation_release( impl );
        return hr;
    }
    impl->u.account.provider = provider;
    IWebAccountProvider_AddRef( provider );
    *out = (IAsyncOperation_WebAccount *)&impl->IAsyncOperation_iface;
    operation_submit( impl );
    return S_OK;
}

HRESULT web_async_accounts_create( IWebAccountProvider *provider, HSTRING client_id,
                                   IAsyncOperation_FindAllAccountsResult **out )
{
    static const WCHAR runtime_name[] = L"Windows.Foundation.IAsyncOperation`1<Windows.Security.Authentication.Web.Core.FindAllAccountsResult>";
    struct web_async_operation *impl = NULL;
    HRESULT hr;

    if (!out) return E_POINTER;
    *out = NULL;
    if (!provider) return E_POINTER;
    if (FAILED(hr = operation_create( WEB_ASYNC_ACCOUNTS, &IID_IAsyncOperation_FindAllAccountsResult,
                                      runtime_name, &impl )) ||
        FAILED(hr = WindowsDuplicateString( client_id, &impl->u.accounts.client_id )))
    {
        if (impl) operation_release( impl );
        return hr;
    }
    impl->u.accounts.provider = provider;
    IWebAccountProvider_AddRef( provider );
    *out = (IAsyncOperation_FindAllAccountsResult *)&impl->IAsyncOperation_iface;
    operation_submit( impl );
    return S_OK;
}
