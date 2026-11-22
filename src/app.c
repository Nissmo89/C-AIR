#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "app.h"
#include "model/project_tree.h"
#include "widgets/editor_tabs.h"
#include "widgets/find_replace_bar.h"
#include "widgets/project_explorer.h"
#include "widgets/status_bar.h"

#include <Raven.h>
#include <string.h>

typedef struct CairApp {
	Rid window;
	CairProjectTree project;
	CairProjectExplorer explorer;
	CairEditorTabs tabs;
	CairFindReplaceBar find_replace;
	CairStatusBar status;
} CairApp;

static CairApp app;

static void editor_changed(Rid editor, const char* path, void* user_data) {
	CairApp* state = (CairApp*)user_data;
	(void)path;
	if (!state || state->find_replace.editor == R_INVALID_ID) return;
	cair_find_replace_bar_set_editor(&state->find_replace, editor);
	if (editor != R_INVALID_ID) cair_find_replace_bar_refresh(&state->find_replace);
}

static void open_file(const CairProjectNode* node, void* user_data) {
	CairApp* state = (CairApp*)user_data;
	if (node && cair_editor_tabs_open_file(&state->tabs, node->path) != R_INVALID_ID) {
		cair_status_bar_set_text(&state->status, "Ready");
	} else {
		cair_status_bar_set_text(&state->status, "Could not read this file.");
	}
}

static int create_window(void) {
	char root_path[MAX_PATH];
	DWORD length = GetCurrentDirectoryA((DWORD)sizeof(root_path), root_path);
	Rid outer;
	Rid header;
	Rid splitter;
	Rid explorer;
	Rid editor;
	Rid find_replace;
	Rid status;
	if (length == 0 || length >= sizeof(root_path)) return 0;
	if (!cair_project_tree_scan(&app.project, root_path)) return 0;

	app.window = Rwindow_create("C-AIR", 1120, 720, R_WINDOW_CUSTOM_HEADER);
	if (app.window == R_INVALID_ID) return 0;
	outer = Rwidget_create(R_WIDGET_VBOX);
	header = Rheader_bar_create(NULL);
	splitter = Rwidget_create(R_WIDGET_SPLITTER);
	if (outer == R_INVALID_ID || header == R_INVALID_ID || splitter == R_INVALID_ID) return 0;
	Rwidget_add_child(outer, header);
	Rsplitter_set_orientation(splitter, R_HORIZONTAL);
	Rsplitter_set_min_pane_size(splitter, 180.0f);
	Rslot_set_fill(splitter, 1.0f);
	Rwidget_add_child(outer, splitter);
	explorer = cair_project_explorer_create(&app.explorer, &app.project, open_file, &app);
	editor = cair_editor_tabs_create(&app.tabs, editor_changed, &app);
	if (editor == R_INVALID_ID) return 0;
	find_replace = cair_find_replace_bar_create(&app.find_replace, cair_editor_tabs_get_active_editor(&app.tabs));
	status = cair_status_bar_create(&app.status);
	if (explorer == R_INVALID_ID || editor == R_INVALID_ID || find_replace == R_INVALID_ID || status == R_INVALID_ID) return 0;
	if (!cair_editor_tabs_add_toolbar(&app.tabs, find_replace)) return 0;
	Rslot_set_fill(explorer, 1.0f);
	Rwidget_add_child(splitter, explorer);
	Rslot_set_fill(editor, 3.0f);
	Rwidget_add_child(splitter, editor);
	Rwidget_add_child(outer, status);
	Rwindow_set_content(app.window, outer);
	cair_find_replace_bar_register_shortcuts(&app.find_replace, app.window);
	return 1;
}

int cair_app_run(int self_test) {
	int result = 0;
	if (!Rinit(self_test ? R_BACKEND_NULL : R_BACKEND_VULKAN)) return 1;
	/* Keep caret movement immediate; Raven currently exposes this globally. */
	Rset_animations_enabled(R_FALSE);
	memset(&app, 0, sizeof(app));
	app.find_replace.root = R_INVALID_ID;
	app.find_replace.editor = R_INVALID_ID;
	if (!create_window()) {
		result = 1;
	} else if (!self_test) {
		while (Rdraw_frame()) {}
	}
	Rshutdown();
	cair_editor_tabs_destroy(&app.tabs);
	cair_project_tree_destroy(&app.project);
	return result;
}
