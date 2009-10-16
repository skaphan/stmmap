/*
 *  AVLtree.h
 *  stmmap
 *
 *  Created by Shel Kaphan on 9/21/09.
 *
 *  Implements AVL trees, a form of balanced binary trees.
 *
 */


typedef struct AVLtreeNode
{ 
	struct AVLtreeNode* parent;
	struct AVLtreeNode* left;
	struct AVLtreeNode* right;
	int depth;
} AVLtreeNode;


/*
 Add a node "i" to the tree "*tree". The tree is rebalanced, and possibly
 re-rooted after the insertion (that's why the pointer-to-pointer is passed).
 */
void AVLaddToTree(AVLtreeNode* i, AVLtreeNode** tree, int (*cmp)(void*,void*), void*(*getKey)(void*));

/* 
 Removes a node  "t" from a tree.
 */ 
void AVLremoveFromTree(AVLtreeNode* t, AVLtreeNode** tree);

/*
 Search for a node in the tree using a user supplied comparison function, and key extractor.
 */
AVLtreeNode* AVLsearch(AVLtreeNode* t, void* key, int (*cmp)(void*,void*), void* (*getKey)(void*));

/* 
 For "subtypes" of AVLtreeNode, there's a hook which, if set, is called on each node when the
 node's depth is being calculated.
 */
extern void (*AVLuserHook)(AVLtreeNode*);
