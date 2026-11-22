#include "widgets/main_menu.h"

#include <string.h>

static void dispatch(CairMainMenu* menu, CairMainMenuAction action) {
	if (menu && menu->on_action) menu->on_action(action, menu->user_data);
}

static void open_file_clicked(Rid item, void* user_data) {
	(void)item;
	dispatch((CairMainMenu*)user_data, CAIR_MAIN_MENU_OPEN_FILE);
}

static void quit_clicked(Rid item, void* user_data) {
	(void)item;
	dispatch((CairMainMenu*)user_data, CAIR_MAIN_MENU_QUIT);
}

static void undo_clicked(Rid item, void* user_data) {
	(void)item;
	dispatch((CairMainMenu*)user_data, CAIR_MAIN_MENU_UNDO);
}

static void redo_clicked(Rid item, void* user_data) {
	(void)item;
	dispatch((CairMainMenu*)user_data, CAIR_MAIN_MENU_REDO);
}

static void find_clicked(Rid item, void* user_data) {
	(void)item;
	dispatch((CairMainMenu*)user_data, CAIR_MAIN_MENU_FIND);
}

static void find_replace_clicked(Rid item, void* user_data) {
	(void)item;
	dispatch((CairMainMenu*)user_data, CAIR_MAIN_MENU_FIND_REPLACE);
}

static void toggle_explorer_clicked(Rid item, void* user_data) {
	(void)item;
	dispatch((CairMainMenu*)user_data, CAIR_MAIN_MENU_TOGGLE_EXPLORER);
}

static void about_clicked(Rid item, void* user_data) {
	(void)item;
	dispatch((CairMainMenu*)user_data, CAIR_MAIN_MENU_ABOUT);
}

static Rid add_item(Rid menu, const char* title, RKey key, Ruint modifiers,
	RMenuItemClickedCallback callback, CairMainMenu* state) {
	Rid item = Rmenu_add_item(menu, title);
	if (item == R_INVALID_ID) return R_INVALID_ID;
	if (key != R_KEY_UNKNOWN) Rmenu_item_set_shortcut(item, key, modifiers);
	Rmenu_item_set_clicked_callback(item, callback, state);
	return item;
}

Rid cair_main_menu_create(CairMainMenu* menu, CairMainMenuActionCallback on_action, void* user_data) {
	Rid file;
	Rid edit;
	Rid view;
	Rid help;
	if (!menu) return R_INVALID_ID;
	memset(menu, 0, sizeof(*menu));
	menu->root = R_INVALID_ID;
	menu->on_action = on_action;
	menu->user_data = user_data;
	menu->root = Rwidget_create(R_WIDGET_MENU_BAR);
	if (menu->root == R_INVALID_ID) return R_INVALID_ID;
	Rmenu_bar_set_native(menu->root, R_FALSE);

	file = Rmenu_add_menu(menu->root, "File");
	edit = Rmenu_add_menu(menu->root, "Edit");
	view = Rmenu_add_menu(menu->root, "View");
	help = Rmenu_add_menu(menu->root, "Help");
	if (file == R_INVALID_ID || edit == R_INVALID_ID || view == R_INVALID_ID || help == R_INVALID_ID) return R_INVALID_ID;
	if (add_item(file, "Open File...", R_KEY_O, R_MOD_CTRL, open_file_clicked, menu) == R_INVALID_ID
		|| Rmenu_add_separator(file) == R_INVALID_ID
		|| add_item(file, "Quit", R_KEY_Q, R_MOD_CTRL, quit_clicked, menu) == R_INVALID_ID
		|| add_item(edit, "Undo", R_KEY_Z, R_MOD_CTRL, undo_clicked, menu) == R_INVALID_ID
		|| add_item(edit, "Redo", R_KEY_Y, R_MOD_CTRL, redo_clicked, menu) == R_INVALID_ID
		|| Rmenu_add_separator(edit) == R_INVALID_ID
		|| add_item(edit, "Find...", R_KEY_F, R_MOD_CTRL, find_clicked, menu) == R_INVALID_ID
		|| add_item(edit, "Find and Replace...", R_KEY_H, R_MOD_CTRL, find_replace_clicked, menu) == R_INVALID_ID
		|| add_item(view, "Toggle Project Explorer", R_KEY_UNKNOWN, 0, toggle_explorer_clicked, menu) == R_INVALID_ID
		|| add_item(help, "About C-AIR", R_KEY_UNKNOWN, 0, about_clicked, menu) == R_INVALID_ID) return R_INVALID_ID;
	return menu->root;
}
