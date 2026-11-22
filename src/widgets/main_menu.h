#ifndef CAIR_MAIN_MENU_H
#define CAIR_MAIN_MENU_H

#include <Raven.h>

typedef enum CairMainMenuAction {
	CAIR_MAIN_MENU_OPEN_FILE,
	CAIR_MAIN_MENU_QUIT,
	CAIR_MAIN_MENU_UNDO,
	CAIR_MAIN_MENU_REDO,
	CAIR_MAIN_MENU_FIND,
	CAIR_MAIN_MENU_FIND_REPLACE,
	CAIR_MAIN_MENU_TOGGLE_EXPLORER,
	CAIR_MAIN_MENU_ABOUT
} CairMainMenuAction;

typedef void (*CairMainMenuActionCallback)(CairMainMenuAction action, void* user_data);

typedef struct CairMainMenu {
	Rid root;
	CairMainMenuActionCallback on_action;
	void* user_data;
} CairMainMenu;

Rid cair_main_menu_create(CairMainMenu* menu, CairMainMenuActionCallback on_action, void* user_data);

#endif
