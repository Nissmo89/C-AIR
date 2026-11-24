#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <Raven.h>
#include "widgets/terminal_panel.h"

#include <string.h>

static int terminal_has_text(CairTerminalPanel *panel, const char *needle) {
	uint16_t cols, rows, row;
	char line[4096];
	if (!cair_terminal_model_get_dimensions(&panel->model, &cols, &rows)) return 0;
	(void)cols;
	for (row = 0; row < rows; ++row) {
		if (cair_terminal_model_read_row_text(&panel->model, row, line, sizeof(line)) >= 0 &&
			strstr(line, needle)) return 1;
	}
	return 0;
}

int main(void) {
	static const char command[] = "echo cair_widget_input_test";
	CairTerminalPanel panel;
	Rid window, root;
	int found = 0;
	int i;

	memset(&panel, 0, sizeof(panel));
	panel.root = panel.header = panel.canvas = R_INVALID_ID;
	if (!Rinit(R_BACKEND_NULL)) return 1;
	window = Rwindow_create("Terminal input test", 640, 360, 0);
	root = Rwidget_create(R_WIDGET_VBOX);
	if (window == R_INVALID_ID || root == R_INVALID_ID ||
		cair_terminal_panel_create(&panel) == R_INVALID_ID) {
		Rshutdown();
		return 2;
	}
	Rwidget_add_child(root, panel.root);
	Rwindow_set_content(window, root);
	SetEnvironmentVariableA("CAIR_TERMINAL_FORCE_PIPES", "1");
	cair_terminal_panel_show(&panel);
	Rwidget_focus(panel.canvas);
	Rwindow_inject_text(window, command);
	Rwindow_inject_key(window, R_KEY_ENTER, 1, 0);
	Rwindow_inject_key(window, R_KEY_ENTER, 0, 0);

	for (i = 0; i < 150 && !found; ++i) {
		Rdraw_frame();
		found = terminal_has_text(&panel, "cair_widget_input_test");
		if (!found) Sleep(20);
	}

	cair_terminal_panel_stop(&panel);
	Rshutdown();
	cair_terminal_panel_destroy(&panel);
	SetEnvironmentVariableA("CAIR_TERMINAL_FORCE_PIPES", NULL);
	return found ? 0 : 3;
}
