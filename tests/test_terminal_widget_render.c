#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <Raven.h>
#include <ghostty/vt.h>
#include "widgets/terminal_panel.h"

#include <stdio.h>
#include <string.h>

/* The visible text of one row, as the model exposes it. */
static int row_contains(CairTerminalPanel *panel, uint16_t row, const char *needle) {
	char line[4096];
	int length;

	if (!panel || !needle) return 0;
	length = cair_terminal_model_read_row_text(&panel->model, row, line, sizeof(line));
	return length >= 0 && strstr(line, needle) != NULL;
}

/*
 * Finds the wide CJK cell on a row and checks that Ghostty marks the next cell
 * as its spacer tail. This is the width contract the renderer relies on.
 */
static int row_wide_cell_is_paired(CairTerminalPanel *panel, uint16_t row) {
	GhosttyRenderStateRowIterator iterator;
	GhosttyRenderStateRowCells cells;
	uint16_t current = 0;

	if (!panel) return 0;
	iterator = (GhosttyRenderStateRowIterator)panel->model.row_iter;
	cells = (GhosttyRenderStateRowCells)panel->model.row_cells;
	if (!iterator || !cells) return 0;
	if (ghostty_render_state_get((GhosttyRenderState)panel->model.render_state,
	        GHOSTTY_RENDER_STATE_DATA_ROW_ITERATOR, &iterator) != GHOSTTY_SUCCESS) {
		return 0;
	}

	while (ghostty_render_state_row_iterator_next(iterator)) {
		if (current == row) {
			uint32_t col = 0;
			if (ghostty_render_state_row_get(iterator,
			        GHOSTTY_RENDER_STATE_ROW_DATA_CELLS, &cells) != GHOSTTY_SUCCESS) {
				return 0;
			}
			while (ghostty_render_state_row_cells_next(cells) &&
			       col < (uint32_t)panel->model.cols) {
				GhosttyCell raw = 0;
				GhosttyCellWide wide = GHOSTTY_CELL_WIDE_NARROW;
				char text[16];
				GhosttyBuffer buffer;

				(void)ghostty_render_state_row_cells_get(cells,
					GHOSTTY_RENDER_STATE_ROW_CELLS_DATA_RAW, &raw);
				if (raw != 0) {
					(void)ghostty_cell_get(raw, GHOSTTY_CELL_DATA_WIDE, &wide);
				}

				buffer.ptr = (uint8_t *)text;
				buffer.cap = sizeof(text) - 1u;
				buffer.len = 0;
				if (ghostty_render_state_row_cells_get(cells,
				        GHOSTTY_RENDER_STATE_ROW_CELLS_DATA_GRAPHEMES_UTF8,
				        &buffer) == GHOSTTY_SUCCESS) {
					text[buffer.len] = '\0';
					if (wide == GHOSTTY_CELL_WIDE_WIDE &&
					    strcmp(text, "\xe4\xb8\xad") == 0) {
						GhosttyCell tail_raw = 0;
						GhosttyCellWide tail_wide = GHOSTTY_CELL_WIDE_NARROW;
						if (!ghostty_render_state_row_cells_next(cells)) return 0;
						(void)ghostty_render_state_row_cells_get(cells,
							GHOSTTY_RENDER_STATE_ROW_CELLS_DATA_RAW, &tail_raw);
						if (tail_raw != 0) {
							(void)ghostty_cell_get(tail_raw,
								GHOSTTY_CELL_DATA_WIDE, &tail_wide);
						}
						return tail_wide == GHOSTTY_CELL_WIDE_SPACER_TAIL;
					}
				}
				++col;
			}
			return 0;
		}
		++current;
	}
	return 0;
}

int main(void) {
	static const char vt_data[] =
		"\x1b[31mRED\x1b[0m normal "
		"wide: \xe4\xb8\xad "
		"emoji: \xf0\x9f\x98\x80\r\n"
		"\x1b[1;3;4mBOLD ITALIC UNDERLINE\x1b[0m\r\n";
	char edge_row[96];
	CairTerminalPanel panel;
	CairTerminalCell cell;
	Rid window;
	Rid root;
	int frame;

	memset(&panel, 0, sizeof(panel));
	panel.root = panel.canvas = R_INVALID_ID;

	if (!Rinit(R_BACKEND_NULL)) return 1;
	window = Rwindow_create("Terminal renderer smoke test", 800, 480, 0);
	root = Rwidget_create(R_WIDGET_VBOX);
	if (window == R_INVALID_ID || root == R_INVALID_ID ||
	    cair_terminal_panel_create(&panel) == R_INVALID_ID) {
		Rshutdown();
		cair_terminal_panel_destroy(&panel);
		return 2;
	}

	Rwidget_add_child(root, panel.root);
	Rslot_set_fill(panel.root, 1.0f);
	Rwindow_set_content(window, root);

	/* Start the model without launching cmd.exe; this test exercises the
	   Raven paint callback and the Ghostty snapshot only. */
	if (!cair_terminal_model_init(&panel.model, 80, 24)) {
		Rshutdown();
		cair_terminal_panel_destroy(&panel);
		return 3;
	}
	panel.initialized = 1;
	Rwidget_set_visibility(panel.root, R_VISIBLE);

	if (!cair_terminal_model_write(&panel.model, vt_data, sizeof(vt_data) - 1u)) {
		cair_terminal_panel_destroy(&panel);
		Rshutdown();
		return 4;
	}

	/* A wide glyph forced into the last column must clip, not shift or crash. */
	memset(edge_row, 'x', 79);
	memcpy(edge_row + 79, "\xe4\xb8\xad", 3);
	edge_row[82] = '\n';
	if (!cair_terminal_model_write(&panel.model, "\x1b[3;1H", 6) ||
	    !cair_terminal_model_write(&panel.model, edge_row, 83)) {
		cair_terminal_panel_destroy(&panel);
		Rshutdown();
		return 5;
	}

	if (!row_contains(&panel, 0, "RED") ||
	    !row_contains(&panel, 0, "normal") ||
	    !row_contains(&panel, 0, "emoji: \xf0\x9f\x98\x80") ||
	    !row_contains(&panel, 1, "BOLD ITALIC UNDERLINE")) {
		cair_terminal_panel_destroy(&panel);
		Rshutdown();
		return 6;
	}

	/* ANSI red must reach the model as a per-cell foreground, not the default. */
	if (!cair_terminal_model_get_cell(&panel.model, 0, 0, &cell) ||
	    strcmp(cell.text, "R") != 0 || !cell.has_fg ||
	    !(cell.fg.r > cell.fg.g && cell.fg.r > cell.fg.b)) {
		cair_terminal_panel_destroy(&panel);
		Rshutdown();
		return 8;
	}

	/* Bold, italic and underline survive the render-state snapshot. */
	if (!cair_terminal_model_get_cell(&panel.model, 1, 0, &cell) ||
	    !cell.bold || !cell.italic || !cell.underline) {
		cair_terminal_panel_destroy(&panel);
		Rshutdown();
		return 9;
	}

	/* The wide CJK base cell and its spacer tail must be paired. */
	if (!row_wide_cell_is_paired(&panel, 0)) {
		cair_terminal_panel_destroy(&panel);
		Rshutdown();
		return 10;
	}

	for (frame = 0; frame < 3; ++frame) {
		Rdraw_frame();
		cair_terminal_panel_tick(&panel);
		Sleep(10);
	}

	if (panel.paint_count == 0u) {
		cair_terminal_panel_destroy(&panel);
		Rshutdown();
		return 11;
	}

	cair_terminal_panel_stop(&panel);
	Rshutdown();
	cair_terminal_panel_destroy(&panel);
	puts("Terminal renderer null-backend smoke test passed.");
	return 0;
}
