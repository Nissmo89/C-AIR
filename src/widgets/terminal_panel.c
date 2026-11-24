#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "widgets/terminal_panel.h"

#include <string.h>
#include <stdlib.h>

/* --------------------------------------------------------------------------
 * Layout constants
 * -------------------------------------------------------------------------- */
#define TERM_COLS       80
#define TERM_ROWS       24
#define CELL_W          8.0f   /* logical units per character cell width  */
#define CELL_H         16.0f   /* logical units per character cell height */
#define HEADER_H       28.0f

/* Default terminal colours (dark background, light foreground) */
#define BG_R  0.11f
#define BG_G  0.11f
#define BG_B  0.15f
#define FG_R  0.85f
#define FG_G  0.85f
#define FG_B  0.85f

/* --------------------------------------------------------------------------
 * Output callback — called from the reader thread
 * -------------------------------------------------------------------------- */
static void apply_queued_output(void *user_data) {
	CairTerminalPanel *panel = (CairTerminalPanel *)user_data;
	char *bytes = NULL;
	size_t length = 0;
	if (!panel || !panel->lock_initialized) return;
	EnterCriticalSection(&panel->output_lock);
	length = panel->output_length;
	bytes = panel->output_queue;
	panel->output_queue = NULL;
	panel->output_length = 0;
	panel->output_capacity = 0;
	panel->output_task_posted = 0;
	LeaveCriticalSection(&panel->output_lock);
	if (bytes) {
		if (panel->initialized) cair_terminal_model_write(&panel->model, bytes, length);
		free(bytes);
	}
	if (panel->canvas != R_INVALID_ID) Rwidget_invalidate(panel->canvas);
}

static void on_process_output(const char *data, size_t len, void *user_data) {
	CairTerminalPanel *panel = (CairTerminalPanel *)user_data;
	int post_task = 0;
	if (!panel || !data || !len || !panel->lock_initialized) return;
	EnterCriticalSection(&panel->output_lock);
	if (len <= SIZE_MAX - panel->output_length) {
		size_t required = panel->output_length + len;
		if (required > panel->output_capacity) {
			size_t capacity = panel->output_capacity ? panel->output_capacity : 4096;
			char *grown;
			while (capacity < required && capacity <= SIZE_MAX / 2) capacity *= 2;
			if (capacity < required) capacity = required;
			grown = (char *)realloc(panel->output_queue, capacity);
			if (grown) {
				panel->output_queue = grown;
				panel->output_capacity = capacity;
			} else {
				len = 0;
			}
		}
	}
	if (len && panel->output_queue) {
		memcpy(panel->output_queue + panel->output_length, data, len);
		panel->output_length += len;
	}
	if (!panel->output_task_posted) {
		panel->output_task_posted = 1;
		post_task = 1;
	}
	LeaveCriticalSection(&panel->output_lock);
	if (post_task) Rtask_post(apply_queued_output, panel);
}

/* --------------------------------------------------------------------------
 * Custom widget: measure
 * -------------------------------------------------------------------------- */
static void on_measure(Rid widget, float *w, float *h, void *user_data) {
	(void)widget;
	(void)user_data;
	*w = TERM_COLS * CELL_W;
	*h = TERM_ROWS * CELL_H;
}

/* --------------------------------------------------------------------------
 * Custom widget: paint
 * -------------------------------------------------------------------------- */
static void on_paint(Rid widget, RPainter *painter, float width, float height,
                     void *user_data) {
	CairTerminalPanel *panel = (CairTerminalPanel *)user_data;
	uint16_t row, col, cols, rows;
	CairTerminalCursor cursor;
	CairTerminalColor default_fg, default_bg;
	char *line;
	float cell_w, cell_h;
	size_t line_capacity;

	(void)widget;

	/* Background */
	Rdraw_rect(painter, 0, 0, width, height, 0, BG_R, BG_G, BG_B, 1.0f);

	if (!panel || !panel->initialized) {
		Rdraw_text(painter, panel->font, 0, 8, height * 0.5f - 8,
		           "Terminal — press Ctrl+` to open", FG_R, FG_G, FG_B, 0.5f);
		return;
	}
	if (!cair_terminal_model_get_dimensions(&panel->model, &cols, &rows)) return;
	line_capacity = (size_t)cols * 32u + 1u;
	line = (char *)malloc(line_capacity);
	if (!line) return;

	/* Sync the render snapshot */
	cair_terminal_model_update(&panel->model);
	cair_terminal_model_get_dimensions(&panel->model, &cols, &rows);
	cair_terminal_model_get_default_colors(&panel->model, &default_fg, &default_bg);
	Rdraw_rect(painter, 0, 0, width, height, 0,
		default_bg.r / 255.0f, default_bg.g / 255.0f, default_bg.b / 255.0f, 1.0f);

	cell_w = (cols > 0) ? (width  / (float)cols) : CELL_W;
	cell_h = (rows > 0) ? (height / (float)rows) : CELL_H;

	for (row = 0; row < rows; ++row) {
		for (col = 0; col < cols; ++col) {
			CairTerminalCell cell;
			float x = col * cell_w;
			if (!cair_terminal_model_get_cell(&panel->model, row, col, &cell)) continue;
			if (cell.has_bg)
				Rdraw_rect(painter, x, row * cell_h, cell_w, cell_h, 0,
					cell.bg.r / 255.0f, cell.bg.g / 255.0f, cell.bg.b / 255.0f, 1.0f);
			if (cell.selected)
				Rdraw_rect(painter, x, row * cell_h, cell_w, cell_h, 0, 0.25f, 0.45f, 0.75f, 0.45f);
		}
		if (cair_terminal_model_read_row_text(&panel->model, row, line, line_capacity) > 0)
			Rdraw_text(painter, panel->font, CELL_H * 0.8f, 0, row * cell_h, line,
				default_fg.r / 255.0f, default_fg.g / 255.0f, default_fg.b / 255.0f, 1.0f);
	}
	free(line);

	/* Cursor */
	if (cair_terminal_model_get_cursor(&panel->model, &cursor) && cursor.visible) {
		float cx = cursor.col * cell_w;
		float cy = cursor.row * cell_h;
		Rdraw_rect_outline(painter, cx, cy, cell_w, cell_h, 0, 1.5f,
		                   FG_R, FG_G, FG_B, 0.9f);
	}
}

/* --------------------------------------------------------------------------
 * Key event handler — forward printable keys + special keys to the process
 * -------------------------------------------------------------------------- */
static int on_key(Rid widget, const REvent *ev, void *user_data) {
	CairTerminalPanel *panel = (CairTerminalPanel *)user_data;
	char buf[8];
	int len = 0;

	(void)widget;
	if (!panel || !panel->initialized) return R_FALSE;
	if (ev->type == R_EVENT_MOUSE_WHEEL) {
		int delta = (int)(-ev->wheel_y * 3.0f);
		if (delta == 0 && ev->wheel_y != 0.0f) delta = ev->wheel_y < 0.0f ? 1 : -1;
		if (delta && cair_terminal_model_scroll_viewport(&panel->model, delta)) {
			Rwidget_invalidate(panel->canvas);
			return R_TRUE;
		}
		return R_FALSE;
	}

	/* Printable text from the input method */
	if (ev->type == R_EVENT_TEXT) {
		Ruint cp = ev->codepoint;
		if (cp >= 0x20 && cp < 0x7f) {
			buf[0] = (char)cp;
			len = 1;
		}
		/* UTF-8 encode non-ASCII (passthrough for now; terminal handles it) */
		else if (cp >= 0x80 && cp <= 0x10ffff && !(cp >= 0xd800 && cp <= 0xdfff)) {
			if (cp < 0x800) {
				buf[0] = (char)(0xC0 | (cp >> 6));
				buf[1] = (char)(0x80 | (cp & 0x3F));
				len = 2;
			} else if (cp < 0x10000) {
				buf[0] = (char)(0xE0 | (cp >> 12));
				buf[1] = (char)(0x80 | ((cp >> 6) & 0x3F));
				buf[2] = (char)(0x80 | (cp & 0x3F));
				len = 3;
			} else {
				buf[0] = (char)(0xF0 | (cp >> 18));
				buf[1] = (char)(0x80 | ((cp >> 12) & 0x3F));
				buf[2] = (char)(0x80 | ((cp >> 6) & 0x3F));
				buf[3] = (char)(0x80 | (cp & 0x3F));
				len = 4;
			}
		}
		if (len > 0) {
			cair_terminal_process_write(&panel->process, buf, (size_t)len);
			return R_TRUE;
		}
		return R_FALSE;
	}

	if (ev->type != R_EVENT_KEY_DOWN) return R_FALSE;

	switch (ev->key) {
		case R_KEY_ENTER:     buf[0] = '\r';   len = 1; break;
		case R_KEY_BACKSPACE: buf[0] = '\x7f'; len = 1; break;
		case R_KEY_TAB:       buf[0] = '\t';   len = 1; break;
		case R_KEY_ESCAPE:    buf[0] = '\x1b'; len = 1; break;
		case R_KEY_UP:    buf[0]='\x1b'; buf[1]='['; buf[2]='A'; len=3; break;
		case R_KEY_DOWN:  buf[0]='\x1b'; buf[1]='['; buf[2]='B'; len=3; break;
		case R_KEY_RIGHT: buf[0]='\x1b'; buf[1]='['; buf[2]='C'; len=3; break;
		case R_KEY_LEFT:  buf[0]='\x1b'; buf[1]='['; buf[2]='D'; len=3; break;
		default:
			/* Ctrl+letter → control code (A=1 … Z=26) */
			if ((ev->modifiers & R_MOD_CTRL) && !(ev->modifiers & R_MOD_ALT)) {
				int k = (int)ev->key;
				if (k >= R_KEY_A && k <= R_KEY_Z) {
					buf[0] = (char)(k - R_KEY_A + 1);
					len = 1;
				}
			}
			break;
	}

	if (len > 0) {
		cair_terminal_process_write(&panel->process, buf, (size_t)len);
		return R_TRUE;
	}
	return R_FALSE;
}

/* --------------------------------------------------------------------------
 * Resize handler — relay terminal size when the canvas is laid out
 * -------------------------------------------------------------------------- */
static void sync_size(CairTerminalPanel *panel, float width, float height) {
	uint16_t cols, rows;
	if (!panel || !panel->initialized || width <= 0 || height <= 0) return;
	cols = (uint16_t)(width  / CELL_W);
	rows = (uint16_t)(height / CELL_H);
	if (cols < 2) cols = 2;
	if (rows < 2) rows = 2;
	cair_terminal_model_resize(&panel->model, cols, rows);
	cair_terminal_process_resize(&panel->process, cols, rows);
}

/* Raven does not have a dedicated resize callback on custom widgets, so we
 * read the widget's laid-out rect inside the paint callback and sync on first
 * paint and whenever the size changes. */
static void on_paint_with_resize(Rid widget, RPainter *painter, float width,
                                  float height, void *user_data) {
	CairTerminalPanel *panel = (CairTerminalPanel *)user_data;
	if (panel && (width != panel->last_width || height != panel->last_height)) {
		panel->last_width = width;
		panel->last_height = height;
		sync_size(panel, width, height);
	}
	on_paint(widget, painter, width, height, user_data);
}

static void close_clicked(Rid button, void *user_data) {
	(void)button;
	cair_terminal_panel_hide((CairTerminalPanel *)user_data);
}

/* --------------------------------------------------------------------------
 * Public API
 * -------------------------------------------------------------------------- */
Rid cair_terminal_panel_create(CairTerminalPanel *panel) {
	Rid title_label;
	Rid close_btn;

	if (!panel) return R_INVALID_ID;
	memset(panel, 0, sizeof(*panel));
	panel->root   = R_INVALID_ID;
	panel->header = R_INVALID_ID;
	panel->canvas = R_INVALID_ID;
	panel->font = Rfont_get_monospace();
	InitializeCriticalSection(&panel->output_lock);
	panel->lock_initialized = 1;

	/* Root: vertical stack — header bar on top, canvas below */
	panel->root = Rwidget_create(R_WIDGET_VBOX);
	if (panel->root == R_INVALID_ID) return R_INVALID_ID;
	Rpanel_set_spacing(panel->root, 0.0f);

	/* Dark theme for the whole terminal panel */
	Rwidget_set_theme(panel->root, "Dark");

	/* Header row */
	panel->header = Rwidget_create(R_WIDGET_HBOX);
	if (panel->header == R_INVALID_ID) return R_INVALID_ID;
	Rpanel_set_spacing(panel->header, 6.0f);
	Rslot_set_padding(panel->header, 6.0f, 4.0f, 6.0f, 4.0f);
	Rwidget_set_min_size(panel->header, 0.0f, HEADER_H);

	title_label = Rlabel_create("Terminal");
	if (title_label == R_INVALID_ID) return R_INVALID_ID;
	Rslot_set_fill(title_label, 1.0f);
	Rwidget_add_child(panel->header, title_label);

	close_btn = Rbutton_create("×");
	if (close_btn == R_INVALID_ID) return R_INVALID_ID;
	Rwidget_set_tooltip(close_btn, "Close terminal (Ctrl+`)");
	Rbutton_set_style(close_btn, R_BUTTON_FLAT);
	Rbutton_set_clicked_callback(close_btn, close_clicked, panel);
	Rwidget_add_child(panel->header, close_btn);
	/* Close button hides the whole panel via the toggle — wired in app.c */
	Rwidget_set_user_handle(close_btn, panel);

	Rwidget_add_child(panel->root, panel->header);

	/* Custom canvas: the VT cell grid */
	panel->canvas = Rwidget_create(R_WIDGET_CUSTOM);
	if (panel->canvas == R_INVALID_ID) return R_INVALID_ID;
	Rslot_set_fill(panel->canvas, 1.0f);
	Rwidget_set_focusable(panel->canvas, R_TRUE);
	Rwidget_set_clipping(panel->canvas, R_TRUE);
	Rcustom_set_measure_callback(panel->canvas, on_measure, panel);
	Rcustom_set_paint_callback(panel->canvas, on_paint_with_resize, panel);
	Rwidget_set_event_callback(panel->canvas, on_key, panel);
	Rwidget_add_child(panel->root, panel->canvas);

	/* Start collapsed; shown by cair_terminal_panel_show() */
	Rwidget_set_visibility(panel->root, R_COLLAPSED);

	return panel->root;
}

void cair_terminal_panel_show(CairTerminalPanel *panel) {
	uint16_t cols = TERM_COLS, rows = TERM_ROWS;
	if (!panel || panel->root == R_INVALID_ID) return;

	if (!panel->initialized) {
		if (!cair_terminal_model_init(&panel->model, cols, rows)) return;
		if (!cair_terminal_process_start(&panel->process, NULL, NULL,
		                                  cols, rows,
		                                  on_process_output, panel)) {
			cair_terminal_model_destroy(&panel->model);
			return;
		}
		panel->initialized = 1;
	}

	Rwidget_set_visibility(panel->root, R_VISIBLE);
	Rwidget_focus(panel->canvas);
}

void cair_terminal_panel_hide(CairTerminalPanel *panel) {
	if (!panel || panel->root == R_INVALID_ID) return;
	Rwidget_set_visibility(panel->root, R_COLLAPSED);
}

void cair_terminal_panel_toggle(CairTerminalPanel *panel) {
	if (!panel || panel->root == R_INVALID_ID) return;
	if (Rwidget_get_visibility(panel->root) == R_VISIBLE) {
		cair_terminal_panel_hide(panel);
	} else {
		cair_terminal_panel_show(panel);
	}
}

void cair_terminal_panel_stop(CairTerminalPanel *panel) {
	if (!panel) return;
	if (panel->initialized) {
		cair_terminal_process_destroy(&panel->process);
		cair_terminal_model_destroy(&panel->model);
		panel->initialized = 0;
	}
	if (panel->lock_initialized) {
		EnterCriticalSection(&panel->output_lock);
		free(panel->output_queue);
		panel->output_queue = NULL;
		panel->output_length = 0;
		panel->output_capacity = 0;
		LeaveCriticalSection(&panel->output_lock);
	}
}

void cair_terminal_panel_destroy(CairTerminalPanel *panel) {
	if (!panel) return;
	cair_terminal_panel_stop(panel);
	if (panel->lock_initialized) {
		DeleteCriticalSection(&panel->output_lock);
		panel->lock_initialized = 0;
	}
	panel->root   = R_INVALID_ID;
	panel->header = R_INVALID_ID;
	panel->canvas = R_INVALID_ID;
}
