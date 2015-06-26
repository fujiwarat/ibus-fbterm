#include <linux/keyboard.h>

#define NoSymbol 0

static unsigned linux_to_x[256] = {
	NoSymbol,   NoSymbol,   NoSymbol,   NoSymbol,
	NoSymbol,   NoSymbol,   NoSymbol,   NoSymbol,
	IBUS_BackSpace,   IBUS_Tab,     IBUS_Linefeed,    NoSymbol,
	NoSymbol,   NoSymbol,   NoSymbol,   NoSymbol,
	NoSymbol,   NoSymbol,   NoSymbol,   NoSymbol,
	NoSymbol,   NoSymbol,   NoSymbol,   NoSymbol,
	NoSymbol,   NoSymbol,   NoSymbol,   IBUS_Escape,
	NoSymbol,   NoSymbol,   NoSymbol,   NoSymbol,
	IBUS_space,   IBUS_exclam,  IBUS_quotedbl,    IBUS_numbersign,
	IBUS_dollar,  IBUS_percent, IBUS_ampersand,   IBUS_apostrophe,
	IBUS_parenleft,   IBUS_parenright,  IBUS_asterisk,    IBUS_plus,
	IBUS_comma,   IBUS_minus,   IBUS_period,  IBUS_slash,
	IBUS_0,       IBUS_1,       IBUS_2,       IBUS_3,
	IBUS_4,       IBUS_5,       IBUS_6,       IBUS_7,
	IBUS_8,       IBUS_9,       IBUS_colon,   IBUS_semicolon,
	IBUS_less,    IBUS_equal,   IBUS_greater, IBUS_question,
	IBUS_at,      IBUS_A,       IBUS_B,       IBUS_C,
	IBUS_D,       IBUS_E,       IBUS_F,       IBUS_G,
	IBUS_H,       IBUS_I,       IBUS_J,       IBUS_K,
	IBUS_L,       IBUS_M,       IBUS_N,       IBUS_O,
	IBUS_P,       IBUS_Q,       IBUS_R,       IBUS_S,
	IBUS_T,       IBUS_U,       IBUS_V,       IBUS_W,
	IBUS_X,       IBUS_Y,       IBUS_Z,       IBUS_bracketleft,
	IBUS_backslash,   IBUS_bracketright,IBUS_asciicircum, IBUS_underscore,
	IBUS_grave,   IBUS_a,       IBUS_b,       IBUS_c,
	IBUS_d,       IBUS_e,       IBUS_f,       IBUS_g,
	IBUS_h,       IBUS_i,       IBUS_j,       IBUS_k,
	IBUS_l,       IBUS_m,       IBUS_n,       IBUS_o,
	IBUS_p,       IBUS_q,       IBUS_r,       IBUS_s,
	IBUS_t,       IBUS_u,       IBUS_v,       IBUS_w,
	IBUS_x,       IBUS_y,       IBUS_z,       IBUS_braceleft,
	IBUS_bar,     IBUS_braceright,  IBUS_asciitilde,  IBUS_BackSpace,
	NoSymbol,   NoSymbol,   NoSymbol,   NoSymbol,
	NoSymbol,   NoSymbol,   NoSymbol,   NoSymbol,
	NoSymbol,   NoSymbol,   NoSymbol,   NoSymbol,
	NoSymbol,   NoSymbol,   NoSymbol,   NoSymbol,
	NoSymbol,   NoSymbol,   NoSymbol,   NoSymbol,
	NoSymbol,   NoSymbol,   NoSymbol,   NoSymbol,
	NoSymbol,   NoSymbol,   NoSymbol,   NoSymbol,
	NoSymbol,   NoSymbol,   NoSymbol,   NoSymbol,
	IBUS_nobreakspace,IBUS_exclamdown,  IBUS_cent,    IBUS_sterling,
	IBUS_currency,    IBUS_yen,     IBUS_brokenbar,   IBUS_section,
	IBUS_diaeresis,   IBUS_copyright,   IBUS_ordfeminine, IBUS_guillemotleft,
	IBUS_notsign, IBUS_hyphen,  IBUS_registered,  IBUS_macron,
	IBUS_degree,  IBUS_plusminus,   IBUS_twosuperior, IBUS_threesuperior,
	IBUS_acute,   IBUS_mu,      IBUS_paragraph,   IBUS_periodcentered,
	IBUS_cedilla, IBUS_onesuperior, IBUS_masculine,   IBUS_guillemotright,
	IBUS_onequarter,  IBUS_onehalf, IBUS_threequarters,IBUS_questiondown,
	IBUS_Agrave,  IBUS_Aacute,  IBUS_Acircumflex, IBUS_Atilde,
	IBUS_Adiaeresis,  IBUS_Aring,   IBUS_AE,      IBUS_Ccedilla,
	IBUS_Egrave,  IBUS_Eacute,  IBUS_Ecircumflex, IBUS_Ediaeresis,
	IBUS_Igrave,  IBUS_Iacute,  IBUS_Icircumflex, IBUS_Idiaeresis,
	IBUS_ETH,     IBUS_Ntilde,  IBUS_Ograve,  IBUS_Oacute,
	IBUS_Ocircumflex, IBUS_Otilde,  IBUS_Odiaeresis,  IBUS_multiply,
	IBUS_Ooblique,    IBUS_Ugrave,  IBUS_Uacute,  IBUS_Ucircumflex,
	IBUS_Udiaeresis,  IBUS_Yacute,  IBUS_THORN,   IBUS_ssharp,
	IBUS_agrave,  IBUS_aacute,  IBUS_acircumflex, IBUS_atilde,
	IBUS_adiaeresis,  IBUS_aring,   IBUS_ae,      IBUS_ccedilla,
	IBUS_egrave,  IBUS_eacute,  IBUS_ecircumflex, IBUS_ediaeresis,
	IBUS_igrave,  IBUS_iacute,  IBUS_icircumflex, IBUS_idiaeresis,
	IBUS_eth,     IBUS_ntilde,  IBUS_ograve,  IBUS_oacute,
	IBUS_ocircumflex, IBUS_otilde,  IBUS_odiaeresis,  IBUS_division,
	IBUS_oslash,  IBUS_ugrave,  IBUS_uacute,  IBUS_ucircumflex,
	IBUS_udiaeresis,  IBUS_yacute,  IBUS_thorn,   IBUS_ydiaeresis
};

static unsigned linux_keysym_to_ibus_keyval(unsigned short keysym, unsigned short keycode)
{
	unsigned kval = KVAL(keysym),  keyval = 0;

	switch (KTYP(keysym)) {
	case KT_LATIN:
	case KT_LETTER:
		keyval = linux_to_x[kval];
		break;

	case KT_FN:
		if (kval <= 19)
			keyval = IBUS_F1 + kval;
		else switch (keysym) {
			case K_FIND:
				keyval = IBUS_Home; /* or IBUS_Find */
				break;
			case K_INSERT:
				keyval = IBUS_Insert;
				break;
			case K_REMOVE:
				keyval = IBUS_Delete;
				break;
			case K_SELECT:
				keyval = IBUS_End; /* or IBUS_Select */
				break;
			case K_PGUP:
				keyval = IBUS_Prior;
				break;
			case K_PGDN:
				keyval = IBUS_Next;
				break;
			case K_HELP:
				keyval = IBUS_Help;
				break;
			case K_DO:
				keyval = IBUS_Execute;
				break;
			case K_PAUSE:
				keyval = IBUS_Pause;
				break;
			case K_MACRO:
				keyval = IBUS_Menu;
				break;
			default:
				break;
			}
		break;

	case KT_SPEC:
		switch (keysym) {
		case K_ENTER:
			keyval = IBUS_Return;
			break;
		case K_BREAK:
			keyval = IBUS_Break;
			break;
		case K_CAPS:
			keyval = IBUS_Caps_Lock;
			break;
		case K_NUM:
			keyval = IBUS_Num_Lock;
			break;
		case K_HOLD:
			keyval = IBUS_Scroll_Lock;
			break;
		case K_COMPOSE:
			keyval = IBUS_Multi_key;
			break;
		default:
			break;
		}
		break;

	case KT_PAD:
		switch (keysym) {
		case K_PPLUS:
			keyval = IBUS_KP_Add;
			break;
		case K_PMINUS:
			keyval = IBUS_KP_Subtract;
			break;
		case K_PSTAR:
			keyval = IBUS_KP_Multiply;
			break;
		case K_PSLASH:
			keyval = IBUS_KP_Divide;
			break;
		case K_PENTER:
			keyval = IBUS_KP_Enter;
			break;
		case K_PCOMMA:
			keyval = IBUS_KP_Separator;
			break;
		case K_PDOT:
			keyval = IBUS_KP_Decimal;
			break;
		case K_PPLUSMINUS:
			keyval = IBUS_KP_Subtract;
			break;
		default:
			if (kval <= 9)
				keyval = IBUS_KP_0 + kval;
			break;
		}
		break;

		/*
		 * KT_DEAD keys are for accelerated diacritical creation.
		 */
	case KT_DEAD:
		switch (keysym) {
		case K_DGRAVE:
			keyval = IBUS_dead_grave;
			break;
		case K_DACUTE:
			keyval = IBUS_dead_acute;
			break;
		case K_DCIRCM:
			keyval = IBUS_dead_circumflex;
			break;
		case K_DTILDE:
			keyval = IBUS_dead_tilde;
			break;
		case K_DDIERE:
			keyval = IBUS_dead_diaeresis;
			break;
		}
		break;

	case KT_CUR:
		switch (keysym) {
		case K_DOWN:
			keyval = IBUS_Down;
			break;
		case K_LEFT:
			keyval = IBUS_Left;
			break;
		case K_RIGHT:
			keyval = IBUS_Right;
			break;
		case K_UP:
			keyval = IBUS_Up;
			break;
		}
		break;

	case KT_SHIFT:
		switch (keysym) {
		case K_ALTGR:
			keyval = IBUS_Alt_R;
			break;
		case K_ALT:
			keyval = (keycode == 0x64 ?
					  IBUS_Alt_R : IBUS_Alt_L);
			break;
		case K_CTRL:
			keyval = (keycode == 0x61 ?
					  IBUS_Control_R : IBUS_Control_L);
			break;
		case K_CTRLL:
			keyval = IBUS_Control_L;
			break;
		case K_CTRLR:
			keyval = IBUS_Control_R;
			break;
		case K_SHIFT:
			keyval = (keycode == 0x36 ?
					  IBUS_Shift_R : IBUS_Shift_L);
			break;
		case K_SHIFTL:
			keyval = IBUS_Shift_L;
			break;
		case K_SHIFTR:
			keyval = IBUS_Shift_R;
			break;
		default:
			break;
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
			keyval = IBUS_Shift_Lock;
		break;

	default:
		break;
	}

	return keyval;
}

static guint32 modifier_state;

void calculate_modifiers(guint32 keyval, char down)
{
	guint32 mask = 0;
	switch (keyval) {
	case IBUS_Shift_L:
	case IBUS_Shift_R:
		mask = IBUS_SHIFT_MASK;
		break;

	case IBUS_Control_L:
	case IBUS_Control_R:
		mask = IBUS_CONTROL_MASK;
		break;

	case IBUS_Alt_L:
	case IBUS_Alt_R:
	case IBUS_Meta_L:
		mask = IBUS_MOD1_MASK;
		break;

	case IBUS_Super_L:
	case IBUS_Hyper_L:
		mask = IBUS_MOD4_MASK;
		break;

	case IBUS_ISO_Level3_Shift:
	case IBUS_Mode_switch:
		mask = IBUS_MOD5_MASK;
		break;

	default:
		break;
	}

	if (mask) {
		if (down) modifier_state |= mask;
		else modifier_state &= ~mask;
	}
}
