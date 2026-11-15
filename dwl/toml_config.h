#ifndef TOML_CONFIG_H
#define TOML_CONFIG_H

#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>

typedef struct {
	void **allocs;
	size_t nallocs;
	size_t capallocs;
} TomlArena;

typedef struct {
	int has_mod;
	uint32_t mod;
	int has_key;
	char *key_name;
	int has_shift_key;
	char *shift_key_name;
	int has_action;
	char *action;
	int has_arg;
	char *arg_str;
	char **arg_arr;
	size_t arg_arr_len;
	int has_tag;
	int tag;
	int has_tag_mask;
	uint32_t tag_mask;
	int has_button;
	unsigned int button;
	int has_dir;
	unsigned int dir;
} TomlBindDraft;

typedef struct {
	char *app_id;
	char *title;
	uint32_t tags;
	int isfloating;
	int monitor;
	int width;
	int height;
} TomlRuleDraft;

typedef struct {
	char *name;
	float mfact;
	int nmaster;
	int resx;
	int resy;
	float refresh;
	float scale;
	const Layout *lt;
	enum wl_output_transform rr;
	int x;
	int y;
} TomlMonDraft;

static TomlArena tc_active_arena = {0};
static uint32_t tc_modkey = WLR_MODIFIER_LOGO;
static char *tc_xkb_rules = NULL;
static char *tc_xkb_model = NULL;
static char *tc_xkb_layout = NULL;
static char *tc_xkb_variant = NULL;
static char *tc_xkb_options = NULL;

static void
tc_arena_free(TomlArena *arena)
{
	size_t i;

	for (i = 0; i < arena->nallocs; i++)
		free(arena->allocs[i]);
	free(arena->allocs);
	arena->allocs = NULL;
	arena->nallocs = 0;
	arena->capallocs = 0;
}

static void *
tc_arena_track(TomlArena *arena, void *ptr)
{
	void **next;
	size_t next_cap;

	if (!ptr)
		return NULL;
	if (arena->nallocs == arena->capallocs) {
		next_cap = arena->capallocs ? arena->capallocs * 2 : 64;
		next = realloc(arena->allocs, next_cap * sizeof(*next));
		if (!next)
			die("dwl: realloc failed:");
		arena->allocs = next;
		arena->capallocs = next_cap;
	}
	arena->allocs[arena->nallocs++] = ptr;
	return ptr;
}

static char *
tc_arena_strdup(TomlArena *arena, const char *s)
{
	size_t len;
	char *copy;

	if (!s)
		return NULL;
	len = strlen(s) + 1;
	copy = malloc(len);
	if (!copy)
		die("dwl: malloc failed:");
	memcpy(copy, s, len);
	return tc_arena_track(arena, copy);
}

static char *
tc_trim(char *s)
{
	char *end;

	while (*s && isspace((unsigned char)*s))
		s++;
	end = s + strlen(s);
	while (end > s && isspace((unsigned char)end[-1]))
		*--end = '\0';
	return s;
}

static void
tc_strip_comment(char *s)
{
	int in_dq = 0;
	int in_sq = 0;
	int esc = 0;
	char *p;

	for (p = s; *p; p++) {
		if (esc) {
			esc = 0;
			continue;
		}
		if (in_dq) {
			if (*p == '\\')
				esc = 1;
			else if (*p == '"')
				in_dq = 0;
		} else if (in_sq) {
			if (*p == '\'')
				in_sq = 0;
		} else {
			if (*p == '"')
				in_dq = 1;
			else if (*p == '\'')
				in_sq = 1;
			else if (*p == '#') {
				*p = '\0';
				return;
			}
		}
	}
}

static int
tc_parse_string(const char *s, char **out)
{
	size_t len;
	char *buf;
	size_t i, o;
	char q;

	len = strlen(s);
	if (len < 2)
		return 0;
	q = s[0];
	if ((q != '"' && q != '\'') || s[len - 1] != q)
		return 0;

	buf = malloc(len);
	if (!buf)
		die("dwl: malloc failed:");

	o = 0;
	for (i = 1; i + 1 < len; i++) {
		if (q == '"' && s[i] == '\\' && i + 2 < len) {
			i++;
			switch (s[i]) {
			case 'n': buf[o++] = '\n'; break;
			case 't': buf[o++] = '\t'; break;
			case 'r': buf[o++] = '\r'; break;
			case '\\': buf[o++] = '\\'; break;
			case '"': buf[o++] = '"'; break;
			default: buf[o++] = s[i]; break;
			}
		} else {
			buf[o++] = s[i];
		}
	}
	buf[o] = '\0';
	*out = buf;
	return 1;
}

static int
tc_parse_bool(const char *s, int *out)
{
	if (strcasecmp(s, "true") == 0) {
		*out = 1;
		return 1;
	}
	if (strcasecmp(s, "false") == 0) {
		*out = 0;
		return 1;
	}
	return 0;
}

static int
tc_parse_int(const char *s, long *out)
{
	char *end = NULL;
	long v;

	if (!*s)
		return 0;
	errno = 0;
	v = strtol(s, &end, 0);
	if (errno != 0 || !end || *tc_trim(end) != '\0')
		return 0;
	*out = v;
	return 1;
}

static int
tc_parse_double(const char *s, double *out)
{
	char *end = NULL;
	double v;

	if (!*s)
		return 0;
	errno = 0;
	v = strtod(s, &end);
	if (errno != 0 || !end || *tc_trim(end) != '\0')
		return 0;
	*out = v;
	return 1;
}

static int
tc_parse_color(const char *s, float out[4])
{
	char *str = NULL;
	const char *hex;
	size_t len;
	unsigned long val;
	char *end = NULL;

	if (tc_parse_string(s, &str))
		hex = str;
	else
		hex = s;

	if (hex[0] == '#')
		hex++;
	else if (hex[0] == '0' && (hex[1] == 'x' || hex[1] == 'X'))
		hex += 2;

	len = strlen(hex);
	if (len != 6 && len != 8) {
		free(str);
		return 0;
	}

	errno = 0;
	val = strtoul(hex, &end, 16);
	if (errno != 0 || !end || *end != '\0') {
		free(str);
		return 0;
	}

	if (len == 6)
		val = (val << 8) | 0xffu;

	out[0] = (float)((val >> 24) & 0xffu) / 255.0f;
	out[1] = (float)((val >> 16) & 0xffu) / 255.0f;
	out[2] = (float)((val >> 8) & 0xffu) / 255.0f;
	out[3] = (float)(val & 0xffu) / 255.0f;
	free(str);
	return 1;
}

static int
tc_parse_string_array(const char *s, char ***out_items, size_t *out_len)
{
	size_t len;
	const char *p;
	char **items = NULL;
	size_t count = 0;
	size_t cap = 0;

	len = strlen(s);
	if (len < 2 || s[0] != '[' || s[len - 1] != ']')
		return 0;

	p = s + 1;
	while (p < s + len - 1) {
		const char *start;
		size_t tok_len;
		char *tok;
		char *val = NULL;
		char **next;
		size_t next_cap;
		int in_dq = 0, in_sq = 0, esc = 0;

		while (p < s + len - 1 && (isspace((unsigned char)*p) || *p == ','))
			p++;
		if (p >= s + len - 1)
			break;

		start = p;
		while (p < s + len - 1) {
			if (esc) {
				esc = 0;
			} else if (in_dq) {
				if (*p == '\\')
					esc = 1;
				else if (*p == '"')
					in_dq = 0;
			} else if (in_sq) {
				if (*p == '\'')
					in_sq = 0;
			} else {
				if (*p == '"')
					in_dq = 1;
				else if (*p == '\'')
					in_sq = 1;
				else if (*p == ',')
					break;
			}
			p++;
		}

		tok_len = (size_t)(p - start);
		tok = malloc(tok_len + 1);
		if (!tok)
			die("dwl: malloc failed:");
		memcpy(tok, start, tok_len);
		tok[tok_len] = '\0';

		if (!tc_parse_string(tc_trim(tok), &val)) {
			val = strdup(tc_trim(tok));
			if (!val)
				die("dwl: strdup failed:");
		}
		free(tok);

		if (count == cap) {
			next_cap = cap ? cap * 2 : 8;
			next = realloc(items, next_cap * sizeof(*next));
			if (!next)
				die("dwl: realloc failed:");
			items = next;
			cap = next_cap;
		}
		items[count++] = val;
	}

	*out_items = items;
	*out_len = count;
	return 1;
}

static uint32_t
tc_mod_token(const char *tok)
{
	if (strcasecmp(tok, "MODKEY") == 0)
		return tc_modkey;
	if (strcasecmp(tok, "SUPER") == 0 || strcasecmp(tok, "LOGO") == 0 ||
	    strcasecmp(tok, "WIN") == 0 || strcasecmp(tok, "MOD4") == 0 ||
	    strcasecmp(tok, "WLR_MODIFIER_LOGO") == 0)
		return WLR_MODIFIER_LOGO;
	if (strcasecmp(tok, "ALT") == 0 || strcasecmp(tok, "MOD1") == 0 ||
	    strcasecmp(tok, "WLR_MODIFIER_ALT") == 0)
		return WLR_MODIFIER_ALT;
	if (strcasecmp(tok, "CTRL") == 0 || strcasecmp(tok, "CONTROL") == 0 ||
	    strcasecmp(tok, "WLR_MODIFIER_CTRL") == 0)
		return WLR_MODIFIER_CTRL;
	if (strcasecmp(tok, "SHIFT") == 0 || strcasecmp(tok, "WLR_MODIFIER_SHIFT") == 0)
		return WLR_MODIFIER_SHIFT;
	if (strcasecmp(tok, "MOD3") == 0 || strcasecmp(tok, "WLR_MODIFIER_MOD3") == 0)
		return WLR_MODIFIER_MOD3;
	if (strcasecmp(tok, "MOD5") == 0 || strcasecmp(tok, "WLR_MODIFIER_MOD5") == 0)
		return WLR_MODIFIER_MOD5;
	return 0;
}

static uint32_t
tc_parse_mods_str(const char *s)
{
	uint32_t mask = 0;
	char *copy;
	char *p;

	copy = strdup(s);
	if (!copy)
		die("dwl: strdup failed:");

	p = copy;
	while (*p) {
		char *tok;
		while (*p && (isspace((unsigned char)*p) || *p == '+' || *p == '|' || *p == ','))
			p++;
		if (!*p)
			break;
		tok = p;
		while (*p && !isspace((unsigned char)*p) && *p != '+' && *p != '|' && *p != ',')
			p++;
		if (*p)
			*p++ = '\0';
		if (strcasecmp(tok, "NONE") != 0 && strcmp(tok, "0") != 0)
			mask |= tc_mod_token(tok);
	}

	free(copy);
	return mask;
}

static int
tc_parse_mods_value(const char *val, uint32_t *out)
{
	char *str = NULL;
	char **arr = NULL;
	size_t arr_len = 0;
	size_t i;
	uint32_t mask = 0;

	if (tc_parse_string_array(val, &arr, &arr_len)) {
		for (i = 0; i < arr_len; i++) {
			mask |= tc_parse_mods_str(arr[i]);
			free(arr[i]);
		}
		free(arr);
		*out = mask;
		return 1;
	}
	if (tc_parse_string(val, &str)) {
		*out = tc_parse_mods_str(str);
		free(str);
		return 1;
	}
	if (strcmp(val, "0") == 0) {
		*out = 0;
		return 1;
	}
	return 0;
}

static xkb_keysym_t
tc_parse_keysym(const char *name)
{
	xkb_keysym_t sym;

	if (!name || !*name)
		return XKB_KEY_NoSymbol;
	if (strncmp(name, "XKB_KEY_", 8) == 0)
		name += 8;
	sym = xkb_keysym_from_name(name, XKB_KEYSYM_NO_FLAGS);
	if (sym == XKB_KEY_NoSymbol)
		sym = xkb_keysym_from_name(name, XKB_KEYSYM_CASE_INSENSITIVE);
	return sym;
}

static const Layout *
tc_parse_layout(const char *val)
{
	char *str = NULL;
	const char *name;
	long idx;

	if (tc_parse_int(val, &idx)) {
		if (idx >= 0 && (size_t)idx < LENGTH(layouts))
			return &layouts[idx];
		return &layouts[0];
	}

	name = tc_parse_string(val, &str) ? str : val;
	if (strcasecmp(name, "dwindle") == 0 || strcmp(name, "[]=") == 0) {
		free(str);
		return &layouts[0];
	}
	if (strcasecmp(name, "floating") == 0 || strcasecmp(name, "float") == 0 ||
	    strcmp(name, "><>") == 0) {
		free(str);
		return &layouts[1];
	}
	if (strcasecmp(name, "tile") == 0) {
		free(str);
		return &layouts[2];
	}
	if (strcasecmp(name, "monocle") == 0 || strcmp(name, "[M]") == 0) {
		free(str);
		return &layouts[3];
	}
	free(str);
	return &layouts[0];
}

static enum wl_output_transform
tc_parse_transform(const char *s)
{
	if (strcasecmp(s, "90") == 0 || strcasecmp(s, "WL_OUTPUT_TRANSFORM_90") == 0)
		return WL_OUTPUT_TRANSFORM_90;
	if (strcasecmp(s, "180") == 0 || strcasecmp(s, "WL_OUTPUT_TRANSFORM_180") == 0)
		return WL_OUTPUT_TRANSFORM_180;
	if (strcasecmp(s, "270") == 0 || strcasecmp(s, "WL_OUTPUT_TRANSFORM_270") == 0)
		return WL_OUTPUT_TRANSFORM_270;
	if (strcasecmp(s, "flipped") == 0 || strcasecmp(s, "WL_OUTPUT_TRANSFORM_FLIPPED") == 0)
		return WL_OUTPUT_TRANSFORM_FLIPPED;
	if (strcasecmp(s, "flipped-90") == 0 || strcasecmp(s, "WL_OUTPUT_TRANSFORM_FLIPPED_90") == 0)
		return WL_OUTPUT_TRANSFORM_FLIPPED_90;
	if (strcasecmp(s, "flipped-180") == 0 || strcasecmp(s, "WL_OUTPUT_TRANSFORM_FLIPPED_180") == 0)
		return WL_OUTPUT_TRANSFORM_FLIPPED_180;
	if (strcasecmp(s, "flipped-270") == 0 || strcasecmp(s, "WL_OUTPUT_TRANSFORM_FLIPPED_270") == 0)
		return WL_OUTPUT_TRANSFORM_FLIPPED_270;
	return WL_OUTPUT_TRANSFORM_NORMAL;
}

static unsigned int
tc_parse_button(const char *s)
{
	long n;

	if (strcasecmp(s, "left") == 0 || strcasecmp(s, "BTN_LEFT") == 0)
		return BTN_LEFT;
	if (strcasecmp(s, "middle") == 0 || strcasecmp(s, "BTN_MIDDLE") == 0)
		return BTN_MIDDLE;
	if (strcasecmp(s, "right") == 0 || strcasecmp(s, "BTN_RIGHT") == 0)
		return BTN_RIGHT;
	if (strcasecmp(s, "side") == 0 || strcasecmp(s, "BTN_SIDE") == 0)
		return BTN_SIDE;
	if (strcasecmp(s, "extra") == 0 || strcasecmp(s, "BTN_EXTRA") == 0)
		return BTN_EXTRA;
	if (tc_parse_int(s, &n))
		return (unsigned int)n;
	return BTN_LEFT;
}

static unsigned int
tc_parse_axis_dir(const char *s)
{
	if (strcasecmp(s, "up") == 0 || strcasecmp(s, "AxisUp") == 0)
		return AxisUp;
	if (strcasecmp(s, "right") == 0 || strcasecmp(s, "AxisRight") == 0)
		return AxisRight;
	if (strcasecmp(s, "down") == 0 || strcasecmp(s, "AxisDown") == 0)
		return AxisDown;
	if (strcasecmp(s, "left") == 0 || strcasecmp(s, "AxisLeft") == 0)
		return AxisLeft;
	return AxisUp;
}

static unsigned int
tc_parse_dir_ui(const char *s)
{
	long n;

	if (!s)
		return 0;
	if (strcasecmp(s, "left") == 0)
		return 0;
	if (strcasecmp(s, "right") == 0)
		return 1;
	if (strcasecmp(s, "up") == 0)
		return 2;
	if (strcasecmp(s, "down") == 0)
		return 3;
	if (tc_parse_int(s, &n))
		return (unsigned int)n;
	return 0;
}

static int
tc_parse_mon_dir_i(const char *s)
{
	long n;

	if (!s)
		return WLR_DIRECTION_LEFT;
	if (strcasecmp(s, "left") == 0)
		return WLR_DIRECTION_LEFT;
	if (strcasecmp(s, "right") == 0)
		return WLR_DIRECTION_RIGHT;
	if (strcasecmp(s, "up") == 0)
		return WLR_DIRECTION_UP;
	if (strcasecmp(s, "down") == 0)
		return WLR_DIRECTION_DOWN;
	if (tc_parse_int(s, &n))
		return (int)n;
	return WLR_DIRECTION_LEFT;
}

static void (*tc_lookup_action(const char *name))(const Arg *)
{
	if (!name)
		return NULL;
	if (strcmp(name, "spawn") == 0) return spawn;
	if (strcmp(name, "reloadconfig") == 0) return reloadconfig;
	if (strcmp(name, "chvt") == 0) return chvt;
	if (strcmp(name, "defaultgaps") == 0) return defaultgaps;
	if (strcmp(name, "focusmon") == 0) return focusmon;
	if (strcmp(name, "focusstack") == 0) return focusstack;
	if (strcmp(name, "focusdir") == 0) return focusdir;
	if (strcmp(name, "swapdir") == 0) return swapdir;
	if (strcmp(name, "incnmaster") == 0) return incnmaster;
	if (strcmp(name, "incgaps") == 0) return incgaps;
	if (strcmp(name, "incigaps") == 0) return incigaps;
	if (strcmp(name, "incihgaps") == 0) return incihgaps;
	if (strcmp(name, "incivgaps") == 0) return incivgaps;
	if (strcmp(name, "incogaps") == 0) return incogaps;
	if (strcmp(name, "incohgaps") == 0) return incohgaps;
	if (strcmp(name, "incovgaps") == 0) return incovgaps;
	if (strcmp(name, "killclient") == 0) return killclient;
	if (strcmp(name, "moveresize") == 0) return moveresize;
	if (strcmp(name, "quit") == 0) return quit;
	if (strcmp(name, "setlayout") == 0) return setlayout;
	if (strcmp(name, "setmfact") == 0) return setmfact;
	if (strcmp(name, "tag") == 0) return tag;
	if (strcmp(name, "tagmon") == 0) return tagmon;
	if (strcmp(name, "togglebar") == 0) return togglebar;
	if (strcmp(name, "togglefloating") == 0) return togglefloating;
	if (strcmp(name, "togglefullscreen") == 0) return togglefullscreen;
	if (strcmp(name, "togglegaps") == 0) return togglegaps;
	if (strcmp(name, "toggletag") == 0) return toggletag;
	if (strcmp(name, "toggleview") == 0) return toggleview;
	if (strcmp(name, "view") == 0) return view;
	if (strcmp(name, "zoom") == 0) return zoom;
	return NULL;
}

static Arg
tc_build_arg(TomlArena *arena, const char *action, const TomlBindDraft *d)
{
	Arg a = {0};
	long iv;
	double fv;
	char **argv;
	size_t i;

	if (!action)
		return a;

	if (strcmp(action, "spawn") == 0) {
		if (d->arg_arr && d->arg_arr_len > 0) {
			argv = calloc(d->arg_arr_len + 1, sizeof(*argv));
			if (!argv)
				die("dwl: calloc failed:");
			tc_arena_track(arena, argv);
			for (i = 0; i < d->arg_arr_len; i++)
				argv[i] = tc_arena_strdup(arena, d->arg_arr[i]);
			argv[d->arg_arr_len] = NULL;
			a.v = argv;
		} else if (d->arg_str && *d->arg_str) {
			argv = calloc(4, sizeof(*argv));
			if (!argv)
				die("dwl: calloc failed:");
			tc_arena_track(arena, argv);
			argv[0] = tc_arena_strdup(arena, "/bin/sh");
			argv[1] = tc_arena_strdup(arena, "-c");
			argv[2] = tc_arena_strdup(arena, d->arg_str);
			argv[3] = NULL;
			a.v = argv;
		}
		return a;
	}

	if (strcmp(action, "setlayout") == 0) {
		a.v = d->arg_str ? tc_parse_layout(d->arg_str) : &layouts[0];
		return a;
	}

	if (strcmp(action, "moveresize") == 0) {
		if (d->arg_str && strcasecmp(d->arg_str, "resize") == 0)
			a.ui = CurResize;
		else
			a.ui = CurMove;
		return a;
	}

	if (strcmp(action, "focusdir") == 0 || strcmp(action, "swapdir") == 0) {
		a.ui = tc_parse_dir_ui(d->arg_str);
		return a;
	}

	if (strcmp(action, "focusmon") == 0 || strcmp(action, "tagmon") == 0) {
		a.i = tc_parse_mon_dir_i(d->arg_str);
		return a;
	}

	if (strcmp(action, "view") == 0 || strcmp(action, "toggleview") == 0 ||
	    strcmp(action, "tag") == 0 || strcmp(action, "toggletag") == 0) {
		if (d->has_tag_mask) {
			a.ui = d->tag_mask;
		} else if (d->has_tag) {
			if (d->tag == 0)
				a.ui = ~0u;
			else if (d->tag >= 1 && d->tag <= 31)
				a.ui = 1u << (unsigned int)(d->tag - 1);
		} else if (d->arg_str) {
			if (strcasecmp(d->arg_str, "all") == 0)
				a.ui = ~0u;
			else if (strcasecmp(d->arg_str, "prev") == 0)
				a.ui = 0;
			else if (tc_parse_int(d->arg_str, &iv)) {
				if (iv == 0)
					a.ui = 0;
				else if (iv >= 1 && iv <= 31)
					a.ui = 1u << (unsigned int)(iv - 1);
			}
		}
		return a;
	}

	if (strcmp(action, "setmfact") == 0) {
		if (d->arg_str && tc_parse_double(d->arg_str, &fv))
			a.f = (float)fv;
		return a;
	}

	if (strcmp(action, "chvt") == 0) {
		if (d->arg_str && tc_parse_int(d->arg_str, &iv))
			a.ui = (uint32_t)iv;
		return a;
	}

	if (d->arg_str && tc_parse_int(d->arg_str, &iv))
		a.i = (int)iv;

	return a;
}

static void
tc_free_draft(TomlBindDraft *d)
{
	size_t i;

	free(d->key_name);
	free(d->shift_key_name);
	free(d->action);
	free(d->arg_str);
	for (i = 0; i < d->arg_arr_len; i++)
		free(d->arg_arr[i]);
	free(d->arg_arr);
	memset(d, 0, sizeof(*d));
}

static void
tc_reset_scalar_defaults(void)
{
	sloppyfocus = 1;
	bypass_surface_visibility = 0;
	smartgaps = 0;
	monoclegaps = 0;
	borderpx = 2;
	gappih = 4;
	gappiv = 4;
	gappoh = 4;
	gappov = 4;
	snap = 32;
	adaptive_sync = 0;
	log_level = WLR_ERROR;

	tc_parse_color("#1a1b26ff", rootcolor);
	tc_parse_color("#414868ff", bordercolor);
	tc_parse_color("#7aa2f7ff", focuscolor);
	tc_parse_color("#f7768eff", urgentcolor);
	tc_parse_color("#000000ff", fullscreen_bg);

	tc_modkey = WLR_MODIFIER_LOGO;
	repeat_rate = 35;
	repeat_delay = 250;

	tap_to_click = 1;
	tap_and_drag = 1;
	drag_lock = 1;
	natural_scrolling = 0;
	disable_while_typing = 1;
	left_handed = 0;
	middle_button_emulation = 0;
	scroll_method = LIBINPUT_CONFIG_SCROLL_2FG;
	click_method = LIBINPUT_CONFIG_CLICK_METHOD_BUTTON_AREAS;
	send_events_mode = LIBINPUT_CONFIG_SEND_EVENTS_ENABLED;
	accel_profile = LIBINPUT_CONFIG_ACCEL_PROFILE_ADAPTIVE;
	accel_speed = 0.0;
	button_map = LIBINPUT_CONFIG_TAP_MAP_LRM;

	tc_xkb_rules = NULL;
	tc_xkb_model = NULL;
	tc_xkb_layout = NULL;
	tc_xkb_variant = NULL;
	tc_xkb_options = NULL;
	xkb_rules = default_xkb_rules;
}

static int
tc_resolve_config_path(char *buf, size_t buf_sz)
{
	const char *xdg;
	const char *home;

	xdg = getenv("XDG_CONFIG_HOME");
	home = getenv("HOME");
	if (xdg && *xdg) {
		snprintf(buf, buf_sz, "%s/dwl/config.toml", xdg);
		return 1;
	}
	if (home && *home) {
		snprintf(buf, buf_sz, "%s/.config/dwl/config.toml", home);
		return 1;
	}
	return 0;
}

static void
tc_apply_rule_kv(TomlRuleDraft *rd, const char *k, char *v)
{
	long lv;
	int bv;
	char *sv = NULL;

	if (strcmp(k, "app_id") == 0 && tc_parse_string(v, &sv)) {
		free(rd->app_id);
		rd->app_id = *sv ? sv : NULL;
		if (!rd->app_id) free(sv);
	} else if (strcmp(k, "title") == 0 && tc_parse_string(v, &sv)) {
		free(rd->title);
		rd->title = *sv ? sv : NULL;
		if (!rd->title) free(sv);
	} else if (strcmp(k, "tags") == 0 && tc_parse_int(v, &lv)) {
		if (lv >= 1 && lv <= 31)
			rd->tags = 1u << (unsigned int)(lv - 1);
		else
			rd->tags = 0;
	} else if (strcmp(k, "tag_mask") == 0 && tc_parse_int(v, &lv)) {
		rd->tags = (uint32_t)lv;
	} else if (strcmp(k, "floating") == 0 && tc_parse_bool(v, &bv)) {
		rd->isfloating = bv;
	} else if (strcmp(k, "monitor") == 0 && tc_parse_int(v, &lv)) {
		rd->monitor = (int)lv;
	} else if (strcmp(k, "width") == 0 && tc_parse_int(v, &lv)) {
		rd->width = lv > 0 ? (int)lv : 0;
	} else if (strcmp(k, "height") == 0 && tc_parse_int(v, &lv)) {
		rd->height = lv > 0 ? (int)lv : 0;
	}
}

static void
tc_apply_bind_kv(TomlBindDraft *bd, const char *k, char *v)
{
	long lv;
	char *sv = NULL;
	size_t i;

	if (strcmp(k, "mod") == 0) {
		uint32_t m = 0;
		if (tc_parse_mods_value(v, &m)) {
			bd->has_mod = 1;
			bd->mod = m;
		}
	} else if (strcmp(k, "key") == 0 && tc_parse_string(v, &sv)) {
		free(bd->key_name);
		bd->key_name = sv;
		bd->has_key = 1;
	} else if (strcmp(k, "shift_key") == 0 && tc_parse_string(v, &sv)) {
		free(bd->shift_key_name);
		bd->shift_key_name = sv;
		bd->has_shift_key = 1;
	} else if (strcmp(k, "action") == 0 && tc_parse_string(v, &sv)) {
		free(bd->action);
		bd->action = sv;
		bd->has_action = 1;
	} else if (strcmp(k, "args") == 0) {
		for (i = 0; i < bd->arg_arr_len; i++)
			free(bd->arg_arr[i]);
		free(bd->arg_arr);
		bd->arg_arr = NULL;
		bd->arg_arr_len = 0;
		if (tc_parse_string_array(v, &bd->arg_arr, &bd->arg_arr_len))
			bd->has_arg = 1;
	} else if (strcmp(k, "arg") == 0) {
		free(bd->arg_str);
		if (tc_parse_string(v, &sv))
			bd->arg_str = sv;
		else
			bd->arg_str = strdup(v);
		if (!bd->arg_str) die("dwl: strdup failed:");
		bd->has_arg = 1;
	} else if (strcmp(k, "tag") == 0 && tc_parse_int(v, &lv)) {
		bd->has_tag = 1;
		bd->tag = (int)lv;
	} else if (strcmp(k, "tag_mask") == 0 && tc_parse_int(v, &lv)) {
		bd->has_tag_mask = 1;
		bd->tag_mask = (uint32_t)lv;
	} else if (strcmp(k, "button") == 0 && tc_parse_string(v, &sv)) {
		bd->button = tc_parse_button(sv);
		bd->has_button = 1;
		free(sv);
	} else if ((strcmp(k, "dir") == 0 || strcmp(k, "direction") == 0) && tc_parse_string(v, &sv)) {
		bd->dir = tc_parse_axis_dir(sv);
		bd->has_dir = 1;
		free(sv);
	}
}

static void
tc_parse_inline_fields(char *body, TomlRuleDraft *rd, TomlBindDraft *bd)
{
	char *p = body;

	while (*p) {
		int in_dq = 0, in_sq = 0, esc = 0, depth = 0;
		char *entry;
		char *eq;
		char *k;
		char *v;

		while (*p && (isspace((unsigned char)*p) || *p == ','))
			p++;
		if (!*p)
			break;

		entry = p;
		while (*p) {
			if (esc) {
				esc = 0;
			} else if (in_dq) {
				if (*p == '\\') esc = 1;
				else if (*p == '"') in_dq = 0;
			} else if (in_sq) {
				if (*p == '\'') in_sq = 0;
			} else {
				if (*p == '"') in_dq = 1;
				else if (*p == '\'') in_sq = 1;
				else if (*p == '[') depth++;
				else if (*p == ']' && depth > 0) depth--;
				else if (*p == ',' && depth == 0) {
					*p++ = '\0';
					break;
				}
			}
			p++;
		}

		eq = strchr(entry, '=');
		if (!eq)
			continue;
		*eq = '\0';
		k = tc_trim(entry);
		v = tc_trim(eq + 1);
		if (rd)
			tc_apply_rule_kv(rd, k, v);
		else if (bd)
			tc_apply_bind_kv(bd, k, v);
	}
}

static void
load_toml_config(void)
{
	char path[PATH_MAX];
	FILE *fp;
	TomlArena arena = {0};
	char line_buf[8192];
	char section[64] = "";
	int inline_list = 0; /* 1=rules, 2=keys, 3=tagkeys, 4=buttons, 5=axes */

	TomlBindDraft *key_drafts = NULL;
	size_t nkey_drafts = 0, cap_key_drafts = 0;

	TomlBindDraft *tagkey_drafts = NULL;
	size_t ntagkey_drafts = 0, cap_tagkey_drafts = 0;

	TomlBindDraft *btn_drafts = NULL;
	size_t nbtn_drafts = 0, cap_btn_drafts = 0;

	TomlBindDraft *axis_drafts = NULL;
	size_t naxis_drafts = 0, cap_axis_drafts = 0;

	TomlRuleDraft *rule_drafts = NULL;
	size_t nrule_drafts = 0, cap_rule_drafts = 0;

	TomlMonDraft *mon_drafts = NULL;
	size_t nmon_drafts = 0, cap_mon_drafts = 0;

	int has_autostart = 0;
	char **autostart_cmds = NULL;
	size_t nautostart_cmds = 0;

	size_t i;

	tc_reset_scalar_defaults();

	if (!tc_resolve_config_path(path, sizeof(path)))
		return;

	fp = fopen(path, "r");
	if (!fp) {
		tc_arena_free(&tc_active_arena);
		autostart = default_autostart;
		rules = default_rules;
		nrules = LENGTH(default_rules);
		monrules = default_monrules;
		nmonrules = LENGTH(default_monrules);
		keys = default_keys;
		nkeys = LENGTH(default_keys);
		buttons = default_buttons;
		nbuttons = LENGTH(default_buttons);
		axes = default_axes;
		naxes = LENGTH(default_axes);
		return;
	}

	while (fgets(line_buf, sizeof(line_buf), fp)) {
		char *line;
		char *eq;
		char *k;
		char *v;
		long lv;
		double dv;
		int bv;
		char *sv = NULL;

		tc_strip_comment(line_buf);
		line = tc_trim(line_buf);
		if (!*line)
			continue;

		if (line[0] == '[' && line[1] == '[') {
			size_t l = strlen(line);
			inline_list = 0;
			if (l >= 4 && line[l - 1] == ']' && line[l - 2] == ']') {
				line[l - 2] = '\0';
				snprintf(section, sizeof(section), "%s", tc_trim(line + 2));
				if (strcmp(section, "keys") == 0) {
					if (nkey_drafts == cap_key_drafts) {
						cap_key_drafts = cap_key_drafts ? cap_key_drafts * 2 : 32;
						key_drafts = realloc(key_drafts, cap_key_drafts * sizeof(*key_drafts));
						if (!key_drafts) die("dwl: realloc failed:");
					}
					memset(&key_drafts[nkey_drafts++], 0, sizeof(*key_drafts));
				} else if (strcmp(section, "tagkeys") == 0) {
					if (ntagkey_drafts == cap_tagkey_drafts) {
						cap_tagkey_drafts = cap_tagkey_drafts ? cap_tagkey_drafts * 2 : 16;
						tagkey_drafts = realloc(tagkey_drafts, cap_tagkey_drafts * sizeof(*tagkey_drafts));
						if (!tagkey_drafts) die("dwl: realloc failed:");
					}
					memset(&tagkey_drafts[ntagkey_drafts++], 0, sizeof(*tagkey_drafts));
				} else if (strcmp(section, "buttons") == 0) {
					if (nbtn_drafts == cap_btn_drafts) {
						cap_btn_drafts = cap_btn_drafts ? cap_btn_drafts * 2 : 8;
						btn_drafts = realloc(btn_drafts, cap_btn_drafts * sizeof(*btn_drafts));
						if (!btn_drafts) die("dwl: realloc failed:");
					}
					memset(&btn_drafts[nbtn_drafts++], 0, sizeof(*btn_drafts));
				} else if (strcmp(section, "axes") == 0) {
					if (naxis_drafts == cap_axis_drafts) {
						cap_axis_drafts = cap_axis_drafts ? cap_axis_drafts * 2 : 8;
						axis_drafts = realloc(axis_drafts, cap_axis_drafts * sizeof(*axis_drafts));
						if (!axis_drafts) die("dwl: realloc failed:");
					}
					memset(&axis_drafts[naxis_drafts++], 0, sizeof(*axis_drafts));
				} else if (strcmp(section, "rules") == 0) {
					if (nrule_drafts == cap_rule_drafts) {
						cap_rule_drafts = cap_rule_drafts ? cap_rule_drafts * 2 : 16;
						rule_drafts = realloc(rule_drafts, cap_rule_drafts * sizeof(*rule_drafts));
						if (!rule_drafts) die("dwl: realloc failed:");
					}
					memset(&rule_drafts[nrule_drafts], 0, sizeof(*rule_drafts));
					rule_drafts[nrule_drafts].monitor = -1;
					nrule_drafts++;
				} else if (strcmp(section, "monitors") == 0) {
					if (nmon_drafts == cap_mon_drafts) {
						cap_mon_drafts = cap_mon_drafts ? cap_mon_drafts * 2 : 8;
						mon_drafts = realloc(mon_drafts, cap_mon_drafts * sizeof(*mon_drafts));
						if (!mon_drafts) die("dwl: realloc failed:");
					}
					memset(&mon_drafts[nmon_drafts], 0, sizeof(*mon_drafts));
					mon_drafts[nmon_drafts].mfact = 0.55f;
					mon_drafts[nmon_drafts].nmaster = 1;
					mon_drafts[nmon_drafts].scale = 1.0f;
					mon_drafts[nmon_drafts].lt = &layouts[0];
					mon_drafts[nmon_drafts].rr = WL_OUTPUT_TRANSFORM_NORMAL;
					mon_drafts[nmon_drafts].x = -1;
					mon_drafts[nmon_drafts].y = -1;
					nmon_drafts++;
				}
			}
			continue;
		}

		if (inline_list) {
			char *lbrace = strchr(line, '{');
			if (lbrace) {
				char *rbrace = strrchr(lbrace + 1, '}');
				if (rbrace) {
					*rbrace = '\0';
					if (inline_list == 1) {
						if (nrule_drafts == cap_rule_drafts) {
							cap_rule_drafts = cap_rule_drafts ? cap_rule_drafts * 2 : 16;
							rule_drafts = realloc(rule_drafts, cap_rule_drafts * sizeof(*rule_drafts));
							if (!rule_drafts) die("dwl: realloc failed:");
						}
						memset(&rule_drafts[nrule_drafts], 0, sizeof(*rule_drafts));
						rule_drafts[nrule_drafts].monitor = -1;
						tc_parse_inline_fields(lbrace + 1, &rule_drafts[nrule_drafts], NULL);
						nrule_drafts++;
					} else {
						TomlBindDraft **arr =
							inline_list == 2 ? &key_drafts :
							inline_list == 3 ? &tagkey_drafts :
							inline_list == 4 ? &btn_drafts : &axis_drafts;
						size_t *cnt =
							inline_list == 2 ? &nkey_drafts :
							inline_list == 3 ? &ntagkey_drafts :
							inline_list == 4 ? &nbtn_drafts : &naxis_drafts;
						size_t *cap =
							inline_list == 2 ? &cap_key_drafts :
							inline_list == 3 ? &cap_tagkey_drafts :
							inline_list == 4 ? &cap_btn_drafts : &cap_axis_drafts;
						if (*cnt == *cap) {
							*cap = *cap ? *cap * 2 : 32;
							*arr = realloc(*arr, *cap * sizeof(**arr));
							if (!*arr) die("dwl: realloc failed:");
						}
						memset(&(*arr)[*cnt], 0, sizeof(**arr));
						tc_parse_inline_fields(lbrace + 1, NULL, &(*arr)[*cnt]);
						(*cnt)++;
					}
					if (strchr(rbrace + 1, ']'))
						inline_list = 0;
				}
				continue;
			}
			if (strchr(line, ']')) {
				inline_list = 0;
				continue;
			}
		}

		if (line[0] == '[') {
			size_t l = strlen(line);
			inline_list = 0;
			if (l >= 2 && line[l - 1] == ']') {
				line[l - 1] = '\0';
				snprintf(section, sizeof(section), "%s", tc_trim(line + 1));
			}
			continue;
		}

		eq = strchr(line, '=');
		if (!eq)
			continue;
		*eq = '\0';
		k = tc_trim(line);
		v = tc_trim(eq + 1);

		if (v[0] == '[' && (strcmp(section, "bindings") == 0 || strcmp(section, "window_rules") == 0 || section[0] == '\0')) {
			if (strcmp(k, "rules") == 0) {
				if (!strchr(v + 1, ']')) inline_list = 1;
				continue;
			} else if (strcmp(k, "keys") == 0) {
				if (!strchr(v + 1, ']')) inline_list = 2;
				continue;
			} else if (strcmp(k, "tagkeys") == 0) {
				if (!strchr(v + 1, ']')) inline_list = 3;
				continue;
			} else if (strcmp(k, "buttons") == 0) {
				if (!strchr(v + 1, ']')) inline_list = 4;
				continue;
			} else if (strcmp(k, "axes") == 0) {
				if (!strchr(v + 1, ']')) inline_list = 5;
				continue;
			}
		}

		if (strcmp(section, "appearance") == 0) {
			if (strcmp(k, "sloppyfocus") == 0 && tc_parse_bool(v, &bv))
				sloppyfocus = bv;
			else if (strcmp(k, "bypass_surface_visibility") == 0 && tc_parse_bool(v, &bv))
				bypass_surface_visibility = bv;
			else if (strcmp(k, "smartgaps") == 0 && tc_parse_bool(v, &bv))
				smartgaps = bv;
			else if (strcmp(k, "monoclegaps") == 0 && tc_parse_bool(v, &bv))
				monoclegaps = bv;
			else if (strcmp(k, "borderpx") == 0 && tc_parse_int(v, &lv))
				borderpx = (unsigned int)(lv < 0 ? 0 : lv);
			else if (strcmp(k, "gappih") == 0 && tc_parse_int(v, &lv))
				gappih = (unsigned int)(lv < 0 ? 0 : lv);
			else if (strcmp(k, "gappiv") == 0 && tc_parse_int(v, &lv))
				gappiv = (unsigned int)(lv < 0 ? 0 : lv);
			else if (strcmp(k, "gappoh") == 0 && tc_parse_int(v, &lv))
				gappoh = (unsigned int)(lv < 0 ? 0 : lv);
			else if (strcmp(k, "gappov") == 0 && tc_parse_int(v, &lv))
				gappov = (unsigned int)(lv < 0 ? 0 : lv);
			else if (strcmp(k, "snap") == 0 && tc_parse_int(v, &lv))
				snap = (unsigned int)(lv < 0 ? 0 : lv);
			else if (strcmp(k, "adaptive_sync") == 0 && tc_parse_bool(v, &bv))
				adaptive_sync = bv;
			else if (strcmp(k, "log_level") == 0 && tc_parse_string(v, &sv)) {
				if (strcasecmp(sv, "silent") == 0) log_level = WLR_SILENT;
				else if (strcasecmp(sv, "error") == 0) log_level = WLR_ERROR;
				else if (strcasecmp(sv, "info") == 0) log_level = WLR_INFO;
				else if (strcasecmp(sv, "debug") == 0) log_level = WLR_DEBUG;
				free(sv);
			} else if (strcmp(k, "rootcolor") == 0)
				tc_parse_color(v, rootcolor);
			else if (strcmp(k, "bordercolor") == 0)
				tc_parse_color(v, bordercolor);
			else if (strcmp(k, "focuscolor") == 0)
				tc_parse_color(v, focuscolor);
			else if (strcmp(k, "urgentcolor") == 0)
				tc_parse_color(v, urgentcolor);
			else if (strcmp(k, "fullscreen_bg") == 0)
				tc_parse_color(v, fullscreen_bg);
		} else if (strcmp(section, "keyboard") == 0) {
			if (strcmp(k, "modkey") == 0 && tc_parse_string(v, &sv)) {
				uint32_t m = tc_mod_token(sv);
				if (m)
					tc_modkey = m;
				free(sv);
			} else if (strcmp(k, "repeat_rate") == 0 && tc_parse_int(v, &lv))
				repeat_rate = (int)lv;
			else if (strcmp(k, "repeat_delay") == 0 && tc_parse_int(v, &lv))
				repeat_delay = (int)lv;
			else if (strcmp(k, "xkb_rules") == 0 && tc_parse_string(v, &sv)) {
				tc_xkb_rules = *sv ? tc_arena_strdup(&arena, sv) : NULL;
				xkb_rules.rules = tc_xkb_rules;
				free(sv);
			} else if (strcmp(k, "xkb_model") == 0 && tc_parse_string(v, &sv)) {
				tc_xkb_model = *sv ? tc_arena_strdup(&arena, sv) : NULL;
				xkb_rules.model = tc_xkb_model;
				free(sv);
			} else if (strcmp(k, "xkb_layout") == 0 && tc_parse_string(v, &sv)) {
				tc_xkb_layout = *sv ? tc_arena_strdup(&arena, sv) : NULL;
				xkb_rules.layout = tc_xkb_layout;
				free(sv);
			} else if (strcmp(k, "xkb_variant") == 0 && tc_parse_string(v, &sv)) {
				tc_xkb_variant = *sv ? tc_arena_strdup(&arena, sv) : NULL;
				xkb_rules.variant = tc_xkb_variant;
				free(sv);
			} else if (strcmp(k, "xkb_options") == 0 && tc_parse_string(v, &sv)) {
				tc_xkb_options = *sv ? tc_arena_strdup(&arena, sv) : NULL;
				xkb_rules.options = tc_xkb_options;
				free(sv);
			}
		} else if (strcmp(section, "trackpad") == 0) {
			if (strcmp(k, "tap_to_click") == 0 && tc_parse_bool(v, &bv))
				tap_to_click = bv;
			else if (strcmp(k, "tap_and_drag") == 0 && tc_parse_bool(v, &bv))
				tap_and_drag = bv;
			else if (strcmp(k, "drag_lock") == 0 && tc_parse_bool(v, &bv))
				drag_lock = bv;
			else if (strcmp(k, "natural_scrolling") == 0 && tc_parse_bool(v, &bv))
				natural_scrolling = bv;
			else if (strcmp(k, "disable_while_typing") == 0 && tc_parse_bool(v, &bv))
				disable_while_typing = bv;
			else if (strcmp(k, "left_handed") == 0 && tc_parse_bool(v, &bv))
				left_handed = bv;
			else if (strcmp(k, "middle_button_emulation") == 0 && tc_parse_bool(v, &bv))
				middle_button_emulation = bv;
			else if (strcmp(k, "accel_speed") == 0 && tc_parse_double(v, &dv))
				accel_speed = dv;
			else if (strcmp(k, "scroll_method") == 0 && tc_parse_string(v, &sv)) {
				if (strcasecmp(sv, "none") == 0) scroll_method = LIBINPUT_CONFIG_SCROLL_NO_SCROLL;
				else if (strcasecmp(sv, "2fg") == 0) scroll_method = LIBINPUT_CONFIG_SCROLL_2FG;
				else if (strcasecmp(sv, "edge") == 0) scroll_method = LIBINPUT_CONFIG_SCROLL_EDGE;
				else if (strcasecmp(sv, "button") == 0) scroll_method = LIBINPUT_CONFIG_SCROLL_ON_BUTTON_DOWN;
				free(sv);
			} else if (strcmp(k, "click_method") == 0 && tc_parse_string(v, &sv)) {
				if (strcasecmp(sv, "none") == 0) click_method = LIBINPUT_CONFIG_CLICK_METHOD_NONE;
				else if (strcasecmp(sv, "button_areas") == 0) click_method = LIBINPUT_CONFIG_CLICK_METHOD_BUTTON_AREAS;
				else if (strcasecmp(sv, "clickfinger") == 0) click_method = LIBINPUT_CONFIG_CLICK_METHOD_CLICKFINGER;
				free(sv);
			} else if (strcmp(k, "send_events_mode") == 0 && tc_parse_string(v, &sv)) {
				if (strcasecmp(sv, "enabled") == 0) send_events_mode = LIBINPUT_CONFIG_SEND_EVENTS_ENABLED;
				else if (strcasecmp(sv, "disabled") == 0) send_events_mode = LIBINPUT_CONFIG_SEND_EVENTS_DISABLED;
				else if (strcasecmp(sv, "disabled_on_external_mouse") == 0) send_events_mode = LIBINPUT_CONFIG_SEND_EVENTS_DISABLED_ON_EXTERNAL_MOUSE;
				free(sv);
			} else if (strcmp(k, "accel_profile") == 0 && tc_parse_string(v, &sv)) {
				if (strcasecmp(sv, "flat") == 0) accel_profile = LIBINPUT_CONFIG_ACCEL_PROFILE_FLAT;
				else if (strcasecmp(sv, "adaptive") == 0) accel_profile = LIBINPUT_CONFIG_ACCEL_PROFILE_ADAPTIVE;
				free(sv);
			} else if (strcmp(k, "button_map") == 0 && tc_parse_string(v, &sv)) {
				if (strcasecmp(sv, "lrm") == 0) button_map = LIBINPUT_CONFIG_TAP_MAP_LRM;
				else if (strcasecmp(sv, "lmr") == 0) button_map = LIBINPUT_CONFIG_TAP_MAP_LMR;
				free(sv);
			}
		} else if (strcmp(section, "autostart") == 0) {
			if (strcmp(k, "commands") == 0) {
				for (i = 0; i < nautostart_cmds; i++)
					free(autostart_cmds[i]);
				free(autostart_cmds);
				autostart_cmds = NULL;
				nautostart_cmds = 0;
				if (tc_parse_string_array(v, &autostart_cmds, &nautostart_cmds))
					has_autostart = 1;
			}
		} else if (strcmp(section, "monitors") == 0 && nmon_drafts > 0) {
			TomlMonDraft *md = &mon_drafts[nmon_drafts - 1];
			if (strcmp(k, "name") == 0 && tc_parse_string(v, &sv)) {
				free(md->name);
				if (*sv && strcmp(sv, "*") != 0)
					md->name = sv;
				else {
					md->name = NULL;
					free(sv);
				}
			} else if (strcmp(k, "mfact") == 0 && tc_parse_double(v, &dv))
				md->mfact = (float)dv;
			else if (strcmp(k, "nmaster") == 0 && tc_parse_int(v, &lv))
				md->nmaster = (int)lv;
			else if (strcmp(k, "width") == 0 && tc_parse_int(v, &lv))
				md->resx = (int)lv;
			else if (strcmp(k, "height") == 0 && tc_parse_int(v, &lv))
				md->resy = (int)lv;
			else if (strcmp(k, "refresh") == 0 && tc_parse_double(v, &dv))
				md->refresh = (float)dv;
			else if (strcmp(k, "scale") == 0 && tc_parse_double(v, &dv))
				md->scale = (float)dv;
			else if (strcmp(k, "layout") == 0)
				md->lt = tc_parse_layout(v);
			else if (strcmp(k, "transform") == 0 && tc_parse_string(v, &sv)) {
				md->rr = tc_parse_transform(sv);
				free(sv);
			} else if (strcmp(k, "x") == 0 && tc_parse_int(v, &lv))
				md->x = (int)lv;
			else if (strcmp(k, "y") == 0 && tc_parse_int(v, &lv))
				md->y = (int)lv;
		} else if (strcmp(section, "rules") == 0 && nrule_drafts > 0) {
			tc_apply_rule_kv(&rule_drafts[nrule_drafts - 1], k, v);
		} else if ((strcmp(section, "keys") == 0 && nkey_drafts > 0) ||
		           (strcmp(section, "tagkeys") == 0 && ntagkey_drafts > 0) ||
		           (strcmp(section, "buttons") == 0 && nbtn_drafts > 0) ||
		           (strcmp(section, "axes") == 0 && naxis_drafts > 0)) {
			TomlBindDraft *bd =
				strcmp(section, "keys") == 0 ? &key_drafts[nkey_drafts - 1] :
				strcmp(section, "tagkeys") == 0 ? &tagkey_drafts[ntagkey_drafts - 1] :
				strcmp(section, "buttons") == 0 ? &btn_drafts[nbtn_drafts - 1] :
				&axis_drafts[naxis_drafts - 1];
			tc_apply_bind_kv(bd, k, v);
		}
	}
	fclose(fp);

	if (has_autostart) {
		char **as = calloc(nautostart_cmds + 1, sizeof(*as));
		if (!as) die("dwl: calloc failed:");
		tc_arena_track(&arena, as);
		for (i = 0; i < nautostart_cmds; i++) {
			as[i] = tc_arena_strdup(&arena, autostart_cmds[i]);
			free(autostart_cmds[i]);
		}
		free(autostart_cmds);
		as[nautostart_cmds] = NULL;
		autostart = (const char *const *)as;
	} else {
		autostart = default_autostart;
	}

	if (nrule_drafts > 0) {
		Rule *rs = calloc(nrule_drafts, sizeof(*rs));
		if (!rs) die("dwl: calloc failed:");
		tc_arena_track(&arena, rs);
		for (i = 0; i < nrule_drafts; i++) {
			rs[i].id = tc_arena_strdup(&arena, rule_drafts[i].app_id);
			rs[i].title = tc_arena_strdup(&arena, rule_drafts[i].title);
			rs[i].tags = rule_drafts[i].tags;
			rs[i].isfloating = rule_drafts[i].isfloating;
			rs[i].monitor = rule_drafts[i].monitor;
			rs[i].width = rule_drafts[i].width;
			rs[i].height = rule_drafts[i].height;
			free(rule_drafts[i].app_id);
			free(rule_drafts[i].title);
		}
		free(rule_drafts);
		rules = rs;
		nrules = nrule_drafts;
	} else {
		rules = default_rules;
		nrules = LENGTH(default_rules);
	}

	if (nmon_drafts > 0) {
		int has_fallback = 0;
		size_t total_mons;
		MonitorRule *ms;

		for (i = 0; i < nmon_drafts; i++) {
			if (!mon_drafts[i].name)
				has_fallback = 1;
		}
		total_mons = nmon_drafts + (has_fallback ? 0 : 1);
		ms = calloc(total_mons, sizeof(*ms));
		if (!ms) die("dwl: calloc failed:");
		tc_arena_track(&arena, ms);
		for (i = 0; i < nmon_drafts; i++) {
			ms[i].name = tc_arena_strdup(&arena, mon_drafts[i].name);
			ms[i].mfact = mon_drafts[i].mfact;
			ms[i].nmaster = mon_drafts[i].nmaster;
			ms[i].resx = mon_drafts[i].resx;
			ms[i].resy = mon_drafts[i].resy;
			ms[i].refresh = mon_drafts[i].refresh;
			ms[i].scale = mon_drafts[i].scale;
			ms[i].lt = mon_drafts[i].lt;
			ms[i].rr = mon_drafts[i].rr;
			ms[i].x = mon_drafts[i].x;
			ms[i].y = mon_drafts[i].y;
			free(mon_drafts[i].name);
		}
		if (!has_fallback) {
			ms[nmon_drafts].name = NULL;
			ms[nmon_drafts].mfact = 0.55f;
			ms[nmon_drafts].nmaster = 1;
			ms[nmon_drafts].resx = 0;
			ms[nmon_drafts].resy = 0;
			ms[nmon_drafts].refresh = 0.0f;
			ms[nmon_drafts].scale = 1.0f;
			ms[nmon_drafts].lt = &layouts[0];
			ms[nmon_drafts].rr = WL_OUTPUT_TRANSFORM_NORMAL;
			ms[nmon_drafts].x = -1;
			ms[nmon_drafts].y = -1;
		}
		free(mon_drafts);
		monrules = ms;
		nmonrules = total_mons;
	} else {
		monrules = default_monrules;
		nmonrules = LENGTH(default_monrules);
	}

	if (nkey_drafts > 0 || ntagkey_drafts > 0) {
		size_t max_keys = nkey_drafts + ntagkey_drafts * 5;
		Key *ks = calloc(max_keys ? max_keys : 1, sizeof(*ks));
		size_t ki = 0;

		if (!ks) die("dwl: calloc failed:");
		tc_arena_track(&arena, ks);

		for (i = 0; i < nkey_drafts; i++) {
			void (*fn)(const Arg *) = tc_lookup_action(key_drafts[i].action);
			xkb_keysym_t sym = tc_parse_keysym(key_drafts[i].key_name);
			if (fn && sym != XKB_KEY_NoSymbol) {
				Key item = {
					.mod = key_drafts[i].has_mod ? key_drafts[i].mod : 0,
					.keysym = sym,
					.func = fn,
					.arg = tc_build_arg(&arena, key_drafts[i].action, &key_drafts[i]),
				};
				memcpy(&ks[ki++], &item, sizeof(item));
			}
			tc_free_draft(&key_drafts[i]);
		}
		free(key_drafts);

		for (i = 0; i < ntagkey_drafts; i++) {
			xkb_keysym_t sym = tc_parse_keysym(tagkey_drafts[i].key_name);
			xkb_keysym_t ssym = tc_parse_keysym(tagkey_drafts[i].shift_key_name);
			int t = tagkey_drafts[i].tag;
			if (sym != XKB_KEY_NoSymbol && t >= 1 && t <= 31) {
				uint32_t mask = 1u << (unsigned int)(t - 1);
				Key k0 = { .mod = tc_modkey, .keysym = sym, .func = view, .arg = {.ui = mask} };
				Key k1 = { .mod = tc_modkey | WLR_MODIFIER_CTRL, .keysym = sym, .func = toggleview, .arg = {.ui = mask} };
				Key k2 = { .mod = tc_modkey | WLR_MODIFIER_SHIFT, .keysym = sym, .func = tag, .arg = {.ui = mask} };
				Key k3 = { .mod = tc_modkey | WLR_MODIFIER_CTRL | WLR_MODIFIER_SHIFT, .keysym = sym, .func = toggletag, .arg = {.ui = mask} };
				memcpy(&ks[ki++], &k0, sizeof(k0));
				memcpy(&ks[ki++], &k1, sizeof(k1));
				memcpy(&ks[ki++], &k2, sizeof(k2));
				memcpy(&ks[ki++], &k3, sizeof(k3));
				if (ssym != XKB_KEY_NoSymbol) {
					Key k4 = { .mod = tc_modkey | WLR_MODIFIER_SHIFT, .keysym = ssym, .func = view, .arg = {.ui = mask} };
					memcpy(&ks[ki++], &k4, sizeof(k4));
				}
			}
			tc_free_draft(&tagkey_drafts[i]);
		}
		free(tagkey_drafts);

		keys = ks;
		nkeys = ki;
	} else {
		keys = default_keys;
		nkeys = LENGTH(default_keys);
	}

	if (nbtn_drafts > 0) {
		Button *bs = calloc(nbtn_drafts, sizeof(*bs));
		size_t bi = 0;
		if (!bs) die("dwl: calloc failed:");
		tc_arena_track(&arena, bs);
		for (i = 0; i < nbtn_drafts; i++) {
			void (*fn)(const Arg *) = tc_lookup_action(btn_drafts[i].action);
			if (fn && btn_drafts[i].has_button) {
				Button item = {
					.mod = btn_drafts[i].has_mod ? btn_drafts[i].mod : tc_modkey,
					.button = btn_drafts[i].button,
					.func = fn,
					.arg = tc_build_arg(&arena, btn_drafts[i].action, &btn_drafts[i]),
				};
				memcpy(&bs[bi++], &item, sizeof(item));
			}
			tc_free_draft(&btn_drafts[i]);
		}
		free(btn_drafts);
		buttons = bs;
		nbuttons = bi;
	} else {
		buttons = default_buttons;
		nbuttons = LENGTH(default_buttons);
	}

	if (naxis_drafts > 0) {
		Axis *as = calloc(naxis_drafts, sizeof(*as));
		size_t ai = 0;
		if (!as) die("dwl: calloc failed:");
		tc_arena_track(&arena, as);
		for (i = 0; i < naxis_drafts; i++) {
			void (*fn)(const Arg *) = tc_lookup_action(axis_drafts[i].action);
			if (fn && axis_drafts[i].has_dir) {
				Axis item = {
					.mod = axis_drafts[i].has_mod ? axis_drafts[i].mod : tc_modkey,
					.dir = axis_drafts[i].dir,
					.func = fn,
					.arg = tc_build_arg(&arena, axis_drafts[i].action, &axis_drafts[i]),
				};
				memcpy(&as[ai++], &item, sizeof(item));
			}
			tc_free_draft(&axis_drafts[i]);
		}
		free(axis_drafts);
		axes = as;
		naxes = ai;
	} else {
		axes = default_axes;
		naxes = LENGTH(default_axes);
	}

	tc_arena_free(&tc_active_arena);
	tc_active_arena = arena;
}

#endif /* TOML_CONFIG_H */
