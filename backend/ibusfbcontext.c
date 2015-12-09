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

#include "fbkey.h"
#include "ibusfbcontext.h"
//#include "keymap.h"

enum {
    PROP_0 = 0,
    PROP_MANAGER
};

struct _IBusFbContextPrivate {
    IBusInputContext *ibuscontext;
    FbKey            *key;
    guint32           modifier_state;
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

static void
ibus_input_context_commit_text_cb (IBusInputContext *ibuscontext,
                                   IBusText         *text,
                                   IBusFbContext    *context)
{
}

static void
ibus_fb_context_init (IBusFbContext *context)
{
    IBusFbContextPrivate *priv =
            ibus_fb_context_get_instance_private (context);
    context->priv = priv;

    priv->key = fb_key_new ();
    if (ibus_bus_is_connected (_bus))
        ibus_fb_create_input_context (context);

    g_signal_connect (_bus, "connected",
                      G_CALLBACK (ibus_fb_bus_connected_cb), context);
}

static void
ibus_fb_context_class_init (IBusFbContextClass *class)
{
    class->filter_keypress = ibus_fb_context_real_filter_keypress;
    /* FIXME: g_main_loop() is needed for async */
    _bus = ibus_bus_new ();
}

#if 0
static guint32
linux_keysym_to_ibus_keyval (guint16 keysym,
                             gchar   keycode)
{
    guint32 kval = KVAL (keysym);
    guint32 keyval = 0;

    switch (KTYP (keysym)) {
    case KT_LATIN:
    case KT_LETTER:
        keyval = linux_to_x[kval];
        break;
    case KT_FN:
        if (kval <= 19)
            keyval = IBUS_KEY_F1 + kval;
        else switch (keysym) {
        case K_FIND:
            keyval = IBUS_KEY_Home; /* or IBUS_KEY_Find */
            break;
        case K_INSERT:
            keyval = IBUS_KEY_Insert;
            break;
        case K_REMOVE:
            keyval = IBUS_KEY_Delete;
            break;
        case K_SELECT:
            keyval = IBUS_KEY_End; /* or IBUS_KEY_Select */
            break;
        case K_PGUP:
            keyval = IBUS_KEY_Prior;
            break;
        case K_PGDN:
            keyval = IBUS_KEY_Next;
            break;
        case K_HELP:
            keyval = IBUS_KEY_Help;
            break;
        case K_DO:
            keyval = IBUS_KEY_Execute;
            break;
        case K_PAUSE:
            keyval = IBUS_KEY_Pause;
            break;
        case K_MACRO:
            keyval = IBUS_KEY_Menu;
            break;
        default:;
        }
        break;
    case KT_SPEC:
        switch (keysym) {
        case K_ENTER:
            keyval = IBUS_KEY_Return;
            break;
        case K_BREAK:
            keyval = IBUS_KEY_Break;
            break;
        case K_CAPS:
            keyval = IBUS_KEY_Caps_Lock;
            break;
        case K_NUM:
            keyval = IBUS_KEY_Num_Lock;
            break;
        case K_HOLD:
            keyval = IBUS_KEY_Scroll_Lock;
            break;
        case K_COMPOSE:
            keyval = IBUS_KEY_Multi_key;
            break;
        default:;
        }
        break;
    case KT_PAD:
        switch (keysym) {
        case K_PPLUS:
            keyval = IBUS_KEY_KP_Add;
            break;
        case K_PMINUS:
            keyval = IBUS_KEY_KP_Subtract;
            break;
        case K_PSTAR:
            keyval = IBUS_KEY_KP_Multiply;
            break;
        case K_PSLASH:
            keyval = IBUS_KEY_KP_Divide;
            break;
        case K_PENTER:
            keyval = IBUS_KEY_KP_Enter;
            break;
        case K_PCOMMA:
            keyval = IBUS_KEY_KP_Separator;
            break;
        case K_PDOT:
            keyval = IBUS_KEY_KP_Decimal;
            break;
        case K_PPLUSMINUS:
            keyval = IBUS_KEY_KP_Subtract;
            break;
        default:
            if (kval <= 9)
                keyval = IBUS_KEY_KP_0 + kval;
        }
        break;

        /*
         * KT_DEAD keys are for accelerated diacritical creation.
         */
    case KT_DEAD:
        switch (keysym) {
        case K_DGRAVE:
            keyval = IBUS_KEY_dead_grave;
            break;
        case K_DACUTE:
            keyval = IBUS_KEY_dead_acute;
            break;
        case K_DCIRCM:
            keyval = IBUS_KEY_dead_circumflex;
            break;
        case K_DTILDE:
            keyval = IBUS_KEY_dead_tilde;
            break;
        case K_DDIERE:
            keyval = IBUS_KEY_dead_diaeresis;
            break;
        default:;
        }
        break;
    case KT_CUR:
        switch (keysym) {
        case K_DOWN:
            keyval = IBUS_KEY_Down;
            break;
        case K_LEFT:
            keyval = IBUS_KEY_Left;
            break;
        case K_RIGHT:
            keyval = IBUS_KEY_Right;
            break;
        case K_UP:
            keyval = IBUS_KEY_Up;
            break;
        default:;
        }
        break;
    case KT_SHIFT:
        switch (keysym) {
        case K_ALTGR:
            keyval = IBUS_KEY_Alt_R;
            break;
        case K_ALT:
            keyval = (keycode == 0x64 ?  IBUS_KEY_Alt_R : IBUS_KEY_Alt_L);
            break;
        case K_CTRL:
            keyval = (keycode == 0x61 ?
                      IBUS_KEY_Control_R : IBUS_KEY_Control_L);
            break;
        case K_CTRLL:
            keyval = IBUS_KEY_Control_L;
            break;
        case K_CTRLR:
            keyval = IBUS_KEY_Control_R;
            break;
        case K_SHIFT:
            keyval = (keycode == 0x36 ?  IBUS_KEY_Shift_R : IBUS_KEY_Shift_L);
            break;
        case K_SHIFTL:
            keyval = IBUS_KEY_Shift_L;
            break;
        case K_SHIFTR:
            keyval = IBUS_KEY_Shift_R;
            break;
        default:;
        }
        break;
        /*
         * KT_ASCII keys accumulate a 3 digit decimal number that gets
         * emitted when the shift state changes. We can't emulate that.
         */
    case KT_ASCII:
        break;
    case KT_LOCK:
        if (keysym == K_SHIFTLOCK)
            keyval = IBUS_KEY_Shift_Lock;
        break;
    default:;
    }

    return keyval;
}
#endif

static void
fb_key_to_keyval (gchar    key,
                  guint32 *keyval,
                  guint32 *modifiers)
{
    switch (key) {
    case 0:
        *keyval = IBUS_KEY_space;
        *modifiers = IBUS_CONTROL_MASK;
        break;
    case '\r':
        *keyval = IBUS_KEY_Return;
        break;
    default:
        *keyval = key;
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

static void
ibus_fb_context_calculate_modifiers (IBusFbContext *context,
                                     guint32        keyval,
                                     char           down)
{
    IBusFbContextPrivate *priv;
    guint32 mask = 0;

    g_return_if_fail (IBUS_IS_FB_CONTEXT (context));

    priv = context->priv;

    switch (keyval) {
    case IBUS_KEY_Shift_L:
    case IBUS_KEY_Shift_R:
        mask = IBUS_SHIFT_MASK;
        break;
    case IBUS_KEY_Control_L:
    case IBUS_KEY_Control_R:
        mask = IBUS_CONTROL_MASK;
        break;
    case IBUS_KEY_Alt_L:
    case IBUS_KEY_Alt_R:
    case IBUS_KEY_Meta_L:
        mask = IBUS_MOD1_MASK;
        break;
    case IBUS_KEY_Super_L:
    case IBUS_KEY_Hyper_L:
        mask = IBUS_MOD4_MASK;
        break;
    case IBUS_KEY_ISO_Level3_Shift:
    case IBUS_KEY_Mode_switch:
        mask = IBUS_MOD5_MASK;
        break;
    default:;
    }

    if (mask) {
        if (down)
            priv->modifier_state |= mask;
        else
            priv->modifier_state &= ~mask;
    }
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
        gboolean processed;

        fb_key_to_keyval (code, &keyval, &modifiers);
        processed = ibus_input_context_process_key_event (
                priv->ibuscontext,
                keyval,
                code,
                modifiers | (down ? 0 : IBUS_RELEASE_MASK));
        //if (!processed)
        //        ibus_fbterm_put_im_text (ibus_fbterm, string, strlen (string));
        if (!processed && dispatched)
            *dispatched[j++] = buff[i];

        ibus_input_context_process_key_event (
                priv->ibuscontext,
                keyval,
                code,
                modifiers | IBUS_RELEASE_MASK);

        ibus_fb_context_calculate_modifiers (context, keyval, down);
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
