#include "widgets/status_bar.h"

Rid cair_status_bar_create(CairStatusBar* bar) {
	if (!bar) return R_INVALID_ID;
	bar->root = Rwidget_create(R_WIDGET_HBOX);
	if (bar->root == R_INVALID_ID) return R_INVALID_ID;
	Rpanel_set_spacing(bar->root, 8.0f);
	bar->label = Rlabel_create("Choose a file from the project tree");
	if (bar->label == R_INVALID_ID) return R_INVALID_ID;
	Rwidget_add_child(bar->root, bar->label);
	return bar->root;
}

void cair_status_bar_set_text(CairStatusBar* bar, const char* text) {
	if (bar && text) Rlabel_set_text(bar->label, text);
}
