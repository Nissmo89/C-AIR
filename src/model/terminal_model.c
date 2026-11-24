#include "terminal_model.h"

#include <ghostty/vt.h>
#include <stdlib.h>
#include <string.h>

int cair_terminal_model_init(CairTerminalModel *model, uint16_t cols, uint16_t rows) {
	GhosttyTerminal term = NULL;
	GhosttyRenderState rstate = NULL;
	GhosttyRenderStateRowIterator row_iter = NULL;
	GhosttyRenderStateRowCells row_cells = NULL;

	if (!model) return 0;
	memset(model, 0, sizeof(*model));
	if (cols == 0 || rows == 0) return 0;

	if (ghostty_terminal_new(NULL, &term, cols, rows) != GHOSTTY_SUCCESS) {
		return 0;
	}
	if (ghostty_render_state_new(NULL, &rstate) != GHOSTTY_SUCCESS) {
		ghostty_terminal_free(term);
		return 0;
	}
	if (ghostty_render_state_row_iterator_new(NULL, &row_iter) != GHOSTTY_SUCCESS) {
		ghostty_render_state_free(rstate);
		ghostty_terminal_free(term);
		return 0;
	}
	if (ghostty_render_state_row_cells_new(NULL, &row_cells) != GHOSTTY_SUCCESS) {
		ghostty_render_state_row_iterator_free(row_iter);
		ghostty_render_state_free(rstate);
		ghostty_terminal_free(term);
		return 0;
	}

	model->terminal = (void*)term;
	model->render_state = (void*)rstate;
	model->row_iter = (void*)row_iter;
	model->row_cells = (void*)row_cells;
	model->cols = cols;
	model->rows = rows;
	model->initialized = 1;

	cair_terminal_model_update(model);
	return 1;
}

void cair_terminal_model_destroy(CairTerminalModel *model) {
	if (!model || !model->initialized) return;

	if (model->row_cells) {
		ghostty_render_state_row_cells_free((GhosttyRenderStateRowCells)model->row_cells);
		model->row_cells = NULL;
	}
	if (model->row_iter) {
		ghostty_render_state_row_iterator_free((GhosttyRenderStateRowIterator)model->row_iter);
		model->row_iter = NULL;
	}
	if (model->render_state) {
		ghostty_render_state_free((GhosttyRenderState)model->render_state);
		model->render_state = NULL;
	}
	if (model->terminal) {
		ghostty_terminal_free((GhosttyTerminal)model->terminal);
		model->terminal = NULL;
	}

	model->cols = 0;
	model->rows = 0;
	model->initialized = 0;
}

int cair_terminal_model_write(CairTerminalModel *model, const char *data, size_t len) {
	if (!model || !model->initialized || !data || len == 0) return 0;
	ghostty_terminal_vt_write((GhosttyTerminal)model->terminal, (const uint8_t*)data, len);
	return cair_terminal_model_update(model);
}

int cair_terminal_model_resize(CairTerminalModel *model, uint16_t cols, uint16_t rows) {
	if (!model || !model->initialized || cols == 0 || rows == 0) return 0;
	if (ghostty_terminal_resize((GhosttyTerminal)model->terminal, cols, rows, 0, 0) != GHOSTTY_SUCCESS) {
		return 0;
	}
	model->cols = cols;
	model->rows = rows;
	return cair_terminal_model_update(model);
}

int cair_terminal_model_update(CairTerminalModel *model) {
	if (!model || !model->initialized) return 0;
	return ghostty_render_state_update(
		(GhosttyRenderState)model->render_state,
		(GhosttyTerminal)model->terminal) == GHOSTTY_SUCCESS;
}

int cair_terminal_model_get_dimensions(const CairTerminalModel *model, uint16_t *cols, uint16_t *rows) {
	if (!model || !model->initialized) return 0;
	if (cols) *cols = model->cols;
	if (rows) *rows = model->rows;
	return 1;
}

int cair_terminal_model_get_cursor(const CairTerminalModel *model, CairTerminalCursor *cursor) {
	GhosttyRenderState rstate;
	bool has_val = false;
	bool vis = false;
	bool blink = false;
	uint16_t vx = 0;
	uint16_t vy = 0;

	if (!model || !model->initialized || !cursor) return 0;
	memset(cursor, 0, sizeof(*cursor));
	rstate = (GhosttyRenderState)model->render_state;

	ghostty_render_state_get(rstate, GHOSTTY_RENDER_STATE_DATA_CURSOR_VIEWPORT_HAS_VALUE, &has_val);
	ghostty_render_state_get(rstate, GHOSTTY_RENDER_STATE_DATA_CURSOR_VISIBLE, &vis);
	ghostty_render_state_get(rstate, GHOSTTY_RENDER_STATE_DATA_CURSOR_BLINKING, &blink);

	cursor->visible = (has_val && vis) ? 1 : 0;
	cursor->blinking = blink ? 1 : 0;

	if (has_val) {
		ghostty_render_state_get(rstate, GHOSTTY_RENDER_STATE_DATA_CURSOR_VIEWPORT_X, &vx);
		ghostty_render_state_get(rstate, GHOSTTY_RENDER_STATE_DATA_CURSOR_VIEWPORT_Y, &vy);
		cursor->col = vx;
		cursor->row = vy;
		return 1;
	}
	return 0;
}

int cair_terminal_model_get_default_colors(const CairTerminalModel *model, CairTerminalColor *fg, CairTerminalColor *bg) {
	GhosttyRenderState rstate;
	GhosttyColorRgb gfg, gbg;

	if (!model || !model->initialized) return 0;
	rstate = (GhosttyRenderState)model->render_state;

	if (fg) {
		if (ghostty_render_state_get(rstate, GHOSTTY_RENDER_STATE_DATA_COLOR_FOREGROUND, &gfg) == GHOSTTY_SUCCESS) {
			fg->r = gfg.r;
			fg->g = gfg.g;
			fg->b = gfg.b;
		} else {
			fg->r = 204; fg->g = 204; fg->b = 204;
		}
	}
	if (bg) {
		if (ghostty_render_state_get(rstate, GHOSTTY_RENDER_STATE_DATA_COLOR_BACKGROUND, &gbg) == GHOSTTY_SUCCESS) {
			bg->r = gbg.r;
			bg->g = gbg.g;
			bg->b = gbg.b;
		} else {
			bg->r = 30; bg->g = 30; bg->b = 32;
		}
	}
	return 1;
}

int cair_terminal_model_get_cell(CairTerminalModel *model, uint16_t row, uint16_t col, CairTerminalCell *cell) {
	GhosttyRenderState rstate;
	GhosttyRenderStateRowIterator row_iter;
	GhosttyRenderStateRowCells row_cells;
	GhosttyColorRgb color;
	GhosttyStyle style;
	bool selected = false;
	uint16_t cur_row = 0;
	GhosttyBuffer gbuf;

	if (!model || !model->initialized || !cell || row >= model->rows || col >= model->cols) {
		return 0;
	}
	memset(cell, 0, sizeof(*cell));

	rstate = (GhosttyRenderState)model->render_state;
	row_iter = (GhosttyRenderStateRowIterator)model->row_iter;
	row_cells = (GhosttyRenderStateRowCells)model->row_cells;

	if (ghostty_render_state_get(rstate, GHOSTTY_RENDER_STATE_DATA_ROW_ITERATOR, &row_iter) != GHOSTTY_SUCCESS) {
		return 0;
	}

	while (ghostty_render_state_row_iterator_next(row_iter)) {
		if (cur_row == row) {
			if (ghostty_render_state_row_get(row_iter, GHOSTTY_RENDER_STATE_ROW_DATA_CELLS, &row_cells) != GHOSTTY_SUCCESS) {
				return 0;
			}
			if (ghostty_render_state_row_cells_select(row_cells, col) != GHOSTTY_SUCCESS) {
				return 0;
			}

			gbuf.ptr = (uint8_t*)cell->text;
			gbuf.cap = sizeof(cell->text) - 1;
			gbuf.len = 0;
			if (ghostty_render_state_row_cells_get(row_cells, GHOSTTY_RENDER_STATE_ROW_CELLS_DATA_GRAPHEMES_UTF8, &gbuf) == GHOSTTY_SUCCESS) {
				cell->text_len = gbuf.len;
				cell->text[gbuf.len] = '\0';
			}

			if (ghostty_render_state_row_cells_get(row_cells, GHOSTTY_RENDER_STATE_ROW_CELLS_DATA_FG_COLOR, &color) == GHOSTTY_SUCCESS) {
				cell->fg.r = color.r;
				cell->fg.g = color.g;
				cell->fg.b = color.b;
				cell->has_fg = 1;
			}
			if (ghostty_render_state_row_cells_get(row_cells, GHOSTTY_RENDER_STATE_ROW_CELLS_DATA_BG_COLOR, &color) == GHOSTTY_SUCCESS) {
				cell->bg.r = color.r;
				cell->bg.g = color.g;
				cell->bg.b = color.b;
				cell->has_bg = 1;
			}
			if (ghostty_render_state_row_cells_get(row_cells, GHOSTTY_RENDER_STATE_ROW_CELLS_DATA_SELECTED, &selected) == GHOSTTY_SUCCESS) {
				cell->selected = selected ? 1 : 0;
			}
			if (ghostty_render_state_row_cells_get(row_cells, GHOSTTY_RENDER_STATE_ROW_CELLS_DATA_STYLE, &style) == GHOSTTY_SUCCESS) {
				cell->bold = (style.flags & GHOSTTY_STYLE_FLAG_BOLD) ? 1 : 0;
				cell->italic = (style.flags & GHOSTTY_STYLE_FLAG_ITALIC) ? 1 : 0;
				cell->underline = (GHOSTTY_STYLE_UNDERLINE(style) != GHOSTTY_SGR_UNDERLINE_NONE) ? 1 : 0;
			}
			return 1;
		}
		cur_row++;
	}
	return 0;
}

int cair_terminal_model_read_row_text(CairTerminalModel *model, uint16_t row, char *buffer, size_t buffer_size) {
	GhosttyRenderState rstate;
	GhosttyRenderStateRowIterator row_iter;
	GhosttyRenderStateRowCells row_cells;
	uint16_t cur_row = 0;
	size_t written = 0;

	if (!model || !model->initialized || !buffer || buffer_size == 0 || row >= model->rows) {
		return -1;
	}
	buffer[0] = '\0';

	rstate = (GhosttyRenderState)model->render_state;
	row_iter = (GhosttyRenderStateRowIterator)model->row_iter;
	row_cells = (GhosttyRenderStateRowCells)model->row_cells;

	if (ghostty_render_state_get(rstate, GHOSTTY_RENDER_STATE_DATA_ROW_ITERATOR, &row_iter) != GHOSTTY_SUCCESS) {
		return -1;
	}

	while (ghostty_render_state_row_iterator_next(row_iter)) {
		if (cur_row == row) {
			if (ghostty_render_state_row_get(row_iter, GHOSTTY_RENDER_STATE_ROW_DATA_CELLS, &row_cells) != GHOSTTY_SUCCESS) {
				return -1;
			}
			while (ghostty_render_state_row_cells_next(row_cells)) {
				char grapheme[32];
				GhosttyBuffer gbuf;
				gbuf.ptr = (uint8_t*)grapheme;
				gbuf.cap = sizeof(grapheme) - 1;
				gbuf.len = 0;

				if (ghostty_render_state_row_cells_get(row_cells, GHOSTTY_RENDER_STATE_ROW_CELLS_DATA_GRAPHEMES_UTF8, &gbuf) == GHOSTTY_SUCCESS && gbuf.len > 0) {
					if (written + gbuf.len < buffer_size) {
						memcpy(buffer + written, grapheme, gbuf.len);
						written += gbuf.len;
					}
				} else {
					if (written + 1 < buffer_size) {
						buffer[written++] = ' ';
					}
				}
			}
			/* Trim trailing spaces */
			while (written > 0 && buffer[written - 1] == ' ') {
				written--;
			}
			buffer[written] = '\0';
			return (int)written;
		}
		cur_row++;
	}
	return -1;
}

int cair_terminal_model_scroll_viewport(CairTerminalModel *model, int delta) {
	GhosttyTerminalScrollViewport behavior;
	if (!model || !model->initialized) return 0;
	memset(&behavior, 0, sizeof(behavior));
	behavior.tag = GHOSTTY_TERMINAL_SCROLL_VIEWPORT_DELTA;
	behavior.value.delta = (intptr_t)delta;
	if (ghostty_terminal_scroll_viewport((GhosttyTerminal)model->terminal, behavior) != GHOSTTY_SUCCESS) {
		return 0;
	}
	return cair_terminal_model_update(model);
}
