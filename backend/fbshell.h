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

#ifndef __FB_SHELL_H_
#define __FB_SHELL_H_

#include <glib-object.h>
#include <ibus.h>

#include "fbio.h"

/*
 * Type macros.
 */

/* define GOBJECT macros */
#define FB_TYPE_SHELL                           (fb_shell_get_type ())
#define FB_SHELL(o)                             (G_TYPE_CHECK_INSTANCE_CAST ((o), FB_TYPE_SHELL, FbShell))
#define FB_SHELL_CLASS(k)                       (G_TYPE_CHECK_CLASS_CAST ((k), FB_TYPE_SHELL, FbShellClass))
#define FB_IS_SHELL(o)                          (G_TYPE_CHECK_INSTANCE_TYPE ((o), FB_TYPE_SHELL))
#define FB_IS_SHELL_CLASS(k)                    (G_TYPE_CHECK_CLASS_TYPE ((k), FB_TYPE_SHELL))


G_BEGIN_DECLS
typedef enum {
    Single = 0,
    DoubleLeft,
    DoubleRight
} CharType;

typedef enum {
    Bell, BellFrequencySet, BellDurationSet,
    PaletteSet, PaletteClear,
    Blank, Unblank,
    LedSet, LedClear,
    VcSwitch, VesaPowerIntervalSet,
} RequestType;

typedef struct _CharAttr CharAttr;
typedef struct _Cursor Cursor;
typedef struct _FbShell FbShell;
typedef struct _FbShellPrivate FbShellPrivate;
typedef struct _FbShellClass FbShellClass;

typedef struct _FbShellManager FbShellManager;
typedef struct _FbTermObject FbTermObject;

struct _CharAttr {
    guint16  fcolor;
    guint16  bcolor;
    guint16  intensity;
    guint16  italic;
    guint16  underline;
    guint16  blink;
    guint16  reverse;
    CharType type;
};

struct _Cursor {
    gboolean showed;
    guint16  x;
    guint16  y;
    guint16  code;
    CharAttr attr;
};

/**
 * FbShell:
 *
 * <structname>FbShell</structname> provides a shell structure.
 */
struct _FbShell {
    FbIo            parent; 
    FbShellPrivate *priv;
};

struct _FbShellClass {
    FbIoClass parent;
};

/**
 * fb_shell_new:
 *
 * Creates  a new #FbShell.
 *
 * Returns: A newly allocated #FbShell
 */
FbShell *        fb_shell_new                   (FbShellManager *manager,
                                                 FbTermObject   *fbterm);

/**
 * fb_shell_switch_vt:
 * @shell: A #FbTerm
 * @enter: If activate
 * @peer: If peer shell exists
 *
 * Open tty and enable the shell if @enter is %TRUE.
 */
void             fb_shell_switch_vt             (FbShell  *shell,
                                                 gboolean  enter,
                                                 FbShell  *peer);

/**
 * fb_shell_init_shell_process:
 *  @shell: A #FbShell
 *
 * Initialize the shell process.
 */
void             fb_shell_init_shell_process    (FbShell *shell);

/**
 * fb_shell_key_input:
 *  @shell: A #FbShell
 *  @buff: an input string.
 *  @length: an input length.
 *
 * Input a string on the shell.
 */
void             fb_shell_key_input             (FbShell     *shell,
                                                 const gchar *buff,
                                                 guint        length);

/**
 * fb_shell_child_process_exited:
 *  @shell: A #FbShell
 *  @pid: A process id.
 *
 * %TRUE if the fork is done, %FALSE otherwise.
 */
gboolean         fb_shell_child_process_exited  (FbShell *shell,
                                                 int      pid);

G_END_DECLS
#endif
