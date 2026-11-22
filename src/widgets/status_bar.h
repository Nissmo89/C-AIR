#ifndef CAIR_STATUS_BAR_H
#define CAIR_STATUS_BAR_H

#include <Raven.h>

typedef struct CairStatusBar {
	Rid root;
	Rid label;
} CairStatusBar;

Rid cair_status_bar_create(CairStatusBar* bar);
void cair_status_bar_set_text(CairStatusBar* bar, const char* text);

#endif
