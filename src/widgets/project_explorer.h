#ifndef CAIR_PROJECT_EXPLORER_H
#define CAIR_PROJECT_EXPLORER_H

#include <Raven.h>
#include "model/project_tree.h"

/* ---------------------------------------------------------------------------
 * CairProjectExplorer
 *
 * A project-file sidebar with Zed-style indent guide lines.
 *
 * Visual design (matches Zed's project panel):
 *   Every expanded directory paints one 1 px vertical guide line per open
 *   nesting level.  A guide starts on the first visible child row and ends on
 *   the bottom of the last visible descendant row, on the left of the
 *   children.  There are no horizontal branch ticks; this is exactly the
 *   geometry of Zed's compute_indent_guides() (see project_explorer.c).
 *
 *   Directories show the folder icons from assets/: folder.svg while
 *   collapsed and folder_open.svg while expanded.  If those files cannot be
 *   found (for example a stripped deployment), the explorer falls back to the
 *   previous painted chevrons so the panel stays usable.
 *
 * Layout
 * ------
 *   root (VBOX, min-width 210)
 *     |- "PROJECT" label
 *     `- scroll_box (SCROLL_BOX, fill=1)
 *          `- canvas (R_WIDGET_CUSTOM, painted row-by-row)
 *
 * Interaction
 * -----------
 *   - Click the folder icon (or double-click a directory row) to
 *     expand/collapse.
 *   - Single-click a file row to open it (on_open_file).
 *   - Directories start collapsed, matching the previous tree view.
 * --------------------------------------------------------------------------- */

typedef struct CairProjectExplorer {
	Rid root;        /* outer VBOX – add to your layout */
	Rid scroll_box;  /* SCROLL_BOX holding the canvas */
	Rid canvas;      /* R_WIDGET_CUSTOM that draws rows + guide lines */

	const CairProjectTree *model;

	/* Application callback, fired when a file row is clicked. */
	void (*on_open_file)(const CairProjectNode *node, void *user_data);
	void *user_data;

	/* Internal state – do NOT touch. */
	void *_state;
} CairProjectExplorer;

/*
 * Creates the explorer widget tree and populates it from `model`.
 * Returns explorer->root on success, R_INVALID_ID on failure.
 * `on_open_file` may be NULL.
 */
Rid cair_project_explorer_create(CairProjectExplorer *explorer,
	const CairProjectTree *model,
	void (*on_open_file)(const CairProjectNode *node, void *user_data),
	void *user_data);

/*
 * Rebuilds the internal flat-row list from the current model state and
 * triggers a repaint.  Call after the model is rescanned.
 */
void cair_project_explorer_refresh(CairProjectExplorer *explorer);

/*
 * Number of visible rows currently drawn (directories that are collapsed
 * hide their subtree).  Returns 0 for a NULL or invalid explorer.
 */
int cair_project_explorer_row_count(const CairProjectExplorer *explorer);

/*
 * Expands or collapses the directory whose absolute `path` matches a model
 * node.  Has no effect for files or unknown paths.  The row list is rebuilt
 * immediately; the caller does not need to call refresh().
 */
void cair_project_explorer_set_directory_expanded(CairProjectExplorer *explorer,
	const char *path, int expanded);

/*
 * Read-only access to the computed indent guides, in row units.  `start_row`
 * and `end_row` are inclusive.  These exist so tests can assert the geometry
 * ported from Zed's compute_indent_guides().
 */
int cair_project_explorer_guide_count(const CairProjectExplorer *explorer);
int cair_project_explorer_get_guide(const CairProjectExplorer *explorer,
	int index, int *depth, int *start_row, int *end_row);

/*
 * Returns 1 when both folder icon textures were loaded from assets/, 0 when
 * the explorer is painting the fallback chevrons instead.  Useful for
 * diagnostics and tests.
 */
int cair_project_explorer_has_icons(const CairProjectExplorer *explorer);

/*
 * Frees the explorer's private state.  Call after the Raven UI has been shut
 * down (or at least after the explorer widgets are gone); the function does
 * not destroy any Raven widget itself.  Icon textures are released while
 * Raven is still initialized; after Rshutdown Raven has already freed them.
 */
void cair_project_explorer_destroy(CairProjectExplorer *explorer);

#endif /* CAIR_PROJECT_EXPLORER_H */
