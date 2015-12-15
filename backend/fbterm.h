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

#ifndef __FBTERM_OBJECT_H_
#define __FBTERM_OBJECT_H_

#include <glib-object.h>

#include "fbio.h"

/*
 * Type macros.
 */

/* define GOBJECT macros */
#define FB_TYPE_SIGNAL_IO                       (fb_signal_io_get_type ())
#define FB_SIGNAL_IO(o)                         (G_TYPE_CHECK_INSTANCE_CAST ((o), FB_TYPE_SIGNAL_IO, FbSignalIo))
#define FB_SIGNAL_IO_CLASS(k)                   (G_TYPE_CHECK_CLASS_CAST ((k), FB_TYPE_SIGNAL_IO, FbSignalIoClass))
#define FB_IS_SIGNAL_IO(o)                      (G_TYPE_CHECK_INSTANCE_TYPE ((o), FB_TYPE_SIGNAL_IO))
#define FB_IS_SIGNAL_IO_CLASS(k)                (G_TYPE_CHECK_CLASS_TYPE ((k), FB_TYPE_SIGNAL_IO))

#define FBTERM_TYPE_OBJECT                      (fbterm_object_get_type ())
#define FBTERM_OBJECT(o)                        (G_TYPE_CHECK_INSTANCE_CAST ((o), FBTERM_TYPE_OBJECT, FbTermObject))
#define FBTERM_OBJECT_CLASS(k)                  (G_TYPE_CHECK_CLASS_CAST ((k), FBTERM_TYPE_OBJECT, FbTermObjectClass))
#define FBTERM_IS_OBJECT(o)                     (G_TYPE_CHECK_INSTANCE_TYPE ((o), FBTERM_TYPE_OBJECT))
#define FBTERM_IS_OBJECT_CLASS(k)               (G_TYPE_CHECK_CLASS_TYPE ((k), FBTERM_TYPE_OBJECT))



G_BEGIN_DECLS
typedef struct _FbSignalIo FbSignalIo;
typedef struct _FbSignalIoPrivate FbSignalIoPrivate;
typedef struct _FbSignalIoClass FbSignalIoClass;
typedef struct _FbTermObject FbTermObject;
typedef struct _FbTermObjectPrivate FbTermObjectPrivate;
typedef struct _FbTermObjectClass FbTermObjectClass;

/**
 * FbSignalIo:
 *
 * <structname>FbSignalIo</structname> provides a signal structure
 * related with the frame buffer.
 */
struct _FbSignalIo {
    FbIo parent;

    FbSignalIoPrivate *priv;
};

struct _FbSignalIoClass {
    FbIoClass parent;
};

/**
 * FbTermObject:
 *
 * <structname>FbTermObject</structname> provides a structure
 * related with the frame buffer.
 */
struct _FbTermObject {
    //GInitiallyUnowned parent;
    IBusObject parent;

    FbTermObjectPrivate *priv;
};

struct _FbTermObjectClass {
    //GInitiallyUnownedClass parent;
    IBusObjectClass parent;
};

GType            fbterm_object_get_type           (void);

/**
 * fbterm_object_new:
 *
 * Creates  a new #FbTermObject.
 *
 * Returns: A newly allocated #FbTermObject
 */
FbTermObject    *fbterm_object_new                (void);

/**
 * fbterm_proess_signal:
 * @fbterm: A #FbTermObject
 * @signo: a signal number
 *
 * Handle the signal events.
 */
void             fbterm_object_process_signal     (FbTermObject    *fbterm, 
                                                   int              signo);

void             fbterm_object_run                (FbTermObject    *fbterm);
void             fbterm_object_exit               (FbTermObject    *fbterm);
void             fbterm_object_init_child_process (FbTermObject    *fbterm);


GType            fb_signal_io_get_type            (void);

/**
 * fb_signal_io_new:
 * @sigmak: signal mask
 * @fbterm: stored #FbTermObject
 *
 * Creates  a new #FbSignalIo.
 *
 * Returns: A newly allocated #FbSignalIo
 */
FbSignalIo      *fb_signal_io_new                 (sigset_t         sigmask,
                                                   FbTermObject    *fbterm);

G_END_DECLS
#endif
