/* -*- mode: C; c-basic-offset: 4; indent-tabs-mode: nil; -*- */
/* vim:set et sts=4: */
/*
 * Copyright(c) 2016 Red Hat, Inc.
 * Copyright(c) 2016 Takao Fujiwara <tfujiwar@redhat.com>
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

class Loadkeys
{
    public signal void   user_warning      (string           message);

    private const string XKB_COMMAND = "loadkeys";

    public Loadkeys() {
    }

    public void set_layout(IBus.EngineDesc engine) {
        string layout = engine.get_layout();
        string variant = engine.get_layout_variant();

        assert (layout != null);

        /* If the layout is "default", return here so that the current
         * keymap is not changed.
         * Some engines do not wish to change the current keymap.
         */
        if (layout == "default")
            return;

        if (layout == "") {
            user_warning("Could not get the correct layout");
            return;
        }

        /* format is under /lib/kbd/keymaps/xkb/*.map.gz */
        if (variant != "" && variant != "default")
            layout = "%s-%s".printf(layout, variant);

        string[] args = {};
        args += XKB_COMMAND;
        args += layout;

        string standard_error = null;
        int exit_status = 0;
        try {
            if (!GLib.Process.spawn_sync(null,
                                         args,
                                         null,
                                         GLib.SpawnFlags.SEARCH_PATH,
                                         null,
                                         null,
                                         out standard_error,
                                         out exit_status))
                user_warning("Switch xkb layout to %s failed.".printf(
                        engine.get_layout()));
        } catch (GLib.SpawnError e) {
            user_warning("Execute setxkbmap failed: %s".printf(e.message));
            return;
        }

        if (exit_status != 0)
            user_warning("Execute loadkeys failed: %s".printf(
                    standard_error ?? "(null)"));
    }
}
