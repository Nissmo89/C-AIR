#ifndef CAIR_FIND_REPLACE_BAR_H
#define CAIR_FIND_REPLACE_BAR_H

#include <Raven.h>

typedef struct CairFindReplaceBar {
	Rid root;
	Rid find_row;
	Rid replace_row;
	Rid find_input;
	Rid replace_input;
	Rid count_label;
	Rid case_checkbox;
	Rid word_checkbox;
	Rid editor;
	size_t match_count;
} CairFindReplaceBar;

Rid cair_find_replace_bar_create(CairFindReplaceBar* bar, Rid editor);
void cair_find_replace_bar_show(CairFindReplaceBar* bar, int show_replace_row);
void cair_find_replace_bar_close(CairFindReplaceBar* bar);
void cair_find_replace_bar_register_shortcuts(CairFindReplaceBar* bar, Rid window);
void cair_find_replace_bar_set_query(CairFindReplaceBar* bar, const char* query);
void cair_find_replace_bar_set_replacement(CairFindReplaceBar* bar, const char* replacement);
void cair_find_replace_bar_refresh(CairFindReplaceBar* bar);
size_t cair_find_replace_bar_get_match_count(const CairFindReplaceBar* bar);
int cair_find_replace_bar_find_next(CairFindReplaceBar* bar);
int cair_find_replace_bar_find_previous(CairFindReplaceBar* bar);
int cair_find_replace_bar_replace_one(CairFindReplaceBar* bar);
size_t cair_find_replace_bar_replace_all(CairFindReplaceBar* bar);

#endif
