#ifndef CAIR_PROJECT_TREE_H
#define CAIR_PROJECT_TREE_H

#include <stddef.h>

typedef struct CairProjectNode {
	char* name;
	char* path;
	int is_directory;
	struct CairProjectNode** children;
	size_t child_count;
	size_t child_capacity;
} CairProjectNode;

typedef struct CairProjectTree {
	CairProjectNode* root;
	size_t node_count;
} CairProjectTree;

/* Initialize the tree with {0}; scanning frees its previous contents. */
/* Scans the root and at most five directory levels. */
int cair_project_tree_scan(CairProjectTree* tree, const char* root_path);
void cair_project_tree_destroy(CairProjectTree* tree);

#endif
