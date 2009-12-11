/*
 
 AVLtree.c
 
 Implementation of AVL trees, a form of balanced binary tree.
 
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


#include <stdio.h>

#include "AVLtree.hpp"

                        
void (*AVLuserHook)(AVLtreeNode*);

/* 
    Set the depth of a tree node, assuming the child nodes have correct depths.
 */
static void setDepth(AVLtreeNode* t) {
    int ldepth = !!t->left? t->left->depth : 0;
    int rdepth = !!t->right? t->right->depth : 0;
        
    t->depth = (((ldepth > rdepth)? ldepth : rdepth) + 1);
    if (AVLuserHook)
        (*AVLuserHook)(t);
    if (!!t->parent)
        setDepth(t->parent.get());
}


/*
 A utility routine which adds a node "new" in place of node "old" immediately under
 node "t".  If "t" is null, the tree is re-rooted at "new".
 */
static void newSubTree(AVLtreeNode* t, offset_ptr<AVLtreeNode> *tree_addr,
                       AVLtreeNode* old, AVLtreeNode* _new) 
{
    if (t) {
        if (t->left.get()==old)
            t->left= _new;
        else if (t->right.get()==old)
            t->right= _new;
    } else {
        *tree_addr = _new;
    }
    if ( _new)
         _new->parent=t;
}




/*
    Rotate the tree right at a particular node.  Used in rebalancing.   
*/
static void rotateRight(AVLtreeNode* t, offset_ptr<AVLtreeNode>* tree_addr) {
    AVLtreeNode* l = t->left.get();
    AVLtreeNode* lr = l->right.get();
    AVLtreeNode* p;
    l->right = t;
    t->left = lr;
    if (lr)
        lr->parent = t;
    p = t->parent.get();
    t->parent = l;
     newSubTree(p, tree_addr,t,l);
    setDepth(t);
}


/*
    Rotate the tree left at a particular node.  Used in rebalancing.
*/
static void rotateLeft(AVLtreeNode* t, offset_ptr<AVLtreeNode>* tree_addr) {
    AVLtreeNode* r = t->right.get();
    AVLtreeNode* rl = r->left.get();
    AVLtreeNode* p;
    r->left = t;
    t->right = rl;
    if (rl)
        rl->parent = t;
    p = t->parent.get();
    t->parent = r;
    newSubTree(p, tree_addr,t,r);
    setDepth(t);
}



/*
 Compute the balance factor at a node "t".  Negative result indicates
 left-heavy, positive result indicates right-heavy.
 */
static int balance(AVLtreeNode* t) {
    int ldepth = !!t->left? t->left->depth : 0;
    int rdepth = !!t->right? t->right->depth : 0;
    return (rdepth - ldepth);
}




/*
 Re-balance a tree starting at node "t" and working upward if necessary.  This
 is where the "AVL" double rotation algorithm is used.
 */

static void rebalance(AVLtreeNode* t, offset_ptr<AVLtreeNode>* tree_addr) {
        
    int b = balance(t);
    if (b == 2) {
        if (balance(t->right.get()) == -1)
            rotateRight(t->right.get(), tree_addr);
        rotateLeft(t, tree_addr);
    } else if (b == -2) {
        if (balance(t->left.get()) == 1)
            rotateLeft(t->left.get(), tree_addr);
        rotateRight(t, tree_addr);
    }
    
    if (t && !!t->parent)
        rebalance(t->parent.get(), tree_addr);
}

/*
 Add a node "i" to the tree "tree_addr".  This is recursive.
 t should start out the same as *tree_addr, but is used on each recursion to
 find the correct branch to insert into.  The tree is rebalanced, and possibly
 re-rooted after the insertion.
 */
static void __addToTree(AVLtreeNode* i, offset_ptr<AVLtreeNode> *tree_addr, AVLtreeNode* t,
                        int (*cmp)(voidish*,voidish*), voidish*(*getKey)(voidish*))
{
        
    if (t) {
        if ((*cmp)((*getKey)((voidish*)i), (*getKey)((voidish*)t)) < 0) {
            if (!!t->left)
                __addToTree(i, tree_addr, t->left.get(), cmp, getKey);
            else {
                t->left=i;
                i->parent = t;
                setDepth(i);
                rebalance(i, tree_addr);
            }
        } else {
            if (!!t->right)
                __addToTree(i, tree_addr, t->right.get(), cmp, getKey);
            else {
                t->right = i;
                i->parent = t;
                setDepth(i);
                rebalance(i, tree_addr);
            }
        }
    } else {
        *tree_addr = i;
        i->parent = NULL;
//      i->depth = 1;
        setDepth(i);
    }
}


void AVLaddToTree(AVLtreeNode* i, offset_ptr<AVLtreeNode>* tree_addr,
                  int (*cmp)(voidish*,voidish*), voidish*(*getKey)(voidish*)) {
    i->parent = i->left = i->right = NULL;
    i->depth = 0;
    
    __addToTree(i, tree_addr, (*tree_addr).get(), cmp, getKey);
}




/* 
    Removes a node  "t" from a tree.  This is where things get hairy, since
    rebalancing is a pain.  But it works.
*/ 
void AVLremoveFromTree(AVLtreeNode* t, offset_ptr<AVLtreeNode>* tree_addr) {
    AVLtreeNode* moved = t->parent.get();
    AVLtreeNode* s;
    if (!!t->left) {
        if (!!t->right) {
            /* there are two subtrees. */

            if (t->left->depth >= t->right->depth) {
                /* tree is left-heavy (or balanced) */
                s = t->left->right.get();
                if (s) {
                    while (!!s->right) s=s->right.get();
                    moved = s->parent.get();
                    s->parent->right=s->left;
                    if (s->left.get()) {
                        s->left->parent=s->parent;
                    }
                    s->left=t->left;
                    t->left->parent = s;
                    // s->depth = t->depth;

                } else {
                    moved = s = t->left.get();
                }
                s->right = t->right;
                t->right->parent = s;
                
                newSubTree(t->parent.get(), tree_addr,t,s);

            } else {
                /* tree is right-heavy */
                s = t->right->left.get();
                if (s) {
                    while (!!s->left) s=s->left.get();
                    moved = s->parent.get();
                    s->parent->left=s->right;
                    if (!!s->right) {
                        s->right->parent=s->parent;
                    }
                    s->right=t->right;
                    t->right->parent = s;
                    // s->depth = t->depth;

                } else {
                    moved = s = t->right.get();
                }
                s->left = t->left;
                t->left->parent = s;
    
                newSubTree(t->parent.get(), tree_addr,t,s);

            }
        } else {
            /* left subtree only */
            newSubTree(t->parent.get(), tree_addr,t,t->left.get());
        }
    } else if (!!t->right) {
        /* right subtree only */
        newSubTree(t->parent.get(), tree_addr,t,t->right.get());
    
    } else {
        /* no subtrees */
        newSubTree(t->parent.get(), tree_addr,t,NULL);
    }
    if(moved) {
        setDepth(moved);
        rebalance(moved, tree_addr);
    }
}



AVLtreeNode* AVLsearch(AVLtreeNode* t, voidish* key, int (*cmp)(voidish*,voidish*), voidish* (*getKey)(voidish*))
{
    int x;
    if ((x = (*cmp)((*getKey)((voidish*)t),key)) == 0) {
        return t;
    } else if (x < 0) {
        if (!!t->right)
            return AVLsearch(t->right.get(), key, cmp, getKey);
        else
            return NULL;
    } else {
        if (!!t->left)
            return AVLsearch(t->left.get(), key, cmp, getKey);
        else
            return NULL;
    }
}



static long treesize(AVLtreeNode* t) {
    return (1 + (!!t->left? treesize(t->left.get()) : 0) + (!!t->right? treesize(t->right.get()) : 0));
    
    
}


