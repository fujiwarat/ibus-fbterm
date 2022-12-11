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

#include <string.h>

#include "fbshell.h"
#include "fbshellman.h"
#include "fbterm.h"

#define NR_SHELLS 10
#define SHELL_ANY ((FbShell *)-1)

struct _FbShellManagerPrivate {
    gboolean        vc_current;
    int             shell_count;
    int             cur_shell;
    FbShell        *active_shell;
    FbShell        *shell_list[NR_SHELLS];
    FbTermObject   *fbterm;
};

G_DEFINE_TYPE_WITH_PRIVATE (FbShellManager,
                            fb_shell_manager,
                            IBUS_TYPE_OBJECT);

static void         fb_shell_manager_destroy (IBusObject *object);
static int          fb_shell_manager_get_index (FbShellManager *shell_manager,
                                                FbShell        *shell,
                                                gboolean        forward,
                                                gboolean        stepfirst);

static void
fb_shell_manager_init (FbShellManager *shell_manager)
{
    FbShellManagerPrivate *priv =
            fb_shell_manager_get_instance_private (shell_manager);
    shell_manager->priv = priv;

    priv->vc_current = FALSE;
    priv->shell_count = 0;
    priv->cur_shell = 0;
    priv->active_shell = NULL;
    memset (priv->shell_list, 0, sizeof (priv->shell_list));
}

static void
fb_shell_manager_class_init (FbShellManagerClass *class)
{
    IBUS_OBJECT_CLASS (class)->destroy = fb_shell_manager_destroy;
}

static void
fb_shell_manager_destroy (IBusObject *object)
{
}

static int
fb_shell_manager_get_index (FbShellManager *shell_manager,
                            FbShell        *shell,
                            gboolean        forward,
                            gboolean        stepfirst)
{
    FbShellManagerPrivate *priv;
    int index, temp, i;

    g_return_if_fail (FB_IS_SHELL_MANAGER (shell_manager));

#define STEP() do { \
    if (forward) temp++; \
    else temp--; \
} while (0)

    priv = shell_manager->priv;
    temp = priv->cur_shell + NR_SHELLS;

    if (stepfirst)
        STEP ();

    for (i = NR_SHELLS; i--;) {
        index = temp % NR_SHELLS;
        if ((shell == SHELL_ANY && priv->shell_list[index]) ||
            shell == priv->shell_list[index])
            break;
        STEP ();
    }

    return index;
}

FbShellManager *
fb_shell_manager_new (FbTermObject *fbterm)
{
    FbShellManager *shell_manager = g_object_new (FB_TYPE_SHELL_MANAGER, NULL);
    shell_manager->priv->fbterm = fbterm;
    return shell_manager;
} 

void
fb_shell_manager_create_shell (FbShellManager *shell_manager)
{
    FbShellManagerPrivate *priv;
    int index;

    g_return_if_fail (FB_IS_SHELL_MANAGER (shell_manager));

    priv = shell_manager->priv;

    if (priv->shell_count == NR_SHELLS)
        return;
    priv->shell_count++;
    index = fb_shell_manager_get_index (shell_manager, 0, TRUE, FALSE);
    priv->shell_list[index] = fb_shell_new (shell_manager,
                                            priv->fbterm);
    fb_shell_manager_switch_shell (shell_manager, index);
}

void
fb_shell_manager_delete_shell (FbShellManager *shell_manager)
{
    FbShellManagerPrivate *priv;
    int i;

    g_return_if_fail (FB_IS_SHELL_MANAGER (shell_manager));

    priv = shell_manager->priv;
    i = priv->cur_shell;
    if (priv->shell_list[i]) {
        g_object_unref (priv->shell_list[i]);
        priv->shell_list[i] = NULL;
    }
}

void
fb_shell_manager_next_shell (FbShellManager *shell_manager)
{
    fb_shell_manager_switch_shell (
            shell_manager,
            fb_shell_manager_get_index (shell_manager, SHELL_ANY, TRUE, TRUE));
}

void
fb_shell_manager_prev_shell (FbShellManager *shell_manager)
{
    fb_shell_manager_switch_shell (
            shell_manager,
            fb_shell_manager_get_index (shell_manager, SHELL_ANY, FALSE, TRUE));
}

void
fb_shell_manager_switch_shell (FbShellManager *shell_manager,
                               int             num)
{
    FbShellManagerPrivate *priv;

    if (num >= NR_SHELLS)
        return;

    g_return_if_fail (FB_IS_SHELL_MANAGER (shell_manager));

    priv = shell_manager->priv;
    priv->cur_shell = num;
    if (priv->vc_current) {
        fb_shell_manager_set_active (shell_manager,
                                     priv->shell_list[num]);
    }
}

void
fb_shell_manager_switch_vc (FbShellManager *shell_manager,
                            gboolean        enter)
{
    FbShellManagerPrivate *priv;

    g_return_if_fail (FB_IS_SHELL_MANAGER (shell_manager));

    priv = shell_manager->priv;
    priv->vc_current = enter;
    fb_shell_manager_set_active (
            shell_manager,
            enter ? priv->shell_list[priv->cur_shell] : 0);
}

void
fb_shell_manager_shell_exited (FbShellManager *shell_manager,
                               FbShell        *shell)
{
    FbShellManagerPrivate *priv;
    int index;

    if (!shell)
        return;

    g_return_if_fail (FB_IS_SHELL_MANAGER (shell_manager));
    priv = shell_manager->priv;
    index = fb_shell_manager_get_index (shell_manager, shell, TRUE, FALSE);
    priv->shell_list[index] = NULL;

    if (index == priv->cur_shell)
        fb_shell_manager_prev_shell (shell_manager);

    if (!--priv->shell_count)
        fbterm_object_exit (priv->fbterm);
}

gboolean
fb_shell_manager_set_active (FbShellManager *shell_manager,
                             FbShell        *shell)
{
    FbShellManagerPrivate *priv;
    FbShell *old_active_shell;

    g_return_val_if_fail (FB_IS_SHELL_MANAGER (shell_manager), FALSE);

    priv = shell_manager->priv;

    if (priv->active_shell == shell)
        return FALSE;

    if (priv->active_shell)
        fb_shell_switch_vt (priv->active_shell, FALSE, shell);

    old_active_shell = priv->active_shell;
    priv->active_shell = shell;

    if (priv->active_shell)
        fb_shell_switch_vt (priv->active_shell, TRUE, old_active_shell);

    return TRUE;
}

void
fb_shell_manager_child_process_exited (FbShellManager *shell_manager,
                                       int             pid)
{
    FbShellManagerPrivate *priv;
    int i;

    g_return_if_fail (FB_IS_SHELL_MANAGER (shell_manager));

    priv = shell_manager->priv;

    for (i = 0; i <= NR_SHELLS; i++) {
        if (priv->shell_list[i] &&
        fb_shell_child_process_exited (priv->shell_list[i], pid))
        break;
    }
}

FbShell *
fb_shell_manager_active_shell (FbShellManager *shell_manager)
{
    g_return_if_fail (FB_IS_SHELL_MANAGER (shell_manager));

    return shell_manager->priv->active_shell;
}
