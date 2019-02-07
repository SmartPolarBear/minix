#ifndef MINIX_SERVERS_FAT32_AVLTREE_H_
#define MINIX_SERVERS_FAT32_AVLTREE_H_
#include <stdint.h>

// datatype of value used
typedef void *TVal;
typedef int TKey;

typedef struct avl_tree
{
	struct node *root;
}avl_tree_t;

// assumes string keys
typedef struct node
{
	struct node *l; // left branch
	struct node *r; // right branch
	TKey k;			// key
	TVal d;			// data

	// height: length of longest path downwards. Height of an empty tree is -1.
	// Height of a singleton is 0.
	int32_t h;
} node_t;

// create a new AVL tree
 avl_tree_t *avl_create();

// insert the key, value pair (key, data) into the AVL Tree avl
void avl_insert( avl_tree_t *avl, TKey key, TVal data);

// return a pointer to the value in avl paired with key; return 0 if key is not
// found in avl
TVal *avl_lookup( avl_tree_t *avl, TKey key);

// remove the node in avl by key, if it exists
void avl_delete( avl_tree_t *avl, TKey key);

// deallocate the memory used by avl
void avl_destroy( avl_tree_t *avl);

#endif