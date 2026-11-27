#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "widgets/terminal_panel.h"

#include <ghostty/vt.h>
#include <limits.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/* --------------------------------------------------------------------------
 * Layout and appearance
 * -------------------------------------------------------------------------- */
#define TERM_COLS              80u
#define TERM_ROWS              24u
#define DEFAULT_FONT_SIZE      14.0f
#define RUN_BUFFER_INITIAL     4096u
#define SELECTION_R            0.25f
#define SELECTION_G            0.45f
#define SELECTION_B            0.75f
#define SELECTION_A            0.45f

#define TERM_BG_R              0.11f
#define TERM_BG_G              0.11f
#define TERM_BG_B              0.15f
#define TERM_FG_R              0.85f
#define TERM_FG_G              0.85f
#define TERM_FG_B              0.85f

/* One contiguous text run: cells sharing the same effective text style. */
typedef struct CairTerminalRun {
	int active;
	uint16_t start_col;
	uint32_t columns;
	size_t length;
	CairTerminalColor fg;
	uint16_t flags;
	int underline;
	int selected;
} CairTerminalRun;

/* --------------------------------------------------------------------------
 * Small helpers
 * -------------------------------------------------------------------------- */
static CairTerminalColor color_from_ghostty(GhosttyColorRgb color) {
	CairTerminalColor out;
	out.r = color.r;
	out.g = color.g;
	out.b = color.b;
	return out;
}

static int color_equal(CairTerminalColor a, CairTerminalColor b) {
	return a.r == b.r && a.g == b.g && a.b == b.b;
}

static float color_channel(uint8_t value) {
	return (float)value / 255.0f;
}

static void update_font_metrics(CairTerminalPanel *panel) {
	float measured_w = 0.0f;
	float measured_h = 0.0f;

	if (!panel) return;

	panel->font_size = DEFAULT_FONT_SIZE;
	panel->cell_width = 8.0f;
	panel->cell_height = 16.0f;
	panel->text_offset_y = 0.0f;

	if (panel->font != R_INVALID_ID) {
		Rfont_measure_text(panel->font, panel->font_size, "M",
		                   &measured_w, &measured_h);
		if (measured_w > 0.0f) panel->cell_width = measured_w;
		if (measured_h > 0.0f) panel->cell_height = measured_h;
	}

	/* Keep metrics sane if a backend cannot provide font measurements. */
	if (panel->cell_width < 1.0f) panel->cell_width = 8.0f;
	if (panel->cell_height < 1.0f) panel->cell_height = 16.0f;
}

static int reserve_run_buffer(CairTerminalPanel *panel, size_t needed) {
	size_t capacity;
	char *grown;

	if (!panel) return 0;
	if (needed <= panel->run_capacity) return 1;

	capacity = panel->run_capacity ? panel->run_capacity : RUN_BUFFER_INITIAL;
	while (capacity < needed && capacity <= SIZE_MAX / 2u) {
		capacity *= 2u;
	}
	if (capacity < needed) capacity = needed;

	grown = (char *)realloc(panel->run_buffer, capacity);
	if (!grown) return 0;

	panel->run_buffer = grown;
	panel->run_capacity = capacity;
	return 1;
}

/*
 * Reads the current cell's UTF-8 grapheme into panel-owned storage.
 * Ghostty reports the required byte count in buffer.len on GHOSTTY_OUT_OF_SPACE.
 * The returned pointer stays valid until the next render-state update.
 */
static const char *read_current_grapheme(CairTerminalPanel *panel,
                                         GhosttyRenderStateRowCells cells,
                                         size_t *out_length) {
	GhosttyBuffer buffer;
	GhosttyResult result;
	char *target;

	if (out_length) *out_length = 0;
	if (!panel || !cells || !out_length) return NULL;

	buffer.ptr = (uint8_t *)panel->grapheme_inline;
	buffer.cap = sizeof(panel->grapheme_inline) - 1u;
	buffer.len = 0;
	result = ghostty_render_state_row_cells_get(
		cells, GHOSTTY_RENDER_STATE_ROW_CELLS_DATA_GRAPHEMES_UTF8, &buffer);

	if (result == GHOSTTY_OUT_OF_SPACE) {
		size_t required = buffer.len;
		size_t capacity;
		char *grown;

		if (required == 0u || required >= SIZE_MAX) return NULL;
		capacity = panel->grapheme_overflow_capacity;
		if (capacity < required + 1u) {
			capacity = capacity ? capacity : sizeof(panel->grapheme_inline) * 2u;
			while (capacity < required + 1u && capacity <= SIZE_MAX / 2u) {
				capacity *= 2u;
			}
			if (capacity < required + 1u) capacity = required + 1u;
			grown = (char *)realloc(panel->grapheme_overflow, capacity);
			if (!grown) return NULL;
			panel->grapheme_overflow = grown;
			panel->grapheme_overflow_capacity = capacity;
		}

		buffer.ptr = (uint8_t *)panel->grapheme_overflow;
		buffer.cap = panel->grapheme_overflow_capacity - 1u;
		buffer.len = 0;
		result = ghostty_render_state_row_cells_get(
			cells, GHOSTTY_RENDER_STATE_ROW_CELLS_DATA_GRAPHEMES_UTF8, &buffer);
		target = panel->grapheme_overflow;
	} else {
		target = panel->grapheme_inline;
	}

	if (result != GHOSTTY_SUCCESS || buffer.len == 0u) return NULL;
	if (buffer.len > SIZE_MAX - 1u) return NULL;
	target[buffer.len] = '\0';
	*out_length = buffer.len;
	return target;
}

static int append_run_bytes(CairTerminalPanel *panel, CairTerminalRun *run,
                            const char *text, size_t length) {
	size_t required;

	if (!panel || !run || !text || length == 0u) return 0;
	if (length > SIZE_MAX - run->length - 1u) return 0;
	required = run->length + length + 1u;
	if (!reserve_run_buffer(panel, required)) return 0;

	memcpy(panel->run_buffer + run->length, text, length);
	run->length += length;
	panel->run_buffer[run->length] = '\0';
	return 1;
}

static int run_key_equal(const CairTerminalRun *run, CairTerminalColor fg,
                         uint16_t flags, int underline, int selected) {
	if (!run || !run->active) return 0;
	return color_equal(run->fg, fg) && run->flags == flags &&
	       run->underline == underline && run->selected == selected;
}

static void begin_run(CairTerminalRun *run, uint16_t col,
                      CairTerminalColor fg, uint16_t flags,
                      int underline, int selected) {
	if (!run) return;
	memset(run, 0, sizeof(*run));
	run->active = 1;
	run->start_col = col;
	run->fg = fg;
	run->flags = flags;
	run->underline = underline;
	run->selected = selected;
}

static void draw_run_decorations(RPainter *painter, float x, float y,
                                 float width, float cell_h,
                                 float thickness, CairTerminalColor fg,
                                 uint16_t flags, int underline) {
	float r = color_channel(fg.r);
	float g = color_channel(fg.g);
	float b = color_channel(fg.b);
	float alpha = (flags & GHOSTTY_STYLE_FLAG_FAINT) ? 0.65f : 1.0f;

	if (width <= 0.0f || (flags & GHOSTTY_STYLE_FLAG_INVISIBLE)) return;

	if (underline != GHOSTTY_SGR_UNDERLINE_NONE) {
		/* Curly, dotted and dashed styles are drawn as a solid line:
		   Raven's public API exposes lines, not patterned strokes. */
		float line_y = y + cell_h - thickness * 1.5f;
		Rdraw_line(painter, x, line_y, x + width, line_y,
		           thickness, r, g, b, alpha);
		if (underline == GHOSTTY_SGR_UNDERLINE_DOUBLE) {
			Rdraw_line(painter, x, line_y - thickness * 2.0f,
			           x + width, line_y - thickness * 2.0f,
			           thickness, r, g, b, alpha);
		}
	}
	if (flags & GHOSTTY_STYLE_FLAG_STRIKETHROUGH) {
		float line_y = y + cell_h * 0.55f;
		Rdraw_line(painter, x, line_y, x + width, line_y,
		           thickness, r, g, b, alpha);
	}
	if (flags & GHOSTTY_STYLE_FLAG_OVERLINE) {
		float line_y = y + thickness;
		Rdraw_line(painter, x, line_y, x + width, line_y,
		           thickness, r, g, b, alpha);
	}
}

static void flush_run(CairTerminalPanel *panel, RPainter *painter,
                      uint16_t row, CairTerminalRun *run) {
	float x;
	float y;
	float width;
	float alpha;
	float italic_offset = 0.0f;
	Rid draw_font;
	int bold;
	int italic;
	int blink_hidden;

	if (!panel || !painter || !run || !run->active) return;

	x = (float)run->start_col * panel->cell_width;
	y = (float)row * panel->cell_height + panel->text_offset_y;
	width = (float)run->columns * panel->cell_width;
	alpha = (run->flags & GHOSTTY_STYLE_FLAG_FAINT) ? 0.65f : 1.0f;
	bold = (run->flags & GHOSTTY_STYLE_FLAG_BOLD) != 0;
	italic = (run->flags & GHOSTTY_STYLE_FLAG_ITALIC) != 0;
	blink_hidden = (run->flags & GHOSTTY_STYLE_FLAG_BLINK) && !panel->blink_visible;

	if (run->length > 0u && width > 0.0f &&
	    !(run->flags & GHOSTTY_STYLE_FLAG_INVISIBLE) && !blink_hidden) {
		panel->run_buffer[run->length] = '\0';
		draw_font = panel->font;
		if (bold && italic && panel->font_bold_italic != R_INVALID_ID) {
			draw_font = panel->font_bold_italic;
		} else if (bold && panel->font_bold != R_INVALID_ID) {
			draw_font = panel->font_bold;
		} else if (italic && panel->font_italic != R_INVALID_ID) {
			draw_font = panel->font_italic;
		}
		/* Only reached when Raven could not derive a variant. */
		if (italic && draw_font == panel->font) {
			italic_offset = panel->font_size / 6.0f;
		}

		Rdraw_text(painter, draw_font, panel->font_size,
		           x + italic_offset, y, panel->run_buffer,
		           color_channel(run->fg.r), color_channel(run->fg.g),
		           color_channel(run->fg.b), alpha);

		if (bold && draw_font == panel->font) {
			Rdraw_text(painter, panel->font, panel->font_size,
			           x + italic_offset + 1.0f, y, panel->run_buffer,
			           color_channel(run->fg.r), color_channel(run->fg.g),
			           color_channel(run->fg.b), alpha);
		}
	}

	if (!blink_hidden) {
		float thickness = panel->font_size / 14.0f;
		if (thickness < 1.0f) thickness = 1.0f;
		draw_run_decorations(painter, x, (float)row * panel->cell_height,
		                     width, panel->cell_height, thickness,
		                     run->fg, run->flags, run->underline);
	}

	run->active = 0;
	run->columns = 0u;
	run->length = 0u;
}

/* --------------------------------------------------------------------------
 * Cursor and scrollback indicator
 * -------------------------------------------------------------------------- */
static void draw_cursor(CairTerminalPanel *panel, RPainter *painter,
                        float grid_width, float grid_height) {
	GhosttyRenderState render_state;
	GhosttyCursorStyle cursor_style = GHOSTTY_CURSOR_STYLE_BLOCK;
	GhosttyColorRgb ghostty_cursor;
	CairTerminalColor cursor_color;
	CairTerminalCursor cursor;
	bool cursor_color_valid = false;
	bool wide_tail = false;
	uint32_t col;
	float x, y, w, h, thickness;

	if (!panel || !painter || !panel->initialized) return;
	if (!cair_terminal_model_get_cursor(&panel->model, &cursor) ||
	    !cursor.visible || (cursor.blinking && !panel->blink_visible)) {
		return;
	}

	render_state = (GhosttyRenderState)panel->model.render_state;
	if (!render_state) return;
	(void)ghostty_render_state_get(render_state,
		GHOSTTY_RENDER_STATE_DATA_CURSOR_VISUAL_STYLE, &cursor_style);
	(void)ghostty_render_state_get(render_state,
		GHOSTTY_RENDER_STATE_DATA_CURSOR_VIEWPORT_WIDE_TAIL, &wide_tail);
	(void)ghostty_render_state_get(render_state,
		GHOSTTY_RENDER_STATE_DATA_COLOR_CURSOR_HAS_VALUE, &cursor_color_valid);

	if (cursor_color_valid &&
	    ghostty_render_state_get(render_state,
	        GHOSTTY_RENDER_STATE_DATA_COLOR_CURSOR, &ghostty_cursor) == GHOSTTY_SUCCESS) {
		cursor_color = color_from_ghostty(ghostty_cursor);
	} else if (!cair_terminal_model_get_default_colors(&panel->model, &cursor_color, NULL)) {
		cursor_color.r = 204;
		cursor_color.g = 204;
		cursor_color.b = 204;
	}

	col = (uint32_t)cursor.col + (uint32_t)panel->input_columns;
	w = panel->cell_width;
	if (wide_tail && panel->input_columns == 0u && col > 0u) {
		--col;
		w *= 2.0f;
	}
	if (col >= (uint32_t)panel->model.cols ||
	    cursor.row >= panel->model.rows) return;

	x = (float)col * panel->cell_width;
	y = (float)cursor.row * panel->cell_height;
	h = panel->cell_height;
	if (x >= grid_width || y >= grid_height) return;
	if (x + w > grid_width) w = grid_width - x;
	if (y + h > grid_height) h = grid_height - y;
	if (w <= 0.0f || h <= 0.0f) return;

	thickness = panel->font_size / 8.0f;
	if (thickness < 1.0f) thickness = 1.0f;

	switch (cursor_style) {
		case GHOSTTY_CURSOR_STYLE_BAR:
			Rdraw_rect(painter, x, y, thickness, h, 0.0f,
			           color_channel(cursor_color.r),
			           color_channel(cursor_color.g),
			           color_channel(cursor_color.b), 0.95f);
			break;
		case GHOSTTY_CURSOR_STYLE_UNDERLINE:
			Rdraw_rect(painter, x, y + h - thickness, w, thickness, 0.0f,
			           color_channel(cursor_color.r),
			           color_channel(cursor_color.g),
			           color_channel(cursor_color.b), 0.95f);
			break;
		case GHOSTTY_CURSOR_STYLE_BLOCK_HOLLOW:
			Rdraw_rect_outline(painter, x, y, w, h, 0.0f, thickness,
			                   color_channel(cursor_color.r),
			                   color_channel(cursor_color.g),
			                   color_channel(cursor_color.b), 0.95f);
			break;
		case GHOSTTY_CURSOR_STYLE_BLOCK:
		default:
			Rdraw_rect(painter, x, y, w, h, 0.0f,
			           color_channel(cursor_color.r),
			           color_channel(cursor_color.g),
			           color_channel(cursor_color.b), 0.30f);
			Rdraw_rect_outline(painter, x, y, w, h, 0.0f, 1.0f,
			                   color_channel(cursor_color.r),
			                   color_channel(cursor_color.g),
			                   color_channel(cursor_color.b), 0.85f);
			break;
	}
}

static void draw_scrollbar(CairTerminalPanel *panel, RPainter *painter,
                           float width, float height) {
	GhosttyTerminalScrollbar scrollbar;
	GhosttyTerminal terminal;
	float track_x;
	float thumb_h;
	float thumb_y;
	double scrollable;
	double fraction;

	if (!panel || !painter || !panel->initialized) return;
	terminal = (GhosttyTerminal)panel->model.terminal;
	if (!terminal) return;
	memset(&scrollbar, 0, sizeof(scrollbar));
	if (ghostty_terminal_get(terminal, GHOSTTY_TERMINAL_DATA_SCROLLBAR,
	                         &scrollbar) != GHOSTTY_SUCCESS) {
		return;
	}
	if (scrollbar.total == 0u || scrollbar.len == 0u ||
	    scrollbar.total <= scrollbar.len ||
	    height < 12.0f || width < 12.0f) {
		return;
	}

	track_x = width - 7.0f;
	if (track_x < 0.0f) track_x = 0.0f;
	Rdraw_rect(painter, track_x, 2.0f, 4.0f, height - 4.0f,
	           2.0f, 0.65f, 0.68f, 0.72f, 0.12f);

	thumb_h = height * ((float)scrollbar.len / (float)scrollbar.total);
	if (thumb_h < 12.0f) thumb_h = 12.0f;
	if (thumb_h > height) thumb_h = height;
	scrollable = (double)(scrollbar.total - scrollbar.len);
	fraction = scrollable > 0.0 ? (double)scrollbar.offset / scrollable : 1.0;
	if (fraction < 0.0) fraction = 0.0;
	if (fraction > 1.0) fraction = 1.0;
	thumb_y = (float)(fraction * (double)(height - thumb_h));

	Rdraw_rect(painter, track_x, thumb_y, 4.0f, thumb_h,
	           2.0f, 0.78f, 0.80f, 0.84f, 0.58f);
}

/* --------------------------------------------------------------------------
 * Single-pass Ghostty -> Raven cell grid renderer
 * -------------------------------------------------------------------------- */
static void render_terminal_grid(CairTerminalPanel *panel, RPainter *painter,
                                 float width, float height) {
	GhosttyRenderState render_state;
	GhosttyRenderStateRowIterator row_iter;
	GhosttyRenderStateRowCells row_cells;
	CairTerminalColor default_fg;
	CairTerminalColor default_bg;
	uint16_t model_cols = 0;
	uint16_t model_rows = 0;
	uint16_t row_index = 0;
	float grid_width;
	float grid_height;

	if (!panel || !painter || !panel->initialized) return;
	if (!cair_terminal_model_get_dimensions(&panel->model, &model_cols, &model_rows)) return;

	if (!cair_terminal_model_get_default_colors(&panel->model, &default_fg, &default_bg)) {
		default_fg.r = 204; default_fg.g = 204; default_fg.b = 204;
		default_bg.r = 30;  default_bg.g = 30;  default_bg.b = 32;
	}

	render_state = (GhosttyRenderState)panel->model.render_state;
	row_iter = (GhosttyRenderStateRowIterator)panel->model.row_iter;
	row_cells = (GhosttyRenderStateRowCells)panel->model.row_cells;
	if (!render_state || !row_iter || !row_cells) return;

	grid_width = (float)model_cols * panel->cell_width;
	grid_height = (float)model_rows * panel->cell_height;
	if (grid_width > width) grid_width = width;
	if (grid_height > height) grid_height = height;
	if (grid_width < 0.0f) grid_width = 0.0f;
	if (grid_height < 0.0f) grid_height = 0.0f;

	Rdraw_rect(painter, 0.0f, 0.0f, width, height, 0.0f,
	           color_channel(default_bg.r), color_channel(default_bg.g),
	           color_channel(default_bg.b), 1.0f);
	if (grid_width <= 0.0f || grid_height <= 0.0f) return;

	Rdraw_push_clip_rect(painter, 0.0f, 0.0f, grid_width, grid_height);

	panel->has_blink_cells = 0;
	if (ghostty_render_state_get(render_state,
	        GHOSTTY_RENDER_STATE_DATA_ROW_ITERATOR, &row_iter) == GHOSTTY_SUCCESS) {
		while (ghostty_render_state_row_iterator_next(row_iter) &&
		       row_index < model_rows) {
			CairTerminalRun run;
			uint32_t col = 0u;
			float row_y = (float)row_index * panel->cell_height;

			memset(&run, 0, sizeof(run));
			if (ghostty_render_state_row_get(row_iter,
			        GHOSTTY_RENDER_STATE_ROW_DATA_CELLS, &row_cells) != GHOSTTY_SUCCESS) {
				++row_index;
				continue;
			}

			while (ghostty_render_state_row_cells_next(row_cells) &&
			       col < (uint32_t)model_cols) {
				GhosttyCell raw_cell = 0;
				GhosttyCellWide wide = GHOSTTY_CELL_WIDE_NARROW;
				GhosttyStyle style;
				GhosttyColorRgb ghostty_fg;
				GhosttyColorRgb ghostty_bg;
				CairTerminalColor fg = default_fg;
				CairTerminalColor bg = default_bg;
				bool selected = false;
				bool has_bg = false;
				uint16_t flags;
				int underline;
				uint32_t advance = 1u;
				uint32_t draw_columns;
				float cell_x;
				float draw_width;
				size_t grapheme_length = 0u;
				const char *grapheme;
				static const char space = ' ';

				(void)ghostty_render_state_row_cells_get(row_cells,
					GHOSTTY_RENDER_STATE_ROW_CELLS_DATA_RAW, &raw_cell);
				if (raw_cell != 0 &&
				    ghostty_cell_get(raw_cell, GHOSTTY_CELL_DATA_WIDE, &wide) == GHOSTTY_SUCCESS) {
					if (wide == GHOSTTY_CELL_WIDE_SPACER_TAIL) {
						/* The wide base cell already advanced x by two columns. */
						continue;
					}
					if (wide == GHOSTTY_CELL_WIDE_WIDE) advance = 2u;
				}

				ghostty_style_default(&style);
				(void)ghostty_render_state_row_cells_get(row_cells,
					GHOSTTY_RENDER_STATE_ROW_CELLS_DATA_STYLE, &style);
				flags = style.flags;
				underline = GHOSTTY_STYLE_UNDERLINE(style);
				if (flags & GHOSTTY_STYLE_FLAG_BLINK) panel->has_blink_cells = 1;

				if (ghostty_render_state_row_cells_get(row_cells,
				        GHOSTTY_RENDER_STATE_ROW_CELLS_DATA_FG_COLOR,
				        &ghostty_fg) == GHOSTTY_SUCCESS) {
					fg = color_from_ghostty(ghostty_fg);
				}
				if (ghostty_render_state_row_cells_get(row_cells,
				        GHOSTTY_RENDER_STATE_ROW_CELLS_DATA_BG_COLOR,
				        &ghostty_bg) == GHOSTTY_SUCCESS) {
					bg = color_from_ghostty(ghostty_bg);
					has_bg = 1;
				}
				(void)ghostty_render_state_row_cells_get(row_cells,
					GHOSTTY_RENDER_STATE_ROW_CELLS_DATA_SELECTED, &selected);

				if (flags & GHOSTTY_STYLE_FLAG_INVERSE) {
					CairTerminalColor swap = fg;
					fg = bg;
					bg = swap;
					has_bg = 1;
				}

				cell_x = (float)col * panel->cell_width;
				draw_columns = advance;
				if (col + draw_columns > (uint32_t)model_cols) {
					draw_columns = (uint32_t)model_cols - col;
				}
				draw_width = (float)draw_columns * panel->cell_width;

				/* Backgrounds and selection are painted before any glyph. */
				if (has_bg) {
					Rdraw_rect(painter, cell_x, row_y, draw_width,
					           panel->cell_height, 0.0f,
					           color_channel(bg.r), color_channel(bg.g),
					           color_channel(bg.b), 1.0f);
				}
				if (selected) {
					Rdraw_rect(painter, cell_x, row_y, draw_width,
					           panel->cell_height, 0.0f,
					           SELECTION_R, SELECTION_G, SELECTION_B, SELECTION_A);
				}

				if (!run_key_equal(&run, fg, flags, underline, selected)) {
					flush_run(panel, painter, row_index, &run);
					begin_run(&run, (uint16_t)col, fg, flags, underline, selected);
				}

				if (wide == GHOSTTY_CELL_WIDE_SPACER_HEAD) {
					/* One blank column at the end of a soft-wrapped row. */
					if (append_run_bytes(panel, &run, &space, 1u)) {
						run.columns += 1u;
					} else {
						flush_run(panel, painter, row_index, &run);
					}
					++col;
					continue;
				}

				grapheme = read_current_grapheme(panel, row_cells, &grapheme_length);
				if (advance == 2u) {
					/*
					 * Draw the wide glyph in a run of its own: its font advance is
					 * not guaranteed to be exactly two cell widths, and letting it
					 * share a run would shift every later glyph.
					 */
					flush_run(panel, painter, row_index, &run);
					begin_run(&run, (uint16_t)col, fg, flags, underline, selected);
					if (grapheme && grapheme_length > 0u) {
						(void)append_run_bytes(panel, &run, grapheme, grapheme_length);
					} else {
						(void)append_run_bytes(panel, &run, &space, 1u);
					}
					run.columns = draw_columns;
					flush_run(panel, painter, row_index, &run);
					col += advance;
					continue;
				}

				if (grapheme && grapheme_length > 0u) {
					if (!append_run_bytes(panel, &run, grapheme, grapheme_length)) {
						flush_run(panel, painter, row_index, &run);
						col += advance;
						continue;
					}
				} else if (!append_run_bytes(panel, &run, &space, 1u)) {
					flush_run(panel, painter, row_index, &run);
					col += advance;
					continue;
				}

				run.columns += 1u;
				col += advance;
			}

			flush_run(panel, painter, row_index, &run);
			++row_index;
		}
	}

	draw_cursor(panel, painter, grid_width, grid_height);
	Rdraw_pop_clip_rect(painter);
	draw_scrollbar(panel, painter, width, height);
}

/* --------------------------------------------------------------------------
 * Process output queue — the reader thread only appends and posts a task
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
	panel->output_length = 0u;
	panel->output_capacity = 0u;
	panel->output_task_posted = 0;
	LeaveCriticalSection(&panel->output_lock);

	if (bytes) {
		if (panel->initialized && length > 0u) {
			(void)cair_terminal_model_write(&panel->model, bytes, length);
		}
		free(bytes);
	}
	/* After stop() the widgets may already be gone (Rshutdown drains tasks
	   after destroying the windows), so never touch a stale widget handle. */
	if (panel->initialized && panel->canvas != R_INVALID_ID) {
		Rwidget_invalidate(panel->canvas);
	}
}

static void on_process_output(const char *data, size_t len, void *user_data) {
	CairTerminalPanel *panel = (CairTerminalPanel *)user_data;
	int post_task = 0;

	if (!panel || !data || len == 0u || !panel->lock_initialized) return;

	EnterCriticalSection(&panel->output_lock);
	if (len <= SIZE_MAX - panel->output_length) {
		size_t required = panel->output_length + len;
		if (required > panel->output_capacity) {
			size_t capacity = panel->output_capacity ? panel->output_capacity : 4096u;
			char *grown;
			while (capacity < required && capacity <= SIZE_MAX / 2u) {
				capacity *= 2u;
			}
			if (capacity < required) capacity = required;
			grown = (char *)realloc(panel->output_queue, capacity);
			if (grown) {
				panel->output_queue = grown;
				panel->output_capacity = capacity;
			} else {
				len = 0u;
			}
		}
	} else {
		len = 0u;
	}

	if (len > 0u && panel->output_queue) {
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
 * Existing line-oriented input path (unchanged by the renderer port)
 * -------------------------------------------------------------------------- */
static int append_input(CairTerminalPanel *panel, const char *text,
                        size_t length, uint16_t columns) {
	if (!panel || !text || length == 0u ||
	    length >= sizeof(panel->input_line) - panel->input_length) {
		return 0;
	}

	memcpy(panel->input_line + panel->input_length, text, length);
	panel->input_length += length;
	panel->input_columns = (uint16_t)(panel->input_columns + columns);
	panel->input_line[panel->input_length] = '\0';
	Rwidget_invalidate(panel->canvas);
	return 1;
}

static int submit_input(CairTerminalPanel *panel) {
	char command[sizeof(panel->input_line) + 2u];
	size_t length;

	if (!panel) return 0;
	memcpy(command, panel->input_line, panel->input_length);
	command[panel->input_length] = '\r';
	command[panel->input_length + 1u] = '\n';
	length = panel->input_length + 2u;

	if (cair_terminal_process_write(&panel->process, command, length) != (int)length) {
		return 0;
	}

	panel->input_length = 0u;
	panel->input_columns = 0u;
	panel->input_line[0] = '\0';
	Rwidget_invalidate(panel->canvas);
	return 1;
}

/* --------------------------------------------------------------------------
 * Raven custom-widget callbacks
 * -------------------------------------------------------------------------- */
static void on_measure(Rid widget, float *width, float *height, void *user_data) {
	CairTerminalPanel *panel = (CairTerminalPanel *)user_data;
	(void)widget;

	if (!width || !height) return;
	if (!panel) {
		*width = (float)TERM_COLS * 8.0f;
		*height = (float)TERM_ROWS * 16.0f;
		return;
	}

	*width = (float)TERM_COLS * panel->cell_width;
	*height = (float)TERM_ROWS * panel->cell_height;
}

static void sync_size(CairTerminalPanel *panel, float width, float height) {
	uint16_t cols;
	uint16_t rows;
	uint16_t old_cols;
	uint16_t old_rows;
	float cw;
	float ch;

	if (!panel || !panel->initialized || width <= 0.0f || height <= 0.0f) return;

	cw = panel->cell_width > 0.0f ? panel->cell_width : 8.0f;
	ch = panel->cell_height > 0.0f ? panel->cell_height : 16.0f;
	if (width / cw >= (float)UINT16_MAX) cols = UINT16_MAX;
	else cols = (uint16_t)(width / cw);
	if (height / ch >= (float)UINT16_MAX) rows = UINT16_MAX;
	else rows = (uint16_t)(height / ch);
	if (cols < 2u) cols = 2u;
	if (rows < 2u) rows = 2u;

	if (!cair_terminal_model_get_dimensions(&panel->model, &old_cols, &old_rows)) {
		return;
	}
	if (cols == old_cols && rows == old_rows) return;

	if (cair_terminal_model_resize(&panel->model, cols, rows)) {
		(void)cair_terminal_process_resize(&panel->process, cols, rows);
	}
}

static void on_paint(Rid widget, RPainter *painter, float width,
                     float height, void *user_data) {
	CairTerminalPanel *panel = (CairTerminalPanel *)user_data;
	CairTerminalCursor cursor;
	CairTerminalColor default_fg;

	(void)widget;
	if (!painter) return;

	/* A missing panel is a harmless empty surface; never dereference it. */
	if (!panel) {
		Rdraw_rect(painter, 0.0f, 0.0f, width, height, 0.0f,
		           TERM_BG_R, TERM_BG_G, TERM_BG_B, 1.0f);
		return;
	}

	panel->paint_count++;

	if (!panel->initialized) {
		Rdraw_rect(painter, 0.0f, 0.0f, width, height, 0.0f,
		           TERM_BG_R, TERM_BG_G, TERM_BG_B, 1.0f);
		Rdraw_text(painter, panel->font, panel->font_size,
		           8.0f, height * 0.5f - panel->cell_height * 0.5f,
		           "Terminal — press Ctrl+` to open",
		           TERM_FG_R, TERM_FG_G, TERM_FG_B, 0.55f);
		return;
	}

	render_terminal_grid(panel, painter, width, height);

	/* Preserve the existing local line-editor echo overlay. */
	if (panel->input_length > 0u &&
	    cair_terminal_model_get_cursor(&panel->model, &cursor) &&
	    cair_terminal_model_get_default_colors(&panel->model, &default_fg, NULL)) {
		Rdraw_text(painter, panel->font, panel->font_size,
		           (float)cursor.col * panel->cell_width,
		           (float)cursor.row * panel->cell_height + panel->text_offset_y,
		           panel->input_line,
		           color_channel(default_fg.r), color_channel(default_fg.g),
		           color_channel(default_fg.b), 1.0f);
	}
}

static void on_paint_with_resize(Rid widget, RPainter *painter,
                                 float width, float height, void *user_data) {
	CairTerminalPanel *panel = (CairTerminalPanel *)user_data;

	if (panel && (width != panel->last_width || height != panel->last_height)) {
		panel->last_width = width;
		panel->last_height = height;
		sync_size(panel, width, height);
	}
	on_paint(widget, painter, width, height, user_data);
}

static int on_key(Rid widget, const REvent *event, void *user_data) {
	CairTerminalPanel *panel = (CairTerminalPanel *)user_data;
	char utf8[4];
	int length = 0;

	(void)widget;
	if (!panel || !panel->initialized || !event) return R_FALSE;

	if (event->type == R_EVENT_MOUSE_DOWN && event->button == R_MOUSE_LEFT) {
		Rwidget_focus(panel->canvas);
		return R_FALSE;
	}
	if (event->type == R_EVENT_MOUSE_WHEEL) {
		int delta = (int)(-event->wheel_y * 3.0f);
		if (delta == 0 && event->wheel_y != 0.0f) {
			delta = event->wheel_y < 0.0f ? 1 : -1;
		}
		if (delta && cair_terminal_model_scroll_viewport(&panel->model, delta)) {
			Rwidget_invalidate(panel->canvas);
			return R_TRUE;
		}
		return R_FALSE;
	}

	if (event->type == R_EVENT_TEXT) {
		Ruint cp = event->codepoint;
		if (cp >= 0x20u && cp < 0x7fu) {
			utf8[0] = (char)cp;
			length = 1;
		} else if (cp >= 0x80u && cp <= 0x10ffffu &&
		           !(cp >= 0xd800u && cp <= 0xdfffu)) {
			if (cp < 0x800u) {
				utf8[0] = (char)(0xc0u | (cp >> 6));
				utf8[1] = (char)(0x80u | (cp & 0x3fu));
				length = 2;
			} else if (cp < 0x10000u) {
				utf8[0] = (char)(0xe0u | (cp >> 12));
				utf8[1] = (char)(0x80u | ((cp >> 6) & 0x3fu));
				utf8[2] = (char)(0x80u | (cp & 0x3fu));
				length = 3;
			} else {
				utf8[0] = (char)(0xf0u | (cp >> 18));
				utf8[1] = (char)(0x80u | ((cp >> 12) & 0x3fu));
				utf8[2] = (char)(0x80u | ((cp >> 6) & 0x3fu));
				utf8[3] = (char)(0x80u | (cp & 0x3fu));
				length = 4;
			}
		}
		return length > 0 ?
			(append_input(panel, utf8, (size_t)length, 1u) ? R_TRUE : R_FALSE) :
			R_FALSE;
	}

	if (event->type != R_EVENT_KEY_DOWN) return R_FALSE;

	switch (event->key) {
		case R_KEY_ENTER:
			return submit_input(panel) ? R_TRUE : R_FALSE;
		case R_KEY_BACKSPACE:
			if (panel->input_length > 0u) {
				--panel->input_length;
				while (panel->input_length > 0u &&
				       (((unsigned char)panel->input_line[panel->input_length] & 0xc0u) == 0x80u)) {
					--panel->input_length;
				}
				panel->input_line[panel->input_length] = '\0';
				if (panel->input_columns > 0u) --panel->input_columns;
				Rwidget_invalidate(panel->canvas);
				return R_TRUE;
			}
			return R_FALSE;
		case R_KEY_TAB:
			return append_input(panel, "    ", 4u, 4u) ? R_TRUE : R_FALSE;
		case R_KEY_ESCAPE:
			panel->input_length = 0u;
			panel->input_columns = 0u;
			panel->input_line[0] = '\0';
			Rwidget_invalidate(panel->canvas);
			return R_TRUE;
		case R_KEY_UP:
		case R_KEY_DOWN:
		case R_KEY_RIGHT:
		case R_KEY_LEFT:
			return R_FALSE;
		default:
			if ((event->modifiers & R_MOD_CTRL) && !(event->modifiers & R_MOD_ALT)) {
				int key = (int)event->key;
				if (key >= R_KEY_A && key <= R_KEY_Z && event->key == R_KEY_C) {
					panel->input_length = 0u;
					panel->input_columns = 0u;
					panel->input_line[0] = '\0';
					Rwidget_invalidate(panel->canvas);
					return R_TRUE;
				}
			}
			break;
	}

	return R_FALSE;
}

/* --------------------------------------------------------------------------
 * Public panel API
 * -------------------------------------------------------------------------- */
Rid cair_terminal_panel_create(CairTerminalPanel *panel) {
	if (!panel) return R_INVALID_ID;
	memset(panel, 0, sizeof(*panel));
	panel->root = R_INVALID_ID;
	panel->canvas = R_INVALID_ID;
	panel->font = Rfont_get_monospace();
	panel->font_bold = R_INVALID_ID;
	panel->font_italic = R_INVALID_ID;
	panel->font_bold_italic = R_INVALID_ID;
	panel->blink_visible = 1;
	panel->last_blink_tick = GetTickCount64();

	update_font_metrics(panel);
	if (panel->font != R_INVALID_ID) {
		panel->font_bold = Rfont_get_variant(panel->font, R_FONT_BOLD);
		panel->font_italic = Rfont_get_variant(panel->font, R_FONT_ITALIC);
		panel->font_bold_italic = Rfont_get_variant(panel->font, R_FONT_BOLD_ITALIC);
	}

	InitializeCriticalSection(&panel->output_lock);
	panel->lock_initialized = 1;
	(void)reserve_run_buffer(panel, RUN_BUFFER_INITIAL);

	panel->root = Rwidget_create(R_WIDGET_VBOX);
	if (panel->root == R_INVALID_ID) goto fail;
	Rpanel_set_spacing(panel->root, 0.0f);
	Rwidget_set_theme(panel->root, "Dark");

	panel->canvas = Rwidget_create(R_WIDGET_CUSTOM);
	if (panel->canvas == R_INVALID_ID) goto fail;
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

fail:
	if (panel->root != R_INVALID_ID) Rwidget_destroy(panel->root);
	panel->root = R_INVALID_ID;
	panel->canvas = R_INVALID_ID;
	free(panel->run_buffer);
	panel->run_buffer = NULL;
	panel->run_capacity = 0u;
	free(panel->grapheme_overflow);
	panel->grapheme_overflow = NULL;
	panel->grapheme_overflow_capacity = 0u;
	if (panel->lock_initialized) {
		DeleteCriticalSection(&panel->output_lock);
		panel->lock_initialized = 0;
	}
	return R_INVALID_ID;
}

void cair_terminal_panel_show(CairTerminalPanel *panel) {
	uint16_t cols = (uint16_t)TERM_COLS;
	uint16_t rows = (uint16_t)TERM_ROWS;

	if (!panel || panel->root == R_INVALID_ID) return;

	if (!panel->initialized) {
		if (!cair_terminal_model_init(&panel->model, cols, rows)) return;
		if (!cair_terminal_process_start(&panel->process, NULL, NULL,
		                                 cols, rows, on_process_output, panel)) {
			cair_terminal_model_destroy(&panel->model);
			return;
		}
		panel->initialized = 1;
		panel->last_width = 0.0f;
		panel->last_height = 0.0f;
		panel->blink_visible = 1;
		panel->last_blink_tick = GetTickCount64();
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

void cair_terminal_panel_tick(CairTerminalPanel *panel) {
	uint64_t now;
	int needs_blink = 0;
	CairTerminalCursor cursor;

	if (!panel || !panel->initialized || panel->canvas == R_INVALID_ID) return;
	if (panel->has_blink_cells) needs_blink = 1;
	if (cair_terminal_model_get_cursor(&panel->model, &cursor) && cursor.blinking) {
		needs_blink = 1;
	}
	if (!needs_blink) return;

	now = GetTickCount64();
	if (now - panel->last_blink_tick >= 500u) {
		panel->last_blink_tick = now;
		panel->blink_visible = !panel->blink_visible;
		Rwidget_invalidate(panel->canvas);
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
		panel->output_length = 0u;
		panel->output_capacity = 0u;
		panel->output_task_posted = 0;
		LeaveCriticalSection(&panel->output_lock);
	}

	panel->has_blink_cells = 0;
	panel->last_width = 0.0f;
	panel->last_height = 0.0f;
	panel->input_length = 0u;
	panel->input_columns = 0u;
	panel->input_line[0] = '\0';
}

void cair_terminal_panel_destroy(CairTerminalPanel *panel) {
	if (!panel) return;

	cair_terminal_panel_stop(panel);

	free(panel->run_buffer);
	panel->run_buffer = NULL;
	panel->run_capacity = 0u;
	free(panel->grapheme_overflow);
	panel->grapheme_overflow = NULL;
	panel->grapheme_overflow_capacity = 0u;

	if (panel->lock_initialized) {
		DeleteCriticalSection(&panel->output_lock);
		panel->lock_initialized = 0;
	}
	panel->root = R_INVALID_ID;
	panel->canvas = R_INVALID_ID;
}
