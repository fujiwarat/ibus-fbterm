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

#include <linux/kd.h>
#include <sys/ioctl.h>
#include <termios.h>

#include "fbtty.h"

enum {
    PROP_0 = 0,
    PROP_MANAGER
};

struct _FbTtyPrivate {
    FbShellManager *manager;
    gboolean        inited;
    long int        kb_mode;
    struct termios  old_tm;
};

G_DEFINE_TYPE_WITH_PRIVATE (FbTty,
                            fb_tty,
                            FB_TYPE_IO);

static GObject     *fb_tty_constructor
                               (GType                  type,
                                guint                  n_construct_properties,
                                GObjectConstructParam *construct_params);
static void         fb_tty_get_property      (FbTty       *tty,
                                              guint        prop_id,
                                              GValue       *value,
                                              GParamSpec   *pspec);
static void         fb_tty_set_property      (FbTty        *tty,
                                              guint         prop_id,
                                              const GValue *value,
                                              GParamSpec   *pspec);
static void         fb_tty_destroy           (FbTty        *tty);
static void         fb_tty_real_ready_read   (FbIo         *io,
                                              const gchar  *buff,
                                              guint         length);

static void
fb_tty_init (FbTty *tty)
{
    FbTtyPrivate *priv =
            fb_tty_get_instance_private (tty);
    tty->priv = priv;
}

static void
fb_tty_class_init (FbTtyClass *class)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS (class);
    IBUS_OBJECT_CLASS (class)->destroy = (IBusObjectDestroyFunc)fb_tty_destroy;
    FB_IO_CLASS (class)->ready_read = fb_tty_real_ready_read;
    gobject_class->constructor = fb_tty_constructor;
    gobject_class->get_property = (GObjectGetPropertyFunc)fb_tty_get_property;
    gobject_class->set_property = (GObjectSetPropertyFunc)fb_tty_set_property;
    /* install properties */
    /**
     * FbTty:shell-manager:
     *
     * The object of FbTty
     */
    g_object_class_install_property (gobject_class,
            PROP_MANAGER,
            g_param_spec_object ("shell-manager",
                                 "shell-manager",
                                 "The object of FbShellManager",
                                 FB_TYPE_SHELL_MANAGER,
                                 G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));
}

static GObject *
fb_tty_constructor (GType                  type,
                    guint                  n_construct_properties,
                    GObjectConstructParam *construct_params)
{
    GObject *object;

    object = G_OBJECT_CLASS (fb_tty_parent_class)->constructor (
            type,
            n_construct_properties,
            construct_params);

    /* Call after fb_io_set_property() */
    fb_io_set_fd (FB_IO (object), dup (STDIN_FILENO));

    return object;
}

static void
fb_tty_get_property (FbTty      *tty,
                     guint       prop_id,
                     GValue     *value,
                     GParamSpec *pspec)
{
    FbTtyPrivate *priv = tty->priv;

    switch (prop_id) {
    case PROP_MANAGER:
        g_value_set_object (value, priv->manager);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (tty, prop_id, pspec);
    }
}

static void
fb_tty_set_property (FbTty        *tty,
                     guint         prop_id,
                     const GValue *value,
                     GParamSpec   *pspec)
{
    FbTtyPrivate *priv = tty->priv;

    switch (prop_id) {
    case PROP_MANAGER: {
        FbShellManager *manager = g_value_get_object (value);
        g_return_if_fail (FB_IS_SHELL_MANAGER (manager));
        priv->manager = g_object_ref_sink (manager);
        break;
    }
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (tty, prop_id, pspec);
    }
}

static void
fb_tty_destroy (FbTty *tty)
{
    FbTtyPrivate *priv;

    g_return_if_fail (FB_IS_TTY (tty));

    priv = tty->priv;

    if (!priv->inited)
        return;

    ioctl (STDIN_FILENO, KDSKBMODE, priv->kb_mode);
    tcsetattr (STDIN_FILENO, TCSAFLUSH, &priv->old_tm);

    g_object_unref (priv->manager);
    priv->manager = NULL;
}

static void
fb_tty_real_ready_read (FbIo       *io,
                        const gchar *buff,
                        guint        length)
{
    FbTtyPrivate *priv;
    FbShell *shell;

    g_return_if_fail (FB_IS_TTY (io));

    priv = FB_TTY (io)->priv;
    shell = fb_shell_manager_active_shell (priv->manager);

    if (!shell)
        return;

    fb_shell_key_input (shell, buff, length);
}

FbTty *
fb_tty_new (FbShellManager *manager)
{
    return g_object_new (FB_TYPE_TTY,
                         "shell-manager", manager,
                         NULL);
}

void
fb_tty_switch_vc (FbTty *tty,
                  gboolean enter)
{
    FbTtyPrivate *priv;
    static struct termios tm;

    g_return_if_fail (FB_IS_TTY (tty));
    priv = tty->priv;

    if (!enter || priv->inited)
        return;

    priv->inited = TRUE;
    tcgetattr (STDIN_FILENO, &priv->old_tm);
    ioctl (STDIN_FILENO, KDGKBMODE, &priv->kb_mode);
    ioctl (STDIN_FILENO, KDSKBMODE, K_UNICODE);

    tm = priv->old_tm;
    cfmakeraw (&tm);
    tm.c_cc[VMIN] = 1;
    tm.c_cc[VTIME] = 0;
    tcsetattr (STDIN_FILENO, TCSAFLUSH, &tm);
}
