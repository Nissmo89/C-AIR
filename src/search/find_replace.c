#include "search/find_replace.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static unsigned char fold_byte(unsigned char byte) {
	return byte >= 'A' && byte <= 'Z' ? (unsigned char)(byte + ('a' - 'A')) : byte;
}

static int is_word_byte(unsigned char byte) {
	return (byte >= 'a' && byte <= 'z') || (byte >= 'A' && byte <= 'Z')
		|| (byte >= '0' && byte <= '9') || byte == '_' || byte >= 0x80;
}

static int equal_byte(unsigned char left, unsigned char right, int case_sensitive) {
	return case_sensitive ? left == right : fold_byte(left) == fold_byte(right);
}

static void make_skip_table(const char* query, size_t query_length, int case_sensitive, size_t skip[256]) {
	size_t i;
	for (i = 0; i < 256; ++i) skip[i] = query_length;
	for (i = 0; i + 1 < query_length; ++i) {
		unsigned char byte = (unsigned char)query[i];
		if (!case_sensitive) byte = fold_byte(byte);
		skip[byte] = query_length - i - 1;
	}
}

static int word_boundaries_match(const char* text, size_t text_length, size_t offset,
	size_t query_length, int whole_word) {
	if (!whole_word) return 1;
	if (offset > 0 && is_word_byte((unsigned char)text[offset - 1])) return 0;
	if (offset + query_length < text_length && is_word_byte((unsigned char)text[offset + query_length])) return 0;
	return 1;
}

static int search_range(const char* text, size_t text_length, const char* query, size_t query_length,
	size_t lower, size_t upper, int case_sensitive, int whole_word, size_t* match_offset) {
	size_t skip[256];
	size_t last_start;
	size_t offset;
	if (query_length == 0 || query_length > text_length || lower > upper) return 0;
	last_start = text_length - query_length;
	if (upper > last_start + 1) upper = last_start + 1;
	if (lower >= upper) return 0;
	make_skip_table(query, query_length, case_sensitive, skip);
	offset = lower;
	while (offset < upper) {
		size_t i = query_length;
		while (i > 0 && equal_byte((unsigned char)text[offset + i - 1],
			(unsigned char)query[i - 1], case_sensitive)) --i;
		if (i == 0) {
			if (word_boundaries_match(text, text_length, offset, query_length, whole_word)) {
				if (match_offset) *match_offset = offset;
				return 1;
			}
			++offset;
		} else {
			unsigned char tail = (unsigned char)text[offset + query_length - 1];
			size_t step = skip[case_sensitive ? tail : fold_byte(tail)];
			offset += step ? step : 1;
		}
	}
	return 0;
}

int cair_find_next(const char* text, size_t text_length, const char* query, size_t query_length,
	size_t start, int case_sensitive, int whole_word, int wrap, size_t* match_offset) {
	size_t last_start;
	if (!text || !query || query_length == 0 || query_length > text_length) return 0;
	last_start = text_length - query_length;
	if (start > last_start + 1) start = last_start + 1;
	if (search_range(text, text_length, query, query_length, start, last_start + 1,
		case_sensitive, whole_word, match_offset)) return 1;
	return wrap && start > 0
		? search_range(text, text_length, query, query_length, 0, start, case_sensitive, whole_word, match_offset)
		: 0;
}

int cair_find_previous(const char* text, size_t text_length, const char* query, size_t query_length,
	size_t before, int case_sensitive, int whole_word, int wrap, size_t* match_offset) {
	size_t skip[256];
	size_t last_start;
	size_t upper;
	size_t candidate;
	size_t i;
	if (!text || !query || query_length == 0 || query_length > text_length) return 0;
	last_start = text_length - query_length;
	if (before > last_start + 1) before = last_start + 1;
	upper = before;
	for (i = 0; i < 256; ++i) skip[i] = query_length;
	for (i = 1; i < query_length; ++i) {
		unsigned char byte = (unsigned char)query[i];
		if (!case_sensitive) byte = fold_byte(byte);
		if (skip[byte] == query_length) skip[byte] = i;
	}
	while (upper > 0) {
		size_t j = 0;
		candidate = upper - 1;
		if (candidate > last_start) {
			upper = last_start + 1;
			continue;
		}
		while (j < query_length && equal_byte((unsigned char)text[candidate + j],
			(unsigned char)query[j], case_sensitive)) ++j;
		if (j == query_length && word_boundaries_match(text, text_length, candidate, query_length, whole_word)) {
			if (match_offset) *match_offset = candidate;
			return 1;
		}
		{
			size_t step = j == query_length ? 1 : skip[case_sensitive
				? (unsigned char)text[candidate] : fold_byte((unsigned char)text[candidate])];
			if (step == 0) step = 1;
			if (step > candidate) break;
			upper = candidate - step + 1;
		}
	}
	if (!wrap || before > last_start) return 0;
	upper = last_start + 1;
	while (upper > before) {
		size_t j = 0;
		candidate = upper - 1;
		while (j < query_length && equal_byte((unsigned char)text[candidate + j],
			(unsigned char)query[j], case_sensitive)) ++j;
		if (j == query_length && word_boundaries_match(text, text_length, candidate, query_length, whole_word)) {
			if (match_offset) *match_offset = candidate;
			return 1;
		}
		{
			size_t step = j == query_length ? 1 : skip[case_sensitive
				? (unsigned char)text[candidate] : fold_byte((unsigned char)text[candidate])];
			if (step == 0) step = 1;
			if (step > candidate - before) break;
			upper = candidate - step + 1;
		}
	}
	return 0;
}

size_t cair_find_count(const char* text, size_t text_length, const char* query, size_t query_length,
	int case_sensitive, int whole_word) {
	size_t count = 0;
	size_t cursor = 0;
	size_t found;
	if (!text || !query || query_length == 0) return 0;
	while (cair_find_next(text, text_length, query, query_length, cursor,
		case_sensitive, whole_word, 0, &found)) {
		++count;
		cursor = found + query_length;
	}
	return count;
}

char* cair_replace_all(const char* text, size_t text_length, const char* query, size_t query_length,
	const char* replacement, size_t replacement_length, int case_sensitive, int whole_word,
	size_t* result_length, size_t* replacement_count) {
	size_t count;
	size_t output_length;
	size_t cursor = 0;
	size_t output_cursor = 0;
	size_t found;
	char* output;
	if (result_length) *result_length = 0;
	if (replacement_count) *replacement_count = 0;
	if (!text || !query || !replacement || query_length == 0) return NULL;
	count = cair_find_count(text, text_length, query, query_length, case_sensitive, whole_word);
	if (replacement_length >= query_length) {
		size_t increase = replacement_length - query_length;
		if (increase && count > (SIZE_MAX - text_length) / increase) return NULL;
		output_length = text_length + count * increase;
	} else {
		output_length = text_length - count * (query_length - replacement_length);
	}
	if (output_length == SIZE_MAX) return NULL;
	output = (char*)malloc(output_length + 1);
	if (!output) return NULL;
	while (cair_find_next(text, text_length, query, query_length, cursor,
		case_sensitive, whole_word, 0, &found)) {
		size_t unchanged = found - cursor;
		memcpy(output + output_cursor, text + cursor, unchanged);
		output_cursor += unchanged;
		memcpy(output + output_cursor, replacement, replacement_length);
		output_cursor += replacement_length;
		cursor = found + query_length;
	}
	memcpy(output + output_cursor, text + cursor, text_length - cursor);
	output[output_length] = '\0';
	if (result_length) *result_length = output_length;
	if (replacement_count) *replacement_count = count;
	return output;
}
