#ifndef CAIR_BREADCRUMB_BAR_H
#define CAIR_BREADCRUMB_BAR_H

#include <Raven.h>

Rid cair_breadcrumb_bar_create(void);
void cair_breadcrumb_bar_set_path(Rid breadcrumb, const char* path);

#endif
