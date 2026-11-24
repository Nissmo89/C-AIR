/*
 * src/widgets/project_explorer.c
 *
 * Project explorer sidebar with Zed-style indent guide lines.
 *
 * The geometry below is a direct port of Zed's indent guide computation:
 *
 *   crates/ui/src/components/indent_guides.rs   fn compute_indent_guides()
 *   used by crates/project_panel/src/project_panel.rs
 *
 * Zed does NOT draw elbows or horizontal branch ticks.  It walks the visible
 * rows keeping a stack of open guides:
 *
 *   - a row at depth D opens one guide for every level from the current stack
 *     depth up to D, each guide starting on that row;
 *   - a row at depth D closes every guide deeper than D, each one ending on
 *     the previous row;
 *   - every guide still open is extended through the current row.
 *
 * A guide at stack level `d` is the line for the children of a directory at
 * depth `d`.  It is painted as a 1 px vertical quad at
 * `x = left_pad + d * indent + indent / 2` (the directory's icon column),
 * from the top of its first child row to the bottom of its last visible
 * descendant row.  That is exactly the requested behaviour: every expanded
 * folder draws a line on the left of its children, down to its last child.
 *
 * The visible rows are flattened here instead of using Raven's tree view so
 * that the guide lines, folder icons and labels can be painted in one pass.
 * The explorer keeps its own collapsed/expanded set (keyed by path, so it
 * survives a model rescan); directories start collapsed like the previous
 * tree view.
 */

#include "widgets/project_explorer.h"

#include <Raven.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ---------------------------------------------------------------------------
 * Visual constants (Zed project panel defaults)
 * --------------------------------------------------------------------------- */
#define PE_INDENT        20.0f   /* px per depth level                         */
#define PE_ROW_H         22.0f   /* row height                                 */
#define PE_LEFT_PAD       8.0f   /* left gutter before the depth-0 column      */
#define PE_GUIDE_W        1.0f   /* guide line width (Zed paints px(1.))       */
#define PE_FONT_SIZE     13.0f   /* row label size                             */
#define PE_CHEVRON_HALF   3.5f   /* half-size of the fallback chevron          */
#define PE_ICON_SIZE     16.0f   /* folder.svg / folder_open.svg viewBox       */
#define PE_MAX_ROWS    8192      /* hard cap, mirrors the model's own cap      */
#define PE_MAX_DEPTH     64      /* guard for malformed models                 */

/* Asset names relative to the project's assets/ directory. */
#define PE_ICON_FOLDER      "folder.svg"
#define PE_ICON_FOLDER_OPEN "folder_open.svg"

/* ---------------------------------------------------------------------------
 * Per-row entry built by the DFS flatten pass
 * --------------------------------------------------------------------------- */
typedef struct {
	const CairProjectNode *node;
	int depth;
} PERow;

/*
 * One precomputed guide line, in row units.  `start_row` and `end_row` are
 * inclusive: the line spans from the top of start_row to the bottom of
 * end_row.
 */
typedef struct {
	int depth;
	int start_row;
	int end_row;
} PEGuide;

/* ---------------------------------------------------------------------------
 * State allocated once and kept for the lifetime of the explorer
 * --------------------------------------------------------------------------- */
typedef struct {
	CairProjectExplorer *explorer; /* back-pointer                     */

	PERow   *rows;         /* flat visible rows                         */
	int      row_count;
	int      row_cap;

	PEGuide *guides;       /* precomputed indent guides                 */
	int      guide_count;
	int      guide_cap;

	char   **expanded;     /* paths of expanded directories             */
	int      expanded_count;
	int      expanded_cap;

	char    *root_path;    /* path of the root row currently shown      */

	Rid      icon_folder;      /* assets/folder.svg, R_INVALID_ID if missing */
	Rid      icon_folder_open; /* assets/folder_open.svg                     */

	int      selected_row; /* -1 = none                                 */
	int      hot_row;      /* hovered row, -1 = none                    */
} PEState;

/* ---------------------------------------------------------------------------
 * Small helpers
 * --------------------------------------------------------------------------- */
/*
 * Asset lookup.
 *
 * Raven loads textures from paths relative to the process working directory.
 * C-AIR is usually launched from the repository root, but double-clicking
 * build/bin/cair.exe (or running the CTest binaries from build/) puts the
 * working directory somewhere else.  Try the three layouts the project uses:
 *
 *   assets/...        cwd = repository root
 *   ../assets/...     cwd = build/
 *   ../../assets/...  cwd = build/bin/
 *
 * The first path that exists wins; fopen keeps a miss quiet instead of making
 * Raven log an error for every candidate.
 */
static int pe_find_asset(const char *name, char *out, size_t out_size)
{
	static const char *const prefixes[] = { "", "../", "../../" };
	size_t i;

	if (!name || !out || out_size == 0) return 0;
	for (i = 0; i < sizeof(prefixes) / sizeof(prefixes[0]); ++i) {
		FILE *file;
		int written = snprintf(out, out_size, "%sassets/%s",
			prefixes[i], name);
		if (written < 0 || (size_t)written >= out_size) continue;
		file = fopen(out, "rb");
		if (file) {
			fclose(file);
			return 1;
		}
	}
	out[0] = '\0';
	return 0;
}

static Rid pe_load_icon(const char *name)
{
	char path[512];
	if (!pe_find_asset(name, path, sizeof(path))) return R_INVALID_ID;
	return Rtexture_create_from_file(path);
}

static void pe_release_icons(PEState *state)
{
	if (!state) return;
	if (Rtexture_is_valid(state->icon_folder))
		Rtexture_destroy(state->icon_folder);
	if (Rtexture_is_valid(state->icon_folder_open))
		Rtexture_destroy(state->icon_folder_open);
	state->icon_folder      = R_INVALID_ID;
	state->icon_folder_open = R_INVALID_ID;
}

static char *pe_strdup(const char *text)
{
	size_t length;
	char *copy;
	if (!text) return NULL;
	length = strlen(text) + 1;
	copy = (char *)malloc(length);
	if (copy) memcpy(copy, text, length);
	return copy;
}

static int pe_is_expanded(const PEState *state, const char *path)
{
	int i;
	if (!path) return 0;
	for (i = 0; i < state->expanded_count; ++i) {
		if (strcmp(state->expanded[i], path) == 0) return 1;
	}
	return 0;
}

static void pe_set_expanded(PEState *state, const char *path, int expanded)
{
	int i;
	if (!path || !path[0]) return;

	for (i = 0; i < state->expanded_count; ++i) {
		if (strcmp(state->expanded[i], path) == 0) {
			if (!expanded) {
				free(state->expanded[i]);
				state->expanded[i] = state->expanded[--state->expanded_count];
			}
			return;
		}
	}
	if (!expanded) return;

	if (state->expanded_count == state->expanded_cap) {
		int new_cap = state->expanded_cap ? state->expanded_cap * 2 : 16;
		char **grown = (char **)realloc(state->expanded,
			(size_t)new_cap * sizeof(char *));
		if (!grown) return;
		state->expanded = grown;
		state->expanded_cap = new_cap;
	}
	state->expanded[state->expanded_count] = pe_strdup(path);
	if (state->expanded[state->expanded_count]) ++state->expanded_count;
}

/* ---------------------------------------------------------------------------
 * DFS: append visible rows into state->rows
 * --------------------------------------------------------------------------- */
static int pe_reserve_row(PEState *state)
{
	if (state->row_count < state->row_cap) return 1;
	{
		int new_cap = state->row_cap ? state->row_cap * 2 : 64;
		PERow *grown;
		if (new_cap > PE_MAX_ROWS) new_cap = PE_MAX_ROWS;
		if (new_cap <= state->row_cap) return 0;
		grown = (PERow *)realloc(state->rows, (size_t)new_cap * sizeof(PERow));
		if (!grown) return 0;
		state->rows = grown;
		state->row_cap = new_cap;
	}
	return 1;
}

static void flatten_children(PEState *state, const CairProjectNode *parent,
	int depth)
{
	size_t i;
	if (!parent || depth >= PE_MAX_DEPTH) return;

	for (i = 0; i < parent->child_count && state->row_count < PE_MAX_ROWS; ++i) {
		const CairProjectNode *child = parent->children[i];
		if (!child) continue;
		if (!pe_reserve_row(state)) return;

		state->rows[state->row_count].node  = child;
		state->rows[state->row_count].depth = depth;
		++state->row_count;

		if (child->is_directory && child->child_count > 0
			&& pe_is_expanded(state, child->path)) {
			flatten_children(state, child, depth + 1);
		}
	}
}

/* ---------------------------------------------------------------------------
 * Zed's compute_indent_guides(), ported to C.
 *
 * `stack_depth[k]` / `stack_start[k]` describe the guides currently open.  A
 * guide is emitted when it closes; its end row is the row before the row that
 * closed it (or the last row when the list ends).
 * --------------------------------------------------------------------------- */
static int pe_add_guide(PEState *state, int depth, int start_row, int end_row)
{
	if (end_row < start_row) return 1;

	if (state->guide_count == state->guide_cap) {
		int new_cap = state->guide_cap ? state->guide_cap * 2 : 32;
		PEGuide *grown = (PEGuide *)realloc(state->guides,
			(size_t)new_cap * sizeof(PEGuide));
		if (!grown) return 0;
		state->guides = grown;
		state->guide_cap = new_cap;
	}
	state->guides[state->guide_count].depth     = depth;
	state->guides[state->guide_count].start_row = start_row;
	state->guides[state->guide_count].end_row   = end_row;
	++state->guide_count;
	return 1;
}

static void rebuild_guides(PEState *state)
{
	int *stack_depth;
	int *stack_start;
	int stack_count = 0;
	int i;

	state->guide_count = 0;
	if (state->row_count <= 0) return;

	stack_depth = (int *)malloc((size_t)state->row_count * sizeof(int));
	stack_start = (int *)malloc((size_t)state->row_count * sizeof(int));
	if (!stack_depth || !stack_start) {
		free(stack_depth);
		free(stack_start);
		return;
	}

	for (i = 0; i < state->row_count; ++i) {
		int depth = state->rows[i].depth;

		if (depth > stack_count) {
			int new_depth;
			for (new_depth = stack_count; new_depth < depth; ++new_depth) {
				stack_depth[stack_count] = new_depth;
				stack_start[stack_count] = i;
				++stack_count;
			}
		} else if (depth < stack_count) {
			while (stack_count > depth) {
				--stack_count;
				pe_add_guide(state, stack_depth[stack_count],
					stack_start[stack_count], i - 1);
			}
		}
	}

	while (stack_count > 0) {
		--stack_count;
		pe_add_guide(state, stack_depth[stack_count],
			stack_start[stack_count], state->row_count - 1);
	}

	free(stack_depth);
	free(stack_start);
}

static void rebuild_rows(PEState *state)
{
	const CairProjectTree *model = state->explorer->model;

	state->row_count    = 0;
	state->selected_row = -1;
	state->hot_row      = -1;

	if (model && model->root) {
		const CairProjectNode *root = model->root;

		/* The scanned root is the first row, so the panel shows the project
		 * folder itself at the top instead of starting with its children.
		 * A new root path (after a rescan) starts expanded; the user can
		 * still collapse it afterwards. */
		if (!state->root_path || !root->path
			|| strcmp(state->root_path, root->path) != 0) {
			free(state->root_path);
			state->root_path = pe_strdup(root->path);
			if (root->path) pe_set_expanded(state, root->path, 1);
		}

		if (pe_reserve_row(state)) {
			state->rows[state->row_count].node  = root;
			state->rows[state->row_count].depth = 0;
			++state->row_count;
		}
		if (pe_is_expanded(state, root->path))
			flatten_children(state, root, 1);
	}

	rebuild_guides(state);
}

static int pe_find_row(const PEState *state, const CairProjectNode *node)
{
	int i;
	for (i = 0; i < state->row_count; ++i) {
		if (state->rows[i].node == node) return i;
	}
	return -1;
}

/* ---------------------------------------------------------------------------
 * Geometry helpers
 * --------------------------------------------------------------------------- */
static float pe_column_x(int depth)
{
	return PE_LEFT_PAD + (float)depth * PE_INDENT;
}

static float pe_icon_cx(int depth)
{
	return pe_column_x(depth) + PE_INDENT * 0.5f;
}

static float pe_guide_left(int depth)
{
	return pe_icon_cx(depth) - PE_GUIDE_W * 0.5f;
}

static int pe_row_at(const PEState *state, float y)
{
	int row;
	if (y < 0.0f) return -1;
	row = (int)(y / PE_ROW_H);
	if (row < 0 || row >= state->row_count) return -1;
	return row;
}

/* ---------------------------------------------------------------------------
 * Measure callback
 * --------------------------------------------------------------------------- */
static void pe_measure(Rid widget, float *w, float *h, void *ud)
{
	PEState *state = (PEState *)ud;
	(void)widget;
	/* The scroll box gives the canvas its full width; the height is the
	 * whole row list so that the scroll bar reflects the real content. */
	*w = 0.0f;
	*h = (float)(state->row_count > 0 ? state->row_count : 1) * PE_ROW_H;
}

/* ---------------------------------------------------------------------------
 * Paint callback
 * --------------------------------------------------------------------------- */
static void pe_paint(Rid widget, RPainter *painter,
	float width, float height, void *ud)
{
	PEState *state = (PEState *)ud;
	float pr, pg, pb, pa;      /* panel background                 */
	float tr, tg, tb, ta;      /* text                             */
	float dr, dg, db, da;      /* disabled / muted text            */
	float gr, gg, gb, ga;      /* guide colour                     */
	float sr, sg, sb, sa;      /* selection                        */
	float hr, hg, hb, ha;      /* row hover                        */
	int i;
	(void)widget;

	/* ---- colours from the active theme ----------------------------------- */
	Rtheme_get_color(R_COLOR_PANEL,       &pr, &pg, &pb, &pa);
	Rtheme_get_color(R_COLOR_TEXT,        &tr, &tg, &tb, &ta);
	Rtheme_get_color(R_COLOR_TEXT_DISABLED, &dr, &dg, &db, &da);
	Rtheme_get_color(R_COLOR_BORDER,      &gr, &gg, &gb, &ga);
	Rtheme_get_color(R_COLOR_SELECTION,   &sr, &sg, &sb, &sa);
	Rtheme_get_color(R_COLOR_ROW_HOVERED, &hr, &hg, &hb, &ha);

	/* ---- panel background ------------------------------------------------ */
	Rdraw_rect(painter, 0.0f, 0.0f, width, height, 0.0f, pr, pg, pb, pa);

	/* ======================================================================
	 * Indent guides – exactly the quads Zed paints.
	 * ====================================================================== */
	for (i = 0; i < state->guide_count; ++i) {
		const PEGuide *guide = &state->guides[i];
		float x  = pe_guide_left(guide->depth);
		float y0 = (float)guide->start_row * PE_ROW_H;
		float y1 = (float)(guide->end_row + 1) * PE_ROW_H;

		if (y1 <= 0.0f || y0 >= height) continue;
		if (y0 < 0.0f) y0 = 0.0f;
		if (y1 > height) y1 = height;

		Rdraw_rect(painter, x, y0, PE_GUIDE_W, y1 - y0, 0.0f,
			gr, gg, gb, ga * 0.85f);
	}

	/* ======================================================================
	 * Rows
	 * ====================================================================== */
	for (i = 0; i < state->row_count; ++i) {
		const PERow *row = &state->rows[i];
		const CairProjectNode *node = row->node;
		float row_top = (float)i * PE_ROW_H;
		float row_mid = row_top + PE_ROW_H * 0.5f;
		float row_bot = row_top + PE_ROW_H;

		if (row_bot <= 0.0f) continue;
		if (row_top >= height) break;

		/* ---- row background: selection wins over hover ------------------ */
		if (i == state->selected_row) {
			Rdraw_rect(painter, 4.0f, row_top + 1.0f,
				width - 8.0f, PE_ROW_H - 2.0f, 4.0f,
				sr, sg, sb, sa * 0.55f);
		} else if (i == state->hot_row) {
			Rdraw_rect(painter, 4.0f, row_top + 1.0f,
				width - 8.0f, PE_ROW_H - 2.0f, 4.0f,
				hr, hg, hb, ha * 0.75f);
		}

		/* ---- folder icon, or chevron fallback when assets are missing ---- */
		if (node->is_directory) {
			float cx = pe_icon_cx(row->depth);
			int expanded = node->child_count > 0
				&& pe_is_expanded(state, node->path);
			Rid icon = expanded ? state->icon_folder_open
				: state->icon_folder;

			if (icon != R_INVALID_ID) {
				Rdraw_image(painter, icon,
					cx - PE_ICON_SIZE * 0.5f,
					row_mid - PE_ICON_SIZE * 0.5f,
					PE_ICON_SIZE, PE_ICON_SIZE,
					1.0f, 1.0f, 1.0f, 1.0f);
			} else if (node->child_count > 0) {
				float hs = PE_CHEVRON_HALF;

				if (expanded) {
					/* "v": the branch is open */
					Rdraw_line(painter, cx - hs, row_mid - hs * 0.5f,
						cx, row_mid + hs * 0.5f, 1.4f,
						dr, dg, db, da);
					Rdraw_line(painter, cx, row_mid + hs * 0.5f,
						cx + hs, row_mid - hs * 0.5f, 1.4f,
						dr, dg, db, da);
				} else {
					/* ">": the branch is closed */
					Rdraw_line(painter, cx - hs * 0.5f, row_mid - hs,
						cx + hs * 0.5f, row_mid, 1.4f,
						dr, dg, db, da);
					Rdraw_line(painter, cx + hs * 0.5f, row_mid,
						cx - hs * 0.5f, row_mid + hs, 1.4f,
						dr, dg, db, da);
				}
			}
		}

		/* ---- label ------------------------------------------------------ */
		{
			float text_x = pe_column_x(row->depth) + PE_INDENT + 4.0f;
			float text_y = row_top + (PE_ROW_H - PE_FONT_SIZE) * 0.5f;
			float alpha  = node->is_directory ? ta : ta * 0.92f;
			Rdraw_text(painter, R_INVALID_ID, PE_FONT_SIZE,
				text_x, text_y, node->name, tr, tg, tb, alpha);
		}
	}
}

/* ---------------------------------------------------------------------------
 * Event callback
 * --------------------------------------------------------------------------- */
static void pe_toggle_directory(PEState *state, int row_index)
{
	const CairProjectNode *node = state->rows[row_index].node;
	int expanded;

	if (!node || !node->is_directory || node->child_count == 0) return;

	expanded = pe_is_expanded(state, node->path);
	pe_set_expanded(state, node->path, !expanded);

	rebuild_rows(state);
	state->selected_row = pe_find_row(state, node);
}

static int pe_event(Rid widget, const REvent *event, void *ud)
{
	PEState *state = (PEState *)ud;

	switch (event->type) {
		case R_EVENT_MOUSE_MOVE: {
			int row = pe_row_at(state, event->local_y);
			if (row != state->hot_row) {
				state->hot_row = row;
				Rwidget_invalidate(widget);
			}
			return R_FALSE; /* let the scroll box see it too */
		}

		case R_EVENT_MOUSE_LEAVE:
			if (state->hot_row != -1) {
				state->hot_row = -1;
				Rwidget_invalidate(widget);
			}
			return R_FALSE;

		case R_EVENT_MOUSE_DOWN: {
			int row;
			const CairProjectNode *node;
			float cell_left, cell_right;
			int on_icon;

			if (event->button != R_MOUSE_LEFT) return R_FALSE;

			row = pe_row_at(state, event->local_y);
			if (row < 0) return R_FALSE;

			node = state->rows[row].node;
			cell_left  = pe_column_x(state->rows[row].depth);
			cell_right = cell_left + PE_INDENT;
			on_icon = event->local_x >= cell_left
				&& event->local_x < cell_right;

			state->selected_row = row;

			if (node->is_directory && node->child_count > 0
				&& (on_icon || event->clicks == 2)) {
				pe_toggle_directory(state, row);
			} else if (!node->is_directory
				&& state->explorer->on_open_file) {
				state->explorer->on_open_file(node,
					state->explorer->user_data);
			}

			Rwidget_invalidate(widget);
			return R_TRUE;
		}

		default:
			return R_FALSE;
	}
}

/* ---------------------------------------------------------------------------
 * Public API – create
 * --------------------------------------------------------------------------- */
Rid cair_project_explorer_create(CairProjectExplorer *explorer,
	const CairProjectTree *model,
	void (*on_open_file)(const CairProjectNode *node, void *user_data),
	void *user_data)
{
	Rid title;
	PEState *state;

	if (!explorer || !model || !model->root) return R_INVALID_ID;

	explorer->model        = model;
	explorer->on_open_file = on_open_file;
	explorer->user_data    = user_data;
	explorer->_state       = NULL;

	/* ---- allocate internal state ---------------------------------------- */
	state = (PEState *)calloc(1, sizeof(PEState));
	if (!state) return R_INVALID_ID;
	state->explorer     = explorer;
	state->selected_row = -1;
	state->hot_row      = -1;
	state->icon_folder      = pe_load_icon(PE_ICON_FOLDER);
	state->icon_folder_open = pe_load_icon(PE_ICON_FOLDER_OPEN);
	explorer->_state    = state;

	/* ---- outer VBOX ------------------------------------------------------ */
	explorer->root = Rwidget_create(R_WIDGET_VBOX);
	if (explorer->root == R_INVALID_ID) goto fail;
	Rpanel_set_spacing(explorer->root, 4.0f);
	Rwidget_set_min_size(explorer->root, 210.0f, 0.0f);
	{
		/* Paint the whole sidebar with the same panel color as the rows.
		 * Without this the VBOX is transparent and the header and the empty
		 * area below the tree show the darker window color, leaving the row
		 * strip as a visibly different shade. */
		float pr, pg, pb, pa;
		Rtheme_get_color(R_COLOR_PANEL, &pr, &pg, &pb, &pa);
		Rwidget_set_color(explorer->root, R_COLOR_PANEL, pr, pg, pb, pa);
		Rwidget_set_material(explorer->root, R_MATERIAL_SOLID);
	}

	/* ---- "PROJECT" section header ---------------------------------------- */
	title = Rlabel_create("PROJECT");
	if (title == R_INVALID_ID) goto fail;
	Rlabel_set_text_size(title, 11.0f);
	Rslot_set_padding(title, 8.0f, 6.0f, 8.0f, 2.0f);
	Rwidget_add_child(explorer->root, title);

	/* ---- scroll box ------------------------------------------------------ */
	explorer->scroll_box = Rwidget_create(R_WIDGET_SCROLL_BOX);
	if (explorer->scroll_box == R_INVALID_ID) goto fail;
	Rslot_set_fill(explorer->scroll_box, 1.0f);
	Rwidget_add_child(explorer->root, explorer->scroll_box);

	/* ---- custom canvas --------------------------------------------------- */
	explorer->canvas = Rwidget_create(R_WIDGET_CUSTOM);
	if (explorer->canvas == R_INVALID_ID) goto fail;

	Rcustom_set_paint_callback(explorer->canvas,   pe_paint,   state);
	Rcustom_set_measure_callback(explorer->canvas, pe_measure, state);
	Rwidget_set_event_callback(explorer->canvas,   pe_event,   state);

	Rwidget_add_child(explorer->scroll_box, explorer->canvas);

	/* ---- initial row build ----------------------------------------------- */
	rebuild_rows(state);

	return explorer->root;

fail:
	pe_release_icons(state);
	free(state->root_path);
	free(state->rows);
	free(state->guides);
	free(state);
	explorer->_state = NULL;
	return R_INVALID_ID;
}

/* ---------------------------------------------------------------------------
 * Public API – refresh (call after model rescan)
 * --------------------------------------------------------------------------- */
void cair_project_explorer_refresh(CairProjectExplorer *explorer)
{
	PEState *state;
	if (!explorer) return;
	state = (PEState *)explorer->_state;
	if (!state) return;
	rebuild_rows(state);
	if (explorer->canvas != R_INVALID_ID)
		Rwidget_invalidate(explorer->canvas);
}

int cair_project_explorer_row_count(const CairProjectExplorer *explorer)
{
	const PEState *state;
	if (!explorer) return 0;
	state = (const PEState *)explorer->_state;
	return state ? state->row_count : 0;
}

void cair_project_explorer_set_directory_expanded(CairProjectExplorer *explorer,
	const char *path, int expanded)
{
	PEState *state;
	if (!explorer || !path) return;
	state = (PEState *)explorer->_state;
	if (!state) return;

	pe_set_expanded(state, path, expanded != 0);
	rebuild_rows(state);
	if (explorer->canvas != R_INVALID_ID)
		Rwidget_invalidate(explorer->canvas);
}

int cair_project_explorer_guide_count(const CairProjectExplorer *explorer)
{
	const PEState *state;
	if (!explorer) return 0;
	state = (const PEState *)explorer->_state;
	return state ? state->guide_count : 0;
}

int cair_project_explorer_get_guide(const CairProjectExplorer *explorer,
	int index, int *depth, int *start_row, int *end_row)
{
	const PEState *state;
	if (!explorer) return 0;
	state = (const PEState *)explorer->_state;
	if (!state || index < 0 || index >= state->guide_count) return 0;
	if (depth)     *depth     = state->guides[index].depth;
	if (start_row) *start_row = state->guides[index].start_row;
	if (end_row)   *end_row   = state->guides[index].end_row;
	return 1;
}

int cair_project_explorer_get_row(const CairProjectExplorer *explorer,
	int index, const CairProjectNode **node, int *depth)
{
	const PEState *state;
	if (!explorer) return 0;
	state = (const PEState *)explorer->_state;
	if (!state || index < 0 || index >= state->row_count) return 0;
	if (node)  *node  = state->rows[index].node;
	if (depth) *depth = state->rows[index].depth;
	return 1;
}

int cair_project_explorer_has_icons(const CairProjectExplorer *explorer)
{
	const PEState *state;
	if (!explorer) return 0;
	state = (const PEState *)explorer->_state;
	if (!state) return 0;
	return state->icon_folder != R_INVALID_ID
		&& state->icon_folder_open != R_INVALID_ID;
}

void cair_project_explorer_destroy(CairProjectExplorer *explorer)
{
	PEState *state;
	int i;
	if (!explorer) return;
	state = (PEState *)explorer->_state;
	if (!state) return;

	pe_release_icons(state);
	for (i = 0; i < state->expanded_count; ++i) free(state->expanded[i]);
	free(state->expanded);
	free(state->root_path);
	free(state->rows);
	free(state->guides);
	free(state);

	explorer->_state    = NULL;
	explorer->root      = R_INVALID_ID;
	explorer->scroll_box = R_INVALID_ID;
	explorer->canvas    = R_INVALID_ID;
}
