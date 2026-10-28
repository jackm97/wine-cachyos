/*
 * Wayland surfaces
 *
 * Copyright 2020 Alexandros Frantzis for Collabora Ltd
 * Copyright 2025 Etaash Mathamsetty
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

#include "waylanddrv.h"
#include "wine/debug.h"
#include "wine/server.h"

WINE_DEFAULT_DEBUG_CHANNEL(waylanddrv);

static struct wp_image_description_v1 *scrgb_desc;
static struct wp_image_description_v1 *bt2100_desc;
static BOOL color_manager_initialized;

static void image_description_failed(void *user_data, struct wp_image_description_v1 *desc,
                                     uint32_t cause, const char *message)
{
    struct wp_image_description_v1 **ret = user_data;

    ERR("cause=%u message=%s\n", cause, debugstr_a(message));

    *ret = NULL;
    wp_image_description_v1_destroy(desc);
}

static void image_description_ready2(void *user_data, struct wp_image_description_v1 *desc,
                                     uint32_t identity_hi, uint32_t identity_lo)
{
    struct wp_image_description_v1 **ret = user_data;

    TRACE("id=%#x%x\n", identity_hi, identity_lo);

    *ret = desc;
}

static void image_description_ready(void *user_data, struct wp_image_description_v1 *desc, uint32_t identity)
{
    image_description_ready2(user_data, desc, 0, identity);
}

static const struct wp_image_description_v1_listener image_description_listener =
{
    image_description_failed,
    image_description_ready,
    image_description_ready2,
};

static void color_manager_supported_intent(void *data, struct wp_color_manager_v1 *manager, uint32_t intent)
{
}

static void color_manager_supported_feature(void *data, struct wp_color_manager_v1 *manager, uint32_t feature)
{
    struct wp_image_description_v1 *desc;

    TRACE("feature %u\n", feature);

    if (feature == WP_COLOR_MANAGER_V1_FEATURE_WINDOWS_SCRGB && (desc = wp_color_manager_v1_create_windows_scrgb(manager)))
    {
        scrgb_desc = (void *)-1;
        wp_image_description_v1_add_listener(desc, &image_description_listener, &scrgb_desc);
    }
    if (feature == WP_COLOR_MANAGER_V1_FEATURE_WINDOWS_BT2100 && (desc = wp_color_manager_v1_create_windows_bt2100(manager)))
    {
        bt2100_desc = (void *)-1;
        wp_image_description_v1_add_listener(desc, &image_description_listener, &bt2100_desc);
    }
}

static void color_manager_supported_named_tf(void *data, struct wp_color_manager_v1 *manager, uint32_t tf)
{
}

static void color_manager_supported_primaries(void *data, struct wp_color_manager_v1 *manager, uint32_t primaries)
{
}

static void color_manager_done(void *data, struct wp_color_manager_v1 *manager)
{
    color_manager_initialized = TRUE;
}

const struct wp_color_manager_v1_listener color_manager_listener =
{
    color_manager_supported_intent,
    color_manager_supported_feature,
    color_manager_supported_named_tf,
    color_manager_supported_primaries,
    color_manager_done,
};

void color_management_init(void)
{
    while (!color_manager_initialized || scrgb_desc == (void *)-1 || bt2100_desc == (void *)-1)
        wl_display_roundtrip_queue(process_wayland.wl_display, process_wayland.wl_event_queue);

    TRACE("Initialized image descriptors, scRGB %p BT.2100 %p\n", scrgb_desc, bt2100_desc);
}

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
    HWND child, root = 0;

    if (!hwnd) return;
    root = NtUserGetAncestor(hwnd, GA_ROOT);

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

static void xdg_exported_v2_handle_handle(void *private, struct zxdg_exported_v2 *exported,
                                          const char *handle)
{
    HWND hwnd = private;
    struct wayland_win_data *data;
    WCHAR handleW[MAX_ATOM_LEN + 1];
    RTL_ATOM atom, old_atom;
    size_t i, len = strlen(handle);
    BOOL owned = FALSE;

    if (len > MAX_ATOM_LEN) return;
    for (i = 0; i < len; i++) handleW[i] = (unsigned char)handle[i];
    handleW[len] = 0;
    if (NtAddAtom(handleW, len * sizeof(WCHAR), &atom)) return;

    if (!(data = wayland_win_data_get(hwnd)))
    {
        NtDeleteAtom(atom);
        return;
    }
    if (!data->wayland_surface || data->wayland_surface->zxdg_exported_v2 != exported)
    {
        wayland_win_data_release(data);
        NtDeleteAtom(atom);
        return;
    }
    old_atom = data->wayland_surface->exported_handle_atom;
    data->wayland_surface->exported_handle_atom = atom;
    wayland_win_data_release(data);

    /* the NtUser calls run outside the win_data lock */
    NtUserRemoveProp(hwnd, xdg_foreign_handle_prop);
    if (!NtUserSetProp(hwnd, xdg_foreign_handle_prop, ULongToHandle(atom)))
    {
        /* clear_role may already have taken the atom, so delete it only if it is still ours */
        if ((data = wayland_win_data_get(hwnd)))
        {
            if (data->wayland_surface && data->wayland_surface->exported_handle_atom == atom)
            {
                data->wayland_surface->exported_handle_atom = 0;
                owned = TRUE;
            }
            wayland_win_data_release(data);
        }
        if (owned) NtDeleteAtom(atom);
    }
    else if (!(data = wayland_win_data_get(hwnd)) ||
             !data->wayland_surface || data->wayland_surface->exported_handle_atom != atom)
    {
        /* cleared meanwhile: whoever cleared the field deleted the atom, so only the prop is ours to remove */
        if (data) wayland_win_data_release(data);
        if (HandleToULong(NtUserGetProp(hwnd, xdg_foreign_handle_prop)) == atom)
            NtUserRemoveProp(hwnd, xdg_foreign_handle_prop);
    }
    else wayland_win_data_release(data);
    if (old_atom) NtDeleteAtom(old_atom);
    TRACE("exported hwnd=%p handle=%s\n", hwnd, handle);
}

static const struct zxdg_exported_v2_listener xdg_exported_v2_listener =
{
    xdg_exported_v2_handle_handle,
};

/***********************************************************************
 *           wayland_surface_export_locked
 *
 * Exports a toplevel that is a root window. Called with the win_data lock held
 * (D1: make_toplevel runs under it). The guard stops a second export, so repeated
 * make_toplevel calls export once.
 */
static void wayland_surface_export_locked(struct wayland_surface *surface)
{
    if (!process_wayland.zxdg_exporter_v2 || surface->zxdg_exported_v2 ||
        !wayland_surface_is_toplevel(surface) ||
        NtUserGetAncestor(surface->hwnd, GA_ROOT) != surface->hwnd)
        return;

    surface->zxdg_exported_v2 =
        zxdg_exporter_v2_export_toplevel(process_wayland.zxdg_exporter_v2, surface->wl_surface);
    if (surface->zxdg_exported_v2)
        zxdg_exported_v2_add_listener(surface->zxdg_exported_v2,
                                      &xdg_exported_v2_listener, surface->hwnd);
}

static void xdg_imported_v2_handle_destroyed(void *private, struct zxdg_imported_v2 *imported)
{
    HWND hwnd = private, parent;
    struct wayland_win_data *data;
    struct wayland_surface *surface;

    if (!(data = wayland_win_data_get(hwnd))) return;
    if (!(surface = data->wayland_surface) || surface->zxdg_imported_v2 != imported)
    {
        wayland_win_data_release(data);
        return;
    }
    parent = surface->foreign_parent_hwnd;
    zxdg_imported_v2_destroy(imported);
    surface->zxdg_imported_v2 = NULL;
    surface->imported_handle_atom = 0;
    surface->foreign_parent_hwnd = NULL;
    surface->foreign_activation_pending = FALSE;
    wayland_win_data_release(data);

    /* the NtUser calls run outside the win_data lock */
    if (parent && NtUserGetProp(parent, xdg_foreign_child_prop) == hwnd)
        NtUserRemoveProp(parent, xdg_foreign_child_prop);
}

static const struct zxdg_imported_v2_listener xdg_imported_v2_listener =
{
    xdg_imported_v2_handle_destroyed,
};

/* Activation frame callback, on the child's own wayland_surface (H8). Its commits come from the child's own surface (D3). */
static void foreign_activation_frame_done(void *private, struct wl_callback *callback,
                                          uint32_t callback_data)
{
    HWND hwnd = private;
    struct wayland_win_data *data;
    struct wayland_surface *surface;

    /* a miss means the window's win_data is gone; clear_role (H13) has already destroyed the callback */
    if (!(data = wayland_win_data_get(hwnd))) return;
    if (!(surface = data->wayland_surface) || surface->foreign_activation_frame_callback != callback)
    {
        wayland_win_data_release(data);
        return;
    }
    wl_callback_destroy(callback);
    surface->foreign_activation_frame_callback = NULL;

    if (surface->foreign_activation_pending)
    {
        surface->foreign_activation_pending = FALSE;
        wayland_request_surface_activation(surface);
    }
    wayland_win_data_release(data);
}

static const struct wl_callback_listener foreign_activation_frame_listener =
{
    foreign_activation_frame_done,
};

/***********************************************************************
 *           update_foreign_parent_locked
 *
 * Called with the win_data lock held. Only Wayland and atom work happens here.
 * Returns TRUE when it newly imported the parent; the caller then sets
 * xdg_foreign_child_prop after releasing the lock. *old_parent is set to the
 * parent the surface was imported from before a re-import, or NULL, and the
 * caller drops its child prop on that parent.
 */
static BOOL update_foreign_parent_locked(struct wayland_surface *surface, HWND parent, RTL_ATOM atom,
                                         HWND *old_parent)
{
    char handle[MAX_ATOM_LEN + 1];

    *old_parent = NULL;
    if (!process_wayland.zxdg_importer_v2 || !wayland_surface_is_toplevel(surface)) return FALSE;
    if (surface->zxdg_imported_v2 && surface->imported_handle_atom == atom) return FALSE;

    if (surface->zxdg_imported_v2)
    {
        /* the caller drops this parent's child prop */
        *old_parent = surface->foreign_parent_hwnd;
        zxdg_imported_v2_destroy(surface->zxdg_imported_v2);
        surface->zxdg_imported_v2 = NULL;
        surface->imported_handle_atom = 0;
        surface->foreign_parent_hwnd = NULL;
    }

    if (!atom_get_ascii(atom, handle, sizeof(handle))) return FALSE;

    surface->zxdg_imported_v2 =
        zxdg_importer_v2_import_toplevel(process_wayland.zxdg_importer_v2, handle);
    if (!surface->zxdg_imported_v2) return FALSE;

    zxdg_imported_v2_add_listener(surface->zxdg_imported_v2,
                                  &xdg_imported_v2_listener, surface->hwnd);
    zxdg_imported_v2_set_parent_of(surface->zxdg_imported_v2, surface->wl_surface);
    surface->imported_handle_atom = atom;
    surface->foreign_parent_hwnd = parent;
    surface->foreign_activation_pending = TRUE;
    wl_display_flush(process_wayland.wl_display);
    TRACE("imported parent hwnd=%p for hwnd=%p handle=%s\n",
          parent, surface->hwnd, handle);
    return TRUE;
}

/***********************************************************************
 *           wayland_surface_update_foreign_parent
 *
 * Called with no lock held, since the NtUser queries take the user lock.
 * Under the lock it imports a newly exported parent, then schedules the
 * activation frame callback on the child's own wayland_surface, whose
 * commits come from the child's own surface (D3).
 */
void wayland_surface_update_foreign_parent(HWND hwnd)
{
    struct wayland_win_data *data;
    struct wayland_surface *surface;
    HWND parent, old_parent = NULL;
    RTL_ATOM atom;
    BOOL imported = FALSE;

    if (!process_wayland.zxdg_importer_v2) return;
    if (!(parent = NtUserGetAncestor(hwnd, GA_ROOT)) || parent == hwnd) return;
    if (!(atom = HandleToULong(NtUserGetProp(parent, xdg_foreign_handle_prop)))) return;

    if (!(data = wayland_win_data_get(hwnd))) return;
    if ((surface = data->wayland_surface))
    {
        imported = update_foreign_parent_locked(surface, parent, atom, &old_parent);
        if (surface->foreign_activation_pending && !surface->foreign_activation_frame_callback &&
            (surface->foreign_activation_frame_callback = wl_surface_frame(surface->wl_surface)))
            wl_callback_add_listener(surface->foreign_activation_frame_callback,
                                     &foreign_activation_frame_listener, surface->hwnd);
    }
    wayland_win_data_release(data);

    /* outside the lock, like the SetProp below */
    if (old_parent && NtUserGetProp(old_parent, xdg_foreign_child_prop) == hwnd)
        NtUserRemoveProp(old_parent, xdg_foreign_child_prop);
    /* a failed SetProp keeps the import, with no retry (accepted, see the failed child-prop bullet) */
    if (imported && NtUserSetProp(parent, xdg_foreign_child_prop, hwnd))
    {
        /* the import may have been dropped meanwhile; keep the prop only if this surface still imports this parent */
        if (!(data = wayland_win_data_get(hwnd)) ||
            !data->wayland_surface || !data->wayland_surface->zxdg_imported_v2 ||
            data->wayland_surface->foreign_parent_hwnd != parent)
        {
            if (data) wayland_win_data_release(data);
            if (NtUserGetProp(parent, xdg_foreign_child_prop) == hwnd)
                NtUserRemoveProp(parent, xdg_foreign_child_prop);
        }
        else wayland_win_data_release(data);
    }
}

static void xdg_surface_handle_configure(void *private, struct xdg_surface *xdg_surface,
                                         uint32_t serial)
{
    struct wayland_surface *surface;
    BOOL should_post = FALSE, should_expose = FALSE;
    struct wayland_win_data *data;
    HWND hwnd = private;

    TRACE("serial=%u\n", serial);

    if (!(data = wayland_win_data_get(hwnd))) return;

    /* Handle this event only if wayland_surface is still associated with
     * the target xdg_surface. */
    if (!(surface = data->wayland_surface) || surface->xdg_surface != xdg_surface)
    {
        wayland_win_data_release(data);
        return;
    }

    if (wayland_surface_is_toplevel(surface))
    {
        /* If we have a previously requested config, we have already sent a
         * WM_WAYLAND_CONFIGURE which hasn't been handled yet. In that case,
         * avoid sending another message to reduce message queue traffic. */
        should_post = surface->requested.serial == 0;
        should_expose = surface->current.serial == 0;
        surface->pending.serial = serial;
        surface->requested = surface->pending;
        memset(&surface->pending, 0, sizeof(surface->pending));
    }
    else if (wayland_surface_is_popup(surface))
    {
        /* expose the surface to ensure that the new config is ack-ed
         * and the popup can move if needed */
        should_expose = surface->current.serial == 0 ||
                        !EqualRect(&surface->current.rect, &surface->pending.rect);
        surface->pending.serial = serial;
        surface->processing = surface->pending;
        surface->processing.processed = 1;
        memset(&surface->pending, 0, sizeof(surface->pending));
    }

    wayland_win_data_release(data);

    if (should_post) NtUserPostMessage(hwnd, WM_WAYLAND_CONFIGURE, 0, 0);

    /* Flush the window surface in case there is content that we weren't
     * able to flush before due to the lack of the initial configure. */
    if (should_expose) NtUserExposeWindowSurface(hwnd, 0, NULL);
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
        SetRect(&surface->pending.rect, 0, 0, width, height);
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

static void xdg_popup_handle_configure(void *private, struct xdg_popup *xdg_popup,
                                       int32_t x, int32_t y, int32_t width, int32_t height)
{
    HWND hwnd = private;
    struct wayland_win_data *data;
    struct wayland_surface *surface;

    TRACE("hwnd=%p (%d,%d) %dx%d\n", hwnd, x, y, width, height);

    if (!(data = wayland_win_data_get(hwnd))) return;

    if ((surface = data->wayland_surface) && wayland_surface_is_popup(surface))
    {
        SetRect(&surface->pending.rect, x, y, x + width, y + height);
        surface->pending.state = 0;
    }

    wayland_win_data_release(data);
}

static void xdg_popup_handle_done(void *private, struct xdg_popup *xdg_popup)
{
    struct wayland_win_data *data, *owner_data = NULL;
    struct wayland_surface *surface = NULL, *owner_surface = NULL;
    HWND hwnd = private;

    if (!xdg_popup) return;

    /* Recreate the popup if the compositor dismissed it for some reason.
     * The protocol does not explicitly prohibit this from occuring on ungrabbed popups. */
    WARN("Compositor dismissed popup hwnd=%p\n", private);

    if (!(data = wayland_win_data_get(hwnd)) || !(surface = data->wayland_surface))
    {
        ERR("Failed to recreate popup!\n");
        xdg_popup_destroy(xdg_popup);
        goto done;
    }

    /* this is a stale event */
    if (!wayland_surface_is_popup(surface)) goto done;
    if (surface->xdg_popup != xdg_popup) goto done;

    /* the protocol requires us to destroy the xdg_popup */
    wayland_surface_clear_role(surface);

    /* win_data_mutex is recursive, so taking the owner's data while holding ours is fine */
    if (!(owner_data = wayland_win_data_get(surface->owner_hwnd)) ||
        !(owner_surface = owner_data->wayland_surface) || !owner_surface->xdg_surface)
    {
        ERR("Failed to recreate popup!\n");
        goto done;
    }

    wayland_surface_make_popup(surface, owner_surface);

done:
    if (owner_data) wayland_win_data_release(owner_data);
    if (data) wayland_win_data_release(data);
    if (surface && owner_surface && owner_surface->xdg_surface)
        NtUserExposeWindowSurface(hwnd, 0, NULL);
}

static void xdg_popup_handle_reposition(void *private, struct xdg_popup *xdg_popup, uint32_t token)
{
    /* we also get a configure event in this case */
    TRACE("hwnd=%p\n", private);
}

static const struct xdg_popup_listener xdg_popup_listener =
{
    xdg_popup_handle_configure,
    xdg_popup_handle_done,
    xdg_popup_handle_reposition,
};

void wp_fractional_scale_handle_scale(void* user_data,
                                      struct wp_fractional_scale_v1 *fractional_scale_v1,
                                      uint32_t scale_fixed)
{
    struct wayland_win_data *data, *owner_data = NULL;
    struct wayland_surface *surface;
    double scale = scale_fixed / 120.0;
    HWND hwnd = user_data;

    TRACE("hwnd=%p scale=%lf\n", hwnd, scale);

    if (!(data = wayland_win_data_get(hwnd))) return;
    if (!(surface = data->wayland_surface) || scale == surface->window.scale)
    {
        wayland_win_data_release(data);
        return;
    }

    surface->window.scale = scale;

    /* reattach client surfaces as their rects have changed */
    update_client_surfaces(hwnd);

    /* the popup x,y position has changed */
    if (wayland_surface_is_popup(surface) &&
       (owner_data = wayland_win_data_get(surface->owner_hwnd)) &&
        owner_data->wayland_surface)
    {
        wayland_surface_make_popup(surface, owner_data->wayland_surface);
    }
    if (owner_data) wayland_win_data_release(owner_data);

    wayland_win_data_release(data);

    NtUserExposeWindowSurface(hwnd, 0, NULL);
}

static const struct wp_fractional_scale_v1_listener wp_fractional_scale_listener =
{
    wp_fractional_scale_handle_scale
};

/**********************************************************************
 *          wayland_surface_create
 *
 * Creates a role-less wayland surface.
 */
static LONG64 surface_serial;

struct wayland_surface *wayland_surface_create(HWND hwnd)
{
    struct wayland_surface *surface;

    surface = calloc(1, sizeof(*surface));
    if (!surface)
    {
        ERR("Failed to allocate space for Wayland surface\n");
        goto err;
    }

    surface->serial = InterlockedIncrement64(&surface_serial);
    TRACE("surface=%p serial=%s\n", surface, wine_dbgstr_longlong(surface->serial));

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
    if (process_wayland.wp_alpha_modifier_v1)
    {
        COLORREF key;
        DWORD flags;
        BYTE alpha;

        surface->wp_alpha_modifier_surface_v1 =
            wp_alpha_modifier_v1_get_surface(process_wayland.wp_alpha_modifier_v1, surface->wl_surface);

        if (!NtUserGetLayeredWindowAttributes(hwnd, &key, &alpha, &flags)) flags = 0;
        wayland_surface_set_opacity(surface, alpha, flags);
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

    if (surface->wp_alpha_modifier_surface_v1)
    {
        wp_alpha_modifier_surface_v1_destroy(surface->wp_alpha_modifier_surface_v1);
        surface->wp_alpha_modifier_surface_v1 = NULL;
    }

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

static void wayland_surface_init_fractional_scale(struct wayland_surface *surface,
                                                  double initial_scale)
{
    surface->window.scale = initial_scale;

    if (!process_wayland.wp_fractional_scale_manager_v1) return;

    surface->wp_fractional_scale_v1 =
        wp_fractional_scale_manager_v1_get_fractional_scale(
            process_wayland.wp_fractional_scale_manager_v1,
            surface->wl_surface);
    if (!surface->wp_fractional_scale_v1)
    {
        ERR("Failed to create wp_fractional_scale_v1\n");
        return;
    }
    wp_fractional_scale_v1_add_listener(
        surface->wp_fractional_scale_v1,
        &wp_fractional_scale_listener,
        surface->hwnd);
}

/**********************************************************************
 *          wayland_surface_make_toplevel
 *
 * Gives the toplevel role to a plain wayland surface.
 */
void wayland_surface_make_toplevel(struct wayland_surface *surface)
{
    static char steam_proton[] = "steam_proton";
    const char *app_id = getenv("SteamAppId");
    char proton_app_class[128];
    WCHAR text[1024];

    TRACE("surface=%p\n", surface);

    assert(!surface->role || surface->role == WAYLAND_SURFACE_ROLE_TOPLEVEL);
    if (surface->xdg_surface && surface->xdg_toplevel)
    {
        surface->owner_hwnd = NtUserGetWindowRelative(surface->hwnd, GW_OWNER);
        wayland_surface_export_locked(surface);
        return;
    }

    wayland_surface_clear_role(surface);
    surface->role = WAYLAND_SURFACE_ROLE_TOPLEVEL;
    surface->owner_hwnd = NtUserGetWindowRelative(surface->hwnd, GW_OWNER);

    surface->xdg_surface =
        xdg_wm_base_get_xdg_surface(process_wayland.xdg_wm_base, surface->wl_surface);
    if (!surface->xdg_surface) goto err;
    xdg_surface_add_listener(surface->xdg_surface, &xdg_surface_listener, surface->hwnd);

    surface->xdg_toplevel = xdg_surface_get_toplevel(surface->xdg_surface);
    if (!surface->xdg_toplevel) goto err;
    xdg_toplevel_add_listener(surface->xdg_toplevel, &xdg_toplevel_listener, surface->hwnd);

    if(!app_id || !*app_id) {
        app_id = getenv("WINE_WMCLASS");
    }

    if (app_id && *app_id) {
        snprintf(proton_app_class, sizeof(proton_app_class), "steam_app_%s", app_id);
        xdg_toplevel_set_app_id(surface->xdg_toplevel, proton_app_class);
    } else {
        xdg_toplevel_set_app_id(surface->xdg_toplevel, steam_proton);
    }

    if (!NtUserInternalGetWindowText(surface->hwnd, text, ARRAY_SIZE(text)))
        text[0] = 0;
    wayland_surface_set_title(surface, text);

    wayland_surface_assign_icon(surface);

    if (process_wayland.zxdg_exporter_v2 &&
        NtUserGetAncestor(surface->hwnd, GA_ROOT) == surface->hwnd)
    {
        wayland_surface_export_locked(surface);
    }
    else if (process_wayland.zxdg_importer_v2)
    {
        HWND parent = NtUserGetAncestor(surface->hwnd, GA_ROOT), old_parent = NULL;
        BOOL imported;
        RTL_ATOM atom = parent && parent != surface->hwnd ?
                        HandleToULong(NtUserGetProp(parent, xdg_foreign_handle_prop)) : 0;

        imported = atom && update_foreign_parent_locked(surface, parent, atom, &old_parent);

        /* under the lock, like the SetProp below */
        if (old_parent && NtUserGetProp(old_parent, xdg_foreign_child_prop) == surface->hwnd)
            NtUserRemoveProp(old_parent, xdg_foreign_child_prop);
        /* a failed SetProp keeps the import, with no retry (accepted, see the failed child-prop bullet) */
        if (imported) NtUserSetProp(parent, xdg_foreign_child_prop, surface->hwnd);
        if (surface->foreign_activation_pending && !surface->foreign_activation_frame_callback &&
            (surface->foreign_activation_frame_callback = wl_surface_frame(surface->wl_surface)))
            wl_callback_add_listener(surface->foreign_activation_frame_callback,
                                     &foreign_activation_frame_listener, surface->hwnd);
    }

    wayland_surface_init_fractional_scale(surface, 1.0);

    wl_surface_commit(surface->wl_surface);
    wl_display_flush(process_wayland.wl_display);

    return;

err:
    wayland_surface_clear_role(surface);
    ERR("Failed to assign toplevel role to wayland surface\n");
}

/* helper to intialize the positioner using a given surface config */
static struct xdg_positioner *wayland_surface_create_positioner(struct wayland_surface *surface,
                                                                const RECT *rect)
{
    int width, height;
    struct xdg_positioner *xdg_positioner =
        xdg_wm_base_create_positioner(process_wayland.xdg_wm_base);

    if (!xdg_positioner) return NULL;

    width = max(1, rect->right - rect->left);
    height = max(1, rect->bottom - rect->top);

    /* this anchor rect is always valid, then we offset by the requested amount */
    xdg_positioner_set_anchor_rect(xdg_positioner, 0, 0, 1, 1);
    xdg_positioner_set_anchor(xdg_positioner, XDG_POSITIONER_ANCHOR_TOP_LEFT);
    xdg_positioner_set_gravity(xdg_positioner, XDG_POSITIONER_GRAVITY_BOTTOM_RIGHT);
    xdg_positioner_set_offset(xdg_positioner, rect->left, rect->top);
    xdg_positioner_set_size(xdg_positioner, width, height);

    return xdg_positioner;
}

/**********************************************************************
 *          wayland_surface_make_popup
 *
 * Gives the popup role to a plain wayland surface.
 */
void wayland_surface_make_popup(struct wayland_surface *surface,
                                struct wayland_surface *owner)
{
    struct xdg_positioner *xdg_positioner = NULL;
    RECT rect = surface->window.rect;

    OffsetRect(&rect, -owner->window.rect.left, -owner->window.rect.top);

    assert(owner->xdg_surface);
    assert(!surface->role || surface->role == WAYLAND_SURFACE_ROLE_POPUP);

    if (surface->xdg_popup && surface->owner_hwnd == owner->hwnd)
    {
        if (!surface->current.serial) return;

        rect = map_rect_to_surface(surface, rect);

        /* reposition the popup if needed */
        if (!EqualRect(&surface->current.rect, &rect)) return;

        xdg_positioner = wayland_surface_create_positioner(surface, &rect);
        if (!xdg_positioner)
        {
            ERR("Failed to create positioner!\n");
            return;
        }

        xdg_popup_reposition(surface->xdg_popup, xdg_positioner, 0);
        xdg_positioner_destroy(xdg_positioner);
        wl_surface_commit(surface->wl_surface);
        wl_display_flush(process_wayland.wl_display);
        return;
    }

    wayland_surface_clear_role(surface);
    surface->role = WAYLAND_SURFACE_ROLE_POPUP;

    wayland_surface_init_fractional_scale(surface, owner->window.scale);
    rect = map_rect_to_surface(surface, rect);

    surface->xdg_surface = xdg_wm_base_get_xdg_surface(process_wayland.xdg_wm_base,
                                                       surface->wl_surface);
    if (!surface->xdg_surface) goto err;
    xdg_surface_add_listener(surface->xdg_surface, &xdg_surface_listener, surface->hwnd);

    xdg_positioner = wayland_surface_create_positioner(surface, &rect);
    if (!xdg_positioner) goto err;

    surface->xdg_popup = xdg_surface_get_popup(surface->xdg_surface, owner->xdg_surface,
                                               xdg_positioner);
    xdg_positioner_destroy(xdg_positioner);
    if (!surface->xdg_popup) goto err;
    xdg_popup_add_listener(surface->xdg_popup, &xdg_popup_listener, surface->hwnd);

    surface->owner_hwnd = owner->hwnd;
    wl_surface_commit(surface->wl_surface);
    wl_display_flush(process_wayland.wl_display);

    return;
err:
    wayland_surface_clear_role(surface);
    ERR("Failed to assign popup role to wayland surface\n");
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

    /* the frame callback is on this surface (H8); destroying it is a Wayland call only */
    if (surface->foreign_activation_frame_callback)
    {
        wl_callback_destroy(surface->foreign_activation_frame_callback);
        surface->foreign_activation_frame_callback = NULL;
    }

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

    /* the token prop is taken by whichever side removes it first, so only that side deletes the atom.
     * Gated on TOPLEVEL: only toplevel children receive tokens, and the role is still set here, before the switch */
    if (surface->role == WAYLAND_SURFACE_ROLE_TOPLEVEL)
    {
        RTL_ATOM token_atom = HandleToULong(NtUserRemoveProp(surface->hwnd, xdg_activation_token_prop));

        if (token_atom) NtDeleteAtom(token_atom);
    }

    /* some objects are shared between several roles */

    if (surface->wp_fractional_scale_v1)
    {
        wp_fractional_scale_v1_destroy(surface->wp_fractional_scale_v1);
        surface->wp_fractional_scale_v1 = NULL;
    }

    switch (surface->role)
    {
    case WAYLAND_SURFACE_ROLE_NONE:
        break;

    case WAYLAND_SURFACE_ROLE_POPUP:

        if (surface->xdg_popup)
        {
            xdg_popup_destroy(surface->xdg_popup);
            surface->xdg_popup = NULL;
        }

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

        surface->requested_output = NULL;
        break;
    }

    if (surface->xdg_surface)
    {
        xdg_surface_destroy(surface->xdg_surface);
        surface->xdg_surface = NULL;
    }

    surface->owner_hwnd = NULL;

    memset(&surface->pending, 0, sizeof(surface->pending));
    memset(&surface->requested, 0, sizeof(surface->requested));
    memset(&surface->processing, 0, sizeof(surface->processing));
    memset(&surface->current, 0, sizeof(surface->current));

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
BOOL wayland_surface_config_is_compatible(struct wayland_surface_config *conf, RECT rect,
                                          enum wayland_surface_config_state state)
{
    static enum wayland_surface_config_state mask =
        WAYLAND_SURFACE_CONFIG_STATE_MAXIMIZED;

    /* The fullscreen state requires a size smaller or equal to the configured
     * size. If we have a larger size, we can use surface geometry during
     * surface reconfiguration to provide the smaller size, so we are always
     * compatible with a fullscreen state.
     * NOTE: Fullscreen combined with maximized is the same as fullscreen. */
    if (conf->state & WAYLAND_SURFACE_CONFIG_STATE_FULLSCREEN)
        return TRUE;

    /* We require the same state. */
    if ((state & mask) != (conf->state & mask)) return FALSE;

    /* The maximized state requires the configured size. During surface
     * reconfiguration we can use surface geometry to provide smaller areas
     * from larger sizes, so only smaller sizes are incompatible. */
    if ((conf->state & WAYLAND_SURFACE_CONFIG_STATE_MAXIMIZED) &&
        (rect.right - rect.left < conf->rect.right - conf->rect.left ||
         rect.bottom - rect.top < conf->rect.bottom - conf->rect.top))
    {
        return FALSE;
    }

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

void wayland_surface_update_toplevel_parent(struct wayland_surface *surface)
{
    struct wayland_win_data *owner_data;
    struct wayland_surface *owner_surface = NULL;
    struct xdg_toplevel *owner_xdg_toplevel = NULL;

    if (!wayland_surface_is_toplevel(surface)) return;

    surface->owner_hwnd = NtUserGetWindowRelative(surface->hwnd, GW_OWNER);

    TRACE("hwnd=%p owner=%p\n", surface->hwnd, surface->owner_hwnd);

    if (surface->owner_hwnd && (owner_data = wayland_win_data_get(surface->owner_hwnd)))
    {
        if ((owner_surface = owner_data->wayland_surface) &&
            wayland_surface_is_toplevel(owner_surface))
            owner_xdg_toplevel = owner_surface->xdg_toplevel;

        wayland_win_data_release(owner_data);
    }

    xdg_toplevel_set_parent(surface->xdg_toplevel, owner_xdg_toplevel);
}

/**********************************************************************
 *          wayland_surface_reconfigure_geometry
 *
 * Sets the xdg_surface geometry
 */
static void wayland_surface_reconfigure_geometry(struct wayland_surface *surface, RECT rect)
{
    const RECT *current = &surface->current.rect;
    /* If the window size is bigger than the current state accepts, use the
     * largest visible (from Windows' perspective) subregion of the window. */
    if ((surface->current.state & (WAYLAND_SURFACE_CONFIG_STATE_MAXIMIZED |
                                   WAYLAND_SURFACE_CONFIG_STATE_FULLSCREEN)) &&
        (rect.right - rect.left > current->right - current->left ||
         rect.bottom - rect.top > current->bottom - current->top))
    {
        wayland_surface_get_rect_in_monitor(surface, &rect);

        rect = map_rect_to_surface(surface, rect);

        /* If the window rect in the monitor is smaller than required,
         * fall back to an appropriately sized rect at the top-left. */
        if ((surface->current.state & WAYLAND_SURFACE_CONFIG_STATE_MAXIMIZED) &&
            !(surface->current.state & WAYLAND_SURFACE_CONFIG_STATE_FULLSCREEN) &&
            (rect.right - rect.left < current->right - current->left ||
             rect.bottom - rect.top < current->bottom - current->top))
        {
            SetRect(&rect, 0, 0, current->right - current->left, current->bottom - current->top);
        }
        else
        {
            rect.right = min(rect.right, rect.left + current->right - current->left);
            rect.bottom = min(rect.bottom, rect.top + current->bottom - current->top);
        }
        TRACE("Window is too large for Wayland state, using subregion\n");
    }
    else
    {
        OffsetRect(&rect, -rect.left, -rect.top);
    }

    TRACE("hwnd=%p geometry=%s\n", surface->hwnd, wine_dbgstr_rect(&rect));

    if (!IsRectEmpty(&rect))
    {
        int width = rect.right - rect.left, height = rect.bottom - rect.top;
        xdg_surface_set_window_geometry(surface->xdg_surface, 0, 0, width, height);

        if (!wayland_surface_is_toplevel(surface)) return;

        if (surface->window.resizeable)
        {
            xdg_toplevel_set_min_size(surface->xdg_toplevel, 0, 0);
            xdg_toplevel_set_max_size(surface->xdg_toplevel, 0, 0);
        }
        else
        {
            xdg_toplevel_set_min_size(surface->xdg_toplevel, width, height);
            xdg_toplevel_set_max_size(surface->xdg_toplevel, width, height);
        }

        wayland_surface_update_toplevel_parent(surface);
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
    RECT rect = client_rect ? *client_rect : client->rect;

    /* The offset of the client area origin relatively to the window origin. */
    rect = map_rect_to_surface(surface, rect);

    TRACE("hwnd=%p rect=%s\n", surface->hwnd, wine_dbgstr_rect(&rect));

    if (client->wl_subsurface)
    {
        wl_subsurface_set_position(client->wl_subsurface, rect.left, rect.top);
        wl_subsurface_place_above(client->wl_subsurface, surface->wl_surface);
    }

    if (rect.left != rect.right && rect.top != rect.bottom)
        wp_viewport_set_destination(client->wp_viewport, rect.right - rect.left, rect.bottom - rect.top);
    else /* We can't have a 0x0 destination, use 1x1 instead. */
        wp_viewport_set_destination(client->wp_viewport, 1, 1);

    client->rect = *client_rect;
}

/**********************************************************************
 *          wayland_surface_reconfigure_xdg
 *
 * Reconfigures the xdg surface as needed to match the latest requested
 * state.
 */
static BOOL wayland_surface_reconfigure_xdg(struct wayland_surface *surface, RECT rect)
{
    struct wayland_window_config *window = &surface->window;

    /* Acknowledge any compatible processed config. */
    if (surface->processing.serial && surface->processing.processed &&
        wayland_surface_config_is_compatible(&surface->processing, rect,
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
             wayland_surface_config_is_compatible(&surface->requested, rect,
                                                  window->state))
    {
        surface->current = surface->requested;
        memset(&surface->requested, 0, sizeof(surface->requested));
        /* clear stale processing config since the requested one is always newer */
        memset(&surface->processing, 0, sizeof(surface->processing));
        xdg_surface_ack_configure(surface->xdg_surface, surface->current.serial);
    }
    else if (!surface->current.serial ||
             !wayland_surface_config_is_compatible(&surface->current, rect,
                                                   window->state))
    {
        return FALSE;
    }

    wayland_surface_reconfigure_geometry(surface, rect);

    return TRUE;
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
    RECT rect = map_rect_to_surface(surface, surface->window.rect);

    TRACE("hwnd=%p window=%s,%#x processing=%s,%#x current=%s,%#x\n",
          surface->hwnd, wine_dbgstr_rect(&rect), window->state,
          wine_dbgstr_rect(&surface->processing.rect), surface->processing.state,
          wine_dbgstr_rect(&surface->current.rect), surface->current.state);

    switch (surface->role)
    {
    case WAYLAND_SURFACE_ROLE_NONE:
        break;
    case WAYLAND_SURFACE_ROLE_POPUP:
    case WAYLAND_SURFACE_ROLE_TOPLEVEL:
        if (!surface->xdg_surface) break; /* surface role has been cleared */
        if (!wayland_surface_reconfigure_xdg(surface, rect)) return FALSE;
        break;
    }

    wayland_surface_reconfigure_size(surface, rect.right - rect.left, rect.bottom - rect.top);

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
 *           copy_rectangle_into_center_of_square
 *
 * Copies non-square rectangle src to the center of square dest.
 */
static void copy_rectangle_into_center_of_square(const unsigned int *src,
                                                 int src_w, int src_h,
                                                 unsigned int *dest)
{
    int dest_length;

    if (src_w > src_h)
    {
        dest += src_w * (src_w - src_h) / 2;
        dest_length = src_w;
    }
    else
    {
        dest += (src_h - src_w) / 2;
        dest_length = src_h;
    }

    for (int h = 0; h < src_h; h++, dest += dest_length, src += src_w)
        memcpy(dest, src, src_w * 4);
}

/***********************************************************************
 *           wayland_shm_buffer_from_color_bitmaps
 *
 * Create a wayland_shm_buffer for a color bitmap.
 *
 * Adapted from wineandroid.drv code.
 */
struct wayland_shm_buffer *wayland_shm_buffer_from_color_bitmaps(HDC hdc, HBITMAP color,
                                                                 HBITMAP mask,
                                                                 BOOL allow_padding)
{
    struct wayland_shm_buffer *shm_buffer = NULL;
    char buffer[FIELD_OFFSET(BITMAPINFO, bmiColors[256])];
    BITMAPINFO *info = (BITMAPINFO *)buffer;
    BITMAP bm;
    unsigned int *ptr, *bits = NULL;
    unsigned char *mask_bits = NULL;
    int i, j, square_length;
    BOOL has_alpha = FALSE, use_padding = FALSE;

    if (!NtGdiExtGetObjectW(color, sizeof(bm), &bm)) goto failed;

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

    use_padding = allow_padding && bm.bmWidth != bm.bmHeight;

    if (use_padding)
    {
        square_length = max(bm.bmWidth, bm.bmHeight);
        shm_buffer = wayland_shm_buffer_create(square_length, square_length,
                                               WL_SHM_FORMAT_ARGB8888);
        if (!shm_buffer) goto failed;
        if (!(bits = malloc(info->bmiHeader.biSizeImage))) goto failed;
    }
    else
    {
        shm_buffer = wayland_shm_buffer_create(bm.bmWidth, bm.bmHeight,
                                               WL_SHM_FORMAT_ARGB8888);
        if (!shm_buffer) goto failed;
        bits = shm_buffer->map_data;
    }

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

    if (use_padding)
    {
        copy_rectangle_into_center_of_square(bits, bm.bmWidth,
                                             bm.bmHeight, shm_buffer->map_data);
        free(bits);
        bits = shm_buffer->map_data;
    }

    /* Wayland requires pre-multiplied alpha values */
    for (ptr = bits, i = 0; i < shm_buffer->width * shm_buffer->height; ptr++, i++)
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
    if (use_padding) free(bits);
    free(mask_bits);
    return NULL;
}

/**********************************************************************
 *          map_rect_to_surface
 *
 * Converts the window (logical) coordinates to wayland surface-local coordinates.
 */
RECT map_rect_to_surface(struct wayland_surface *surface, RECT rect)
{
    rect.left = round(rect.left / surface->window.scale);
    rect.top  = round(rect.top / surface->window.scale);
    rect.right = round(rect.right / surface->window.scale);
    rect.bottom  = round(rect.bottom / surface->window.scale);
    return rect;
}

/**********************************************************************
 *          map_point_to_surface
 *
 * Converts the window (logical) coordinates to wayland surface-local coordinates.
 */
POINT map_point_to_surface(struct wayland_surface *surface, POINT point)
{
    point.x = round(point.x / surface->window.scale);
    point.y  = round(point.y / surface->window.scale);
    return point;
}

/**********************************************************************
 *          map_rect_from_surface
 *
 * Converts the surface-local coordinates to window (logical) coordinates.
 */
RECT map_rect_from_surface(struct wayland_surface *surface, RECT rect)
{
    rect.left = round(rect.left * surface->window.scale);
    rect.top  = round(rect.top * surface->window.scale);
    rect.right = round(rect.right * surface->window.scale);
    rect.bottom  = round(rect.bottom * surface->window.scale);
    return rect;
}

/**********************************************************************
 *          map_point_from_surface
 *
 * Converts the surface-local coordinates to window (logical) coordinates.
 */
POINT map_point_from_surface(struct wayland_surface *surface, POINT point)
{
    point.x = round(point.x * surface->window.scale);
    point.y = round(point.y * surface->window.scale);
    return point;
}

static void wayland_client_surface_destroy(struct client_surface *client)
{
    struct wayland_client_surface *surface = impl_from_client_surface(client);

    TRACE("%s\n", debugstr_client_surface(client));

    if (surface->color_management)
        wp_color_management_surface_v1_destroy(surface->color_management);
    if (surface->wp_viewport)
        wp_viewport_destroy(surface->wp_viewport);
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
        wayland_client_surface_attach(surface, NULL, NULL);
        wayland_win_data_release(data);
    }
}

/* Adapted from winex11.drv */
static BOOL needs_client_window_clipping(HWND hwnd)
{
    RECT rect, client;
    UINT ret = 0;
    HRGN region;
    HDC hdc;

    if (!NtUserGetClientRect(hwnd, &client, NtUserGetDpiForWindow(hwnd))) return FALSE;
    OffsetRect(&client, -client.left, -client.top);

    if (!(hdc = NtUserGetDCEx(hwnd, 0, DCX_CACHE | DCX_USESTYLE))) return FALSE;
    if ((region = NtGdiCreateRectRgn(0, 0, 0, 0)))
    {
        ret = NtGdiGetRandomRgn(hdc, region, SYSRGN);
        if (ret > 0 && (ret = NtGdiGetRgnBox(region, &rect)) < NULLREGION) ret = 0;
        OffsetRect(&rect, -rect.left, -rect.top);
        if (ret == SIMPLEREGION && EqualRect(&rect, &client)) ret = 0;
        NtGdiDeleteObjectApp(region);
    }
    NtUserReleaseDC(hwnd, hdc);

    return ret > 0;
}

/* fork a4c5bf2e756: WAYLANDDRV_OFFSCREEN=1 forces offscreen presentation of every client surface */
static LONG force_use_offscreen(void)
{
    static LONG ret = -1;
    const char *env;

    if (ret == -1) ret = (env = getenv("WAYLANDDRV_OFFSCREEN")) && !strcmp(env, "1");

    return ret;
}

/* fork b347413b4c3, fe7e7131078, 9d2a2b7d94b, 9892dc41c87: decide whether win32u has to copy the client
 * surface contents into the window surface instead of showing it through a wl_subsurface. */
void wayland_client_surface_update_offscreen(struct wayland_client_surface *surface)
{
    HWND hwnd = surface->client.hwnd, toplevel = surface->client.toplevel;
    LONG offscreen = force_use_offscreen();
    struct wayland_win_data *data;
    DWORD pid, flags;
    BYTE alpha;

    if (!toplevel) toplevel = NtUserGetAncestor(hwnd, GA_ROOT);

    /* windows of other processes are rendered by win32u, we cannot share their surfaces */
    if (NtUserGetWindowThread(toplevel, &pid) && pid != GetCurrentProcessId())
        offscreen = TRUE;

    if (!offscreen && (data = wayland_win_data_get(toplevel)))
    {
        if (data->shaped) offscreen = TRUE;
        wayland_win_data_release(data);
    }

    /* the alpha-modifier protocol does not specify blending, so layered windows are composited by win32u */
    if (!offscreen && NtUserGetLayeredWindowAttributes(toplevel, NULL, &alpha, &flags) &&
        (flags & LWA_ALPHA) && alpha != 0xff)
        offscreen = TRUE;

    if (!offscreen && NtUserGetWindowRelative(hwnd, GW_CHILD))
        offscreen = needs_client_window_clipping(hwnd);

    InterlockedExchange(&surface->client.offscreen, offscreen);
}

static void wayland_client_surface_update(struct client_surface *client)
{
    struct wayland_client_surface *surface = impl_from_client_surface(client);
    HWND hwnd = client->hwnd, toplevel = client->toplevel;
    struct wayland_win_data *data;
    BOOL visible = FALSE;

    TRACE("%s\n", debugstr_client_surface(client));
    wayland_client_surface_update_offscreen(surface);
    if (toplevel) visible = !client->offscreen && NtUserIsWindowVisible(hwnd);
    if (!(data = wayland_win_data_get(hwnd))) return;

    if (toplevel && visible)
        wayland_client_surface_attach(surface, toplevel, &client->monitor_rect);
    else
        wayland_client_surface_attach(surface, NULL, NULL);

    wayland_win_data_release(data);
}

static void wayland_client_surface_present(struct client_surface *client, HDC hdc)
{
    struct wayland_client_surface *surface = impl_from_client_surface(client);
    HWND hwnd = client->hwnd, toplevel = client->toplevel;
    struct wayland_surface *wayland_surface;
    struct wayland_win_data *data;

    DWORD pid;
    HWND root;

    /* NtUser queries, so this runs before any win_data lock is taken, and before
     * the early return below, which skips foreign children. The gate tests the child's
     * GA_ROOT, not `toplevel`, which may be 0; it is the `NtUserGetWindowThread` test that the rule removed by P3-H8 used (`wayland_surface.c:1474`).
     * The frame callback is scheduled inside the call (H11 step 4).
     * If the child's GA_ROOT is 0 (its root cannot be found, as when the window is being destroyed),
     * root is 0 and the gate is FALSE before NtUserGetWindowThread runs. That lookup is not safe for 0
     * (see the zero-thread paragraph in Section 4b), so the import is skipped. If the root is 0, the P3-H10 commit and `set_client_surface` are skipped too: the early return below finds no local win_data for a foreign toplevel, so nothing after the gate runs for that child. That matches HEAD for every foreign child, and is accepted for a window being torn down. */
    if (hwnd != toplevel && (root = NtUserGetAncestor(hwnd, GA_ROOT)) &&
        NtUserGetWindowThread(root, &pid) && pid != GetCurrentProcessId())
    {
        wayland_surface_update_foreign_parent(hwnd);
    }

    /* offscreen: win32u copies the contents to the window surface for us */
    if (hdc || !(data = wayland_win_data_get(toplevel))) return;

    if ((wayland_surface = data->wayland_surface))
    {
        wayland_surface_ensure_contents(wayland_surface);

        /* Handle any processed configure request, to ensure the related
         * surface state is applied by the compositor. */
        if (wayland_surface->processing.serial &&
            wayland_surface->processing.processed &&
            wayland_surface_reconfigure(wayland_surface))
        {
            wl_surface_commit(wayland_surface->wl_surface);
        }
    }

    wayland_win_data_release(data);

    set_client_surface(hwnd, surface);
}

static BOOL wayland_client_surface_set_color_space(struct client_surface *client, VkColorSpaceKHR color_space)
{
    struct wayland_client_surface *surface = impl_from_client_surface(client);
    struct wp_image_description_v1 *desc;
    struct wp_color_manager_v1 *manager;

    TRACE("%s color_space %#x\n", debugstr_client_surface(client), color_space);

    if (!(manager = process_wayland.wp_color_manager_v1)) return FALSE;

    switch (color_space)
    {
    case VK_COLOR_SPACE_EXTENDED_SRGB_LINEAR_EXT: desc = scrgb_desc; break;
    case VK_COLOR_SPACE_HDR10_ST2084_EXT:         desc = bt2100_desc; break;
    default:                                      desc = NULL; break;
    }

    if (!desc)
    {
        if (surface->color_management)
        {
            wp_color_management_surface_v1_destroy(surface->color_management);
            surface->color_management = NULL;
        }
    }
    else if (!surface->color_management && !(surface->color_management = wp_color_manager_v1_get_surface(manager, surface->wl_surface)))
    {
        WARN("Failed to create color management surface for %s\n", debugstr_client_surface(client));
        return FALSE;
    }
    else
    {
        wp_color_management_surface_v1_set_image_description(surface->color_management, desc, WP_COLOR_MANAGER_V1_RENDER_INTENT_PERCEPTUAL);
    }
    wl_display_flush(process_wayland.wl_display);

    return desc || color_space == -1;
}

static const struct client_surface_funcs wayland_client_surface_funcs =
{
    .size = sizeof(struct wayland_client_surface),
    .destroy = wayland_client_surface_destroy,
    .detach = wayland_client_surface_detach,
    .update = wayland_client_surface_update,
    .present = wayland_client_surface_present,
    .set_color_space = wayland_client_surface_set_color_space,
};

struct wayland_client_surface *impl_from_client_surface(struct client_surface *client)
{
    assert(client->funcs == &wayland_client_surface_funcs);
    return CONTAINING_RECORD(client, struct wayland_client_surface, client);
}

struct client_surface *WAYLAND_CreateClientSurface(HWND hwnd, int pixel_format, BOOL raw)
{
    struct wayland_client_surface *client;
    struct wl_region *empty_region;

    if (!(client = client_surface_create(&wayland_client_surface_funcs, hwnd, pixel_format, raw))) return NULL;

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

    return &client->client;

err:
    client_surface_release(&client->client);
    return NULL;
}

void wayland_client_surface_attach(struct wayland_client_surface *client, HWND toplevel, const RECT *rect)
{
    struct wayland_win_data *toplevel_data;
    struct wayland_surface *surface;

    if (!toplevel)
    {
        if (client->wl_subsurface)
        {
            wl_subsurface_destroy(client->wl_subsurface);
            client->wl_subsurface = NULL;
        }

        client->toplevel = 0;
        client->parent_serial = 0;
        return;
    }

    if (!(toplevel_data = wayland_win_data_get(toplevel)) || !(surface = toplevel_data->wayland_surface))
    {
        if (toplevel_data) wayland_win_data_release(toplevel_data);
        return wayland_client_surface_attach(client, NULL, NULL);
    }

    /* The toplevel can keep its HWND while its wayland_surface is recreated; the old
     * subsurface then belongs to a stale wl_surface and must be recreated too. */
    if (client->toplevel != toplevel || client->parent_serial != surface->serial)
    {
        if (client->toplevel == toplevel && client->wl_subsurface)
            TRACE("hwnd=%p recreating subsurface, parent serial %s -> %s\n", toplevel,
                  wine_dbgstr_longlong(client->parent_serial), wine_dbgstr_longlong(surface->serial));
        wayland_client_surface_attach(client, NULL, NULL);

        client->wl_subsurface =
            wl_subcompositor_get_subsurface(process_wayland.wl_subcompositor,
                                            client->wl_surface,
                                            surface->wl_surface);
        if (!client->wl_subsurface) goto done;

        /* Present contents independently of the parent surface. */
        wl_subsurface_set_desync(client->wl_subsurface);

        client->toplevel = toplevel;
        client->parent_serial = surface->serial;
    }

    wayland_surface_reconfigure_client(surface, client, rect);
    /* Commit to apply subsurface positioning. */
    wl_surface_commit(surface->wl_surface);

done:
    wayland_win_data_release(toplevel_data);
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

    wayland_shm_buffer_unref(dummy_shm_buffer);
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
    icon_buf = wayland_shm_buffer_from_color_bitmaps(hDC, ii->hbmColor, ii->hbmMask, TRUE);
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

void wayland_surface_set_opacity(struct wayland_surface *surface, BYTE alpha, UINT flags)
{
    if (surface->wp_alpha_modifier_surface_v1)
    {
        uint32_t opacity = (flags & LWA_ALPHA) ? (UINT32_MAX / 0xff) * alpha : UINT32_MAX;
        wp_alpha_modifier_surface_v1_set_multiplier(surface->wp_alpha_modifier_surface_v1, opacity);
        wl_surface_commit(surface->wl_surface);
        wl_display_flush(process_wayland.wl_display);
    }
}

static void activate_token_handle_done(void *user_data,
                                       struct xdg_activation_token_v1 *xdg_activation_token_v1,
                                       const char *token)
{
    HWND hwnd = user_data;
    struct wayland_win_data *data;
    struct wayland_surface *surface;


    if ((data = wayland_win_data_get(hwnd)))
    {
        if ((surface = data->wayland_surface))
            xdg_activation_v1_activate(process_wayland.xdg_activation_v1, token, surface->wl_surface);
        wayland_win_data_release(data);
    }

    xdg_activation_token_v1_destroy(xdg_activation_token_v1);
}

const static struct xdg_activation_token_v1_listener activate_listener = {
    activate_token_handle_done,
};

static struct xdg_activation_token_v1 *wayland_surface_create_token(
                                        struct wayland_surface *surface,
                                        BOOL activate)
{
    struct wayland_seat *seat = &process_wayland.seat;
    struct xdg_activation_token_v1 *token;
    uint32_t serial = ReadAcquire(&process_wayland.input_serial);

    if (!process_wayland.xdg_activation_v1) return NULL;
    if (!wayland_surface_is_toplevel(surface)) return NULL;

    if (!(token = xdg_activation_v1_get_activation_token(process_wayland.xdg_activation_v1)))
    {
        ERR("Failed to create activation token!\n");
        return NULL;
    }

    xdg_activation_token_v1_set_surface(token, surface->wl_surface);
    if (process_name && activate) xdg_activation_token_v1_set_app_id(token, process_name);

    pthread_mutex_lock(&seat->mutex);
    if (activate && seat->wl_seat) xdg_activation_token_v1_set_serial(token, serial, seat->wl_seat);
    pthread_mutex_unlock(&seat->mutex);

    return token;
}

void wayland_surface_flash_window(struct wayland_surface *surface)
{
    struct xdg_activation_token_v1 *token;

    if (!process_wayland.xdg_activation_v1) return;
    if (!wayland_surface_is_toplevel(surface)) return;

    if (!(token = wayland_surface_create_token(surface, FALSE))) return;
    xdg_activation_token_v1_add_listener(token, &activate_listener, surface->hwnd);
    xdg_activation_token_v1_commit(token);
}
