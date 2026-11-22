#include "widgets/editor_tabs.h"

#include <stdlib.h>
#include <string.h>

static char* copy_string(const char* text) {
	size_t length;
	char* copy;
	if (!text) return NULL;
	length = strlen(text);
	if (length == (size_t)-1) return NULL;
	copy = (char*)malloc(length + 1);
	if (!copy) return NULL;
	memcpy(copy, text, length + 1);
	return copy;
}

static int reserve_document(CairEditorTabs* tabs) {
	CairEditorDocument* documents;
	size_t capacity;
	if (tabs->count < tabs->capacity) return 1;
	capacity = tabs->capacity ? tabs->capacity * 2 : 4;
	if (capacity < tabs->capacity || capacity > (size_t)-1 / sizeof(*documents)) return 0;
	documents = (CairEditorDocument*)realloc(tabs->documents, capacity * sizeof(*documents));
	if (!documents) return 0;
	tabs->documents = documents;
	tabs->capacity = capacity;
	return 1;
}

static const char* tab_title(const char* path) {
	const char* title = path;
	const char* cursor;
	for (cursor = path; *cursor; ++cursor) {
		if (*cursor == '/' || *cursor == '\\') title = cursor + 1;
	}
	return title;
}

static CairEditorDocument* document_for_page(CairEditorTabs* tabs, Rid page) {
	size_t i;
	for (i = 0; i < tabs->count; ++i) {
		if (tabs->documents[i].pane.root == page) return &tabs->documents[i];
	}
	return NULL;
}

static void tab_changed(Rid tab_view, int index, void* user_data) {
	CairEditorTabs* tabs = (CairEditorTabs*)user_data;
	CairEditorDocument* document;
	Rid page;
	if (!tabs) return;
	if (index < 0) {
		tabs->active_editor = R_INVALID_ID;
		if (tabs->count == 0) Rwidget_set_visibility(tabs->tab_view, R_COLLAPSED);
		if (tabs->on_changed) tabs->on_changed(R_INVALID_ID, NULL, tabs->user_data);
		return;
	}
	page = Rtab_view_get_tab(tab_view, index);
	document = document_for_page(tabs, page);
	if (!document) return;
	tabs->active_editor = document->pane.editor;
	if (tabs->on_changed) tabs->on_changed(document->pane.editor, document->path, tabs->user_data);
}

static int tab_closing(Rid tab_view, int index, Rid page, void* user_data) {
	CairEditorTabs* tabs = (CairEditorTabs*)user_data;
	size_t i;
	(void)tab_view;
	(void)index;
	if (!tabs) return R_FALSE;
	for (i = 0; i < tabs->count; ++i) {
		if (tabs->documents[i].pane.root == page) {
			free(tabs->documents[i].path);
			if (i + 1 < tabs->count)
				memmove(&tabs->documents[i], &tabs->documents[i + 1], (tabs->count - i - 1) * sizeof(*tabs->documents));
			--tabs->count;
			if (tabs->count == 0) {
				Rwidget_set_visibility(tabs->tab_view, R_COLLAPSED);
				tabs->active_editor = R_INVALID_ID;
				if (tabs->on_changed) tabs->on_changed(R_INVALID_ID, NULL, tabs->user_data);
			}
			return R_TRUE;
		}
	}
	return R_FALSE;
}

static int add_document(CairEditorTabs* tabs, CairEditorDocument* document, const char* title) {
	if (!reserve_document(tabs)) return 0;
	if (Rtab_view_add_tab(tabs->tab_view, title, document->pane.root) < 0) return 0;
	tabs->documents[tabs->count++] = *document;
	Rwidget_set_visibility(tabs->tab_view, R_VISIBLE);
	Rslot_set_fill(document->pane.root, 1.0f);
	Rtab_view_show_tab(tabs->tab_view, document->pane.root);
	if (tabs->active_editor != document->pane.editor)
		tab_changed(tabs->tab_view, Rtab_view_get_active(tabs->tab_view), tabs);
	return 1;
}

Rid cair_editor_tabs_create(CairEditorTabs* tabs, CairEditorTabsChangedCallback on_changed, void* user_data) {
	if (!tabs) return R_INVALID_ID;
	memset(tabs, 0, sizeof(*tabs));
	tabs->root = tabs->toolbar_host = tabs->tab_view = tabs->active_editor = R_INVALID_ID;
	tabs->on_changed = on_changed;
	tabs->user_data = user_data;
	tabs->root = Rwidget_create(R_WIDGET_VBOX);
	tabs->toolbar_host = Rwidget_create(R_WIDGET_VBOX);
	tabs->tab_view = Rwidget_create(R_WIDGET_TAB_VIEW);
	if (tabs->root == R_INVALID_ID || tabs->toolbar_host == R_INVALID_ID || tabs->tab_view == R_INVALID_ID) return R_INVALID_ID;
	Rwidget_add_child(tabs->root, tabs->toolbar_host);
	Rslot_set_fill(tabs->tab_view, 1.0f);
	Rwidget_set_visibility(tabs->tab_view, R_COLLAPSED);
	Rtab_view_set_documents(tabs->tab_view, R_TRUE);
	Rwidget_set_metric(tabs->tab_view, R_METRIC_CONTROL_HEIGHT, 28.0f);
	Rwidget_add_child(tabs->root, tabs->tab_view);
	Rtab_view_set_changed_callback(tabs->tab_view, tab_changed, tabs);
	Rtab_view_set_close_callback(tabs->tab_view, tab_closing, tabs);

	if (tabs->on_changed) tabs->on_changed(R_INVALID_ID, NULL, tabs->user_data);
	return tabs->root;
}

int cair_editor_tabs_add_toolbar(CairEditorTabs* tabs, Rid toolbar) {
	if (!tabs || tabs->toolbar_host == R_INVALID_ID || toolbar == R_INVALID_ID) return 0;
	return Rwidget_add_child(tabs->toolbar_host, toolbar) != R_INVALID_ID;
}

Rid cair_editor_tabs_open_file(CairEditorTabs* tabs, const char* path) {
	CairEditorDocument document;
	size_t i;
	if (!tabs || !path || !path[0]) return R_INVALID_ID;
	for (i = 0; i < tabs->count; ++i) {
		if (tabs->documents[i].path && strcmp(tabs->documents[i].path, path) == 0) {
			Rtab_view_show_tab(tabs->tab_view, tabs->documents[i].pane.root);
			return tabs->documents[i].pane.editor;
		}
	}
	memset(&document, 0, sizeof(document));
	document.pane.root = document.pane.breadcrumb = document.pane.toolbar_host = document.pane.editor = R_INVALID_ID;
	if (cair_editor_pane_create(&document.pane) == R_INVALID_ID) {
		if (document.pane.root != R_INVALID_ID) Rwidget_destroy(document.pane.root);
		return R_INVALID_ID;
	}
	if (!cair_editor_pane_open_file(&document.pane, path)) {
		Rwidget_destroy(document.pane.root);
		return R_INVALID_ID;
	}
	document.path = copy_string(path);
	if (!document.path) {
		Rwidget_destroy(document.pane.root);
		return R_INVALID_ID;
	}
	if (!add_document(tabs, &document, tab_title(path))) {
		free(document.path);
		Rwidget_destroy(document.pane.root);
		return R_INVALID_ID;
	}
	return document.pane.editor;
}

Rid cair_editor_tabs_get_active_editor(const CairEditorTabs* tabs) {
	return tabs ? tabs->active_editor : R_INVALID_ID;
}

size_t cair_editor_tabs_get_count(const CairEditorTabs* tabs) {
	return tabs ? tabs->count : 0;
}

void cair_editor_tabs_destroy(CairEditorTabs* tabs) {
	size_t i;
	if (!tabs) return;
	for (i = 0; i < tabs->count; ++i) free(tabs->documents[i].path);
	free(tabs->documents);
	memset(tabs, 0, sizeof(*tabs));
	tabs->root = tabs->toolbar_host = tabs->tab_view = tabs->active_editor = R_INVALID_ID;
}
