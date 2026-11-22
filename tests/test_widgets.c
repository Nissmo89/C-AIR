#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <Raven.h>

#include "widgets/editor_pane.h"
#include "widgets/find_replace_bar.h"
#include "widgets/project_explorer.h"
#include "widgets/status_bar.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void on_open(const CairProjectNode* node, void* user_data) {
	int* opened = (int*)user_data;
	if (node && node->path) *opened = 1;
}

int main(void) {
	CairProjectNode sample_file;
	CairProjectNode* root_children[1];
	CairProjectTree model;
	CairProjectNode root_node;
	CairProjectExplorer explorer;
	CairEditorPane editor;
	CairFindReplaceBar find_replace;
	CairStatusBar status;
	Rid container;
	Rid root_item;
	Rid file_item;
	char temp[MAX_PATH];
	char path[MAX_PATH];
	FILE* file;
	DWORD temp_length;
	int opened = 0;
	int failed = 0;
	int widgets_ready;

	memset(&sample_file, 0, sizeof(sample_file));
	memset(&root_node, 0, sizeof(root_node));
	memset(&model, 0, sizeof(model));
	memset(&explorer, 0, sizeof(explorer));
	memset(&editor, 0, sizeof(editor));
	memset(&find_replace, 0, sizeof(find_replace));
	memset(&status, 0, sizeof(status));
	sample_file.name = "example.c";
	sample_file.path = "example.c";
	root_children[0] = &sample_file;
	root_node.name = "sample";
	root_node.path = ".";
	root_node.is_directory = 1;
	root_node.children = root_children;
	root_node.child_count = 1;
	model.root = &root_node;
	model.node_count = 2;

	if (!Rinit(R_BACKEND_NULL)) return 1;
	container = Rwidget_create(R_WIDGET_VBOX);
	if (cair_project_explorer_create(&explorer, &model, on_open, &opened) == R_INVALID_ID) failed = 1;
	if (cair_editor_pane_create(&editor) == R_INVALID_ID) failed = 1;
	if (cair_find_replace_bar_create(&find_replace, editor.editor) == R_INVALID_ID) failed = 1;
	if (cair_status_bar_create(&status) == R_INVALID_ID) failed = 1;
	widgets_ready = container != R_INVALID_ID && explorer.root != R_INVALID_ID && explorer.tree != R_INVALID_ID
		&& editor.root != R_INVALID_ID && editor.editor != R_INVALID_ID && find_replace.root != R_INVALID_ID
		&& status.root != R_INVALID_ID && status.label != R_INVALID_ID;
	if (!widgets_ready) failed = 1;
	if (widgets_ready) {
		Rwidget_add_child(container, explorer.root);
		Rwidget_add_child(container, editor.root);
		Rwidget_add_child(container, status.root);
		if (!cair_editor_pane_add_toolbar(&editor, find_replace.root)) failed = 1;
		root_item = Rtree_view_get_item(explorer.tree, 0);
		file_item = root_item != R_INVALID_ID ? Rtree_item_get_child(root_item, 0) : R_INVALID_ID;
		if (file_item == R_INVALID_ID || Rtree_item_get_user_handle(file_item) != &sample_file) failed = 1;
		else Rtree_view_set_selected_item(explorer.tree, file_item);
		on_open(&sample_file, &opened);
		if (!opened) failed = 1;
		cair_status_bar_set_text(&status, "integration test");
		cair_find_replace_bar_show(&find_replace, 1);
		if (Rwidget_get_visibility(find_replace.root) != R_VISIBLE
			|| Rwidget_get_visibility(find_replace.replace_row) != R_VISIBLE) failed = 1;
		cair_find_replace_bar_show(&find_replace, 0);
		if (Rwidget_get_visibility(find_replace.replace_row) != R_COLLAPSED) failed = 1;
		temp_length = GetTempPathA((DWORD)sizeof(temp), temp);
		if (temp_length == 0 || temp_length >= sizeof(temp)
			|| _snprintf(path, sizeof(path), "%sCAIR-widget-test-%lu.c", temp, GetCurrentProcessId()) < 0) {
			failed = 1;
		} else {
			file = fopen(path, "wb");
			if (!file) failed = 1;
			else {
				fputs("int widget_test(void) { return 7; }\n", file);
				fclose(file);
				if (!cair_editor_pane_open_file(&editor, path)) failed = 1;
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
				DeleteFileA(path);
			}
		}
	}
	Rshutdown();
	if (failed) {
		fputs("widget integration test failed\n", stderr);
		return 1;
	}
	puts("widget integration tests passed");
	return 0;
}
