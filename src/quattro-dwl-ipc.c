#define _POSIX_C_SOURCE 200809L

#include <errno.h>
#include <poll.h>
#include <signal.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <wayland-client.h>

#include "dwl-ipc-unstable-v2-protocol.h"
#include "xdg-output-unstable-v1-protocol.h"

#define MAX_OUTPUTS 32
#define MAX_TAGS 32
#define MAX_LAYOUTS 32
#define MAX_MODES 64

struct tag_state {
	uint32_t state;
	uint32_t clients;
	uint32_t focused;
};

struct mode_info {
	int32_t width;
	int32_t height;
	int32_t refresh_mhz;
	bool preferred;
	bool current;
};

struct output_info {
	bool used;
	uint32_t registry_name;
	struct wl_output *wl_output;
	struct zxdg_output_v1 *xdg_output;
	struct zdwl_ipc_output_v2 *ipc_output;
	char name[64];
	char make[128];
	char model[128];
	char description[256];
	char title[512];
	char appid[256];
	char layout_symbol[64];
	uint32_t layout_idx;
	bool active;
	bool fullscreen;
	bool floating;
	int32_t geom_x;
	int32_t geom_y;
	int32_t geom_w;
	int32_t geom_h;
	int32_t wl_x;
	int32_t wl_y;
	int32_t wl_w;
	int32_t wl_h;
	int32_t wl_refresh_mhz;
	int32_t phys_w;
	int32_t phys_h;
	int32_t transform;
	int32_t wl_scale;
	bool has_logical;
	int32_t logical_x;
	int32_t logical_y;
	int32_t logical_w;
	int32_t logical_h;
	struct mode_info modes[MAX_MODES];
	uint32_t mode_count;
	bool has_frame;
	struct tag_state tags[MAX_TAGS];
};

struct app_state {
	struct wl_display *display;
	struct wl_registry *registry;
	struct zdwl_ipc_manager_v2 *ipc_manager;
	struct zxdg_output_manager_v1 *xdg_output_mgr;
	uint32_t tag_count;
	char layouts[MAX_LAYOUTS][64];
	uint32_t layout_count;
	struct output_info outputs[MAX_OUTPUTS];
	bool ready;
	bool watch_mode;
	bool dirty;
	volatile sig_atomic_t stop;
};

static struct app_state g_state;

static void handle_signal(int sig) {
	(void)sig;
	g_state.stop = 1;
}

static void print_json_escaped(FILE *out, const char *s) {
	fputc('"', out);
	if (s) {
		for (const unsigned char *p = (const unsigned char *)s; *p; p++) {
			switch (*p) {
			case '"':  fputs("\\\"", out); break;
			case '\\': fputs("\\\\", out); break;
			case '\b': fputs("\\b", out); break;
			case '\f': fputs("\\f", out); break;
			case '\n': fputs("\\n", out); break;
			case '\r': fputs("\\r", out); break;
			case '\t': fputs("\\t", out); break;
			default:
				if (*p < 0x20) {
					fprintf(out, "\\u%04x", *p);
				} else {
					fputc(*p, out);
				}
				break;
			}
		}
	}
	fputc('"', out);
}

static const struct output_info *find_focused_output(const struct app_state *st) {
	const struct output_info *first = NULL;
	for (int i = 0; i < MAX_OUTPUTS; i++) {
		if (!st->outputs[i].used) continue;
		if (!first) first = &st->outputs[i];
		if (st->outputs[i].active) return &st->outputs[i];
	}
	return first;
}

static struct output_info *find_target_output(struct app_state *st, const char *target) {
	if (!target || strcmp(target, "focused") == 0 || strcmp(target, "-") == 0) {
		struct output_info *first = NULL;
		for (int i = 0; i < MAX_OUTPUTS; i++) {
			if (!st->outputs[i].used) continue;
			if (!first) first = &st->outputs[i];
			if (st->outputs[i].active) return &st->outputs[i];
		}
		return first;
	}
	for (int i = 0; i < MAX_OUTPUTS; i++) {
		if (!st->outputs[i].used) continue;
		if (strcmp(st->outputs[i].name, target) == 0) return &st->outputs[i];
	}
	return NULL;
}

static void emit_state_json(const struct app_state *st) {
	const struct output_info *focused = find_focused_output(st);
	printf("{\"tagCount\":%u,\"focusedOutput\":", st->tag_count);
	print_json_escaped(stdout, focused ? focused->name : "");
	printf(",\"layouts\":[");
	for (uint32_t i = 0; i < st->layout_count; i++) {
		if (i > 0) putchar(',');
		print_json_escaped(stdout, st->layouts[i]);
	}
	printf("],\"outputs\":[");
	bool first_out = true;
	uint32_t tcount = st->tag_count > MAX_TAGS ? MAX_TAGS : st->tag_count;
	for (int i = 0; i < MAX_OUTPUTS; i++) {
		const struct output_info *o = &st->outputs[i];
		if (!o->used) continue;
		if (!first_out) putchar(',');
		first_out = false;
		printf("{\"name\":");
		print_json_escaped(stdout, o->name);
		printf(",\"active\":%s,\"layoutIndex\":%u,\"layoutSymbol\":",
		       o->active ? "true" : "false", o->layout_idx);
		print_json_escaped(stdout, o->layout_symbol);
		printf(",\"title\":");
		print_json_escaped(stdout, o->title);
		printf(",\"appId\":");
		print_json_escaped(stdout, o->appid);
		printf(",\"fullscreen\":%s,\"floating\":%s",
		       o->fullscreen ? "true" : "false",
		       o->floating ? "true" : "false");
		printf(",\"geometry\":{\"x\":%d,\"y\":%d,\"width\":%d,\"height\":%d}",
		       o->geom_x, o->geom_y, o->geom_w, o->geom_h);
		printf(",\"outputGeometry\":{\"x\":%d,\"y\":%d,\"width\":%d,\"height\":%d,\"scale\":%d}",
		       o->wl_x, o->wl_y, o->wl_w, o->wl_h, o->wl_scale > 0 ? o->wl_scale : 1);
		printf(",\"tags\":[");
		for (uint32_t t = 0; t < tcount; t++) {
			const struct tag_state *ts = &o->tags[t];
			if (t > 0) putchar(',');
			printf("{\"id\":%u,\"index\":%u,\"mask\":%u,\"active\":%s,\"urgent\":%s,\"focused\":%s,\"clients\":%u}",
			       t + 1, t, (1U << t),
			       (ts->state & ZDWL_IPC_OUTPUT_V2_TAG_STATE_ACTIVE) ? "true" : "false",
			       (ts->state & ZDWL_IPC_OUTPUT_V2_TAG_STATE_URGENT) ? "true" : "false",
			       ts->focused ? "true" : "false",
			       ts->clients);
		}
		printf("]}");
	}
	printf("]}\n");
	fflush(stdout);
}

static const char *transform_to_str(int32_t t) {
	switch (t) {
	case WL_OUTPUT_TRANSFORM_90:          return "90";
	case WL_OUTPUT_TRANSFORM_180:         return "180";
	case WL_OUTPUT_TRANSFORM_270:         return "270";
	case WL_OUTPUT_TRANSFORM_FLIPPED:     return "flipped";
	case WL_OUTPUT_TRANSFORM_FLIPPED_90:  return "flipped-90";
	case WL_OUTPUT_TRANSFORM_FLIPPED_180: return "flipped-180";
	case WL_OUTPUT_TRANSFORM_FLIPPED_270: return "flipped-270";
	default:                              return "normal";
	}
}

static double compute_output_scale(const struct output_info *o) {
	double s = o->wl_scale > 0 ? (double)o->wl_scale : 1.0;
	if (o->has_logical && o->logical_w > 0 && o->wl_w > 0 && o->wl_h > 0) {
		bool rotated = (o->transform == WL_OUTPUT_TRANSFORM_90 ||
		                o->transform == WL_OUTPUT_TRANSFORM_270 ||
		                o->transform == WL_OUTPUT_TRANSFORM_FLIPPED_90 ||
		                o->transform == WL_OUTPUT_TRANSFORM_FLIPPED_270);
		int32_t phys_logical_w = rotated ? o->wl_h : o->wl_w;
		double computed = (double)phys_logical_w / (double)o->logical_w;
		if (computed >= 0.25 && computed <= 8.0) {
			s = computed;
		}
	}
	return s;
}

static void emit_monitors_json(const struct app_state *st) {
	putchar('[');
	bool first = true;
	for (int i = 0; i < MAX_OUTPUTS; i++) {
		const struct output_info *o = &st->outputs[i];
		if (!o->used) continue;
		if (!first) putchar(',');
		first = false;
		int32_t pos_x = o->has_logical ? o->logical_x : o->wl_x;
		int32_t pos_y = o->has_logical ? o->logical_y : o->wl_y;
		double scale = compute_output_scale(o);
		int32_t log_w = (o->has_logical && o->logical_w > 0) ? o->logical_w : o->wl_w;
		int32_t log_h = (o->has_logical && o->logical_h > 0) ? o->logical_h : o->wl_h;

		printf("{\"name\":");
		print_json_escaped(stdout, o->name);
		printf(",\"description\":");
		print_json_escaped(stdout, o->description);
		printf(",\"make\":");
		print_json_escaped(stdout, o->make);
		printf(",\"model\":");
		print_json_escaped(stdout, o->model);
		printf(",\"physical_size\":{\"width\":%d,\"height\":%d}", o->phys_w, o->phys_h);
		printf(",\"enabled\":true");
		printf(",\"modes\":[");
		for (uint32_t m = 0; m < o->mode_count; m++) {
			const struct mode_info *mi = &o->modes[m];
			if (m > 0) putchar(',');
			printf("{\"width\":%d,\"height\":%d,\"refresh\":%.6f,\"preferred\":%s,\"current\":%s}",
			       mi->width, mi->height,
			       mi->refresh_mhz > 0 ? (double)mi->refresh_mhz / 1000.0 : 60.0,
			       mi->preferred ? "true" : "false",
			       mi->current ? "true" : "false");
		}
		printf("]");
		printf(",\"position\":{\"x\":%d,\"y\":%d}", pos_x, pos_y);
		printf(",\"logical_size\":{\"width\":%d,\"height\":%d}", log_w, log_h);
		printf(",\"transform\":");
		print_json_escaped(stdout, transform_to_str(o->transform));
		printf(",\"scale\":%.4f}", scale);
	}
	printf("]\n");
	fflush(stdout);
}

static void emit_active_window_json(const struct app_state *st) {
	const struct output_info *o = find_focused_output(st);
	if (!o) {
		printf("{\"output\":\"\",\"title\":\"\",\"appId\":\"\",\"fullscreen\":false,\"floating\":false,\"geometry\":{\"x\":0,\"y\":0,\"width\":0,\"height\":0}}\n");
		fflush(stdout);
		return;
	}
	printf("{\"output\":");
	print_json_escaped(stdout, o->name);
	printf(",\"title\":");
	print_json_escaped(stdout, o->title);
	printf(",\"appId\":");
	print_json_escaped(stdout, o->appid);
	printf(",\"fullscreen\":%s,\"floating\":%s",
	       o->fullscreen ? "true" : "false",
	       o->floating ? "true" : "false");
	printf(",\"geometry\":{\"x\":%d,\"y\":%d,\"width\":%d,\"height\":%d}}\n",
	       o->geom_x, o->geom_y, o->geom_w, o->geom_h);
	fflush(stdout);
}

/* zdwl_ipc_output_v2 listeners */
static void dwl_out_toggle_visibility(void *data, struct zdwl_ipc_output_v2 *out) {
	(void)data;
	(void)out;
}

static void dwl_out_active(void *data, struct zdwl_ipc_output_v2 *out, uint32_t active) {
	(void)out;
	struct output_info *o = data;
	o->active = (active != 0);
	g_state.dirty = true;
}

static void dwl_out_tag(void *data, struct zdwl_ipc_output_v2 *out,
                        uint32_t tag, uint32_t state, uint32_t clients, uint32_t focused) {
	(void)out;
	struct output_info *o = data;
	if (tag < MAX_TAGS) {
		o->tags[tag].state = state;
		o->tags[tag].clients = clients;
		o->tags[tag].focused = focused;
		g_state.dirty = true;
	}
}

static void dwl_out_layout(void *data, struct zdwl_ipc_output_v2 *out, uint32_t layout) {
	(void)out;
	struct output_info *o = data;
	o->layout_idx = layout;
	g_state.dirty = true;
}

static void dwl_out_title(void *data, struct zdwl_ipc_output_v2 *out, const char *title) {
	(void)out;
	struct output_info *o = data;
	snprintf(o->title, sizeof(o->title), "%s", title ? title : "");
	g_state.dirty = true;
}

static void dwl_out_appid(void *data, struct zdwl_ipc_output_v2 *out, const char *appid) {
	(void)out;
	struct output_info *o = data;
	snprintf(o->appid, sizeof(o->appid), "%s", appid ? appid : "");
	g_state.dirty = true;
}

static void dwl_out_layout_symbol(void *data, struct zdwl_ipc_output_v2 *out, const char *layout) {
	(void)out;
	struct output_info *o = data;
	snprintf(o->layout_symbol, sizeof(o->layout_symbol), "%s", layout ? layout : "");
	g_state.dirty = true;
}

static void dwl_out_frame(void *data, struct zdwl_ipc_output_v2 *out) {
	(void)out;
	struct output_info *o = data;
	o->has_frame = true;
	if (g_state.ready && g_state.watch_mode && g_state.dirty) {
		emit_state_json(&g_state);
		g_state.dirty = false;
	}
}

static void dwl_out_fullscreen(void *data, struct zdwl_ipc_output_v2 *out, uint32_t is_fullscreen) {
	(void)out;
	struct output_info *o = data;
	o->fullscreen = (is_fullscreen != 0);
	g_state.dirty = true;
}

static void dwl_out_floating(void *data, struct zdwl_ipc_output_v2 *out, uint32_t is_floating) {
	(void)out;
	struct output_info *o = data;
	o->floating = (is_floating != 0);
	g_state.dirty = true;
}

static void dwl_out_focused_geometry(void *data, struct zdwl_ipc_output_v2 *out,
                                     int32_t x, int32_t y, int32_t width, int32_t height) {
	(void)out;
	struct output_info *o = data;
	o->geom_x = x;
	o->geom_y = y;
	o->geom_w = width;
	o->geom_h = height;
	g_state.dirty = true;
}

static const struct zdwl_ipc_output_v2_listener dwl_output_listener = {
	.toggle_visibility = dwl_out_toggle_visibility,
	.active = dwl_out_active,
	.tag = dwl_out_tag,
	.layout = dwl_out_layout,
	.title = dwl_out_title,
	.appid = dwl_out_appid,
	.layout_symbol = dwl_out_layout_symbol,
	.frame = dwl_out_frame,
	.fullscreen = dwl_out_fullscreen,
	.floating = dwl_out_floating,
	.focused_geometry = dwl_out_focused_geometry,
};

/* wl_output listeners */
static void wl_out_geometry(void *data, struct wl_output *wl_output,
                            int32_t x, int32_t y, int32_t phys_w, int32_t phys_h,
                            int32_t subpixel, const char *make, const char *model,
                            int32_t transform) {
	(void)wl_output;
	(void)subpixel;
	struct output_info *o = data;
	o->wl_x = x;
	o->wl_y = y;
	o->phys_w = phys_w;
	o->phys_h = phys_h;
	o->transform = transform;
	snprintf(o->make, sizeof(o->make), "%s", make ? make : "");
	snprintf(o->model, sizeof(o->model), "%s", model ? model : "");
}

static void wl_out_mode(void *data, struct wl_output *wl_output, uint32_t flags,
                        int32_t width, int32_t height, int32_t refresh) {
	(void)wl_output;
	struct output_info *o = data;
	bool is_cur = (flags & WL_OUTPUT_MODE_CURRENT) != 0;
	bool is_pref = (flags & WL_OUTPUT_MODE_PREFERRED) != 0;
	if (is_cur) {
		o->wl_w = width;
		o->wl_h = height;
		o->wl_refresh_mhz = refresh;
	}
	for (uint32_t i = 0; i < o->mode_count; i++) {
		if (o->modes[i].width == width && o->modes[i].height == height &&
		    o->modes[i].refresh_mhz == refresh) {
			if (is_cur) o->modes[i].current = true;
			if (is_pref) o->modes[i].preferred = true;
			return;
		}
	}
	if (o->mode_count < MAX_MODES) {
		o->modes[o->mode_count++] = (struct mode_info){
			.width = width,
			.height = height,
			.refresh_mhz = refresh,
			.preferred = is_pref,
			.current = is_cur,
		};
	}
}

static void wl_out_done(void *data, struct wl_output *wl_output) {
	(void)data;
	(void)wl_output;
}

static void wl_out_scale(void *data, struct wl_output *wl_output, int32_t factor) {
	(void)wl_output;
	struct output_info *o = data;
	o->wl_scale = factor;
}

static void wl_out_name(void *data, struct wl_output *wl_output, const char *name) {
	(void)wl_output;
	struct output_info *o = data;
	snprintf(o->name, sizeof(o->name), "%s", name ? name : "");
	g_state.dirty = true;
}

static void wl_out_description(void *data, struct wl_output *wl_output, const char *desc) {
	(void)wl_output;
	struct output_info *o = data;
	snprintf(o->description, sizeof(o->description), "%s", desc ? desc : "");
}

static const struct wl_output_listener wl_output_listener = {
	.geometry = wl_out_geometry,
	.mode = wl_out_mode,
	.done = wl_out_done,
	.scale = wl_out_scale,
	.name = wl_out_name,
	.description = wl_out_description,
};

/* zxdg_output_v1 listeners */
static void xdg_out_logical_position(void *data, struct zxdg_output_v1 *xdg_output, int32_t x, int32_t y) {
	(void)xdg_output;
	struct output_info *o = data;
	o->has_logical = true;
	o->logical_x = x;
	o->logical_y = y;
}

static void xdg_out_logical_size(void *data, struct zxdg_output_v1 *xdg_output, int32_t w, int32_t h) {
	(void)xdg_output;
	struct output_info *o = data;
	o->has_logical = true;
	o->logical_w = w;
	o->logical_h = h;
}

static void xdg_out_done(void *data, struct zxdg_output_v1 *xdg_output) {
	(void)data;
	(void)xdg_output;
}

static void xdg_out_name(void *data, struct zxdg_output_v1 *xdg_output, const char *name) {
	(void)xdg_output;
	struct output_info *o = data;
	if (name && *name) snprintf(o->name, sizeof(o->name), "%s", name);
}

static void xdg_out_description(void *data, struct zxdg_output_v1 *xdg_output, const char *desc) {
	(void)xdg_output;
	struct output_info *o = data;
	if (desc && *desc) snprintf(o->description, sizeof(o->description), "%s", desc);
}

static const struct zxdg_output_v1_listener xdg_output_listener = {
	.logical_position = xdg_out_logical_position,
	.logical_size = xdg_out_logical_size,
	.done = xdg_out_done,
	.name = xdg_out_name,
	.description = xdg_out_description,
};

/* zdwl_ipc_manager_v2 listeners */
static void dwl_mgr_tags(void *data, struct zdwl_ipc_manager_v2 *mgr, uint32_t amount) {
	(void)data;
	(void)mgr;
	g_state.tag_count = amount > MAX_TAGS ? MAX_TAGS : amount;
	g_state.dirty = true;
}

static void dwl_mgr_layout(void *data, struct zdwl_ipc_manager_v2 *mgr, const char *name) {
	(void)data;
	(void)mgr;
	if (g_state.layout_count < MAX_LAYOUTS) {
		snprintf(g_state.layouts[g_state.layout_count],
		         sizeof(g_state.layouts[0]), "%s", name ? name : "");
		g_state.layout_count++;
		g_state.dirty = true;
	}
}

static const struct zdwl_ipc_manager_v2_listener dwl_mgr_listener = {
	.tags = dwl_mgr_tags,
	.layout = dwl_mgr_layout,
};

static void bind_ipc_for_output(struct output_info *o) {
	if (!g_state.ipc_manager || !o->wl_output || o->ipc_output) return;
	o->ipc_output = zdwl_ipc_manager_v2_get_output(g_state.ipc_manager, o->wl_output);
	if (o->ipc_output) {
		zdwl_ipc_output_v2_add_listener(o->ipc_output, &dwl_output_listener, o);
	}
}

static void bind_xdg_for_output(struct output_info *o) {
	if (!g_state.xdg_output_mgr || !o->wl_output || o->xdg_output) return;
	o->xdg_output = zxdg_output_manager_v1_get_xdg_output(g_state.xdg_output_mgr, o->wl_output);
	if (o->xdg_output) {
		zxdg_output_v1_add_listener(o->xdg_output, &xdg_output_listener, o);
	}
}

static void registry_global(void *data, struct wl_registry *registry,
                            uint32_t name, const char *interface, uint32_t version) {
	(void)data;
	if (strcmp(interface, wl_output_interface.name) == 0) {
		for (int i = 0; i < MAX_OUTPUTS; i++) {
			if (!g_state.outputs[i].used) {
				struct output_info *o = &g_state.outputs[i];
				memset(o, 0, sizeof(*o));
				o->used = true;
				o->registry_name = name;
				o->wl_scale = 1;
				snprintf(o->name, sizeof(o->name), "OUT-%u", name);
				uint32_t v = version < 4 ? version : 4;
				o->wl_output = wl_registry_bind(registry, name, &wl_output_interface, v);
				wl_output_add_listener(o->wl_output, &wl_output_listener, o);
				bind_xdg_for_output(o);
				bind_ipc_for_output(o);
				break;
			}
		}
	} else if (strcmp(interface, zxdg_output_manager_v1_interface.name) == 0) {
		uint32_t v = version < 3 ? version : 3;
		g_state.xdg_output_mgr = wl_registry_bind(registry, name, &zxdg_output_manager_v1_interface, v);
		for (int i = 0; i < MAX_OUTPUTS; i++) {
			if (g_state.outputs[i].used) bind_xdg_for_output(&g_state.outputs[i]);
		}
	} else if (strcmp(interface, zdwl_ipc_manager_v2_interface.name) == 0) {
		uint32_t v = version < 2 ? version : 2;
		g_state.ipc_manager = wl_registry_bind(registry, name, &zdwl_ipc_manager_v2_interface, v);
		zdwl_ipc_manager_v2_add_listener(g_state.ipc_manager, &dwl_mgr_listener, &g_state);
		for (int i = 0; i < MAX_OUTPUTS; i++) {
			if (g_state.outputs[i].used) bind_ipc_for_output(&g_state.outputs[i]);
		}
	}
}

static void registry_global_remove(void *data, struct wl_registry *registry, uint32_t name) {
	(void)data;
	(void)registry;
	for (int i = 0; i < MAX_OUTPUTS; i++) {
		struct output_info *o = &g_state.outputs[i];
		if (o->used && o->registry_name == name) {
			if (o->ipc_output) zdwl_ipc_output_v2_destroy(o->ipc_output);
			if (o->xdg_output) zxdg_output_v1_destroy(o->xdg_output);
			if (o->wl_output) wl_output_destroy(o->wl_output);
			memset(o, 0, sizeof(*o));
			g_state.dirty = true;
			if (g_state.ready && g_state.watch_mode) {
				emit_state_json(&g_state);
				g_state.dirty = false;
			}
			break;
		}
	}
}

static const struct wl_registry_listener registry_listener = {
	.global = registry_global,
	.global_remove = registry_global_remove,
};

static bool parse_u32(const char *s, uint32_t *out) {
	if (!s || !*s) return false;
	char *end = NULL;
	errno = 0;
	unsigned long val = strtoul(s, &end, 0);
	if (errno != 0 || !end || *end != '\0' || val > UINT32_MAX) return false;
	*out = (uint32_t)val;
	return true;
}

static void usage(const char *prog) {
	fprintf(stderr,
	        "Usage:\n"
	        "  %s [--once]                         Print dwl IPC state JSON once\n"
	        "  %s --watch                          Stream dwl IPC state JSON lines\n"
	        "  %s --monitors                       Print Wayland output/mode JSON\n"
	        "  %s active-window                    Print focused window JSON\n"
	        "  %s set-tags <output|focused> <mask> [toggle]\n"
	        "  %s view-tag <output|focused> <1..N> [toggle]\n"
	        "  %s set-client-tags <output|focused> <and_tags> <xor_tags>\n"
	        "  %s set-layout <output|focused> <index>\n"
	        "  %s cycle-layout <output|focused> [delta]\n",
	        prog, prog, prog, prog, prog, prog, prog, prog, prog);
}

int main(int argc, char **argv) {
	const char *cmd = argc > 1 ? argv[1] : "--once";

	if (strcmp(cmd, "-h") == 0 || strcmp(cmd, "--help") == 0) {
		usage(argv[0]);
		return 0;
	}

	signal(SIGINT, handle_signal);
	signal(SIGTERM, handle_signal);
	signal(SIGPIPE, handle_signal);

	g_state.display = wl_display_connect(NULL);
	if (!g_state.display) {
		fprintf(stderr, "quattro-dwl-ipc: failed to connect to Wayland display\n");
		return 1;
	}

	g_state.registry = wl_display_get_registry(g_state.display);
	wl_registry_add_listener(g_state.registry, &registry_listener, &g_state);

	/* Roundtrip 1: discover globals; Roundtrip 2/3: receive output/xdg/ipc initial state. */
	wl_display_roundtrip(g_state.display);
	wl_display_roundtrip(g_state.display);
	if (g_state.xdg_output_mgr) wl_display_roundtrip(g_state.display);

	bool monitors_only = (strcmp(cmd, "--monitors") == 0);
	if (!monitors_only && !g_state.ipc_manager) {
		fprintf(stderr, "quattro-dwl-ipc: compositor does not expose zdwl_ipc_manager_v2\n");
		wl_display_disconnect(g_state.display);
		return 2;
	}

	g_state.ready = true;

	int rc = 0;

	if (monitors_only) {
		emit_monitors_json(&g_state);
	} else if (strcmp(cmd, "--once") == 0) {
		emit_state_json(&g_state);
	} else if (strcmp(cmd, "--watch") == 0) {
		g_state.watch_mode = true;
		emit_state_json(&g_state);
		g_state.dirty = false;

		int fd = wl_display_get_fd(g_state.display);
		while (!g_state.stop) {
			while (wl_display_prepare_read(g_state.display) != 0) {
				if (wl_display_dispatch_pending(g_state.display) < 0) {
					g_state.stop = 1;
					break;
				}
			}
			if (g_state.stop) {
				wl_display_cancel_read(g_state.display);
				break;
			}
			if (wl_display_flush(g_state.display) < 0 && errno != EAGAIN) {
				wl_display_cancel_read(g_state.display);
				break;
			}
			struct pollfd pfd = { .fd = fd, .events = POLLIN };
			int pr = poll(&pfd, 1, -1);
			if (pr < 0) {
				wl_display_cancel_read(g_state.display);
				if (errno == EINTR) continue;
				rc = 1;
				break;
			}
			if (pfd.revents & (POLLERR | POLLHUP | POLLNVAL)) {
				wl_display_cancel_read(g_state.display);
				break;
			}
			if (pfd.revents & POLLIN) {
				if (wl_display_read_events(g_state.display) < 0) {
					rc = 1;
					break;
				}
				if (wl_display_dispatch_pending(g_state.display) < 0) {
					rc = 1;
					break;
				}
			} else {
				wl_display_cancel_read(g_state.display);
			}
		}
	} else if (strcmp(cmd, "active-window") == 0) {
		emit_active_window_json(&g_state);
	} else if (strcmp(cmd, "set-tags") == 0) {
		if (argc < 4) {
			usage(argv[0]);
			rc = 1;
		} else {
			struct output_info *o = find_target_output(&g_state, argv[2]);
			uint32_t mask = 0, toggle = 0;
			if (!o || !o->ipc_output || !parse_u32(argv[3], &mask) ||
			    (argc >= 5 && !parse_u32(argv[4], &toggle))) {
				fprintf(stderr, "quattro-dwl-ipc: invalid target or tag mask\n");
				rc = 1;
			} else {
				zdwl_ipc_output_v2_set_tags(o->ipc_output, mask, toggle);
				wl_display_roundtrip(g_state.display);
			}
		}
	} else if (strcmp(cmd, "view-tag") == 0) {
		if (argc < 4) {
			usage(argv[0]);
			rc = 1;
		} else {
			struct output_info *o = find_target_output(&g_state, argv[2]);
			uint32_t idx = 0, toggle = 0;
			if (!o || !o->ipc_output || !parse_u32(argv[3], &idx) ||
			    idx < 1 || idx > MAX_TAGS ||
			    (argc >= 5 && !parse_u32(argv[4], &toggle))) {
				fprintf(stderr, "quattro-dwl-ipc: invalid output or tag index\n");
				rc = 1;
			} else {
				zdwl_ipc_output_v2_set_tags(o->ipc_output, 1U << (idx - 1), toggle);
				wl_display_roundtrip(g_state.display);
			}
		}
	} else if (strcmp(cmd, "set-client-tags") == 0) {
		if (argc < 5) {
			usage(argv[0]);
			rc = 1;
		} else {
			struct output_info *o = find_target_output(&g_state, argv[2]);
			uint32_t and_tags = 0, xor_tags = 0;
			if (!o || !o->ipc_output || !parse_u32(argv[3], &and_tags) ||
			    !parse_u32(argv[4], &xor_tags)) {
				fprintf(stderr, "quattro-dwl-ipc: invalid target or client tag masks\n");
				rc = 1;
			} else {
				zdwl_ipc_output_v2_set_client_tags(o->ipc_output, and_tags, xor_tags);
				wl_display_roundtrip(g_state.display);
			}
		}
	} else if (strcmp(cmd, "set-layout") == 0) {
		if (argc < 4) {
			usage(argv[0]);
			rc = 1;
		} else {
			struct output_info *o = find_target_output(&g_state, argv[2]);
			uint32_t idx = 0;
			if (!o || !o->ipc_output || !parse_u32(argv[3], &idx)) {
				fprintf(stderr, "quattro-dwl-ipc: invalid target or layout index\n");
				rc = 1;
			} else {
				zdwl_ipc_output_v2_set_layout(o->ipc_output, idx);
				wl_display_roundtrip(g_state.display);
			}
		}
	} else if (strcmp(cmd, "cycle-layout") == 0) {
		if (argc < 3) {
			usage(argv[0]);
			rc = 1;
		} else {
			struct output_info *o = find_target_output(&g_state, argv[2]);
			long delta = argc >= 4 ? strtol(argv[3], NULL, 10) : 1;
			if (!o || !o->ipc_output || g_state.layout_count == 0) {
				fprintf(stderr, "quattro-dwl-ipc: invalid output or no layouts\n");
				rc = 1;
			} else {
				int32_t count = (int32_t)g_state.layout_count;
				int32_t next = ((int32_t)o->layout_idx + (int32_t)delta) % count;
				if (next < 0) next += count;
				zdwl_ipc_output_v2_set_layout(o->ipc_output, (uint32_t)next);
				wl_display_roundtrip(g_state.display);
			}
		}
	} else {
		usage(argv[0]);
		rc = 1;
	}

	for (int i = 0; i < MAX_OUTPUTS; i++) {
		if (g_state.outputs[i].used) {
			if (g_state.outputs[i].ipc_output)
				zdwl_ipc_output_v2_destroy(g_state.outputs[i].ipc_output);
			if (g_state.outputs[i].xdg_output)
				zxdg_output_v1_destroy(g_state.outputs[i].xdg_output);
			if (g_state.outputs[i].wl_output)
				wl_output_destroy(g_state.outputs[i].wl_output);
		}
	}
	if (g_state.xdg_output_mgr) zxdg_output_manager_v1_destroy(g_state.xdg_output_mgr);
	if (g_state.ipc_manager) zdwl_ipc_manager_v2_destroy(g_state.ipc_manager);
	if (g_state.registry) wl_registry_destroy(g_state.registry);
	wl_display_disconnect(g_state.display);
	return rc;
}
