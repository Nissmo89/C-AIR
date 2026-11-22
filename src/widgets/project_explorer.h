#ifndef CAIR_PROJECT_EXPLORER_H
#define CAIR_PROJECT_EXPLORER_H

#include <Raven.h>
#include "model/project_tree.h"

typedef struct CairProjectExplorer {
	Rid root;
	Rid tree;
	const CairProjectTree* model;
	void (*on_open_file)(const CairProjectNode* node, void* user_data);
	void* user_data;
} CairProjectExplorer;

Rid cair_project_explorer_create(CairProjectExplorer* explorer, const CairProjectTree* model,
	void (*on_open_file)(const CairProjectNode* node, void* user_data), void* user_data);

#endif
