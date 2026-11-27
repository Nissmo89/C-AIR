#ifndef CAIR_BOTTOM_PANEL_H
#define CAIR_BOTTOM_PANEL_H

#include <Raven.h>
#include "widgets/terminal_panel.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum CairBottomPanelTab {
	CAIR_BOTTOM_PANEL_OUTPUT = 0,
	CAIR_BOTTOM_PANEL_TERMINAL = 1
} CairBottomPanelTab;

typedef struct CairBottomPanel {
	Rid root;
	Rid tabs;
	Rid output_view;
	Rid output_label;
	CairTerminalPanel *terminal;
	CairBottomPanelTab active_tab;
} CairBottomPanel;

Rid cair_bottom_panel_create(CairBottomPanel *panel, CairTerminalPanel *terminal);
void cair_bottom_panel_show(CairBottomPanel *panel);
void cair_bottom_panel_hide(CairBottomPanel *panel);
void cair_bottom_panel_toggle(CairBottomPanel *panel);
void cair_bottom_panel_select(CairBottomPanel *panel, CairBottomPanelTab tab);

#ifdef __cplusplus
}
#endif

#endif
