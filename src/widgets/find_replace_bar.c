#include "widgets/find_replace_bar.h"

#include "search/find_replace.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int case_sensitive(const CairFindReplaceBar* bar) {
	return Rcheckbox_is_checked(bar->case_checkbox);
}

static int whole_word(const CairFindReplaceBar* bar) {
	return Rcheckbox_is_checked(bar->word_checkbox);
}

static const char* query_text(const CairFindReplaceBar* bar) {
	const char* query = Rtext_input_get_text(bar->find_input);
	return query ? query : "";
}

static const char* replacement_text(const CairFindReplaceBar* bar) {
	const char* replacement = Rtext_input_get_text(bar->replace_input);
	return replacement ? replacement : "";
}

static size_t position_to_offset(const char* text, size_t length, int target_line, int target_column) {
	size_t offset = 0;
	int line = 0;
	int column = 0;
	if (target_line < 0) target_line = 0;
	if (target_column < 0) target_column = 0;
	while (offset < length && line < target_line) {
		if (text[offset++] == '\n') ++line;
	}
	while (offset < length && column < target_column && text[offset] != '\n') {
		++offset;
		++column;
	}
	return offset;
}

static void offset_to_position(const char* text, size_t offset, int* line_out, int* column_out) {
	size_t i;
	int line = 0;
	int column = 0;
	for (i = 0; i < offset && text[i]; ++i) {
		if (text[i] == '\n') {
			++line;
			column = 0;
		} else {
			++column;
		}
	}
	*line_out = line;
	*column_out = column;
}

static size_t editor_cursor_offset(Rid editor) {
	const char* text = Rtext_editor_get_text(editor);
	int line;
	int column;
	Rtext_editor_get_cursor(editor, &line, &column);
	return position_to_offset(text, strlen(text), line, column);
}

static void select_editor_range(CairFindReplaceBar* bar, size_t start, size_t end) {
	const char* text = Rtext_editor_get_text(bar->editor);
	int from_line;
	int from_column;
	int to_line;
	int to_column;
	offset_to_position(text, start, &from_line, &from_column);
	offset_to_position(text, end, &to_line, &to_column);
	Rtext_editor_select(bar->editor, from_line, from_column, to_line, to_column);
}

static size_t search_from_cursor(CairFindReplaceBar* bar, int backwards) {
	int from_line;
	int from_column;
	int to_line;
	int to_column;
	if (Rtext_editor_get_selection(bar->editor, &from_line, &from_column, &to_line, &to_column)) {
		const char* text = Rtext_editor_get_text(bar->editor);
		size_t offset = position_to_offset(text, strlen(text), backwards ? from_line : to_line,
			backwards ? from_column : to_column);
		if (backwards) {
			size_t query_length = strlen(query_text(bar));
			return offset >= query_length ? offset - query_length + 1 : 0;
		}
		return offset;
	}
	return editor_cursor_offset(bar->editor);
}

static void update_count(CairFindReplaceBar* bar) {
	const char* text;
	const char* query;
	char label[64];
	if (!bar || bar->editor == R_INVALID_ID) return;
	text = Rtext_editor_get_text(bar->editor);
	query = query_text(bar);
	bar->match_count = cair_find_count(text, (size_t)Rtext_editor_get_length(bar->editor),
		query, strlen(query), case_sensitive(bar), whole_word(bar));
	if (bar->match_count == 1) strcpy(label, "1 match");
	else snprintf(label, sizeof(label), "%zu matches", bar->match_count);
	Rlabel_set_text(bar->count_label, label);
}

static int find_at(CairFindReplaceBar* bar, size_t start, int backwards, int wrap) {
	const char* text = Rtext_editor_get_text(bar->editor);
	const char* query = query_text(bar);
	size_t text_length = (size_t)Rtext_editor_get_length(bar->editor);
	size_t query_length = strlen(query);
	size_t found;
	int result = backwards
		? cair_find_previous(text, text_length, query, query_length, start,
			case_sensitive(bar), whole_word(bar), wrap, &found)
		: cair_find_next(text, text_length, query, query_length, start,
			case_sensitive(bar), whole_word(bar), wrap, &found);
	if (!result) return 0;
	select_editor_range(bar, found, found + query_length);
	return 1;
}

static void search_changed(Rid input, const char* text, void* user_data) {
	CairFindReplaceBar* bar = (CairFindReplaceBar*)user_data;
	(void)input;
	(void)text;
	update_count(bar);
	find_at(bar, 0, 0, 0);
}

static void editor_changed(Rid editor, void* user_data) {
	(void)editor;
	update_count((CairFindReplaceBar*)user_data);
}

static void find_committed(Rid input, const char* text, void* user_data) {
	(void)input;
	(void)text;
	cair_find_replace_bar_find_next((CairFindReplaceBar*)user_data);
}

static void replace_committed(Rid input, const char* text, void* user_data) {
	(void)input;
	(void)text;
	cair_find_replace_bar_replace_one((CairFindReplaceBar*)user_data);
}

static void previous_clicked(Rid button, void* user_data) {
	(void)button;
	cair_find_replace_bar_find_previous((CairFindReplaceBar*)user_data);
}

static void next_clicked(Rid button, void* user_data) {
	(void)button;
	cair_find_replace_bar_find_next((CairFindReplaceBar*)user_data);
}

static void replace_clicked(Rid button, void* user_data) {
	(void)button;
	cair_find_replace_bar_replace_one((CairFindReplaceBar*)user_data);
}

static void replace_all_clicked(Rid button, void* user_data) {
	(void)button;
	cair_find_replace_bar_replace_all((CairFindReplaceBar*)user_data);
}

static void close_clicked(Rid button, void* user_data) {
	(void)button;
	cair_find_replace_bar_close((CairFindReplaceBar*)user_data);
}

static void options_changed(Rid checkbox, int checked, void* user_data) {
	(void)checkbox;
	(void)checked;
	search_changed(R_INVALID_ID, NULL, user_data);
}

static int on_key_event(Rid widget, const REvent* event, void* user_data) {
	CairFindReplaceBar* bar = (CairFindReplaceBar*)user_data;
	(void)widget;
	if (event->type == R_EVENT_KEY_DOWN && event->key == R_KEY_ESCAPE) {
		cair_find_replace_bar_close(bar);
		return R_TRUE;
	}
	if (event->type == R_EVENT_KEY_DOWN && event->key == R_KEY_ENTER
		&& (event->modifiers & R_MOD_SHIFT) && event->target == bar->find_input) {
		cair_find_replace_bar_find_previous(bar);
		return R_TRUE;
	}
	return R_FALSE;
}

static void open_find_action(Rid sender, const char* action, void* user_data) {
	(void)sender;
	cair_find_replace_bar_show((CairFindReplaceBar*)user_data, strcmp(action, "cair.find_replace") == 0);
}

Rid cair_find_replace_bar_create(CairFindReplaceBar* bar, Rid editor) {
	Rid find_row;
	Rid replace_row;
	Rid button;
	if (!bar) return R_INVALID_ID;
	memset(bar, 0, sizeof(*bar));
	bar->editor = editor;
	bar->root = Rwidget_create(R_WIDGET_VBOX);
	if (bar->root == R_INVALID_ID) return R_INVALID_ID;
	Rpanel_set_spacing(bar->root, 4.0f);
	find_row = Rwidget_create(R_WIDGET_HBOX);
	replace_row = Rwidget_create(R_WIDGET_HBOX);
	if (find_row == R_INVALID_ID || replace_row == R_INVALID_ID) return R_INVALID_ID;
	bar->find_row = find_row;
	bar->replace_row = replace_row;
	Rpanel_set_spacing(find_row, 4.0f);
	Rpanel_set_spacing(replace_row, 4.0f);
	bar->find_input = Rwidget_add_child(find_row, Rwidget_create(R_WIDGET_TEXT_INPUT));
	if (bar->find_input == R_INVALID_ID) return R_INVALID_ID;
	Rtext_input_set_placeholder(bar->find_input, "Find");
	Rwidget_set_min_size(bar->find_input, 190.0f, 0.0f);
	Rslot_set_fill(bar->find_input, 1.0f);
	bar->count_label = Rwidget_add_child(find_row, Rlabel_create("0 matches"));
	bar->case_checkbox = Rwidget_add_child(find_row, Rcheckbox_create("Aa"));
	bar->word_checkbox = Rwidget_add_child(find_row, Rcheckbox_create("ab"));
	if (bar->case_checkbox != R_INVALID_ID) Rwidget_set_tooltip(bar->case_checkbox, "Match case");
	if (bar->word_checkbox != R_INVALID_ID) Rwidget_set_tooltip(bar->word_checkbox, "Match whole words");
	button = Rwidget_add_child(find_row, Rbutton_create("<"));
	if (bar->count_label == R_INVALID_ID || bar->case_checkbox == R_INVALID_ID
		|| bar->word_checkbox == R_INVALID_ID || button == R_INVALID_ID) return R_INVALID_ID;
	Rwidget_set_tooltip(button, "Find previous (Shift+Enter)");
	Rbutton_set_clicked_callback(button, previous_clicked, bar);
	button = Rwidget_add_child(find_row, Rbutton_create(">"));
	if (button == R_INVALID_ID) return R_INVALID_ID;
	Rwidget_set_tooltip(button, "Find next (Enter)");
	Rbutton_set_clicked_callback(button, next_clicked, bar);
	button = Rwidget_add_child(find_row, Rbutton_create("x"));
	if (button == R_INVALID_ID) return R_INVALID_ID;
	Rwidget_set_tooltip(button, "Close (Escape)");
	Rbutton_set_clicked_callback(button, close_clicked, bar);

	bar->replace_input = Rwidget_add_child(replace_row, Rwidget_create(R_WIDGET_TEXT_INPUT));
	if (bar->replace_input == R_INVALID_ID) return R_INVALID_ID;
	Rtext_input_set_placeholder(bar->replace_input, "Replace");
	Rwidget_set_min_size(bar->replace_input, 190.0f, 0.0f);
	Rslot_set_fill(bar->replace_input, 1.0f);
	button = Rwidget_add_child(replace_row, Rbutton_create("Replace"));
	if (button == R_INVALID_ID) return R_INVALID_ID;
	Rbutton_set_clicked_callback(button, replace_clicked, bar);
	button = Rwidget_add_child(replace_row, Rbutton_create("Replace All"));
	if (button == R_INVALID_ID) return R_INVALID_ID;
	Rbutton_set_clicked_callback(button, replace_all_clicked, bar);
	Rwidget_set_visibility(replace_row, R_COLLAPSED);
	Rwidget_add_child(bar->root, find_row);
	Rwidget_add_child(bar->root, replace_row);
	Rwidget_set_visibility(bar->root, R_COLLAPSED);
	Rwidget_set_event_callback(bar->root, on_key_event, bar);
	Rtext_input_set_changed_callback(bar->find_input, search_changed, bar);
	Rtext_input_set_committed_callback(bar->find_input, find_committed, bar);
	Rtext_input_set_committed_callback(bar->replace_input, replace_committed, bar);
	Rcheckbox_set_changed_callback(bar->case_checkbox, options_changed, bar);
	Rcheckbox_set_changed_callback(bar->word_checkbox, options_changed, bar);
	if (editor != R_INVALID_ID) Rtext_editor_set_changed_callback(editor, editor_changed, bar);
	return bar->root;
}

void cair_find_replace_bar_set_editor(CairFindReplaceBar* bar, Rid editor) {
	if (!bar || bar->editor == editor) return;
	if (bar->editor != R_INVALID_ID) Rtext_editor_set_changed_callback(bar->editor, NULL, NULL);
	bar->editor = editor;
	if (editor == R_INVALID_ID) {
		bar->match_count = 0;
		Rlabel_set_text(bar->count_label, "0 matches");
		Rwidget_set_visibility(bar->root, R_COLLAPSED);
		return;
	}
	Rtext_editor_set_changed_callback(editor, editor_changed, bar);
	update_count(bar);
}

void cair_find_replace_bar_show(CairFindReplaceBar* bar, int show_replace_row) {
	if (!bar || bar->root == R_INVALID_ID || bar->editor == R_INVALID_ID) return;
	Rwidget_set_visibility(bar->root, R_VISIBLE);
	Rwidget_set_visibility(bar->replace_row, show_replace_row ? R_VISIBLE : R_COLLAPSED);
	Rwidget_focus(bar->find_input);
}

void cair_find_replace_bar_close(CairFindReplaceBar* bar) {
	if (!bar || bar->root == R_INVALID_ID) return;
	Rwidget_set_visibility(bar->root, R_COLLAPSED);
	if (bar->editor != R_INVALID_ID) Rwidget_focus(bar->editor);
}

void cair_find_replace_bar_register_shortcuts(CairFindReplaceBar* bar, Rid window) {
	if (!bar || window == R_INVALID_ID) return;
	Raction_bind("cair.find", open_find_action, bar);
	Raction_bind("cair.find_replace", open_find_action, bar);
	Rwindow_register_shortcut(window, R_KEY_F, R_MOD_CTRL, "cair.find");
	Rwindow_register_shortcut(window, R_KEY_H, R_MOD_CTRL, "cair.find_replace");
}

void cair_find_replace_bar_set_query(CairFindReplaceBar* bar, const char* query) {
	if (!bar || !query) return;
	Rtext_input_set_text(bar->find_input, query);
	update_count(bar);
}

void cair_find_replace_bar_set_replacement(CairFindReplaceBar* bar, const char* replacement) {
	if (bar && replacement) Rtext_input_set_text(bar->replace_input, replacement);
}

void cair_find_replace_bar_refresh(CairFindReplaceBar* bar) {
	update_count(bar);
}

size_t cair_find_replace_bar_get_match_count(const CairFindReplaceBar* bar) {
	return bar ? bar->match_count : 0;
}

int cair_find_replace_bar_find_next(CairFindReplaceBar* bar) {
	if (!bar) return 0;
	return find_at(bar, search_from_cursor(bar, 0), 0, 1);
}

int cair_find_replace_bar_find_previous(CairFindReplaceBar* bar) {
	if (!bar) return 0;
	return find_at(bar, search_from_cursor(bar, 1), 1, 1);
}

static int selected_text_is_query(CairFindReplaceBar* bar) {
	const char* selected;
	const char* text;
	const char* query = query_text(bar);
	int from_line, from_column, to_line, to_column;
	size_t start;
	size_t length;
	size_t found;
	size_t i;
	if (!Rtext_editor_get_selection(bar->editor, &from_line, &from_column, &to_line, &to_column)) return 0;
	selected = Rtext_editor_get_selected_text(bar->editor);
	length = strlen(query);
	if (strlen(selected) != length) return 0;
	for (i = 0; query[i]; ++i) {
		unsigned char left = (unsigned char)selected[i];
		unsigned char right = (unsigned char)query[i];
		if (!case_sensitive(bar)) {
			if (left >= 'A' && left <= 'Z') left = (unsigned char)(left + ('a' - 'A'));
			if (right >= 'A' && right <= 'Z') right = (unsigned char)(right + ('a' - 'A'));
		}
		if (left != right) return 0;
	}
	if (whole_word(bar)) {
		text = Rtext_editor_get_text(bar->editor);
		start = position_to_offset(text, (size_t)Rtext_editor_get_length(bar->editor), from_line, from_column);
		if (!cair_find_next(text, (size_t)Rtext_editor_get_length(bar->editor), query, length, start,
			case_sensitive(bar), 1, 0, &found) || found != start) return 0;
	}
	return query[0] != '\0';
}

int cair_find_replace_bar_replace_one(CairFindReplaceBar* bar) {
	const char* replacement;
	if (!bar || !query_text(bar)[0]) return 0;
	if (!selected_text_is_query(bar) && !find_at(bar, search_from_cursor(bar, 0), 0, 1)) return 0;
	replacement = replacement_text(bar);
	Rtext_editor_insert_text(bar->editor, replacement);
	update_count(bar);
	return 1;
}

size_t cair_find_replace_bar_replace_all(CairFindReplaceBar* bar) {
	const char* text;
	const char* query;
	const char* replacement;
	size_t length;
	size_t result_length;
	size_t count;
	char* result;
	char label[64];
	if (!bar) return 0;
	text = Rtext_editor_get_text(bar->editor);
	query = query_text(bar);
	replacement = replacement_text(bar);
	length = (size_t)Rtext_editor_get_length(bar->editor);
	result = cair_replace_all(text, length, query, strlen(query), replacement, strlen(replacement),
		case_sensitive(bar), whole_word(bar), &result_length, &count);
	if (!result) return 0;
	if (count) {
		Rtext_editor_select_all(bar->editor);
		Rtext_editor_insert_text(bar->editor, result);
	}
	free(result);
	update_count(bar);
	snprintf(label, sizeof(label), "%zu replaced", count);
	Rlabel_set_text(bar->count_label, label);
	(void)result_length;
	return count;
}
