#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "model/project_tree.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int failures;

#define CHECK(condition, message) do { \
	if (!(condition)) { fprintf(stderr, "FAIL: %s\n", message); ++failures; } \
} while (0)

static int make_directory(const char* path) {
	return CreateDirectoryA(path, NULL) || GetLastError() == ERROR_ALREADY_EXISTS;
}

static void write_file(const char* path) {
	FILE* file = fopen(path, "wb");
	if (file) { fputs("int main(void) { return 0; }\n", file); fclose(file); }
}

static const CairProjectNode* find_child(const CairProjectNode* parent, const char* name) {
	size_t i;
	for (i = 0; parent && i < parent->child_count; ++i) {
		if (_stricmp(parent->children[i]->name, name) == 0) return parent->children[i];
	}
	return NULL;
}

static void remove_tree(const char* path) {
	char pattern[MAX_PATH];
	WIN32_FIND_DATAA data;
	HANDLE handle;
	if (snprintf(pattern, sizeof(pattern), "%s\\*", path) >= (int)sizeof(pattern)) return;
	handle = FindFirstFileA(pattern, &data);
	if (handle != INVALID_HANDLE_VALUE) {
		do {
			char child[MAX_PATH];
			if (strcmp(data.cFileName, ".") == 0 || strcmp(data.cFileName, "..") == 0) continue;
			if (snprintf(child, sizeof(child), "%s\\%s", path, data.cFileName) >= (int)sizeof(child)) continue;
			if (data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) remove_tree(child);
			else DeleteFileA(child);
		} while (FindNextFileA(handle, &data));
		FindClose(handle);
	}
	RemoveDirectoryA(path);
}

int main(void) {
	char temp[MAX_PATH];
	char root[MAX_PATH];
	char src[MAX_PATH];
	char main_file[MAX_PATH];
	char ignored[MAX_PATH];
	CairProjectTree tree = { 0 };
	const CairProjectNode* src_node;
	DWORD temp_length = GetTempPathA((DWORD)sizeof(temp), temp);

	CHECK(temp_length > 0 && temp_length < sizeof(temp), "Windows temp directory exists");
	_snprintf(root, sizeof(root), "%sCAIR-test-%lu", temp, GetCurrentProcessId());
	_snprintf(src, sizeof(src), "%s\\src", root);
	_snprintf(main_file, sizeof(main_file), "%s\\main.c", src);
	_snprintf(ignored, sizeof(ignored), "%s\\build", root);
	CHECK(make_directory(root), "create temporary project root");
	CHECK(make_directory(src), "create source directory");
	CHECK(make_directory(ignored), "create ignored build directory");
	write_file(main_file);

	CHECK(cair_project_tree_scan(&tree, root), "scan a project directory");
	CHECK(tree.root != NULL && tree.root->is_directory, "scan creates a directory root");
	src_node = find_child(tree.root, "src");
	CHECK(src_node != NULL, "scan includes source directories");
	CHECK(find_child(src_node, "main.c") != NULL, "scan includes files under source directories");
	CHECK(find_child(tree.root, "build") == NULL, "scan excludes build output");
	CHECK(tree.node_count == 3, "node count includes root and visible descendants");

	CHECK(!cair_project_tree_scan(&tree, main_file), "a file path is rejected as a project root");
	CHECK(tree.root == NULL && tree.node_count == 0, "failed rescan leaves an empty model");
	cair_project_tree_destroy(&tree);
	remove_tree(root);
	if (failures) return 1;
	puts("project tree unit tests passed");
	return 0;
}
