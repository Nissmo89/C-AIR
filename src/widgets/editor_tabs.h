#ifndef CAIR_EDITOR_TABS_H
#define CAIR_EDITOR_TABS_H

#include "widgets/editor_pane.h"

#include <stddef.h>

typedef struct CairEditorDocument {
	CairEditorPane pane;
	char* path;
} CairEditorDocument;

typedef void (*CairEditorTabsChangedCallback)(Rid editor, const char* path, void* user_data);

typedef struct CairEditorTabs {
	Rid root;
	Rid toolbar_host;
	Rid tab_view;
	Rid active_editor;
	CairEditorPane welcome;
	CairEditorDocument* documents;
	size_t count;
	size_t capacity;
	CairEditorTabsChangedCallback on_changed;
	void* user_data;
} CairEditorTabs;

Rid cair_editor_tabs_create(CairEditorTabs* tabs, CairEditorTabsChangedCallback on_changed, void* user_data);
int cair_editor_tabs_add_toolbar(CairEditorTabs* tabs, Rid toolbar);
Rid cair_editor_tabs_open_file(CairEditorTabs* tabs, const char* path);
Rid cair_editor_tabs_get_active_editor(const CairEditorTabs* tabs);
size_t cair_editor_tabs_get_count(const CairEditorTabs* tabs);
void cair_editor_tabs_destroy(CairEditorTabs* tabs);

#endif
