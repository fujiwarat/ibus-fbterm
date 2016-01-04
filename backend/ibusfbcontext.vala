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
    public abstract void load_settings     ();

    public signal void user_warning        (string           message);
    public signal void cursor_position     (int              x,
                                            int              y);
    public signal int  switcher_switch     (IBus.EngineDesc[]
                                                             engines,
                                            uint32           keyval);
    public signal void commit              (IBus.Text        text);
    public signal void preedit_changed     (IBus.Text        text,
                                            uint             cursor_pos,
                                            bool             visible);
    public signal void update_lookup_table (IBus.LookupTable table,
                                            bool             visible);
}

class IBusFbContext : GLib.InitiallyUnowned, FbContext {
    private enum BindingState {
        NO_BINDING = 0,
        SINGLE_BINDING,
        DOUBLE_BINDING,
    }

    private GLib.Settings m_settings_general;
    private GLib.Settings m_settings_hotkey;
    private IBus.Bus m_bus;
    private IBus.InputContext m_ibuscontext;
    private GLib.List<Keybinding> m_bindings;
    private IBus.EngineDesc[] m_engines = {};
    private bool m_is_escaped;
    private BindingState m_is_binding;

    private class Keybinding {
        public Keybinding(uint32 keyval,
                          uint32 modifiers) {
            this.keyval = keyval;
            this.modifiers = modifiers;
        }
        public uint32 keyval { get; set; }
        public uint32 modifiers { get; set; }
    }

    public IBusFbContext() {
        m_settings_general =
                new GLib.Settings("org.freedesktop.ibus.general");
        m_settings_hotkey =
                new GLib.Settings("org.freedesktop.ibus.general.hotkey");

        m_settings_hotkey.changed["triggers"].connect((key) => {
                bind_switch_shortcut();
        });

        m_bus = new IBus.Bus ();
        if (m_bus.is_connected())
            create_input_context();

        m_bus.connected.connect((bus) => {
            create_input_context();
        });
    }

    private void state_changed(IBus.EngineDesc engine) {
        int i;
        for (i = 0; i < m_engines.length; i++) {
            if (m_engines[i].get_name() == engine.get_name())
                break;
        }

        // engine is first engine in m_engines.
        if (i == 0)
            return;

        // engine is not in m_engines.
        if (i >= m_engines.length)
            return;

        for (int j = i; j > 0; j--) {
            m_engines[j] = m_engines[j - 1];
        }
        m_engines[0] = engine;

        string[] names = {};
        foreach(var desc in m_engines) {
            names += desc.get_name();
        }
        m_settings_general.set_strv("engines-order", names);
    }

    private void set_engine(IBus.EngineDesc engine) {
        if (!m_bus.set_global_engine(engine.get_name())) {
            user_warning(
                    "Switch engine to %s failed.".printf(engine.get_name()));
            return;
        }

        state_changed(engine);
    }

    private void switch_engine(int  i,
                               bool force = false) {
        GLib.assert(i >= 0 && i < m_engines.length);

        if (i == 0 && !force)
            return;

        IBus.EngineDesc engine = m_engines[i];

        set_engine(engine);
    }

    private void update_engines(string[]? unowned_engine_names,
                                string[]? order_names) {
        string[]? engine_names = unowned_engine_names;

        if (engine_names == null || engine_names.length == 0)
            engine_names = {"xkb:us::eng"};

        string[] names = {};

        foreach (var name in order_names) {
            if (name in engine_names)
                names += name;
        }

        foreach (var name in engine_names) {
            if (name in names)
                continue;
            names += name;
        }

        var engines = m_bus.get_engines_by_names(names);

        /* Fedora internal patch could save engines not in simple.xml
         * likes 'xkb:cn::chi'.
         */
        if (engines.length == 0) {
            names =  {"xkb:us::eng"};
            m_settings_general.set_strv("preload-engines", names);
            engines = m_bus.get_engines_by_names(names);
        }

        if (m_engines.length == 0) {
            m_engines = engines;
            switch_engine(0, true);
#if 0
            run_preload_engines(engines, 1);
#endif
        } else {
            var current_engine = m_engines[0];
            m_engines = engines;
            int i;
            for (i = 0; i < m_engines.length; i++) {
                if (current_engine.get_name() == engines[i].get_name()) {
                    switch_engine(i);
#if 0
                    if (i != 0) {
                        run_preload_engines(engines, 0);
                    } else {
                        run_preload_engines(engines, 1);
                    }
#endif
                    return;
                }
            }
            switch_engine(0, true);
#if 0
            run_preload_engines(engines, 1);
#endif
        }
    }

    private void bind_switch_shortcut() {
        string[] accelerators = m_settings_hotkey.get_strv("triggers");
        m_bindings = new GLib.List<Keybinding>();
        foreach (var accelerator in accelerators) {
            if (accelerator == "<Super>space") {
                Keybinding keybinding =
                        new Keybinding(IBus.KEY_space,
                                       IBus.ModifierType.SUPER_MASK);
                m_bindings.append(keybinding);
            }
            if (accelerator == "<Control>space") {
                Keybinding keybinding =
                        new Keybinding(IBus.KEY_space,
                                       IBus.ModifierType.CONTROL_MASK);
                m_bindings.append(keybinding);
            }
            if (accelerator == "<Ctrl>space") {
                Keybinding keybinding =
                        new Keybinding(IBus.KEY_space,
                                       IBus.ModifierType.CONTROL_MASK);
                m_bindings.append(keybinding);
            }
        }
        if (m_bindings.length() == 0) {
            Keybinding keybinding =
                    new Keybinding(IBus.KEY_space,
                                   IBus.ModifierType.SUPER_MASK);
            m_bindings.append(keybinding);
        }
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

        /* Escape key('\x1b') is used for terminal special keybindings
         * in ibus-fbterm since compound shortcut keys does not work
         * on terminal.
         * E.g.
         * Super+Shift+space is Escape, Super+space in ibus-fbterm.
         * Control+Shift+u is Escape, Super+u in ibus-fbterm.
         */
        if (length == 1) {
            char ch = buff.get(0);

            /* Between Ctrl + a and Ctrl + z */
            if ((ch >= '\x1' && ch <= '\x6') || (ch >= '\xe' && ch <= '\x1a')) {
                keyval = (uint32)ch - (uint32)'\x1' + IBus.KEY_a;
                modifiers = IBus.ModifierType.CONTROL_MASK;
                if (m_is_escaped) {
                    modifiers |= IBus.ModifierType.SHIFT_MASK;
                    m_is_escaped = false;
                }
                return true;
            }

            switch(ch) {
            case '\x1b':
                m_is_escaped = !m_is_escaped;
                keyval = IBus.KEY_Escape;
                return true;
            case 0x7f:
                keyval = IBus.KEY_BackSpace;
                if (m_is_escaped) {
                    modifiers |= IBus.ModifierType.SHIFT_MASK;
                    m_is_escaped = false;
                }
                return true;
            case '\r':
                keyval = IBus.KEY_Return;
                if (m_is_escaped) {
                    modifiers |= IBus.ModifierType.SHIFT_MASK;
                    m_is_escaped = false;
                }
                return true;
            case '\t':
                keyval = IBus.KEY_Tab;
                if (m_is_escaped) {
                    modifiers |= IBus.ModifierType.SHIFT_MASK;
                    m_is_escaped = false;
                }
                return true;
            case '\0':
                keyval = IBus.KEY_space;
                modifiers = IBus.ModifierType.CONTROL_MASK;
                if (m_is_escaped) {
                    modifiers |= IBus.ModifierType.SHIFT_MASK;
                    m_is_escaped = false;
                }
                return true;
            case ' ':
                if (m_is_escaped) {
                    keyval = IBus.KEY_space;
                    modifiers |= IBus.ModifierType.SHIFT_MASK;
                    m_is_escaped = false;
                    return true;
                }
                /* space without modifiers is not a control key. */
                break;
            default: break;
            }
        }
        if (length == 2 && buff.get(0) == '\x1b' && buff.get(1) == ' ') {
            keyval = IBus.KEY_space;
            modifiers = IBus.ModifierType.SUPER_MASK;
            if (m_is_escaped) {
                modifiers |= IBus.ModifierType.SHIFT_MASK;
                m_is_escaped = false;
            }
            return true;
        }

        if (m_is_escaped)
            m_is_escaped = false;

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

    private bool handle_engine_switch(uint32 keyval,
                                      uint32 modifiers) {
        bool reverse = false;
        uint32 binding_modifiers = modifiers;

        /* Shift+space cannot be received on terminal so all shift-mask
         * keys are reverse shortcut keys.
         */
        if ((binding_modifiers & IBus.ModifierType.SHIFT_MASK) != 0) {
            reverse = true;
            binding_modifiers &= ~IBus.ModifierType.SHIFT_MASK;
        }

        foreach (var binding in m_bindings) {
            if (binding.keyval == keyval &&
                binding.modifiers == binding_modifiers) {
                if (m_is_binding == BindingState.SINGLE_BINDING)
                    m_is_binding = BindingState.DOUBLE_BINDING;
                if (m_is_binding == BindingState.DOUBLE_BINDING) {
                    if (!reverse)
                        switcher_switch(m_engines, IBus.KEY_Right);
                    else
                        switcher_switch(m_engines, IBus.KEY_Left);
                }
                else if (m_engines.length > 1) {
                    if (!reverse)
                        switch_engine(1);
                    else
                        switch_engine(m_engines.length - 1);
                    m_is_binding = BindingState.SINGLE_BINDING;
                } else {
                    user_warning(
                            "Only one engine(%s) is configured so use ibus-setup or gsettings".
                                    printf(m_engines[0].get_name()));
                }
                return true;
            }
        }

        if (m_is_binding == BindingState.DOUBLE_BINDING) {
            if (binding_modifiers == 0) {
                if (keyval == IBus.KEY_Return || keyval == IBus.KEY_Escape) {
                    int index = switcher_switch(m_engines, keyval);

                    if (index >= 0)
                        switch_engine(index);
                    m_is_binding = BindingState.NO_BINDING;
                    return true;
                }
                if (keyval == IBus.KEY_Left || keyval == IBus.KEY_Right) {
                    switcher_switch(m_engines, keyval);
                    return true;
                }
            }
            if ((modifiers & IBus.ModifierType.CONTROL_MASK) != 0) {
                if (keyval == IBus.KEY_b) {
                    switcher_switch(m_engines, IBus.KEY_Left);
                    return true;
                }
                if (keyval == IBus.KEY_f) {
                    switcher_switch(m_engines, IBus.KEY_Right);
                    return true;
                }
            }
            switcher_switch(m_engines, IBus.KEY_Escape);
            m_is_binding = BindingState.NO_BINDING;
        } else {
            m_is_binding = BindingState.NO_BINDING;
        }

        return false;
    }

    public uint filter_keypress(string?     buff,
                                uint        length,
                                out string? dispatched = null) {
        dispatched = "";

        if (length == 0)
            return length;

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
                                      out modifiers)) {
                is_control = true;
            } else {
                keyval = code;
                if (m_is_binding != BindingState.DOUBLE_BINDING)
                    m_is_binding = BindingState.NO_BINDING;
            }

            /* Terminal signal "\x1b[" should not be sent to IME */
            if (is_control && keyval == 0 && modifiers == 0) {
                dispatched = null;
                return 0;
            }

            if (is_control && handle_engine_switch(keyval, modifiers)) {
                dispatched = null;
                return 0;
            }

            /* Stop switcher if keymap is not binding keys when switcher
             * is running. */
            if (m_is_binding == BindingState.DOUBLE_BINDING) {
                switcher_switch(m_engines, IBus.KEY_Escape);
                m_is_binding = BindingState.NO_BINDING;
            }

            processed = m_ibuscontext.process_key_event(
                    keyval,
                    code,
                    modifiers);

            if (is_control) {
                if (!processed) {
                    if (buff.get(0) == '\0')
                        dispatched = "";
                    else
                        dispatched = buff.substring(j, length);
                    j = length;
                }
                i = length;
            } else {
                if (!processed) {
                    if (buff.get(0) == '\0')
                        dispatched += "";
                    else
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

    public void load_settings () {
        update_engines(m_settings_general.get_strv("preload-engines"),
                       m_settings_general.get_strv("engines-order"));
        bind_switch_shortcut();
    }
}
