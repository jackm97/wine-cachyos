#ifndef __WINE_EAGLE_IDENTITY_TRACE_H
#define __WINE_EAGLE_IDENTITY_TRACE_H

#include "eagle_trace.h"

#define EAGLE_IDENTITY_BEGIN 1
#define EAGLE_IDENTITY_TRANSPORT 2
#define EAGLE_IDENTITY_RESPONSE 3
#define EAGLE_IDENTITY_COMPLETE 4
#define EAGLE_IDENTITY_CACHE 5
#define EAGLE_IDENTITY_REQUEST 6
#define EAGLE_IDENTITY_PROMPT 7
#define EAGLE_IDENTITY_PERSIST 8

static inline void eagle_trace_identity(ULONG request, unsigned operation, unsigned stage,
                                       HRESULT result, const void *caller)
{
    if (!request || (operation != 2 && operation != 3 && operation != 5) ||
        stage < EAGLE_IDENTITY_BEGIN || stage > EAGLE_IDENTITY_COMPLETE) return;
    eagle_trace_event(1, "identity.stage", result, operation, 1, request, stage, NULL, NULL, caller);
}

static inline void eagle_trace_native_identity(ULONG request, unsigned operation, unsigned stage,
                                              HRESULT result, const void *caller)
{
    if (!request || (operation != 2 && operation != 3 && operation != 5) ||
        stage < EAGLE_IDENTITY_BEGIN || stage > EAGLE_IDENTITY_PERSIST || stage == EAGLE_IDENTITY_TRANSPORT) return;
    eagle_trace_event(1, "identity.native.stage", result, operation, 2, request, stage, NULL, NULL, caller);
}

#endif
