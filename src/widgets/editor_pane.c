#include "widgets/editor_pane.h"
#include "widgets/breadcrumb_bar.h"

Rid cair_editor_pane_create(CairEditorPane* pane) {
	if (!pane) return R_INVALID_ID;
	pane->root = Rwidget_create(R_WIDGET_VBOX);
	if (pane->root == R_INVALID_ID) return R_INVALID_ID;
	Rpanel_set_spacing(pane->root, 0.0f);
	pane->breadcrumb = cair_breadcrumb_bar_create();
	if (pane->breadcrumb == R_INVALID_ID) return R_INVALID_ID;
	Rwidget_set_visibility(pane->breadcrumb, R_COLLAPSED);
	Rwidget_add_child(pane->root, pane->breadcrumb);
	pane->toolbar_host = Rwidget_add_child(pane->root, Rwidget_create(R_WIDGET_VBOX));
	if (pane->toolbar_host == R_INVALID_ID) return R_INVALID_ID;
	pane->editor = Rwidget_add_child(pane->root, Rtext_editor_create(NULL));
	if (pane->editor == R_INVALID_ID) return R_INVALID_ID;
	Rslot_set_fill(pane->editor, 1.0f);
	Rtext_editor_set_language(pane->editor, "C");
	Rtext_editor_set_line_numbers(pane->editor, R_TRUE);
	Rtext_editor_set_tab_size(pane->editor, 4);
	Rtext_editor_set_tab_inserts_spaces(pane->editor, R_TRUE);
	cair_editor_pane_set_welcome_text(pane);
	return pane->root;
}

int cair_editor_pane_add_toolbar(CairEditorPane* pane, Rid toolbar) {
	if (!pane || pane->toolbar_host == R_INVALID_ID || toolbar == R_INVALID_ID) return 0;
	return Rwidget_add_child(pane->toolbar_host, toolbar) != R_INVALID_ID;
}

int cair_editor_pane_open_file(CairEditorPane* pane, const char* path) {
	char* text;
	if (!pane || !path) return 0;
	text = Rfile_read(path, NULL);
	if (!text) return 0;
	Rtext_editor_set_text(pane->editor, text);
	Rmemory_free(text);
	Rtext_editor_set_language(pane->editor, Rlanguage_for_file(path));
	cair_breadcrumb_bar_set_path(pane->breadcrumb, path);
	Rwidget_set_visibility(pane->breadcrumb, R_VISIBLE);
	return 1;
}

void cair_editor_pane_set_welcome_text(CairEditorPane* pane) {
	if (!pane) return;
	Rtext_editor_set_text(pane->editor,
		"/* C-AIR: a lightweight C editor */\n"
		"#include <stdio.h>\n\n"
		"int main(void) {\n"
		"    puts(\"Hello from C-AIR\");\n"
		"    return 0;\n"
		"}\n");
}
