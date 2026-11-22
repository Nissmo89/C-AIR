#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "model/project_tree.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CAIR_PROJECT_MAX_ITEMS 4096
#define CAIR_PROJECT_MAX_DEPTH 5

static char* copy_string(const char* text) {
	size_t length;
	char* copy;
	if (!text) return NULL;
	length = strlen(text) + 1;
	copy = (char*)malloc(length);
	if (copy) memcpy(copy, text, length);
	return copy;
}

static void destroy_node(CairProjectNode* node) {
	size_t i;
	if (!node) return;
	for (i = 0; i < node->child_count; ++i) destroy_node(node->children[i]);
	free(node->children);
	free(node->name);
	free(node->path);
	free(node);
}

void cair_project_tree_destroy(CairProjectTree* tree) {
	if (!tree) return;
	destroy_node(tree->root);
	tree->root = NULL;
	tree->node_count = 0;
}

static int skipped_directory(const char* name) {
	static const char* skipped[] = {
		".git", ".agents", ".codex", "build", "install", "target",
		"third_party", "node_modules", NULL
	};
	size_t i;
	for (i = 0; skipped[i]; ++i) {
		if (_stricmp(name, skipped[i]) == 0) return 1;
	}
	return 0;
}

static CairProjectNode* make_node(const char* name, const char* path, int is_directory) {
	CairProjectNode* node = (CairProjectNode*)calloc(1, sizeof(*node));
	if (!node) return NULL;
	node->name = copy_string(name);
	node->path = copy_string(path);
	node->is_directory = is_directory;
	if (!node->name || !node->path) {
		destroy_node(node);
		return NULL;
	}
	return node;
}

static int append_child(CairProjectNode* parent, CairProjectNode* child) {
	CairProjectNode** children;
	size_t capacity;
	if (parent->child_count == parent->child_capacity) {
		capacity = parent->child_capacity ? parent->child_capacity * 2 : 8;
		children = (CairProjectNode**)realloc(parent->children, capacity * sizeof(*children));
		if (!children) return 0;
		parent->children = children;
		parent->child_capacity = capacity;
	}
	parent->children[parent->child_count++] = child;
	return 1;
}

static void scan_directory(CairProjectTree* tree, CairProjectNode* parent, int depth) {
	WIN32_FIND_DATAA data;
	HANDLE find_handle;
	char pattern[MAX_PATH];
	if (depth >= CAIR_PROJECT_MAX_DEPTH || tree->node_count >= CAIR_PROJECT_MAX_ITEMS) return;
	if (snprintf(pattern, sizeof(pattern), "%s\\*", parent->path) >= (int)sizeof(pattern)) return;
	find_handle = FindFirstFileA(pattern, &data);
	if (find_handle == INVALID_HANDLE_VALUE) return;
	do {
		char path[MAX_PATH];
		int is_directory;
		CairProjectNode* child;
		if (strcmp(data.cFileName, ".") == 0 || strcmp(data.cFileName, "..") == 0) continue;
		if (data.dwFileAttributes & FILE_ATTRIBUTE_HIDDEN) continue;
		is_directory = (data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
		if (is_directory && (data.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT)) continue;
		if (is_directory && skipped_directory(data.cFileName)) continue;
		if (snprintf(path, sizeof(path), "%s\\%s", parent->path, data.cFileName) >= (int)sizeof(path)) continue;
		child = make_node(data.cFileName, path, is_directory);
		if (!child) break;
		if (!append_child(parent, child)) {
			destroy_node(child);
			break;
		}
		++tree->node_count;
		if (is_directory) scan_directory(tree, child, depth + 1);
	} while (FindNextFileA(find_handle, &data) && tree->node_count < CAIR_PROJECT_MAX_ITEMS);
	FindClose(find_handle);
}

int cair_project_tree_scan(CairProjectTree* tree, const char* root_path) {
	const char* name;
	DWORD attributes;
	if (!tree) return 0;
	cair_project_tree_destroy(tree);
	if (!root_path || !root_path[0]) return 0;
	attributes = GetFileAttributesA(root_path);
	if (attributes == INVALID_FILE_ATTRIBUTES || !(attributes & FILE_ATTRIBUTE_DIRECTORY)) return 0;
	name = strrchr(root_path, '\\');
	name = name ? name + 1 : root_path;
	if (!name[0]) name = root_path;
	tree->root = make_node(name, root_path, 1);
	if (!tree->root) return 0;
	tree->node_count = 1;
	scan_directory(tree, tree->root, 0);
	return 1;
}
