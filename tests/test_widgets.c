#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <Raven.h>

#include "widgets/editor_pane.h"
#include "widgets/editor_tabs.h"
#include "widgets/find_replace_bar.h"
#include "widgets/main_menu.h"
#include "widgets/project_explorer.h"
#include "widgets/status_bar.h"
#include "widgets/terminal_panel.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void on_open(const CairProjectNode* node, void* user_data) {
	int* opened = (int*)user_data;
	if (node && node->path) *opened = 1;
}

static int color_matches(float a, float b) {
	float d = a - b;
	if (d < 0.0f) d = -d;
	return d < 0.002f;
}

typedef struct TabObserver {
	Rid editor;
	int changes;
} TabObserver;

static void tabs_changed(Rid editor, const char* path, void* user_data) {
	TabObserver* observer = (TabObserver*)user_data;
	(void)path;
	observer->editor = editor;
	observer->changes++;
}

static int widget_color_matches_theme(Rid widget, RColorRole role) {
	float actual[4];
	float expected[4];
	int i;
	Rwidget_get_color(widget, role, &actual[0], &actual[1], &actual[2], &actual[3]);
	Rtheme_get_color(role, &expected[0], &expected[1], &expected[2], &expected[3]);
	for (i = 0; i < 4; ++i) {
		float difference = actual[i] - expected[i];
		if (difference < -0.001f || difference > 0.001f) return 0;
	}
	return 1;
}

static int editor_colors_follow_theme(const CairEditorTabs* tabs, const CairEditorPane* pane) {
	RLook original = Rlook_get();
	int follows_theme = 1;
	RColorRole tab_roles[] = {
		R_COLOR_HEADER, R_COLOR_HEADER_HOVERED, R_COLOR_TAB_ACTIVE, R_COLOR_SEPARATOR,
		R_COLOR_TEXT, R_COLOR_TEXT_DISABLED, R_COLOR_CONTROL_PRESSED
	};
	RColorRole breadcrumb_roles[] = {
		R_COLOR_TAB_ACTIVE, R_COLOR_SEPARATOR, R_COLOR_TEXT, R_COLOR_ROW_HOVERED
	};
	size_t i;
	int look;
	for (look = 0; look < 2; ++look) {
		Rlook_set(look == 0 ? R_LOOK_RAVEN_DARK : R_LOOK_RAVEN_LIGHT);
		for (i = 0; i < sizeof(tab_roles) / sizeof(tab_roles[0]); ++i)
			if (!widget_color_matches_theme(tabs->tab_view, tab_roles[i])) follows_theme = 0;
		for (i = 0; i < sizeof(breadcrumb_roles) / sizeof(breadcrumb_roles[0]); ++i)
			if (!widget_color_matches_theme(pane->breadcrumb, breadcrumb_roles[i])) follows_theme = 0;
	}
	Rlook_set(original);
	return follows_theme;
}

int main(void) {
	CairProjectNode sample_file;
	CairProjectNode nested_file;
	CairProjectNode sample_dir;
	CairProjectNode* nested_children[1];
	CairProjectNode* root_children[2];
	CairProjectTree model;
	CairProjectNode root_node;
	CairProjectExplorer explorer;
	CairEditorPane editor;
	CairEditorTabs tabs;
	CairMainMenu menu;
	CairFindReplaceBar find_replace;
	CairStatusBar status;
	CairTerminalPanel terminal;
	Rid container;
	Rid tab_editor;
	Rid second_tab_editor;
	TabObserver observer;
	FILE* file;
	char temp[MAX_PATH];
	char path[MAX_PATH];
	char second_path[MAX_PATH];
	DWORD temp_length;
	int opened = 0;
	int failed = 0;
	int widgets_ready;
	int guide_depth;
	int guide_start;
	int guide_end;
	const CairProjectNode *row_node;
	int row_depth;

	memset(&sample_file, 0, sizeof(sample_file));
	memset(&nested_file, 0, sizeof(nested_file));
	memset(&sample_dir, 0, sizeof(sample_dir));
	memset(&root_node, 0, sizeof(root_node));
	memset(&model, 0, sizeof(model));
	memset(&explorer, 0, sizeof(explorer));
	memset(&editor, 0, sizeof(editor));
	memset(&tabs, 0, sizeof(tabs));
	memset(&menu, 0, sizeof(menu));
	memset(&find_replace, 0, sizeof(find_replace));
	memset(&status, 0, sizeof(status));
	memset(&terminal, 0, sizeof(terminal));
	terminal.root = R_INVALID_ID;
	terminal.header = R_INVALID_ID;
	terminal.canvas = R_INVALID_ID;
	observer.editor = R_INVALID_ID;
	observer.changes = 0;
	sample_file.name = "example.c";
	sample_file.path = "example.c";
	nested_file.name = "nested.c";
	nested_file.path = "src\\nested.c";
	nested_children[0] = &nested_file;
	sample_dir.name = "src";
	sample_dir.path = "src";
	sample_dir.is_directory = 1;
	sample_dir.children = nested_children;
	sample_dir.child_count = 1;
	root_children[0] = &sample_dir;
	root_children[1] = &sample_file;
	root_node.name = "sample";
	root_node.path = ".";
	root_node.is_directory = 1;
	root_node.children = root_children;
	root_node.child_count = 2;
	model.root = &root_node;
	model.node_count = 4;

	if (!Rinit(R_BACKEND_NULL)) return 1;
	container = Rwidget_create(R_WIDGET_VBOX);
	if (cair_project_explorer_create(&explorer, &model, on_open, &opened) == R_INVALID_ID) failed = 1;
	if (cair_editor_pane_create(&editor) == R_INVALID_ID) failed = 1;
	if (cair_editor_tabs_create(&tabs, tabs_changed, &observer) == R_INVALID_ID) failed = 1;
	if (cair_main_menu_create(&menu, NULL, NULL) == R_INVALID_ID) failed = 1;
	else if (Rmenu_bar_is_native(menu.root) != R_FALSE) failed = 1;
	if (cair_find_replace_bar_create(&find_replace, R_INVALID_ID) == R_INVALID_ID) failed = 1;
	if (find_replace.root != R_INVALID_ID) cair_find_replace_bar_set_editor(&find_replace, editor.editor);
	if (cair_status_bar_create(&status) == R_INVALID_ID) failed = 1;
	if (cair_terminal_panel_create(&terminal) == R_INVALID_ID) failed = 1;
	widgets_ready = container != R_INVALID_ID && explorer.root != R_INVALID_ID && explorer.canvas != R_INVALID_ID
		&& editor.root != R_INVALID_ID && editor.editor != R_INVALID_ID && tabs.root != R_INVALID_ID
		&& tabs.tab_view != R_INVALID_ID && menu.root != R_INVALID_ID && find_replace.root != R_INVALID_ID
		&& status.root != R_INVALID_ID && status.label != R_INVALID_ID
		&& terminal.root != R_INVALID_ID && terminal.canvas != R_INVALID_ID;
	if (!widgets_ready) failed = 1;
	if (widgets_ready) {
		if (Rwidget_get_visibility(editor.breadcrumb) != R_COLLAPSED
			|| Rwidget_get_visibility(tabs.tab_view) != R_COLLAPSED
			|| cair_editor_tabs_get_active_editor(&tabs) != R_INVALID_ID
			|| strcmp(Rtext_editor_get_text(editor.editor), "") != 0) failed = 1;
		if (!editor_colors_follow_theme(&tabs, &editor)) failed = 1;
		Rwidget_add_child(container, explorer.root);
		Rwidget_add_child(container, editor.root);
		Rwidget_add_child(container, tabs.root);
		Rwidget_add_child(container, status.root);
		Rwidget_add_child(container, terminal.root);
		if (Rwidget_get_type(terminal.canvas) != R_WIDGET_CUSTOM
			|| Rwidget_get_visibility(terminal.root) != R_COLLAPSED) failed = 1;
		if (!cair_editor_pane_add_toolbar(&editor, find_replace.root)) failed = 1;
		/* Explorer uses a custom canvas that paints Zed-style indent
		 * guides.  The scanned root is the first row and starts expanded,
		 * so the two top-level entries are visible below it; expanding
		 * "src" reveals its child. */
		if (Rwidget_get_type(explorer.canvas) != R_WIDGET_CUSTOM) failed = 1;
		if (cair_project_explorer_row_count(&explorer) != 3) failed = 1;
		/* The whole sidebar paints the panel color, not the darker window
		 * color, so the header and the empty area match the row strip. */
		{
			float pr, pg, pb, pa;
			float wr, wg, wb, wa;
			Rtheme_get_color(R_COLOR_PANEL, &pr, &pg, &pb, &pa);
			Rwidget_get_color(explorer.root, R_COLOR_PANEL, &wr, &wg, &wb, &wa);
			if (Rwidget_get_material(explorer.root) != R_MATERIAL_SOLID
				|| !color_matches(pr, wr) || !color_matches(pg, wg)
				|| !color_matches(pb, wb) || !color_matches(pa, wa)) failed = 1;
		}
		/* The folder SVGs in assets/ must be loaded, otherwise the panel
		 * silently falls back to the old painted chevrons. */
		if (!cair_project_explorer_has_icons(&explorer)) failed = 1;
		/* Row 0 is the project root itself, at depth 0. */
		if (!cair_project_explorer_get_row(&explorer, 0, &row_node, &row_depth)
			|| row_node != &root_node || row_depth != 0) failed = 1;
		if (!cair_project_explorer_get_row(&explorer, 1, &row_node, &row_depth)
			|| row_node != &sample_dir || row_depth != 1) failed = 1;
		if (!cair_project_explorer_get_row(&explorer, 2, &row_node, &row_depth)
			|| row_node != &sample_file || row_depth != 1) failed = 1;
		/* The root's guide covers its two visible child rows (1 and 2). */
		if (cair_project_explorer_guide_count(&explorer) != 1
			|| !cair_project_explorer_get_guide(&explorer, 0,
				&guide_depth, &guide_start, &guide_end)
			|| guide_depth != 0 || guide_start != 1 || guide_end != 2) failed = 1;
		cair_project_explorer_set_directory_expanded(&explorer, "src", 1);
		if (cair_project_explorer_row_count(&explorer) != 4) failed = 1;
		if (!cair_project_explorer_get_row(&explorer, 2, &row_node, &row_depth)
			|| row_node != &nested_file || row_depth != 2) failed = 1;
		/* Zed geometry: the "src" guide at depth 1 covers row 2 only, and
		 * the root guide at depth 0 now covers rows 1 through 3.  Guides
		 * are emitted when they close, so the deeper one comes first. */
		if (cair_project_explorer_guide_count(&explorer) != 2
			|| !cair_project_explorer_get_guide(&explorer, 0,
				&guide_depth, &guide_start, &guide_end)
			|| guide_depth != 1 || guide_start != 2 || guide_end != 2
			|| !cair_project_explorer_get_guide(&explorer, 1,
				&guide_depth, &guide_start, &guide_end)
			|| guide_depth != 0 || guide_start != 1 || guide_end != 3) failed = 1;
		cair_project_explorer_set_directory_expanded(&explorer, "src", 0);
		if (cair_project_explorer_row_count(&explorer) != 3) failed = 1;
		if (cair_project_explorer_guide_count(&explorer) != 1
			|| !cair_project_explorer_get_guide(&explorer, 0,
				&guide_depth, &guide_start, &guide_end)
			|| guide_depth != 0 || guide_start != 1 || guide_end != 2) failed = 1;
		/* The root row itself can be collapsed; only the root remains. */
		cair_project_explorer_set_directory_expanded(&explorer, ".", 0);
		if (cair_project_explorer_row_count(&explorer) != 1) failed = 1;
		if (cair_project_explorer_guide_count(&explorer) != 0) failed = 1;
		if (!cair_project_explorer_get_row(&explorer, 0, &row_node, &row_depth)
			|| row_node != &root_node || row_depth != 0) failed = 1;
		cair_project_explorer_set_directory_expanded(&explorer, ".", 1);
		if (cair_project_explorer_row_count(&explorer) != 3) failed = 1;
		if (cair_project_explorer_guide_count(&explorer) != 1) failed = 1;
		on_open(&sample_file, &opened);
		if (!opened) failed = 1;
		cair_status_bar_set_text(&status, "integration test");
		cair_find_replace_bar_show(&find_replace, 1);
		if (Rwidget_get_visibility(find_replace.root) != R_VISIBLE
			|| Rwidget_get_visibility(find_replace.replace_row) != R_VISIBLE) failed = 1;
		cair_find_replace_bar_show(&find_replace, 0);
		if (Rwidget_get_visibility(find_replace.replace_row) != R_COLLAPSED) failed = 1;
		if (cair_editor_tabs_get_count(&tabs) != 0 || observer.editor != cair_editor_tabs_get_active_editor(&tabs)) failed = 1;
		temp_length = GetTempPathA((DWORD)sizeof(temp), temp);
		if (temp_length == 0 || temp_length >= sizeof(temp)
			|| _snprintf(path, sizeof(path), "%sCAIR-widget-test-%lu.c", temp, GetCurrentProcessId()) < 0) {
			failed = 1;
		} else {
			file = fopen(path, "wb");
			if (!file) failed = 1;
			else {
				const char* filename;
				fputs("int widget_test(void) { return 7; }\n", file);
				fclose(file);
				if (!cair_editor_pane_open_file(&editor, path)) failed = 1;
				if (Rwidget_get_visibility(editor.breadcrumb) != R_VISIBLE) failed = 1;
				filename = strrchr(path, '\\');
				if (!filename) filename = strrchr(path, '/');
				if (filename) ++filename;
				else filename = path;
				if (Rbreadcrumb_get_item_count(editor.breadcrumb) < 2
					|| strcmp(Rbreadcrumb_get_item_text(editor.breadcrumb,
						Rbreadcrumb_get_item_count(editor.breadcrumb) - 1), filename) != 0) failed = 1;
				tab_editor = cair_editor_tabs_open_file(&tabs, path);
				if (tab_editor == R_INVALID_ID || cair_editor_tabs_get_count(&tabs) != 1
					|| observer.editor != tab_editor || Rtab_view_get_active(tabs.tab_view) != 0
					|| Rwidget_get_visibility(tabs.tab_view) != R_VISIBLE) failed = 1;
				if (tabs.count != 1 || Rbreadcrumb_get_item_count(tabs.documents[0].pane.breadcrumb) < 2
					|| Rwidget_get_visibility(tabs.documents[0].pane.breadcrumb) != R_VISIBLE
					|| strcmp(Rbreadcrumb_get_item_text(tabs.documents[0].pane.breadcrumb,
						Rbreadcrumb_get_item_count(tabs.documents[0].pane.breadcrumb) - 1), filename) != 0) failed = 1;
				if (tab_editor != R_INVALID_ID
					&& strcmp(Rtext_editor_get_text(tab_editor), "int widget_test(void) { return 7; }\n") != 0) failed = 1;
				if (cair_editor_tabs_open_file(&tabs, path) != tab_editor || cair_editor_tabs_get_count(&tabs) != 1) failed = 1;
				if (_snprintf(second_path, sizeof(second_path), "%sCAIR-widget-test-%lu-second.c", temp,
					(unsigned long)GetCurrentProcessId()) < 0) failed = 1;
				else {
					file = fopen(second_path, "wb");
					if (!file) failed = 1;
					else {
						fputs("int second_widget_test(void) { return 9; }\n", file);
						fclose(file);
						second_tab_editor = cair_editor_tabs_open_file(&tabs, second_path);
						if (second_tab_editor == R_INVALID_ID || cair_editor_tabs_get_count(&tabs) != 2
							|| observer.editor != second_tab_editor || Rtab_view_get_active(tabs.tab_view) != 1) failed = 1;
						Rtab_view_show_tab(tabs.tab_view, Rtab_view_get_tab(tabs.tab_view, 0));
						if (observer.editor != tab_editor) failed = 1;
						Rtab_view_show_tab(tabs.tab_view, Rtab_view_get_tab(tabs.tab_view, 1));
						if (observer.editor != second_tab_editor) failed = 1;
						DeleteFileA(second_path);
					}
				}
				cair_find_replace_bar_set_editor(&find_replace, tab_editor);
				if (cair_find_replace_bar_get_match_count(&find_replace) != 0) failed = 1;
				cair_find_replace_bar_set_query(&find_replace, "widget_test");
				if (cair_find_replace_bar_get_match_count(&find_replace) != 1) failed = 1;
				cair_find_replace_bar_set_editor(&find_replace, editor.editor);
				cair_find_replace_bar_set_query(&find_replace, "");
				if (strcmp(Rtext_editor_get_text(editor.editor), "int widget_test(void) { return 7; }\n") != 0) failed = 1;
				if (strcmp(Rtext_editor_get_language(editor.editor), "C") != 0) failed = 1;
				cair_find_replace_bar_set_query(&find_replace, "widget_test");
				if (cair_find_replace_bar_get_match_count(&find_replace) != 1) failed = 1;
				if (!cair_find_replace_bar_find_next(&find_replace)) failed = 1;
				cair_find_replace_bar_set_replacement(&find_replace, "pane_test");
				if (!cair_find_replace_bar_replace_one(&find_replace)) failed = 1;
				if (strstr(Rtext_editor_get_text(editor.editor), "pane_test") == NULL) failed = 1;
				cair_find_replace_bar_set_query(&find_replace, "return");
				cair_find_replace_bar_set_replacement(&find_replace, "break");
				if (cair_find_replace_bar_replace_all(&find_replace) != 1) failed = 1;
				if (strstr(Rtext_editor_get_text(editor.editor), "break 7") == NULL) failed = 1;
				Rtext_editor_undo(editor.editor);
				if (strstr(Rtext_editor_get_text(editor.editor), "return 7") == NULL) failed = 1;
				Rtext_editor_redo(editor.editor);
				if (strstr(Rtext_editor_get_text(editor.editor), "break 7") == NULL) failed = 1;
				cair_find_replace_bar_set_editor(&find_replace, R_INVALID_ID);
				if (cair_find_replace_bar_get_match_count(&find_replace) != 0
					|| Rwidget_get_visibility(find_replace.root) != R_COLLAPSED) failed = 1;
				cair_find_replace_bar_close(&find_replace);

				/* Continuous text selection rendering integration tests */
				/* 1. Single character / very short selection */
				Rtext_editor_select(editor.editor, 0, 4, 0, 5);
				Rdraw_frame();

				/* 2. Single line word selection */
				Rtext_editor_select(editor.editor, 0, 4, 0, 15);
				Rdraw_frame();

				/* 3. Multi-line selection with partial rows */
				Rtext_editor_set_text(editor.editor,
					"void function_alpha(int a, int b) {\n"
					"    int value = a + b;\n"
					"    if (value > 10) {\n"
					"        return value * 2;\n"
					"    }\n"
					"    return 0;\n"
					"}\n");
				Rtext_editor_select(editor.editor, 0, 5, 3, 10);
				Rdraw_frame();

				/* 4. Reverse selection (drag bottom to top) */
				Rtext_editor_select(editor.editor, 3, 10, 0, 5);
				Rdraw_frame();

				/* 5. Inward step and outward step transitions */
				Rtext_editor_set_text(editor.editor,
					"short\n"
					"very_long_line_here_to_step_outward_and_then_inward\n"
					"mid_length_line\n");
				Rtext_editor_select(editor.editor, 0, 2, 2, 8);
				Rdraw_frame();

				/* 6. Multi-cursor selections */
				Rtext_editor_select(editor.editor, 0, 0, 0, 3);
				Rtext_editor_add_selection(editor.editor, 1, 0, 1, 5);
				Rdraw_frame();
				Rtext_editor_clear_cursors(editor.editor);

				/* 7. Soft wrapping selection */
				Rtext_editor_set_word_wrap(editor.editor, 1);
				Rtext_editor_set_text(editor.editor,
					"A long text line that will be wrapped across multiple visual rows inside the editor viewport when soft wrap is enabled.\n"
					"Second line following the wrapped block.\n");
				Rtext_editor_select(editor.editor, 0, 10, 1, 10);
				Rdraw_frame();
				Rtext_editor_set_word_wrap(editor.editor, 0);

				/* 8. Scrolling with selection */
				Rtext_editor_scroll_to_line(editor.editor, 1);
				Rdraw_frame();

				DeleteFileA(path);
			}
		}
	}
	cair_terminal_panel_destroy(&terminal);
	Rshutdown();
	cair_project_explorer_destroy(&explorer);
	cair_editor_tabs_destroy(&tabs);
	if (failed) {
		fputs("widget integration test failed\n", stderr);
		return 1;
	}
	puts("widget integration tests passed");
	return 0;
}
