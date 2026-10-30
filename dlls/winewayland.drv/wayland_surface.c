/*
 * Wayland surfaces
 *
 * Copyright 2020 Alexandros Frantzis for Collabora Ltd
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

#if 0
#pragma makedep unix
#endif

#include "config.h"

#include <assert.h>
#include <stdlib.h>
#include <unistd.h>

#include "ntstatus.h"
#define WIN32_NO_STATUS

#include "waylanddrv.h"
#include "wine/debug.h"
#include "wine/server.h"

WINE_DEFAULT_DEBUG_CHANNEL(waylanddrv);

static const WCHAR xdg_foreign_handle_prop[] =
    {'_','_','w','i','n','e','_','w','a','y','l','a','n','d','_','x','d','g','_',
     'f','o','r','e','i','g','n','_','h','a','n','d','l','e',0};
static const WCHAR xdg_foreign_child_prop[] =
    {'_','_','w','i','n','e','_','w','a','y','l','a','n','d','_','x','d','g','_',
     'f','o','r','e','i','g','n','_','c','h','i','l','d',0};
static const WCHAR xdg_activation_token_prop[] =
    {'_','_','w','i','n','e','_','w','a','y','l','a','n','d','_','x','d','g','_',
     'a','c','t','i','v','a','t','i','o','n','_','t','o','k','e','n',0};

struct foreign_activation_request
{
    struct xdg_activation_token_v1 *token;
    HWND child;
};

static BOOL atom_get_ascii(RTL_ATOM atom, char *string, size_t size)
{
    char buffer[sizeof(ATOM_BASIC_INFORMATION) + MAX_ATOM_LEN * sizeof(WCHAR)];
    ATOM_BASIC_INFORMATION *info = (ATOM_BASIC_INFORMATION *)buffer;
    unsigned int i, len;

    if (!atom || NtQueryInformationAtom(atom, AtomBasicInformation,
                                        info, sizeof(buffer), NULL))
        return FALSE;

    len = info->NameLength / sizeof(WCHAR);
    if (!len || len >= size) return FALSE;

    for (i = 0; i < len; i++)
    {
        if (info->Name[i] > 0x7f) return FALSE;
        string[i] = info->Name[i];
    }
    string[len] = 0;
    return TRUE;
}

static void foreign_activation_token_handle_done(void *private,
                                                 struct xdg_activation_token_v1 *token,
                                                 const char *token_string)
{
    struct foreign_activation_request *request = private;
    WCHAR tokenW[MAX_ATOM_LEN + 1];
    RTL_ATOM atom, old_atom;
    size_t i, len = strlen(token_string);

    TRACE("done child=%p token=%s\n", request->child, token_string);

    if (request->token != token || len > MAX_ATOM_LEN || !NtUserIsWindow(request->child))
        goto done;

    for (i = 0; i < len; i++) tokenW[i] = (unsigned char)token_string[i];
    tokenW[len] = 0;
    if (NtAddAtom(tokenW, len * sizeof(WCHAR), &atom)) goto done;

    old_atom = HandleToULong(NtUserRemoveProp(request->child, xdg_activation_token_prop));
    if (old_atom) NtDeleteAtom(old_atom);
    if (!NtUserSetProp(request->child, xdg_activation_token_prop, ULongToHandle(atom)) ||
        !NtUserPostMessage(request->child, WM_WAYLAND_ACTIVATE_FOREIGN, 0, 0))
    {
        NtUserRemoveProp(request->child, xdg_activation_token_prop);
        NtDeleteAtom(atom);
    }

done:
    xdg_activation_token_v1_destroy(token);
    free(request);
}

static const struct xdg_activation_token_v1_listener foreign_activation_token_listener =
{
    foreign_activation_token_handle_done,
};

static void wayland_request_surface_activation(struct wayland_surface *surface)
{
    struct foreign_activation_request *request;

    if (!process_wayland.xdg_activation_v1 || !surface->wl_surface) return;
    if (!(request = calloc(1, sizeof(*request)))) return;

    request->child = surface->hwnd;
    request->token = xdg_activation_v1_get_activation_token(process_wayland.xdg_activation_v1);
    if (!request->token)
    {
        free(request);
        return;
    }

    xdg_activation_token_v1_add_listener(request->token,
                                         &foreign_activation_token_listener, request);
    xdg_activation_token_v1_set_surface(request->token, surface->wl_surface);
    xdg_activation_token_v1_commit(request->token);
    wl_display_flush(process_wayland.wl_display);
}

void wayland_activate_foreign_child(HWND hwnd, uint32_t serial)
{
    struct foreign_activation_request *request;
    struct wayland_win_data *data;
    struct wl_seat *seat;
    HWND child, root = NtUserGetAncestor(hwnd, GA_ROOT);

    child = root ? NtUserGetProp(root, xdg_foreign_child_prop) : 0;
    TRACE("request hwnd=%p root=%p child=%p serial=%u\n", hwnd, root, child, serial);
    if (!process_wayland.xdg_activation_v1 || !root || !child || child == hwnd ||
        !NtUserIsWindow(child))
        return;

    if (!(data = wayland_win_data_get(hwnd))) return;
    if (!data->wayland_surface || !wayland_surface_is_toplevel(data->wayland_surface))
    {
        wayland_win_data_release(data);
        return;
    }

    if (!(request = calloc(1, sizeof(*request))))
    {
        wayland_win_data_release(data);
        return;
    }

    request->child = child;
    request->token = xdg_activation_v1_get_activation_token(process_wayland.xdg_activation_v1);
    if (!request->token)
    {
        free(request);
        wayland_win_data_release(data);
        return;
    }

    xdg_activation_token_v1_add_listener(request->token,
                                         &foreign_activation_token_listener, request);
    pthread_mutex_lock(&process_wayland.seat.mutex);
    if ((seat = process_wayland.seat.wl_seat) && serial)
        xdg_activation_token_v1_set_serial(request->token, serial, seat);
    pthread_mutex_unlock(&process_wayland.seat.mutex);
    xdg_activation_token_v1_set_surface(request->token,
                                        data->wayland_surface->wl_surface);
    xdg_activation_token_v1_commit(request->token);
    TRACE("committed request for child=%p\n", child);
    wayland_win_data_release(data);
    wl_display_flush(process_wayland.wl_display);
}

void wayland_surface_activate_from_token(HWND hwnd)
{
    struct wayland_win_data *data;
    char token[MAX_ATOM_LEN + 1];
    RTL_ATOM atom;

    atom = HandleToULong(NtUserRemoveProp(hwnd, xdg_activation_token_prop));
    TRACE("consume hwnd=%p atom=%u\n", hwnd, atom);
    if (!atom) return;

    if (process_wayland.xdg_activation_v1 && atom_get_ascii(atom, token, sizeof(token)) &&
        (data = wayland_win_data_get(hwnd)))
    {
        if (data->wayland_surface && wayland_surface_is_toplevel(data->wayland_surface))
        {
            xdg_activation_v1_activate(process_wayland.xdg_activation_v1, token,
                                       data->wayland_surface->wl_surface);
            wl_display_flush(process_wayland.wl_display);
        }
        wayland_win_data_release(data);
    }

    NtDeleteAtom(atom);
}

static void xdg_exported_v2_handle_handle(void *private,
                                          struct zxdg_exported_v2 *exported,
                                          const char *handle)
{
    struct wayland_surface *surface = private;
    WCHAR handleW[MAX_ATOM_LEN + 1];
    RTL_ATOM atom, old_atom;
    size_t i, len = strlen(handle);

    if (surface->zxdg_exported_v2 != exported || len > MAX_ATOM_LEN) return;

    for (i = 0; i < len; i++) handleW[i] = (unsigned char)handle[i];
    handleW[len] = 0;

    if (NtAddAtom(handleW, len * sizeof(WCHAR), &atom)) return;

    old_atom = HandleToULong(NtUserRemoveProp(surface->hwnd, xdg_foreign_handle_prop));
    if (!NtUserSetProp(surface->hwnd, xdg_foreign_handle_prop, ULongToHandle(atom)))
    {
        NtDeleteAtom(atom);
        return;
    }

    if (old_atom) NtDeleteAtom(old_atom);
    surface->exported_handle_atom = atom;
    TRACE("exported hwnd=%p handle=%s\n", surface->hwnd, handle);
}

static const struct zxdg_exported_v2_listener xdg_exported_v2_listener =
{
    xdg_exported_v2_handle_handle,
};

static void xdg_imported_v2_handle_destroyed(void *private,
                                             struct zxdg_imported_v2 *imported)
{
    struct wayland_surface *surface = private;

    if (surface->zxdg_imported_v2 != imported) return;
    if (surface->foreign_parent_hwnd &&
        NtUserGetProp(surface->foreign_parent_hwnd, xdg_foreign_child_prop) == surface->hwnd)
        NtUserRemoveProp(surface->foreign_parent_hwnd, xdg_foreign_child_prop);
    zxdg_imported_v2_destroy(imported);
    surface->zxdg_imported_v2 = NULL;
    surface->imported_handle_atom = 0;
    surface->foreign_parent_hwnd = 0;
    surface->foreign_activation_pending = FALSE;
}

static const struct zxdg_imported_v2_listener xdg_imported_v2_listener =
{
    xdg_imported_v2_handle_destroyed,
};

void wayland_surface_update_foreign_parent(struct wayland_surface *surface)
{
    char handle[MAX_ATOM_LEN + 1];
    RTL_ATOM atom;
    HWND parent;

    if (!process_wayland.zxdg_importer_v2 || !wayland_surface_is_toplevel(surface)) return;
    if (!(parent = NtUserGetAncestor(surface->hwnd, GA_ROOT)) || parent == surface->hwnd) return;

    atom = HandleToULong(NtUserGetProp(parent, xdg_foreign_handle_prop));
    if (!atom || (surface->zxdg_imported_v2 && surface->imported_handle_atom == atom)) return;

    if (surface->zxdg_imported_v2)
    {
        zxdg_imported_v2_destroy(surface->zxdg_imported_v2);
        surface->zxdg_imported_v2 = NULL;
        surface->imported_handle_atom = 0;
    }

    if (!atom_get_ascii(atom, handle, sizeof(handle))) return;

    surface->zxdg_imported_v2 =
        zxdg_importer_v2_import_toplevel(process_wayland.zxdg_importer_v2, handle);
    if (!surface->zxdg_imported_v2) return;

    zxdg_imported_v2_add_listener(surface->zxdg_imported_v2,
                                  &xdg_imported_v2_listener, surface);
    zxdg_imported_v2_set_parent_of(surface->zxdg_imported_v2, surface->wl_surface);
    surface->imported_handle_atom = atom;
    surface->foreign_parent_hwnd = parent;
    NtUserSetProp(parent, xdg_foreign_child_prop, surface->hwnd);
    surface->foreign_activation_pending = TRUE;
    wl_display_flush(process_wayland.wl_display);
    TRACE("imported parent hwnd=%p for hwnd=%p handle=%s\n",
          parent, surface->hwnd, handle);
}

static void xdg_surface_handle_configure(void *private, struct xdg_surface *xdg_surface,
                                         uint32_t serial)
{
    struct wayland_surface *surface;
    BOOL should_post = FALSE, initial_configure = FALSE;
    struct wayland_win_data *data;
    HWND hwnd = private;

    TRACE("serial=%u\n", serial);

    if (!(data = wayland_win_data_get(hwnd))) return;

    /* Handle this event only if wayland_surface is still associated with
     * the target xdg_surface. */
    if ((surface = data->wayland_surface) && wayland_surface_is_toplevel(surface) &&
        surface->xdg_surface == xdg_surface)
    {
        /* If we have a previously requested config, we have already sent a
         * WM_WAYLAND_CONFIGURE which hasn't been handled yet. In that case,
         * avoid sending another message to reduce message queue traffic. */
        should_post = surface->requested.serial == 0;
        initial_configure = surface->current.serial == 0;
        surface->pending.serial = serial;
        surface->requested = surface->pending;
        memset(&surface->pending, 0, sizeof(surface->pending));
    }

    wayland_win_data_release(data);

    if (should_post) NtUserPostMessage(hwnd, WM_WAYLAND_CONFIGURE, 0, 0);

    /* Flush the window surface in case there is content that we weren't
     * able to flush before due to the lack of the initial configure. */
    if (initial_configure)
    {
        NtUserExposeWindowSurface(hwnd, 0, NULL, 0);
    }
}

static const struct xdg_surface_listener xdg_surface_listener =
{
    xdg_surface_handle_configure
};

static void xdg_toplevel_handle_configure(void *private,
                                          struct xdg_toplevel *xdg_toplevel,
                                          int32_t width, int32_t height,
                                          struct wl_array *states)
{
    struct wayland_surface *surface;
    HWND hwnd = private;
    uint32_t *state;
    enum wayland_surface_config_state config_state = 0;
    struct wayland_win_data *data;

    wl_array_for_each(state, states)
    {
        switch(*state)
        {
        case XDG_TOPLEVEL_STATE_MAXIMIZED:
            config_state |= WAYLAND_SURFACE_CONFIG_STATE_MAXIMIZED;
            break;
        case XDG_TOPLEVEL_STATE_RESIZING:
            config_state |= WAYLAND_SURFACE_CONFIG_STATE_RESIZING;
            break;
        case XDG_TOPLEVEL_STATE_TILED_LEFT:
        case XDG_TOPLEVEL_STATE_TILED_RIGHT:
        case XDG_TOPLEVEL_STATE_TILED_TOP:
        case XDG_TOPLEVEL_STATE_TILED_BOTTOM:
            config_state |= WAYLAND_SURFACE_CONFIG_STATE_TILED;
            break;
        case XDG_TOPLEVEL_STATE_FULLSCREEN:
            config_state |= WAYLAND_SURFACE_CONFIG_STATE_FULLSCREEN;
            break;
        default:
            break;
        }
    }

    TRACE("hwnd=%p %dx%d,%#x\n", hwnd, width, height, config_state);

    if (!(data = wayland_win_data_get(hwnd))) return;

    if ((surface = data->wayland_surface) && wayland_surface_is_toplevel(surface))
    {
        surface->pending.width = width;
        surface->pending.height = height;
        surface->pending.state = config_state;
    }

    wayland_win_data_release(data);
}

static void xdg_toplevel_handle_close(void *data, struct xdg_toplevel *xdg_toplevel)
{
    NtUserPostMessage((HWND)data, WM_SYSCOMMAND, SC_CLOSE, 0);
}

static const struct xdg_toplevel_listener xdg_toplevel_listener =
{
    xdg_toplevel_handle_configure,
    xdg_toplevel_handle_close
};

/**********************************************************************
 *          wayland_surface_create
 *
 * Creates a role-less wayland surface.
 */
struct wayland_surface *wayland_surface_create(HWND hwnd)
{
    struct wayland_surface *surface;

    surface = calloc(1, sizeof(*surface));
    if (!surface)
    {
        ERR("Failed to allocate space for Wayland surface\n");
        goto err;
    }

    TRACE("surface=%p\n", surface);

    surface->hwnd = hwnd;
    surface->wl_surface = wl_compositor_create_surface(process_wayland.wl_compositor);
    if (!surface->wl_surface)
    {
        ERR("Failed to create wl_surface Wayland surface\n");
        goto err;
    }
    wl_surface_set_user_data(surface->wl_surface, hwnd);

    surface->wp_viewport =
        wp_viewporter_get_viewport(process_wayland.wp_viewporter,
                                   surface->wl_surface);
    if (!surface->wp_viewport)
    {
        ERR("Failed to create wp_viewport Wayland surface\n");
        goto err;
    }

    surface->window.scale = 1.0;

    return surface;

err:
    if (surface) wayland_surface_destroy(surface);
    return NULL;
}

/**********************************************************************
 *          wayland_surface_destroy
 *
 * Destroys a wayland surface.
 */
void wayland_surface_destroy(struct wayland_surface *surface)
{
    pthread_mutex_lock(&process_wayland.pointer.mutex);
    if (process_wayland.pointer.focused_hwnd == surface->hwnd)
    {
        process_wayland.pointer.focused_hwnd = NULL;
        process_wayland.pointer.enter_serial = 0;
    }
    if (process_wayland.pointer.constraint_hwnd == surface->hwnd)
        wayland_pointer_clear_constraint();
    pthread_mutex_unlock(&process_wayland.pointer.mutex);

    pthread_mutex_lock(&process_wayland.keyboard.mutex);
    if (process_wayland.keyboard.focused_hwnd == surface->hwnd)
        process_wayland.keyboard.focused_hwnd = NULL;
    pthread_mutex_unlock(&process_wayland.keyboard.mutex);

    pthread_mutex_lock(&process_wayland.text_input.mutex);
    if (process_wayland.text_input.focused_hwnd == surface->hwnd)
        process_wayland.text_input.focused_hwnd = NULL;
    pthread_mutex_unlock(&process_wayland.text_input.mutex);

    wayland_surface_clear_role(surface);

    if (surface->wp_viewport)
    {
        wp_viewport_destroy(surface->wp_viewport);
        surface->wp_viewport = NULL;
    }

    if (surface->wl_surface)
    {
        wl_surface_destroy(surface->wl_surface);
        surface->wl_surface = NULL;
    }

    if (surface->big_icon_buffer)
    {
        wayland_shm_buffer_unref(surface->big_icon_buffer);
        surface->big_icon_buffer = NULL;
    }

    if (surface->small_icon_buffer)
    {
        wayland_shm_buffer_unref(surface->small_icon_buffer);
        surface->small_icon_buffer = NULL;
    }

    wl_display_flush(process_wayland.wl_display);

    free(surface);
}

/**********************************************************************
 *          wayland_surface_make_toplevel
 *
 * Gives the toplevel role to a plain wayland surface.
 */
void wayland_surface_make_toplevel(struct wayland_surface *surface)
{
    WCHAR text[1024];

    TRACE("surface=%p\n", surface);

    assert(!surface->role || surface->role == WAYLAND_SURFACE_ROLE_TOPLEVEL);
    if (surface->xdg_surface && surface->xdg_toplevel) return;

    wayland_surface_clear_role(surface);
    surface->role = WAYLAND_SURFACE_ROLE_TOPLEVEL;

    surface->xdg_surface =
        xdg_wm_base_get_xdg_surface(process_wayland.xdg_wm_base, surface->wl_surface);
    if (!surface->xdg_surface) goto err;
    xdg_surface_add_listener(surface->xdg_surface, &xdg_surface_listener, surface->hwnd);

    surface->xdg_toplevel = xdg_surface_get_toplevel(surface->xdg_surface);
    if (!surface->xdg_toplevel) goto err;
    xdg_toplevel_add_listener(surface->xdg_toplevel, &xdg_toplevel_listener, surface->hwnd);

    if (process_name)
        xdg_toplevel_set_app_id(surface->xdg_toplevel, process_name);

    if (!NtUserInternalGetWindowText(surface->hwnd, text, ARRAY_SIZE(text)))
        text[0] = 0;
    wayland_surface_set_title(surface, text);

    wayland_surface_assign_icon(surface);

    if (process_wayland.zxdg_exporter_v2 &&
        NtUserGetAncestor(surface->hwnd, GA_ROOT) == surface->hwnd)
    {
        surface->zxdg_exported_v2 =
            zxdg_exporter_v2_export_toplevel(process_wayland.zxdg_exporter_v2,
                                              surface->wl_surface);
        if (surface->zxdg_exported_v2)
            zxdg_exported_v2_add_listener(surface->zxdg_exported_v2,
                                          &xdg_exported_v2_listener, surface);
    }
    else
        wayland_surface_update_foreign_parent(surface);

    wl_surface_commit(surface->wl_surface);
    wl_display_flush(process_wayland.wl_display);

    return;

err:
    wayland_surface_clear_role(surface);
    ERR("Failed to assign toplevel role to wayland surface\n");
}

/**********************************************************************
 *          wayland_surface_make_subsurface
 *
 * Gives the subsurface role to a plain wayland surface.
 */
void wayland_surface_make_subsurface(struct wayland_surface *surface,
                                     struct wayland_surface *parent)
{
    assert(!surface->role || surface->role == WAYLAND_SURFACE_ROLE_SUBSURFACE);
    if (surface->wl_subsurface && surface->toplevel_hwnd == parent->hwnd) return;

    wayland_surface_clear_role(surface);
    surface->role = WAYLAND_SURFACE_ROLE_SUBSURFACE;

    TRACE("surface=%p parent=%p\n", surface, parent);

    surface->wl_subsurface =
        wl_subcompositor_get_subsurface(process_wayland.wl_subcompositor,
                                        surface->wl_surface,
                                        parent->wl_surface);
    if (!surface->wl_subsurface)
    {
        ERR("Failed to create client wl_subsurface\n");
        goto err;
    }

    surface->role = WAYLAND_SURFACE_ROLE_SUBSURFACE;
    surface->toplevel_hwnd = parent->hwnd;

    /* Present contents independently of the parent surface. */
    wl_subsurface_set_desync(surface->wl_subsurface);

    wl_display_flush(process_wayland.wl_display);

    return;

err:
    wayland_surface_clear_role(surface);
    ERR("Failed to assign subsurface role to wayland surface\n");
}

/**********************************************************************
 *          wayland_surface_clear_role
 *
 * Clears the role related Wayland objects of a Wayland surface, making it a
 * plain surface again. We can later assign the same role (but not a
 * different one!) to the surface.
 */
void wayland_surface_clear_role(struct wayland_surface *surface)
{
    TRACE("surface=%p\n", surface);

    if (surface->zxdg_imported_v2)
    {
        if (surface->foreign_parent_hwnd &&
            NtUserGetProp(surface->foreign_parent_hwnd, xdg_foreign_child_prop) == surface->hwnd)
            NtUserRemoveProp(surface->foreign_parent_hwnd, xdg_foreign_child_prop);
        zxdg_imported_v2_destroy(surface->zxdg_imported_v2);
        surface->zxdg_imported_v2 = NULL;
        surface->imported_handle_atom = 0;
        surface->foreign_parent_hwnd = 0;
        surface->foreign_activation_pending = FALSE;
    }

    if (surface->zxdg_exported_v2)
    {
        zxdg_exported_v2_destroy(surface->zxdg_exported_v2);
        surface->zxdg_exported_v2 = NULL;
    }

    if (surface->exported_handle_atom)
    {
        NtUserRemoveProp(surface->hwnd, xdg_foreign_handle_prop);
        NtDeleteAtom(surface->exported_handle_atom);
        surface->exported_handle_atom = 0;
    }

    switch (surface->role)
    {
    case WAYLAND_SURFACE_ROLE_NONE:
        break;

    case WAYLAND_SURFACE_ROLE_TOPLEVEL:
        if (surface->xdg_toplevel_icon)
        {
            xdg_toplevel_icon_manager_v1_set_icon(
                process_wayland.xdg_toplevel_icon_manager_v1,
                surface->xdg_toplevel, NULL);
            xdg_toplevel_icon_v1_destroy(surface->xdg_toplevel_icon);
            surface->xdg_toplevel_icon = NULL;
        }

        if (surface->xdg_toplevel)
        {
            xdg_toplevel_destroy(surface->xdg_toplevel);
            surface->xdg_toplevel = NULL;
        }

        if (surface->xdg_surface)
        {
            xdg_surface_destroy(surface->xdg_surface);
            surface->xdg_surface = NULL;
        }
        break;

    case WAYLAND_SURFACE_ROLE_SUBSURFACE:
        if (surface->wl_subsurface)
        {
            wl_subsurface_destroy(surface->wl_subsurface);
            surface->wl_subsurface = NULL;
        }

        surface->toplevel_hwnd = 0;
        break;
    }

    memset(&surface->pending, 0, sizeof(surface->pending));
    memset(&surface->requested, 0, sizeof(surface->requested));
    memset(&surface->processing, 0, sizeof(surface->processing));
    memset(&surface->current, 0, sizeof(surface->current));
    surface->toplevel_hwnd = 0;

    /* Ensure no buffer is attached, otherwise future role assignments may fail. */
    wl_surface_attach(surface->wl_surface, NULL, 0, 0);
    wl_surface_commit(surface->wl_surface);

    surface->content_width = 0;
    surface->content_height = 0;

    wl_display_flush(process_wayland.wl_display);
}

/**********************************************************************
 *          wayland_surface_attach_shm
 *
 * Attaches a SHM buffer to a wayland surface.
 *
 * The buffer is marked as unavailable until committed and subsequently
 * released by the compositor.
 */
void wayland_surface_attach_shm(struct wayland_surface *surface,
                                struct wayland_shm_buffer *shm_buffer,
                                HRGN surface_damage_region)
{
    RGNDATA *surface_damage;
    int win_width, win_height;

    TRACE("surface=%p shm_buffer=%p (%dx%d)\n",
          surface, shm_buffer, shm_buffer->width, shm_buffer->height);

    shm_buffer->busy = TRUE;
    wayland_shm_buffer_ref(shm_buffer);

    wl_surface_attach(surface->wl_surface, shm_buffer->wl_buffer, 0, 0);

    /* Add surface damage, i.e., which parts of the surface have changed since
     * the last surface commit. Note that this is different from the buffer
     * damage region. */
    surface_damage = get_region_data(surface_damage_region);
    if (surface_damage)
    {
        RECT *rgn_rect = (RECT *)surface_damage->Buffer;
        RECT *rgn_rect_end = rgn_rect + surface_damage->rdh.nCount;

        for (;rgn_rect < rgn_rect_end; rgn_rect++)
        {
            wl_surface_damage_buffer(surface->wl_surface,
                                     rgn_rect->left, rgn_rect->top,
                                     rgn_rect->right - rgn_rect->left,
                                     rgn_rect->bottom - rgn_rect->top);
        }
        free(surface_damage);
    }

    win_width = surface->window.rect.right - surface->window.rect.left;
    win_height = surface->window.rect.bottom - surface->window.rect.top;

    /* It is an error to specify a wp_viewporter source rectangle that
     * is partially or completely outside of the wl_buffe.
     * 0 is also an invalid width / height value so use 1x1 instead.
     */
    win_width = max(1, min(win_width, shm_buffer->width));
    win_height = max(1, min(win_height, shm_buffer->height));

    wp_viewport_set_source(surface->wp_viewport, 0, 0,
                           wl_fixed_from_int(win_width),
                           wl_fixed_from_int(win_height));

    surface->content_width = win_width;
    surface->content_height = win_height;
}

/**********************************************************************
 *          wayland_surface_config_is_compatible
 *
 * Checks whether a wayland_surface_config object is compatible with the
 * the provided arguments.
 */
BOOL wayland_surface_config_is_compatible(struct wayland_surface_config *conf,
                                          int width, int height,
                                          enum wayland_surface_config_state state)
{
    static enum wayland_surface_config_state mask =
        WAYLAND_SURFACE_CONFIG_STATE_MAXIMIZED;

    /* We require the same state. */
    if ((state & mask) != (conf->state & mask)) return FALSE;

    /* The maximized state requires the configured size. During surface
     * reconfiguration we can use surface geometry to provide smaller areas
     * from larger sizes, so only smaller sizes are incompatible. */
    if ((conf->state & WAYLAND_SURFACE_CONFIG_STATE_MAXIMIZED) &&
        (width < conf->width || height < conf->height))
    {
        return FALSE;
    }

    /* The fullscreen state requires a size smaller or equal to the configured
     * size. If we have a larger size, we can use surface geometry during
     * surface reconfiguration to provide the smaller size, so we are always
     * compatible with a fullscreen state. */

    return TRUE;
}

/**********************************************************************
 *          wayland_surface_get_rect_in_monitor
 *
 * Gets the largest rectangle within a surface's window (in window coordinates)
 * that is visible in a monitor.
 */
static void wayland_surface_get_rect_in_monitor(struct wayland_surface *surface,
                                                RECT *rect)
{
    HMONITOR hmonitor;
    MONITORINFO mi;

    mi.cbSize = sizeof(mi);
    if (!(hmonitor = NtUserMonitorFromRect(&surface->window.rect, 0)) ||
        !NtUserGetMonitorInfo(hmonitor, (MONITORINFO *)&mi))
    {
        SetRectEmpty(rect);
        return;
    }

    intersect_rect(rect, &mi.rcMonitor, &surface->window.rect);
    OffsetRect(rect, -surface->window.rect.left, -surface->window.rect.top);
}

/**********************************************************************
 *          wayland_surface_reconfigure_geometry
 *
 * Sets the xdg_surface geometry
 */
static void wayland_surface_reconfigure_geometry(struct wayland_surface *surface,
                                                 int width, int height)
{
    RECT rect;

    /* If the window size is bigger than the current state accepts, use the
     * largest visible (from Windows' perspective) subregion of the window. */
    if ((surface->current.state & (WAYLAND_SURFACE_CONFIG_STATE_MAXIMIZED |
                                   WAYLAND_SURFACE_CONFIG_STATE_FULLSCREEN)) &&
        (width > surface->current.width || height > surface->current.height))
    {
        wayland_surface_get_rect_in_monitor(surface, &rect);

        wayland_surface_coords_from_window(surface, rect.left, rect.top,
                                           (int *)&rect.left, (int *)&rect.top);
        wayland_surface_coords_from_window(surface, rect.right, rect.bottom,
                                           (int *)&rect.right, (int *)&rect.bottom);

        /* If the window rect in the monitor is smaller than required,
         * fall back to an appropriately sized rect at the top-left. */
        if ((surface->current.state & WAYLAND_SURFACE_CONFIG_STATE_MAXIMIZED) &&
            (rect.right - rect.left < surface->current.width ||
             rect.bottom - rect.top < surface->current.height))
        {
            SetRect(&rect, 0, 0, surface->current.width, surface->current.height);
        }
        else
        {
            rect.right = min(rect.right, rect.left + surface->current.width);
            rect.bottom = min(rect.bottom, rect.top + surface->current.height);
        }
        TRACE("Window is too large for Wayland state, using subregion\n");
    }
    else
    {
        SetRect(&rect, 0, 0, width, height);
    }

    TRACE("hwnd=%p geometry=%s\n", surface->hwnd, wine_dbgstr_rect(&rect));

    if (!IsRectEmpty(&rect))
    {
        xdg_surface_set_window_geometry(surface->xdg_surface,
                                        rect.left, rect.top,
                                        rect.right - rect.left,
                                        rect.bottom - rect.top);
    }
}

/**********************************************************************
 *          wayland_surface_reconfigure_size
 *
 * Sets the surface size with viewporter
 */
static void wayland_surface_reconfigure_size(struct wayland_surface *surface,
                                             int width, int height)
{
    TRACE("hwnd=%p size=%dx%d\n", surface->hwnd, width, height);

    if (width != 0 && height != 0)
        wp_viewport_set_destination(surface->wp_viewport, width, height);
    else
        wp_viewport_set_destination(surface->wp_viewport, -1, -1);
}

/**********************************************************************
 *          wayland_surface_reconfigure_client
 *
 * Reconfigures the subsurface covering the client area.
 */
static void wayland_surface_reconfigure_client(struct wayland_surface *surface,
                                               struct wayland_client_surface *client,
                                               const RECT *client_rect)
{
    struct wayland_window_config *window = &surface->window;
    int client_x, client_y, x, y;
    int client_width, client_height, width, height;

    /* The offset of the client area origin relatively to the window origin. */
    client_x = client_rect->left + window->client_rect.left - window->rect.left;
    client_y = client_rect->top + window->client_rect.top - window->rect.top;

    client_width = client_rect->right - client_rect->left;
    client_height = client_rect->bottom - client_rect->top;

    wayland_surface_coords_from_window(surface, client_x, client_y, &x, &y);
    wayland_surface_coords_from_window(surface, client_width, client_height,
                                       &width, &height);

    TRACE("hwnd=%p subsurface=%d,%d+%dx%d\n", surface->hwnd, x, y, width, height);

    if (client->wl_subsurface)
    {
        wl_subsurface_set_position(client->wl_subsurface, x, y);
        wl_subsurface_place_above(client->wl_subsurface, surface->wl_surface);
    }

    if (width != 0 && height != 0)
        wp_viewport_set_destination(client->wp_viewport, width, height);
    else /* We can't have a 0x0 destination, use 1x1 instead. */
        wp_viewport_set_destination(client->wp_viewport, 1, 1);
}

/**********************************************************************
 *          wayland_surface_reconfigure_xdg
 *
 * Reconfigures the xdg surface as needed to match the latest requested
 * state.
 */
static BOOL wayland_surface_reconfigure_xdg(struct wayland_surface *surface,
                                            int width, int height)
{
    struct wayland_window_config *window = &surface->window;

    /* Acknowledge any compatible processed config. */
    if (surface->processing.serial && surface->processing.processed &&
        wayland_surface_config_is_compatible(&surface->processing,
                                             width, height,
                                             window->state))
    {
        surface->current = surface->processing;
        memset(&surface->processing, 0, sizeof(surface->processing));
        xdg_surface_ack_configure(surface->xdg_surface, surface->current.serial);
    }
    /* If this is the initial configure, and we have a compatible requested
     * config, use that, in order to draw windows that don't go through the
     * message loop (e.g., some splash screens). */
    else if (!surface->current.serial && surface->requested.serial &&
             wayland_surface_config_is_compatible(&surface->requested,
                                                  width, height,
                                                  window->state))
    {
        surface->current = surface->requested;
        memset(&surface->requested, 0, sizeof(surface->requested));
        xdg_surface_ack_configure(surface->xdg_surface, surface->current.serial);
    }
    else if (!surface->current.serial ||
             !wayland_surface_config_is_compatible(&surface->current,
                                                   width, height,
                                                   window->state))
    {
        return FALSE;
    }

    wayland_surface_reconfigure_geometry(surface, width, height);

    return TRUE;
}

/**********************************************************************
 *          wayland_surface_reconfigure_subsurface
 *
 * Reconfigures the subsurface as needed to match the latest requested
 * state.
 */
static void wayland_surface_reconfigure_subsurface(struct wayland_surface *surface)
{
    struct wayland_win_data *toplevel_data;
    struct wayland_surface *toplevel_surface;
    int local_x, local_y, x, y;

    if (surface->processing.serial && surface->processing.processed &&
        (toplevel_data = wayland_win_data_get_nolock(surface->toplevel_hwnd)) &&
        (toplevel_surface = toplevel_data->wayland_surface))
    {
        local_x = surface->window.rect.left - toplevel_surface->window.rect.left;
        local_y = surface->window.rect.top - toplevel_surface->window.rect.top;

        wayland_surface_coords_from_window(surface, local_x, local_y, &x, &y);

        TRACE("hwnd=%p pos=%d,%d\n", surface->hwnd, x, y);

        wl_subsurface_set_position(surface->wl_subsurface, x, y);
        /* popups go above every client surface of their toplevel */
        toplevel_data->restack_pending = TRUE;
        wl_surface_commit(toplevel_surface->wl_surface);

        memset(&surface->processing, 0, sizeof(surface->processing));
    }
}

/**********************************************************************
 *          wayland_surface_reconfigure
 *
 * Reconfigures the wayland surface as needed to match the latest requested
 * state.
 */
BOOL wayland_surface_reconfigure(struct wayland_surface *surface)
{
    struct wayland_window_config *window = &surface->window;
    int win_width, win_height, width, height;

    win_width = surface->window.rect.right - surface->window.rect.left;
    win_height = surface->window.rect.bottom - surface->window.rect.top;

    wayland_surface_coords_from_window(surface, win_width, win_height,
                                       &width, &height);

    TRACE("hwnd=%p window=%dx%d,%#x processing=%dx%d,%#x current=%dx%d,%#x\n",
          surface->hwnd, win_width, win_height, window->state,
          surface->processing.width, surface->processing.height,
          surface->processing.state, surface->current.width,
          surface->current.height, surface->current.state);

    switch (surface->role)
    {
    case WAYLAND_SURFACE_ROLE_NONE:
        break;
    case WAYLAND_SURFACE_ROLE_TOPLEVEL:
        if (!surface->xdg_surface) break; /* surface role has been cleared */
        if (!wayland_surface_reconfigure_xdg(surface, width, height)) return FALSE;
        break;
    case WAYLAND_SURFACE_ROLE_SUBSURFACE:
        if (!surface->wl_subsurface) break; /* surface role has been cleared */
        wayland_surface_reconfigure_subsurface(surface);
        break;
    }

    wayland_surface_reconfigure_size(surface, width, height);

    return TRUE;
}

/**********************************************************************
 *          wayland_shm_buffer_ref
 *
 * Increases the reference count of a SHM buffer.
 */
void wayland_shm_buffer_ref(struct wayland_shm_buffer *shm_buffer)
{
    InterlockedIncrement(&shm_buffer->ref);
}

/**********************************************************************
 *          wayland_shm_buffer_unref
 *
 * Decreases the reference count of a SHM buffer (and may destroy it).
 */
void wayland_shm_buffer_unref(struct wayland_shm_buffer *shm_buffer)
{
    if (InterlockedDecrement(&shm_buffer->ref) > 0) return;

    TRACE("destroying %p map=%p\n", shm_buffer, shm_buffer->map_data);

    if (shm_buffer->wl_buffer)
        wl_buffer_destroy(shm_buffer->wl_buffer);
    if (shm_buffer->map_data)
        NtUnmapViewOfSection(GetCurrentProcess(), shm_buffer->map_data);
    if (shm_buffer->damage_region)
        NtGdiDeleteObjectApp(shm_buffer->damage_region);

    free(shm_buffer);
}

/**********************************************************************
 *          wayland_shm_buffer_create
 *
 * Creates a SHM buffer with the specified width, height and format.
 */
struct wayland_shm_buffer *wayland_shm_buffer_create(int width, int height,
                                                     enum wl_shm_format format)
{
    struct wayland_shm_buffer *shm_buffer = NULL;
    HANDLE handle = 0;
    int fd = -1;
    SIZE_T view_size = 0;
    LARGE_INTEGER section_size;
    NTSTATUS status;
    struct wl_shm_pool *pool;
    int stride, size;

    stride = width * WINEWAYLAND_BYTES_PER_PIXEL;
    size = stride * height;
    if (size == 0)
    {
        ERR("Invalid shm_buffer size %dx%d\n", width, height);
        goto err;
    }

    shm_buffer = calloc(1, sizeof(*shm_buffer));
    if (!shm_buffer)
    {
        ERR("Failed to allocate space for SHM buffer\n");
        goto err;
    }

    TRACE("%p %dx%d format=%d size=%d\n", shm_buffer, width, height, format, size);

    shm_buffer->ref = 1;
    shm_buffer->width = width;
    shm_buffer->height = height;
    shm_buffer->format = format;
    shm_buffer->map_size = size;

    shm_buffer->damage_region = NtGdiCreateRectRgn(0, 0, width, height);
    if (!shm_buffer->damage_region)
    {
        ERR("Failed to create buffer damage region\n");
        goto err;
    }

    section_size.QuadPart = size;
    status = NtCreateSection(&handle,
                             GENERIC_READ | SECTION_MAP_READ | SECTION_MAP_WRITE,
                             NULL, &section_size, PAGE_READWRITE, SEC_COMMIT, 0);
    if (status)
    {
        ERR("Failed to create SHM section status=0x%x\n", status);
        goto err;
    }

    status = NtMapViewOfSection(handle, GetCurrentProcess(),
                                (PVOID)&shm_buffer->map_data, 0, 0, NULL,
                                &view_size, ViewUnmap, 0, PAGE_READWRITE);
    if (status)
    {
        shm_buffer->map_data = NULL;
        ERR("Failed to create map SHM handle status=0x%x\n", status);
        goto err;
    }

    status = wine_server_handle_to_fd(handle, FILE_READ_DATA, &fd, NULL);
    if (status)
    {
        ERR("Failed to get fd from SHM handle status=0x%x\n", status);
        goto err;
    }

    pool = wl_shm_create_pool(process_wayland.wl_shm, fd, size);
    if (!pool)
    {
        ERR("Failed to create SHM pool fd=%d size=%d\n", fd, size);
        goto err;
    }
    shm_buffer->wl_buffer = wl_shm_pool_create_buffer(pool, 0, width, height,
                                                      stride, format);
    wl_shm_pool_destroy(pool);
    if (!shm_buffer->wl_buffer)
    {
        ERR("Failed to create SHM buffer %dx%d\n", width, height);
        goto err;
    }

    close(fd);
    NtClose(handle);

    TRACE("=> map=%p\n", shm_buffer->map_data);

    return shm_buffer;

err:
    if (fd >= 0) close(fd);
    if (handle) NtClose(handle);
    if (shm_buffer) wayland_shm_buffer_unref(shm_buffer);
    return NULL;
}

/***********************************************************************
 *           wayland_shm_buffer_from_color_bitmaps
 *
 * Create a wayland_shm_buffer for a color bitmap.
 *
 * Adapted from wineandroid.drv code.
 */
struct wayland_shm_buffer *wayland_shm_buffer_from_color_bitmaps(HDC hdc, HBITMAP color,
                                                                 HBITMAP mask)
{
    struct wayland_shm_buffer *shm_buffer = NULL;
    char buffer[FIELD_OFFSET(BITMAPINFO, bmiColors[256])];
    BITMAPINFO *info = (BITMAPINFO *)buffer;
    BITMAP bm;
    unsigned int *ptr, *bits = NULL;
    unsigned char *mask_bits = NULL;
    int i, j;
    BOOL has_alpha = FALSE;

    if (!NtGdiExtGetObjectW(color, sizeof(bm), &bm)) goto failed;

    shm_buffer = wayland_shm_buffer_create(bm.bmWidth, bm.bmHeight,
                                           WL_SHM_FORMAT_ARGB8888);
    if (!shm_buffer) goto failed;
    bits = shm_buffer->map_data;

    info->bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    info->bmiHeader.biWidth = bm.bmWidth;
    info->bmiHeader.biHeight = -bm.bmHeight;
    info->bmiHeader.biPlanes = 1;
    info->bmiHeader.biBitCount = 32;
    info->bmiHeader.biCompression = BI_RGB;
    info->bmiHeader.biSizeImage = bm.bmWidth * bm.bmHeight * 4;
    info->bmiHeader.biXPelsPerMeter = 0;
    info->bmiHeader.biYPelsPerMeter = 0;
    info->bmiHeader.biClrUsed = 0;
    info->bmiHeader.biClrImportant = 0;

    if (!NtGdiGetDIBitsInternal(hdc, color, 0, bm.bmHeight, bits, info,
                                DIB_RGB_COLORS, 0, 0))
        goto failed;

    for (i = 0; i < bm.bmWidth * bm.bmHeight; i++)
        if ((has_alpha = (bits[i] & 0xff000000) != 0)) break;

    if (!has_alpha)
    {
        unsigned int width_bytes = (bm.bmWidth + 31) / 32 * 4;
        /* generate alpha channel from the mask */
        info->bmiHeader.biBitCount = 1;
        info->bmiHeader.biSizeImage = width_bytes * bm.bmHeight;
        if (!(mask_bits = malloc(info->bmiHeader.biSizeImage))) goto failed;
        if (!NtGdiGetDIBitsInternal(hdc, mask, 0, bm.bmHeight, mask_bits,
                                    info, DIB_RGB_COLORS, 0, 0))
            goto failed;
        ptr = bits;
        for (i = 0; i < bm.bmHeight; i++)
        {
            for (j = 0; j < bm.bmWidth; j++, ptr++)
            {
                if (!((mask_bits[i * width_bytes + j / 8] << (j % 8)) & 0x80))
                    *ptr |= 0xff000000;
            }
        }
        free(mask_bits);
    }

    /* Wayland requires pre-multiplied alpha values */
    for (ptr = bits, i = 0; i < bm.bmWidth * bm.bmHeight; ptr++, i++)
    {
        unsigned char alpha = *ptr >> 24;
        if (alpha == 0)
        {
            *ptr = 0;
        }
        else if (alpha != 255)
        {
            *ptr = (alpha << 24) |
                   (((BYTE)(*ptr >> 16) * alpha / 255) << 16) |
                   (((BYTE)(*ptr >> 8) * alpha / 255) << 8) |
                   (((BYTE)*ptr * alpha / 255));
        }
    }

    return shm_buffer;

failed:
    if (shm_buffer) wayland_shm_buffer_unref(shm_buffer);
    free(mask_bits);
    return NULL;
}

/**********************************************************************
 *          wayland_surface_coords_from_window
 *
 * Converts the window (logical) coordinates to wayland surface-local coordinates.
 */
void wayland_surface_coords_from_window(struct wayland_surface *surface,
                                        int window_x, int window_y,
                                        int *surface_x, int *surface_y)
{
    *surface_x = round(window_x / surface->window.scale);
    *surface_y = round(window_y / surface->window.scale);
}

/**********************************************************************
 *          wayland_surface_coords_to_window
 *
 * Converts the surface-local coordinates to window (logical) coordinates.
 */
void wayland_surface_coords_to_window(struct wayland_surface *surface,
                                      double surface_x, double surface_y,
                                      int *window_x, int *window_y)
{
    *window_x = round(surface_x * surface->window.scale);
    *window_y = round(surface_y * surface->window.scale);
}

static struct wayland_client_surface *impl_from_client_surface(struct client_surface *client)
{
    return CONTAINING_RECORD(client, struct wayland_client_surface, client);
}

static void foreign_activation_frame_done(void *private, struct wl_callback *callback,
                                          uint32_t callback_data)
{
    struct wayland_client_surface *client = private;
    struct wayland_win_data *data;

    if (client->foreign_activation_frame_callback != callback) return;
    wl_callback_destroy(callback);
    client->foreign_activation_frame_callback = NULL;

    if ((data = wayland_win_data_get(client->client.hwnd)))
    {
        if (data->wayland_surface && data->wayland_surface->foreign_activation_pending)
        {
            data->wayland_surface->foreign_activation_pending = FALSE;
            wayland_request_surface_activation(data->wayland_surface);
        }
        wayland_win_data_release(data);
    }
}

static const struct wl_callback_listener foreign_activation_frame_listener =
{
    foreign_activation_frame_done,
};

static void wayland_client_surface_schedule_activation(struct wayland_client_surface *client)
{
    if (client->foreign_activation_frame_callback) return;
    if (!(client->foreign_activation_frame_callback = wl_surface_frame(client->wl_surface))) return;
    wl_callback_add_listener(client->foreign_activation_frame_callback,
                             &foreign_activation_frame_listener, client);
}

static void wayland_client_surface_destroy(struct client_surface *client)
{
    struct wayland_client_surface *surface = impl_from_client_surface(client);

    TRACE("%s\n", debugstr_client_surface(client));

    if (surface->wp_viewport)
        wp_viewport_destroy(surface->wp_viewport);
    if (surface->foreign_activation_frame_callback)
        wl_callback_destroy(surface->foreign_activation_frame_callback);
    if (surface->wl_subsurface)
        wl_subsurface_destroy(surface->wl_subsurface);
    if (surface->wl_surface)
        wl_surface_destroy(surface->wl_surface);
}

static void wayland_client_surface_detach(struct client_surface *client)
{
    struct wayland_client_surface *surface = impl_from_client_surface(client);
    struct wayland_win_data *data;

    if ((data = wayland_win_data_get(client->hwnd)))
    {
        if (data->client_surface == surface) data->client_surface = NULL;
        wayland_client_surface_attach(surface, NULL);
        wayland_win_data_release(data);
    }
}

static void wayland_client_surface_attach_rect(struct wayland_client_surface *client, HWND toplevel,
                                               const RECT *rect);

static void wayland_client_surface_update(struct client_surface *client)
{
    struct wayland_client_surface *surface = impl_from_client_surface(client);
    HWND hwnd = client->hwnd, toplevel = NtUserGetAncestor(hwnd, GA_ROOT);
    struct wayland_win_data *data;
    BOOL visible = toplevel && NtUserIsWindowVisible(hwnd);
    RECT client_rect, toplevel_rect;

    /* This runs on every present, possibly while another thread holds the
     * win32u user lock and waits for the win_data lock, so query the window
     * geometry before taking the win_data lock. */
    NtUserGetClientRect(hwnd, &client_rect, NtUserGetWinMonitorDpi(hwnd, MDT_RAW_DPI));
    toplevel_rect = client_rect;
    if (toplevel)
        NtUserMapWindowPoints(hwnd, toplevel, (POINT *)&toplevel_rect, 2, NtUserGetWinMonitorDpi(hwnd, MDT_RAW_DPI));

    if (!(data = wayland_win_data_get(hwnd))) return;

    if (visible && data->client_surface == surface)
    {
        wayland_client_surface_attach_rect(surface, toplevel, &toplevel_rect);
        if (!surface->toplevel && data->wayland_surface)
            wayland_client_surface_attach_rect(surface, hwnd, &client_rect);
    }
    else
        wayland_client_surface_attach(surface, NULL);

    wayland_win_data_release(data);

    if (toplevel) wayland_restack_client_surfaces(toplevel);
}

static void wayland_client_surface_present(struct client_surface *client, HDC hdc)
{
    struct wayland_client_surface *surface = impl_from_client_surface(client);
    HWND hwnd = client->hwnd, toplevel = NtUserGetAncestor(hwnd, GA_ROOT);
    struct wayland_win_data *data;

    ensure_window_surface_contents(toplevel);
    set_client_surface(hwnd, surface);
    if (hwnd != toplevel) ensure_window_surface_contents(hwnd);
    if (hwnd != toplevel && (data = wayland_win_data_get(hwnd)))
    {
        if (data->wayland_surface)
        {
            wayland_surface_update_foreign_parent(data->wayland_surface);
            if (data->wayland_surface->foreign_activation_pending)
                wayland_client_surface_schedule_activation(surface);
        }
        wayland_win_data_release(data);
    }
}

static const struct client_surface_funcs wayland_client_surface_funcs =
{
    .destroy = wayland_client_surface_destroy,
    .detach = wayland_client_surface_detach,
    .update = wayland_client_surface_update,
    .present = wayland_client_surface_present,
};

struct wayland_client_surface *wayland_client_surface_create(HWND hwnd, int format, BOOL raw)
{
    struct wayland_client_surface *client;
    struct wl_region *empty_region;

    if ((client = (struct wayland_client_surface *)get_unused_client_surface(hwnd, format, raw))) return client;
    if (!(client = reusable_client_surface_create(sizeof(*client), &wayland_client_surface_funcs,
                                                  hwnd, format, raw))) return NULL;
    client->client.host_scaled = TRUE;

    client->wl_surface =
        wl_compositor_create_surface(process_wayland.wl_compositor);
    if (!client->wl_surface)
    {
        ERR("Failed to create client wl_surface\n");
        goto err;
    }
    wl_surface_set_user_data(client->wl_surface, hwnd);

    /* Let parent handle all pointer events. */
    empty_region = wl_compositor_create_region(process_wayland.wl_compositor);
    if (!empty_region)
    {
        ERR("Failed to create wl_region\n");
        goto err;
    }
    wl_surface_set_input_region(client->wl_surface, empty_region);
    wl_region_destroy(empty_region);

    client->wp_viewport =
        wp_viewporter_get_viewport(process_wayland.wp_viewporter,
                                    client->wl_surface);
    if (!client->wp_viewport)
    {
        ERR("Failed to create client wp_viewport\n");
        goto err;
    }

    return client;

err:
    client_surface_release(&client->client);
    return NULL;
}

/* Stack the client subsurfaces of a toplevel in window z-order. Each of them
 * is placed right above the parent surface when reconfigured, so without this
 * the last reconfigured child ends up below its siblings, and an opaque
 * sibling that Windows paints underneath can hide it completely. */
static HWND *build_hwnd_list(HWND parent, BOOL children, ULONG *count)
{
    HWND *list;
    NTSTATUS status;

    *count = 128;
    for (;;)
    {
        if (!(list = malloc(*count * sizeof(*list)))) return NULL;
        status = NtUserBuildHwndList(0, parent, children, children, 0, *count, list, count);
        if (!status) break;
        free(list);
        if (status != STATUS_BUFFER_TOO_SMALL) return NULL;
    }
    /* the list ends with HWND_BOTTOM */
    (*count)--;
    return list;
}

/* GDI paints child windows into the window surface of their toplevel, below
 * all the client surfaces, while Windows shows a child above the client
 * surfaces painted before it. An overlay subsurface shows a copy of the
 * window surface pixels of such a child at its place in painting order. */
struct wayland_overlay
{
    struct wl_list link;
    HWND hwnd;
    RECT rect; /* in window surface coordinates of the toplevel */
    struct wl_surface *wl_surface;
    struct wl_subsurface *wl_subsurface;
    struct wp_viewport *wp_viewport;
    struct wayland_shm_buffer *buffers[2];
    BOOL needs_contents;
    BOOL used;
};

static void overlay_buffer_release(void *data, struct wl_buffer *buffer)
{
    struct wayland_shm_buffer *shm_buffer = data;
    TRACE("shm_buffer=%p\n", shm_buffer);
    shm_buffer->busy = FALSE;
    wayland_shm_buffer_unref(shm_buffer);
}

static const struct wl_buffer_listener overlay_buffer_listener =
{
    overlay_buffer_release
};

static void wayland_overlay_destroy(struct wayland_overlay *overlay)
{
    UINT i;

    TRACE("hwnd=%p\n", overlay->hwnd);

    wl_list_remove(&overlay->link);
    for (i = 0; i < ARRAY_SIZE(overlay->buffers); i++)
        if (overlay->buffers[i]) wayland_shm_buffer_unref(overlay->buffers[i]);
    if (overlay->wp_viewport) wp_viewport_destroy(overlay->wp_viewport);
    if (overlay->wl_subsurface) wl_subsurface_destroy(overlay->wl_subsurface);
    if (overlay->wl_surface) wl_surface_destroy(overlay->wl_surface);
    free(overlay);
}

static struct wayland_overlay *wayland_overlay_create(struct wayland_win_data *toplevel_data, HWND hwnd)
{
    struct wayland_surface *surface = toplevel_data->wayland_surface;
    struct wayland_overlay *overlay;
    struct wl_region *empty_region;

    if (!(overlay = calloc(1, sizeof(*overlay)))) return NULL;
    wl_list_insert(&toplevel_data->overlays, &overlay->link);
    overlay->hwnd = hwnd;

    if (!(overlay->wl_surface = wl_compositor_create_surface(process_wayland.wl_compositor)))
        goto err;
    wl_surface_set_user_data(overlay->wl_surface, hwnd);

    /* Let the toplevel handle all pointer events. */
    if (!(empty_region = wl_compositor_create_region(process_wayland.wl_compositor)))
        goto err;
    wl_surface_set_input_region(overlay->wl_surface, empty_region);
    wl_region_destroy(empty_region);

    if (!(overlay->wp_viewport = wp_viewporter_get_viewport(process_wayland.wp_viewporter,
                                                            overlay->wl_surface)))
        goto err;
    if (!(overlay->wl_subsurface = wl_subcompositor_get_subsurface(process_wayland.wl_subcompositor,
                                                                   overlay->wl_surface,
                                                                   surface->wl_surface)))
        goto err;

    TRACE("hwnd=%p\n", hwnd);
    return overlay;

err:
    ERR("Failed to create overlay for hwnd=%p\n", hwnd);
    wayland_overlay_destroy(overlay);
    return NULL;
}

static void wayland_overlay_set_rect(struct wayland_overlay *overlay, struct wayland_surface *surface,
                                     const RECT *rect)
{
    int x, y, width, height;

    if (memcmp(&overlay->rect, rect, sizeof(*rect)))
    {
        overlay->rect = *rect;
        overlay->needs_contents = TRUE;
    }

    wayland_surface_coords_from_window(surface, rect->left, rect->top, &x, &y);
    wayland_surface_coords_from_window(surface, rect->right - rect->left, rect->bottom - rect->top,
                                       &width, &height);

    TRACE("hwnd=%p rect=%s subsurface=%d,%d+%dx%d\n", overlay->hwnd, wine_dbgstr_rect(rect),
          x, y, width, height);

    wl_subsurface_set_position(overlay->wl_subsurface, x, y);
    wp_viewport_set_destination(overlay->wp_viewport, max(1, width), max(1, height));
}

static void wayland_overlay_update(struct wayland_overlay *overlay, struct wayland_shm_buffer *contents)
{
    static const int bpp = WINEWAYLAND_BYTES_PER_PIXEL;
    int width = overlay->rect.right - overlay->rect.left;
    int height = overlay->rect.bottom - overlay->rect.top;
    RECT contents_rect = {0, 0, contents->width, contents->height}, rect;
    struct wayland_shm_buffer *buffer = NULL;
    UINT i;
    int y;

    for (i = 0; i < ARRAY_SIZE(overlay->buffers); i++)
    {
        if (!(buffer = overlay->buffers[i])) continue;
        if (!buffer->busy && buffer->width == width && buffer->height == height &&
            buffer->format == contents->format)
            break;
    }
    if (i == ARRAY_SIZE(overlay->buffers))
    {
        /* replace a free buffer of the wrong size, or the first one if both
         * are busy, the compositor keeps a reference until it releases it */
        for (i = 0; i < ARRAY_SIZE(overlay->buffers); i++)
            if (!overlay->buffers[i] || !overlay->buffers[i]->busy) break;
        if (i == ARRAY_SIZE(overlay->buffers)) i = 0;
        if (overlay->buffers[i]) wayland_shm_buffer_unref(overlay->buffers[i]);
        if (!(buffer = overlay->buffers[i] = wayland_shm_buffer_create(width, height, contents->format)))
            return;
        wl_buffer_add_listener(buffer->wl_buffer, &overlay_buffer_listener, buffer);
    }

    if (intersect_rect(&rect, &overlay->rect, &contents_rect))
    {
        for (y = rect.top; y < rect.bottom; y++)
        {
            memcpy((char *)buffer->map_data + ((y - overlay->rect.top) * width + rect.left - overlay->rect.left) * bpp,
                   (const char *)contents->map_data + (y * contents->width + rect.left) * bpp,
                   (rect.right - rect.left) * bpp);
        }
    }

    buffer->busy = TRUE;
    wayland_shm_buffer_ref(buffer);
    wl_surface_attach(overlay->wl_surface, buffer->wl_buffer, 0, 0);
    wl_surface_damage_buffer(overlay->wl_surface, 0, 0, width, height);
    wl_surface_commit(overlay->wl_surface);
    overlay->needs_contents = FALSE;
}

/**********************************************************************
 *          wayland_update_overlays
 *
 * Copies the new window surface contents of a toplevel into the overlays
 * that need them. Must be called before committing the toplevel surface.
 */
void wayland_update_overlays(struct wayland_win_data *data, struct wayland_shm_buffer *contents,
                             HRGN damage_region)
{
    struct wayland_overlay *overlay;

    wl_list_for_each(overlay, &data->overlays, link)
    {
        if (overlay->needs_contents || (damage_region && NtGdiRectInRegion(damage_region, &overlay->rect)))
            wayland_overlay_update(overlay, contents);
    }
}

/**********************************************************************
 *          wayland_destroy_overlays
 */
void wayland_destroy_overlays(struct wayland_win_data *data)
{
    struct wayland_overlay *overlay, *next;

    wl_list_for_each_safe(overlay, next, &data->overlays, link)
        wayland_overlay_destroy(overlay);
}

static struct wayland_overlay *wayland_find_overlay(struct wayland_win_data *data, HWND hwnd)
{
    struct wayland_overlay *overlay;

    wl_list_for_each(overlay, &data->overlays, link)
        if (overlay->hwnd == hwnd) return overlay;
    return NULL;
}

struct restack_rects
{
    RECT window; /* visible part of the window, in toplevel client coordinates */
    RECT client; /* visible part of its client area */
    BOOL opaque; /* the window is visible and hides what is painted before it */
};

static void get_restack_rects(HWND hwnd, HWND toplevel, UINT dpi, struct restack_rects *rects)
{
    BOOL transparent = FALSE;
    HWND parent = hwnd;
    RECT rect;

    memset(rects, 0, sizeof(*rects));
    if (!NtUserIsWindowVisible(hwnd)) return;

    NtUserGetWindowRect(hwnd, &rects->window, dpi);
    NtUserMapWindowPoints(0, toplevel, (POINT *)&rects->window, 2, dpi);
    while (parent != toplevel)
    {
        /* a transparent window and its children are not clipped out of
         * their parent, which paints over them */
        if (NtUserGetWindowLongW(parent, GWL_EXSTYLE) & WS_EX_TRANSPARENT) transparent = TRUE;
        if (!(parent = NtUserGetAncestor(parent, GA_PARENT))) break;
        NtUserGetClientRect(parent, &rect, dpi);
        NtUserMapWindowPoints(parent, toplevel, (POINT *)&rect, 2, dpi);
        if (!intersect_rect(&rects->window, &rects->window, &rect)) return;
    }

    NtUserGetClientRect(hwnd, &rect, dpi);
    NtUserMapWindowPoints(hwnd, toplevel, (POINT *)&rect, 2, dpi);
    intersect_rect(&rects->client, &rect, &rects->window);
    rects->opaque = !transparent;
}

static BOOL rect_contains(const RECT *outer, const RECT *inner)
{
    return inner->left >= outer->left && inner->top >= outer->top &&
           inner->right <= outer->right && inner->bottom <= outer->bottom;
}

/**********************************************************************
 *          wayland_restack_client_surfaces
 *
 * Stacks the client surfaces of a toplevel in painting order, with overlays
 * for the child windows painted above them and its owned popups on top.
 * Walking the window tree takes the win32u user lock, which may be held
 * while waiting for the win_data lock, so this must be called without
 * holding the win_data lock.
 */
void wayland_restack_client_surfaces(HWND toplevel)
{
    HWND *list, *popups = NULL, *stack = NULL, *order;
    struct wayland_client_surface *client;
    struct wayland_win_data *toplevel_data, *data;
    struct wayland_surface *surface, *popup;
    struct wayland_overlay *overlay, *next;
    struct restack_rects *rects = NULL;
    RECT *covered = NULL, toplevel_client, bounds, overlaid, rect;
    struct wl_surface *below;
    ULONG count, popup_count;
    UINT i, j, depth = 0, done = 0, covered_count = 0, dpi;

    if (!(toplevel_data = wayland_win_data_get(toplevel))) return;
    if (!toplevel_data->restack_pending)
    {
        wayland_win_data_release(toplevel_data);
        return;
    }
    wayland_win_data_release(toplevel_data);

    if (!(list = build_hwnd_list(toplevel, TRUE, &count))) return;

    /* The list is in pre-order with siblings from the top. Reversing its
     * post-order gives the painting order. */
    if (!(stack = malloc(2 * (count + 1) * sizeof(*stack)))) goto done;
    order = stack + count + 1;
    for (i = 0; i < count; i++)
    {
        HWND parent = NtUserGetAncestor(list[i], GA_PARENT);

        while (depth && stack[depth - 1] != parent) order[done++] = stack[--depth];
        stack[depth++] = list[i];
    }
    while (depth) order[done++] = stack[--depth];

    if (!(rects = malloc((done + 1) * sizeof(*rects)))) goto done;
    if (!(covered = malloc((done + 1) * sizeof(*covered)))) goto done;
    dpi = NtUserGetWinMonitorDpi(toplevel, MDT_RAW_DPI);
    for (i = 0; i < done; i++) get_restack_rects(order[i], toplevel, dpi, &rects[i]);
    NtUserGetClientRect(toplevel, &toplevel_client, dpi);

    /* top level windows are listed from the top */
    if (!(popups = build_hwnd_list(0, FALSE, &popup_count))) goto done;

    if (!(toplevel_data = wayland_win_data_get(toplevel))) goto done;
    toplevel_data->restack_pending = FALSE;
    if (!(surface = toplevel_data->wayland_surface))
    {
        wayland_destroy_overlays(toplevel_data);
        wayland_win_data_release(toplevel_data);
        goto done;
    }

    wl_list_for_each(overlay, &toplevel_data->overlays, link) overlay->used = FALSE;
    SetRect(&bounds, 0, 0, surface->window.rect.right - surface->window.rect.left,
            surface->window.rect.bottom - surface->window.rect.top);
    SetRect(&overlaid, 0, 0, 0, 0);

    below = surface->wl_surface;
    if ((client = toplevel_data->client_surface) && client->toplevel == toplevel && client->wl_subsurface)
    {
        below = client->wl_surface;
        covered[covered_count++] = toplevel_client;
    }

    for (i = done; i--;)
    {
        if (!(data = wayland_win_data_get_nolock(order[i]))) continue;
        if ((client = data->client_surface))
        {
            if (client->toplevel != toplevel || !client->wl_subsurface) continue;
            wl_subsurface_place_above(client->wl_subsurface, below);
            below = client->wl_surface;
            covered[covered_count++] = rects[i].client;
            SetRect(&overlaid, 0, 0, 0, 0);
            continue;
        }

        /* only GDI windows painted over a client surface need an overlay */
        if (!rects[i].opaque || data->wayland_surface) continue;
        for (j = 0; j < covered_count; j++)
            if (intersect_rect(&rect, &rects[i].window, &covered[j])) break;
        if (j == covered_count) continue;
        /* the overlay of an enclosing window already shows it */
        if (rect_contains(&overlaid, &rects[i].window)) continue;

        rect = rects[i].window;
        rect.left += surface->window.client_rect.left - surface->window.rect.left;
        rect.right += surface->window.client_rect.left - surface->window.rect.left;
        rect.top += surface->window.client_rect.top - surface->window.rect.top;
        rect.bottom += surface->window.client_rect.top - surface->window.rect.top;
        if (!intersect_rect(&rect, &rect, &bounds)) continue;

        if (!(overlay = wayland_find_overlay(toplevel_data, order[i])) &&
            !(overlay = wayland_overlay_create(toplevel_data, order[i])))
            continue;
        wayland_overlay_set_rect(overlay, surface, &rect);
        overlay->used = TRUE;
        wl_subsurface_place_above(overlay->wl_subsurface, below);
        below = overlay->wl_surface;
        overlaid = rects[i].window;
    }

    wl_list_for_each_safe(overlay, next, &toplevel_data->overlays, link)
        if (!overlay->used) wayland_overlay_destroy(overlay);
    if (toplevel_data->window_contents)
        wayland_update_overlays(toplevel_data, toplevel_data->window_contents, NULL);

    /* owned popups are subsurfaces of the toplevel as well and must stay
     * above all of its client surfaces */
    for (i = popup_count; i--;)
    {
        if (!(data = wayland_win_data_get_nolock(popups[i])) || !(popup = data->wayland_surface)) continue;
        if (popup == surface || popup->role != WAYLAND_SURFACE_ROLE_SUBSURFACE) continue;
        if (popup->toplevel_hwnd != toplevel || !popup->wl_subsurface) continue;
        wl_subsurface_place_above(popup->wl_subsurface, below);
        below = popup->wl_surface;
    }

    wl_surface_commit(surface->wl_surface);
    wayland_win_data_release(toplevel_data);

done:
    free(popups);
    free(stack);
    free(list);
}

static void wayland_client_surface_attach_rect(struct wayland_client_surface *client, HWND toplevel,
                                               const RECT *rect)
{
    struct wayland_win_data *toplevel_data;
    struct wayland_surface *surface;
    HWND hwnd = client->client.hwnd;
    RECT client_rect;

    if (!toplevel)
    {
        if (client->wl_subsurface)
        {
            wl_subsurface_destroy(client->wl_subsurface);
            client->wl_subsurface = NULL;
        }

        client->toplevel = 0;
        return;
    }

    if (!(toplevel_data = wayland_win_data_get_nolock(toplevel)) || !(surface = toplevel_data->wayland_surface))
    {
        wayland_client_surface_attach(client, NULL);
        return;
    }

    if (client->toplevel != toplevel)
    {
        wayland_client_surface_attach(client, NULL);

        client->wl_subsurface =
            wl_subcompositor_get_subsurface(process_wayland.wl_subcompositor,
                                            client->wl_surface,
                                            surface->wl_surface);
        if (!client->wl_subsurface)
        {
            ERR("Failed to create client wl_subsurface\n");
            return;
        }
        /* Present contents independently of the parent surface. */
        wl_subsurface_set_desync(client->wl_subsurface);

        client->toplevel = toplevel;
    }

    if (rect) client_rect = *rect;
    else
    {
        NtUserGetClientRect(hwnd, &client_rect, NtUserGetWinMonitorDpi(hwnd, MDT_RAW_DPI));
        NtUserMapWindowPoints(hwnd, toplevel, (POINT *)&client_rect, 2, NtUserGetWinMonitorDpi(hwnd, MDT_RAW_DPI));
    }

    wayland_surface_reconfigure_client(surface, client, &client_rect);
    /* reconfiguring puts the client surface right above its parent, below
     * its siblings, restore the window stacking once the lock is released */
    toplevel_data->restack_pending = TRUE;
    /* Commit to apply subsurface positioning. */
    wl_surface_commit(surface->wl_surface);
}

void wayland_client_surface_attach(struct wayland_client_surface *client, HWND toplevel)
{
    wayland_client_surface_attach_rect(client, toplevel, NULL);
}

static void dummy_buffer_release(void *data, struct wl_buffer *buffer)
{
    struct wayland_shm_buffer *shm_buffer = data;
    TRACE("shm_buffer=%p\n", shm_buffer);
    wayland_shm_buffer_unref(shm_buffer);
}

static const struct wl_buffer_listener dummy_buffer_listener =
{
    dummy_buffer_release
};

/**********************************************************************
 *          wayland_surface_ensure_contents
 *
 * Ensure that the wayland surface has up-to-date contents, by committing
 * a dummy buffer if necessary.
 */
void wayland_surface_ensure_contents(struct wayland_surface *surface)
{
    struct wayland_shm_buffer *dummy_shm_buffer;
    HRGN damage;
    int width, height;
    BOOL needs_contents;

    width = surface->window.rect.right - surface->window.rect.left;
    height = surface->window.rect.bottom - surface->window.rect.top;
    needs_contents = surface->window.visible &&
                     (surface->content_width != width ||
                      surface->content_height != height);

    TRACE("surface=%p hwnd=%p needs_contents=%d\n",
          surface, surface->hwnd, needs_contents);

    if (!needs_contents) return;

    /* Create a transparent dummy buffer. */
    dummy_shm_buffer = wayland_shm_buffer_create(width, height, WL_SHM_FORMAT_ARGB8888);
    if (!dummy_shm_buffer)
    {
        ERR("Failed to create dummy buffer\n");
        return;
    }
    wl_buffer_add_listener(dummy_shm_buffer->wl_buffer, &dummy_buffer_listener,
                           dummy_shm_buffer);

    if (!(damage = NtGdiCreateRectRgn(0, 0, width, height)))
        WARN("Failed to create damage region for dummy buffer\n");

    if (wayland_surface_reconfigure(surface))
    {
        wayland_surface_attach_shm(surface, dummy_shm_buffer, damage);
        wl_surface_commit(surface->wl_surface);
    }
    else
    {
        wayland_shm_buffer_unref(dummy_shm_buffer);
    }

    if (damage) NtGdiDeleteObjectApp(damage);
}

/**********************************************************************
 *          wayland_surface_set_title
 */
void wayland_surface_set_title(struct wayland_surface *surface, LPCWSTR text)
{
    DWORD text_len;
    DWORD utf8_count;
    char *utf8 = NULL;

    assert(wayland_surface_is_toplevel(surface));

    TRACE("surface=%p hwnd=%p text='%s'\n",
          surface, surface->hwnd, wine_dbgstr_w(text));

    text_len = (lstrlenW(text) + 1) * sizeof(WCHAR);

    if (!RtlUnicodeToUTF8N(NULL, 0, &utf8_count, text, text_len) &&
        (utf8 = malloc(utf8_count)))
    {
        RtlUnicodeToUTF8N(utf8, utf8_count, &utf8_count, text, text_len);
        xdg_toplevel_set_title(surface->xdg_toplevel, utf8);
    }

    free(utf8);
}

/**********************************************************************
 *          wayland_surface_set_icon_buffer
 */
void wayland_surface_set_icon_buffer(struct wayland_surface *surface, UINT type, const ICONINFO *ii)
{
    struct wayland_shm_buffer *icon_buf;
    HDC hDC;

    if (!process_wayland.xdg_toplevel_icon_manager_v1) return;

    assert(ii);

    TRACE("surface=%p type=%x ii=%p\n", surface, type, ii);

    hDC = NtGdiCreateCompatibleDC(0);
    icon_buf = wayland_shm_buffer_from_color_bitmaps(hDC, ii->hbmColor, ii->hbmMask);
    NtGdiDeleteObjectApp(hDC);

    if (surface->big_icon_buffer && type == ICON_BIG)
    {
        wayland_shm_buffer_unref(surface->big_icon_buffer);
        surface->big_icon_buffer = NULL;
    }
    else if (surface->small_icon_buffer && type != ICON_BIG)
    {
        wayland_shm_buffer_unref(surface->small_icon_buffer);
        surface->small_icon_buffer = NULL;
    }

    if (icon_buf)
    {
        if (type == ICON_BIG) surface->big_icon_buffer = icon_buf;
        else surface->small_icon_buffer = icon_buf;
    }
}

/**********************************************************************
 *          wayland_surface_assign_icon
 */
void wayland_surface_assign_icon(struct wayland_surface *surface)
{
    if (!process_wayland.xdg_toplevel_icon_manager_v1) return;

    assert(wayland_surface_is_toplevel(surface));

    TRACE("surface=%p\n", surface);

    if (surface->xdg_toplevel_icon)
    {
        xdg_toplevel_icon_manager_v1_set_icon(process_wayland.xdg_toplevel_icon_manager_v1,
                                              surface->xdg_toplevel, NULL);
        xdg_toplevel_icon_v1_destroy(surface->xdg_toplevel_icon);
        surface->xdg_toplevel_icon = NULL;
    }

    if (surface->big_icon_buffer)
    {
        surface->xdg_toplevel_icon =
            xdg_toplevel_icon_manager_v1_create_icon(process_wayland.xdg_toplevel_icon_manager_v1);

        /* FIXME: what to do with scale ? */
        xdg_toplevel_icon_v1_add_buffer(surface->xdg_toplevel_icon,
                                        surface->big_icon_buffer->wl_buffer, 1);
        if (surface->small_icon_buffer)
        {
            xdg_toplevel_icon_v1_add_buffer(surface->xdg_toplevel_icon,
                                            surface->small_icon_buffer->wl_buffer, 1);
        }

        xdg_toplevel_icon_v1_set_name(surface->xdg_toplevel_icon, "");

        xdg_toplevel_icon_manager_v1_set_icon(process_wayland.xdg_toplevel_icon_manager_v1,
                                              surface->xdg_toplevel, surface->xdg_toplevel_icon);
    }
}
