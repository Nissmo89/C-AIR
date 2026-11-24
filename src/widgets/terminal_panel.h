#ifndef CAIR_TERMINAL_PANEL_H
#define CAIR_TERMINAL_PANEL_H

#include <Raven.h>
#include <windows.h>
#include "model/terminal_process.h"
#include "model/terminal_model.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct CairTerminalPanel {
	Rid root;           /* outer VBox (title bar + custom canvas) */
	Rid header;         /* HBox title bar row */
	Rid canvas;         /* R_WIDGET_CUSTOM — the VT cell grid */
	CairTerminalProcess process;
	CairTerminalModel   model;
	Rid font;
	CRITICAL_SECTION    output_lock;
	char               *output_queue;
	size_t              output_length;
	size_t              output_capacity;
	char                input_line[4096];
	size_t              input_length;
	uint16_t            input_columns;
	float               last_width;
	float               last_height;
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

#ifdef __cplusplus
}
#endif

#endif /* CAIR_TERMINAL_PANEL_H */
