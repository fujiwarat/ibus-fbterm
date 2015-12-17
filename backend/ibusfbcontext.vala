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

/* FIXME: Cannot use fbcontext.vapi because GObject.TypeInterface
 * does not exist in gobject-2.0.vapi
 */
public interface FbContext {
    public abstract uint filter_keypress   (string?          buff,
                                            uint             length,
                                            out string?      dispatched = null);

    public signal void cursor_position     (int              x,
                                            int              y);
    public signal void commit              (IBus.Text        text);
    public signal void preedit_changed     (IBus.Text        text,
                                            uint             cursor_pos,
                                            bool             visible);
    public signal void update_lookup_table (IBus.LookupTable table,
                                            bool             visible);
}

class IBusFbContext : GLib.InitiallyUnowned, FbContext {
    private IBus.Bus m_bus;
    private IBus.InputContext m_ibuscontext;

    public IBusFbContext() {
        m_bus = new IBus.Bus ();
        if (m_bus.is_connected())
            create_input_context();

        m_bus.connected.connect((bus) => {
            create_input_context();
        });
    }

    private void create_input_context() {
        /* FIXME: g_main_loop() is needed for async. */
        m_ibuscontext = m_bus.create_input_context ("fbterm");

        m_ibuscontext.commit_text.connect(commit_text_cb);
        m_ibuscontext.update_preedit_text.connect(update_preedit_text_cb);
        m_ibuscontext.update_lookup_table.connect(update_lookup_table_cb);
        m_ibuscontext.set_capabilities(IBus.Capabilite.AUXILIARY_TEXT |
                                       IBus.Capabilite.LOOKUP_TABLE |
                                       IBus.Capabilite.PROPERTY |
                                       IBus.Capabilite.FOCUS |
                                       IBus.Capabilite.PREEDIT_TEXT);
    }

    private void commit_text_cb(IBus.Text text) {
        commit(text);
    }

    private void update_preedit_text_cb(IBus.Text text,
                                        uint      cursor_pos,
                                        bool      visible) {
        preedit_changed(text, cursor_pos, visible);
    }

    private void update_lookup_table_cb(IBus.LookupTable table,
                                        bool             visible) {
        update_lookup_table(table, visible);
    }

    private bool control_key_to_keyval(string?    buff,
                                       uint       length,
                                       out uint32 keyval,
                                       out uint32 modifiers) {

        keyval = 0;
        modifiers = 0;

        GLib.return_val_if_fail(buff != null, false);

        if (length == 2 && buff.get(0) == '\x1b' && buff.get(1) == ' ') {
            keyval = IBus.KEY_space;
            modifiers = IBus.ModifierType.SUPER_MASK;
            return true;
        }

        if (length < 3)
            return false;
        if (buff.get(0) != '\x1b' || buff.get(1) != '[')
            return false;

        uint i = 0;
        string delim = null;
        for (; i < length; i++) {
            delim = buff.substring(i);
            if (delim.get(0) == ';')
                break;
        }
        if (i < length) {
            /* format is '\033[x;yR]' */
            int x = int.parse(buff.substring(2));
            int y = int.parse(delim.substring(1));
            if (buff.get(length - 1) == 'R') {
                cursor_position(x, y);
                return true;
            }
        }
        switch (buff.get(2)) {
        case 'A':
            keyval = IBus.KEY_Up;
            return true;
        case 'B':
            keyval = IBus.KEY_Down;
            return true;
        case 'C':
            keyval = IBus.KEY_Right;
            return true;
        case 'D':
            keyval = IBus.KEY_Left;
            return true;
        case 'P':
            keyval = IBus.KEY_Pause;
            return true;
        case '1':
            if (length == 4 && buff.get(3) == '~') {
                keyval = IBus.KEY_Home;
                return true;
            }
            break;
        case '2':
            if (length == 4 && buff.get(3) == '~') {
                keyval = IBus.KEY_Insert;
                return true;
            }
            break;
        case '3':
            if (length == 4 && buff.get(3) == '~') {
                keyval = IBus.KEY_Delete;
                return true;
            }
            break;
        case '4':
            if (length == 4 && buff.get(3) == '~') {
                keyval = IBus.KEY_End;
                return true;
            }
            break;
        case '5':
            if (length == 4 && buff.get(3) == '~') {
                keyval = IBus.KEY_Page_Up;
                return true;
            }
            break;
        case '6':
            if (length == 4 && buff.get(3) == '~') {
                keyval = IBus.KEY_Page_Down;
                return true;
            }
            break;
        case '[':
            if (length == 4 && buff.get(3) >= 'A') {
                keyval = IBus.KEY_F1 + (buff.get(3) - 'A');
                return true;
            }
            break;
        default: break;
        }

        return false;
    }

    private void char_to_keyval(char       ch,
                                out uint32 keyval,
                                out uint32 modifiers) {
        keyval = 0;
        modifiers = 0;

        switch (ch) {
        case 0:
            keyval = IBus.KEY_space;
            modifiers = IBus.ModifierType.CONTROL_MASK;
            break;
        case 0x1b:
            keyval = IBus.KEY_Escape;
            break;
        case 0x7f:
            keyval = IBus.KEY_BackSpace;
            break;
        case '\r':
            keyval = IBus.KEY_Return;
            break;
        case '\t':
            keyval = IBus.KEY_Tab;
            break;
        default:
            keyval = ch;
            break;
        }
    }

    public uint filter_keypress(string?     buff,
                                uint        length,
                                out string? dispatched = null) {
        dispatched = "";

        if (length == 0)
            return length;

        //char[length] dispatched_chars = { 0, };

        uint j = 0;
        for (uint i = 0; i < length; i++) {
            char code = buff.get(i);
            uint32 keyval = 0;
            uint32 modifiers = 0;
            bool is_control = false;
            bool processed;

            if (i == 0 &&
                control_key_to_keyval(buff,
                                      length,
                                      out keyval,
                                      out modifiers))
                is_control = true;
            else
                char_to_keyval(code, out keyval, out modifiers);

            if (is_control && keyval == 0 && modifiers == 0) {
                dispatched = null;
                return 0;
            }

            processed = m_ibuscontext.process_key_event(
                    keyval,
                    code,
                    modifiers);

            if (is_control) {
                if (!processed) {
                    dispatched = buff.substring(j, length);
                }
                i += length;
            } else {
                if (!processed) {
                    dispatched += buff.substring(i, 1);
                    j++;
                }
            }

            m_ibuscontext.process_key_event(
                keyval,
                code,
                modifiers | IBus.ModifierType.RELEASE_MASK);
        }

        if (j == 0)
            dispatched = null;
        return j;
    }
}
