/* WinRT weak reference helpers
 *
 * Copyright 2025 Zhiyi Zhang for CodeWeavers
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 */

#ifndef __WINE_COREMESSAGING_WEAKREF_H
#define __WINE_COREMESSAGING_WEAKREF_H

#include "weakreference.h"

struct weak_reference_source
{
    IWeakReferenceSource IWeakReferenceSource_iface;
    IWeakReference *weak_reference;
};

HRESULT weak_reference_source_init( struct weak_reference_source *source, IUnknown *object );
ULONG weak_reference_strong_add_ref( struct weak_reference_source *source );
ULONG weak_reference_strong_release( struct weak_reference_source *source );

#endif
