#ifndef CAIR_FIND_REPLACE_H
#define CAIR_FIND_REPLACE_H

#include <stddef.h>

/* Searches are byte based, as Raven's C editor columns are UTF-8 byte offsets. */
int cair_find_next(const char* text, size_t text_length, const char* query, size_t query_length,
	size_t start, int case_sensitive, int whole_word, int wrap, size_t* match_offset);
int cair_find_previous(const char* text, size_t text_length, const char* query, size_t query_length,
	size_t before, int case_sensitive, int whole_word, int wrap, size_t* match_offset);
size_t cair_find_count(const char* text, size_t text_length, const char* query, size_t query_length,
	int case_sensitive, int whole_word);

/* Returns an allocated result; release it with free(). NULL means invalid input or allocation failure. */
char* cair_replace_all(const char* text, size_t text_length, const char* query, size_t query_length,
	const char* replacement, size_t replacement_length, int case_sensitive, int whole_word,
	size_t* result_length, size_t* replacement_count);

#endif
