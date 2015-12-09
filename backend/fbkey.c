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

#include <linux/kd.h>
#include <linux/keyboard.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include "fbkey.h"
#include "input_key.h"

struct _FbKeyPrivate {
    int                 npadch;
    short               shift_state;
    gchar               key_down[NR_KEYS];
    unsigned char       shift_down[NR_SHIFT];
    gchar               lock_state;
    gchar               cr_with_lf;
    gchar               applic_keypad;
    gchar               cursor_esco;
};

G_DEFINE_TYPE_WITH_PRIVATE (FbKey,
                            fb_key,
                            G_TYPE_INITIALLY_UNOWNED);

static void
fb_key_init (FbKey *key)
{
    FbKeyPrivate *priv = fb_key_get_instance_private (key);

    key->priv = priv;
}

static void
fb_key_class_init (FbKeyClass *class)
{
}

static guint16
fb_key_keypad_keysym_redirect(FbKey   *key,
                              guint16  keysym)
{
    FbKeyPrivate *priv;

    g_return_if_fail (FB_IS_KEY (key));

    priv = key->priv;

    if (priv->applic_keypad || KTYP (keysym) != KT_PAD ||
        KVAL (keysym) >= NR_PAD)
        return keysym;

    #define KL(val) K(KT_LATIN, val)
    static const guint16 num_map[] = {
                KL ('0'), KL ('1'), KL ('2'), KL ('3'), KL ('4'),
                KL ('5'), KL ('6'), KL ('7'), KL ('8'), KL ('9'),
                KL ('+'), KL ('-'), KL ('*'), KL ('/'), K_ENTER,
                KL (','), KL ('.'), KL ('?'), KL ('('), KL (')'),
                KL ('#')
    };

    static const guint16 fn_map[] = {
                K_INSERT, K_SELECT, K_DOWN, K_PGDN, K_LEFT,
                K_P5, K_RIGHT, K_FIND, K_UP, K_PGUP,
                KL ('+'), KL ('-'), KL ('*'), KL ('/'), K_ENTER,
                K_REMOVE, K_REMOVE, KL ('?'), KL ('('), KL (')'),
                KL ('#')
    };

    if (priv->lock_state & K_NUMLOCK)
        return num_map[keysym - K_P0];
    return fn_map[keysym - K_P0];
}

FbKey *
fb_key_new ()
{
    return g_object_new (FB_TYPE_KEY,
                         NULL);
}

void
fb_key_reset (FbKey *key)
{
    FbKeyPrivate *priv;

    g_return_if_fail (FB_IS_KEY (key));

    priv = key->priv;
    priv->npadch = -1;
    priv->shift_state = 0;
    memset (priv->key_down, 0, sizeof (char) * NR_KEYS);
    memset (priv->shift_down, 0, sizeof (char) * NR_SHIFT);
    ioctl (STDIN_FILENO, KDGKBLED, &priv->lock_state);
}

guint16
fb_key_keycode_to_keysym (FbKey   *key,
                          guint16  keycode,
                          char     down)
{
    FbKeyPrivate *priv;
    struct kbentry ke;

    g_return_val_if_fail (FB_IS_KEY (key), K_HOLE);

    priv = key->priv;

    if (keycode >= NR_KEYS)
        return K_HOLE;

    char rep = (down && priv->key_down[keycode]);
    priv->key_down[keycode] = down;

    ke.kb_table = priv->shift_state;
    ke.kb_index = keycode;

    if (ioctl(STDIN_FILENO, KDGKBENT, &ke) == -1)
        return K_HOLE;

    if (KTYP(ke.kb_value) == KT_LETTER && (priv->lock_state & K_CAPSLOCK)) {
        ke.kb_table = priv->shift_state ^ (1 << KG_SHIFT);
        if (ioctl(STDIN_FILENO, KDGKBENT, &ke) == -1)
            return K_HOLE;
    }

    if (ke.kb_value == K_HOLE || ke.kb_value == K_NOSUCHMAP)
        return K_HOLE;

    unsigned int value = KVAL (ke.kb_value);

    switch (KTYP (ke.kb_value)) {
    case KT_SPEC:
        switch (ke.kb_value) {
        case K_NUM:
            if (priv->applic_keypad)
                break;
        case K_BARENUMLOCK:
        case K_CAPS:
        case K_CAPSON:
            if (down && !rep) {
                if (value == KVAL (K_NUM) || value == KVAL (K_BARENUMLOCK))
                    priv->lock_state ^= K_NUMLOCK;
                else if (value == KVAL (K_CAPS))
                    priv->lock_state ^= K_CAPSLOCK;
                else if (value == KVAL (K_CAPSON))
                    priv->lock_state |= K_CAPSLOCK;

                ioctl (STDIN_FILENO, KDSKBLED, priv->lock_state);
            }
            break;

        default:;
        }
        break;

    case KT_SHIFT:
        if (value >= NR_SHIFT || rep)
            break;

        if (value == KVAL (K_CAPSSHIFT)) {
            value = KVAL (K_SHIFT);

            if (down && (priv->lock_state & K_CAPSLOCK)) {
                priv->lock_state &= ~K_CAPSLOCK;
                ioctl (STDIN_FILENO, KDSKBLED, priv->lock_state);
            }
        }

        if (down)
            priv->shift_down[value]++;
        else if (priv->shift_down[value])
            priv->shift_down[value]--;

        if (priv->shift_down[value])
            priv->shift_state |= (1 << value);
        else
            priv->shift_state &= ~(1 << value);

        break;

    case KT_LATIN:
    case KT_LETTER:
    case KT_FN:
    case KT_PAD:
    case KT_CONS:
    case KT_CUR:
    case KT_META:
    case KT_ASCII:
        break;

    default:
        g_warning ("not support!");
    }

    return ke.kb_value;
}

gchar *
fb_key_keysym_to_term_string (FbKey   *key,
                              guint16  keysym,
                              gchar    down)
{
    FbKeyPrivate *priv;
    static struct kbsentry kse;
    gchar *buff = (gchar *)kse.kb_string;
    *buff = '\0';

    g_return_val_if_fail (FB_IS_KEY (key), K_HOLE);

    priv = key->priv;

    if (KTYP (keysym) != KT_SHIFT && !down)
        return buff;

    keysym = fb_key_keypad_keysym_redirect (key, keysym);
    int index = 0;
    guint32 value = KVAL (keysym);

    switch (KTYP (keysym)) {
    case KT_LATIN:
    case KT_LETTER:
        if (value < KVAL (AC_START) || value > KVAL (AC_END))
            index = g_unichar_to_utf8 (value, buff);
        break;
    case KT_FN:
        kse.kb_func = value;
        ioctl (STDIN_FILENO, KDGKBSENT, &kse);
        index = strlen (buff);
        break;
    case KT_SPEC:
        if (keysym == K_ENTER) {
            buff[index++] = '\r';
            if (priv->cr_with_lf) buff[index++] = '\n';
        } else if (keysym == K_NUM && priv->applic_keypad) {
            buff[index++] = '\e';
            buff[index++] = 'O';
            buff[index++] = 'P';
        }
        break;
    case KT_PAD:
        if (priv->applic_keypad && !priv->shift_down[KG_SHIFT]) {
            if (value < NR_PAD) {
                static const char app_map[] = "pqrstuvwxylSRQMnnmPQS";

                buff[index++] = '\e';
                buff[index++] = 'O';
                buff[index++] = app_map[value];
            }
        } else if (keysym == K_P5 && !(priv->lock_state & K_NUMLOCK)) {
            buff[index++] = '\e';
            buff[index++] = (priv->applic_keypad ? 'O' : '[');
            buff[index++] = 'G';
        }
        break;
    case KT_CUR:
        if (value < 4) {
            static const char cur_chars[] = "BDCA";

            buff[index++] = '\e';
            buff[index++] = (priv->cursor_esco ? 'O' : '[');
            buff[index++] = cur_chars[value];
        }
        break;
    case KT_META: {
        long int flag;
        ioctl (STDIN_FILENO, KDGKBMETA, &flag);

        if (flag == K_METABIT) {
            buff[index++] = 0x80 | value;
        } else {
            buff[index++] = '\e';
            buff[index++] = value;
        }
        break;
    }
    case KT_SHIFT:
        if (!down && priv->npadch != -1) {
            index = g_unichar_to_utf8 (priv->npadch, buff);
            priv->npadch = -1;
        }
        break;
    case KT_ASCII:
        if (value < NR_ASCII) {
            int base = 10;

            if (value >= KVAL (K_HEX0)) {
                base = 16;
                value -= KVAL (K_HEX0);
            }

            if (priv->npadch == -1)
                priv->npadch = value;
            else
                priv->npadch = priv->npadch * base + value;
        }
        break;
    default:;
    }

    buff[index] = 0;
    return buff;
}
