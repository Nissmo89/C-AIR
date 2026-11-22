#ifndef CAIR_EDITOR_PANE_H
#define CAIR_EDITOR_PANE_H

#include <Raven.h>

typedef struct CairEditorPane {
	Rid root;
	Rid file_label;
	Rid toolbar_host;
	Rid editor;
} CairEditorPane;

Rid cair_editor_pane_create(CairEditorPane* pane);
int cair_editor_pane_add_toolbar(CairEditorPane* pane, Rid toolbar);
int cair_editor_pane_open_file(CairEditorPane* pane, const char* path);
void cair_editor_pane_set_welcome_text(CairEditorPane* pane);

#endif
