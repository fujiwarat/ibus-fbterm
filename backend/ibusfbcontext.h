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

#ifndef __IBUS_FB_CONTEXT_H_
#define __IBUS_FB_CONTEXT_H_

#include <glib-object.h>

/*
 * Type macros.
 */

/* define GOBJECT macros */
#define IBUS_TYPE_FB_CONTEXT                    (ibus_fb_context_get_type ())
#define IBUS_FB_CONTEXT(o)                      (G_TYPE_CHECK_INSTANCE_CAST ((o), IBUS_TYPE_FB_CONTEXT, IBusFbContext))
#define IBUS_FB__CONTEXT_CLASS(k)               (G_TYPE_CHECK_CLASS_CAST ((k), IBUS_TYPE_FB_CONTEXT, IBusFbContextClass))
#define IBUS_IS_FB_CONTEXT(o)                   (G_TYPE_CHECK_INSTANCE_TYPE ((o), IBUS_TYPE_FB_CONTEXT))
#define IBUS_IS_FB_CONTEXT_CLASS(k)             (G_TYPE_CHECK_CLASS_TYPE ((k), IBUS_TYPE_FB_CONTEXT))
#define IBUS_FB_CONTEXT_GET_CLASS(o)            (G_TYPE_INSTANCE_GET_CLASS ((o), IBUS_TYPE_FB_CONTEXT, IBusFbContextClass))


G_BEGIN_DECLS
typedef struct _IBusFbContext IBusFbContext;
typedef struct _IBusFbContextPrivate IBusFbContextPrivate;
typedef struct _IBusFbContextClass IBusFbContextClass;

/**
 * IBusFbContext:
 *
 * <structname>IBusFbContext</structname> provides a frame buffer context
 * for IBus.
 */
struct _IBusFbContext {
    GInitiallyUnowned parent;

    IBusFbContextPrivate *priv;
};

struct _IBusFbContextClass {
    GInitiallyUnownedClass parent;

    guint    (*filter_keypress)                    (IBusFbContext *context,
                                                    const gchar   *buff,
                                                    guint          length,
                                                    gchar        **dispatched);
    gpointer dummy[5];
};

GType            ibus_fb_context_get_type          (void);

/**
 * ibus_fb_context_new:
 *
 * Creates  a new #IBusFbContext.
 *
 * Returns: A newly allocated #IBusFbContext
 */
IBusFbContext   *ibus_fb_context_new               (void);

G_END_DECLS
#endif
