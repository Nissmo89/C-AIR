#include "widgets/bottom_panel.h"

#include <string.h>

static void apply_tab(CairBottomPanel *panel, int index) {
	if (!panel || !panel->terminal) return;
	if (index == CAIR_BOTTOM_PANEL_TERMINAL) {
		panel->active_tab = CAIR_BOTTOM_PANEL_TERMINAL;
		cair_terminal_panel_show(panel->terminal);
	} else {
		panel->active_tab = CAIR_BOTTOM_PANEL_OUTPUT;
		cair_terminal_panel_hide(panel->terminal);
	}
}

static void tab_changed(Rid tab_view, int index, void *user_data) {
	(void)tab_view;
	apply_tab((CairBottomPanel *)user_data, index);
}

Rid cair_bottom_panel_create(CairBottomPanel *panel, CairTerminalPanel *terminal) {
	Rid top_border;

	if (!panel || !terminal || terminal->root == R_INVALID_ID) return R_INVALID_ID;
	memset(panel, 0, sizeof(*panel));
	panel->root = R_INVALID_ID;
	panel->tabs = R_INVALID_ID;
	panel->output_view = R_INVALID_ID;
	panel->output_label = R_INVALID_ID;
	panel->terminal = terminal;

	panel->root = Rwidget_create(R_WIDGET_VBOX);
	if (panel->root == R_INVALID_ID) goto fail;
	Rpanel_set_spacing(panel->root, 0.0f);

	top_border = Rwidget_create(R_WIDGET_SEPARATOR);
	if (top_border == R_INVALID_ID) goto fail;
	Rseparator_set_orientation(top_border, R_HORIZONTAL);
	Rwidget_add_child(panel->root, top_border);

	panel->tabs = Rwidget_create(R_WIDGET_TAB_VIEW);
	panel->output_view = Rwidget_create(R_WIDGET_VBOX);
	if (panel->tabs == R_INVALID_ID || panel->output_view == R_INVALID_ID) goto fail;

	panel->output_label = Rlabel_create("No output yet.");
	if (panel->output_label == R_INVALID_ID) goto fail;
	Rwidget_add_child(panel->output_view, panel->output_label);

	/* Match the editor's Zed-style document tabs, without closable pages. */
	Rtab_view_set_documents(panel->tabs, R_TRUE);
	Rwidget_set_metric(panel->tabs, R_METRIC_CONTROL_HEIGHT, 28.0f);
	if (Rtab_view_add_tab(panel->tabs, "Output", panel->output_view) < 0 ||
	    Rtab_view_add_tab(panel->tabs, "Terminal", terminal->root) < 0) goto fail;
	Rtab_view_set_tab_closable(panel->tabs, panel->output_view, R_FALSE);
	Rtab_view_set_tab_closable(panel->tabs, terminal->root, R_FALSE);
	Rtab_view_set_changed_callback(panel->tabs, tab_changed, panel);
	Rslot_set_fill(panel->tabs, 1.0f);
	Rwidget_add_child(panel->root, panel->tabs);

	panel->active_tab = CAIR_BOTTOM_PANEL_OUTPUT;
	Rtab_view_set_active(panel->tabs, CAIR_BOTTOM_PANEL_OUTPUT);
	Rwidget_set_visibility(panel->root, R_COLLAPSED);
	return panel->root;

fail:
	if (panel->root != R_INVALID_ID) Rwidget_destroy(panel->root);
	else {
		if (panel->tabs != R_INVALID_ID) Rwidget_destroy(panel->tabs);
		if (panel->output_view != R_INVALID_ID) Rwidget_destroy(panel->output_view);
	}
	panel->root = R_INVALID_ID;
	panel->tabs = R_INVALID_ID;
	panel->output_view = R_INVALID_ID;
	return R_INVALID_ID;
}

void cair_bottom_panel_select(CairBottomPanel *panel, CairBottomPanelTab tab) {
	if (!panel || panel->root == R_INVALID_ID || panel->tabs == R_INVALID_ID) return;
	if (tab != CAIR_BOTTOM_PANEL_OUTPUT && tab != CAIR_BOTTOM_PANEL_TERMINAL) return;
	Rtab_view_set_active(panel->tabs, (int)tab);
	apply_tab(panel, (int)tab);
}

void cair_bottom_panel_show(CairBottomPanel *panel) {
	if (!panel || panel->root == R_INVALID_ID) return;
	Rwidget_set_visibility(panel->root, R_VISIBLE);
	cair_bottom_panel_select(panel, panel->active_tab);
}

void cair_bottom_panel_hide(CairBottomPanel *panel) {
	if (!panel || panel->root == R_INVALID_ID) return;
	Rwidget_set_visibility(panel->root, R_COLLAPSED);
}

void cair_bottom_panel_toggle(CairBottomPanel *panel) {
	if (!panel || panel->root == R_INVALID_ID) return;
	if (Rwidget_get_visibility(panel->root) == R_VISIBLE) cair_bottom_panel_hide(panel);
	else cair_bottom_panel_show(panel);
}
