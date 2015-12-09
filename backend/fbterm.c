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

#include <linux/kdev_t.h> /* MINOR() */
#include <linux/vt.h>
#include <locale.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <sys/signalfd.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

#include "fbshellman.h"
#include "fbterm.h"
#include "fbtty.h"

enum {
    PROP_0 = 0,
    PROP_FBTERM
};

struct _FbSignalIoPrivate {
    FbTermObject       *fbterm;
};

struct _FbTermObjectPrivate {
    sigset_t            old_sigmask;
    FbTty              *tty;
    FbSignalIo         *io;
    gboolean            is_running;
    FbShellManager     *manager;
    FbIoDispatcher     *dispatcher;
};

G_DEFINE_TYPE_WITH_PRIVATE (FbSignalIo,
                            fb_signal_io,
                            FB_TYPE_IO);
G_DEFINE_TYPE_WITH_PRIVATE (FbTermObject,
                            fbterm_object,
//                            G_TYPE_INITIALLY_UNOWNED);
                            IBUS_TYPE_OBJECT);

static void            fb_signal_io_ready_read      (FbIo        *fbio,
                                                     const gchar *buff,
                                                     guint        length);
static void            fb_signal_io_get_property    (FbSignalIo *io,
                                                     guint       prop_id,
                                                     GValue     *value,
                                                     GParamSpec *pspec);
static void            fb_signal_io_set_property    (FbSignalIo   *io,
                                                     guint         prop_id,
                                                     const GValue *value,
                                                     GParamSpec   *pspec);
static GObject *       fbterm_object_constructor (
                               GType                  type,
                               guint                  n_construct_properties,
                               GObjectConstructParam *construct_params);
static void            fbterm_object_destroy        (FbTermObject *fbterm);

static void
fb_signal_io_init (FbSignalIo *io)
{
    FbSignalIoPrivate *priv =
            fb_signal_io_get_instance_private (io);
    io->priv = priv;
}

static void
fb_signal_io_class_init (FbSignalIoClass *class)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS (class);
    FB_IO_CLASS (class)->ready_read = fb_signal_io_ready_read;

    gobject_class->get_property =
            (GObjectGetPropertyFunc)fb_signal_io_get_property;
    gobject_class->set_property =
            (GObjectSetPropertyFunc)fb_signal_io_set_property;
    /* install properties */
    /**
     * FbShell:fbterm:
     *
     * The object of FbTermObject
     */
    g_object_class_install_property (gobject_class,
            PROP_FBTERM,
            g_param_spec_object ("fbterm",
                                 "fbterm",
                                 "The object of FbTerm",
                                 FBTERM_TYPE_OBJECT,
                                 G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));
}

static void
fb_signal_io_ready_read (FbIo *fbio, const gchar *buff, guint length)
{
    FbSignalIo *io = FB_SIGNAL_IO (fbio);
    struct signalfd_siginfo *si = (struct signalfd_siginfo *)buff;
    for (length /= sizeof (*si); length--; si++) {
        fbterm_object_process_signal (io->priv->fbterm, si->ssi_signo);
    }
}

static void
fb_signal_io_get_property (FbSignalIo *io,
                           guint       prop_id,
                           GValue     *value,
                           GParamSpec *pspec)
{
    FbSignalIoPrivate *priv = io->priv;

    switch (prop_id) {
    case PROP_FBTERM:
        g_value_set_object (value, priv->fbterm);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (io, prop_id, pspec);
    }
}

static void
fb_signal_io_set_property (FbSignalIo   *io,
                           guint         prop_id,
                           const GValue *value,
                           GParamSpec   *pspec)
{
    FbSignalIoPrivate *priv = io->priv;

    switch (prop_id) {
    case PROP_FBTERM: {
        FbTermObject *fbterm = g_value_get_object (value);
        g_return_if_fail (FBTERM_IS_OBJECT (fbterm));
        priv->fbterm= g_object_ref_sink (fbterm);
        break;
    }
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (io, prop_id, pspec);
    }
}

static void
fbterm_object_init (FbTermObject *fbterm)
{
    FbTermObjectPrivate *priv =
            fbterm_object_get_instance_private (fbterm);

    fbterm->priv = priv;
}

static void
fbterm_object_class_init (FbTermObjectClass *class)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS (class);
    IBUS_OBJECT_CLASS (class)->destroy =
            (IBusObjectDestroyFunc)fbterm_object_destroy;
    gobject_class->constructor = fbterm_object_constructor;
}

static GObject *
fbterm_object_constructor (GType                  type,
                           guint                  n_construct_properties,
                           GObjectConstructParam *construct_params)
{
    GObject *object;
    FbTermObject *fbterm;
    FbTermObjectPrivate *priv;
    struct vt_mode vtm;
    sigset_t sigmask;

    object = G_OBJECT_CLASS (fbterm_object_parent_class)->constructor (
            type,
            n_construct_properties,
            construct_params);

    fbterm = FBTERM_OBJECT (object);
    priv = fbterm->priv;

    priv->dispatcher = fb_io_dispatcher_new ();
    priv->manager = fb_shell_manager_new (NULL, fbterm, priv->dispatcher);
    priv->tty = fb_tty_new (priv->dispatcher, priv->manager);

    vtm.mode = VT_PROCESS;
    vtm.waitv = 0;
    vtm.relsig = SIGUSR1;
    vtm.acqsig = SIGUSR2;
    vtm.frsig = 0;
    ioctl (STDIN_FILENO, VT_SETMODE, &vtm);

    sigemptyset (&sigmask);

    sigaddset (&sigmask, SIGCHLD);
    sigaddset (&sigmask, SIGUSR1);
    sigaddset (&sigmask, SIGUSR2);
    //sigaddset (&sigmask, SIGALRM);
    sigaddset (&sigmask, SIGTERM);
    sigaddset (&sigmask, SIGHUP);

    sigprocmask (SIG_BLOCK, &sigmask, &priv->old_sigmask);
    priv->io = fb_signal_io_new (sigmask, fbterm, priv->dispatcher);

    signal (SIGPIPE, SIG_IGN);

    return object;
}

static void
fbterm_object_destroy (FbTermObject *fbterm)
{
    FbTermObjectPrivate *priv;

    g_return_if_fail (FBTERM_IS_OBJECT (fbterm));

    priv = fbterm->priv;

    ibus_object_destroy (IBUS_OBJECT (priv->io));
    priv->io = NULL;
    ibus_object_destroy (IBUS_OBJECT (priv->tty));
    priv->tty = NULL;
    ibus_object_destroy (IBUS_OBJECT (priv->dispatcher));
    priv->dispatcher = NULL;
}

static gboolean
fbterm_object_is_active_term (FbTermObject *fbterm)
{
    struct vt_stat vtstat;
    struct stat ttystat;

    ioctl (STDIN_FILENO, VT_GETSTATE, &vtstat);
    fstat (STDIN_FILENO, &ttystat);
    return vtstat.v_active == MINOR (ttystat.st_rdev);

}

FbSignalIo *
fb_signal_io_new (sigset_t        sigmask,
                  FbTermObject   *fbterm,
                  FbIoDispatcher *dispatcher)
{
    FbSignalIo *io = g_object_new (FB_TYPE_SIGNAL_IO,
                                   "fbterm", fbterm,
                                   "dispatcher", dispatcher,
                                   NULL);
    int fd = signalfd (-1, &sigmask, 0);
    fb_io_set_fd (FB_IO (io), fd);
    return io;
}

FbTermObject *
fbterm_object_new ()
{
    return g_object_new (FBTERM_TYPE_OBJECT, NULL);
}

void
fbterm_object_process_signal (FbTermObject *fbterm, int signo)
{
    FbTermObjectPrivate *priv;

    g_return_if_fail (FBTERM_IS_OBJECT (fbterm));

    priv = fbterm->priv;

    switch (signo) {
    case SIGTERM:
    case SIGHUP:
        fbterm_object_exit (fbterm);
        break;
    case SIGALRM:
        break;
    case SIGUSR1:
        fb_shell_manager_switch_vc (priv->manager, FALSE);
        fb_tty_switch_vc (priv->tty, FALSE);
        ioctl (STDIN_FILENO, VT_RELDISP, 1);
        break;
    case SIGUSR2:
        fb_tty_switch_vc (priv->tty, TRUE);
        fb_shell_manager_switch_vc (priv->manager, TRUE);
        break;
    case SIGCHLD:
        if (priv->is_running) {
            int pid = waitpid (WAIT_ANY, 0, WNOHANG);
            if (pid > 0)
                fb_shell_manager_child_process_exited (priv->manager, pid);
        }
        break;
    default:;
    }
}

void
fbterm_object_run (FbTermObject *fbterm)
{
    FbTermObjectPrivate *priv;

    g_return_if_fail (FBTERM_IS_OBJECT (fbterm));

    priv = fbterm->priv;
    //if (fbterm_object_is_active_term (fbterm))
        fbterm_object_process_signal (fbterm, SIGUSR2);
    fb_shell_manager_create_shell (priv->manager);
    //write (STDIN_FILENO, "\033[H\033[J", 6);
    priv->is_running = TRUE;
    while (priv->is_running) {
        fb_io_dispatcher_poll (priv->dispatcher);
    }
    if (fbterm_object_is_active_term (fbterm))
        fbterm_object_process_signal (fbterm, SIGUSR1);
}

void
fbterm_object_exit (FbTermObject *fbterm)
{
    g_return_if_fail (FBTERM_IS_OBJECT (fbterm));
    fbterm->priv->is_running = FALSE;
}

void
fbterm_object_init_child_process (FbTermObject *fbterm)
{
    FbTermObjectPrivate *priv;

    g_return_if_fail (FBTERM_IS_OBJECT (fbterm));

    priv = fbterm->priv;
    sigprocmask (SIG_SETMASK, &priv->old_sigmask, 0);
    signal (SIGPIPE, SIG_DFL);
}

int
main (int argc, char *argv[])
{
    FbTermObject *fbterm;

    setlocale (LC_ALL, "");
    setuid (getuid ());
    fbterm  = fbterm_object_new ();
    fbterm_object_run (fbterm);
    ibus_object_destroy (IBUS_OBJECT (fbterm));
    return 0;
}
