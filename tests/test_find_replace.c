#include "search/find_replace.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int failures;
static unsigned int random_state = 0xC0FFEEu;

#define CHECK(condition, message) do { \
	if (!(condition)) { fprintf(stderr, "FAIL: %s\n", message); ++failures; } \
} while (0)

static unsigned int next_random(void) {
	random_state = random_state * 1664525u + 1013904223u;
	return random_state;
}

static unsigned char folded(unsigned char byte) {
	return byte >= 'A' && byte <= 'Z' ? (unsigned char)(byte + ('a' - 'A')) : byte;
}

static int word_byte(unsigned char byte) {
	return (byte >= 'a' && byte <= 'z') || (byte >= 'A' && byte <= 'Z')
		|| (byte >= '0' && byte <= '9') || byte == '_' || byte >= 0x80;
}

static int oracle_match(const char* text, size_t text_length, const char* query, size_t query_length,
	size_t offset, int sensitive, int whole) {
	size_t i;
	if (offset + query_length > text_length) return 0;
	if (whole && offset && word_byte((unsigned char)text[offset - 1])) return 0;
	if (whole && offset + query_length < text_length && word_byte((unsigned char)text[offset + query_length])) return 0;
	for (i = 0; i < query_length; ++i) {
		unsigned char left = (unsigned char)text[offset + i];
		unsigned char right = (unsigned char)query[i];
		if (sensitive ? left != right : folded(left) != folded(right)) return 0;
	}
	return 1;
}

static size_t oracle_count(const char* text, size_t text_length, const char* query, size_t query_length,
	int sensitive, int whole) {
	size_t count = 0;
	size_t offset = 0;
	if (!query_length || query_length > text_length) return 0;
	while (offset + query_length <= text_length) {
		if (oracle_match(text, text_length, query, query_length, offset, sensitive, whole)) {
			++count;
			offset += query_length;
		} else ++offset;
	}
	return count;
}

static void property_checks(void) {
	static const char alphabet[] = "abcABC_09 ";
	int trial;
	for (trial = 0; trial < 1500; ++trial) {
		char text[160];
		char query[12];
		size_t text_length = next_random() % 120;
		size_t query_length = 1 + next_random() % 9;
		size_t start;
		size_t expected_count;
		size_t actual_offset = 0;
		size_t expected_offset = 0;
		size_t previous_expected = 0;
		size_t previous_before;
		int sensitive = (int)(next_random() & 1u);
		int whole = (int)(next_random() & 1u);
		int wrap = (int)(next_random() & 1u);
		int expected_found = 0;
		int actual_found;
		size_t i;
		for (i = 0; i < text_length; ++i) text[i] = alphabet[next_random() % (sizeof(alphabet) - 1)];
		text[text_length] = '\0';
		for (i = 0; i < query_length; ++i) query[i] = alphabet[next_random() % (sizeof(alphabet) - 1)];
		query[query_length] = '\0';
		expected_count = oracle_count(text, text_length, query, query_length, sensitive, whole);
		CHECK(cair_find_count(text, text_length, query, query_length, sensitive, whole) == expected_count,
			"deterministic property: count agrees with naive matcher");
		start = next_random() % (text_length + 1);
		for (i = start; i + query_length <= text_length; ++i) {
			if (oracle_match(text, text_length, query, query_length, i, sensitive, whole)) {
				expected_offset = i;
				expected_found = 1;
				break;
			}
		}
		if (!expected_found && wrap) {
			for (i = 0; i < start && i + query_length <= text_length; ++i) {
				if (oracle_match(text, text_length, query, query_length, i, sensitive, whole)) {
					expected_offset = i;
					expected_found = 1;
					break;
				}
			}
		}
		actual_found = cair_find_next(text, text_length, query, query_length, start,
			sensitive, whole, wrap, &actual_offset);
		CHECK(actual_found == expected_found && (!actual_found || actual_offset == expected_offset),
			"deterministic property: next match agrees with naive matcher");
		previous_before = next_random() % (text_length + 1);
		expected_found = 0;
		if (query_length <= text_length) {
			i = previous_before < text_length - query_length + 1
				? previous_before : text_length - query_length + 1;
			while (i > 0) {
				--i;
				if (oracle_match(text, text_length, query, query_length, i, sensitive, whole)) {
					previous_expected = i;
					expected_found = 1;
					break;
				}
			}
			if (!expected_found && wrap) {
				i = text_length - query_length + 1;
				while (i > previous_before) {
					--i;
					if (oracle_match(text, text_length, query, query_length, i, sensitive, whole)) {
						previous_expected = i;
						expected_found = 1;
						break;
					}
				}
			}
		}
		actual_found = cair_find_previous(text, text_length, query, query_length, previous_before,
			sensitive, whole, wrap, &actual_offset);
		CHECK(actual_found == expected_found && (!actual_found || actual_offset == previous_expected),
			"deterministic property: previous match agrees with naive matcher");
	}
}

int main(void) {
	const char* text = "one two one";
	const char* mixed = "foo Foo food foo";
	size_t offset = 99;
	size_t output_length = 0;
	size_t replaced = 0;
	char* output;
	char* large_text;
	size_t large_length = 2u * 1024u * 1024u;

	CHECK(cair_find_count(text, strlen(text), "one", 3, 1, 0) == 2, "count repeated exact matches");
	CHECK(cair_find_next(text, strlen(text), "two", 3, 0, 1, 0, 0, &offset) && offset == 4,
		"find the next match from the beginning");
	CHECK(cair_find_next(text, strlen(text), "one", 3, 9, 1, 0, 1, &offset) && offset == 0,
		"next search wraps to the beginning");
	CHECK(cair_find_previous(text, strlen(text), "one", 3, 10, 1, 0, 0, &offset) && offset == 8,
		"find the previous match");
	CHECK(cair_find_previous(text, strlen(text), "one", 3, 0, 1, 0, 1, &offset) && offset == 8,
		"previous search wraps to the end");
	CHECK(cair_find_count(mixed, strlen(mixed), "foo", 3, 0, 0) == 4,
		"case-insensitive matching includes prefixes");
	CHECK(cair_find_count(mixed, strlen(mixed), "foo", 3, 0, 1) == 3,
		"whole-word mode excludes embedded matches");
	CHECK(cair_find_count(text, strlen(text), "", 0, 1, 0) == 0, "empty query has no matches");
	CHECK(!cair_find_next(text, strlen(text), "missing", 7, 0, 1, 0, 0, &offset), "no-match search fails");

	output = cair_replace_all("a A cat", 7, "a", 1, "x", 1, 0, 0, &output_length, &replaced);
	CHECK(output != NULL, "replace-all allocates an output buffer");
	if (output) {
		CHECK(strcmp(output, "x x cxt") == 0, "replace-all preserves unmatched content");
		CHECK(output_length == strlen("x x cxt") && replaced == 3, "replace-all reports size and count");
		free(output);
	}

	output = cair_replace_all("cat scatter cat", 15, "cat", 3, "dog", 3, 1, 1, &output_length, &replaced);
	CHECK(output != NULL, "whole-word replacement succeeds");
	if (output) {
		CHECK(strcmp(output, "dog scatter dog") == 0 && replaced == 2,
			"whole-word replacement leaves embedded text alone");
		free(output);
	}
	output = cair_replace_all("aa-aa", 5, "aa", 2, "long", 4, 1, 0, &output_length, &replaced);
	CHECK(output != NULL, "replacement can grow the document");
	if (output) {
		CHECK(strcmp(output, "long-long") == 0 && output_length == 9 && replaced == 2,
			"growing replacement produces the exact output length");
		free(output);
	}
	output = cair_replace_all("foo-bar", 7, "-", 1, "", 0, 1, 0, &output_length, &replaced);
	CHECK(output != NULL, "empty replacement deletes the match");
	if (output) {
		CHECK(strcmp(output, "foobar") == 0 && output_length == 6 && replaced == 1,
			"deletion replacement preserves surrounding text");
		free(output);
	}
	CHECK(cair_replace_all(text, strlen(text), "", 0, "x", 1, 1, 0, NULL, NULL) == NULL,
		"replace-all rejects an empty query");
	large_text = (char*)malloc(large_length + 1);
	CHECK(large_text != NULL, "allocate a two-megabyte search fixture");
	if (large_text) {
		memset(large_text, 'x', large_length);
		memcpy(large_text + large_length - 6, "target", 6);
		large_text[large_length] = '\0';
		CHECK(cair_find_count(large_text, large_length, "target", 6, 1, 0) == 1,
			"search a large file buffer");
		free(large_text);
	}
	property_checks();

	if (failures) return 1;
	puts("find and replace unit tests passed");
	return 0;
}
