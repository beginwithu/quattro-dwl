/* Taken from https://github.com/djpohly/dwl/issues/466 */
#define COLOR(hex)    { ((hex >> 24) & 0xFF) / 255.0f, \
                        ((hex >> 16) & 0xFF) / 255.0f, \
                        ((hex >> 8) & 0xFF) / 255.0f, \
                        (hex & 0xFF) / 255.0f }

/* appearance */
static int sloppyfocus               = 1;  /* focus follows mouse */
static int bypass_surface_visibility = 0;  /* 1 means idle inhibitors will disable idle tracking even if it's surface isn't visible */
static int smartgaps                 = 0;  /* 1 means no outer gap when there is only one window */
static int monoclegaps               = 0;  /* 1 means outer gaps in monocle layout */
static unsigned int borderpx         = 2;  /* border pixel of windows */
static unsigned int gappih           = 4;  /* horiz inner gap between windows */
static unsigned int gappiv           = 4;  /* vert inner gap between windows */
static unsigned int gappoh           = 4;  /* horiz outer gap between windows and screen edge */
static unsigned int gappov           = 4;  /* vert outer gap between windows and screen edge */
static unsigned int snap             = 32; /* snap pixel */
static int adaptive_sync             = 0;  /* 1 = enable VRR (FreeSync/G-Sync) if the monitor supports it */
static float rootcolor[]             = COLOR(0x1a1b26ff);
static float bordercolor[]           = COLOR(0x414868ff);
static float focuscolor[]            = COLOR(0x7aa2f7ff);
static float urgentcolor[]           = COLOR(0xf7768eff);
static float fullscreen_bg[]         = {0.0f, 0.0f, 0.0f, 1.0f};

/* tagging - TAGCOUNT must be no greater than 31 */
#define TAGCOUNT (10)

/* logging */
static int log_level = WLR_ERROR;

/* Autostart (handled by quattro-dwl-session & quattro-shell; no autostart.sh needed) */
static const char *const autostart[] = {
	NULL
};

static const Rule rules[] = {
	/* app_id                     title  tags mask  isfloating  monitor  width  height */
	{ "localsend",                NULL,  0,         1,          -1,      0,     0 },
	{ "org.gnome.baobab",         NULL,  0,         1,          -1,      0,     0 },
	{ "xdg-desktop-portal-gtk",   NULL,  0,         1,          -1,      0,     0 },
	{ "imv",                      NULL,  0,         1,          -1,      0,     0 },
	{ "mpv",                      NULL,  0,         1,          -1,      0,     0 },
	{ "WebcamOverlay-",           NULL,  0,         1,          -1,      0,     0 },
	{ "org.quattro.about",        NULL,  0,         1,          -1,      920,   520 },
	{ "org.quattro.terminal",     NULL,  0,         1,          -1,      875,   600 },
	{ "quattro-floating",         NULL,  0,         1,          -1,      875,   600 },
	{ "TUI.float",                NULL,  0,         1,          -1,      875,   600 },
};

/* layout(s) */
static const Layout layouts[] = {
	/* symbol     arrange function */
	{ "[]=",      dwindle },
	{ "><>",      NULL },    /* floating */
	{ "[]=",      tile },
	{ "[M]",      monocle },
};

/* monitors */
/* (x=-1, y=-1) is reserved as an "autoconfigure" monitor position indicator.
 * (resx=0, resy=0, refresh=0.0f) uses the monitor's preferred mode/refresh rate.
 * WARNING: negative values other than (-1, -1) cause problems with Xwayland clients due to
 * https://gitlab.freedesktop.org/xorg/xserver/-/issues/899 */
static const MonitorRule monrules[] = {
	/* name        mfact  nmaster  resx  resy  refresh  scale  layout       rotate/reflect              x   y */
	{ NULL,        0.55f, 1,       0,    0,    0.0f,    1.0f,  &layouts[0], WL_OUTPUT_TRANSFORM_NORMAL, -1, -1 },
};

/* keyboard */
static const struct xkb_rule_names xkb_rules = {
	.options = NULL,
};

static int repeat_rate = 35;
static int repeat_delay = 250;

/* Trackpad */
static int tap_to_click = 1;
static int tap_and_drag = 1;
static int drag_lock = 1;
static int natural_scrolling = 0;
static int disable_while_typing = 1;
static int left_handed = 0;
static int middle_button_emulation = 0;
static enum libinput_config_scroll_method scroll_method = LIBINPUT_CONFIG_SCROLL_2FG;
static enum libinput_config_click_method click_method = LIBINPUT_CONFIG_CLICK_METHOD_BUTTON_AREAS;
static uint32_t send_events_mode = LIBINPUT_CONFIG_SEND_EVENTS_ENABLED;
static enum libinput_config_accel_profile accel_profile = LIBINPUT_CONFIG_ACCEL_PROFILE_ADAPTIVE;
static double accel_speed = 0.0;
static enum libinput_config_tap_button_map button_map = LIBINPUT_CONFIG_TAP_MAP_LRM;

#define MODKEY WLR_MODIFIER_LOGO

#define TAGKEYS(KEY, SHIFTKEY, TAG) \
	{ MODKEY, KEY, view, {.ui = 1 << TAG} }, \
	{ MODKEY|WLR_MODIFIER_CTRL, KEY, toggleview, {.ui = 1 << TAG} }, \
	{ MODKEY|WLR_MODIFIER_SHIFT, KEY, tag, {.ui = 1 << TAG} }, \
	{ MODKEY|WLR_MODIFIER_CTRL|WLR_MODIFIER_SHIFT, KEY, toggletag, {.ui = 1 << TAG} }, \
	{ MODKEY|WLR_MODIFIER_SHIFT, SHIFTKEY, view, {.ui = 1 << TAG} }

#define SHCMD(cmd) { .v = (const char*[]){ "/bin/sh", "-c", cmd, NULL } }

static const char *termcmd[] = { "quattro-cmd-terminal-cwd", NULL };
static const char *menucmd[] = { "quattro-menu", NULL };
static const char *filecmd[] = { "quattro-launch-files", NULL };

static const Key keys[] = {
	/* modifier                                      key                        function          argument */
	/* Applications & Quattro Menus */
	{ MODKEY,                                        XKB_KEY_Return,            spawn,            {.v = termcmd} },
	{ MODKEY,                                        XKB_KEY_space,             spawn,            {.v = menucmd} },
	{ MODKEY|WLR_MODIFIER_ALT,                       XKB_KEY_space,             spawn,            {.v = menucmd} },
	{ MODKEY,                                        XKB_KEY_d,                 spawn,            {.v = menucmd} },
	{ MODKEY,                                        XKB_KEY_e,                 spawn,            {.v = filecmd} },
	{ MODKEY|WLR_MODIFIER_SHIFT,                     XKB_KEY_B,                 spawn,            SHCMD("quattro-launch-browser") },
	{ MODKEY|WLR_MODIFIER_SHIFT,                     XKB_KEY_M,                 spawn,            SHCMD("quattro-launch-tui btop") },
	{ MODKEY,                                        XKB_KEY_slash,             spawn,            SHCMD("quattro-menu-keybindings") },
	{ MODKEY,                                        XKB_KEY_v,                 spawn,            SHCMD("quattro-launch-clipboard") },
	{ MODKEY,                                        XKB_KEY_period,            spawn,            SHCMD("quattro-launch-emoji") },
	{ MODKEY,                                        XKB_KEY_Escape,            spawn,            SHCMD("quattro-menu system") },
	{ MODKEY|WLR_MODIFIER_CTRL,                      XKB_KEY_l,                 spawn,            SHCMD("quattro-system-lock") },
	{ MODKEY|WLR_MODIFIER_SHIFT,                     XKB_KEY_space,             spawn,            SHCMD("quattro-bar --toggle") },
	{ MODKEY|WLR_MODIFIER_CTRL,                      XKB_KEY_space,             spawn,            SHCMD("quattro-theme-bg-next") },
	{ MODKEY|WLR_MODIFIER_SHIFT|WLR_MODIFIER_CTRL,   XKB_KEY_space,             spawn,            SHCMD("quattro-menu theme") },
	{ MODKEY,                                        XKB_KEY_comma,             spawn,            SHCMD("quattro-shell notifications dismiss") },
	{ MODKEY|WLR_MODIFIER_SHIFT,                     XKB_KEY_comma,             spawn,            SHCMD("quattro-shell notifications dismiss-all") },
	{ MODKEY|WLR_MODIFIER_CTRL,                      XKB_KEY_comma,             spawn,            SHCMD("quattro-shell notifications toggle-dnd") },

	/* Capture & Media */
	{ 0,                                             XKB_KEY_Print,             spawn,            SHCMD("quattro-capture-screenshot") },
	{ WLR_MODIFIER_SHIFT,                            XKB_KEY_Print,             spawn,            SHCMD("quattro-capture-region") },
	{ WLR_MODIFIER_ALT,                              XKB_KEY_Print,             spawn,            SHCMD("quattro-capture-screenrecording") },
	{ MODKEY|WLR_MODIFIER_SHIFT,                     XKB_KEY_S,                 spawn,            SHCMD("quattro-capture-region") },
	{ MODKEY|WLR_MODIFIER_SHIFT,                     XKB_KEY_P,                 spawn,            SHCMD("quattro-color-picker") },

	/* Hardware Keys */
	{ 0,                                             XKB_KEY_XF86AudioRaiseVolume,  spawn,        SHCMD("quattro-audio-output-volume +5%") },
	{ 0,                                             XKB_KEY_XF86AudioLowerVolume,  spawn,        SHCMD("quattro-audio-output-volume -5%") },
	{ 0,                                             XKB_KEY_XF86AudioMute,         spawn,        SHCMD("quattro-audio-output-mute") },
	{ 0,                                             XKB_KEY_XF86AudioMicMute,      spawn,        SHCMD("quattro-audio-input-mute") },
	{ 0,                                             XKB_KEY_XF86MonBrightnessUp,   spawn,        SHCMD("quattro-brightness-display +5%") },
	{ 0,                                             XKB_KEY_XF86MonBrightnessDown, spawn,        SHCMD("quattro-brightness-display -5%") },
	{ 0,                                             XKB_KEY_XF86AudioPlay,         spawn,        SHCMD("playerctl play-pause") },
	{ 0,                                             XKB_KEY_XF86AudioNext,         spawn,        SHCMD("playerctl next") },
	{ 0,                                             XKB_KEY_XF86AudioPrev,         spawn,        SHCMD("playerctl previous") },

	/* Window & Layout Management */
	{ MODKEY,                                        XKB_KEY_Left,              focusdir,         {.ui = 0} },
	{ MODKEY,                                        XKB_KEY_Right,             focusdir,         {.ui = 1} },
	{ MODKEY,                                        XKB_KEY_Up,                focusdir,         {.ui = 2} },
	{ MODKEY,                                        XKB_KEY_Down,              focusdir,         {.ui = 3} },
	{ MODKEY,                                        XKB_KEY_h,                 focusdir,         {.ui = 0} },
	{ MODKEY,                                        XKB_KEY_l,                 focusdir,         {.ui = 1} },
	{ MODKEY,                                        XKB_KEY_k,                 focusdir,         {.ui = 2} },
	{ MODKEY,                                        XKB_KEY_j,                 focusdir,         {.ui = 3} },
	{ MODKEY|WLR_MODIFIER_SHIFT,                     XKB_KEY_Left,              swapdir,          {.ui = 0} },
	{ MODKEY|WLR_MODIFIER_SHIFT,                     XKB_KEY_Right,             swapdir,          {.ui = 1} },
	{ MODKEY|WLR_MODIFIER_SHIFT,                     XKB_KEY_Up,                swapdir,          {.ui = 2} },
	{ MODKEY|WLR_MODIFIER_SHIFT,                     XKB_KEY_Down,              swapdir,          {.ui = 3} },
	{ MODKEY|WLR_MODIFIER_SHIFT,                     XKB_KEY_H,                 swapdir,          {.ui = 0} },
	{ MODKEY|WLR_MODIFIER_SHIFT,                     XKB_KEY_L,                 swapdir,          {.ui = 1} },
	{ MODKEY|WLR_MODIFIER_SHIFT,                     XKB_KEY_K,                 swapdir,          {.ui = 2} },
	{ MODKEY|WLR_MODIFIER_SHIFT,                     XKB_KEY_J,                 swapdir,          {.ui = 3} },
	{ MODKEY,                                        XKB_KEY_Tab,               view,             {0} },
	{ MODKEY,                                        XKB_KEY_q,                 killclient,       {0} },
	{ MODKEY,                                        XKB_KEY_w,                 killclient,       {0} },
	{ MODKEY,                                        XKB_KEY_f,                 togglefullscreen, {0} },
	{ MODKEY|WLR_MODIFIER_SHIFT,                     XKB_KEY_F,                 togglefloating,   {0} },
	{ MODKEY,                                        XKB_KEY_t,                 togglefloating,   {0} },
	{ MODKEY,                                        XKB_KEY_m,                 setlayout,        {.v = &layouts[0]} },
	{ MODKEY,                                        XKB_KEY_n,                 setlayout,        {.v = &layouts[2]} },
	{ MODKEY,                                        XKB_KEY_b,                 setlayout,        {.v = &layouts[3]} },
	{ MODKEY,                                        XKB_KEY_0,                 view,             {.ui = ~0} },
	{ MODKEY|WLR_MODIFIER_SHIFT,                     XKB_KEY_parenright,        tag,              {.ui = ~0} },

	TAGKEYS(                                         XKB_KEY_1, XKB_KEY_exclam,                   0),
	TAGKEYS(                                         XKB_KEY_2, XKB_KEY_at,                       1),
	TAGKEYS(                                         XKB_KEY_3, XKB_KEY_numbersign,               2),
	TAGKEYS(                                         XKB_KEY_4, XKB_KEY_dollar,                   3),
	TAGKEYS(                                         XKB_KEY_5, XKB_KEY_percent,                  4),
	TAGKEYS(                                         XKB_KEY_6, XKB_KEY_asciicircum,              5),
	TAGKEYS(                                         XKB_KEY_7, XKB_KEY_ampersand,                6),
	TAGKEYS(                                         XKB_KEY_8, XKB_KEY_asterisk,                 7),
	TAGKEYS(                                         XKB_KEY_9, XKB_KEY_parenleft,                8),

	{ MODKEY|WLR_MODIFIER_SHIFT,                     XKB_KEY_E,                 quit,             {0} },
	{ WLR_MODIFIER_CTRL|WLR_MODIFIER_ALT,            XKB_KEY_BackSpace,         quit,             {0} },
#define CHVT(n) { WLR_MODIFIER_CTRL|WLR_MODIFIER_ALT, XKB_KEY_F##n, chvt, {.ui = (n)} }
	CHVT(1), CHVT(2), CHVT(3), CHVT(4), CHVT(5), CHVT(6),
	CHVT(7), CHVT(8), CHVT(9), CHVT(10), CHVT(11), CHVT(12),
};

static const Button buttons[] = {
	{ MODKEY, BTN_LEFT,   moveresize,     {.ui = CurMove} },
	{ MODKEY, BTN_MIDDLE, togglefloating, {0} },
	{ MODKEY, BTN_RIGHT,  moveresize,     {.ui = CurResize} },
};

static const Axis axes[] = {
	{ 0, 0, NULL, {0} },
};
