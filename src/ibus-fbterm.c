#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <glib.h>
#include <ibus.h>
#include "imapi.h"
#include "keycode.h"

static IBusText *auxiliary_text;
static IBusLookupTable *lookup_table;
static IBusPropList *property_list;

static GMainLoop *main_loop;
static IBusBus *ibus_bus;
static IBusInputContext *ibus_ctx;

//#define debug(args...) fprintf(stderr, args)
#define debug(args...)

#include "keymap.c"
#include "display.c"

static void slot_commit_text(IBusInputContext *ctx, IBusText *text, gpointer data)
{
	debug("commit text: %s\n", text->text);
	put_im_text(text->text, strlen(text->text));
}

static void slot_hide_lookup_table(IBusInputContext *ctx, gpointer data)
{
	debug("hide lookup table\n");
	if (lookup_table) {
		g_object_unref(lookup_table);
		lookup_table = 0;
	}

	calculate_lookup_win();
	draw_lookup_table();
}

static void slot_update_lookup_table(IBusInputContext *ctx, IBusLookupTable *table, gboolean visible, gpointer data)
{
	debug("update lookup table\n");
	if (!visible) return;

	if (lookup_table) g_object_unref(lookup_table);

	lookup_table = table;
	g_object_ref(table);

	calculate_lookup_win();
	draw_lookup_table();
}

static void slot_hide_auxiliary_text(IBusInputContext *ctx, gpointer data)
{
	debug("hide auxiliary text\n");
	if (auxiliary_text) {
		g_object_unref(auxiliary_text);
		auxiliary_text = 0;
	}

	calculate_auxiliary_win();
	draw_auxiliary_text();
}

static void slot_update_auxiliary_text(IBusInputContext *ctx, IBusText *text, gboolean visible, gpointer data)
{
	debug("update auxiliary text: %s\n", text->text);
	if (!visible) return;

	if (auxiliary_text) g_object_unref(auxiliary_text);

	auxiliary_text = text;
	g_object_ref(text);

	calculate_auxiliary_win();
	draw_auxiliary_text();
}

/*
static void slot_show_preedit_text(IBusInputContext *ctx, gpointer data)
{
}

static void slot_hide_preedit_text(IBusInputContext *ctx, gpointer data)
{
}

static void slot_update_preedit_text(IBusInputContext *ctx, IBusText *text, guint cursor_pos, gboolean visible, gpointer data)
{
}
*/

static void slot_register_properties(IBusInputContext *ctx, IBusPropList *props, gpointer data)
{
	debug("register properties\n");
	if (property_list) {
		g_object_unref(property_list);
		property_list = 0;
	}
}

static void slot_update_property(IBusInputContext *ctx, IBusProperty *prop, gpointer data)
{
	debug("update property, key: %s, label: %s\n", prop->key, prop->label->text);
	if (!property_list) property_list = ibus_prop_list_new();

	if (!ibus_prop_list_update_property(property_list, prop)) {
		ibus_prop_list_append(property_list, prop);
	}

	calculate_status_win();
	draw_status_bar();
}

static void im_active()
{
	debug("im active\n");
	modifier_state = 0;
	init_keycode_state();
	ibus_input_context_enable(ibus_ctx);
}

static void im_deactive()
{
	debug("im deactive\n");
	ibus_input_context_disable(ibus_ctx);

	auxiliary_text_win.w = 0;
	lookup_table_win.w = 0;
	status_bar_win.w = 0;

	draw_auxiliary_text();
	draw_lookup_table();
	draw_status_bar();
}

static void process_key(char *buf, unsigned len)
{
	unsigned i;
	for (i = 0; i < len; i++) {
		char down = !(buf[i] & 0x80);
		short code = buf[i] & 0x7f;

		if (!code) {
			if (i + 2 >= len) break;

			code = (buf[++i] & 0x7f) << 7;
			code |= buf[++i] & 0x7f;
			if (!(buf[i] & 0x80) || !(buf[i - 1] & 0x80)) continue;
		}

		unsigned short keysym = keycode_to_keysym(code, down);

		guint32 keyval = linux_keysym_to_ibus_keyval(keysym, code);
		if (!keyval) continue;

	debug("send key, 0x%x, down:%d\n", keyval, down);
		if (!ibus_input_context_process_key_event(ibus_ctx, keyval, code, modifier_state | (down ? 0 : IBUS_RELEASE_MASK))) {
			char *str = keysym_to_term_string(keysym, down);
			if (str) put_im_text(str, strlen(str));
		}

		calculate_modifiers(keyval, down);
	}
}

static void im_show(unsigned winid)
{
	if (winid == (unsigned)-1 || winid == AuxiliaryTextWin) draw_auxiliary_text();
	if (winid == (unsigned)-1 || winid == LookupTableWin) draw_lookup_table();
	if (winid == (unsigned)-1 || winid == StatusBarWin) draw_status_bar();
}

static void im_hide()
{
}

static void cursor_pos_changed(unsigned x, unsigned y)
{
	cursor_x = x;
	cursor_y = y;

	calculate_lookup_win();
	calculate_auxiliary_win();
	calculate_status_win();

	draw_lookup_table();
	draw_auxiliary_text();
	draw_status_bar();
}

static void update_fbterm_info(Info *_info)
{
	memcpy((void *)&info, _info, sizeof(info));
}

static ImCallbacks cbs = {
	im_active, // .active
	im_deactive, // .deactive
	im_show,	 // .show_ui
	im_hide, // .hide_ui
	process_key, // .send_key
	cursor_pos_changed, // .cursor_position
	update_fbterm_info, // .fbterm_info
	update_term_mode // .term_mode
};

static gboolean iochannel_fbterm_callback(GIOChannel *source, GIOCondition condition, gpointer data)
{
	if (!check_im_message()) {
		g_main_loop_quit(main_loop);
		return FALSE;
	}

	return TRUE;
}

int main()
{
	if (get_im_socket() == -1) return 1;

	GIOChannel *iochannel_fbterm = g_io_channel_unix_new(get_im_socket());
	g_io_add_watch(iochannel_fbterm, (GIOCondition)(G_IO_IN | G_IO_HUP | G_IO_ERR), (GIOFunc)iochannel_fbterm_callback, NULL);

	g_type_init();

	ibus_bus = ibus_bus_new();
	ibus_ctx = ibus_bus_create_input_context(ibus_bus, "");
	g_object_connect(ibus_ctx,
					 "signal::commit-text", slot_commit_text, NULL,
					 "signal::hide-lookup-table", slot_hide_lookup_table, NULL,
					 "signal::update-lookup-table", slot_update_lookup_table, NULL,
					 "signal::hide-auxiliary-text", slot_hide_auxiliary_text, NULL,
					 "signal::update-auxiliary-text", slot_update_auxiliary_text, NULL,
//					 "signal::show-preedit-text", slot_show_preedit_text, NULL,
//					 "signal::hide-preedit-text", slot_hide_preedit_text, NULL,
//					 "signal::update-preedit-text", slot_update_preedit_text, NULL,
					 "signal::register-properties", slot_register_properties, NULL,
					 "signal::update-property", slot_update_property, NULL,
					 NULL);
	ibus_input_context_set_capabilities(ibus_ctx, IBUS_CAP_AUXILIARY_TEXT | IBUS_CAP_LOOKUP_TABLE | IBUS_CAP_PROPERTY);

	register_im_callbacks(cbs);
	connect_fbterm(1);

	main_loop = g_main_loop_new(NULL, FALSE);
	g_main_loop_run(main_loop);

	if (auxiliary_text) g_object_unref(auxiliary_text);
	if (lookup_table) g_object_unref(lookup_table);
	if (property_list) g_object_unref(property_list);

	g_object_unref(ibus_ctx);
	g_object_unref(ibus_bus);
	g_io_channel_unref(iochannel_fbterm);
	g_main_loop_unref(main_loop);

	return 0;
}
