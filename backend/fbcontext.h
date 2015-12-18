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

#ifndef __FB_CONTEXT_H_
#define __FB_CONTEXT_H_

#include <glib-object.h>

/*
 * Type macros.
 */

/* define GOBJECT macros */
#define TYPE_FB_CONTEXT (fb_context_get_type ())
#define FB_CONTEXT(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), TYPE_FB_CONTEXT, FbContext))
#define IS_FB_CONTEXT(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), TYPE_FB_CONTEXT))
#define FB_CONTEXT_GET_INTERFACE(obj) (G_TYPE_INSTANCE_GET_INTERFACE ((obj), TYPE_FB_CONTEXT, FbContextIface))


G_BEGIN_DECLS
typedef struct _FbContext FbContext;
typedef struct _FbContextIface FbContextIface;

/**
 * FbContextIface:
 *
 * <structname>FbContextIface</structname> provides an interface of
 * FbContext.
 */
struct _FbContextIface {
    GTypeInterface parent_iface;

    guint      (*filter_keypress)                  (FbContext    *context,
                                                    const gchar  *buff,
                                                    guint         length,
                                                    gchar       **dispatched);
    void       (*load_settings)                    (FbContext    *context);

    gpointer dummy[5];
};

GType            fb_context_get_type               (void) G_GNUC_CONST;

G_END_DECLS
#endif
