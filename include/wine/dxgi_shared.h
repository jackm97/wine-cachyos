/*
 * Copyright 2009 Henri Verbeet for CodeWeavers
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
 *
 */

#ifndef __WINE_DXGI_SHARED_H
#define __WINE_DXGI_SHARED_H

#include "d3d11.h"
#include "ddk/d3dkmthk.h"

struct dxgi_shared_texture_metadata
{
    UINT size, version, width, height;
    DXGI_FORMAT format;
    UINT unknown_0, unknown_1, keyed_mutex;
    D3DKMT_HANDLE mutex_handle, sync_handle;
    UINT nt_shared, unknown_2, unknown_3, unknown_4;
    D3D11_RESOURCE_DIMENSION dimension;
    D3D11_TEXTURE2D_DESC texture;
};
C_ASSERT(sizeof(struct dxgi_shared_texture_metadata) == 0x68);

#endif
