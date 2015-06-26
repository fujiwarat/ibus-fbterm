enum { AuxiliaryTextWin = 0, LookupTableWin, StatusBarWin };

static unsigned short cursor_x, cursor_y;
static Rectangle auxiliary_text_win, lookup_table_win, status_bar_win;

static const Info info;

#define FW(x) ((x) * info.fontWidth)
#define FH(y) ((y) * info.fontHeight)
#define SW (info.screenWidth)
#define SH (info.screenHeight)

#define MARGIN 5
#define GAP 5
#define WIN_HEIGHT (FH(1) + 2 * MARGIN)
#define WIN_INTERVAL (WIN_HEIGHT + GAP)

#define COLOR_BG Gray
#define COLOR_FG Black
#define COLOR_ACTIVE_CANDIDATE DarkBlue

struct interval {
	unsigned first;
	unsigned last;
};

static char bisearch(unsigned ucs, const struct interval *table, unsigned max)
{
	unsigned min = 0;
	unsigned mid;

	if (ucs < table[0].first || ucs > table[max].last)
		return 0;
	while (max >= min) {
		mid = (min + max) / 2;
		if (ucs > table[mid].last)
			min = mid + 1;
		else if (ucs < table[mid].first)
			max = mid - 1;
		else
			return 1;
	}
	return 0;
}

static char is_double_width(unsigned ucs)
{
	static const struct interval double_width[] = {
		{ 0x1100, 0x115F}, { 0x2329, 0x232A}, { 0x2E80, 0x303E},
		{ 0x3040, 0xA4CF}, { 0xAC00, 0xD7A3}, { 0xF900, 0xFAFF},
		{ 0xFE10, 0xFE19}, { 0xFE30, 0xFE6F}, { 0xFF00, 0xFF60},
		{ 0xFFE0, 0xFFE6}, { 0x20000, 0x2FFFD}, { 0x30000, 0x3FFFD}
	};
	return bisearch(ucs, double_width, sizeof(double_width) / sizeof(struct interval) - 1);
}

static inline unsigned short get_cursor_y()
{
	if (SH <= 3 * WIN_INTERVAL) return 0;

	unsigned short max_cursor_y = SH - 3 * WIN_INTERVAL;

	if (cursor_y < max_cursor_y) return cursor_y;
	if (cursor_y >= 4 * WIN_INTERVAL) return cursor_y - 4 * WIN_INTERVAL;
	return max_cursor_y;
}

static void utf8_to_utf16(unsigned char *utf8, unsigned short *utf16)
{
	unsigned index = 0;
	for (; *utf8;) {
		if ((*utf8 & 0x80) == 0) {
			utf16[index++] = *utf8;
			utf8++;
		} else if ((*utf8 & 0xe0) == 0xc0) {
			utf16[index++] = ((*utf8 & 0x1f) << 6) | (utf8[1] & 0x3f);
			utf8 += 2;
		} else if ((*utf8 & 0xf0) == 0xe0) {
			utf16[index++] = ((*utf8 & 0xf) << 12) | ((utf8[1] & 0x3f) << 6) | (utf8[2] & 0x3f);
			utf8 += 3;
		} else utf8++;
	}

	utf16[index] = 0;
}

static unsigned text_width(char *utf8)
{
	unsigned short utf16[strlen(utf8) + 1];
	utf8_to_utf16(utf8, utf16);

	unsigned i, w = 0;
	for (i = 0; utf16[i]; i++, w++) {
		if (is_double_width(utf16[i])) w++;
	}

	return w;
}

static void draw_margin(Rectangle rect, char color)
{
	Rectangle r1 = { rect.x, rect.y, rect.w, MARGIN };
	fill_rect(r1, color);

	r1.y = rect.y + rect.h - MARGIN;
	fill_rect(r1, color);

	Rectangle r2 = { rect.x, rect.y + MARGIN, MARGIN, rect.h - 2 * MARGIN };
	fill_rect(r2, color);

	r2.x = rect.x + rect.w - MARGIN;
	fill_rect(r2, color);
}

static void calculate_lookup_win()
{
	if (!lookup_table) {
		lookup_table_win.w = 0;
		return;
	}

	unsigned i, w = 0;
	for (i = 0; ; i++) {
		IBusText *text = ibus_lookup_table_get_candidate(lookup_table, i);
		if (!text) break;

		w += text_width(text->text);
	}

	lookup_table_win.x = cursor_x;
	lookup_table_win.y = get_cursor_y() + WIN_INTERVAL + GAP;
	lookup_table_win.w = FW(w + 3 * lookup_table->page_size) + 2 * MARGIN;
	lookup_table_win.h = WIN_HEIGHT;

	if (lookup_table_win.x + lookup_table_win.w > SW) {
		if (lookup_table_win.w > SW) lookup_table_win.x = 0;
		else lookup_table_win.x = SW - lookup_table_win.w;
	}
}

static void draw_lookup_table()
{
	set_im_window(LookupTableWin, lookup_table_win);
	if (!lookup_table_win.w) return;

	draw_margin(lookup_table_win, COLOR_BG);

	unsigned i, x = lookup_table_win.x + MARGIN, y = lookup_table_win.y + MARGIN;
	for (i = 0; ; i++) {
		IBusText *text = ibus_lookup_table_get_candidate(lookup_table, i);
		if (!text) break;

		char buf[8];
		snprintf(buf, sizeof(buf), "%d.", i + 1);
		draw_text(x, y, COLOR_FG, COLOR_BG, buf, strlen(buf));
		x += FW(2);

		draw_text(x, y, i == lookup_table->cursor_pos ? COLOR_ACTIVE_CANDIDATE : COLOR_FG, COLOR_BG, text->text, strlen(text->text));
		x += FW(text_width(text->text));

		char space = ' ';
		draw_text(x, y, COLOR_FG, COLOR_BG, &space, 1);
		x += FW(1);
	}

	unsigned endx = lookup_table_win.x + lookup_table_win.w - MARGIN;
	if (x < endx) {
		Rectangle rect = { x, y, endx - x, FH(1) };
		fill_rect(rect, COLOR_BG);
	}
}


static void calculate_auxiliary_win()
{
	if (!auxiliary_text) {
		auxiliary_text_win.w = 0;
		return;
	}

	auxiliary_text_win.x = cursor_x;
	auxiliary_text_win.y = get_cursor_y() + GAP;
	auxiliary_text_win.w = FW(text_width(auxiliary_text->text)) + 2 * MARGIN;
	auxiliary_text_win.h = WIN_HEIGHT;

	if (auxiliary_text_win.x + auxiliary_text_win.w > SW) {
		if (auxiliary_text_win.w > SW) auxiliary_text_win.x = 0;
		else auxiliary_text_win.x = SW - auxiliary_text_win.w;
	}
}

static void draw_auxiliary_text()
{
	set_im_window(AuxiliaryTextWin, auxiliary_text_win);
	if (!auxiliary_text_win.w) return;

	draw_margin(auxiliary_text_win, COLOR_BG);

	unsigned x = auxiliary_text_win.x + MARGIN, y = auxiliary_text_win.y + MARGIN;
	draw_text(x, y, COLOR_FG, COLOR_BG, auxiliary_text->text, strlen(auxiliary_text->text));
}

static void calculate_status_win()
{
	if (!property_list) {
		status_bar_win.w = 0;
		return;
	}

	unsigned i, w = 0;
	for (i = 0; ; i++) {
		IBusProperty *prop = ibus_prop_list_get(property_list, i);
		if (!prop) break;

		w += text_width(prop->label->text);
	}

	status_bar_win.x = cursor_x;
	status_bar_win.y = get_cursor_y() + 2 * WIN_INTERVAL + GAP;
	status_bar_win.w = FW(w + property_list->properties->len) + 2 * MARGIN;
	status_bar_win.h = WIN_HEIGHT;

	if (status_bar_win.x + status_bar_win.w > SW) {
		if (status_bar_win.w > SW) status_bar_win.x = 0;
		else status_bar_win.x = SW - status_bar_win.w;
	}
}

static void draw_status_bar()
{
	set_im_window(StatusBarWin, status_bar_win);
	if (!status_bar_win.w) return;

	draw_margin(status_bar_win, COLOR_BG);

	unsigned i, x = status_bar_win.x + MARGIN, y = status_bar_win.y + MARGIN;
	for (i = 0; ; i++) {
		IBusProperty *prop = ibus_prop_list_get(property_list, i);
		if (!prop) break;

		draw_text(x, y, COLOR_FG, COLOR_BG, prop->label->text, strlen(prop->label->text));
		x += FW(text_width(prop->label->text));

		char space = ' ';
		draw_text(x, y, COLOR_FG, COLOR_BG, &space, 1);
		x += FW(1);
	}
}
