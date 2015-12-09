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

#ifndef __FB_TTY_H_
#define __FB_TTY_H_

#include <glib-object.h>

#include "fbio.h"
#include "fbshellman.h"


/*
 * Type macros.
 */

/* define GOBJECT macros */
#define FB_TYPE_TTY                             (fb_tty_get_type ())
#define FB_TTY(o)                               (G_TYPE_CHECK_INSTANCE_CAST ((o), FB_TYPE_TTY, FbTty))
#define FB_TTY_CLASS(k)                         (G_TYPE_CHECK_CLASS_CAST ((k), FB_TYPE_TTY, FbTtyClass))
#define FB_IS_TTY(o)                            (G_TYPE_CHECK_INSTANCE_TYPE ((o), FB_TYPE_TTY))
#define FB_IS_TTY_CLASS(k)                      (G_TYPE_CHECK_CLASS_TYPE ((k), FB_TYPE_TTY))
#define FB_TTY_GET_CLASS(o)                     (G_TYPE_INSTANCE_GET_CLASS ((o), FB_TYPE_TTY, FbTtyClass))


G_BEGIN_DECLS
typedef struct _FbTty FbTty;
typedef struct _FbTtyPrivate FbTtyPrivate;
typedef struct _FbTtyClass FbTtyClass;

/**
 * FbTty:
 *
 * <structname>FbTty</structname> provides an TTY structure
 * related with the frame buffer.
 */
struct _FbTty {
    FbIo parent; 
    FbTtyPrivate *priv;
};

struct _FbTtyClass {
    FbIoClass parent;

    /**
     * FbTtyClass::ready_read:
     * @io: A #FbTty.
     * @buff: A read buffer
     * @length: A length of the buffer.
     *
     * The ::ready_read class method is to read
     * sequences from the frame buffer.
     */

    void (* ready_read)     (FbIo        *io,
                             const gchar *buff,
                             guint        length);

    gpointer dummy[5];
};

GType            fb_tty_get_type                   (void);

/**
 * fb_tty_new:
 * @dispatcher: #FbIoDispatcher;
 *
 * Creates  a new #FbTty.
 *
 * Returns: A newly allocated #FbTty
 */
FbTty           *fb_tty_new                        (FbIoDispatcher *dispatcher,
                                                    FbShellManager *manager);
void             fb_tty_switch_vc                  (FbTty   *tty,
                                                    gboolean enter);

G_END_DECLS
#endif
