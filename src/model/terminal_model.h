#ifndef CAIR_TERMINAL_MODEL_H
#define CAIR_TERMINAL_MODEL_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct CairTerminalColor {
	uint8_t r;
	uint8_t g;
	uint8_t b;
} CairTerminalColor;

typedef struct CairTerminalCell {
	char text[32];          /* UTF-8 grapheme string (null-terminated) */
	size_t text_len;        /* Length of text in bytes */
	CairTerminalColor fg;   /* Foreground RGB color */
	CairTerminalColor bg;   /* Background RGB color */
	int has_fg;             /* Non-zero if custom FG color is set */
	int has_bg;             /* Non-zero if custom BG color is set */
	int selected;           /* Non-zero if cell is selected */
	int bold;               /* Style attributes */
	int italic;
	int underline;
} CairTerminalCell;

typedef struct CairTerminalCursor {
	uint16_t row;
	uint16_t col;
	int visible;
	int blinking;
} CairTerminalCursor;

typedef struct CairTerminalModel {
	void *terminal;         /* GhosttyTerminal handle */
	void *render_state;     /* GhosttyRenderState handle */
	void *row_iter;         /* GhosttyRenderStateRowIterator handle */
	void *row_cells;        /* GhosttyRenderStateRowCells handle */
	uint16_t cols;
	uint16_t rows;
	int initialized;
} CairTerminalModel;

/**
 * Initialize a terminal model with given initial columns and rows.
 * Returns 1 on success, 0 on failure.
 */
int cair_terminal_model_init(CairTerminalModel *model, uint16_t cols, uint16_t rows);

/**
 * Free all internal state of the terminal model.
 */
void cair_terminal_model_destroy(CairTerminalModel *model);

/**
 * Feed VT escape sequences or raw text into the terminal.
 * Returns 1 on success, 0 on failure.
 */
int cair_terminal_model_write(CairTerminalModel *model, const char *data, size_t len);

/**
 * Resize the terminal grid.
 * Returns 1 on success, 0 on failure.
 */
int cair_terminal_model_resize(CairTerminalModel *model, uint16_t cols, uint16_t rows);

/**
 * Synchronize the render state snapshot with the current terminal state.
 * Returns 1 on success, 0 on failure.
 */
int cair_terminal_model_update(CairTerminalModel *model);

/**
 * Query current dimensions (columns and rows) of the terminal.
 */
int cair_terminal_model_get_dimensions(const CairTerminalModel *model, uint16_t *cols, uint16_t *rows);

/**
 * Query current cursor position and visibility.
 * Returns 1 if cursor is visible in viewport, 0 if hidden/out of view.
 */
int cair_terminal_model_get_cursor(const CairTerminalModel *model, CairTerminalCursor *cursor);

/**
 * Query default foreground and background colors.
 */
int cair_terminal_model_get_default_colors(const CairTerminalModel *model, CairTerminalColor *fg, CairTerminalColor *bg);

/**
 * Fetch cell details at (row, col) in viewport coordinates.
 * Returns 1 on success, 0 on failure.
 */
int cair_terminal_model_get_cell(CairTerminalModel *model, uint16_t row, uint16_t col, CairTerminalCell *cell);

/**
 * Read the visible text of an entire row as a null-terminated UTF-8 string.
 * Returns number of bytes written, or -1 on failure.
 */
int cair_terminal_model_read_row_text(CairTerminalModel *model, uint16_t row, char *buffer, size_t buffer_size);

/**
 * Scroll the viewport by a delta amount (negative = scroll up into history, positive = scroll down).
 * Returns 1 on success, 0 on failure.
 */
int cair_terminal_model_scroll_viewport(CairTerminalModel *model, int delta);

#ifdef __cplusplus
}
#endif

#endif /* CAIR_TERMINAL_MODEL_H */
