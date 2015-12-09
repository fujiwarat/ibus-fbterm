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

#ifndef __FB_SHELL_MANAGER_H_
#define __FB_SHELL_MANAGER_H_

#include <glib-object.h>
#include <ibus.h>

#if 0
#include "screen.h"
#endif
#include "fbshell.h"
#include "fbterm.h"

/*
 * Type macros.
 */

/* define GOBJECT macros */
#define FB_TYPE_SHELL_MANAGER                   (fb_shell_manager_get_type ())
#define FB_SHELL_MANAGER(o)                     (G_TYPE_CHECK_INSTANCE_CAST ((o), FB_TYPE_SHELL_MANAGER, FbShellManager))
#define FB_SHELL_MANAGER_CLASS(k)               (G_TYPE_CHECK_CLASS_CAST ((k), FB_TYPE_SHELL_MANAGER, FbShellManagerClass))
#define FB_IS_SHELL_MANAGER(o)                  (G_TYPE_CHECK_INSTANCE_TYPE ((o), FB_TYPE_SHELL_MANAGER))
#define FB_IS_SHELL_MANAGER_CLASS(k)            (G_TYPE_CHECK_CLASS_TYPE ((k), FB_TYPE_SHELL_MANAGER))


G_BEGIN_DECLS
typedef struct _FbShellManager FbShellManager;
typedef struct _FbShellManagerPrivate FbShellManagerPrivate;
typedef struct _FbShellManagerClass FbShellManagerClass;

/**
 * FbShellManager:
 *
 * <structname>FbShellManager</structname> provides a shell management.
 */
struct _FbShellManager {
    IBusObject parent; 
    FbShellManagerPrivate *priv;
};

struct _FbShellManagerClass {
    IBusObjectClass parent;
};


GType            fb_shell_manager_get_type        (void);

/**
 * fb_shell_manager_new:
 *
 * Creates  a new #FbShellManager.
 *
 * Returns: A newly allocated #FbShellManager
 */
FbShellManager  *fb_shell_manager_new           (FbScreen *screen,
                                                 FbTermObject *fbterm,
                                                 FbIoDispatcher *dispatcher);

void             fb_shell_manager_create_shell  (FbShellManager *shell_manager);
void             fb_shell_manager_delete_shell  (FbShellManager *shell_manager);
void             fb_shell_manager_next_shell    (FbShellManager *shell_manager);
void             fb_shell_manager_prev_shell    (FbShellManager *shell_manager);
void             fb_shell_manager_switch_shell  (FbShellManager *shell_manager,
                                                 int             num);
#if 0
void             fb_shell_manager_draw_cursor   (FbShellManager *shell_manager);
#endif
void             fb_shell_manager_switch_vc     (FbShellManager *shell_manager,
                                                 gboolean        enter);
void             fb_shell_manager_shell_exited  (FbShellManager *shell_manager,
                                                 FbShell        *shell);
gboolean         fb_shell_manager_set_active    (FbShellManager *shell_manager,
                                                 FbShell        *shell);
#if 0
void             fb_shell_manager_redraw        (FbShellManager *shell_manager,
                                                 short int       x,
                                                 short int       y,
                                                 short int       w,
                                                 short int       h);
#endif
void             fb_shell_manager_child_process_exited
                                                (FbShellManager *shell_manager,
                                                 int             pid);
FbShell *        fb_shell_manager_active_shell  (FbShellManager *shell_manager);

G_END_DECLS
#endif
