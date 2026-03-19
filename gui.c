/* OneOS-ARM GUI Module Implementation */

#include "gui.h"
#include "fb.h"
#include "graphics.h"
#include "keyboard.h"
#include "vfs.h"
#include "uart.h"
#include "string.h"
#include "virtio_input.h"

/* Screen dimensions */
#define SCREEN_W 1024
#define SCREEN_H 768

/* Taskbar */
#define TASKBAR_H 32
#define TASKBAR_COLOR   0xFF202040
#define TASKBAR_TEXT    0xFFFFFFFF

/* Desktop */
#define DESKTOP_COLOR   0xFF206080

/* Mouse cursor */
static int mouse_x = 512;
static int mouse_y = 384;
#define MOUSE_COLOR     0xFFFFFFFF
#define MOUSE_OUTLINE   0xFF000000
#define MOUSE_SHADOW    0xFF404040

/* Clock state */
static int clock_hour = 12;
static int clock_minute = 0;
static int clock_second = 0;
static unsigned long clock_last_timer = 0;

/* Start menu */
static int start_menu_open = 0;
#define MENU_X 4
#define MENU_Y (SCREEN_H - TASKBAR_H - 3 * 28 - 8)
#define MENU_W 160
#define MENU_ITEM_H 28
#define MENU_BG     0xFF303050
#define MENU_HOVER  0xFF506090
#define MENU_TEXT    0xFFFFFFFF
#define MENU_TEXT_SHADOW 0xFF101010
#define MENU_ITEMS  3

static const char *menu_labels[7] = {
    "File Manager",
    "Notepad",
    "Settings",
    "System Info",
    "Clock",
    "Paint",
    "Terminal"
};
#undef MENU_ITEMS
#define MENU_ITEMS 7

/* ---- Window management ---- */
#define WIN_MAX_TITLE 32
#define WIN_TITLEBAR_H 24
#define WIN_TITLEBAR_COLOR 0xFF304060
#define WIN_TITLEBAR_ACTIVE 0xFF4060A0
#define WIN_BG_COLOR   0xFFE0E0E0
#define WIN_TEXT_COLOR  0xFF000000
#define WIN_BORDER     0xFF808080

#define WIN_FILE_MGR 0
#define WIN_NOTEPAD  1
#define WIN_SETTINGS 2
#define WIN_SYSINFO  3
#define WIN_CLOCK    4
#define WIN_PAINT    5
#define WIN_TERMINAL 6

#define MAX_WINDOWS 7

typedef struct {
    int x, y, w, h;
    int old_x, old_y, old_w, old_h;
    int visible;
    int maximized;
    char title[WIN_MAX_TITLE];
} gui_window_t;

static gui_window_t windows[MAX_WINDOWS];
static int active_win = -1;
static int win_order[MAX_WINDOWS]; /* z-order: front = last */
static int win_count = MAX_WINDOWS;

/* File manager state */
static int fm_current_dir = 0;
static int fm_selected = -1;
static int fm_file_indices[64];
static int fm_file_count = 0;

/* Desktop state */
static int desktop_file_indices[32];
static int desktop_file_count = 0;

/* Notepad state */
#define NOTEPAD_BUF_SIZE 1024
static char notepad_buf[NOTEPAD_BUF_SIZE];
static int notepad_len = 0;
static int notepad_active = 0; /* 1 = notepad captures keyboard */
static char notepad_filename[WIN_MAX_TITLE];
static int notepad_dir = 0;

/* Paint state */
#define PAINT_W 320
#define PAINT_H 200
static unsigned int paint_canvas[PAINT_W * PAINT_H];
static unsigned int paint_color = 0xFF000000;
static int paint_brush = 1;

/* Terminal state */
#define TERM_BUF_SIZE 1024
#define TERM_PROMPT "OneOS> "
#define TERM_PROMPT_LEN 7
static char term_buf[TERM_BUF_SIZE];
static int term_len = 0;
static int term_active = 0;

/* Context Menu State */
static int context_menu_open = 0;
static int context_x = 0;
static int context_y = 0;
static int context_file_idx = -1;
#define CONTEXT_W 100
#define CONTEXT_H (3 * 28)
#define CONTEXT_BG 0xFFD0D0D0
#define CONTEXT_HOVER 0xFFA0C0E0
#define CONTEXT_TEXT 0xFF000000

/* Rename Dialog State */
static int rename_dialog_open = 0;
static int rename_file_idx = -1;
static char rename_buf[VFS_MAX_NAME];
static int rename_len = 0;

static const char *context_menu_items[3] = {"Open", "Rename", "Delete"};

/* GUI state */
static int gui_running = 0;
static unsigned int current_desktop_color = 0xFF206080;
static int transparency_enabled = 0;
static int show_seconds = 0;
static int mouse_speed = 1;
static int cursor_scale = 1;
static int gui_use_backbuffer = 0;
static int cursor_style = 0;
#define CURSOR_STYLE_COUNT 3
static const char *cursor_style_labels[CURSOR_STYLE_COUNT] = {
    "Arrow",
    "Cross",
    "I-Beam"
};

/* Cursor overlay (frontbuffer mode) */
#define CURSOR_MAX 34
static unsigned int cursor_under[CURSOR_MAX * CURSOR_MAX];
static int cursor_valid = 0;
static int cursor_old_x = 0;
static int cursor_old_y = 0;

/* Desktop icon positions */
#define ICON_W 64
#define ICON_H 56
#define ICON_TEXT_COLOR 0xFFFFFFFF

typedef struct {
    int x, y;
    const char *label;
    int win_id;
    unsigned int color;
} desktop_icon_t;

static desktop_icon_t icons[] = {
    { 20,  20, "Files",    WIN_FILE_MGR, 0xFFE0C060 },
    { 20, 100, "Notepad",  WIN_NOTEPAD,  0xFFFFFFFF },
    { 20, 180, "Settings", WIN_SETTINGS, 0xFF808090 },
    { 20, 260, "Clock",    WIN_CLOCK,    0xFF60A0C0 },
    { 20, 340, "Paint",    WIN_PAINT,    0xFFA0C060 },
    { 20, 420, "Terminal", WIN_TERMINAL, 0xFF60C090 },
};
#define ICON_COUNT 6

/* ---- Helper: copy string ---- */
static void str_copy(char *dst, const char *src, int max)
{
    int i = 0;
    while (src[i] && i < max - 1) { dst[i] = src[i]; i++; }
    dst[i] = '\0';
}

static void term_append_char(char c)
{
    if (term_len < TERM_BUF_SIZE - 1) {
        term_buf[term_len++] = c;
        term_buf[term_len] = '\0';
    }
}

static void term_append_str(const char *s)
{
    while (*s) {
        term_append_char(*s++);
    }
}

/* ---- Helper: int to 2-digit string ---- */
static void int_to_2digit(int val, char *out)
{
    if (val < 0) val = 0;
    if (val > 99) val = 99;
    out[0] = '0' + (val / 10);
    out[1] = '0' + (val % 10);
}

/* ---- Forward declarations ---- */
static void gui_draw_desktop(void);
static void gui_draw_taskbar(void);
static void gui_draw_mouse(void);
static void gui_draw_window(gui_window_t *win, int active);
static void gui_draw_file_manager(void);
static void gui_draw_notepad(void);
static void gui_draw_settings(void);
static void gui_draw_sysinfo(void);
static void gui_draw_clock(void);
static void gui_draw_paint(void);
static void gui_draw_terminal(void);
static void gui_draw_start_menu(void);
static void gui_draw_icons(void);
static void gui_sync_desktop(void);
static void gui_handle_mouse_move(int dx, int dy);
static void gui_handle_click(void);
static void gui_refresh_file_list(void);
static void gui_bring_to_front(int win_id);
static void gui_draw_mouse_overlay(void);

/* ---- Clock ---- */
void gui_set_time(int hour, int minute)
{
    clock_hour = hour % 24;
    clock_minute = minute % 60;
}

static unsigned long get_timer_count(void)
{
    unsigned long val;
    asm volatile("mrs %0, cntvct_el0" : "=r" (val));
    return val;
}

static unsigned long get_timer_freq(void)
{
    unsigned long val;
    asm volatile("mrs %0, cntfrq_el0" : "=r" (val));
    return val;
}

static void clock_advance(void)
{
    unsigned long now = get_timer_count();
    unsigned long freq = get_timer_freq();
    if (clock_last_timer == 0) {
        clock_last_timer = now;
        return;
    }
    
    if (now - clock_last_timer >= freq) {
        /* Advance 1 second */
        unsigned long elapsed_secs = (now - clock_last_timer) / freq;
        clock_last_timer += elapsed_secs * freq;
        
        clock_second += elapsed_secs;
        while (clock_second >= 60) {
            clock_second -= 60;
            clock_minute++;
            if (clock_minute >= 60) {
                clock_minute -= 60;
                clock_hour++;
                if (clock_hour >= 24) clock_hour = 0;
            }
        }
    }
}

/* ---- Init ---- */
void gui_init(void)
{
    mouse_x = SCREEN_W / 2;
    mouse_y = SCREEN_H / 2;
    fb_use_backbuffer(gui_use_backbuffer);

    /* File Manager */
    windows[WIN_FILE_MGR].x = 200;
    windows[WIN_FILE_MGR].y = 60;
    windows[WIN_FILE_MGR].w = 400;
    windows[WIN_FILE_MGR].h = 450;
    windows[WIN_FILE_MGR].visible = 1;
    windows[WIN_FILE_MGR].maximized = 0;
    str_copy(windows[WIN_FILE_MGR].title, "File Manager", WIN_MAX_TITLE);

    /* Notepad */
    windows[WIN_NOTEPAD].x = 260;
    windows[WIN_NOTEPAD].y = 80;
    windows[WIN_NOTEPAD].w = 440;
    windows[WIN_NOTEPAD].h = 400;
    windows[WIN_NOTEPAD].visible = 0;
    windows[WIN_NOTEPAD].maximized = 0;
    str_copy(windows[WIN_NOTEPAD].title, "Notepad", WIN_MAX_TITLE);

    /* Settings */
    windows[WIN_SETTINGS].x = 300;
    windows[WIN_SETTINGS].y = 120;
    windows[WIN_SETTINGS].w = 360;
    windows[WIN_SETTINGS].h = 500; /* Increased H for more options */
    windows[WIN_SETTINGS].visible = 0;
    windows[WIN_SETTINGS].maximized = 0;
    str_copy(windows[WIN_SETTINGS].title, "Settings", WIN_MAX_TITLE);

    /* System Info */
    windows[WIN_SYSINFO].x = 320;
    windows[WIN_SYSINFO].y = 140;
    windows[WIN_SYSINFO].w = 300;
    windows[WIN_SYSINFO].h = 240;
    windows[WIN_SYSINFO].visible = 0;
    windows[WIN_SYSINFO].maximized = 0;
    str_copy(windows[WIN_SYSINFO].title, "System Info", WIN_MAX_TITLE);

    /* Clock */
    windows[WIN_CLOCK].x = 420;
    windows[WIN_CLOCK].y = 180;
    windows[WIN_CLOCK].w = 240;
    windows[WIN_CLOCK].h = 180;
    windows[WIN_CLOCK].visible = 0;
    windows[WIN_CLOCK].maximized = 0;
    str_copy(windows[WIN_CLOCK].title, "Clock", WIN_MAX_TITLE);

    /* Paint */
    windows[WIN_PAINT].x = 180;
    windows[WIN_PAINT].y = 140;
    windows[WIN_PAINT].w = 380;
    windows[WIN_PAINT].h = 300;
    windows[WIN_PAINT].visible = 0;
    windows[WIN_PAINT].maximized = 0;
    str_copy(windows[WIN_PAINT].title, "Paint", WIN_MAX_TITLE);

    /* Terminal */
    windows[WIN_TERMINAL].x = 120;
    windows[WIN_TERMINAL].y = 200;
    windows[WIN_TERMINAL].w = 520;
    windows[WIN_TERMINAL].h = 280;
    windows[WIN_TERMINAL].visible = 0;
    windows[WIN_TERMINAL].maximized = 0;
    str_copy(windows[WIN_TERMINAL].title, "Terminal", WIN_MAX_TITLE);

    /* Z-order */
    win_order[0] = WIN_SYSINFO;
    win_order[1] = WIN_SETTINGS;
    win_order[2] = WIN_NOTEPAD;
    win_order[3] = WIN_PAINT;
    win_order[4] = WIN_CLOCK;
    win_order[5] = WIN_TERMINAL;
    win_order[6] = WIN_FILE_MGR;
    active_win = WIN_FILE_MGR;

    notepad_len = 0;
    notepad_buf[0] = '\0';
    str_copy(notepad_filename, "untitled.txt", WIN_MAX_TITLE);
    notepad_active = 0;
    term_len = 0;
    term_buf[0] = '\0';
    term_append_str(TERM_PROMPT);
    term_active = 0;
    start_menu_open = 0;

    fm_current_dir = vfs_find("Desktop", 0);
    if (fm_current_dir < 0) fm_current_dir = vfs_root();

    /* Init paint canvas to white */
    for (int i = 0; i < PAINT_W * PAINT_H; i++) {
        paint_canvas[i] = 0xFFFFFFFF;
    }
    
    gui_sync_desktop();
    gui_refresh_file_list();
    gui_running = 1;
}

static void gui_sync_desktop(void)
{
    int desktop = vfs_find("Desktop", 0);
    if (desktop >= 0) {
        desktop_file_count = vfs_list(desktop, desktop_file_indices, 32);
    } else {
        desktop_file_count = 0;
    }
}

static void gui_refresh_file_list(void)
{
    fm_file_count = vfs_list(fm_current_dir, fm_file_indices, 64);
    fm_selected = -1;
    gui_sync_desktop(); /* Keep desktop dynamic icons in sync */
}

/* ---- Bring window to front ---- */
static void gui_bring_to_front(int win_id)
{
    /* Remove from order and append at end (top) */
    int pos = -1;
    for (int i = 0; i < win_count; i++) {
        if (win_order[i] == win_id) { pos = i; break; }
    }
    if (pos >= 0) {
        for (int i = pos; i < win_count - 1; i++) {
            win_order[i] = win_order[i + 1];
        }
        win_order[win_count - 1] = win_id;
    }
    active_win = win_id;
    notepad_active = (win_id == WIN_NOTEPAD && windows[WIN_NOTEPAD].visible);
    term_active = (win_id == WIN_TERMINAL && windows[WIN_TERMINAL].visible);
}

/* ---- Drawing ---- */
static void gui_draw_desktop(void)
{
    unsigned char base_r = (current_desktop_color >> 16) & 0xFF;
    unsigned char base_g = (current_desktop_color >> 8) & 0xFF;
    unsigned char base_b = current_desktop_color & 0xFF;

    /* Gradient-style desktop: two-tone */
    for (int y = 0; y < SCREEN_H - TASKBAR_H; y++) {
        /* Darken slightly toward bottom */
        int r = (int)base_r - (y * 0x10 / SCREEN_H);
        int g = (int)base_g - (y * 0x20 / SCREEN_H);
        int b = (int)base_b - (y * 0x10 / SCREEN_H);
        if (r < 0) r = 0;
        if (g < 0) g = 0;
        if (b < 0) b = 0;
        unsigned int color = 0xFF000000 | ((unsigned int)r << 16) | ((unsigned int)g << 8) | (unsigned int)b;
        fb_draw_hline(0, y, SCREEN_W, color);
    }
}

static void gui_draw_icons(void)
{
    /* 1. Hardcoded app icons */
    for (int i = 0; i < ICON_COUNT; i++) {
        int ix = icons[i].x;
        int iy = icons[i].y;

        /* Icon background rectangle */
        fb_draw_rect(ix + 16, iy, 32, 32, icons[i].color);

        /* Small inner detail */
        if (i == 0) { /* Files: folder shape */
            fb_draw_rect(ix + 18, iy + 2, 12, 6, 0xFFC0A030);
            fb_draw_rect(ix + 18, iy + 6, 28, 22, 0xFFD0B050);
        } else if (i == 1) { /* Notepad: lined paper */
            fb_draw_rect(ix + 20, iy + 4, 24, 24, 0xFFF0F0F0);
            for (int ln = 0; ln < 4; ln++) {
                fb_draw_hline(ix + 22, iy + 8 + ln * 5, 20, 0xFFA0A0CC);
            }
        } else { /* Settings: gear-ish */
            fb_draw_rect(ix + 24, iy + 4, 16, 24, 0xFF606070);
            fb_draw_rect(ix + 20, iy + 10, 24, 12, 0xFF707080);
        }

        /* Label below icon */
        int lx = ix + 4;
        int ly = iy + 36;
        gfx_draw_string(lx + 1, ly + 1, icons[i].label, 0xFF000000, DESKTOP_COLOR);
        gfx_draw_string(lx, ly, icons[i].label, ICON_TEXT_COLOR, DESKTOP_COLOR);
    }

    /* 2. Dynamic icons from Desktop folder */
    int start_x = 100;
    int cur_y = 20;
    for (int i = 0; i < desktop_file_count; i++) {
        vfs_node_t *node = vfs_get(desktop_file_indices[i]);
        if (!node) continue;

        int ix = start_x;
        int iy = cur_y;

        /* Folder icon */
        if (node->type == VFS_TYPE_DIR) {
            fb_draw_rect(ix + 18, iy + 2, 12, 6, 0xFFE0C060);
            fb_draw_rect(ix + 18, iy + 6, 28, 22, 0xFFE0C060);
        } else { /* File icon */
            fb_draw_rect(ix + 20, iy + 2, 24, 28, 0xFFFFFFFF);
            fb_draw_rect(ix + 20, iy + 2, 24, 1, 0xFF808080);
        }

        /* Label */
        gfx_draw_string(ix + 4, iy + 36, node->name, ICON_TEXT_COLOR, DESKTOP_COLOR);

        cur_y += 80;
        if (cur_y > SCREEN_H - 100) {
            cur_y = 20;
            start_x += 80;
        }
    }
}

static void gui_draw_taskbar(void)
{
    /* Taskbar background */
    fb_draw_rect(0, SCREEN_H - TASKBAR_H, SCREEN_W, TASKBAR_H, TASKBAR_COLOR);

    /* Separator line */
    fb_draw_hline(0, SCREEN_H - TASKBAR_H, SCREEN_W, 0x00404060);

    /* Start button */
    unsigned int btn_color = start_menu_open ? 0x00506090 : 0x00405080;
    fb_draw_rect(4, SCREEN_H - TASKBAR_H + 4, 80, TASKBAR_H - 8, btn_color);
    gfx_draw_string(12, SCREEN_H - TASKBAR_H + 8, "OneOS", TASKBAR_TEXT, btn_color);

    /* Window buttons on taskbar */
    int tx = 92;
    for (int i = 0; i < win_count; i++) {
        if (windows[i].visible) {
            unsigned int tbtn = (i == active_win) ? 0x00506090 : 0x00354060;
            fb_draw_rect(tx, SCREEN_H - TASKBAR_H + 4, 100, TASKBAR_H - 8, tbtn);
            gfx_draw_string(tx + 6, SCREEN_H - TASKBAR_H + 8, windows[i].title, TASKBAR_TEXT, tbtn);
            tx += 104;
        }
    }

    /* Clock (right side) */
    char time_str[9]; /* "HH:MM" or "HH:MM:SS" */
    int_to_2digit(clock_hour, time_str);
    time_str[2] = ':';
    int_to_2digit(clock_minute, time_str + 3);
    if (show_seconds) {
        time_str[5] = ':';
        int_to_2digit(clock_second, time_str + 6);
        time_str[8] = '\0';
    } else {
        time_str[5] = '\0';
    }
    int text_len = show_seconds ? 8 : 5;
    int clock_x = SCREEN_W - (text_len * 16) - 10;
    int clock_y = SCREEN_H - TASKBAR_H + 8;
    gfx_draw_string(clock_x, clock_y, time_str, TASKBAR_TEXT, TASKBAR_COLOR);
}

static void gui_draw_window(gui_window_t *win, int is_active)
{
    if (!win->visible) return;

    int x = win->x, y = win->y, w = win->w, h = win->h;

    /* Shadow */
    fb_draw_rect(x + 3, y + 3, w, h, 0xFF101010);

    /* Window border */
    fb_draw_rect(x, y, w, h, WIN_BORDER);

    /* Title bar */
    unsigned int tb_color = is_active ? WIN_TITLEBAR_ACTIVE : WIN_TITLEBAR_COLOR;
    fb_draw_rect(x + 1, y + 1, w - 2, WIN_TITLEBAR_H, tb_color);
    gfx_draw_string(x + 8, y + 4, win->title, 0xFFFFFFFF, tb_color);

    /* Close button [X] */
    fb_draw_rect(x + w - 25, y + 3, 20, 18, 0xFFC04040);
    gfx_draw_string(x + w - 22, y + 4, "X", 0xFFFFFFFF, 0xFFC04040);

    /* Maximize button [M] / Restore [R] */
    fb_draw_rect(x + w - 48, y + 3, 20, 18, 0xFF408040);
    gfx_draw_string(x + w - 45, y + 4, win->maximized ? "R" : "M", 0xFFFFFFFF, 0xFF408040);

    /* Window background */
    unsigned int bg = WIN_BG_COLOR;
    if (transparency_enabled) {
        /* Mock transparency: just use a slightly different color or actual alpha if supported */
        bg = (bg & 0x00FFFFFF) | 0xCC000000; /* ~80% opaque */
    }
    fb_draw_rect(x + 1, y + WIN_TITLEBAR_H + 1, w - 2, h - WIN_TITLEBAR_H - 2, bg);

    /* Toolbar for Notepad */
    if (win == &windows[WIN_NOTEPAD]) {
        fb_draw_rect(x + 1, y + WIN_TITLEBAR_H + 1, w - 2, 28, 0x00D0D0D0);
        /* Save button */
        fb_draw_rect(x + 4, y + WIN_TITLEBAR_H + 4, 60, 20, 0x0040A040);
        gfx_draw_string(x + 10, y + WIN_TITLEBAR_H + 6, "Save", 0x00FFFFFF, 0x0040A040);
        /* New button */
        fb_draw_rect(x + 70, y + WIN_TITLEBAR_H + 4, 60, 20, 0x004040A0);
        gfx_draw_string(x + 76, y + WIN_TITLEBAR_H + 6, "New", 0x00FFFFFF, 0x004040A0);
        
        /* Filename */
        gfx_draw_string(x + 140, y + WIN_TITLEBAR_H + 6, notepad_filename, 0x00000000, 0x00D0D0D0);
        /* Folder icon/indicator */
        vfs_node_t *nd = vfs_get(notepad_dir);
        if (nd) {
            gfx_draw_string(x + 300, y + WIN_TITLEBAR_H + 6, "@", 0x00404040, 0x00D0D0D0);
            gfx_draw_string(x + 315, y + WIN_TITLEBAR_H + 6, nd->name, 0x00606060, 0x00D0D0D0);
        }
    }
}

static void gui_draw_file_manager(void)
{
    gui_window_t *win = &windows[WIN_FILE_MGR];
    if (!win->visible) return;

    int x = win->x + 4;
    int y = win->y + WIN_TITLEBAR_H + 8;

    /* Path bar */
    vfs_node_t *dir = vfs_get(fm_current_dir);
    if (dir) {
        fb_draw_rect(x, y - 4, win->w - 10, 20, 0x00FFFFFF);
        gfx_draw_string(x + 4, y, dir->name[0] == '/' && dir->name[1] == '\0' ? "/" : dir->name, 0x00000000, 0x00FFFFFF);
    }
    y += 24;

    /* Separator */
    fb_draw_hline(x, y, win->w - 10, 0x00A0A0A0);
    y += 8;

    /* "Up" entry if not at root */
    if (fm_current_dir != vfs_root()) {
        unsigned int bg = (fm_selected == -2) ? 0x00A0C0E0 : WIN_BG_COLOR;
        fb_draw_rect(x, y - 2, win->w - 10, 20, bg);
        gfx_draw_string(x + 24, y, ".. (up)", 0x00000080, bg);
        y += 22;
    }

    /* List files */
    for (int i = 0; i < fm_file_count; i++) {
        vfs_node_t *node = vfs_get(fm_file_indices[i]);
        if (!node) continue;

        unsigned int bg = (fm_selected == i) ? 0x00A0C0E0 : WIN_BG_COLOR;
        fb_draw_rect(x, y - 2, win->w - 10, 20, bg);

        if (node->type == VFS_TYPE_DIR) {
            fb_draw_rect(x + 2, y, 14, 12, 0x00E0C060);
            fb_draw_rect(x + 2, y - 2, 8, 4, 0x00E0C060);
        } else {
            fb_draw_rect(x + 4, y - 1, 10, 14, 0x00FFFFFF);
            fb_draw_rect(x + 4, y - 1, 10, 1, 0x00808080);
            fb_draw_rect(x + 4, y + 13, 10, 1, 0x00808080);
            fb_draw_rect(x + 4, y - 1, 1, 14, 0x00808080);
            fb_draw_rect(x + 13, y - 1, 1, 14, 0x00808080);
        }
        gfx_draw_string(x + 24, y, node->name, 0x00000000, bg);

        y += 22;
    }
}

static void gui_draw_notepad(void)
{
    gui_window_t *win = &windows[WIN_NOTEPAD];
    if (!win->visible) return;

    int x = win->x + 8;
    int y = win->y + WIN_TITLEBAR_H + 8 + 28; /* +28 for toolbar */
    int max_x = win->x + win->w - 16;

    /* Text area background */
    fb_draw_rect(win->x + 2, win->y + WIN_TITLEBAR_H + 2 + 28,
                 win->w - 4, win->h - WIN_TITLEBAR_H - 4 - 28, 0x00F8F8F8);

    /* Draw text content */
    int cx = x, cy = y;
    for (int i = 0; i < notepad_len; i++) {
        if (notepad_buf[i] == '\n') {
            cx = x;
            cy += 18;
            if (cy > win->y + win->h - 20) break;
        } else {
            if (cx + 16 > max_x) {
                cx = x;
                cy += 18;
                if (cy > win->y + win->h - 20) break;
            }
            gfx_draw_char(cx, cy, notepad_buf[i], 0x00000000, 0x00F8F8F8);
            cx += 16;
        }
    }

    /* Blinking cursor (always shown for simplicity) */
    if (notepad_active) {
        fb_draw_rect(cx, cy, 2, 16, 0x00000000);
    }

    /* Status bar at bottom */
    int sy = win->y + win->h - 20;
    fb_draw_rect(win->x + 1, sy, win->w - 2, 19, 0x00D0D0D0);
    char status[32];
    /* "Chars: NNNN" */
    str_copy(status, "Chars: ", 32);
    /* Simple number append */
    int slen = 7;
    int n = notepad_len;
    char digits[6];
    int dcount = 0;
    if (n == 0) { digits[dcount++] = '0'; }
    else { while (n > 0 && dcount < 5) { digits[dcount++] = '0' + (n % 10); n /= 10; } }
    /* Reverse */
    for (int i = dcount - 1; i >= 0; i--) {
        status[slen++] = digits[i];
    }
    status[slen] = '\0';
    gfx_draw_string(win->x + 8, sy + 2, status, 0x00404040, 0x00D0D0D0);
}

static void gui_draw_settings(void)
{
    gui_window_t *win = &windows[WIN_SETTINGS];
    if (!win->visible) return;

    int x = win->x;
    int y = win->y + WIN_TITLEBAR_H + 16;

    /* Title */
    gfx_draw_string(x + 16, y, "Clock Settings", 0xFF000000, WIN_BG_COLOR);
    y += 30;

    /* Current time display */
    char time_str[6];
    int_to_2digit(clock_hour, time_str);
    time_str[2] = ':';
    int_to_2digit(clock_minute, time_str + 3);
    time_str[5] = '\0';

    fb_draw_rect(x + 100, y - 4, 100, 28, 0xFFFFFFFF);
    gfx_draw_string(x + 120, y, time_str, 0xFF000000, 0xFFFFFFFF);
    y += 40;

    /* Hour controls */
    gfx_draw_string(x + 16, y + 4, "Hour:", 0xFF000000, WIN_BG_COLOR);
    /* [-] button */
    fb_draw_rect(x + 120, y, 36, 24, 0xFF506090);
    gfx_draw_string(x + 130, y + 4, "-", 0xFFFFFFFF, 0xFF506090);
    /* [+] button */
    fb_draw_rect(x + 170, y, 36, 24, 0xFF506090);
    gfx_draw_string(x + 178, y + 4, "+", 0xFFFFFFFF, 0xFF506090);
    y += 36;

    /* Minute controls */
    gfx_draw_string(x + 16, y + 4, "Min:", 0xFF000000, WIN_BG_COLOR);
    /* [-] button */
    fb_draw_rect(x + 120, y, 36, 24, 0xFF506090);
    gfx_draw_string(x + 130, y + 4, "-", 0xFFFFFFFF, 0xFF506090);
    /* [+] button */
    fb_draw_rect(x + 170, y, 36, 24, 0xFF506090);
    gfx_draw_string(x + 178, y + 4, "+", 0xFFFFFFFF, 0xFF506090);
    y += 48;

    /* Desktop Theme */
    gfx_draw_string(x + 16, y, "Desktop Theme", 0xFF000000, WIN_BG_COLOR);
    y += 24;
    unsigned int themes[] = { 0x206080, 0x602040, 0x206040, 0x303030 };
    for (int i = 0; i < 4; i++) {
        unsigned int c = 0xFF000000 | themes[i];
        fb_draw_rect(x + 16 + i * 40, y, 32, 24, c);
        if (current_desktop_color == c) {
            fb_draw_rect(x + 16 + i * 40, y + 26, 32, 2, 0xFF00FF00);
        }
    }
    y += 40;

    /* Transparency Toggle */
    gfx_draw_string(x + 16, y + 4, "Transparency:", 0xFF000000, WIN_BG_COLOR);
    fb_draw_rect(x + 140, y, 60, 24, transparency_enabled ? 0xFF40A040 : 0xFF804040);
    gfx_draw_string(x + 145, y + 4, transparency_enabled ? "ON" : "OFF", 0xFFFFFFFF, transparency_enabled ? 0xFF40A040 : 0xFF804040);

    y += 40;

    /* Show seconds toggle */
    gfx_draw_string(x + 16, y + 4, "Show Seconds:", 0xFF000000, WIN_BG_COLOR);
    fb_draw_rect(x + 140, y, 60, 24, show_seconds ? 0xFF40A040 : 0xFF804040);
    gfx_draw_string(x + 145, y + 4, show_seconds ? "ON" : "OFF", 0xFFFFFFFF, show_seconds ? 0xFF40A040 : 0xFF804040);

    y += 36;

    /* Mouse speed */
    gfx_draw_string(x + 16, y + 4, "Mouse Speed:", 0xFF000000, WIN_BG_COLOR);
    fb_draw_rect(x + 140, y, 36, 24, 0xFF506090);
    gfx_draw_string(x + 150, y + 4, "-", 0xFFFFFFFF, 0xFF506090);
    fb_draw_rect(x + 220, y, 36, 24, 0xFF506090);
    gfx_draw_string(x + 230, y + 4, "+", 0xFFFFFFFF, 0xFF506090);
    /* Value */
    char ms[4] = { '0' + mouse_speed, 'x', '\0', '\0' };
    gfx_draw_string(x + 185, y + 4, ms, 0xFF000000, WIN_BG_COLOR);

    y += 36;

    /* Cursor size */
    gfx_draw_string(x + 16, y + 4, "Cursor Size:", 0xFF000000, WIN_BG_COLOR);
    fb_draw_rect(x + 140, y, 80, 24, 0xFF606060);
    gfx_draw_string(x + 148, y + 4, cursor_scale == 1 ? "Small" : "Large", 0xFFFFFFFF, 0xFF606060);

    y += 36;

    /* Cursor style */
    gfx_draw_string(x + 16, y + 4, "Cursor Style:", 0xFF000000, WIN_BG_COLOR);
    fb_draw_rect(x + 140, y, 36, 24, 0xFF506090);
    gfx_draw_string(x + 150, y + 4, "-", 0xFFFFFFFF, 0xFF506090);
    fb_draw_rect(x + 220, y, 36, 24, 0xFF506090);
    gfx_draw_string(x + 230, y + 4, "+", 0xFFFFFFFF, 0xFF506090);
    gfx_draw_string(x + 180, y + 4, cursor_style_labels[cursor_style], 0xFF000000, WIN_BG_COLOR);

    y += 36;

    /* Brush size */
    gfx_draw_string(x + 16, y + 4, "Brush Size:", 0xFF000000, WIN_BG_COLOR);
    fb_draw_rect(x + 140, y, 36, 24, 0xFF506090);
    gfx_draw_string(x + 150, y + 4, "-", 0xFFFFFFFF, 0xFF506090);
    fb_draw_rect(x + 220, y, 36, 24, 0xFF506090);
    gfx_draw_string(x + 230, y + 4, "+", 0xFFFFFFFF, 0xFF506090);
    char bs[4] = { '0' + paint_brush, '\0', '\0', '\0' };
    gfx_draw_string(x + 190, y + 4, bs, 0xFF000000, WIN_BG_COLOR);
}

static void gui_draw_sysinfo(void)
{
    gui_window_t *win = &windows[WIN_SYSINFO];
    if (!win->visible) return;

    int x = win->x + 16;
    int y = win->y + WIN_TITLEBAR_H + 20;

    gfx_draw_string(x, y, "OneOS-ARM Kernel v1.0", 0xFF000000, WIN_BG_COLOR);
    y += 24;
    gfx_draw_string(x, y, "Architecture: AArch64", 0xFF404040, WIN_BG_COLOR);
    y += 20;
    gfx_draw_string(x, y, "Machine: QEMU Virt", 0xFF404040, WIN_BG_COLOR);
    y += 20;
    gfx_draw_string(x, y, "Memory: 256 MB RAM", 0xFF404040, WIN_BG_COLOR);
    y += 30;

    /* Some dynamic mock data or from hardware */
    gfx_draw_string(x, y, "Status: System Running", 0xFF006000, WIN_BG_COLOR);
    y += 20;
    
    unsigned long freq = get_timer_freq();
    char freq_str[32] = "Timer Freq (Hz): ";
    /* Simple integer to string conversion */
    int slen = 17;
    unsigned long n = freq;
    char digits[12]; int dcount = 0;
    if (n == 0) digits[dcount++] = '0';
    else while (n > 0 && dcount < 11) { digits[dcount++] = '0' + (n % 10); n /= 10; }
    for (int i = dcount-1; i >= 0; i--) freq_str[slen++] = digits[i];
    freq_str[slen] = '\0';
    gfx_draw_string(x, y, freq_str, 0xFF404040, WIN_BG_COLOR);
}

static void gui_draw_clock(void)
{
    gui_window_t *win = &windows[WIN_CLOCK];
    if (!win->visible) return;

    int x = win->x + 20;
    int y = win->y + WIN_TITLEBAR_H + 24;

    char time_str[9];
    int_to_2digit(clock_hour, time_str);
    time_str[2] = ':';
    int_to_2digit(clock_minute, time_str + 3);
    time_str[5] = ':';
    int_to_2digit(clock_second, time_str + 6);
    time_str[8] = '\0';

    gfx_draw_string(x, y, time_str, 0xFF000000, WIN_BG_COLOR);
    y += 28;
    gfx_draw_string(x, y, show_seconds ? "Seconds: ON" : "Seconds: OFF", 0xFF404040, WIN_BG_COLOR);
}

static void gui_draw_paint(void)
{
    gui_window_t *win = &windows[WIN_PAINT];
    if (!win->visible) return;

    int toolbar_x = win->x + 8;
    int toolbar_y = win->y + WIN_TITLEBAR_H + 8;
    int toolbar_w = win->w - 16;

    /* Toolbar background */
    fb_draw_rect(toolbar_x, toolbar_y, toolbar_w, 28, 0xFFD0D0D0);

    /* Color palette */
    static const unsigned int palette[] = {
        0xFF000000, 0xFFFFFFFF, 0xFFFF0000, 0xFF00FF00, 0xFF0000FF, 0xFFFFFF00
    };
    int px = toolbar_x + 6;
    int py = toolbar_y + 4;
    for (int i = 0; i < 6; i++) {
        fb_draw_rect(px + i * 22, py, 18, 18, palette[i]);
        if (paint_color == palette[i]) {
            fb_draw_rect(px + i * 22 - 2, py - 2, 22, 22, 0xFF202020);
        }
    }

    /* Clear button */
    fb_draw_rect(toolbar_x + toolbar_w - 60, toolbar_y + 4, 52, 20, 0xFF804040);
    gfx_draw_string(toolbar_x + toolbar_w - 54, toolbar_y + 6, "Clear", 0xFFFFFFFF, 0xFF804040);

    /* Canvas */
    int canvas_x = win->x + 12;
    int canvas_y = win->y + WIN_TITLEBAR_H + 44;
    fb_draw_rect(canvas_x - 2, canvas_y - 2, PAINT_W + 4, PAINT_H + 4, 0xFF404040);

    for (int cy = 0; cy < PAINT_H; cy++) {
        int row = cy * PAINT_W;
        for (int cx = 0; cx < PAINT_W; cx++) {
            fb_draw_pixel(canvas_x + cx, canvas_y + cy, paint_canvas[row + cx]);
        }
    }
}

static void gui_draw_terminal(void)
{
    gui_window_t *win = &windows[WIN_TERMINAL];
    if (!win->visible) return;

    int x = win->x + 8;
    int y = win->y + WIN_TITLEBAR_H + 8;
    int w = win->w - 16;
    int h = win->h - WIN_TITLEBAR_H - 16;

    fb_draw_rect(x, y, w, h, 0xFF000000);

    int cx = x + 4;
    int cy = y + 4;
    int max_x = x + w - 4;
    int max_y = y + h - 20;

    for (int i = 0; i < term_len; i++) {
        char c = term_buf[i];
        if (c == '\n' || cx + 16 > max_x) {
            cx = x + 4;
            cy += 16;
            if (cy > max_y) break;
            if (c == '\n') continue;
        }
        gfx_draw_char(cx, cy, c, 0xFF00FF00, 0xFF000000);
        cx += 16;
    }

    /* Cursor */
    if (term_active) {
        if (cx + 2 > max_x) {
            cx = x + 4;
            cy += 16;
        }
        if (cy <= max_y) {
            fb_draw_rect(cx, cy, 2, 16, 0xFF00FF00);
        }
    }
}

static void gui_draw_start_menu(void)
{
    if (!start_menu_open) return;

    int menu_h = MENU_ITEMS * MENU_ITEM_H + 8;
    int my = SCREEN_H - TASKBAR_H - menu_h;

    /* Menu shadow */
    fb_draw_rect(MENU_X + 3, my + 3, MENU_W, menu_h, 0x00101010);
    /* Menu background */
    fb_draw_rect(MENU_X, my, MENU_W, menu_h, MENU_BG);
    /* Border */
    fb_draw_hline(MENU_X, my, MENU_W, 0x00505070);
    fb_draw_vline(MENU_X, my, menu_h, 0x00505070);
    fb_draw_vline(MENU_X + MENU_W - 1, my, menu_h, 0x00505070);

    for (int i = 0; i < MENU_ITEMS; i++) {
        int iy = my + 4 + i * MENU_ITEM_H;
        /* Highlight if mouse is over */
        int hover = (mouse_x >= MENU_X && mouse_x < MENU_X + MENU_W &&
                     mouse_y >= iy && mouse_y < iy + MENU_ITEM_H);
        unsigned int bg = hover ? MENU_HOVER : MENU_BG;
        fb_draw_rect(MENU_X + 2, iy, MENU_W - 4, MENU_ITEM_H, bg);
        gfx_draw_string_transparent(MENU_X + 13, iy + 5, menu_labels[i], MENU_TEXT_SHADOW);
        gfx_draw_string_transparent(MENU_X + 12, iy + 4, menu_labels[i], MENU_TEXT);
    }
}

static void gui_draw_context_menu(void)
{
    if (!context_menu_open) return;
    
    int mx = context_x;
    int my = context_y;
    
    /* Keep menu on screen */
    if (mx + CONTEXT_W > SCREEN_W) mx = SCREEN_W - CONTEXT_W;
    if (my + CONTEXT_H > SCREEN_H) my = SCREEN_H - CONTEXT_H;
    
    /* Shadow */
    fb_draw_rect(mx + 3, my + 3, CONTEXT_W, CONTEXT_H, 0x00101010);
    /* Background */
    fb_draw_rect(mx, my, CONTEXT_W, CONTEXT_H, CONTEXT_BG);
    /* Border */
    fb_draw_hline(mx, my, CONTEXT_W, 0x00505070);
    fb_draw_vline(mx, my, CONTEXT_H, 0x00505070);
    fb_draw_vline(mx + CONTEXT_W - 1, my, CONTEXT_H, 0x00505070);
    fb_draw_hline(mx, my + CONTEXT_H - 1, CONTEXT_W, 0xFF505070);
    
    for (int i = 0; i < 3; i++) {
        int iy = my + 4 + i * 28;
        int hover = (mouse_x >= mx && mouse_x < mx + CONTEXT_W &&
                     mouse_y >= iy - 4 && mouse_y < iy + 24);
        unsigned int bg = hover ? CONTEXT_HOVER : CONTEXT_BG;
        fb_draw_rect(mx + 2, iy - 4, CONTEXT_W - 4, 28, bg);
        gfx_draw_string_transparent(mx + 12, iy + 4, context_menu_items[i], CONTEXT_TEXT);
    }
}

static void gui_draw_rename_dialog(void)
{
    if (!rename_dialog_open) return;
    
    int dw = 300;
    int dh = 100;
    int dx = (SCREEN_W - dw) / 2;
    int dy = (SCREEN_H - dh) / 2;
    
    /* Shadow */
    fb_draw_rect(dx + 4, dy + 4, dw, dh, 0x00101010);
    /* Background */
    fb_draw_rect(dx, dy, dw, dh, WIN_BG_COLOR);
    /* Border */
    fb_draw_rect(dx, dy, dw, dh, WIN_BORDER);
    
    /* Title */
    fb_draw_rect(dx + 1, dy + 1, dw - 2, WIN_TITLEBAR_H, WIN_TITLEBAR_ACTIVE);
    gfx_draw_string(dx + 8, dy + 4, "Rename File", 0x00FFFFFF, WIN_TITLEBAR_ACTIVE);
    
    /* Input field */
    int ix = dx + 20;
    int iy = dy + WIN_TITLEBAR_H + 20;
    fb_draw_rect(ix, iy, dw - 40, 24, 0x00FFFFFF);
    fb_draw_rect(ix, iy, dw - 40, 24, WIN_BORDER); /* draw border again just in case, actually let's draw lines */
    fb_draw_hline(ix, iy, dw - 40, 0x00404040);
    fb_draw_vline(ix, iy, 24, 0x00404040);
    
    /* Draw text */
    gfx_draw_string(ix + 6, iy + 6, rename_buf, 0x00000000, 0x00FFFFFF);
    
    /* Blinking cursor (always on) */
    int cursor_x = ix + 6 + rename_len * 8; /* each char is ~8 wide */
    fb_draw_rect(cursor_x, iy + 4, 2, 16, 0x00000000);
}

static void gui_draw_mouse(void)
{
    int scale = cursor_scale;
    int sx = mouse_x;
    int sy = mouse_y;

    if (cursor_style == 0) {
        /* Windows-like arrow cursor (16x16), with centered tail */
        static const unsigned short outline[16] = {
            0x8000, 0xC000, 0xE000, 0xF000,
            0xF800, 0xFC00, 0xFE00, 0xFF00,
            0xFF80, 0xFFC0, 0xFFE0, 0xFFF0,
            0x0000, 0x0000, 0x0000, 0x0000
        };
        static const unsigned short fill[16] = {
            0x0000, 0x8000, 0xC000, 0xE000,
            0xF000, 0xF800, 0xFC00, 0xFE00,
            0xFF00, 0xFF80, 0xFFC0, 0xFFE0,
            0x0000, 0x0000, 0x0000, 0x0000
        };

        /* Shadow pass (offset 1,1) */
        for (int row = 0; row < 16; row++) {
            for (int col = 0; col < 16; col++) {
                unsigned short bit = (unsigned short)(1u << (15 - col));
                if (outline[row] & bit) {
                    int px = sx + col * scale + 1;
                    int py = sy + row * scale + 1;
                    for (int dy = 0; dy < scale; dy++) {
                        for (int dx = 0; dx < scale; dx++) {
                            fb_draw_pixel(px + dx, py + dy, MOUSE_SHADOW);
                        }
                    }
                }
            }
        }

        /* Outline pass */
        for (int row = 0; row < 16; row++) {
            for (int col = 0; col < 16; col++) {
                unsigned short bit = (unsigned short)(1u << (15 - col));
                if (outline[row] & bit) {
                    int px = sx + col * scale;
                    int py = sy + row * scale;
                    for (int dy = 0; dy < scale; dy++) {
                        for (int dx = 0; dx < scale; dx++) {
                            fb_draw_pixel(px + dx, py + dy, MOUSE_OUTLINE);
                        }
                    }
                }
            }
        }

        /* Fill pass */
        for (int row = 0; row < 16; row++) {
            for (int col = 0; col < 16; col++) {
                unsigned short bit = (unsigned short)(1u << (15 - col));
                if (fill[row] & bit) {
                    int px = sx + col * scale;
                    int py = sy + row * scale;
                    for (int dy = 0; dy < scale; dy++) {
                        for (int dx = 0; dx < scale; dx++) {
                            fb_draw_pixel(px + dx, py + dy, MOUSE_COLOR);
                        }
                    }
                }
            }
        }
    } else if (cursor_style == 1) {
        /* Crosshair */
        int size = 16 * scale;
        int cx = sx + 8 * scale;
        int cy = sy + 8 * scale;
        for (int i = -8 * scale; i <= 8 * scale; i++) {
            fb_draw_pixel(cx + i, cy, MOUSE_OUTLINE);
            fb_draw_pixel(cx, cy + i, MOUSE_OUTLINE);
        }
        for (int i = -6 * scale; i <= 6 * scale; i++) {
            fb_draw_pixel(cx + i, cy, MOUSE_COLOR);
            fb_draw_pixel(cx, cy + i, MOUSE_COLOR);
        }
        (void)size;
    } else {
        /* I-Beam */
        int cx = sx + 7 * scale;
        int top = sy + 2 * scale;
        int bottom = sy + 14 * scale;
        for (int i = 0; i < 8 * scale; i++) {
            fb_draw_pixel(cx + i, top, MOUSE_OUTLINE);
            fb_draw_pixel(cx + i, bottom, MOUSE_OUTLINE);
        }
        for (int y = top; y <= bottom; y++) {
            fb_draw_pixel(cx + 4 * scale, y, MOUSE_OUTLINE);
        }
        for (int y = top + 1; y <= bottom - 1; y++) {
            fb_draw_pixel(cx + 4 * scale, y, MOUSE_COLOR);
        }
    }
}

/* ---- Input handling ---- */
static void gui_handle_mouse_move(int dx, int dy)
{
    mouse_x += dx * mouse_speed;
    mouse_y += dy * mouse_speed;
    if (mouse_x < 0) mouse_x = 0;
    if (mouse_y < 0) mouse_y = 0;
    if (mouse_x >= SCREEN_W) mouse_x = SCREEN_W - 1;
    if (mouse_y >= SCREEN_H) mouse_y = SCREEN_H - 1;
}

/* Check if point is inside a rectangle */
static int hit_rect(int px, int py, int rx, int ry, int rw, int rh)
{
    return (px >= rx && px < rx + rw && py >= ry && py < ry + rh);
}

static void gui_cursor_restore(void)
{
    if (!cursor_valid) return;
    framebuffer_t *fbp = fb_get();
    unsigned int *vram = (unsigned int *)fbp->buffer;
    int cursor_w = 16 * cursor_scale + 1;
    int cursor_h = 16 * cursor_scale + 1;
    if (cursor_w > CURSOR_MAX) cursor_w = CURSOR_MAX;
    if (cursor_h > CURSOR_MAX) cursor_h = CURSOR_MAX;

    for (int y = 0; y < cursor_h; y++) {
        int py = cursor_old_y + y;
        if (py < 0 || py >= (int)fbp->height) continue;
        for (int x = 0; x < cursor_w; x++) {
            int px = cursor_old_x + x;
            if (px < 0 || px >= (int)fbp->width) continue;
            vram[py * fbp->width + px] = cursor_under[y * CURSOR_MAX + x];
        }
    }
}

static void gui_cursor_save(void)
{
    framebuffer_t *fbp = fb_get();
    unsigned int *vram = (unsigned int *)fbp->buffer;
    int cursor_w = 16 * cursor_scale + 1;
    int cursor_h = 16 * cursor_scale + 1;
    if (cursor_w > CURSOR_MAX) cursor_w = CURSOR_MAX;
    if (cursor_h > CURSOR_MAX) cursor_h = CURSOR_MAX;

    for (int y = 0; y < cursor_h; y++) {
        int py = mouse_y + y;
        for (int x = 0; x < cursor_w; x++) {
            int px = mouse_x + x;
            unsigned int val = 0;
            if (px >= 0 && px < (int)fbp->width && py >= 0 && py < (int)fbp->height) {
                val = vram[py * fbp->width + px];
            }
            cursor_under[y * CURSOR_MAX + x] = val;
        }
    }

    cursor_old_x = mouse_x;
    cursor_old_y = mouse_y;
    cursor_valid = 1;
}

static void gui_draw_mouse_overlay(void)
{
    gui_cursor_restore();
    gui_cursor_save();
    gui_draw_mouse();
}

static int gui_paint_draw_at(int px, int py)
{
    gui_window_t *win = &windows[WIN_PAINT];
    if (!win->visible) return 0;

    int canvas_x = win->x + 12;
    int canvas_y = win->y + WIN_TITLEBAR_H + 44;
    if (!hit_rect(px, py, canvas_x, canvas_y, PAINT_W, PAINT_H)) return 0;

    int cx = px - canvas_x;
    int cy = py - canvas_y;
    int half = paint_brush / 2;

    for (int by = -half; by <= half; by++) {
        for (int bx = -half; bx <= half; bx++) {
            int tx = cx + bx;
            int ty = cy + by;
            if (tx >= 0 && tx < PAINT_W && ty >= 0 && ty < PAINT_H) {
                paint_canvas[ty * PAINT_W + tx] = paint_color;
                if (!gui_use_backbuffer) {
                    fb_draw_pixel(canvas_x + tx, canvas_y + ty, paint_color);
                }
            }
        }
    }

    if (!gui_use_backbuffer) {
        cursor_valid = 0;
    }

    return 1;
}

static void gui_handle_right_click(void)
{
    /* Close menu if already open */
    if (context_menu_open) {
        context_menu_open = 0;
        return;
    }

    /* Check if right-clicking on a file in File Manager */
    for (int z = win_count - 1; z >= 0; z--) {
        int wid = win_order[z];
        gui_window_t *win = &windows[wid];
        if (!win->visible) continue;

        if (wid == WIN_FILE_MGR) {
            if (hit_rect(mouse_x, mouse_y, win->x, win->y, win->w, win->h)) {
                int list_x = win->x + 4;
                int list_y = win->y + WIN_TITLEBAR_H + 8 + 32;
                int item_h = 22;

                /* Skip "Up" entry */
                if (fm_current_dir != vfs_root()) list_y += item_h;

                for (int fi = 0; fi < fm_file_count; fi++) {
                    int iy = list_y + fi * item_h;
                    if (hit_rect(mouse_x, mouse_y, list_x, iy - 2, win->w - 10, item_h)) {
                        context_menu_open = 1;
                        context_x = mouse_x;
                        context_y = mouse_y;
                        context_file_idx = fm_file_indices[fi];
                        return;
                    }
                }
            }
        }
    }
}

static void gui_handle_click(void)
{
    /* 0. Context Menu Click */
    if (context_menu_open) {
        if (hit_rect(mouse_x, mouse_y, context_x, context_y, CONTEXT_W, CONTEXT_H)) {
            int item = (mouse_y - context_y - 4) / 28;
            if (item == 0) { /* Open */
                vfs_node_t *node = vfs_get(context_file_idx);
                if (node && node->type == VFS_TYPE_FILE) {
                    str_copy(notepad_filename, node->name, WIN_MAX_TITLE);
                    int rlen = vfs_read(context_file_idx, notepad_buf, NOTEPAD_BUF_SIZE);
                    if (rlen >= 0) {
                        notepad_len = rlen;
                        notepad_buf[notepad_len] = '\0';
                        windows[WIN_NOTEPAD].visible = 1;
                        gui_bring_to_front(WIN_NOTEPAD);
                    }
                }
            } else if (item == 1) { /* Rename */
                rename_dialog_open = 1;
                rename_file_idx = context_file_idx;
                vfs_node_t *node = vfs_get(context_file_idx);
                if (node) {
                    str_copy(rename_buf, node->name, VFS_MAX_NAME);
                    rename_len = 0;
                    while (rename_buf[rename_len]) rename_len++;
                }
            } else if (item == 2) { /* Delete */
                vfs_delete(context_file_idx);
                gui_refresh_file_list();
            }
        }
        context_menu_open = 0;
        return;
    }

    /* 1. Start menu click */
    if (start_menu_open) {
        int menu_h = MENU_ITEMS * MENU_ITEM_H + 8;
        int my = SCREEN_H - TASKBAR_H - menu_h;

        if (hit_rect(mouse_x, mouse_y, MENU_X, my, MENU_W, menu_h)) {
            /* Which item? */
            for (int i = 0; i < MENU_ITEMS; i++) {
                int iy = my + 4 + i * MENU_ITEM_H;
                if (hit_rect(mouse_x, mouse_y, MENU_X, iy, MENU_W, MENU_ITEM_H)) {
                    start_menu_open = 0;
                    if (i == 0) { windows[WIN_FILE_MGR].visible = 1; gui_refresh_file_list(); gui_bring_to_front(WIN_FILE_MGR); }
                    else if (i == 1) { windows[WIN_NOTEPAD].visible = 1; gui_bring_to_front(WIN_NOTEPAD); }
                    else if (i == 2) { windows[WIN_SETTINGS].visible = 1; gui_bring_to_front(WIN_SETTINGS); }
                    else if (i == 3) { windows[WIN_SYSINFO].visible = 1; gui_bring_to_front(WIN_SYSINFO); }
                    else if (i == 4) { windows[WIN_CLOCK].visible = 1; gui_bring_to_front(WIN_CLOCK); }
                    else if (i == 5) { windows[WIN_PAINT].visible = 1; gui_bring_to_front(WIN_PAINT); }
                    else if (i == 6) { windows[WIN_TERMINAL].visible = 1; gui_bring_to_front(WIN_TERMINAL); }
                    return;
                }
            }
        }
        /* Clicked outside menu -> close it */
        start_menu_open = 0;
    }

    /* 2. Start button */
    if (hit_rect(mouse_x, mouse_y, 4, SCREEN_H - TASKBAR_H + 4, 80, TASKBAR_H - 8)) {
        start_menu_open = !start_menu_open;
        return;
    }

    /* 2.5 Dynamic Desktop Icons */
    {
        int start_x = 100;
        int cur_y = 20;
        for (int i = 0; i < desktop_file_count; i++) {
            if (hit_rect(mouse_x, mouse_y, start_x, cur_y, 64, 80)) {
                vfs_node_t *node = vfs_get(desktop_file_indices[i]);
                if (node) {
                    if (node->type == VFS_TYPE_DIR) {
                        fm_current_dir = desktop_file_indices[i];
                        windows[WIN_FILE_MGR].visible = 1;
                        gui_bring_to_front(WIN_FILE_MGR);
                        gui_refresh_file_list();
                    } else {
                        /* Open file in notepad */
                        str_copy(notepad_filename, node->name, WIN_MAX_TITLE);
                        int rlen = vfs_read(desktop_file_indices[i], notepad_buf, NOTEPAD_BUF_SIZE);
                        if (rlen >= 0) {
                            notepad_len = rlen;
                            notepad_buf[notepad_len] = '\0';
                            windows[WIN_NOTEPAD].visible = 1;
                            gui_bring_to_front(WIN_NOTEPAD);
                        }
                    }
                }
                return;
            }
            cur_y += 80;
            if (cur_y > SCREEN_H - 100) {
                cur_y = 20;
                start_x += 80;
            }
        }
    }

    /* 3. Taskbar window buttons */
    int tx = 92;
    for (int i = 0; i < win_count; i++) {
        if (windows[i].visible) {
            if (hit_rect(mouse_x, mouse_y, tx, SCREEN_H - TASKBAR_H + 4, 100, TASKBAR_H - 8)) {
                gui_bring_to_front(i);
                return;
            }
            tx += 104;
        }
    }

    /* 4. Desktop icons (double-click simulated: any click opens) */
    for (int i = 0; i < ICON_COUNT; i++) {
        if (hit_rect(mouse_x, mouse_y, icons[i].x, icons[i].y, ICON_W, ICON_H)) {
            int wid = icons[i].win_id;
            windows[wid].visible = 1;
            if (wid == WIN_FILE_MGR) gui_refresh_file_list();
            gui_bring_to_front(wid);
            start_menu_open = 0;
            return;
        }
    }

    /* 5. Windows (check from front to back in z-order) */
    for (int z = win_count - 1; z >= 0; z--) {
        int wid = win_order[z];
        gui_window_t *win = &windows[wid];
        if (!win->visible) continue;

        if (hit_rect(mouse_x, mouse_y, win->x, win->y, win->w, win->h)) {
            gui_bring_to_front(wid);

            /* Close button */
            if (hit_rect(mouse_x, mouse_y, win->x + win->w - 25, win->y + 3, 20, 18)) {
                win->visible = 0;
                notepad_active = 0;
                term_active = 0;
                /* Find next visible window */
                active_win = -1;
                for (int j = win_count - 1; j >= 0; j--) {
                    if (windows[win_order[j]].visible) {
                        active_win = win_order[j];
                        break;
                    }
                }
                return;
            }

            /* Maximize/Restore button */
            if (hit_rect(mouse_x, mouse_y, win->x + win->w - 48, win->y + 3, 20, 18)) {
                if (win->maximized) {
                    win->x = win->old_x;
                    win->y = win->old_y;
                    win->w = win->old_w;
                    win->h = win->old_h;
                    win->maximized = 0;
                } else {
                    win->old_x = win->x;
                    win->old_y = win->y;
                    win->old_w = win->w;
                    win->old_h = win->h;
                    win->x = 0;
                    win->y = 0;
                    win->w = SCREEN_W;
                    win->h = SCREEN_H - TASKBAR_H;
                    win->maximized = 1;
                }
                return;
            }

            /* Settings-specific button handling */
            if (wid == WIN_SETTINGS) {
                int sx = win->x;
                int sy = win->y + WIN_TITLEBAR_H + 16;

                /* Skip title row */
                sy += 30;
                /* Skip time display row */
                sy += 40;

                /* Hour [-] */
                if (hit_rect(mouse_x, mouse_y, sx + 120, sy, 36, 24)) {
                    clock_hour = (clock_hour + 23) % 24;
                    return;
                }
                /* Hour [+] */
                if (hit_rect(mouse_x, mouse_y, sx + 170, sy, 36, 24)) {
                    clock_hour = (clock_hour + 1) % 24;
                    return;
                }
                sy += 36; /* Move to "Min:" row */
                /* Min [-] */
                if (hit_rect(mouse_x, mouse_y, sx + 120, sy, 36, 24)) {
                    clock_minute = (clock_minute + 59) % 60;
                    return;
                }
                /* Min [+] */
                if (hit_rect(mouse_x, mouse_y, sx + 170, sy, 36, 24)) {
                    clock_minute = (clock_minute + 1) % 60;
                    return;
                }
                sy += 48; /* Move to Desktop Theme label */
                sy += 24; /* Move to theme buttons row */
                for (int i = 0; i < 4; i++) {
                    if (hit_rect(mouse_x, mouse_y, sx + 16 + i * 40, sy, 32, 24)) {
                        unsigned int themes[] = { 0x206080, 0x602040, 0x206040, 0x303030 };
                        current_desktop_color = 0xFF000000 | themes[i];
                        return;
                    }
                }
                sy += 40; /* Move to Transparency row */
                if (hit_rect(mouse_x, mouse_y, sx + 140, sy, 60, 24)) {
                    transparency_enabled = !transparency_enabled;
                    return;
                }
                sy += 40; /* Move to Show Seconds row */
                if (hit_rect(mouse_x, mouse_y, sx + 140, sy, 60, 24)) {
                    show_seconds = !show_seconds;
                    return;
                }
                sy += 36; /* Mouse speed row */
                if (hit_rect(mouse_x, mouse_y, sx + 140, sy, 36, 24)) {
                    if (mouse_speed > 1) mouse_speed--;
                    return;
                }
                if (hit_rect(mouse_x, mouse_y, sx + 220, sy, 36, 24)) {
                    if (mouse_speed < 3) mouse_speed++;
                    return;
                }
                sy += 36; /* Cursor size row */
                if (hit_rect(mouse_x, mouse_y, sx + 140, sy, 80, 24)) {
                    cursor_scale = (cursor_scale == 1) ? 2 : 1;
                    cursor_valid = 0;
                    return;
                }
                sy += 36; /* Cursor style row */
                if (hit_rect(mouse_x, mouse_y, sx + 140, sy, 36, 24)) {
                    cursor_style = (cursor_style + CURSOR_STYLE_COUNT - 1) % CURSOR_STYLE_COUNT;
                    cursor_valid = 0;
                    return;
                }
                if (hit_rect(mouse_x, mouse_y, sx + 220, sy, 36, 24)) {
                    cursor_style = (cursor_style + 1) % CURSOR_STYLE_COUNT;
                    cursor_valid = 0;
                    return;
                }
                sy += 36; /* Brush size row */
                if (hit_rect(mouse_x, mouse_y, sx + 140, sy, 36, 24)) {
                    if (paint_brush > 1) paint_brush--;
                    return;
                }
                if (hit_rect(mouse_x, mouse_y, sx + 220, sy, 36, 24)) {
                    if (paint_brush < 4) paint_brush++;
                    return;
                }
            }

            /* File manager click */
            if (wid == WIN_FILE_MGR) {
                int list_x = win->x + 4;
                int list_y = win->y + WIN_TITLEBAR_H + 8 + 32;
                int item_h = 22;

                /* "Up" entry */
                if (fm_current_dir != vfs_root()) {
                    if (hit_rect(mouse_x, mouse_y, list_x, list_y - 2, win->w - 10, item_h)) {
                        vfs_node_t *cur = vfs_get(fm_current_dir);
                        if (cur && cur->parent >= 0) {
                            fm_current_dir = cur->parent;
                            gui_refresh_file_list();
                        }
                        return;
                    }
                    list_y += item_h;
                }

                for (int fi = 0; fi < fm_file_count; fi++) {
                    int iy = list_y + fi * item_h;
                    if (hit_rect(mouse_x, mouse_y, list_x, iy - 2, win->w - 10, item_h)) {
                        if (fm_selected == fi) {
                            vfs_node_t *node = vfs_get(fm_file_indices[fi]);
                            if (node && node->type == VFS_TYPE_DIR) {
                                fm_current_dir = fm_file_indices[fi];
                                gui_refresh_file_list();
                            } else if (node && node->type == VFS_TYPE_FILE) {
                                /* Open in notepad if it's likely a text file */
                                str_copy(notepad_filename, node->name, WIN_MAX_TITLE);
                                int rlen = vfs_read(fm_file_indices[fi], notepad_buf, NOTEPAD_BUF_SIZE);
                                if (rlen >= 0) {
                                    notepad_len = rlen;
                                    notepad_buf[notepad_len] = '\0';
                                    windows[WIN_NOTEPAD].visible = 1;
                                    gui_bring_to_front(WIN_NOTEPAD);
                                }
                            }
                        } else {
                            fm_selected = fi;
                        }
                        return;
                    }
                }
                fm_selected = -1;
            }

            /* Notepad button handling */
            if (wid == WIN_NOTEPAD) {
                int ty = win->y + WIN_TITLEBAR_H + 1;
                /* Save button */
                if (hit_rect(mouse_x, mouse_y, win->x + 4, ty + 3, 60, 22)) {
                    int idx = vfs_find(notepad_filename, notepad_dir);
                    if (idx < 0) idx = vfs_create(notepad_filename, notepad_dir, VFS_TYPE_FILE);
                    if (idx >= 0) vfs_write(idx, notepad_buf, notepad_len);
                    gui_refresh_file_list();
                    return;
                }
                /* New button */
                if (hit_rect(mouse_x, mouse_y, win->x + 70, ty + 3, 60, 22)) {
                    notepad_len = 0;
                    notepad_buf[0] = '\0';
                    str_copy(notepad_filename, "untitled.txt", WIN_MAX_TITLE);
                    notepad_dir = vfs_root();
                    return;
                }

                /* Directory selector (in filename text area) */
                if (hit_rect(mouse_x, mouse_y, win->x + 140, ty + 3, 200, 22)) {
                    /* Cycle through directories for demo, or set to current FM dir */
                    notepad_dir = fm_current_dir;
                    return;
                }
            }

            /* Paint button handling */
            if (wid == WIN_PAINT) {
                int toolbar_x = win->x + 8;
                int toolbar_y = win->y + WIN_TITLEBAR_H + 8;
                int toolbar_w = win->w - 16;

                /* Palette clicks */
                static const unsigned int palette[] = {
                    0xFF000000, 0xFFFFFFFF, 0xFFFF0000, 0xFF00FF00, 0xFF0000FF, 0xFFFFFF00
                };
                int px = toolbar_x + 6;
                int py = toolbar_y + 4;
                for (int i = 0; i < 6; i++) {
                    if (hit_rect(mouse_x, mouse_y, px + i * 22, py, 18, 18)) {
                        paint_color = palette[i];
                        return;
                    }
                }

                /* Clear button */
                if (hit_rect(mouse_x, mouse_y, toolbar_x + toolbar_w - 60, toolbar_y + 4, 52, 20)) {
                    for (int i = 0; i < PAINT_W * PAINT_H; i++) {
                        paint_canvas[i] = 0xFFFFFFFF;
                    }
                    return;
                }

                /* Canvas click */
                if (gui_paint_draw_at(mouse_x, mouse_y)) {
                    return;
                }
            }

            return;
        }
    }

    /* Clicked on desktop background */
    start_menu_open = 0;
    notepad_active = 0;
    term_active = 0;
}

/* ---- Render all ---- */
static void gui_render(void)
{
    if (!gui_use_backbuffer) {
        cursor_valid = 0;
    }

    gui_draw_desktop();
    gui_draw_icons();

    /* Draw windows in z-order (back to front) */
    for (int z = 0; z < win_count; z++) {
        int wid = win_order[z];
        gui_draw_window(&windows[wid], wid == active_win);
        if (wid == WIN_FILE_MGR) gui_draw_file_manager();
        else if (wid == WIN_NOTEPAD) gui_draw_notepad();
        else if (wid == WIN_SETTINGS) gui_draw_settings();
        else if (wid == WIN_SYSINFO) gui_draw_sysinfo();
        else if (wid == WIN_CLOCK) gui_draw_clock();
        else if (wid == WIN_PAINT) gui_draw_paint();
        else if (wid == WIN_TERMINAL) gui_draw_terminal();
    }

    if (rename_dialog_open) gui_draw_rename_dialog();
    gui_draw_taskbar();
    gui_draw_start_menu();
    if (context_menu_open) gui_draw_context_menu();
    if (gui_use_backbuffer) {
        gui_draw_mouse();
        fb_present();
    } else {
        gui_draw_mouse_overlay();
    }
}

/* ---- Main loop ---- */
void gui_run(void)
{
    gui_render();

    while (gui_running) {
        int needs_render = 0;
        int cursor_only = 0;
        static int btn_was_down = 0;

        /* Advance clock */
        clock_advance();

        /* Poll real virtio mouse */
        int dx = 0, dy = 0, btn = 0;
        virtio_input_mouse(&dx, &dy, &btn);

        /* Poll for arrow key navigation */
        int arrow = virtio_input_get_arrow();
        int mods = virtio_input_get_modifiers();

        if (arrow) {
            if (arrow == 103) gui_handle_mouse_move(0, -8);
            if (arrow == 108) gui_handle_mouse_move(0, 8);
            if (arrow == 105) gui_handle_mouse_move(-8, 0);
            if (arrow == 106) gui_handle_mouse_move(8, 0);
            if (gui_use_backbuffer) needs_render = 1;
            else cursor_only = 1;
        }

        /* CTRL = left click */
        if ((mods & 1) && !btn_was_down) {
            gui_handle_click();
            needs_render = 1;
            btn_was_down = 1;
        }

        if (dx != 0 || dy != 0) {
            gui_handle_mouse_move(dx, dy);
            if (gui_use_backbuffer) needs_render = 1;
            else cursor_only = 1;
        }

        if (btn & 1) {
            if (gui_paint_draw_at(mouse_x, mouse_y)) {
                if (gui_use_backbuffer) needs_render = 1;
            }
        }

        /* Mouse button press */
        if ((btn & 1) && !btn_was_down) { /* Left click */
            gui_handle_click();
            needs_render = 1;
            btn_was_down = 1;
        }
        else if ((btn & 2) && !btn_was_down) { /* Right click */
            gui_handle_right_click();
            needs_render = 1;
            btn_was_down = 1;
        }
        else if (btn == 0) {
            btn_was_down = 0;
        }

        /* Poll keyboard */
        unsigned char key = virtio_input_getchar();
        if (!key) {
            extern unsigned char keyboard_read_nonblock(void);
            key = keyboard_read_nonblock();
        }

        if (key) {
            if (key == 0x1B) {
                /* Escape - exit GUI */
                if (rename_dialog_open) {
                    rename_dialog_open = 0;
                    needs_render = 1;
                } else if (context_menu_open) {
                    context_menu_open = 0;
                    needs_render = 1;
                } else {
                    gui_running = 0;
                    break;
                }
            }

            /* Rename dialog input */
            if (rename_dialog_open) {
                if (key == 0x08 || key == 127) {
                    if (rename_len > 0) {
                        rename_len--;
                        rename_buf[rename_len] = '\0';
                    }
                } else if (key == '\r' || key == '\n') {
                    if (rename_len > 0) {
                        vfs_rename(rename_file_idx, rename_buf);
                        gui_refresh_file_list();
                    }
                    rename_dialog_open = 0;
                } else if (key >= 32 && key < 127) {
                    if (rename_len < VFS_MAX_NAME - 1) {
                        rename_buf[rename_len++] = (char)key;
                        rename_buf[rename_len] = '\0';
                    }
                }
                needs_render = 1;
            }
            /* If terminal is active, send keystrokes to it */
            else if (term_active && windows[WIN_TERMINAL].visible) {
                if (key == 0x0C) { /* Ctrl+L */
                    term_len = 0;
                    term_buf[0] = '\0';
                    term_append_str(TERM_PROMPT);
                } else if (key == 0x08 || key == 127) {
                    int last_nl = 0;
                    for (int i = term_len - 1; i >= 0; i--) {
                        if (term_buf[i] == '\n') { last_nl = i + 1; break; }
                    }
                    if (term_len > last_nl + TERM_PROMPT_LEN) {
                        term_len--;
                        term_buf[term_len] = '\0';
                    }
                } else if (key == '\r' || key == '\n') {
                    term_append_char('\n');
                    term_append_str(TERM_PROMPT);
                } else if (key >= 32 && key < 127) {
                    term_append_char((char)key);
                }
                needs_render = 1;
            }
            /* If notepad is active, send keystrokes to it */
            else if (notepad_active && windows[WIN_NOTEPAD].visible) {
                if (key == 0x08 || key == 127) {
                    /* Backspace */
                    if (notepad_len > 0) {
                        notepad_len--;
                        notepad_buf[notepad_len] = '\0';
                    }
                } else if (key == '\r' || key == '\n') {
                    if (notepad_len < NOTEPAD_BUF_SIZE - 1) {
                        notepad_buf[notepad_len++] = '\n';
                        notepad_buf[notepad_len] = '\0';
                    }
                } else if (key >= 32 && key < 127) {
                    if (notepad_len < NOTEPAD_BUF_SIZE - 1) {
                        notepad_buf[notepad_len++] = (char)key;
                        notepad_buf[notepad_len] = '\0';
                    }
                }
                needs_render = 1;
            } else {
                /* Global hotkeys */
                if (key == ' ') {
                    windows[WIN_FILE_MGR].visible = !windows[WIN_FILE_MGR].visible;
                    needs_render = 1;
                }
                if (key == '\t') {
                    windows[WIN_FILE_MGR].visible = 1;
                    gui_refresh_file_list();
                    needs_render = 1;
                }
            }
        }

        if (needs_render) {
            gui_render();
        } else if (cursor_only && !gui_use_backbuffer) {
            gui_draw_mouse_overlay();
        }

        /* Small delay */
        for (volatile int i = 0; i < 1000; i++) {}
    }

    fb_use_backbuffer(0);
}
