#include "model/terminal_model.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

static void test_invalid_arguments(void) {
	CairTerminalModel model;
	memset(&model, 0, sizeof(model));
	assert(!cair_terminal_model_init(NULL, 80, 24));
	assert(!cair_terminal_model_init(&model, 0, 24));
	assert(!cair_terminal_model_init(&model, 80, 0));
	assert(!cair_terminal_model_write(NULL, "test", 4));
	assert(!cair_terminal_model_resize(NULL, 80, 24));
	assert(!cair_terminal_model_get_dimensions(NULL, NULL, NULL));
	assert(!cair_terminal_model_get_cursor(NULL, NULL));
	assert(cair_terminal_model_read_row_text(NULL, 0, NULL, 0) == -1);
	/* Double destroy on uninitialized should be safe */
	cair_terminal_model_destroy(&model);
	cair_terminal_model_destroy(NULL);
}

static void test_init_and_dimensions(void) {
	CairTerminalModel model;
	uint16_t cols = 0, rows = 0;
	CairTerminalCursor cursor;
	CairTerminalColor fg, bg;

	assert(cair_terminal_model_init(&model, 80, 24));
	assert(cair_terminal_model_get_dimensions(&model, &cols, &rows));
	assert(cols == 80);
	assert(rows == 24);

	assert(cair_terminal_model_get_cursor(&model, &cursor));
	assert(cursor.col == 0);
	assert(cursor.row == 0);
	assert(cursor.visible);

	assert(cair_terminal_model_get_default_colors(&model, &fg, &bg));

	cair_terminal_model_destroy(&model);
}

static void test_write_and_read_text(void) {
	CairTerminalModel model;
	char buffer[128];
	int len;
	CairTerminalCursor cursor;

	assert(cair_terminal_model_init(&model, 80, 24));

	const char *msg1 = "Hello, C-AIR Terminal!\r\nLine Two";
	assert(cair_terminal_model_write(&model, msg1, strlen(msg1)));

	len = cair_terminal_model_read_row_text(&model, 0, buffer, sizeof(buffer));
	assert(len > 0);
	assert(strcmp(buffer, "Hello, C-AIR Terminal!") == 0);

	len = cair_terminal_model_read_row_text(&model, 1, buffer, sizeof(buffer));
	assert(len > 0);
	assert(strcmp(buffer, "Line Two") == 0);

	/* Line 2 should be empty */
	len = cair_terminal_model_read_row_text(&model, 2, buffer, sizeof(buffer));
	assert(len == 0);
	assert(strcmp(buffer, "") == 0);

	/* Check cursor after typing */
	assert(cair_terminal_model_get_cursor(&model, &cursor));
	assert(cursor.row == 1);
	assert(cursor.col == 8);

	cair_terminal_model_destroy(&model);
}

static void test_ansi_colors_and_styles(void) {
	CairTerminalModel model;
	CairTerminalCell cell;

	assert(cair_terminal_model_init(&model, 80, 24));

	/* Bold red 'R', then reset */
	const char *styled = "\x1b[1;31mR\x1b[0m";
	assert(cair_terminal_model_write(&model, styled, strlen(styled)));

	assert(cair_terminal_model_get_cell(&model, 0, 0, &cell));
	assert(strcmp(cell.text, "R") == 0);
	assert(cell.bold == 1);
	assert(cell.has_fg == 1);

	cair_terminal_model_destroy(&model);
}

static void test_resize_and_scroll(void) {
	CairTerminalModel model;
	uint16_t cols = 0, rows = 0;
	char buffer[128];

	assert(cair_terminal_model_init(&model, 80, 24));

	const char *lines = "Line A\r\nLine B\r\nLine C";
	assert(cair_terminal_model_write(&model, lines, strlen(lines)));

	assert(cair_terminal_model_resize(&model, 100, 30));
	assert(cair_terminal_model_get_dimensions(&model, &cols, &rows));
	assert(cols == 100);
	assert(rows == 30);

	assert(cair_terminal_model_read_row_text(&model, 0, buffer, sizeof(buffer)) >= 0);
	assert(strcmp(buffer, "Line A") == 0);

	/* Test viewport scrolling */
	assert(cair_terminal_model_scroll_viewport(&model, -1));
	assert(cair_terminal_model_scroll_viewport(&model, 1));

	cair_terminal_model_destroy(&model);
}

int main(void) {
	printf("1. test_invalid_arguments\n"); fflush(stdout);
	test_invalid_arguments();
	printf("2. test_init_and_dimensions\n"); fflush(stdout);
	test_init_and_dimensions();
	printf("3. test_write_and_read_text\n"); fflush(stdout);
	test_write_and_read_text();
	printf("4. test_ansi_colors_and_styles\n"); fflush(stdout);
	test_ansi_colors_and_styles();
	printf("5. test_resize_and_scroll\n"); fflush(stdout);
	test_resize_and_scroll();
	printf("All terminal model unit tests passed!\n");
	return 0;
}
