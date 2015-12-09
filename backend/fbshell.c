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

#define _XOPEN_SOURCE
#define __USE_XOPEN

#include <errno.h>
#include <fcntl.h>
#include <pty.h>
#include <pwd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <sys/wait.h>

#include "fbshell.h"
#include "fbshellman.h"
#include "fbterm.h"
#include "ibusfbcontext.h"
//#include "screen.h"
//#include "vterm.h"

//#include "defcolor.h"

#define NO_SCREEN
#define NO_VTERM

typedef struct {
    int start;
    int end;
    gboolean selecting;
    gboolean color_inversed;
} TextSelection;

typedef enum {
    CursorVisible = 1 << 0,
    CursorShape   = 1 << 1,
    MouseReport   = 1 << 2,
    CursorKeyEscO = 1 << 3,
    AutoRepeatKey = 1 << 4,
    ApplicKeypad  = 1 << 5,
    CRWithLF      = 1 << 6,
    ClearScreen   = 1 << 7,
    AllModes      = 0xff
} ModeType;

enum {
    PROP_0 = 0,
    PROP_MANAGER,
    PROP_SCREEN,
    PROP_FBTERM
};

struct _FbShellPrivate {
    int             pid;
#ifndef NO_VTERM
    TextSelection   sel_state;
    gboolean        palette_changed;
    Color          *palette;
#endif
    gboolean        first_shell;
    FbShellManager *manager;
#ifndef NO_SCREEN
    FbScreen       *screen;
#endif
#ifndef NO_VTERM
    Cursor          cursor;
#endif
    int             tty0_fd;
#ifndef NO_VTERM
    VTerm          *vterm;
#endif
    FbTermObject   *fbterm;
    struct winsize  size;
    IBusFbContext  *context;
};

G_DEFINE_TYPE_WITH_PRIVATE (FbShell,
                            fb_shell,
                            FB_TYPE_IO);

static GObject     *fb_shell_constructor
                               (GType                  type,
                                guint                  n_construct_properties,
                                GObjectConstructParam *construct_params);
static void         fb_shell_get_property         (FbShell     *shell,
                                                   guint        prop_id,
                                                   GValue       *value,
                                                   GParamSpec   *pspec);
static void         fb_shell_set_property         (FbShell      *shell,
                                                   guint         prop_id,
                                                   const GValue *value,
                                                   GParamSpec   *pspec);
static void         fb_shell_destroy              (FbShell      *shell);
static void         wait_child_process_exit       (int           pid);
static void         fb_shell_create_shell_process (FbShell      *shell,
                                                   gchar       **command);
#ifndef NO_VTERM
static void         fb_shell_reset_text_select    (FbShell      *shell);
#endif
#ifndef NO_SCREEN
static void         fb_shell_resize               (FbShell      *shell,
                                                   guint16       width,
                                                   guint16       height);
#endif
#ifndef NO_VTERM
static void         fb_shell_inverse_text_color   (FbShell      *shell,
                                                   guint         start,
                                                   guint         end);
#endif
static void         fb_shell_adjust_char_attr     (FbShell      *shell,
                                                   CharAttr     *attr);
static void         fb_shell_change_mode          (FbShell      *shell,
                                                   ModeType      type,
                                                   guint16       val);
static void         fb_shell_ready_read           (FbIo         *io,
                                                   const gchar  *buff,
                                                   guint         length);

static void
fb_shell_init (FbShell *shell)
{
    FbShellPrivate *priv =
            fb_shell_get_instance_private (shell);

    shell->priv = priv;

    priv->pid = -1;
#ifndef NO_VTERM
    priv->sel_state.selecting = FALSE;
    priv->sel_state.color_inversed = FALSE;
#endif
    priv->first_shell = TRUE;
#ifndef NO_VTERM
    priv->palette_changed = FALSE;
    priv->palette = NULL;
    priv->cursor.x = -1;
    priv->cursor.y = -1;
    priv->cursor.showed = FALSE;
#endif
    priv->tty0_fd = -1;
    priv->context = ibus_fb_context_new ();
}

static void
fb_shell_class_init (FbShellClass *class)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS (class);
    IBUS_OBJECT_CLASS (class)->destroy =
            (IBusObjectDestroyFunc)fb_shell_destroy;
    FB_IO_CLASS (class)->ready_read = fb_shell_ready_read;
    gobject_class->constructor = fb_shell_constructor;
    gobject_class->get_property = (GObjectGetPropertyFunc)fb_shell_get_property;
    gobject_class->set_property = (GObjectSetPropertyFunc)fb_shell_set_property;
    /* install properties */
    /**
     * FbShell:shell-manager:
     *
     * The object of FbShellManager
     */
    g_object_class_install_property (gobject_class,
            PROP_MANAGER,
            g_param_spec_object ("shell-manager",
                                 "shell-manager",
                                 "The object of FbShellManager",
                                 FB_TYPE_SHELL_MANAGER,
                                 G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));

    /**
     * FbShell:screen:
     *
     * The object of FbScreen
     */
#if 0
    g_object_class_install_property (gobject_class,
            PROP_SCREEN,
            g_param_spec_object ("screen",
                                 "screen",
                                 "The object of FbScreen",
                                 FB_TYPE_SCREEN,
                                 G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));
#endif

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

static GObject *
fb_shell_constructor (GType                  type,
                      guint                  n_construct_properties,
                      GObjectConstructParam *construct_params)
{
    GObject *object;
    FbShell *shell;
    FbShellPrivate *priv;

    object = G_OBJECT_CLASS (fb_shell_parent_class)->constructor (
            type,
            n_construct_properties,
            construct_params);

    shell = FB_SHELL (object);
    priv = shell->priv;

    /* Call after fb_shell_set_property() */
    //priv->vterm = vterm_new (0, 0, shell, priv->manager);

    fb_shell_create_shell_process (shell, NULL);
#ifndef NO_SCREEN
    fb_shell_resize(shell, screen->cols(), screen->rows());
#endif

    priv->first_shell = FALSE;

    return object;
}

static void
fb_shell_get_property (FbShell    *shell,
                       guint       prop_id,
                       GValue     *value,
                       GParamSpec *pspec)
{
    FbShellPrivate *priv = shell->priv;

    switch (prop_id) {
    case PROP_MANAGER:
        g_value_set_object (value, priv->manager);
        break;
#ifndef NO_SCREEN
    case PROP_SCREEN:
        g_value_set_object (value, priv->screen);
        break;
#endif
    case PROP_FBTERM:
        g_value_set_object (value, priv->fbterm);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (shell, prop_id, pspec);
    }
}

static void
fb_shell_set_property (FbShell      *shell,
                       guint         prop_id,
                       const GValue *value,
                       GParamSpec   *pspec)
{
    FbShellPrivate *priv = shell->priv;

    switch (prop_id) {
    case PROP_MANAGER: {
        FbShellManager *manager = g_value_get_object (value);
        g_return_if_fail (FB_IS_SHELL_MANAGER (manager));
        priv->manager = g_object_ref_sink (manager);
        break;
    }
#ifndef NO_SCREEN
    case PROP_SCREEN: {
        FbScreen *screen = g_value_get_object (value);
        g_return_if_fail (FB_IS_SCREEN (screen));
        priv->screen = g_object_ref_sink (screen);
        break;
    }
#endif
    case PROP_FBTERM: {
        FbTermObject *fbterm = g_value_get_object (value);
        g_return_if_fail (FBTERM_IS_OBJECT (fbterm));
        priv->fbterm= g_object_ref_sink (fbterm);
        break;
    }
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (shell, prop_id, pspec);
    }
}

static void
fb_shell_destroy (FbShell *shell)
{
    FbShellPrivate *priv;

    g_return_if_fail (FB_IS_SHELL (shell));

    priv = shell->priv;

    fb_shell_manager_shell_exited (priv->manager, shell);
    fb_io_set_fd (FB_IO (shell), -1);
    wait_child_process_exit (priv->pid);
}

static void
wait_child_process_exit (int pid)
{
    int retval;
    int i;

    if (pid < 0)
        return;

    kill (pid, SIGTERM);
    sched_yield ();

    retval = waitpid (pid, 0, WNOHANG);
    if (retval > 0 || (retval == -1 && errno == ECHILD))
        return;

    for (i = 5; i--;) {
        usleep (100 * 1000);

        retval = waitpid (pid, 0, WNOHANG);
        if (retval > 0)
        break;
    }

    if (retval <= 0) {
        kill (pid, SIGKILL);
        waitpid (pid, 0, 0);
    }
}

static void
fb_shell_create_shell_process (FbShell *shell, gchar **command)
{
    int fd;
    FbShellPrivate *priv;

    g_return_if_fail (FB_IS_SHELL (shell));

    priv = shell->priv;

    priv->pid = forkpty (&fd, NULL, NULL, NULL);

    switch (priv->pid) {
    case -1:
        break;
    case 0:  // child process
        fb_shell_init_shell_process (shell);
        setenv ("TERM", "linux", 1);

        if (command) {
            execvp (command[0], command);
        } else {
            struct passwd *userpd = NULL;
            const gchar *shell_str = getenv ("SHELL");
            if (shell_str)
                execlp (shell_str, shell_str, NULL);

            userpd = getpwuid (getuid ());
            execlp (userpd->pw_shell, userpd->pw_shell, NULL);

            execlp ("/bin/sh", "/bin/sh", NULL);
        }

        exit (1);
        break;
    default:
        fb_io_set_fd (FB_IO (shell), fd);
#ifndef NO_SCREEN
        fb_shell_resize (shell,
                         vterm_get_width (priv->vterm),
                         vterm_get_height (priv->vterm));
#endif
        break;
    }
}

#ifndef NO_SCREEN
static void
fb_shell_resize (FbShell *shell, guint16 w, guint16 h)
{
    FbShellPrivate *priv;

    g_return_if_fail (FB_IS_SHELL (shell));

    priv = shell->priv;

    if (!w || !h) return;

    fb_shell_reset_text_select (shell);
    vterm_resize (priv->vterm, w, h);

    struct winsize size;
    size.ws_xpixel = 0;
    size.ws_ypixel = 0;
    size.ws_col = w;
    size.ws_row = h;
    ioctl (fb_io_get_fd (FB_IO (shell)), TIOCSWINSZ, &size);
}
#endif

#ifndef NO_VTERM
static void
fb_shell_inverse_text_color (FbShell *shell,
                             guint    start,
                             guint    end)
{
    FbShellPrivate *priv;
    guint16 width;
    guint16 sx, sy, ex, ey;
    CharAttr *attr;

    g_return_if_fail (FB_IS_SHELL (shell));

    priv = shell->priv;

    width = vterm_get_width (priv->vterm);
    sx = start % width;
    sy = start / width;
    ex = end % width;
    ey = end / width;

    vterm_inverse (priv->vterm, sx, sy, ex, ey);

    attr = vterm_get_char_attr (priv->vterm, sx, sy);
    if (attr && attr->type == DoubleRight)
        sx--;

    attr = vterm_get_char_attr (priv->vterm, ex, ey);
    if (attr && attr->type == DoubleLeft)
        ex++;

    vterm_request_update (priv->vterm, sx, sy,
                          (sy == ey ? (ex + 1) : width) - sx, 1);

    if (ey > sy + 1)
        vterm_request_update (priv->vterm, 0, sy + 1, width, ey - sy - 1);

    if (ey > sy)
        vterm_request_update (priv->vterm, 0, ey, ex + 1, 1);
}

static void
fb_shell_reset_text_select (FbShell *shell)
{
    FbShellPrivate *priv;

    g_return_if_fail (FB_IS_SHELL (shell));

    priv = shell->priv;

    priv->sel_state.selecting = FALSE;
    if (priv->sel_state.color_inversed) {
        priv->sel_state.color_inversed = FALSE;
        fb_shell_inverse_text_color (shell,
                                     priv->sel_state.start,
                                     priv->sel_state.end);
    }
}
#endif

static void
fb_shell_adjust_char_attr (FbShell  *shell,
                           CharAttr *attr)
{
    if (attr->italic)
        attr->fcolor = 2; // green
    else if (attr->underline)
        attr->fcolor = 6; // cyan
    else if (attr->intensity == 0)
        attr->fcolor = 8; // gray

    if (attr->blink && attr->bcolor < 8)
        attr->bcolor ^= 8;
    if (attr->intensity == 2 && attr->fcolor < 8)
        attr->fcolor ^= 8;

    if (attr->reverse) {
        guint16 temp = attr->bcolor;
        attr->bcolor = attr->fcolor;
        attr->fcolor = temp;

        if (attr->bcolor > 8 && attr->bcolor < 16)
            attr->bcolor -= 8;
    }
}

static void
fb_shell_change_mode (FbShell  *shell,
                      ModeType  type,
                      guint16   val)
{
    const gchar *str = 0;

    if (type == CursorKeyEscO)
        str = (val ? "\e[?1h" : "\e[?1l");
    else if (type == AutoRepeatKey)
        str = (val ? "\e[?8h" : "\e[?8l");
    else if (type == ApplicKeypad)
        str = (val ? "\e=" : "\e>");
    else if (type == CRWithLF)
        str = (val ? "\e[20h" : "\e[20l");
    else if (type == ClearScreen)
        str = "\033[H\033[J";

    if (str)
        write (STDIN_FILENO, str, strlen (str));
}

static void
fb_shell_ready_read (FbIo        *io,
                     const gchar *buff,
                     guint        length)
{
#ifndef NO_VTERM
    FbShell *shell;
    //FbShellPrivate *priv;
#endif

    g_return_if_fail (FB_IS_SHELL (io));

#ifndef NO_VTERM
    shell = FB_SHELL (io);
    //priv = shell->priv;
#endif
#ifndef NO_VTERM
    fb_shell_reset_text_select (shell);
#endif
    //vterm_input (priv->vterm, buff, length);
    write (STDOUT_FILENO, buff, length);
}

FbShell *
fb_shell_new (FbShellManager *manager,
              FbScreen       *screen,
              FbTermObject   *fbterm,
              FbIoDispatcher *dispatcher)
{
    return g_object_new (FB_TYPE_SHELL,
                         "shell-manager", manager,
#if 0
                         "screen", screen,
#endif
                         "fbterm", fbterm,
                         "dispatcher", dispatcher,
                         NULL);
}

void
fb_shell_draw_chars (FbShell *shell,
                     CharAttr attr,
                     unsigned short x,
                     unsigned short y,
                     unsigned short w,
                     unsigned short num,
                     unsigned short *chars,
                     gboolean *dws)
{
    FbShellPrivate *priv;

    g_return_if_fail (FB_IS_SHELL (shell));

    priv = shell->priv;
    if (fb_shell_manager_active_shell (priv->manager) != shell)
        return;

    fb_shell_adjust_char_attr (shell, &attr);
#ifndef NO_SCREEN
    fb_screen_draw_text (priv->screen,
                         FW(x), FH(y),
                         attr.fcolor, attr.bcolor,
                         num, chars, dws);
#endif
}

gboolean
fb_shell_move_chars (FbShell *shell,
                     guint16 sx,
                     guint16 sy,
                     guint16 dx,
                     guint16 dy,
                     guint16 w,
                     guint16 h)
{
    FbShellPrivate *priv;

    g_return_if_fail (FB_IS_SHELL (shell));

    priv = shell->priv;
    if (fb_shell_manager_active_shell (priv->manager) != shell)
        return TRUE;

#ifndef NO_SCREEN
    return fb_screen_move (priv->screen, sx, sy, dx, dy, w, h);
#else
    return FALSE;
#endif
}

#ifndef NO_VTERM
void
fb_shell_draw_cursor (FbShell *shell,
                      CharAttr attr,
                      guint16 x,
                      guint16 y,
                      guint16 c)
{
    FbShellPrivate *priv;

    g_return_if_fail (FB_IS_SHELL (shell));

    priv = shell->priv;

    fb_shell_adjust_char_attr (shell, &attr);
    priv->cursor.attr = attr;
    priv->cursor.x = x;
    priv->cursor.y = y;
    priv->cursor.code = c;
    priv->cursor.showed = FALSE;

    fb_shell_update_cursor (shell);
}

void
fb_shell_enable_cursor (FbShell *shell, gboolean enable)
{
    static guint interval = 500;
    static gboolean enabled = FALSE;
    guint val = (enable ? interval : 0);
    guint sec = val / 1000 ;
    guint usec = (val % 1000) * 1000;

    struct itimerval timer;

    if (enabled == enable)
        return;
    enabled = enable;

    timer.it_interval.tv_usec = usec;
    timer.it_interval.tv_sec = sec;
    timer.it_value.tv_usec = usec;
    timer.it_value.tv_sec = sec;

    setitimer (ITIMER_REAL, &timer, NULL);
}
#endif

void
fb_shell_mode_changed (FbShell *shell, ModeType type)
{
    FbShellPrivate *priv;

    g_return_if_fail (FB_IS_SHELL (shell));

    priv = shell->priv;

    if (fb_shell_manager_active_shell (priv->manager) != shell)
        return;
#ifndef NO_VTERM
    if (type & (CursorVisible | CursorShape))
        fb_shell_enable_cursor (shell, TRUE);
#endif

    if (type & CursorKeyEscO)
        fb_shell_change_mode (shell, CursorKeyEscO, FALSE);

    if (type & AutoRepeatKey)
        fb_shell_change_mode (shell, AutoRepeatKey, TRUE);

    if (type & ApplicKeypad)
        fb_shell_change_mode (shell, ApplicKeypad, FALSE);

    if (type & CRWithLF)
        fb_shell_change_mode (shell, CRWithLF, FALSE);

    if (type & ClearScreen)
        fb_shell_change_mode (shell, ClearScreen, TRUE);
}

#ifndef NO_SCREEN
void
fb_shell_request (FbShell *shell, RequestType type, guint value)
{
    FbShellPrivate *priv;
    gboolean active = TRUE;

    g_return_if_fail (FB_IS_SHELL (shell));

    priv = shell->priv;

    active = (fb_shell_manager_active_shell (priv->manager) == shell);

    switch (type) {
    case PaletteSet:
        if ((value >> 24) >= NR_COLORS)
            break;

        if (!priv->palette_changed) {
            priv->palette_changed = TRUE;

            if (!priv->palette)
                priv->palette = g_new0 (Color, NR_COLORS);
            memcpy(priv->palette, default_palette, sizeof (default_palette));
        }

        priv->palette[value >> 24].red = (value >> 16) & 0xff;
        priv->palette[value >> 24].green = (value >> 8) & 0xff;
        priv->palette[value >> 24].blue = value & 0xff;

        if (active)
            fb_screen_set_pallet (priv->screen, priv->palette);
        break;
    case PaletteClear:
        if (!priv->palette_changed)
            break;
        priv->palette_changed = FALSE;
        if (active)
            fb_screen_set_pallet (priv->screen, default_pallet);
        break;
    case VcSwitch:
        break;
    default:;
    }
}
#endif

void
fb_shell_switch_vt (FbShell *shell, gboolean enter, FbShell *peer)
{
    FbShellPrivate *priv;

    g_return_if_fail (FB_IS_SHELL (shell));

    priv = shell->priv;

    if (priv->tty0_fd == -1)
        priv->tty0_fd = open ("/dev/tty0", O_RDWR);
    if (priv->tty0_fd != -1) {
        seteuid (0);
        ioctl (priv->tty0_fd, TIOCGWINSZ, &priv->size);
        ioctl (priv->tty0_fd, TIOCCONS, 0);
        if (enter) {
            int slavefd = open (ptsname (fb_io_get_fd (FB_IO (shell))), O_RDWR);
            ioctl (slavefd, TIOCCONS, 0);
            close (slavefd);
        }
        seteuid (getuid ());
    }

    if (enter) {
#ifndef NO_SCREEN
        fb_screen_set_palette (
                priv->screen,
                priv->palette_changed ? priv->palette : default_palette);
#endif
        fb_shell_mode_changed (shell, AllModes);
    } else if (!peer) {
        fb_shell_change_mode (shell, CursorKeyEscO, FALSE);
        fb_shell_change_mode (shell, ApplicKeypad, FALSE);
        fb_shell_change_mode (shell, CRWithLF, FALSE);
        fb_shell_change_mode (shell, AutoRepeatKey, TRUE);

#ifndef NO_VTERM
        fb_shell_enable_cursor (shell, FALSE);
#endif
#ifndef NO_SCREEN
        fb_screen_set_palette (shell, priv->screen, default_palette);
#endif
    }
}

void
fb_shell_init_shell_process (FbShell *shell)
{
    FbShellPrivate *priv;

    g_return_if_fail (FB_IS_SHELL (shell));

    priv = shell->priv;

    if (priv->tty0_fd != -1)
        close (priv->tty0_fd);
    fbterm_object_init_child_process (priv->fbterm);

    if (!priv->first_shell)
        return;
}

void
fb_shell_key_input (FbShell     *shell,
                    const gchar *buff,
                    guint        length)
{
    FbShellPrivate *priv;
    guint retval;
    gchar *dispatched = NULL;
    g_return_if_fail (FB_IS_SHELL (shell));

    priv = shell->priv;

#ifndef NO_VTERM
    fb_shell_reset_text_select (shell);
#endif
    retval = IBUS_FB_CONTEXT_GET_CLASS (priv->context)->filter_keypress (
            priv->context, buff, length, &dispatched);
    if (!retval)
        return;

    fb_io_write (FB_IO (shell), dispatched, retval);
    g_free (dispatched);
}

gboolean
fb_shell_child_process_exited (FbShell *shell, int pid)
{
    FbShellPrivate *priv;

    g_return_val_if_fail (FB_IS_SHELL (shell), FALSE);

    priv = shell->priv;
    if (pid == priv->pid) {
        ibus_object_destroy (IBUS_OBJECT (shell));
        return TRUE;
    }

    return FALSE;
}

#ifndef NO_VTERM
void
fb_shell_expose (FbShell *shell,
                 guint16 x,
                 guint16 y,
                 guint16 width,
                 guint16 height)
{
    FbShellPrivate *priv;

    g_return_if_fail (FB_IS_SHELL (shell));

    priv = shell->priv;
    vterm_expose (priv->vterm, x, y, width, height);
    if (priv->cursor.y >= y && priv->cursor.y < (y + height) &&
        priv->cursor.x >= x && priv->cursor.x < (x + width)) {
        priv->cursor.showed = FALSE;
        fb_shell_update_cursor (shell);
    }
}

void
fb_shell_update_cursor (FbShell *shell)
{
    FbShellPrivate *priv;

    g_return_if_fail (FB_IS_SHELL (shell));

    priv = shell->priv;

    if (fb_shell_manager_active_shell (priv->manager) != shell ||
        priv->cursor.x >= vterm_get_width (priv->vterm) ||
        priv->cursor.y >= vterm_get_height (priv->vterm))
        return;

    priv->cursor.showed ^= TRUE;

#ifndef NO_SCREEN
    fb_screen_fill_rect (
            priv->screen,
            FW (priv->cursor.x),
            FH (priv->cursor.y + 1) - 1,
            FW (1),
            1,
            priv->cursor.showed ?
                    priv->cursor.attr.fcolor : priv->cursor.attr.bcolor);
#endif
}
#endif
