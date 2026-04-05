typedef struct DwindleNode DwindleNode;
struct DwindleNode {
	bool is_split;
	bool split_h;
	float ratio;
	int32_t container_w;
	int32_t container_h;
	DwindleNode *parent;
	DwindleNode *first;
	DwindleNode *second;
	Client *client;
};

static DwindleNode *dwindle_new_leaf(Client *c) {
	DwindleNode *n = calloc(1, sizeof(DwindleNode));
	n->client = c;
	return n;
}

static DwindleNode *dwindle_find_leaf(DwindleNode *node, Client *c) {
	if (!node)
		return NULL;
	if (!node->is_split)
		return node->client == c ? node : NULL;
	DwindleNode *r = dwindle_find_leaf(node->first, c);
	return r ? r : dwindle_find_leaf(node->second, c);
}

static DwindleNode *dwindle_first_leaf(DwindleNode *node) {
	if (!node)
		return NULL;
	while (node->is_split)
		node = node->first;
	return node;
}

static void dwindle_free_tree(DwindleNode *node) {
	if (!node)
		return;
	dwindle_free_tree(node->first);
	dwindle_free_tree(node->second);
	free(node);
}

static void dwindle_insert(DwindleNode **root, Client *new_c, Client *focused,
						   float ratio) {
	DwindleNode *new_leaf = dwindle_new_leaf(new_c);

	if (!*root) {
		*root = new_leaf;
		return;
	}

	DwindleNode *target = focused ? dwindle_find_leaf(*root, focused) : NULL;
	if (!target)
		target = dwindle_first_leaf(*root);

	DwindleNode *split = calloc(1, sizeof(DwindleNode));
	split->is_split = true;
	split->ratio = ratio;
	split->first = target;
	split->second = new_leaf;
	split->parent = target->parent;
	target->parent = split;
	new_leaf->parent = split;

	if (!split->parent) {
		*root = split;
	} else {
		if (split->parent->first == target)
			split->parent->first = split;
		else
			split->parent->second = split;
	}
}

static void dwindle_remove(DwindleNode **root, Client *c) {
	DwindleNode *leaf = dwindle_find_leaf(*root, c);
	if (!leaf)
		return;

	DwindleNode *parent = leaf->parent;
	if (!parent) {
		free(leaf);
		*root = NULL;
		return;
	}

	DwindleNode *sibling =
		(parent->first == leaf) ? parent->second : parent->first;
	DwindleNode *grandparent = parent->parent;
	sibling->parent = grandparent;

	if (!grandparent) {
		*root = sibling;
	} else {
		if (grandparent->first == parent)
			grandparent->first = sibling;
		else
			grandparent->second = sibling;
	}

	free(leaf);
	free(parent);
}

static void dwindle_assign(DwindleNode *node, int32_t ax, int32_t ay,
						   int32_t aw, int32_t ah, int32_t gap_h,
						   int32_t gap_v) {
	if (!node)
		return;

	if (!node->is_split) {
		if (node->client) {
			struct wlr_box box = {ax, ay, MAX(1, aw), MAX(1, ah)};
			resize(node->client, box, 0);
		}
		return;
	}

	if (node->container_w == 0 && node->container_h == 0)
		node->split_h = (aw >= ah);
	node->container_w = aw;
	node->container_h = ah;
	if (node->split_h) {
		int32_t w1 = MAX(1, (int32_t)(aw * node->ratio) - gap_h / 2);
		dwindle_assign(node->first, ax, ay, w1, ah, gap_h, gap_v);
		dwindle_assign(node->second, ax + w1 + gap_h, ay, aw - w1 - gap_h, ah,
					   gap_h, gap_v);
	} else {
		int32_t h1 = MAX(1, (int32_t)(ah * node->ratio) - gap_v / 2);
		dwindle_assign(node->first, ax, ay, aw, h1, gap_h, gap_v);
		dwindle_assign(node->second, ax, ay + h1 + gap_v, aw, ah - h1 - gap_v,
					   gap_h, gap_v);
	}
}

static void dwindle_move_client(DwindleNode **root, Client *c, Client *target,
								float ratio) {
	if (!c || !target || c == target)
		return;
	if (!dwindle_find_leaf(*root, c) || !dwindle_find_leaf(*root, target))
		return;
	dwindle_remove(root, c);
	dwindle_insert(root, c, target, ratio);
}

static void dwindle_swap_clients(DwindleNode **root, Client *a, Client *b) {
	DwindleNode *la = dwindle_find_leaf(*root, a);
	DwindleNode *lb = dwindle_find_leaf(*root, b);
	if (!la || !lb || la == lb)
		return;
	la->client = b;
	lb->client = a;
}

static void dwindle_resize_client(Monitor *m, Client *c, int32_t dx,
								  int32_t dy) {
	uint32_t tag = m->pertag->curtag;
	DwindleNode *leaf = dwindle_find_leaf(m->pertag->dwindle_root[tag], c);
	if (!leaf)
		return;

	bool want_h = abs(dx) >= abs(dy);

	DwindleNode *node = leaf->parent;
	while (node) {
		if (node->split_h == want_h) {
			float container_sz = want_h ? (float)MAX(1, node->container_w)
										: (float)MAX(1, node->container_h);

			float delta =
				want_h ? (float)dx / container_sz : (float)dy / container_sz;

			node->ratio += delta;
			node->ratio = CLAMP_FLOAT(node->ratio, 0.05f, 0.95f);

			int32_t n = m->visible_tiling_clients;
			int32_t gap_ih = enablegaps ? m->gappih : 0;
			int32_t gap_iv = enablegaps ? m->gappiv : 0;
			int32_t gap_oh = enablegaps ? m->gappoh : 0;
			int32_t gap_ov = enablegaps ? m->gappov : 0;
			if (config.smartgaps && n == 1)
				gap_ih = gap_iv = gap_oh = gap_ov = 0;
			dwindle_assign(m->pertag->dwindle_root[tag], m->w.x + gap_oh,
						   m->w.y + gap_ov, m->w.width - 2 * gap_oh,
						   m->w.height - 2 * gap_ov, gap_ih, gap_iv);
			return;
		}
		node = node->parent;
	}
}

static void dwindle_remove_client(Client *c) {
	Monitor *m;
	wl_list_for_each(m, &mons, link) {
		for (uint32_t t = 0; t < LENGTH(tags) + 1; t++)
			dwindle_remove(&m->pertag->dwindle_root[t], c);
	}
}

void dwindle(Monitor *m) {
	int32_t n = m->visible_tiling_clients;
	if (n == 0)
		return;

	uint32_t tag = m->pertag->curtag;
	DwindleNode **root = &m->pertag->dwindle_root[tag];
	float ratio = m->pertag->mfacts[tag];

	Client *vis[512];
	int32_t count = 0;
	Client *c;
	wl_list_for_each(c, &clients, link) {
		if (VISIBLEON(c, m) && ISTILED(c))
			vis[count++] = c;
		if (count >= 512)
			break;
	}

	{
		DwindleNode *leaves[512];
		int32_t lc = 0;

		DwindleNode *stack[1024];
		int32_t sp = 0;
		if (*root)
			stack[sp++] = *root;
		while (sp > 0) {
			DwindleNode *nd = stack[--sp];
			if (!nd->is_split) {
				leaves[lc++] = nd;
			} else {
				if (nd->second)
					stack[sp++] = nd->second;
				if (nd->first)
					stack[sp++] = nd->first;
			}
		}

		for (int32_t i = 0; i < lc; i++) {
			bool found = false;
			for (int32_t j = 0; j < count; j++)
				if (vis[j] == leaves[i]->client) {
					found = true;
					break;
				}
			if (!found)
				dwindle_remove(root, leaves[i]->client);
		}
	}

	Client *focused = focustop(m);
	if (focused && !dwindle_find_leaf(*root, focused))
		focused = m->sel;
	for (int32_t i = 0; i < count; i++) {
		if (!dwindle_find_leaf(*root, vis[i]))
			dwindle_insert(root, vis[i], focused, ratio);
	}

	int32_t gap_ih = enablegaps ? m->gappih : 0;
	int32_t gap_iv = enablegaps ? m->gappiv : 0;
	int32_t gap_oh = enablegaps ? m->gappoh : 0;
	int32_t gap_ov = enablegaps ? m->gappov : 0;
	if (config.smartgaps && n == 1)
		gap_ih = gap_iv = gap_oh = gap_ov = 0;

	dwindle_assign(*root, m->w.x + gap_oh, m->w.y + gap_ov,
				   m->w.width - 2 * gap_oh, m->w.height - 2 * gap_ov, gap_ih,
				   gap_iv);
}
