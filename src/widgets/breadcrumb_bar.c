#include "widgets/breadcrumb_bar.h"

#include <string.h>

Rid cair_breadcrumb_bar_create(void) {
	Rid breadcrumb = Rwidget_create(R_WIDGET_BREADCRUMB);
	if (breadcrumb == R_INVALID_ID) return R_INVALID_ID;
	Rwidget_set_metric(breadcrumb, R_METRIC_CONTROL_HEIGHT, 30.0f);
	Rwidget_set_color(breadcrumb, R_COLOR_TAB_ACTIVE, 1.0f, 1.0f, 1.0f, 1.0f);
	Rwidget_set_color(breadcrumb, R_COLOR_SEPARATOR, 0.820f, 0.820f, 0.820f, 1.0f);
	Rwidget_set_color(breadcrumb, R_COLOR_TEXT, 0.133f, 0.133f, 0.133f, 1.0f);
	Rwidget_set_color(breadcrumb, R_COLOR_ROW_HOVERED, 0.937f, 0.937f, 0.937f, 1.0f);
	cair_breadcrumb_bar_set_path(breadcrumb, NULL);
	return breadcrumb;
}

void cair_breadcrumb_bar_set_path(Rid breadcrumb, const char* path) {
	const char* part;
	const char* cursor;
	char item[512];
	size_t length;
	if (breadcrumb == R_INVALID_ID) return;
	Rbreadcrumb_set_items(breadcrumb, "");
	if (!path || !path[0]) {
		Rbreadcrumb_add_item(breadcrumb, "welcome.c");
		return;
	}
	part = path;
	for (cursor = path;; ++cursor) {
		if (*cursor == '\\' || *cursor == '/' || *cursor == '\0') {
			length = (size_t)(cursor - part);
			if (length > 0) {
				if (length >= sizeof(item)) length = sizeof(item) - 1;
				memcpy(item, part, length);
				item[length] = '\0';
				Rbreadcrumb_add_item(breadcrumb, item);
			}
			if (!*cursor) break;
			part = cursor + 1;
		}
	}
}
