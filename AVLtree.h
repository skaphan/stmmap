/*
 *  AVLtree.h
 *  stmtest
 *
 *  Created by Shel Kaphan on 9/21/09.
 *  Copyright 2009 __MyCompanyName__. All rights reserved.
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

AVLtreeNode* AVLsearch(AVLtreeNode* t, void* key, int (*cmp)(void*,void*), void* (*getKey)(void*));

extern void (*AVLuserHook)(AVLtreeNode*);
