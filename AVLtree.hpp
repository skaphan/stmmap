/*
 
 AVLtree.hpp
 
 Interface to position-independent implementation of AVL trees, a form of balanced
 binary trees. This is a low level implementation which does not depend on anything else
 in stmmap in any way.
 
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

#include <boost/interprocess/managed_shared_memory.hpp>
#include <boost/interprocess/offset_ptr.hpp>

using namespace boost::interprocess;

    
class AVLtreeNode
{ 
public:
    offset_ptr<AVLtreeNode> parent;
    offset_ptr<AVLtreeNode> left;
    offset_ptr<AVLtreeNode> right;
    int depth;
};

typedef char voidish;

/*
 Add a node "i" to the tree "*tree". The tree is rebalanced, and possibly
 re-rooted after the insertion (that's why the pointer-to-pointer is passed).
 */
void AVLaddToTree(AVLtreeNode* i, offset_ptr<AVLtreeNode>* tree_addr, int (*cmp)(voidish*,voidish*), voidish*(*getKey)(voidish*));

/* 
 Removes a node  "t" from a tree.
 */ 
void AVLremoveFromTree(AVLtreeNode* t, offset_ptr<AVLtreeNode>* tree_addr);

/*
 Search for a node in the tree using a user supplied comparison function, and key extractor.
 */
AVLtreeNode* AVLsearch(AVLtreeNode* t, voidish* key, int (*cmp)(voidish*,voidish*), voidish* (*getKey)(voidish*));

/* 
 For "subtypes" of AVLtreeNode, there's a hook which, if set, is called on each node when the
 node's depth is being calculated.
 */
extern void (*AVLuserHook)(AVLtreeNode*);
    

