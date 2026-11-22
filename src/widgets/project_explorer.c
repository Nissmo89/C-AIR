#include "widgets/project_explorer.h"

static void append_nodes(Rid tree, Rid parent_item, const CairProjectNode* node) {
	size_t i;
	Rid item = Rtree_view_add_item(tree, parent_item, node->name);
	if (item == R_INVALID_ID) return;
	Rtree_item_set_user_handle(item, (void*)node);
	for (i = 0; i < node->child_count; ++i) append_nodes(tree, item, node->children[i]);
}

static void selection_changed(Rid tree, Rid item, void* user_data) {
	CairProjectExplorer* explorer = (CairProjectExplorer*)user_data;
	const CairProjectNode* node;
	(void)tree;
	if (!explorer || item == R_INVALID_ID) return;
	node = (const CairProjectNode*)Rtree_item_get_user_handle(item);
	if (node && !node->is_directory && explorer->on_open_file) {
		explorer->on_open_file(node, explorer->user_data);
	}
}

Rid cair_project_explorer_create(CairProjectExplorer* explorer, const CairProjectTree* model,
	void (*on_open_file)(const CairProjectNode* node, void* user_data), void* user_data) {
	Rid title;
	if (!explorer || !model || !model->root) return R_INVALID_ID;
	explorer->model = model;
	explorer->on_open_file = on_open_file;
	explorer->user_data = user_data;
	explorer->root = Rwidget_create(R_WIDGET_VBOX);
	if (explorer->root == R_INVALID_ID) return R_INVALID_ID;
	Rpanel_set_spacing(explorer->root, 8.0f);
	Rwidget_set_min_size(explorer->root, 210.0f, 0.0f);
	title = Rlabel_create("PROJECT");
	if (title == R_INVALID_ID) return R_INVALID_ID;
	Rlabel_set_text_size(title, 13.0f);
	Rwidget_add_child(explorer->root, title);
	explorer->tree = Rwidget_add_child(explorer->root, Rwidget_create(R_WIDGET_TREE_VIEW));
	if (explorer->tree == R_INVALID_ID) return R_INVALID_ID;
	Rslot_set_fill(explorer->tree, 1.0f);
	Rtree_view_set_selection_changed_callback(explorer->tree, selection_changed, explorer);
	append_nodes(explorer->tree, R_INVALID_ID, model->root);
	{
		Rid root_item = Rtree_view_get_item(explorer->tree, 0);
		if (root_item != R_INVALID_ID) Rtree_item_set_expanded(root_item, R_TRUE);
	}
	return explorer->root;
}
