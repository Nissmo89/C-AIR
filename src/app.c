#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "app.h"
#include "model/project_tree.h"
#include "widgets/editor_tabs.h"
#include "widgets/find_replace_bar.h"
#include "widgets/main_menu.h"
#include "widgets/project_explorer.h"
#include "widgets/status_bar.h"
#include "widgets/terminal_panel.h"

#include <Raven.h>
#include <string.h>

typedef struct CairApp {
	Rid window;
	CairProjectTree project;
	CairProjectExplorer explorer;
	CairEditorTabs tabs;
	CairMainMenu main_menu;
	CairFindReplaceBar find_replace;
	CairStatusBar status;
	CairTerminalPanel terminal;
	Rid right_splitter;  /* vertical splitter: editor on top, terminal below */
} CairApp;

static CairApp app;

static void editor_changed(Rid editor, const char* path, void* user_data) {
	CairApp* state = (CairApp*)user_data;
	(void)path;
	if (!state || state->find_replace.root == R_INVALID_ID) return;
	cair_find_replace_bar_set_editor(&state->find_replace, editor);
	if (editor != R_INVALID_ID) cair_find_replace_bar_refresh(&state->find_replace);
}

static void open_path(CairApp* state, const char* path) {
	if (state && path && cair_editor_tabs_open_file(&state->tabs, path) != R_INVALID_ID) {
		cair_status_bar_set_text(&state->status, "Ready");
	} else {
		cair_status_bar_set_text(&state->status, "Could not read this file.");
	}
}

static void open_file(const CairProjectNode* node, void* user_data) {
	if (node && !node->is_directory) open_path((CairApp*)user_data, node->path);
}

static void main_menu_action(CairMainMenuAction action, void* user_data) {
	CairApp* state = (CairApp*)user_data;
	Rid editor;
	if (!state) return;
	switch (action) {
		case CAIR_MAIN_MENU_OPEN_FILE: {
			static const RFileFilter filters[] = {
				{ "C and C++ source files", "*.c;*.h;*.cc;*.cpp;*.cxx;*.hpp" },
				{ "All files", "*" }
			};
			RFileDialogDesc desc;
			const char* path;
			memset(&desc, 0, sizeof(desc));
			desc.parent = state->window;
			desc.title = "Open File";
			desc.directory = state->project.root ? state->project.root->path : NULL;
			desc.filters = filters;
			desc.filter_count = (int)(sizeof(filters) / sizeof(filters[0]));
			path = Rfile_dialog_open(&desc);
			if (path) open_path(state, path);
			break;
		}
		case CAIR_MAIN_MENU_QUIT:
			Rwindow_close(state->window);
			break;
		case CAIR_MAIN_MENU_UNDO:
			editor = cair_editor_tabs_get_active_editor(&state->tabs);
			if (editor != R_INVALID_ID) Rtext_editor_undo(editor);
			break;
		case CAIR_MAIN_MENU_REDO:
			editor = cair_editor_tabs_get_active_editor(&state->tabs);
			if (editor != R_INVALID_ID) Rtext_editor_redo(editor);
			break;
		case CAIR_MAIN_MENU_FIND:
			cair_find_replace_bar_show(&state->find_replace, 0);
			break;
		case CAIR_MAIN_MENU_FIND_REPLACE:
			cair_find_replace_bar_show(&state->find_replace, 1);
			break;
		case CAIR_MAIN_MENU_TOGGLE_EXPLORER:
			Rwidget_set_visibility(state->explorer.root,
				Rwidget_get_visibility(state->explorer.root) == R_VISIBLE
				? R_COLLAPSED : R_VISIBLE);
			break;
		case CAIR_MAIN_MENU_TOGGLE_TERMINAL:
			cair_terminal_panel_toggle(&state->terminal);
			break;
		case CAIR_MAIN_MENU_ABOUT:
			Rmessage_box(state->window, "About C-AIR", "A lightweight C code editor.", R_MESSAGE_OK);
			break;
	}
}

static int create_window(void) {
	char root_path[MAX_PATH];
	DWORD length = GetCurrentDirectoryA((DWORD)sizeof(root_path), root_path);
	Rid outer;
	Rid header;
	Rid menu_bar;
	Rid h_splitter;   /* left: explorer  |  right: v_splitter */
	Rid v_splitter;   /* top:  editor    |  bottom: terminal  */
	Rid explorer;
	Rid editor;
	Rid find_replace;
	Rid terminal;
	Rid status;

	if (length == 0 || length >= sizeof(root_path)) return 0;
	if (!cair_project_tree_scan(&app.project, root_path)) return 0;

	app.window = Rwindow_create("C-AIR", 1120, 720, R_WINDOW_CUSTOM_HEADER);
	if (app.window == R_INVALID_ID) return 0;

	outer    = Rwidget_create(R_WIDGET_VBOX);
	header   = Rheader_bar_create(NULL);
	menu_bar = cair_main_menu_create(&app.main_menu, main_menu_action, &app);
	h_splitter = Rwidget_create(R_WIDGET_SPLITTER);
	v_splitter = Rwidget_create(R_WIDGET_SPLITTER);
	if (outer == R_INVALID_ID || header == R_INVALID_ID ||
	    menu_bar == R_INVALID_ID ||
	    h_splitter == R_INVALID_ID || v_splitter == R_INVALID_ID) return 0;

	app.right_splitter = v_splitter;

	/* Header */
	Rwidget_add_child(header, menu_bar);
	Rwidget_add_child(outer, header);

	/* Outer horizontal splitter fills the remaining space */
	Rsplitter_set_orientation(h_splitter, R_HORIZONTAL);
	Rsplitter_set_min_pane_size(h_splitter, 180.0f);
	Rslot_set_fill(h_splitter, 1.0f);
	Rwidget_add_child(outer, h_splitter);

	/* Left pane: project explorer */
	explorer = cair_project_explorer_create(&app.explorer, &app.project, open_file, &app);
	if (explorer == R_INVALID_ID) return 0;
	Rslot_set_fill(explorer, 1.0f);
	Rwidget_add_child(h_splitter, explorer);

	/* Right pane: vertical splitter (editor top / terminal bottom) */
	Rsplitter_set_orientation(v_splitter, R_VERTICAL);
	Rsplitter_set_min_pane_size(v_splitter, 60.0f);
	Rslot_set_fill(v_splitter, 3.0f);
	Rwidget_add_child(h_splitter, v_splitter);

	/* Editor tabs (top of vertical splitter) */
	editor = cair_editor_tabs_create(&app.tabs, editor_changed, &app);
	if (editor == R_INVALID_ID) return 0;
	find_replace = cair_find_replace_bar_create(&app.find_replace,
	    cair_editor_tabs_get_active_editor(&app.tabs));
	status = cair_status_bar_create(&app.status);
	if (find_replace == R_INVALID_ID || status == R_INVALID_ID) return 0;
	if (!cair_editor_tabs_add_toolbar(&app.tabs, find_replace)) return 0;
	Rslot_set_fill(editor, 3.0f);
	Rwidget_add_child(v_splitter, editor);

	/* Terminal panel (bottom of vertical splitter, initially collapsed) */
	terminal = cair_terminal_panel_create(&app.terminal);
	if (terminal == R_INVALID_ID) return 0;
	Rslot_set_fill(terminal, 1.0f);
	Rwidget_add_child(v_splitter, terminal);

	/* Status bar at the very bottom */
	Rwidget_add_child(outer, status);

	Rwindow_set_content(app.window, outer);
	/* Open the integrated command prompt with the app so users see a live
	 * terminal immediately; the View menu can still hide and restore it. */
	cair_terminal_panel_show(&app.terminal);
	cair_find_replace_bar_register_shortcuts(&app.find_replace, app.window);

	/* Ctrl+` shortcut also handled through the menu item — no extra binding needed */

	return 1;
}

int cair_app_run(int self_test) {
	int result = 0;
	if (!Rinit(self_test ? R_BACKEND_NULL : R_BACKEND_VULKAN)) return 1;
	Rset_animations_enabled(R_FALSE);
	memset(&app, 0, sizeof(app));
	app.find_replace.root = R_INVALID_ID;
	app.find_replace.editor = R_INVALID_ID;
	app.terminal.root   = R_INVALID_ID;
	app.terminal.canvas = R_INVALID_ID;
	app.terminal.header = R_INVALID_ID;
	if (!create_window()) {
		result = 1;
	} else if (!self_test) {
		while (Rdraw_frame()) {}
	}
	cair_terminal_panel_stop(&app.terminal);
	Rshutdown();
	cair_terminal_panel_destroy(&app.terminal);
	cair_project_explorer_destroy(&app.explorer);
	cair_editor_tabs_destroy(&app.tabs);
	cair_project_tree_destroy(&app.project);
	return result;
}
