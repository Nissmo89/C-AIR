#include "widgets/bottom_panel.h"

#include <string.h>

static void output_clicked(Rid button, void *user_data) {
	(void)button;
	cair_bottom_panel_select((CairBottomPanel *)user_data, CAIR_BOTTOM_PANEL_OUTPUT);
}

static void terminal_clicked(Rid button, void *user_data) {
	(void)button;
	cair_bottom_panel_select((CairBottomPanel *)user_data, CAIR_BOTTOM_PANEL_TERMINAL);
}

Rid cair_bottom_panel_create(CairBottomPanel *panel, CairTerminalPanel *terminal) {
	Rid header;
	Rid output_button;
	Rid terminal_button;

	if (!panel || !terminal || terminal->root == R_INVALID_ID) return R_INVALID_ID;
	memset(panel, 0, sizeof(*panel));
	panel->root = R_INVALID_ID;
	panel->tabs = R_INVALID_ID;
	panel->output_view = R_INVALID_ID;
	panel->output_label = R_INVALID_ID;
	panel->terminal = terminal;

	panel->root = Rwidget_create(R_WIDGET_VBOX);
	if (panel->root == R_INVALID_ID) return R_INVALID_ID;
	Rpanel_set_spacing(panel->root, 0.0f);

	header = Rwidget_create(R_WIDGET_HBOX);
	if (header == R_INVALID_ID) goto fail;
	Rpanel_set_spacing(header, 4.0f);
	Rslot_set_padding(header, 6.0f, 3.0f, 6.0f, 3.0f);
	Rwidget_set_min_size(header, 0.0f, 30.0f);
	output_button = Rbutton_create("Output");
	terminal_button = Rbutton_create("Terminal");
	if (output_button == R_INVALID_ID || terminal_button == R_INVALID_ID) goto fail;
	Rbutton_set_clicked_callback(output_button, output_clicked, panel);
	Rbutton_set_clicked_callback(terminal_button, terminal_clicked, panel);
	Rwidget_add_child(header, output_button);
	Rwidget_add_child(header, terminal_button);
	Rwidget_add_child(panel->root, header);

	panel->output_view = Rwidget_create(R_WIDGET_VBOX);
	if (panel->output_view == R_INVALID_ID) goto fail;
	panel->output_label = Rlabel_create("No output yet.");
	if (panel->output_label == R_INVALID_ID) goto fail;
	Rwidget_add_child(panel->output_view, panel->output_label);
	Rslot_set_fill(panel->output_view, 1.0f);
	Rwidget_add_child(panel->root, panel->output_view);

	/* The terminal retains its own lifecycle and is shown when its tab is selected. */
	Rwidget_add_child(panel->root, terminal->root);
	panel->active_tab = CAIR_BOTTOM_PANEL_OUTPUT;
	Rwidget_set_visibility(panel->root, R_COLLAPSED);
	return panel->root;

fail:
	Rwidget_destroy(panel->root);
	panel->root = R_INVALID_ID;
	return R_INVALID_ID;
}

void cair_bottom_panel_select(CairBottomPanel *panel, CairBottomPanelTab tab) {
	if (!panel || panel->root == R_INVALID_ID || !panel->terminal) return;
	if (tab != CAIR_BOTTOM_PANEL_OUTPUT && tab != CAIR_BOTTOM_PANEL_TERMINAL) return;
	panel->active_tab = tab;
	if (tab == CAIR_BOTTOM_PANEL_OUTPUT) {
		Rwidget_set_visibility(panel->output_view, R_VISIBLE);
		cair_terminal_panel_hide(panel->terminal);
	} else {
		Rwidget_set_visibility(panel->output_view, R_COLLAPSED);
		cair_terminal_panel_show(panel->terminal);
	}
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
