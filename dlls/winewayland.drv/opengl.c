/*
 * Wayland OpenGL functions
 *
 * Copyright 2020 Alexandros Frantzis for Collabora Ltd.
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
#include <dlfcn.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>

#include "ntstatus.h"
#include "waylanddrv.h"
#include "wine/debug.h"

#ifdef HAVE_LIBWAYLAND_EGL

WINE_DEFAULT_DEBUG_CHANNEL(waylanddrv);

#include <wayland-egl.h>

#include "wine/opengl_driver.h"

static const struct egl_platform *egl;
static const struct opengl_funcs *funcs;
static const struct opengl_drawable_funcs wayland_drawable_funcs;
static BOOL (*p_egl_describe_pixel_format)( int format, struct wgl_pixel_format *pf );

struct wayland_gl_drawable
{
    struct opengl_drawable base;
    struct wl_egl_window *wl_egl_window;
    BOOL interval_applied;
};

/* Private data of a GL frame callback, which belongs to a client surface, not a window. */
struct wayland_gl_frame
{
    struct wayland_client_surface *surface;
};

/* Leaf lock guarding surface->gl_frame, every wayland_gl_frame.surface, surface->throttle_stalled
 * and the callback lifetime. No driver lock is taken while it is held. */
static pthread_mutex_t gl_throttle_mutex = PTHREAD_MUTEX_INITIALIZER;

static struct wayland_gl_drawable *impl_from_opengl_drawable(struct opengl_drawable *base)
{
    return CONTAINING_RECORD(base, struct wayland_gl_drawable, base);
}

static void wayland_drawable_destroy(struct opengl_drawable *base)
{
    struct wayland_gl_drawable *gl = impl_from_opengl_drawable(base);
    if (gl->wl_egl_window) wl_egl_window_destroy(gl->wl_egl_window);
}

static EGLConfig egl_config_for_format(int format)
{
    return egl->configs[(format - 1) % egl->config_count];
}

static BOOL wayland_opengl_surface_create(struct client_surface *client, int format, struct opengl_drawable **drawable)
{
    struct wayland_client_surface *surface = impl_from_client_surface(client);
    EGLConfig config = egl_config_for_format(format);
    EGLint attribs[5], *attrib = attribs;
    struct wayland_gl_drawable *gl;
    struct wgl_pixel_format desc;
    HWND hwnd = client->hwnd;
    SIZE size;
    NTSTATUS status;

    TRACE("client=%s format=%d\n", debugstr_client_surface(client), format);

    p_egl_describe_pixel_format( format, &desc );

    if (!egl->extensions[EGL_EXT_present_opaque])
        WARN("Missing EGL_EXT_present_opaque extension\n");
    else
    {
        *attrib++ = EGL_PRESENT_OPAQUE_EXT;
        *attrib++ = EGL_TRUE;
    }
    if (desc.framebuffer_srgb_capable)
    {
        *attrib++ = EGL_GL_COLORSPACE;
        *attrib++ = EGL_GL_COLORSPACE_SRGB;
    }
    *attrib++ = EGL_NONE;

    if (!(gl = opengl_drawable_create(&wayland_drawable_funcs, format, client, NULL))) return FALSE;
    size = client->raw ? gl->base.monitor_size : gl->base.virtual_size;

    opengl_drawable_map_buffer(&gl->base, GL_FRONT_LEFT, GL_BACK_LEFT);
    opengl_drawable_map_buffer(&gl->base, GL_FRONT, GL_BACK);
    opengl_drawable_map_buffer(&gl->base, GL_FRONT_AND_BACK, GL_BACK);
    if (gl->base.stereo) opengl_drawable_map_buffer(&gl->base, GL_FRONT_RIGHT, GL_BACK_RIGHT);

    /* starts signalled, so the first qualifying swap passes its wait */
    if (!surface->throttle &&
        (status = NtCreateEvent(&surface->throttle, EVENT_ALL_ACCESS, NULL, SynchronizationEvent, TRUE)))
    {
        WARN("Failed to create the GL throttle event, status %#x\n", (int)status);
        surface->throttle = NULL;
    }

    if (!(gl->wl_egl_window = wl_egl_window_create(surface->wl_surface, size.cx, size.cy))) goto err;
    if (!(gl->base.surface = funcs->p_eglCreateWindowSurface(egl->display, config, gl->wl_egl_window, attribs))) goto err;
    set_client_surface(hwnd, surface);

    TRACE("Created drawable %s with egl_surface %p\n", debugstr_opengl_drawable(&gl->base), gl->base.surface);

    *drawable = &gl->base;
    return TRUE;

err:
    opengl_drawable_release(&gl->base);
    return FALSE;
}

static void wayland_init_egl_platform(struct egl_platform *platform)
{
    platform->type = EGL_PLATFORM_WAYLAND_KHR;
    platform->native_display = process_wayland.wl_display;
    platform->force_pbuffer_formats = TRUE;
    egl = platform;
}

static void wayland_drawable_flush(struct opengl_drawable *base, UINT flags)
{
    struct wayland_gl_drawable *gl = impl_from_opengl_drawable(base);
    SIZE size = gl->base.client && gl->base.client->raw ? gl->base.monitor_size : gl->base.virtual_size;

    TRACE("drawable %s, flags %#x\n", debugstr_opengl_drawable(base), flags);

    /* the driver waits on the frame callback itself in swap, so EGL must not also block */
    if (flags & GL_FLUSH_INTERVAL) funcs->p_eglSwapInterval(egl->display, 0);

    /* Since context_flush is called from operations that may latch the native size,
     * perform any pending resizes before calling them. */
    if (flags & GL_FLUSH_UPDATED) wl_egl_window_resize(gl->wl_egl_window, size.cx, size.cy, 0, 0);
}

static void wayland_gl_frame_done(void *data, struct wl_callback *callback, uint32_t time)
{
    struct wayland_gl_frame *frame = data;

    pthread_mutex_lock(&gl_throttle_mutex);
    /* a NULL surface means the client surface was destroyed and detached this frame */
    if (frame->surface)
    {
        TRACE("hwnd=%p\n", frame->surface->client.hwnd);
        frame->surface->gl_frame = NULL;
        frame->surface->throttle_stalled = FALSE;
        if (frame->surface->throttle) NtSetEvent(frame->surface->throttle, NULL);
    }
    pthread_mutex_unlock(&gl_throttle_mutex);

    /* the only place a frame callback is destroyed */
    wl_callback_destroy(callback);
    free(frame);
}

static const struct wl_callback_listener gl_frame_listener =
{
    wayland_gl_frame_done,
};

void wayland_gl_client_surface_destroy(struct wayland_client_surface *surface)
{
    pthread_mutex_lock(&gl_throttle_mutex);
    if (surface->gl_frame)
    {
        /* the callback stays alive; its done handler frees the frame and the proxy */
        surface->gl_frame->surface = NULL;
        surface->gl_frame = NULL;
    }
    pthread_mutex_unlock(&gl_throttle_mutex);

    if (surface->throttle) NtClose(surface->throttle);
    surface->throttle = NULL;
}

static BOOL wayland_drawable_swap(struct opengl_drawable *base)
{
    struct wayland_gl_drawable *gl = impl_from_opengl_drawable(base);
    struct wayland_client_surface *surface = impl_from_client_surface(base->client);

    client_surface_present(base->client);

    if (!gl->interval_applied)
    {
        funcs->p_eglSwapInterval(egl->display, 0);
        gl->interval_applied = TRUE;
    }

    if (base->interval && surface->toplevel && surface->throttle)
    {
        /* 1 second, relative */
        const LARGE_INTEGER timeout = { .QuadPart = -10000 * 1000 };
        struct wayland_gl_frame *frame;
        struct wl_callback *callback;
        BOOL stalled;

        pthread_mutex_lock(&gl_throttle_mutex);
        stalled = surface->throttle_stalled;
        pthread_mutex_unlock(&gl_throttle_mutex);

        /* the wait runs with gl_throttle_mutex released, so the done handler can signal it */
        if (!stalled && NtWaitForSingleObject(surface->throttle, FALSE, &timeout) == STATUS_TIMEOUT)
        {
            WARN("Present timed out!\n");
            pthread_mutex_lock(&gl_throttle_mutex);
            surface->throttle_stalled = TRUE;
            pthread_mutex_unlock(&gl_throttle_mutex);
        }

        /* Currently, EGL is implemented using frame callbacks, so replicate that here.
         * The commit made by eglSwapBuffers applies the request. At most one request is
         * outstanding per client surface. */
        pthread_mutex_lock(&gl_throttle_mutex);
        if (!surface->gl_frame)
        {
            if (!(frame = calloc(1, sizeof(*frame))))
                NtSetEvent(surface->throttle, NULL);
            else if (!(callback = wl_surface_frame(surface->wl_surface)))
            {
                free(frame);
                NtSetEvent(surface->throttle, NULL);
            }
            else
            {
                frame->surface = surface;
                wl_callback_add_listener(callback, &gl_frame_listener, frame);
                surface->gl_frame = frame;
            }
        }
        pthread_mutex_unlock(&gl_throttle_mutex);
    }

    funcs->p_eglSwapBuffers(egl->display, gl->base.surface);

    return TRUE;
}

struct wayland_pbuffer
{
    struct opengl_drawable base;
    struct wl_surface *surface;
    struct wl_egl_window *window;
};

static struct wayland_pbuffer *pbuffer_from_opengl_drawable(struct opengl_drawable *base)
{
    return CONTAINING_RECORD(base, struct wayland_pbuffer, base);
}

static void wayland_pbuffer_destroy(struct opengl_drawable *base)
{
    struct wayland_pbuffer *gl = pbuffer_from_opengl_drawable(base);

    TRACE("%s\n", debugstr_opengl_drawable(base));

    if (gl->window)
        wl_egl_window_destroy(gl->window);
    if (gl->surface)
        wl_surface_destroy(gl->surface);
}

static const struct opengl_drawable_funcs wayland_pbuffer_funcs =
{
    .size = sizeof(struct wayland_pbuffer),
    .destroy = wayland_pbuffer_destroy,
};

static BOOL wayland_pbuffer_create(HDC hdc, int format, SIZE size, BOOL largest, GLenum texture_format, GLenum texture_target,
                                   GLint max_level, struct opengl_drawable **drawable)
{
    EGLConfig config = egl_config_for_format(format);
    struct wayland_pbuffer *gl;

    TRACE("hdc %p, format %d, size %s, largest %u, texture_format %#x, texture_target %#x, max_level %#x, drawable %p\n",
          hdc, format, wine_dbgstr_point((POINT *)&size), largest, texture_format, texture_target, max_level, drawable);

    if (!(gl = opengl_drawable_create(&wayland_pbuffer_funcs, format, NULL, &size))) return FALSE;
    /* Wayland EGL doesn't support pixmap or pbuffer, create a dummy window surface to act as the target render surface. */
    if (!(gl->surface = wl_compositor_create_surface(process_wayland.wl_compositor))) goto err;
    if (!(gl->window = wl_egl_window_create(gl->surface, size.cx, size.cy))) goto err;
    if (!(gl->base.surface = funcs->p_eglCreateWindowSurface(egl->display, config, gl->window, NULL))) goto err;

    TRACE("Created pbuffer %s with egl_surface %p\n", debugstr_opengl_drawable(&gl->base), gl->base.surface);
    *drawable = &gl->base;
    return TRUE;

err:
    opengl_drawable_release(&gl->base);
    return FALSE;
}

static struct opengl_driver_funcs wayland_driver_funcs =
{
    .p_init_egl_platform = wayland_init_egl_platform,
    .p_surface_create = wayland_opengl_surface_create,
    .p_pbuffer_create = wayland_pbuffer_create,
};

static const struct opengl_drawable_funcs wayland_drawable_funcs =
{
    .size = sizeof(struct wayland_gl_drawable),
    .destroy = wayland_drawable_destroy,
    .flush = wayland_drawable_flush,
    .swap = wayland_drawable_swap,
};

/**********************************************************************
 *           WAYLAND_OpenGLInit
 */
UINT WAYLAND_OpenGLInit(UINT version, const struct opengl_funcs *opengl_funcs, const struct opengl_driver_funcs **driver_funcs)
{
    if (version != WINE_OPENGL_DRIVER_VERSION)
    {
        ERR("Version mismatch, opengl32 wants %u but driver has %u\n",
            version, WINE_OPENGL_DRIVER_VERSION);
        return STATUS_INVALID_PARAMETER;
    }

    if (!opengl_funcs->egl_handle) return STATUS_NOT_SUPPORTED;
    funcs = opengl_funcs;

    wayland_driver_funcs.p_get_proc_address = (*driver_funcs)->p_get_proc_address;
    wayland_driver_funcs.p_init_pixel_formats = (*driver_funcs)->p_init_pixel_formats;
    wayland_driver_funcs.p_describe_pixel_format = (*driver_funcs)->p_describe_pixel_format;
    wayland_driver_funcs.p_init_extensions = (*driver_funcs)->p_init_extensions;
    wayland_driver_funcs.p_context_create = (*driver_funcs)->p_context_create;
    wayland_driver_funcs.p_context_destroy = (*driver_funcs)->p_context_destroy;
    wayland_driver_funcs.p_context_activate = (*driver_funcs)->p_context_activate;
    p_egl_describe_pixel_format = (*driver_funcs)->p_describe_pixel_format;

    *driver_funcs = &wayland_driver_funcs;
    return STATUS_SUCCESS;
}

#else /* No GL */

void wayland_gl_client_surface_destroy(struct wayland_client_surface *surface)
{
}

UINT WAYLAND_OpenGLInit(UINT version, const struct opengl_funcs *opengl_funcs, const struct opengl_driver_funcs **driver_funcs)
{
    return STATUS_NOT_IMPLEMENTED;
}

#endif
