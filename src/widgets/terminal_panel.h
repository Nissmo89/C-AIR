#ifndef CAIR_TERMINAL_PANEL_H
#define CAIR_TERMINAL_PANEL_H

#include <Raven.h>
#include <windows.h>
#include <stddef.h>
#include <stdint.h>
#include "model/terminal_process.h"
#include "model/terminal_model.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct CairTerminalPanel {
	Rid root;           /* outer VBox containing the custom terminal canvas */
	Rid canvas;         /* R_WIDGET_CUSTOM — the VT cell grid */
	CairTerminalProcess process;
	CairTerminalModel   model;
	Rid font;
	Rid font_bold;
	Rid font_italic;
	Rid font_bold_italic;
	CRITICAL_SECTION    output_lock;
	char               *output_queue;
	size_t              output_length;
	size_t              output_capacity;
	char                input_line[4096];
	size_t              input_length;
	uint16_t            input_columns;
	float               last_width;
	float               last_height;

	/* Persistent renderer storage. The inline grapheme buffer covers the
	   common case; grapheme_overflow grows only for larger clusters. */
	char               *run_buffer;
	size_t              run_capacity;
	char                grapheme_inline[256];
	char               *grapheme_overflow;
	size_t              grapheme_overflow_capacity;

	/* Logical font/cell metrics shared by painting and terminal resizing. */
	float               font_size;
	float               cell_width;
	float               cell_height;
	float               text_offset_y;
	uint64_t            last_blink_tick;
	int                 blink_visible;
	int                 has_blink_cells;

	/* Diagnostics for the renderer test: number of paint callbacks run. */
	uint64_t            paint_count;

	int                 output_task_posted;
	int                 initialized;
	int                 lock_initialized;
} CairTerminalPanel;

/**
 * Create the terminal panel widgets.
 * Returns the root Rid on success, R_INVALID_ID on failure.
 * Does NOT start the shell process yet — call cair_terminal_panel_show() for that.
 */
Rid cair_terminal_panel_create(CairTerminalPanel *panel);

/**
 * Start the shell and make the panel visible.
 * Safe to call repeatedly; a no-op if already running.
 */
void cair_terminal_panel_show(CairTerminalPanel *panel);

/**
 * Hide the panel while preserving the running shell session.
 */
void cair_terminal_panel_hide(CairTerminalPanel *panel);

/**
 * Toggle between visible and hidden.
 */
void cair_terminal_panel_toggle(CairTerminalPanel *panel);

/* Stop worker activity before Raven shuts down; safe to call more than once. */
void cair_terminal_panel_stop(CairTerminalPanel *panel);

/**
 * Release all resources. Called at app shutdown.
 */
void cair_terminal_panel_destroy(CairTerminalPanel *panel);

/**
 * Optional UI-loop tick. Call it from the frame loop to advance the cursor and
 * SGR blink phase. It never creates a worker thread: it only invalidates the
 * canvas when a blink phase change is due.
 */
void cair_terminal_panel_tick(CairTerminalPanel *panel);

#ifdef __cplusplus
}
#endif

#endif /* CAIR_TERMINAL_PANEL_H */
