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
#include <config.h>

#include <glib.h>

#include <errno.h>
#include <fcntl.h>
#include <string.h>

#include "fbio.h"

struct _FbIoPrivate {
    GIOChannel     *iochannel;
    int             fd;
    void           *coded_read;
    void           *coded_write;
    gchar           buffer_read[16];
    gchar           buffer_write[16];
    int             buffer_read_length;
    int             buffer_write_length;
};

G_DEFINE_TYPE_WITH_PRIVATE (FbIo,
                            fb_io,
                            IBUS_TYPE_OBJECT);

static void         fb_io_destroy            (FbIo       *io);
static void         fb_io_real_ready_read    (FbIo         *io,
                                              const gchar  *buff,
                                              guint         length);
static void         fb_io_write_io           (FbIo *io,
                                              const gchar  *buff,
                                              guint         length);

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
    //GObjectClass *gobject_class = G_OBJECT_CLASS (class);
    IBUS_OBJECT_CLASS (class)->destroy = (IBusObjectDestroyFunc)fb_io_destroy;
    class->ready_read = fb_io_real_ready_read;
}

static void
fb_io_destroy (FbIo *io)
{
    FbIoPrivate *priv;

    g_return_if_fail (FB_IS_IO (io));
    priv = io->priv;

    if (priv->iochannel) {
        g_io_channel_unref (priv->iochannel);
        priv->fd = - 1;
        priv->iochannel = NULL;
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
                    ;
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

static gboolean
fb_io_watch_cb (GIOChannel   *source,
                GIOCondition  condition,
                FbIo         *io)
{
    FbIoPrivate *priv;
    gboolean retval = FALSE;

    g_return_val_if_fail (FB_IS_IO (io), FALSE);

    priv = io->priv;

    if (condition & G_IO_IN) {
        fb_io_ready (io, TRUE);
        retval = TRUE;
    }
    if (condition & G_IO_OUT)
        fb_io_ready (io, FALSE);
    if (condition & G_IO_HUP) {
        g_object_unref (io);
        retval = TRUE;
    }
    if (condition & G_IO_ERR)
        g_warning ("FbIo Error: (%d) %s", priv->fd, g_strerror (errno));
    return retval;
}

FbIo *
fb_io_new (void)
{
    return g_object_new (FB_TYPE_IO,
                         NULL);
}

int
fb_io_get_fd (FbIo *io)
{
    g_return_if_fail (FB_IS_IO (io));
    return io->priv->fd;
}

void
fb_io_set_fd (FbIo *io,
              int   fd)
{
    FbIoPrivate *priv;
    GError *error = NULL;

    g_return_if_fail (FB_IS_IO (io));

    priv = io->priv;

    if (priv->iochannel) {
        if (priv->fd == fd)
            return;
        g_io_channel_unref (priv->iochannel);
    }

    if (fd == -1) {
        priv->fd = -1;
        return;
    }

    priv->iochannel = g_io_channel_unix_new (fd);
    priv->fd = fd;

    if (g_io_channel_set_flags (priv->iochannel, G_IO_FLAG_NONBLOCK, &error)
        != G_IO_STATUS_NORMAL) {
        g_warning ("FbIo Error with flag: (%d) %s", fd, error->message);
        g_error_free (error);
    }

    fcntl (fd, F_SETFD, fcntl (fd, F_GETFD) | FD_CLOEXEC);

    g_io_add_watch (priv->iochannel,
                    /* G_IO_OUT prevents G_IO_IN callbacks. */
                    G_IO_IN | G_IO_HUP | G_IO_ERR,
                    (GIOFunc)fb_io_watch_cb, io);
}

void
fb_io_write (FbIo        *io,
             const gchar *buff,
             guint        length)
{
    g_return_if_fail (FB_IS_IO (io));
    fb_io_translate (io, FALSE, buff, length);
}
