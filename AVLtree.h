/*
 
 AVLtree.h
 
 Interface to implementation of AVL trees, a form of balanced binary trees.
 
 Copyright 2009 Shel Kaphan
 
 This file is part of stmmap.
 
 stmmap is free software: you can redistribute it and/or modify
 it under the terms of the GNU Lesser General Public License as published by
 the Free Software Foundation, either version 3 of the License, or
 (at your option) any later version.
 
 stmmap is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU Lesser General Public License for more details.
 
 You should have received a copy of the GNU Lesser General Public License
 along with stmmap.  If not, see <http://www.gnu.org/licenses/>.
 
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
