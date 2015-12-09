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

#ifndef __FB_KEY_H_
#define __FB_KEY_H_

#include <glib-object.h>


/*
 * Type macros.
 */

/* define GOBJECT macros */
#define FB_TYPE_KEY                             (fb_key_get_type ())
#define FB_KEY(o)                               (G_TYPE_CHECK_INSTANCE_CAST ((o), FB_TYPE_KEY, FbKey))
#define FB_KEY_CLASS(k)                         (G_TYPE_CHECK_CLASS_CAST ((k), FB_TYPE_KEY, FbKeyClass))
#define FB_IS_KEY(o)                            (G_TYPE_CHECK_INSTANCE_TYPE ((o), FB_TYPE_KEY))
#define FB_IS_KEY_CLASS(k)                      (G_TYPE_CHECK_CLASS_TYPE ((k), FB_TYPE_KEY))



G_BEGIN_DECLS
typedef struct _FbKey FbKey;
typedef struct _FbKeyPrivate FbKeyPrivate;
typedef struct _FbKeyClass FbKeyClass;

/**
 * FbKey:
 *
 * <structname>FbKey</structname> provides a key structure
 * related with the frame buffer.
 */
struct _FbKey {
    GInitiallyUnowned parent;

    FbKeyPrivate *priv;
};

struct _FbKeyClass {
    GInitiallyUnownedClass parent;
};

GType            fb_key_get_type                  (void);

/**
 * fb_key_new:
 *
 * Creates  a new #FbKey.
 *
 * Returns: A newly allocated #FbKey
 */
FbKey           *fb_key_new                       (void);

/**
 * fb_key_reset:
 *
 * Reset the structure of #FbKey.
 */
void             fb_key_reset                     (FbKey           *key);

/**
 * fb_key_keycode_to_keysym:
 *
 * Convert keycode to keysym.
 */
guint16          fb_key_keycode_to_keysym         (FbKey           *key,
                                                   guint16          keycode,
                                                   gchar            down);

/**
 * fb_key_keysym_to_term_string:
 *
 * Convert keysym to the output string.
 */
gchar *          fb_key_keysym_to_term_string     (FbKey           *key,
                                                   guint16          keysym,
                                                   gchar            down);

G_END_DECLS
#endif
