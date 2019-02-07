#include "avl.h"
#include <stdlib.h>
#include <string.h>

template <typename T>
constexpr T max(T x, T y)
{
	return x < y ? y : x;
}

template <typename T>
constexpr decltype(auto) hl(T x)
{
	return (x->l ? x->l->h : -1);
}

template <typename T>
constexpr decltype(auto) hr(T x)
{
	return (x->r ? x->r->h : -1);
}

/************** helper functions/definitions ***************/
typedef struct munge
{
	node_t *n;
	TKey k;
	TVal d;
} munge;

// update the height of a node_t
void update_height(node_t *n)
{
	n->h = max(hl(n), hr(n)) + 1;
}

// deallocate a node_t
void node_destroy(node_t *n, int recurse)
{
	n->k = 0;
	if (recurse && n->l)
		node_destroy(n->l, 1);
	if (recurse && n->r)
		node_destroy(n->r, 1);
	free(n);
}

// create a node_t
node_t *node_create(TKey key, TVal data)
{
	node_t *n = (node_t *)malloc(sizeof(node_t));
	n->l = 0;
	n->r = 0;
	n->k = key;
	n->d = data;
	n->h = 0;
	return n;
}

// left rotation
node_t *lrot(node_t *n)
{
	node_t *oldright = n->r;
	n->r = n->r->l;
	oldright->l = n;

	update_height(n);
	update_height(oldright);
	return oldright;
}

// right rotation
node_t *rrot(node_t *n)
{
	node_t *oldleft = n->l;
	n->l = n->l->r;
	oldleft->r = n;

	update_height(n);
	update_height(oldleft);
	return oldleft;
}

// rebalance a node_t (assumed node_t is unbalanced by at most 1; heights differ <2)
node_t *rebalance(node_t *n)
{
	if (!n)
		return n;

	int lh = n->l ? n->l->h : -1;
	int rh = n->r ? n->r->h : -1;

	if (lh == rh + 2)
	{
		if (n->l->l && n->l->l->h == rh + 1)
		{ // right rotation
			n = rrot(n);
		}
		else if (n->l->r && n->l->r->h == rh + 1)
		{ // left-right rotation
			n->l = lrot(n->l);
			n = rrot(n);
		}
	}
	else if (rh == lh + 2)
	{
		if (n->r->r && n->r->r->h == lh + 1)
		{ // left rotation
			n = lrot(n);
		}
		else if (n->r->l && n->r->l->h == lh + 1)
		{ // right-left rotation
			n->r = rrot(n->r);
			n = lrot(n);
		}
	}

	return n;
}

// insert a kv pair into a node_t
node_t *node_insert(node_t *n, TKey key, TVal data)
{
	int cmp = n->k - key; //strcmp(n->k, key);
	if (cmp < 0)
		n->r = n->r ? node_insert(n->r, key, data) : node_create(key, data);
	else if (cmp > 0)
		n->l = n->l ? node_insert(n->l, key, data) : node_create(key, data);
	update_height(n);
	n = rebalance(n);
	return n;
}

// lookup a value in a node_t
TVal *node_lookup(node_t *n, TKey key)
{
	if (!n)
		return 0;
	int cmp = n->k - key; //strcmp(n->k, key);
	if (cmp < 0)
		return node_lookup(n->r, key);
	else if (cmp > 0)
		return node_lookup(n->l, key);
	else
		return &(n->d);
}

// remove the rightmost node_t of a nonempty tree while maintaining balance;
// return the new tree, and the key/data pair of the removed node_t
struct munge get_rightmost(node_t *n)
{
	if (n->r)
	{
		struct munge m = get_rightmost(n->r);
		n->r = m.n;
		update_height(n);
		m.n = rebalance(n);
		return m;
	}
	else
	{
		struct munge m;
		m.n = n->l;
		m.k = n->k;
		m.d = n->d;
		free(n);
		return m;
	}
}

// delete a value in a node_t
node_t *node_delete(node_t *n, TKey key)
{
	if (!n)
		return 0;
	int cmp = n->k - key; //strcmp(n->k, key);
	if (cmp == 0)
	{ // delete the root node_t
		if (!n->l)
		{
			node_t *r = n->r;
			node_destroy(n, 0);
			return r;
		}
		else if (!n->r)
		{
			node_t *l = n->l;
			node_destroy(n, 0);
			return l;
		}
		else
		{
			struct munge m = get_rightmost(n->l);
			//free(n->k);
			n->k = 0;
			n->l = m.n;
			n->k = m.k;
			n->d = m.d;
			update_height(n);
			return rebalance(n);
		}
	}
	if (cmp < 0)
		n->r = node_delete(n->r, key);
	if (cmp > 0)
		n->l = node_delete(n->l, key);
	update_height(n);
	return rebalance(n);
}

/************** implementation functions ***************/

avl_tree_t *avl_create()
{
	avl_tree_t *t = (avl_tree_t *)malloc(sizeof(avl_tree_t));
	t->root = 0;
	return t;
}

void avl_destroy(avl_tree_t *tree)
{
	node_destroy(tree->root, 1);
	free(tree);
}

void avl_insert(avl_tree_t *avl, TKey key, TVal data)
{
	if (avl->root)
	{
		avl->root = node_insert(avl->root, key, data);
	}
	else
	{
		avl->root = node_create(key, data);
	}
}

void avl_delete(avl_tree_t *avl, TKey key)
{
	avl->root = node_delete(avl->root, key);
}

TVal *avl_lookup(avl_tree_t *tree, TKey key)
{
	return node_lookup(tree->root, key);
}
