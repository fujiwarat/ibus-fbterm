/* -*- mode: C; c-basic-offset: 4; indent-tabs-mode: nil; -*- */
/* vim:set et sts=4: */
/*
 * Copyright (C) 2015 Takao Fujiwara <takao.fujiwara1@gmail.com>
 * Copyright (C) 2015 Red Hat, Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#include <glib.h>

#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <sys/epoll.h>
#include <time.h>

#include "fbio.h"

#define NR_FDS 32
#define NR_EPOLL_FDS 10

enum {
    PROP_0 = 0,
    PROP_DISPATCHER
};

struct _FbIoPrivate {
    FbIoDispatcher *dispatcher;
    int             fd;
    void           *coded_read;
    void           *coded_write;
    gchar           buffer_read[16];
    gchar           buffer_write[16];
    int             buffer_read_length;
    int             buffer_write_length;
};

struct _FbIoDispatcherPrivate {
    FbIo  *io_map[NR_FDS];
    int    epoll_fd;
};

G_DEFINE_TYPE_WITH_PRIVATE (FbIo,
                            fb_io,
                            IBUS_TYPE_OBJECT);
G_DEFINE_TYPE_WITH_PRIVATE (FbIoDispatcher,
                            fb_io_dispatcher,
                            IBUS_TYPE_OBJECT);

static void         fb_io_destroy            (FbIo       *io);
static void         fb_io_get_property       (FbIo       *io,
                                              guint       prop_id,
                                              GValue     *value,
                                              GParamSpec *pspec);
static void         fb_io_set_property       (FbIo         *io,
                                              guint         prop_id,
                                              const GValue *value,
                                              GParamSpec   *pspec);
static void         fb_io_real_ready_read    (FbIo         *io,
                                              const gchar  *buff,
                                              guint         length);
static void         fb_io_write_io           (FbIo *io,
                                              const gchar  *buff,
                                              guint         length);

static void         fb_io_dispatcher_destroy (FbIoDispatcher *io);

static void
fb_io_init (FbIo *io)
{
    FbIoPrivate *priv =
            fb_io_get_instance_private (io);
    io->priv = priv;

    priv->fd = -1;
    priv->buffer_read_length = 0;
    priv->buffer_write_length = 0;
}

static void
fb_io_class_init (FbIoClass *class)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS (class);
    IBUS_OBJECT_CLASS (class)->destroy = (IBusObjectDestroyFunc)fb_io_destroy;
    class->ready_read = fb_io_real_ready_read;

    gobject_class->get_property = (GObjectGetPropertyFunc)fb_io_get_property;
    gobject_class->set_property = (GObjectSetPropertyFunc)fb_io_set_property;

    /* install properties */
    /**
     * FbIo:dispatcher:
     *
     * The object of FbIoDispatcher
     */
    g_object_class_install_property (gobject_class,
            PROP_DISPATCHER,
            g_param_spec_object ("dispatcher",
                                 "dispatcher",
                                 "The object of FbIoDispatcher",
                                 FB_TYPE_IO_DISPATCHER,
                                 G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));

}

static void
fb_io_destroy (FbIo *io)
{
    FbIoPrivate *priv;

    g_return_if_fail (FB_IS_IO (io));
    priv = io->priv;

    if (priv->fd != -1 && priv->dispatcher) {
        fb_io_dispatcher_remove_io_source (priv->dispatcher, io, TRUE);
        close (priv->fd);
    }
}

static void
fb_io_get_property (FbIo       *io,
                    guint       prop_id,
                    GValue     *value,
                    GParamSpec *pspec)
{
    FbIoPrivate *priv = io->priv;

    switch (prop_id) {
    case PROP_DISPATCHER:
        g_value_set_object (value, priv->dispatcher);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (io, prop_id, pspec);
    }
}

static void
fb_io_set_property (FbIo         *io,
                    guint         prop_id,
                    const GValue *value,
                    GParamSpec   *pspec)
{
    FbIoPrivate *priv = io->priv;

    switch (prop_id) {
    case PROP_DISPATCHER: {
        FbIoDispatcher *dispatcher = g_value_get_object (value);
        g_return_if_fail (FB_IS_IO_DISPATCHER (dispatcher));
        priv->dispatcher = g_object_ref_sink (dispatcher);
        break;
    }
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (io, prop_id, pspec);
    }
}

static void
fb_io_translate (FbIo *io,
                 gboolean isread,
                 const gchar *buff,
                 guint length)
{
    if (!buff || !length)
        return;

    if (isread)
        FB_IO_GET_CLASS (io)->ready_read (io, buff, length);
    else
        fb_io_write_io (io, buff, length);
}

static void
fb_io_real_ready_read (FbIo        *io,
                       const gchar *buff,
                       guint        length)
{
}

static void
fb_io_write_io (FbIo        *io,
                const gchar *buff,
                guint        length)
{
    int num = 5;
    FbIoPrivate *priv;

    g_return_if_fail (FB_IS_IO (io));
    priv = io->priv;

    while (length) {
        int retval = write (priv->fd, buff, length);
        if (retval == -1) {
            if (errno == EAGAIN) {
                if (num--) {
                    struct timespec tm = { 0, 200 * 1000000UL };
                    nanosleep (&tm, 0);
                } else {
                    break;
                }
            } else {
                if (errno != EINTR) {
                    //
                }
                break;
            }
        } else if (retval > 0) {
            buff += retval;
            length -= retval;
        }
    }
}

static void
fb_io_dispatcher_init (FbIoDispatcher *dispatcher)
{
    FbIoDispatcherPrivate *priv =
            fb_io_dispatcher_get_instance_private (dispatcher);
    dispatcher->priv = priv;

    priv->epoll_fd = epoll_create (NR_EPOLL_FDS);
    fcntl (priv->epoll_fd,
           F_SETFD,
           fcntl (priv->epoll_fd, F_GETFD) | FD_CLOEXEC);
}

static void
fb_io_dispatcher_class_init (FbIoDispatcherClass *class)
{
    IBUS_OBJECT_CLASS (class)->destroy =
            (IBusObjectDestroyFunc)fb_io_dispatcher_destroy;
}

static void
fb_io_dispatcher_destroy (FbIoDispatcher *io)
{
    FbIoDispatcherPrivate *priv;
    int i;

    g_return_if_fail (FB_IS_IO_DISPATCHER (io));
    priv = FB_IO_DISPATCHER (io)->priv;

    for (i = NR_FDS; i--;) {
        if (priv->io_map[i]) {
            g_object_unref (priv->io_map[i]);
            priv->io_map[i] = NULL;
        }
    }

    close (priv->epoll_fd);
}

FbIo *
fb_io_new (FbIoDispatcher *dispatcher)
{
    return g_object_new (FB_TYPE_IO,
                         "dispatcher", dispatcher,
                         NULL);
}

int
fb_io_get_fd (FbIo *io)
{
    g_return_if_fail (FB_IS_IO (io));
    return io->priv->fd;
}

void
fb_io_set_fd (FbIo *io, int fd)
{
    FbIoPrivate *priv;

    g_return_if_fail (FB_IS_IO (io));
    priv = io->priv;
    if (priv->fd == fd)
        return;
    if (priv->fd != -1 && priv->dispatcher) {
        fb_io_dispatcher_remove_io_source (priv->dispatcher, io, TRUE);
        close (priv->fd);
    }

    priv->fd = fd;

    if (fd == -1)
        return;

    fcntl (fd, F_SETFD, fcntl (fd, F_GETFD) | FD_CLOEXEC);
    fcntl (fd, F_SETFL, fcntl (fd, F_GETFL) | O_NONBLOCK);

    if (priv->dispatcher)
        fb_io_dispatcher_add_io_source (priv->dispatcher, io, TRUE);
}

void
fb_io_ready (FbIo *io, gboolean isread)
{
#define BUFF_SIZE 10240

    FbIoPrivate *priv;
    gchar buff[BUFF_SIZE];
    int length;

    g_return_if_fail (FB_IS_IO (io));
    priv = io->priv;

    if (!isread)
        return;
    length = read (priv->fd,
                   buff + priv->buffer_read_length,
                   sizeof (buff) - priv->buffer_read_length);
    if (length <= 0) {
        return;
    }
    if (priv->buffer_read_length) {
        memcpy (buff, priv->buffer_read, priv->buffer_read_length);
        length += priv->buffer_read_length;
        priv->buffer_read_length = 0;
    }
    fb_io_translate (io, TRUE, buff, length);
}


void
fb_io_write (FbIo        *io,
             const gchar *buff,
             guint        length)
{
    g_return_if_fail (FB_IS_IO (io));
    fb_io_translate (io, FALSE, buff, length);
}

FbIoDispatcher *
fb_io_dispatcher_new ()
{
    return g_object_new (FB_TYPE_IO_DISPATCHER, NULL);
}

void
fb_io_dispatcher_add_io_source (FbIoDispatcher *dispatcher,
                                FbIo           *io,
                                gboolean        isread)
{
    FbIoDispatcherPrivate *priv;
    int fd;
    struct epoll_event ev;

    g_return_if_fail (FB_IS_IO_DISPATCHER (dispatcher));
    priv = dispatcher->priv;

    fd = fb_io_get_fd (io);
    if (fd >= NR_FDS)
        return;
    priv->io_map[fd] = io;
    ev.data.fd = fd;
    ev.events = (isread ? EPOLLIN : EPOLLOUT);
    epoll_ctl (priv->epoll_fd, EPOLL_CTL_ADD, fd, &ev);
}

void
fb_io_dispatcher_remove_io_source (FbIoDispatcher *dispatcher,
                                   FbIo           *io,
                                   gboolean        isread)
{
    FbIoDispatcherPrivate *priv;
    int fd;
    struct epoll_event ev;

    g_return_if_fail (FB_IS_IO_DISPATCHER (dispatcher));
    priv = dispatcher->priv;

    fd = fb_io_get_fd (io);
    if (fd >= NR_FDS)
        return;
    priv->io_map[fd] = io;
    ev.data.fd = fd;
    ev.events = (isread ? EPOLLIN : EPOLLOUT);
    epoll_ctl (priv->epoll_fd, EPOLL_CTL_DEL, fd, &ev);
}

void
fb_io_dispatcher_poll (FbIoDispatcher *dispatcher)
{
    FbIoDispatcherPrivate *priv;
    struct epoll_event evs[NR_EPOLL_FDS];
    int i, nfds;

    g_return_if_fail (FB_IS_IO_DISPATCHER (dispatcher));
    priv = dispatcher->priv;
    nfds = epoll_wait (priv->epoll_fd, evs, NR_EPOLL_FDS, -1);

    for (i = 0; i < nfds; i++) {
        FbIo *io = priv->io_map[evs[i].data.fd];
        if (!io)
            continue;
        if (evs[i].events & EPOLLIN)
            fb_io_ready (io, TRUE);
        if (evs[i].events & EPOLLOUT)
            fb_io_ready (io, FALSE);
        if (evs[i].events & EPOLLHUP) {
            g_object_unref (io);
            priv->io_map[evs[i].data.fd] = NULL;
        }
    }
}
