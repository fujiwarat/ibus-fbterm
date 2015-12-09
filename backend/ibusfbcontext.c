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
#include <ibus.h>

#include <linux/keyboard.h>

#include "ibusfbcontext.h"
#include "marshalers.h"
//#include "keymap.h"

enum {
    COMMIT,
    PREEDIT_CHANGED,
    LAST_SIGNAL
};

struct _IBusFbContextPrivate {
    IBusInputContext *ibuscontext;
};

G_DEFINE_TYPE_WITH_PRIVATE (IBusFbContext,
                            ibus_fb_context,
                            G_TYPE_INITIALLY_UNOWNED);

static guint ibus_fb_context_real_filter_keypress  (IBusFbContext *context,
                                                    const gchar   *buff,
                                                    guint          length,
                                                    gchar        **dispatched);
static void  ibus_fb_create_input_context          (IBusFbContext *context);
static void  ibus_fb_bus_connected_cb              (IBusBus       *bus,
                                                    IBusFbContext *context);


static IBusBus *_bus;
static guint context_signals[LAST_SIGNAL] = { 0 };

static void
ibus_input_context_commit_text_cb (IBusInputContext *ibuscontext,
                                   IBusText         *text,
                                   IBusFbContext    *context)
{
    g_return_if_fail (IBUS_IS_FB_CONTEXT (context));

    g_signal_emit (context,
                   context_signals[COMMIT], 0,
                   text);
}

static void
ibus_input_context_update_preedit_text_cb (IBusInputContext *ibuscontext,
                                           IBusText         *text,
                                           int               cursor_pos,
                                           gboolean          visible,
                                           IBusFbContext    *context)
{
    g_return_if_fail (IBUS_IS_FB_CONTEXT (context));

    g_signal_emit (context,
                   context_signals[PREEDIT_CHANGED], 0,
                   text,
                   cursor_pos,
                   visible);
}

static void
ibus_fb_context_init (IBusFbContext *context)
{
    IBusFbContextPrivate *priv =
            ibus_fb_context_get_instance_private (context);
    context->priv = priv;

    if (ibus_bus_is_connected (_bus))
        ibus_fb_create_input_context (context);

    g_signal_connect (_bus, "connected",
                      G_CALLBACK (ibus_fb_bus_connected_cb), context);
}

static void
ibus_fb_context_class_init (IBusFbContextClass *class)
{
#define I_ g_intern_static_string
    GObjectClass *gobject_class = G_OBJECT_CLASS (class);

    class->filter_keypress = ibus_fb_context_real_filter_keypress;

    context_signals[COMMIT] =
            g_signal_new (I_("commit"),
            G_TYPE_FROM_CLASS (gobject_class),
            G_SIGNAL_RUN_LAST,
            0,
            NULL, NULL,
            _fb_marshal_VOID__OBJECT,
            G_TYPE_NONE, 1,
            IBUS_TYPE_TEXT);

    context_signals[PREEDIT_CHANGED] =
            g_signal_new (I_("preedit-changed"),
            G_TYPE_FROM_CLASS (gobject_class),
            G_SIGNAL_RUN_LAST,
            0,
            NULL, NULL,
            _fb_marshal_VOID__OBJECT_INT_BOOLEAN,
            G_TYPE_NONE, 3,
            IBUS_TYPE_TEXT,
            G_TYPE_INT,
            G_TYPE_BOOLEAN);

    /* FIXME: g_main_loop() is needed for async */
    _bus = ibus_bus_new ();
#undef I_
}

static gboolean
fb_control_key_to_keyval (const gchar *buff,
                          guint        length,
                          guint32     *keyval,
                          guint32     *modifiers)
{
    g_assert (keyval != NULL && modifiers != NULL);
    g_return_val_if_fail (buff != NULL, FALSE);

    if (length == 2 && buff[0] == '\033' && buff[1] == ' ') {
        *keyval = IBUS_KEY_space;
        *modifiers = IBUS_SUPER_MASK;
        return TRUE;
    }

    if (length < 3)
        return FALSE;
    if (buff[0] != '\033' || buff[1] != '[')
        return FALSE;

    switch (buff[2]) {
    case 'A':
        *keyval = IBUS_KEY_Up;
        return TRUE;
    case 'B':
        *keyval = IBUS_KEY_Down;
        return TRUE;
    case 'C':
        *keyval = IBUS_KEY_Right;
        return TRUE;
    case 'D':
        *keyval = IBUS_KEY_Left;
        return TRUE;
    case 'P':
        *keyval = IBUS_KEY_Pause;
        return TRUE;
    case '1':
        if (length == 4 && buff[3] == '~') {
            *keyval = IBUS_KEY_Home;
            return TRUE;
        }
    case '2':
        if (length == 4 && buff[3] == '~') {
            *keyval = IBUS_KEY_Insert;
            return TRUE;
        }
    case '3':
        if (length == 4 && buff[3] == '~') {
            *keyval = IBUS_KEY_Delete;
            return TRUE;
        }
    case '4':
        if (length == 4 && buff[3] == '~') {
            *keyval = IBUS_KEY_End;
            return TRUE;
        }
    case '5':
        if (length == 4 && buff[3] == '~') {
            *keyval = IBUS_KEY_Page_Up;
            return TRUE;
        }
    case '6':
        if (length == 4 && buff[3] == '~') {
            *keyval = IBUS_KEY_Page_Down;
            return TRUE;
        }
        break;
    case '[':
        if (length == 4 && buff[3] >= 'A') {
            *keyval = IBUS_KEY_F1 + (buff[3] - 'A');
            return TRUE;
        }
        break;
    default:;
    }

    return FALSE;
}

static void
fb_char_to_keyval (gchar    ch,
                   guint32 *keyval,
                   guint32 *modifiers)
{
    switch (ch) {
    case 0:
        *keyval = IBUS_KEY_space;
        *modifiers = IBUS_CONTROL_MASK;
        break;
    case 0x1b:
        *keyval = IBUS_KEY_Escape;
        break;
    case 0x7f:
        *keyval = IBUS_KEY_BackSpace;
        break;
    case '\r':
        *keyval = IBUS_KEY_Return;
        break;
    case '\t':
        *keyval = IBUS_KEY_Tab;
        break;
    default:
        *keyval = ch;
    }
}

static void
ibus_fb_create_input_context (IBusFbContext *context)
{
    IBusFbContextPrivate *priv;

    g_return_if_fail (IBUS_IS_FB_CONTEXT (context));

    priv = context->priv;

    /* FIXME: g_main_loop() is needed for async. */
    priv->ibuscontext = ibus_bus_create_input_context (_bus, "fbterm");

    g_signal_connect (priv->ibuscontext,
                      "commit-text",
                      G_CALLBACK (ibus_input_context_commit_text_cb),
                      context);
    g_signal_connect (priv->ibuscontext,
                      "update-preedit-text",
                      G_CALLBACK (ibus_input_context_update_preedit_text_cb),
                      context);
    ibus_input_context_set_capabilities (priv->ibuscontext,
                                         IBUS_CAP_AUXILIARY_TEXT |
                                         IBUS_CAP_LOOKUP_TABLE |
                                         IBUS_CAP_PROPERTY |
                                         IBUS_CAP_FOCUS |
                                         IBUS_CAP_PREEDIT_TEXT);
}

static void
ibus_fb_bus_connected_cb (IBusBus *bus,
                          IBusFbContext *context)
{
    ibus_fb_create_input_context (context);
}

static guint
ibus_fb_context_real_filter_keypress (IBusFbContext *context,
                                      const gchar   *buff,
                                      guint          length,
                                      gchar        **dispatched)
{
    IBusFbContextPrivate *priv;
    guint i, j;

    g_return_val_if_fail (IBUS_IS_FB_CONTEXT (context), length);

    if (length == 0)
        return length;

    priv = context->priv;

    g_return_val_if_fail (IBUS_IS_INPUT_CONTEXT (priv->ibuscontext), length);

    if (dispatched)
        *dispatched = g_new0 (gchar, length);

    for (i = 0, j = 0; i < length; i++) {
        gchar down = !(buff[i] & 0x80);
        gchar code = buff[i] & 0x7f;
        guint32 keyval = 0;
        guint32 modifiers = 0;
        gboolean is_control = FALSE;
        gboolean processed;

        if (i == 0 &&
            fb_control_key_to_keyval (buff, length, &keyval, &modifiers))
            is_control = TRUE;
        else
            fb_char_to_keyval (code, &keyval, &modifiers);

        processed = ibus_input_context_process_key_event (
                priv->ibuscontext,
                keyval,
                code,
                modifiers | (down ? 0 : IBUS_RELEASE_MASK));
        if (is_control) {
            if (!processed && dispatched) {
                for (j = 0; j < length; j++)
                    (*dispatched)[j] = buff[j];
            }
            i += length;
        } else {
            if (!processed && dispatched)
                (*dispatched)[j++] = buff[i];
        }

        ibus_input_context_process_key_event (
                priv->ibuscontext,
                keyval,
                code,
                modifiers | IBUS_RELEASE_MASK);
    }

    if (j == 0 && dispatched) {
        g_free (*dispatched);
        *dispatched = NULL;
    }
    return j;
}

IBusFbContext *
ibus_fb_context_new (void)
{
    return g_object_new (IBUS_TYPE_FB_CONTEXT,
                         NULL);
}
