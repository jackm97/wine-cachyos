/*
 * Wayland window handling
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

#include "ntstatus.h"

#include "waylanddrv.h"

#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(waylanddrv);


static int wayland_win_data_cmp_rb(const void *key,
                                   const struct rb_entry *entry)
{
    HWND key_hwnd = (HWND)key; /* cast to work around const */
    const struct wayland_win_data *entry_win_data =
        RB_ENTRY_VALUE(entry, const struct wayland_win_data, entry);

    if (key_hwnd < entry_win_data->hwnd) return -1;
    if (key_hwnd > entry_win_data->hwnd) return 1;
    return 0;
}

static pthread_mutex_t win_data_mutex;
static struct rb_tree win_data_rb = { wayland_win_data_cmp_rb };

/***********************************************************************
 *           wayland_win_data_create
 *
 * Create a data window structure for an existing window.
 */
static struct wayland_win_data *wayland_win_data_create(HWND hwnd, const struct window_rects *rects)
{
    struct wayland_win_data *data;
    struct rb_entry *rb_entry;
    HWND parent;

    /* Don't create win data for desktop or HWND_MESSAGE windows. */
    if (!(parent = NtUserGetAncestor(hwnd, GA_PARENT))) return NULL;
    if (parent != NtUserGetDesktopWindow() && !NtUserGetAncestor(parent, GA_PARENT))
        return NULL;

    if (!(data = calloc(1, sizeof(*data)))) return NULL;

    data->hwnd = hwnd;
    data->rects = *rects;
    wl_list_init(&data->overlays);

    pthread_mutex_lock(&win_data_mutex);

    /* Check that another thread hasn't already created the wayland_win_data. */
    if ((rb_entry = rb_get(&win_data_rb, hwnd)))
    {
        free(data);
        return RB_ENTRY_VALUE(rb_entry, struct wayland_win_data, entry);
    }

    rb_put(&win_data_rb, hwnd, &data->entry);

    TRACE("hwnd=%p\n", data->hwnd);

    return data;
}

/***********************************************************************
 *           wayland_win_data_destroy
 */
static void wayland_win_data_destroy(struct wayland_win_data *data)
{
    TRACE("hwnd=%p\n", data->hwnd);

    rb_remove(&win_data_rb, &data->entry);

    pthread_mutex_unlock(&win_data_mutex);

    wayland_destroy_overlays(data);
    if (data->wayland_surface) wayland_surface_destroy(data->wayland_surface);
    if (data->window_contents) wayland_shm_buffer_unref(data->window_contents);
    free(data);
}

/***********************************************************************
 *           wayland_win_data_get
 *
 * Lock and return the data structure associated with a window.
 */
struct wayland_win_data *wayland_win_data_get(HWND hwnd)
{
    struct rb_entry *entry;

    pthread_mutex_lock(&win_data_mutex);
    if ((entry = rb_get(&win_data_rb, hwnd)))
        return RB_ENTRY_VALUE(entry, struct wayland_win_data, entry);
    pthread_mutex_unlock(&win_data_mutex);

    return NULL;
}

/***********************************************************************
 *           wayland_win_data_release
 *
 * Release the data returned by wayland_win_data_get.
 */
void wayland_win_data_release(struct wayland_win_data *data)
{
    assert(data);
    pthread_mutex_unlock(&win_data_mutex);
}

static void wayland_win_data_get_config(struct wayland_win_data *data,
                                        struct wayland_window_config *conf)
{
    enum wayland_surface_config_state window_state = 0;
    DWORD style, exstyle;

    conf->rect = data->rects.visible;
    conf->client_rect = data->rects.client;
    style = NtUserGetWindowLongW(data->hwnd, GWL_STYLE);
    exstyle = NtUserGetWindowLongW(data->hwnd, GWL_EXSTYLE);

    TRACE("window=%s style=%#x exstyle=%#x\n", wine_dbgstr_rect(&conf->rect), style, exstyle);

    conf->minimized = !!(style & WS_MINIMIZE);

    /* The fullscreen state is implied by the window position and style. */
    if (data->is_fullscreen)
    {
        /* A window that keeps its caption and its resizing frame is a normal
         * window which happens to cover the monitor, not a fullscreen one:
         * applications drop their decorations when they go fullscreen. Making
         * it fullscreen would leave it stuck to the monitor, since a Wayland
         * fullscreen window cannot be moved or resized. */
        if ((style & WS_CAPTION) == WS_CAPTION && (style & WS_THICKFRAME))
        {
            if (style & WS_MAXIMIZE)
                window_state |= WAYLAND_SURFACE_CONFIG_STATE_MAXIMIZED;
        }
        else if (!(style & WS_MINIMIZE))
            window_state |= WAYLAND_SURFACE_CONFIG_STATE_FULLSCREEN;
    }
    else if (style & WS_MAXIMIZE)
    {
        window_state |= WAYLAND_SURFACE_CONFIG_STATE_MAXIMIZED;
    }

    conf->resizeable = data->resizeable;
    conf->state = window_state;
    conf->managed = data->managed;
}

static void reapply_cursor_clipping(void)
{
    RECT rect;
    UINT context = NtUserSetThreadDpiAwarenessContext(NTUSER_DPI_PER_MONITOR_AWARE);
    if (NtUserGetClipCursor(&rect)) NtUserClipCursor(&rect);
    NtUserSetThreadDpiAwarenessContext(context);
}

/* Clears the role of every popup owned by the window of surface, deepest
 * first, so each xdg_popup is destroyed before the xdg_surface it is a child
 * of. Called with win_data_mutex held. Recursion is bounded by a depth guard:
 * past depth 32 the walk stops with a WARN instead of recursing, so a cyclic
 * owner chain cannot overflow the stack. Popups past that point are not
 * cleared by this walk. */
static void reparent_popups_depth(struct wayland_surface *surface, int depth)
{
    HWND hwnd = surface->hwnd;
    struct wayland_win_data *entry;
    struct wayland_surface *entry_surface;

    if (depth > 32)
    {
        WARN("owner chain too deep at hwnd %p\n", hwnd);
        return;
    }
    if (!hwnd) return;

    RB_FOR_EACH_ENTRY(entry, &win_data_rb, struct wayland_win_data, entry)
    {
        if (entry->hwnd == hwnd) continue;
        if (!(entry_surface = entry->wayland_surface)) continue;
        if (entry_surface->owner_hwnd != hwnd) continue;
        if (!wayland_surface_is_popup(entry_surface)) continue;

        /* deepest first: clear the popups this popup owns before this popup */
        reparent_popups_depth(entry_surface, depth + 1);
        wayland_surface_clear_role(entry_surface);
        NtUserPostMessage(entry_surface->hwnd, WM_WINE_UPDATEWINDOWSTATE, 0, 0);
    }
}

void wayland_surface_reparent_popups(struct wayland_surface *surface)
{
    reparent_popups_depth(surface, 0);
}

static BOOL wayland_win_data_create_wayland_surface(struct wayland_win_data *data, struct wayland_surface *owner_surface)
{
    struct wayland_surface *surface;
    enum wayland_surface_role role;
    BOOL visible, layer_set;
    DWORD exstyle = NtUserGetWindowLongW(data->hwnd, GWL_EXSTYLE);
    struct wl_region *input_region;
    int old_width, old_height;
    BOOL server_decor;

    TRACE("hwnd=%p\n", data->hwnd);

    layer_set = !(exstyle & WS_EX_LAYERED) || data->layered_attribs_set || data->client_surface;
    visible = (NtUserGetWindowLongW(data->hwnd, GWL_STYLE) & WS_VISIBLE) == WS_VISIBLE;
    /* a layered window stays mapped once it is mapped */
    if (!data->wayland_surface || !data->wayland_surface->window.visible) visible = visible && layer_set;

    /* 0x0 windows are roleless */
    if (!visible || IsRectEmpty(&data->rects.visible)) role = WAYLAND_SURFACE_ROLE_NONE;
    else if (owner_surface && !data->managed) role = WAYLAND_SURFACE_ROLE_POPUP;
    else if (!owner_surface || !data->managed) role = WAYLAND_SURFACE_ROLE_TOPLEVEL;
    /* managed child windows do not need subsurfaces */
    else role = WAYLAND_SURFACE_ROLE_NONE;

    /* the surface as it was before this call decides whether a refused decoration stays refused */
    server_decor = !EqualRect(&data->rects.visible, &data->rects.window) &&
                   is_decoration_enabled(NtUserGetWindowLongW(data->hwnd, GWL_STYLE), exstyle) &&
                   !(data->wayland_surface && data->wayland_surface->server_decor_refused);

    /* we can temporarily clear the role of a surface but cannot assign a different one after it's set */
    if ((surface = data->wayland_surface) && role && surface->role && surface->role != role)
    {
        /* Make sure any attached client surface is detached before we destroy the surface.
         * They will be reattached when win32u updates them again after WindowPosChanged.
         */
        data->wayland_surface = NULL;
        update_client_surfaces(data->hwnd);
        wayland_surface_reparent_popups(surface);
        wayland_destroy_overlays(data);
        wayland_surface_destroy(surface);
    }

    if (!(surface = data->wayland_surface) && !(surface = wayland_surface_create(data->hwnd))) return FALSE;

    /* Pass through mouse events for layered, transparent windows, to match
     * Windows behavior. */
    input_region = ((exstyle & WS_EX_TRANSPARENT) && (exstyle & WS_EX_LAYERED)) ?
                   wl_compositor_create_region(process_wayland.wl_compositor) :
                   NULL;
    wl_surface_set_input_region(surface->wl_surface, input_region);
    if (input_region) wl_region_destroy(input_region);

    /* a resize invalidates the ensured contents; a move or a state change that keeps the size does not */
    old_width = surface->window.rect.right - surface->window.rect.left;
    old_height = surface->window.rect.bottom - surface->window.rect.top;
    surface->window.visible = visible;
    wayland_win_data_get_config(data, &surface->window);
    if (old_width != surface->window.rect.right - surface->window.rect.left ||
        old_height != surface->window.rect.bottom - surface->window.rect.top)
    {
        surface->ensured_contents = WAYLAND_SURFACE_NOT_ENSURED;
        surface->expose_posted = FALSE;
    }

    /* If the window is a visible toplevel make it a wayland
     * xdg_toplevel. Otherwise keep it role-less to avoid polluting the
     * compositor with empty xdg_toplevels. */
    switch (role)
    {
    case WAYLAND_SURFACE_ROLE_NONE:
        wayland_surface_clear_role(surface);
        break;
    case WAYLAND_SURFACE_ROLE_POPUP:
        wayland_surface_make_popup(surface, owner_surface);
        break;
    case WAYLAND_SURFACE_ROLE_TOPLEVEL:
        wayland_surface_make_toplevel(surface, server_decor);
        break;
    }


    /* Size/position changes affect the effective pointer constraint, so update
     * it as needed. */
    if (data->hwnd == NtUserGetForegroundWindow()) reapply_cursor_clipping();

    TRACE("hwnd=%p surface=%p=>%p\n", data->hwnd, data->wayland_surface, surface);
    data->wayland_surface = surface;
    return TRUE;
}

static void wayland_surface_update_state_toplevel(struct wayland_surface *surface)
{
    const RECT *rect = &surface->window.rect;
    BOOL processing_config = surface->processing.serial &&
                             !surface->processing.processed;

    TRACE("hwnd=%p window_state=%#x %s->state=%#x\n",
          surface->hwnd, surface->window.state,
          processing_config ? "processing" : "current",
          processing_config ? surface->processing.state : surface->current.state);

    /* update the parent here as well to ensure that its not stale if the owner is updated
     * with no new contents comitted or state change */
    wayland_surface_update_toplevel_parent(surface);

    /* If we are not processing a compositor requested config, use the
     * window state to determine and update the Wayland state. */
    if (!processing_config)
    {
        xdg_toplevel_set_min_size(surface->xdg_toplevel, 0, 0);
        xdg_toplevel_set_max_size(surface->xdg_toplevel, 0, 0);

         /* First do all state unsettings, before setting new state. Some
          * Wayland compositors misbehave if the order is reversed. */
        if (!(surface->window.state & WAYLAND_SURFACE_CONFIG_STATE_MAXIMIZED) &&
            (surface->current.state & WAYLAND_SURFACE_CONFIG_STATE_MAXIMIZED) &&
            !surface->window.minimized)
        {
            xdg_toplevel_unset_maximized(surface->xdg_toplevel);
        }
        if (!(surface->window.state & WAYLAND_SURFACE_CONFIG_STATE_FULLSCREEN) &&
            (surface->current.state & WAYLAND_SURFACE_CONFIG_STATE_FULLSCREEN) &&
            !surface->window.minimized)
        {
            xdg_toplevel_unset_fullscreen(surface->xdg_toplevel);
            wayland_surface_shortcut_control(surface, FALSE);
            surface->requested_output = NULL;
        }

        if ((surface->window.state & WAYLAND_SURFACE_CONFIG_STATE_MAXIMIZED) &&
           !(surface->current.state & WAYLAND_SURFACE_CONFIG_STATE_MAXIMIZED))
        {
            xdg_toplevel_set_maximized(surface->xdg_toplevel);
        }
        if (surface->window.state & WAYLAND_SURFACE_CONFIG_STATE_FULLSCREEN)
        {
            struct wayland_output *wayland_output;
            struct wl_output *output = NULL;

            wayland_output = wayland_output_for_rect(rect);
            pthread_mutex_lock(&process_wayland.output_mutex);
            if (wayland_output) output = wayland_output->wl_output;

            if (surface->current.state & WAYLAND_SURFACE_CONFIG_STATE_FULLSCREEN)
            {
                if (surface->requested_output != output)
                {
                    xdg_toplevel_unset_fullscreen(surface->xdg_toplevel);
                    wl_display_flush(process_wayland.wl_display);
                }
                else
                    goto skip_fullscreen;
            }

            xdg_toplevel_set_fullscreen(surface->xdg_toplevel, output);
            wayland_surface_shortcut_control(surface, TRUE);
            surface->requested_output = output;

            skip_fullscreen:
            pthread_mutex_unlock(&process_wayland.output_mutex);
            if (wayland_output) wayland_output_release(wayland_output);
        }
        /* xdg-shell has no unset_minimized: the request is flushed by the caller */
        if (surface->window.minimized && !surface->committed.minimized)
        {
            xdg_toplevel_set_minimized(surface->xdg_toplevel);
        }

        /* reset the size hint since we don't want to poison the next configure event with it,
         * only when the state differs from what was committed */
        if (surface->committed.state != surface->window.state)
        {
            xdg_toplevel_set_min_size(surface->xdg_toplevel, 0, 0);
            xdg_toplevel_set_max_size(surface->xdg_toplevel, 0, 0);
            wl_surface_commit(surface->wl_surface);
        }

        surface->committed.minimized = surface->window.minimized;
        surface->committed.state = surface->window.state;
    }
    else
    {
        surface->processing.processed = TRUE;
    }
}

static void wayland_win_data_update_wayland_state(struct wayland_win_data *data)
{
    struct wayland_surface *surface = data->wayland_surface;

    switch (surface->role)
    {
    case WAYLAND_SURFACE_ROLE_NONE:
    /* popups do not have any state to update */
    case WAYLAND_SURFACE_ROLE_POPUP:
        break;
    case WAYLAND_SURFACE_ROLE_TOPLEVEL:
        if (!surface->xdg_surface) break; /* surface role has been cleared */
        wayland_surface_update_state_toplevel(surface);
        break;
    }

    wl_display_flush(process_wayland.wl_display);
}

static BOOL is_managed(HWND hwnd)
{
    struct wayland_win_data *data = wayland_win_data_get(hwnd);
    BOOL ret = data && data->managed;
    if (data) wayland_win_data_release(data);
    return ret;
}

static HWND *build_hwnd_list(void)
{
    NTSTATUS status;
    HWND *list;
    ULONG count = 128;

    for (;;)
    {
        if (!(list = malloc(count * sizeof(*list)))) return NULL;
        status = NtUserBuildHwndList(0, 0, 0, 0, 0, count, list, &count);
        if (!status) return list;
        free(list);
        if (status != STATUS_BUFFER_TOO_SMALL) return NULL;
    }
}

static BOOL has_owned_popups(HWND hwnd)
{
    HWND *list;
    UINT i;
    BOOL ret = FALSE;

    if (!(list = build_hwnd_list())) return FALSE;

    for (i = 0; list[i] != HWND_BOTTOM; i++)
    {
        if (list[i] == hwnd) break;  /* popups are always above owner */
        if (NtUserGetWindowRelative(list[i], GW_OWNER) != hwnd) continue;
        if ((ret = is_managed(list[i]))) break;
    }

    free(list);
    return ret;
}

static inline HWND get_active_window(void)
{
    GUITHREADINFO info;
    info.cbSize = sizeof(info);
    return NtUserGetGUIThreadInfo(GetCurrentThreadId(), &info) ? info.hwndActive : 0;
}

static BOOL is_mso_border_effect_window(HWND hwnd)
{
    static const WCHAR border_effect[] = u"MSO_BORDEREFFECT_WINDOW_CLASS";
    WCHAR class_name[ARRAY_SIZE(border_effect) + 1];
    UNICODE_STRING name = {.Buffer = class_name, .MaximumLength = sizeof(class_name)};
    DWORD style = NtUserGetWindowLongW(hwnd, GWL_STYLE);
    DWORD ex_style = NtUserGetWindowLongW(hwnd, GWL_EXSTYLE);

    return NtUserGetClassName(hwnd, FALSE, &name) == (INT)(ARRAY_SIZE(border_effect) - 1) &&
           !wcsncmp(class_name, border_effect, ARRAY_SIZE(border_effect) - 1) &&
           (style & WS_POPUP) && (ex_style & WS_EX_LAYERED);
}

/***********************************************************************
 *		is_window_managed
 *
 * Check if a given window should be managed
 */
static BOOL is_window_managed(HWND hwnd, UINT swp_flags, BOOL fullscreen)
{
    DWORD style, ex_style;

    /* child windows are not managed */
    style = NtUserGetWindowLongW(hwnd, GWL_STYLE);
    if ((style & (WS_CHILD|WS_POPUP)) == WS_CHILD) return FALSE;
    /* tool popups (tooltips, menus, flyouts) stay attached to the window they
     * belong to, even when active, since a Wayland toplevel cannot be
     * positioned. They do not always have an owner, tooltips of an application
     * that makes its own often have none, and without this they would show up
     * as separate windows. */
    ex_style = NtUserGetWindowLongW(hwnd, GWL_EXSTYLE);
    if ((style & WS_POPUP) && !(style & (WS_CAPTION|WS_THICKFRAME)) &&
        (ex_style & WS_EX_TOOLWINDOW))
        return FALSE;
    /* activated windows are managed */
    if (!(swp_flags & (SWP_NOACTIVATE|SWP_HIDEWINDOW))) return TRUE;
    if (hwnd == get_active_window()) return TRUE;
    /* windows with caption are managed */
    if ((style & WS_CAPTION) == WS_CAPTION) return TRUE;
    /* windows with thick frame are managed */
    if (style & WS_THICKFRAME) return TRUE;
    if (style & WS_POPUP)
    {
        /* popup with sysmenu == caption are managed */
        if (style & WS_SYSMENU) return TRUE;
        /* full-screen popup windows are managed */
        if (fullscreen) return TRUE;
    }
    /* application windows are managed */
    ex_style = NtUserGetWindowLongW(hwnd, GWL_EXSTYLE);
    if (ex_style & WS_EX_APPWINDOW) return TRUE;
    /* windows that own popups are managed */
    if (has_owned_popups(hwnd)) return TRUE;
    /* default: not managed */
    return FALSE;
}

/***********************************************************************
 *           WAYLAND_DestroyWindow
 */
void WAYLAND_DestroyWindow(HWND hwnd)
{
    struct wayland_win_data *data;
    HWND parent = NULL, toplevel;
    BOOL restack;

    TRACE("%p\n", hwnd);

    if (!(data = wayland_win_data_get(hwnd))) return;
    if (data->wayland_surface)
    {
        wayland_surface_reparent_popups(data->wayland_surface);
        parent = data->wayland_surface->foreign_parent_hwnd;
    }
    /* the toplevel whose overlays may show this window, read under the lock */
    toplevel = data->overlay_toplevel;
    wayland_win_data_destroy(data);

    /* the child prop lives on the parent, which was read under the lock above */
    wayland_forget_foreign_child(parent, hwnd);

    /* drop the overlay of the destroyed window by restacking its toplevel, with no lock held */
    if (!toplevel || toplevel == hwnd || !(data = wayland_win_data_get(toplevel))) return;
    if ((restack = !wl_list_empty(&data->overlays))) data->restack_pending = TRUE;
    wayland_win_data_release(data);
    if (restack) wayland_restack_client_surfaces(toplevel);
}

/***********************************************************************
 *           WAYLAND_WindowPosChanging
 */
BOOL WAYLAND_WindowPosChanging(HWND hwnd, UINT swp_flags, BOOL shaped, const struct window_rects *rects)
{
    struct wayland_win_data *data = wayland_win_data_get(hwnd);

    TRACE("hwnd %p, swp_flags %04x, shaped %u, rects %s\n", hwnd, swp_flags, shaped, debugstr_window_rects(rects));

    if (!data && !(data = wayland_win_data_create(hwnd, rects))) return FALSE;

    data->shaped = shaped;

    wayland_win_data_release(data);

    return TRUE;
}

/***********************************************************************
 *           WAYLAND_WindowPosChanged
 */
void WAYLAND_WindowPosChanged(HWND hwnd, HWND insert_after, HWND owner_hint, UINT swp_flags,
                              const struct window_rects *new_rects, struct window_surface *surface)
{
    HWND owner = NtUserGetAncestor(hwnd, GA_ROOT);
    struct wayland_surface *owner_surface = NULL;
    struct wayland_win_data *data, *owner_data, *toplevel_data, *old_data;
    HWND old_toplevel = NULL;
    BOOL managed, fullscreen = swp_flags & WINE_SWP_FULLSCREEN;
    /* queried before win_data is locked */
    HWND toplevel = NtUserGetAncestor(hwnd, GA_ROOT);
    BOOL border_effect, visible, foreign_child;
    DWORD pid;

    /* these are NtUser calls, so they run before win_data is locked */
    border_effect = is_mso_border_effect_window(hwnd);
    visible = NtUserIsWindowVisible(hwnd);
    /* a foreign child's toplevel belongs to another process, so it has no win_data here */
    foreign_child = toplevel && toplevel != hwnd && NtUserGetWindowThread(toplevel, &pid) && pid != GetCurrentProcessId();
    /* Windows paints a border effect window for its owner, so it never gets a wayland surface */
    if (border_effect) surface = NULL;

    /* Get the managed state with win_data unlocked, as is_window_managed
     * may need to query win_data information about other HWNDs and thus
     * acquire the lock itself internally. */
    if (!(managed = is_window_managed(hwnd, swp_flags, fullscreen)) && surface) owner = owner_hint;

    TRACE("hwnd %p owner %p new_rects %s after %p flags %08x\n", hwnd,
          owner, debugstr_window_rects(new_rects), insert_after, swp_flags);

    if (!(data = wayland_win_data_get(hwnd))) return;
    owner_data = owner && owner != hwnd && !foreign_child ? wayland_win_data_get(owner) : NULL;
    owner_surface = owner_data ? owner_data->wayland_surface : NULL;
    /* for it to be a popup, we need a valid xdg surface. */
    if (owner_surface && !owner_surface->xdg_surface) owner_surface = NULL;

    data->rects = *new_rects;
    data->is_fullscreen = fullscreen;
    data->resizeable = swp_flags & WINE_SWP_RESIZABLE;
    data->managed = managed;

    /* the toplevel whose overlay last showed this window, read under its lock;
     * cleared when the window has left it, since that toplevel is restacked below */
    old_toplevel = data->overlay_toplevel;
    if (old_toplevel && old_toplevel != toplevel) data->overlay_toplevel = NULL;

    /* a moved child may change the stacking of its toplevel */
    if (toplevel && toplevel != hwnd && (toplevel_data = wayland_win_data_get(toplevel)))
    {
        toplevel_data->restack_pending = TRUE;
        wayland_win_data_release(toplevel_data);
    }

    if (!surface && (border_effect || !visible || (toplevel != hwnd && !(foreign_child && data->client_surface))))
    {
        /* a visible, onscreen in-process child attaches its client surface to its toplevel; anything else detaches it (an offscreen client is detached, as in P3-H9 and P3-H12) */
        if (data->client_surface)
        {
            if (!border_effect && visible && toplevel && !data->client_surface->client.offscreen)
                wayland_client_surface_attach(data->client_surface, toplevel, &data->client_surface->client.monitor_rect);
            else
                wayland_client_surface_attach(data->client_surface, NULL, NULL);
        }
        if (data->wayland_surface)
        {
            wayland_surface_reparent_popups(data->wayland_surface);
            wayland_destroy_overlays(data);
            wayland_surface_destroy(data->wayland_surface);
            data->wayland_surface = NULL;
        }
    }
    else if (wayland_win_data_create_wayland_surface(data, owner_surface))
    {
        wayland_win_data_update_wayland_state(data);
    }

    if (owner_data) wayland_win_data_release(owner_data);
    wayland_win_data_release(data);

    if (toplevel) wayland_restack_client_surfaces(toplevel);

    /* the child left old_toplevel: restack it with no lock held, so its unused overlay is destroyed */
    if (old_toplevel && old_toplevel != toplevel && (old_data = wayland_win_data_get(old_toplevel)))
    {
        old_data->restack_pending = TRUE;
        wayland_win_data_release(old_data);
        wayland_restack_client_surfaces(old_toplevel);
    }
}

static void wayland_configure_window(HWND hwnd)
{
    struct wayland_surface *surface;
    INT width, height;
    UINT flags = 0;
    uint32_t state;
    DWORD style;
    BOOL needs_enter_size_move = FALSE;
    BOOL needs_exit_size_move = FALSE;
    BOOL restoring_from_minimize = FALSE;
    struct wayland_win_data *data;
    RECT rect;

    if (!(data = wayland_win_data_get(hwnd))) return;
    if (!(surface = data->wayland_surface))
    {
        wayland_win_data_release(data);
        return;
    }

    /* popups reach this path on purpose: their empty config only acks the processed configure */
    if (!surface->xdg_surface)
    {
        TRACE("missing xdg_surface, returning\n");
        wayland_win_data_release(data);
        return;
    }

    if (!surface->requested.serial)
    {
        TRACE("requested configure event already handled, returning\n");
        wayland_win_data_release(data);
        return;
    }

    surface->processing = surface->requested;
    memset(&surface->requested, 0, sizeof(surface->requested));

    state = surface->processing.state;
    /* Ignore size hints if we don't have a state that requires strict
     * size adherence, in order to avoid spurious resizes.
     * The tiled and maximized states have a strict size adherance, so
     * their sizes cannot change while we are still processing the new config.
     * This allows us to respect the size hint on transitions of maximized/tiled
     * to a stateless regular window. */
    if (state || surface->current.state &
        (WAYLAND_SURFACE_CONFIG_STATE_MAXIMIZED |
               WAYLAND_SURFACE_CONFIG_STATE_TILED))
    {
        width = surface->processing.rect.right - surface->processing.rect.left;
        height = surface->processing.rect.bottom - surface->processing.rect.top;
    }
    else
    {
        width = height = 0;
    }

    if ((state & WAYLAND_SURFACE_CONFIG_STATE_RESIZING) && !surface->resizing)
    {
        surface->resizing = TRUE;
        needs_enter_size_move = TRUE;
    }

    if (!(state & WAYLAND_SURFACE_CONFIG_STATE_RESIZING) && surface->resizing)
    {
        surface->resizing = FALSE;
        needs_exit_size_move = TRUE;
    }

    /* Transitions between normal/max/fullscreen may entail a frame change. */
    if ((state ^ surface->current.state) &
        (WAYLAND_SURFACE_CONFIG_STATE_MAXIMIZED |
         WAYLAND_SURFACE_CONFIG_STATE_FULLSCREEN))
    {
        flags |= SWP_FRAMECHANGED;
    }
    else if (surface->processing.decor != surface->current.decor) flags |= SWP_FRAMECHANGED;

    /* If the window is already fullscreen and its size is compatible with what
     * the compositor is requesting, don't force a resize, since some applications
     * are very insistent on a particular fullscreen size (which may not match
     * the monitor size). */
    if ((surface->window.state & WAYLAND_SURFACE_CONFIG_STATE_FULLSCREEN) &&
        wayland_surface_config_is_compatible(&surface->processing, surface->window.rect,
                                             surface->window.state))
    {
        flags |= SWP_NOSIZE;
    }

    /* Detect a restore from an application-initiated minimize: the last
     * requested config placed the window at the offscreen sentinel position
     * with WS_MINIMIZE, and the compositor is now sending a configure. Ack
     * the configure to avoid a protocol violation and send SC_RESTORE so
     * Win32 runs the full restore sequence (clearing WS_MINIMIZE, restoring
     * position/size, sending WM_SIZE, etc.), which triggers a new configure
     * cycle. */
    restoring_from_minimize = surface->window.rect.left <= -32000 &&
                              surface->window.rect.top  <= -32000 &&
                              surface->window.minimized;
    if (restoring_from_minimize)
    {
        TRACE("hwnd=%p restoring from minimize\n", hwnd);
        surface->current = surface->processing;
        memset(&surface->processing, 0, sizeof(surface->processing));
        xdg_surface_ack_configure(surface->xdg_surface,
                                  surface->current.serial);
        wayland_win_data_release(data);
        send_message(hwnd, WM_SYSCOMMAND, SC_RESTORE, 0);
        return;
    }

    SetRect(&rect, 0, 0, width, height);
    OffsetRect(&rect, data->rects.window.left, data->rects.window.top);
    if (!IsRectEmpty(&rect)) rect = window_rect_from_visible(&data->rects, rect);

    wayland_win_data_release(data);

    TRACE("processing=%dx%d,%#x\n", width, height, state);

    if (needs_enter_size_move) send_message(hwnd, WM_ENTERSIZEMOVE, 0, 0);
    if (needs_exit_size_move) send_message(hwnd, WM_EXITSIZEMOVE, 0, 0);

    flags |= SWP_NOACTIVATE | SWP_NOZORDER | SWP_NOOWNERZORDER | SWP_NOMOVE;
    if (IsRectEmpty(&rect)) flags |= SWP_NOSIZE;

    style = NtUserGetWindowLongW(hwnd, GWL_STYLE);
    if (!(state & WAYLAND_SURFACE_CONFIG_STATE_MAXIMIZED) != !(style & WS_MAXIMIZE)
        && !(state & WAYLAND_SURFACE_CONFIG_STATE_FULLSCREEN))
        NtUserSetWindowLong(hwnd, GWL_STYLE, style ^ WS_MAXIMIZE, FALSE);

    /* The Wayland maximized and fullscreen states are very strict about
     * surface size, so don't let the application override it. The tiled state
     * is not as strict, but it indicates a strong size preference, so try to
     * respect it. */
    if (state & (WAYLAND_SURFACE_CONFIG_STATE_MAXIMIZED |
                 WAYLAND_SURFACE_CONFIG_STATE_FULLSCREEN |
                 WAYLAND_SURFACE_CONFIG_STATE_TILED))
    {
        flags |= SWP_NOSENDCHANGING;
    }

    /* A maximized window lies on a monitor, which applications rely on, so
     * center it on the nearest one instead of keeping its old position. */
    if ((state & WAYLAND_SURFACE_CONFIG_STATE_MAXIMIZED) && !(flags & SWP_NOSIZE))
    {
        int window_width = rect.right - rect.left, window_height = rect.bottom - rect.top;
        HMONITOR hmonitor;
        MONITORINFO mi;

        mi.cbSize = sizeof(mi);
        if ((hmonitor = NtUserMonitorFromRect(&rect, MONITOR_DEFAULTTONEAREST)) &&
            NtUserGetMonitorInfo(hmonitor, &mi))
        {
            OffsetRect(&rect,
                       mi.rcWork.left + (mi.rcWork.right - mi.rcWork.left - window_width) / 2 - rect.left,
                       mi.rcWork.top + (mi.rcWork.bottom - mi.rcWork.top - window_height) / 2 - rect.top);
            flags &= ~SWP_NOMOVE;
        }
    }

    NtUserSetRawWindowPos(hwnd, rect, flags, FALSE);
}

/**********************************************************************
 *           WAYLAND_WindowMessage
 */
LRESULT WAYLAND_WindowMessage(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    switch (msg)
    {
    case WM_WAYLAND_INIT_DISPLAY_DEVICES:
        NtUserCallNoParam(NtUserCallNoParam_DisplayModeChanged);
        return 0;
    case WM_WAYLAND_CONFIGURE:
        wayland_configure_window(hwnd);
        return 0;
    case WM_WAYLAND_ACTIVATE_FOREIGN:
        wayland_surface_activate_from_token(hwnd);
        return 0;
    case WM_WAYLAND_FOREIGN_PARENT_CHANGED:
        wayland_refresh_foreign_child(hwnd, (HWND)wp);
        return 0;
    case WM_WAYLAND_EXPOSE_SURFACE:
    {
        struct wayland_win_data *data;

        /* no lock held here: the expose flushes the window surface, which commits its contents */
        NtUserExposeWindowSurface(hwnd, 0, NULL);
        if ((data = wayland_win_data_get(hwnd)))
        {
            if (data->wayland_surface) data->wayland_surface->expose_posted = FALSE;
            wayland_win_data_release(data);
        }
        return 0;
    }
    default:
        FIXME("got window msg %x hwnd %p wp %lx lp %lx\n", msg, hwnd, (long)wp, lp);
        return 0;
    }
}

/**********************************************************************
 *           WAYLAND_DesktopWindowProc
 */
LRESULT WAYLAND_DesktopWindowProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    return NtUserMessageCall(hwnd, msg, wp, lp, 0, NtUserDefWindowProc, FALSE);
}

/*****************************************************************
 *		WAYLAND_SetLayeredWindowAttributes
 */
void WAYLAND_SetLayeredWindowAttributes(HWND hwnd, COLORREF key, BYTE alpha, DWORD flags)
{
    struct wayland_win_data *data;
    struct wayland_surface *surface;

    TRACE("hwnd=%p key=%x alpha=%u flags=%x\n", hwnd, key, alpha, flags);

    if (!(data = wayland_win_data_get(hwnd))) return;

    if ((surface = data->wayland_surface))
        wayland_surface_set_opacity(surface, alpha, flags);
    data->layered_attribs_set = TRUE;

    wayland_win_data_release(data);
}

static enum xdg_toplevel_resize_edge hittest_to_resize_edge(WPARAM hittest)
{
    switch (hittest)
    {
    case WMSZ_LEFT:        return XDG_TOPLEVEL_RESIZE_EDGE_LEFT;
    case WMSZ_RIGHT:       return XDG_TOPLEVEL_RESIZE_EDGE_RIGHT;
    case WMSZ_TOP:         return XDG_TOPLEVEL_RESIZE_EDGE_TOP;
    case WMSZ_TOPLEFT:     return XDG_TOPLEVEL_RESIZE_EDGE_TOP_LEFT;
    case WMSZ_TOPRIGHT:    return XDG_TOPLEVEL_RESIZE_EDGE_TOP_RIGHT;
    case WMSZ_BOTTOM:      return XDG_TOPLEVEL_RESIZE_EDGE_BOTTOM;
    case WMSZ_BOTTOMLEFT:  return XDG_TOPLEVEL_RESIZE_EDGE_BOTTOM_LEFT;
    case WMSZ_BOTTOMRIGHT: return XDG_TOPLEVEL_RESIZE_EDGE_BOTTOM_RIGHT;
    default:               return XDG_TOPLEVEL_RESIZE_EDGE_NONE;
    }
}

/*****************************************************************
 *		WAYLAND_SetWindowIcons
 */
void WAYLAND_SetWindowIcons(HWND hwnd, HICON icon, const ICONINFO *ii, HICON icon_small, const ICONINFO *ii_small)
{
    struct wayland_surface *surface;
    struct wayland_win_data *data;

    if (!process_wayland.xdg_toplevel_icon_manager_v1) return;

    TRACE("hwnd=%p icon=%p ii=%p icon_small=%p ii_small=%p\n", hwnd, icon, ii, icon_small, ii_small);

    if (!(data = wayland_win_data_get(hwnd))) return;

    if ((surface = data->wayland_surface))
    {
        wayland_surface_set_icon_buffer(surface, ICON_BIG, ii);
        if (icon_small) wayland_surface_set_icon_buffer(surface, ICON_SMALL, ii_small);
        if (wayland_surface_is_toplevel(surface)) wayland_surface_assign_icon(surface);
    }

    wayland_win_data_release(data);
}

/***********************************************************************
 *		WAYLAND_SetWindowStyle
 */
void WAYLAND_SetWindowStyle(HWND hwnd, INT offset, STYLESTRUCT *style)
{
    struct wayland_win_data *data;
    struct wayland_surface *surface;
    DWORD changed = style->styleNew ^ style->styleOld;

    TRACE("hwnd=%p offset=%d\n", hwnd, offset);

    if (hwnd == NtUserGetDesktopWindow()) return;
    if (!(data = wayland_win_data_get(hwnd))) return;

    /* Changing WS_EX_LAYERED resets attributes */
    if (offset == GWL_EXSTYLE && (changed & WS_EX_LAYERED))
    {
        if ((surface = data->wayland_surface))
            wayland_surface_set_opacity(surface, 0, 0);
        data->layered_attribs_set = FALSE;
    }

    wayland_win_data_release(data);
}

/*****************************************************************
 *		WAYLAND_SetWindowText
 */
void WAYLAND_SetWindowText(HWND hwnd, LPCWSTR text)
{
    struct wayland_surface *surface;
    struct wayland_win_data *data;

    TRACE("hwnd=%p text=%s\n", hwnd, wine_dbgstr_w(text));

    if ((data = wayland_win_data_get(hwnd)))
    {
        if ((surface = data->wayland_surface) && wayland_surface_is_toplevel(surface))
            wayland_surface_set_title(surface, text);
        wayland_win_data_release(data);
    }
}

/***********************************************************************
 *          WAYLAND_SysCommand
 */
LRESULT WAYLAND_SysCommand(HWND hwnd, WPARAM wparam, LPARAM lparam, const POINT *pos)
{
    LRESULT ret = -1;
    WPARAM command = wparam & 0xfff0;
    uint32_t button_serial;
    struct wl_seat *wl_seat;
    struct wayland_surface *surface;
    struct wayland_win_data *data;

    TRACE("cmd=%lx hwnd=%p, %lx, %lx\n",
          (long)command, hwnd, (long)wparam, lparam);

    pthread_mutex_lock(&process_wayland.pointer.mutex);
    if (process_wayland.pointer.focused_hwnd == hwnd)
        button_serial = process_wayland.pointer.button_serial;
    else
        button_serial = 0;
    pthread_mutex_unlock(&process_wayland.pointer.mutex);

    if (command == SC_MOVE || command == SC_SIZE)
    {
        if ((data = wayland_win_data_get(hwnd)))
        {
            pthread_mutex_lock(&process_wayland.seat.mutex);
            wl_seat = process_wayland.seat.wl_seat;
            if (wl_seat && (surface = data->wayland_surface) &&
                wayland_surface_is_toplevel(surface) && button_serial)
            {
                if (command == SC_MOVE)
                {
                    xdg_toplevel_move(surface->xdg_toplevel, wl_seat, button_serial);
                }
                else if (command == SC_SIZE)
                {
                    xdg_toplevel_resize(surface->xdg_toplevel, wl_seat, button_serial,
                                        hittest_to_resize_edge(wparam & 0x0f));
                }
            }
            pthread_mutex_unlock(&process_wayland.seat.mutex);
            wayland_win_data_release(data);
            ret = 0;
        }
    }

    wl_display_flush(process_wayland.wl_display);
    return ret;
}

/***********************************************************************
 *          WAYLAND_UpdateLayeredWindow
 */
void WAYLAND_UpdateLayeredWindow(HWND hwnd, BYTE alpha, UINT flags)
{
    struct wayland_win_data *data;
    struct wayland_surface *surface;

    TRACE("hwnd=%p alpha=%u flags=%x\n", hwnd, alpha, flags);

    if (!(data = wayland_win_data_get(hwnd))) return;

    if ((surface = data->wayland_surface))
        wayland_surface_set_opacity(surface, alpha, flags);

    wayland_win_data_release(data);
}

/***********************************************************************
 *          WAYLAND_HasWindowManager
 */
BOOL WAYLAND_HasWindowManager(const char *name)
{
    static int once;
    const char *env = getenv("XDG_CURRENT_DESKTOP");

    if (!once++) TRACE("DE: %s\n", debugstr_a(env));

    if (!strcmp("waylanddrv", name)) return TRUE;
    if (env && !strcmp(env, name)) return TRUE;

    return FALSE;
}

/***********************************************************************
 *          WAYLAND_GetWindowStyleMasks
 */
BOOL WAYLAND_GetWindowStyleMasks(HWND hwnd, UINT style, UINT ex_style, UINT *style_mask, UINT *ex_style_mask)
{
    struct wayland_win_data *data;
    struct wayland_surface *surface;
    BOOL refused = FALSE;

    *style_mask = 0;
    *ex_style_mask = 0;

    if (!process_wayland.zxdg_decoration_manager_v1) return FALSE;
    if (!(data = wayland_win_data_get(hwnd))) return FALSE;

    if ((surface = data->wayland_surface) && wayland_surface_is_toplevel(surface))
        refused = !data->managed || surface->server_decor_refused;
    wayland_win_data_release(data);

    if (refused) return FALSE;
    if (!is_decoration_enabled(style, ex_style)) return FALSE;

    *style_mask |= WS_CAPTION | WS_DLGFRAME | WS_THICKFRAME;
    *ex_style_mask |= WS_EX_DLGMODALFRAME;
    return TRUE;
}

/**********************************************************************
 *          WAYLAND_FlashWindowEx
 */
void WAYLAND_FlashWindowEx(FLASHWINFO *info)
{
    struct wayland_win_data *data;

    TRACE("hwnd %p flags %u\n", info->hwnd, info->dwFlags);

    if ((data = wayland_win_data_get(info->hwnd)))
    {
        if (data->wayland_surface && info->dwFlags)
            wayland_surface_flash_window(data->wayland_surface);
        wayland_win_data_release(data);
    }
}

/* Output-follow: moves a window that is on none of the monitors of the outputs its surface
 * entered to the first of them. Called with no lock held. */
static void wayland_follow_output(HWND hwnd, UINT *swp_flags, RECT *rect)
{
    struct wayland_output *outputs[16];
    struct surface_output_entry *entry;
    struct wayland_win_data *data;
    struct wayland_surface *surface;
    RECT window_rect = {0}, monitor, target = {0}, tmp;
    BOOL intersects = FALSE, have_target = FALSE;
    UINT count = 0, i;

    if (!(data = wayland_win_data_get(hwnd))) return;
    if ((surface = data->wayland_surface) && wayland_surface_is_toplevel(surface) &&
        !(surface->window.state & WAYLAND_SURFACE_CONFIG_STATE_FULLSCREEN))
    {
        window_rect = surface->window.rect;
        wl_list_for_each(entry, &surface->output_list, entry)
        {
            if (count == ARRAY_SIZE(outputs)) break;
            wayland_output_add_ref(entry->output);
            outputs[count++] = entry->output;
        }
    }
    wayland_win_data_release(data);

    for (i = 0; i < count; i++)
    {
        if (!wayland_output_get_monitor_rect(outputs[i], &monitor)) continue;
        if (intersect_rect(&tmp, &monitor, &window_rect))
        {
            intersects = TRUE;
            break;
        }
        if (!have_target)
        {
            target = monitor;
            have_target = TRUE;
        }
    }
    for (i = 0; i < count; i++) wayland_output_release(outputs[i]);

    if (!intersects && have_target)
    {
        *swp_flags = SWP_NOSIZE | SWP_NOOWNERZORDER | SWP_NOZORDER | SWP_NOACTIVATE;
        SetRect(rect, target.left, target.top, target.left + 1, target.top + 1);
        TRACE("Moving hwnd=%p to %s\n", hwnd, wine_dbgstr_rect(rect));
    }
}

/***********************************************************************
 *           WAYLAND_GetWindowStateUpdates
 */
BOOL WAYLAND_GetWindowStateUpdates(HWND hwnd, UINT *state_cmd, UINT *swp_flags, RECT *rect, HWND *foreground)
{
    HWND focused, old;
    DWORD style;

    /* win32u calls with NULL pointers to unlock host state, which this driver does not have */
    if (!foreground) return FALSE;

    style = NtUserGetWindowLongW(hwnd, GWL_STYLE);
    if (!(style & WS_VISIBLE) || (style & (WS_CHILD | WS_POPUP)) == WS_CHILD || (style & WS_DISABLED))
        return FALSE;

    focused = wayland_keyboard_get_focused_hwnd();
    old = NtUserGetForegroundWindow();
    /* the compositor gave the keyboard focus to another client: the desktop window stands for that */
    if (!focused && old == hwnd) focused = NtUserGetDesktopWindow();
    else if (focused != hwnd) focused = NULL;

    if (old != focused) *foreground = focused;

    if ((style & WS_MINIMIZE) && focused == hwnd && old != hwnd)
        *state_cmd = MAKELONG(SC_RESTORE, 0);
    else if (!(style & WS_MINIMIZE))
        wayland_follow_output(hwnd, swp_flags, rect);

    TRACE("hwnd=%p foreground=%p state=%#x\n", hwnd, *foreground, *state_cmd);
    return TRUE;
}

/***********************************************************************
 *           WAYLAND_ActivateWindow
 */
void WAYLAND_ActivateWindow(HWND hwnd, HWND previous)
{
    struct wayland_win_data *data;

    TRACE("hwnd=%p previous=%p\n", hwnd, previous);

    if (hwnd == previous) return;
    if (!(data = wayland_win_data_get(hwnd))) return;
    if (data->wayland_surface) wayland_surface_activate(data->wayland_surface);
    wayland_win_data_release(data);
}

void set_client_surface(HWND hwnd, struct wayland_client_surface *new_client)
{
    HWND toplevel = new_client->client.toplevel;
    RECT rect = new_client->client.monitor_rect;
    struct wayland_client_surface *old_client;
    struct wayland_win_data *data;
    BOOL visible = FALSE, foreign = FALSE;
    RECT child_rect = {0}, present_rect;
    DWORD pid;

    /* ownership is shared with the callers, the last caller to release
     * its reference will also destroy it and clear our pointer. */
    wayland_client_surface_update_offscreen(new_client);
    if (toplevel) visible = !new_client->client.offscreen && (NtUserIsWindowVisible(hwnd) || NtUserGetPresentRect(hwnd, &present_rect, 0));

    /* NtUser queries, before win_data is locked. visible keeps the tree's offscreen test, so an offscreen client is not attached, as in the tree's attach. */
    if (toplevel && toplevel != hwnd && NtUserGetWindowThread(toplevel, &pid) && pid != GetCurrentProcessId())
    {
        foreign = TRUE;
        if (visible) child_rect = foreign_child_monitor_rect(hwnd);
    }
    if (!(data = wayland_win_data_get(hwnd))) return;

    if (new_client != data->client_surface)
    {
        if ((old_client = data->client_surface))
            wayland_client_surface_attach(old_client, NULL, NULL);

        if ((data->client_surface = new_client))
        {
            /* a foreign child stays on its own surface, as update attached it (P3-H9) */
            if (!(foreign && visible && new_client->toplevel == hwnd))
            {
                if (toplevel && visible)
                    wayland_client_surface_attach(new_client, toplevel, &rect);
                else
                    wayland_client_surface_attach(new_client, NULL, NULL);
            }
        }
    }

    /* a foreign child with no attached client gets its own TOPLEVEL and attaches to it (patch 970-972) */
    if (foreign && visible && !new_client->toplevel)
    {
        if (!data->wayland_surface && wayland_win_data_create_wayland_surface(data, NULL))
            wayland_win_data_update_wayland_state(data);
        if (data->wayland_surface)
            wayland_client_surface_attach(new_client, hwnd, &child_rect);
    }

    wayland_win_data_release(data);

    if (toplevel) wayland_restack_client_surfaces(toplevel);
}

BOOL set_window_surface_contents(HWND hwnd, struct wayland_shm_buffer *shm_buffer, HRGN damage_region)
{
    struct wayland_surface *wayland_surface;
    struct wayland_win_data *data;
    BOOL committed = FALSE;
    /* a user lock query, so it runs before win_data is locked */
    BOOL border_effect = is_mso_border_effect_window(hwnd);
    RECT box, *damage_box = NULL;

    /* the region box is a GDI query, so it is read before win_data is locked */
    if (damage_region && NtGdiGetRgnBox(damage_region, &box) >= NULLREGION) damage_box = &box;

    if (!(data = wayland_win_data_get(hwnd))) return FALSE;

    if ((wayland_surface = data->wayland_surface))
    {
        if (wayland_surface_reconfigure(wayland_surface))
        {
            wayland_update_overlays(data, shm_buffer, damage_box);
            wayland_surface_attach_shm(wayland_surface, shm_buffer, damage_region);
            wl_surface_commit(wayland_surface->wl_surface);
            wayland_surface->ensured_contents = WAYLAND_SURFACE_ENSURED_FLUSH;
            wayland_surface->expose_posted = FALSE;
            committed = TRUE;
        }
        else
        {
            TRACE("Wayland surface not configured yet, not flushing\n");
        }
    }
    else if (border_effect)
    {
        /* no wayland surface is built for these, so there is nothing to flush */
        committed = TRUE;
    }

    /* Update the latest window buffer for the wayland surface. Note that we
     * only care whether the buffer contains the latest window contents,
     * it's irrelevant if it was actually committed or not. */
    if (data->window_contents)
        wayland_shm_buffer_unref(data->window_contents);
    wayland_shm_buffer_ref((data->window_contents = shm_buffer));

    wayland_win_data_release(data);

    return committed;
}

struct wayland_shm_buffer *get_window_surface_contents(HWND hwnd)
{
    struct wayland_shm_buffer *shm_buffer;
    struct wayland_win_data *data;

    if (!(data = wayland_win_data_get(hwnd))) return NULL;
    if ((shm_buffer = data->window_contents)) wayland_shm_buffer_ref(shm_buffer);
    wayland_win_data_release(data);

    return shm_buffer;
}

void wayland_window_init(void)
{
    pthread_mutexattr_t attr;

    pthread_mutexattr_init(&attr);
    pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
    pthread_mutex_init(&win_data_mutex, &attr);
    pthread_mutexattr_destroy(&attr);
}
