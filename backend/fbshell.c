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

#define WRITE_STR(fd, string) write ((fd), (string), strlen ((string)))

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
    PROP_FBTERM
};

struct _FbShellPrivate {
    int             pid;
    gboolean        first_shell;
    FbShellManager *manager;
    int             tty0_fd;
    FbTermObject   *fbterm;
    struct winsize  size;
    IBusFbContext  *context;
    gchar          *preedit_text;
    gchar          *lookup_table_head;
    gchar          *lookup_table_middle;
    gchar          *lookup_table_end;
    int             lookup_table_x;
    int             lookup_table_y;
};

G_DEFINE_TYPE_WITH_PRIVATE (FbShell,
                            fb_shell,
                            FB_TYPE_IO);

static GObject     *fb_shell_constructor
                               (GType                  type,
                                guint                  n_construct_properties,
                                GObjectConstructParam *construct_params);
static void         fb_shell_get_property         (FbShell         *shell,
                                                   guint            prop_id,
                                                   GValue          *value,
                                                   GParamSpec      *pspec);
static void         fb_shell_set_property         (FbShell         *shell,
                                                   guint            prop_id,
                                                   const GValue    *value,
                                                   GParamSpec      *pspec);
static void         fb_shell_destroy              (FbShell         *shell);
static void         wait_child_process_exit       (int              pid);
static void         fb_shell_create_shell_process (FbShell         *shell,
                                                   gchar          **command);
static void         fb_shell_change_mode          (FbShell         *shell,
                                                   ModeType         type,
                                                   guint16          val);
static void         fb_shell_ready_read           (FbIo            *io,
                                                   const gchar     *buff,
                                                   guint            length);
static void         fb_context_cursor_position_cb (IBusFbContext   *context,
                                                   int              x,
                                                   int              y,
                                                   FbShell         *shell);
static void         fb_context_commit_cb          (IBusFbContext   *context,
                                                   IBusText        *text,
                                                   FbShell         *shell);
static void         fb_context_preedit_changed_cb (IBusFbContext   *context,
                                                   IBusText        *text,
                                                   int             *cursor_pos,
                                                   gboolean        *visible,
                                                   FbShell         *shell);
static void         fb_context_update_lookup_table_cb
                                                  (IBusFbContext   *context,
                                                   IBusLookupTable *table,
                                                   gboolean        *visible,
                                                   FbShell         *shell);

static void
fb_shell_init (FbShell *shell)
{
    FbShellPrivate *priv =
            fb_shell_get_instance_private (shell);

    shell->priv = priv;

    priv->pid = -1;
    priv->first_shell = TRUE;
    priv->tty0_fd = -1;
    priv->context = ibus_fb_context_new ();
    g_object_connect (priv->context,
                      "signal::cursor-position",
                      (GCallback)fb_context_cursor_position_cb,
                      shell,
                      "signal::commit",
                      (GCallback)fb_context_commit_cb,
                      shell,
                      "signal::preedit-changed",
                      (GCallback)fb_context_preedit_changed_cb,
                      shell,
                      "signal::update-lookup-table",
                      (GCallback)fb_context_update_lookup_table_cb,
                      shell,
                      NULL);
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
    fb_shell_create_shell_process (shell, NULL);

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
    case PROP_FBTERM: {
        FbTermObject *fbterm = g_value_get_object (value);
        g_return_if_fail (FBTERM_IS_OBJECT (fbterm));
        priv->fbterm = g_object_ref_sink (fbterm);
        break;
    }
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (shell, prop_id, pspec);
    }
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
        break;
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
        WRITE_STR (STDIN_FILENO, str);
}

static void
fb_shell_ready_read (FbIo        *io,
                     const gchar *buff,
                     guint        length)
{
    g_return_if_fail (FB_IS_SHELL (io));

    write (STDOUT_FILENO, buff, length);
}

static void
fb_shell_save_cursor (FbShell *shell)
{
    WRITE_STR (STDIN_FILENO, "\033\067");
}

static void
fb_shell_restore_cursor (FbShell *shell)
{
    WRITE_STR (STDIN_FILENO, "\033\070");
}

static void
fb_shell_get_cursor (FbShell *shell)
{
    WRITE_STR (STDIN_FILENO, "\033[6n");
}

static void
fb_shell_move_cursor (FbShell *shell,
                      int      x,
                      int      y)
{
    gchar *str = g_strdup_printf ("\033[%d;%dH", x, y);
    WRITE_STR (STDIN_FILENO, str);
    g_free (str);
}

static void
fbshell_draw_inverse_color (FbShell *shell)
{
    WRITE_STR (STDIN_FILENO, "\033[7m");
}

static void
fbshell_draw_blue_color_bg (FbShell *shell)
{
    /* underline "\033[4m" is not underline actually */
    WRITE_STR (STDIN_FILENO, "\033[44m");
}

static void
fbshell_reset_color (FbShell *shell)
{
    WRITE_STR (STDIN_FILENO, "\033[m");
}

static void
fb_shell_erase_cursor_line (FbShell *shell)
{
    WRITE_STR (STDIN_FILENO, "\033[K");
}

static void
fb_shell_set_scrolling_region (FbShell *shell,
                               int      top,
                               int      bottom)
{
    char *str = g_strdup_printf("\033[%d;%dr", top, bottom);
    WRITE_STR (STDIN_FILENO, str);
    g_free (str);
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

    g_free (priv->preedit_text);
    priv->preedit_text = NULL;

    g_object_unref (priv->manager);
    priv->manager = NULL;
    g_object_unref (priv->fbterm);
    priv->fbterm = NULL;

    fb_shell_set_scrolling_region (shell, 0, priv->size.ws_row);
}

static void
fb_shell_reset_preedit (FbShell *shell)
{
    FbShellPrivate *priv;
    gunichar *ucs;
    glong i, read, written;
    GError *error = NULL;
    int width = 0;
    gchar *cleared_text = NULL;

    g_return_if_fail (FB_IS_SHELL (shell));

    priv = shell->priv;

    if (priv->preedit_text == NULL)
        return;

    ucs = g_utf8_to_ucs4 (priv->preedit_text, -1, &read, &written, &error);
    if (ucs == NULL) {
        g_warning ("Invalid string %s: %s", priv->preedit_text, error->message);
        g_error_free (error);
        g_free (priv->preedit_text);
        priv->preedit_text = NULL;
        return;
    }
    for (i = 0; i < read; i++) {
        width += (g_unichar_iswide (ucs[i]) ? 2 : 1);
    }

    g_free (ucs);
    g_free (priv->preedit_text);
    priv->preedit_text = NULL;

    if (!width)
        return;

    cleared_text = g_new0 (gchar, width + 1);
    memset (cleared_text, ' ', sizeof (gchar) * width);
    cleared_text[width] = '\0';

    fb_shell_save_cursor (shell);

    WRITE_STR (STDOUT_FILENO, cleared_text);
    g_free (cleared_text);

    fb_shell_restore_cursor (shell);
}

static void
fb_shell_reset_lookup_table (FbShell *shell)
{
    FbShellPrivate *priv;

    g_return_if_fail (FB_IS_SHELL (shell));

    priv = shell->priv;

    if (priv->lookup_table_head == NULL)
        return;

    g_free (priv->lookup_table_head);
    g_free (priv->lookup_table_middle);
    g_free (priv->lookup_table_end);
    priv->lookup_table_head = NULL;
    priv->lookup_table_middle = NULL;
    priv->lookup_table_end = NULL;

    fb_shell_save_cursor (shell);
    fb_shell_move_cursor (shell, priv->lookup_table_x, priv->lookup_table_y);
    fb_shell_erase_cursor_line (shell);
    fb_shell_restore_cursor (shell);
}

static void
fb_context_cursor_position_cb (IBusFbContext *context,
                               int            x,
                               int            y,
                               FbShell       *shell)
{
    FbShellPrivate *priv;
    int lookup_table_x = x + 1;
    int lookup_table_y = 1;

    g_return_if_fail (FB_IS_SHELL (shell));

    priv = shell->priv;

    if (priv->lookup_table_head == NULL)
        return;

    fb_shell_save_cursor (shell);
    fb_shell_move_cursor (shell, lookup_table_x, lookup_table_y);
    priv->lookup_table_x = lookup_table_x;
    priv->lookup_table_y = lookup_table_y;
    WRITE_STR (STDOUT_FILENO, priv->lookup_table_head);
    fbshell_draw_inverse_color (shell);
    WRITE_STR (STDOUT_FILENO, priv->lookup_table_middle);
    fbshell_reset_color (shell);
    WRITE_STR (STDOUT_FILENO, priv->lookup_table_end);
    fb_shell_restore_cursor (shell);
}

static void
fb_context_commit_cb (IBusFbContext *context,
                      IBusText      *text,
                      FbShell       *shell)
{
    g_return_if_fail (FB_IS_SHELL (shell));

    fb_io_write (FB_IO (shell), text->text, strlen (text->text));
}

static void
fb_context_preedit_changed_cb (IBusFbContext *context,
                               IBusText      *text,
                               int           *cursor_pos,
                               gboolean      *visible,
                               FbShell       *shell)
{
    FbShellPrivate *priv;
    int text_length;

    g_return_if_fail (FB_IS_SHELL (shell));

    priv = shell->priv;

    fb_shell_reset_preedit (shell);
    fb_shell_save_cursor (shell);

    text_length = strlen (text->text);

    if (text->attrs) {
        int i;
        gchar *start_pointer, *end_pointer;
        gchar *substr;
        gboolean has_whole_preedit = FALSE;
        gboolean has_sub_preedit = FALSE;

        for (i = 0; ; i++) {
            IBusAttribute *attr = ibus_attr_list_get (text->attrs, i);
            gchar *s, *e;

            if (attr == NULL)
                break;
            if (attr->type != IBUS_ATTR_TYPE_UNDERLINE &&
                attr->type != IBUS_ATTR_TYPE_FOREGROUND &&
                attr->type != IBUS_ATTR_TYPE_BACKGROUND)
                continue;

            s = g_utf8_offset_to_pointer (text->text, attr->start_index);
            e = g_utf8_offset_to_pointer (text->text, attr->end_index);

            if ((e - s) == text_length) {
                has_whole_preedit = TRUE;
                continue;
            } else {
                has_sub_preedit = TRUE;
                start_pointer = s;
                end_pointer = e;
                continue;
            }
        }
        if (has_whole_preedit)
            fbshell_draw_inverse_color (shell);
        if (has_sub_preedit) {
            if (start_pointer > text->text) {
                substr = g_strndup (text->text, start_pointer - text->text);
                WRITE_STR (STDOUT_FILENO, substr);
                g_free (substr);
            }

            fbshell_draw_blue_color_bg (shell);
            substr = g_strndup (start_pointer, end_pointer - start_pointer);
            WRITE_STR (STDOUT_FILENO, substr);
            g_free (substr);
            fbshell_reset_color (shell);
            if (has_whole_preedit)
                fbshell_draw_inverse_color (shell);

            if (text->text + text_length > end_pointer) {
                substr = g_strndup (end_pointer,
                                    text->text + text_length - end_pointer);
                WRITE_STR (STDOUT_FILENO, substr);
                g_free (substr);
            }
        } else {
            write (STDOUT_FILENO, text->text, text_length);
        }
    } else {
        write (STDOUT_FILENO, text->text, text_length);
    }

    priv->preedit_text = g_strdup (text->text);
    fb_shell_restore_cursor (shell);
}

static void
fb_context_update_lookup_table_cb (IBusFbContext   *context,
                                   IBusLookupTable *table,
                                   gboolean        *visible,
                                   FbShell         *shell)
{
    FbShellPrivate *priv;
    guint page_size;
    guint ncandidates;
    guint cursor;
    guint cursor_in_page = 0;
    guint page_start_pos;
    guint page_end_pos;
    guint i;
    gboolean show_cursor = TRUE;
    GString *candidate_list_head;
    GString *candidate_list_middle;
    GString *candidate_list_end;

    g_return_if_fail (FB_IS_SHELL (shell));

    if (!visible) {
        fb_shell_reset_lookup_table (shell);
        return;
    }

    priv = shell->priv;

    page_size = ibus_lookup_table_get_page_size (table);
    ncandidates = ibus_lookup_table_get_number_of_candidates (table);
    cursor = ibus_lookup_table_get_cursor_pos (table);
    cursor_in_page = ibus_lookup_table_get_cursor_in_page (table);
    page_start_pos = cursor / page_size * page_size;
    page_end_pos = MIN (page_start_pos + page_size, ncandidates);
    show_cursor = ibus_lookup_table_is_cursor_visible (table);
    candidate_list_head = g_string_new (NULL);
    candidate_list_middle = g_string_new (NULL);
    candidate_list_end = g_string_new (NULL);

    for (i = page_start_pos; i < page_end_pos; i++) {
        IBusText *candidate = ibus_lookup_table_get_candidate (table, i);
        guint index = i - page_start_pos;
        IBusText *label = ibus_lookup_table_get_label (table, index);
        gchar *label_str;
        gchar *candidate_cell;

        if (label)
            label_str = g_strdup (label->text);
        else
            label_str = g_strdup_printf ("%d", index);

        if (index == 0) {
            candidate_cell = g_strdup_printf ("%s. %s",
                                              label_str, candidate->text);
        } else {
            candidate_cell = g_strdup_printf (" %s. %s",
                                              label_str, candidate->text);
        }

        if (show_cursor) {
            if (index < cursor_in_page)
                g_string_append (candidate_list_head, candidate_cell);
            else if (index == cursor_in_page)
                g_string_append (candidate_list_middle, candidate_cell);
            else
                g_string_append (candidate_list_end, candidate_cell);
        } else {
            g_string_append (candidate_list_head, candidate_cell);
        }
        g_free (candidate_cell);
        g_free (label_str);
    }

    fb_shell_reset_lookup_table (shell);
    priv->lookup_table_head = g_string_free (candidate_list_head, FALSE);
    priv->lookup_table_middle = g_string_free (candidate_list_middle, FALSE);
    priv->lookup_table_end = g_string_free (candidate_list_end, FALSE);
    fb_shell_get_cursor (shell);
}

FbShell *
fb_shell_new (FbShellManager *manager,
              FbTermObject   *fbterm)
{
    return g_object_new (FB_TYPE_SHELL,
                         "shell-manager", manager,
                         "fbterm", fbterm,
                         NULL);
}

void
fb_shell_mode_changed (FbShell *shell, ModeType type)
{
    FbShellPrivate *priv;

    g_return_if_fail (FB_IS_SHELL (shell));

    priv = shell->priv;

    if (fb_shell_manager_active_shell (priv->manager) != shell)
        return;

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

void
fb_shell_switch_vt (FbShell *shell, gboolean enter, FbShell *peer)
{
    FbShellPrivate *priv;

    g_return_if_fail (FB_IS_SHELL (shell));

    priv = shell->priv;

    if (priv->tty0_fd == -1)
        priv->tty0_fd = open ("/dev/tty", O_RDWR);
    if (priv->tty0_fd != -1) {
        seteuid (0);
        /* Need tty instead of tty0 to get the right size. */
        ioctl (priv->tty0_fd, TIOCGWINSZ, &priv->size);
        fb_shell_set_scrolling_region (shell, 0, priv->size.ws_row - 1);
        ioctl (priv->tty0_fd, TIOCCONS, 0);
        if (enter) {
            int slavefd = open (ptsname (fb_io_get_fd (FB_IO (shell))), O_RDWR);
            ioctl (slavefd, TIOCCONS, 0);
            close (slavefd);
        }
        seteuid (getuid ());
    }

    if (enter) {
        fb_shell_mode_changed (shell, AllModes);
    } else if (!peer) {
        fb_shell_change_mode (shell, CursorKeyEscO, FALSE);
        fb_shell_change_mode (shell, ApplicKeypad, FALSE);
        fb_shell_change_mode (shell, CRWithLF, FALSE);
        fb_shell_change_mode (shell, AutoRepeatKey, TRUE);
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
