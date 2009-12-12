/*
 
 AVLtree.cpp
 
 Position-independent, multi-threading compatible implementation of AVL trees,
 a form of balanced binary tree.
 
 The trees are assumed to be in a mapped, shared segment, so we use offset_ptrs instead
 of regular pointers, to make the tree structures position-independent.
 
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

#if 0
// There's a built-in "placement" operator new that does this already.
void *AVLtreeNode::operator new(size_t size, void *base)
{
    return base;
}
#endif
                        
void (*AVLuserHook)(AVLtreeNode *);

/* 
    Set the depth of a tree node, assuming the child nodes have correct depths.
 */

#if 0
void AVLtreeNode::setDepth()
{
    int ldepth = !!left? left->depth : 0;
    int rdepth = !!right? right->depth : 0;
    
    depth = (((ldepth > rdepth)? ldepth : rdepth) + 1);
    if (AVLuserHook)
        (*AVLuserHook)(this);
    if (!!parent)
        parent.get()->setDepth();
}
#endif


static void setDepth(offset_ptr<AVLtreeNode> t) {
    
    int ldepth = !!t->left? t->left->depth : 0;
    int rdepth = !!t->right? t->right->depth : 0;
    
    t->depth = (((ldepth > rdepth)? ldepth : rdepth) + 1);
    if (AVLuserHook)
        (*AVLuserHook)(t.get());
    if (!!t->parent)
        setDepth(t->parent);
    
}

/*
 A utility routine which adds a node "new" in place of node "old" immediately under
 node "t".  If "t" is null, the tree is re-rooted at "new".
 */
static void newSubTree(offset_ptr<AVLtreeNode> t, offset_ptr<AVLtreeNode> *tree_addr,
                       offset_ptr<AVLtreeNode> old, offset_ptr<AVLtreeNode> _new) 
{
    if (!!t) {
        if (t->left==old)
            t->left= _new;
        else if (t->right==old)
            t->right= _new;
    } else {
        *tree_addr = _new;
    }
    if (!! _new)
         _new->parent=t;
}




/*
    Rotate the tree right at a particular node.  Used in rebalancing.   
*/
static void rotateRight(offset_ptr<AVLtreeNode> t, offset_ptr<AVLtreeNode>* tree_addr) {
    offset_ptr<AVLtreeNode> l = t->left;
    offset_ptr<AVLtreeNode> lr = l->right;
    offset_ptr<AVLtreeNode> p;
    l->right = t;
    t->left = lr;
    if (!!lr)
        lr->parent = t;
    p = t->parent;
    t->parent = l;
     newSubTree(p, tree_addr,t,l);
    setDepth(t);
}


/*
    Rotate the tree left at a particular node.  Used in rebalancing.
*/
static void rotateLeft(offset_ptr<AVLtreeNode> t, offset_ptr<AVLtreeNode>* tree_addr) {
    offset_ptr<AVLtreeNode> r = t->right;
    offset_ptr<AVLtreeNode> rl = r->left;
    offset_ptr<AVLtreeNode> p;
    r->left = t;
    t->right = rl;
    if (!!rl)
        rl->parent = t;
    p = t->parent;
    t->parent = r;
    newSubTree(p, tree_addr,t,r);
    setDepth(t);
}



/*
 Compute the balance factor at a node "t".  Negative result indicates
 left-heavy, positive result indicates right-heavy.
 */
static int balance(offset_ptr<AVLtreeNode> t) {
    int ldepth = !!t->left? t->left->depth : 0;
    int rdepth = !!t->right? t->right->depth : 0;
    return (rdepth - ldepth);
}




/*
 Re-balance a tree starting at node "t" and working upward if necessary.  This
 is where the "AVL" double rotation algorithm is used.
 */

static void rebalance(offset_ptr<AVLtreeNode> t, offset_ptr<AVLtreeNode>* tree_addr) {
        
    int b = balance(t);
    if (b == 2) {
        if (balance(t->right) == -1)
            rotateRight(t->right, tree_addr);
        rotateLeft(t, tree_addr);
    } else if (b == -2) {
        if (balance(t->left) == 1)
            rotateLeft(t->left, tree_addr);
        rotateRight(t, tree_addr);
    }
    
    if (!!t && !!t->parent)
        rebalance(t->parent, tree_addr);
}

/*
 Add a node "i" to the tree "tree_addr".  This is recursive.
 t should start out the same as *tree_addr, but is used on each recursion to
 find the correct branch to insert into.  The tree is rebalanced, and possibly
 re-rooted after the insertion.
 */
static void __addToTree(offset_ptr<AVLtreeNode> i, offset_ptr<AVLtreeNode> *tree_addr, offset_ptr<AVLtreeNode> t,
                        int (*cmp)(void*,void*), void*(*getKey)(void*))
{
        
    if (!!t) {
        if ((*cmp)((*getKey)((void*)i.get()), (*getKey)((void*)t.get())) < 0) {
            if (!!t->left)
                __addToTree(i, tree_addr, t->left, cmp, getKey);
            else {
                t->left=i;
                i->parent = t;
                setDepth(i);
                rebalance(i, tree_addr);
            }
        } else {
            if (!!t->right)
                __addToTree(i, tree_addr, t->right, cmp, getKey);
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
                  int (*cmp)(void*,void*), void*(*getKey)(void*)) {
    i->parent = i->left = i->right = NULL;
    i->depth = 0;
    
    __addToTree(i, tree_addr, (*tree_addr), cmp, getKey);
}




/* 
    Removes a node  "t" from a tree.  This is where things get hairy, since
    rebalancing is a pain.  But it works.
*/ 
void AVLremoveFromTree(AVLtreeNode* t, offset_ptr<AVLtreeNode>* tree_addr) {
    offset_ptr<AVLtreeNode> moved = t->parent;
    offset_ptr<AVLtreeNode> s;
    if (!!t->left) {
        if (!!t->right) {
            /* there are two subtrees. */

            if (t->left->depth >= t->right->depth) {
                /* tree is left-heavy (or balanced) */
                s = t->left->right;
                if (!!s) {
                    while (!!s->right) s=s->right;
                    moved = s->parent;
                    s->parent->right=s->left;
                    if (!!s->left) {
                        s->left->parent=s->parent;
                    }
                    s->left=t->left;
                    t->left->parent = s;
                    // s->depth = t->depth;

                } else {
                    moved = s = t->left;
                }
                s->right = t->right;
                t->right->parent = s;
                
                newSubTree(t->parent, tree_addr,t,s);

            } else {
                /* tree is right-heavy */
                s = t->right->left;
                if (!!s) {
                    while (!!s->left) s=s->left;
                    moved = s->parent;
                    s->parent->left=s->right;
                    if (!!s->right) {
                        s->right->parent=s->parent;
                    }
                    s->right=t->right;
                    t->right->parent = s;
                    // s->depth = t->depth;

                } else {
                    moved = s = t->right;
                }
                s->left = t->left;
                t->left->parent = s;
    
                newSubTree(t->parent, tree_addr,t,s);

            }
        } else {
            /* left subtree only */
            newSubTree(t->parent, tree_addr,t,t->left);
        }
    } else if (!!t->right) {
        /* right subtree only */
        newSubTree(t->parent, tree_addr,t,t->right);
    
    } else {
        /* no subtrees */
        newSubTree(t->parent, tree_addr,t,NULL);
    }
    if(!!moved) {
        setDepth(moved);
        rebalance(moved, tree_addr);
    }
}



AVLtreeNode* AVLsearch(AVLtreeNode *t, void* key, int (*cmp)(void*,void*), void* (*getKey)(void*))
{
    int x;
    if ((x = (*cmp)((*getKey)((void*)t),key)) == 0) {
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



#if 0
AVLtreeNode* AVLsearch(AVLtreeNode *t, void* key)
{
    int x;
    if ((x = t->compareToKey(key)) == 0) {
        return t;
    } else if (x < 0) {
        if (!!t->right)
            return AVLsearch(t->right.get(), key);
        else
            return NULL;
    } else {
        if (!!t->left)
            return AVLsearch(t->left.get(), key);
        else
            return NULL;
    }
}
#endif



static long treesize(offset_ptr<AVLtreeNode> t) {
    return (1 + (!!t->left? treesize(t->left) : 0) + (!!t->right? treesize(t->right) : 0));
    
    
}


