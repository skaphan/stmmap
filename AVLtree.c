#include <stdio.h>

#include "AVLtree.h"

						
void (*AVLuserHook)(AVLtreeNode*);

/* 
	Set the depth of a tree node, assuming the child nodes have correct depths.
 */
static void setDepth(AVLtreeNode* t) {
	int ldepth = t->left? t->left->depth : 0;
	int rdepth = t->right? t->right->depth : 0;
		
	t->depth = (((ldepth > rdepth)? ldepth : rdepth) + 1);
	if (AVLuserHook)
		(*AVLuserHook)(t);
	if (t->parent)
		setDepth(t->parent);
}


/*
 A utility routine which adds a node "new" in place of node "old" immediately under
 node "t".  If "t" is null, the tree is re-rooted at "new".
 */
static void newSubTree(AVLtreeNode* t, AVLtreeNode** tree,
					   AVLtreeNode* old, AVLtreeNode* new) 
{
	if (t) {
		if (t->left==old)
			t->left=new;
		else if (t->right==old)
			t->right=new;
	} else
		*tree = new;
	
	if (new)
		new->parent=t;
}




/*
	Rotate the tree right at a particular node.  Used in rebalancing.	
*/
static void rotateRight(AVLtreeNode* t, AVLtreeNode** tree) {
	AVLtreeNode* l = t->left;
  	AVLtreeNode* lr = l->right;
  	AVLtreeNode* p;
	l->right = t;
	t->left = lr;
	if (lr)
		lr->parent = t;
	p = t->parent;
	t->parent = l;
	newSubTree(p, tree,t,l);
	setDepth(t);
}


/*
	Rotate the tree left at a particular node.  Used in rebalancing.
*/
static void rotateLeft(AVLtreeNode* t, AVLtreeNode** tree) {
	AVLtreeNode* r = t->right;
	AVLtreeNode* rl = r->left;
	AVLtreeNode* p;
	r->left = t;
	t->right = rl;
	if (rl)
		rl->parent = t;
	p = t->parent;
	t->parent = r;
	newSubTree(p, tree,t,r);
  	setDepth(t);
}



/*
 Compute the balance factor at a node "t".  Negative result indicates
 left-heavy, positive result indicates right-heavy.
 */
static int balance(AVLtreeNode* t) {
	int ldepth = t->left? t->left->depth : 0;
	int rdepth = t->right? t->right->depth : 0;
	return (rdepth - ldepth);
}




/*
 Re-balance a tree starting at node "t" and working upward if necessary.  This
 is where the "AVL" double rotation algorithm is used.
 */

static void rebalance(AVLtreeNode* t, AVLtreeNode** tree) {
		
	int b = balance(t);
	if (b == 2) {
		if (balance(t->right) == -1)
			rotateRight(t->right, tree);
		rotateLeft(t, tree);
	} else if (b == -2) {
		if (balance(t->left) == 1)
			rotateLeft(t->left, tree);
		rotateRight(t, tree);
	}
	
	if (t && t->parent)
		rebalance(t->parent, tree);
}

/*
 Add a node "i" to the tree "tree".  This is recursive.
 t should start out the same as *tree, but is used on each recursion to
 find the correct branch to insert into.  The tree is rebalanced, and possibly
 re-rooted after the insertion.
 */
static void __addToTree(AVLtreeNode* i, AVLtreeNode** tree, AVLtreeNode* t,
						int (*cmp)(void*,void*), void*(*getKey)(void*))
{
		
	if (t) {
		if ((*cmp)((*getKey)(i), (*getKey)(t)) < 0) {
			if (t->left)
				__addToTree(i, tree, t->left, cmp, getKey);
			else {
				t->left=i;
				i->parent = t;
				setDepth(i);
				rebalance(i, tree);
			}
		} else {
			if (t->right)
				__addToTree(i, tree, t->right, cmp, getKey);
			else {
				t->right = i;
				i->parent = t;
				setDepth(i);
				rebalance(i, tree);
			}
		}
	} else {
		*tree = i;
		i->parent = NULL;
//		i->depth = 1;
		setDepth(i);
	}
}


void AVLaddToTree(AVLtreeNode* i, AVLtreeNode** tree, int (*cmp)(void*,void*), void*(*getKey)(void*)) {
	i->parent = i->left = i->right = NULL;
	i->depth = 0;
	__addToTree(i, tree, *tree, cmp, getKey);
}




/* 
	Removes a node  "t" from a tree.  This is where things get hairy, since
	rebalancing is a pain.  But it works.
*/ 
void AVLremoveFromTree(AVLtreeNode* t, AVLtreeNode** tree) {
	AVLtreeNode* moved = t->parent;
	AVLtreeNode* s;
	if (t->left) {
		if (t->right) {
			/* there are two subtrees. */

			if (t->left->depth >= t->right->depth) {
				/* tree is left-heavy (or balanced) */
				s = t->left->right;
				if (s) {
					while (s->right) s=s->right;
					moved = s->parent;
					s->parent->right=s->left;
					if (s->left) {
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
				
				newSubTree(t->parent, tree,t,s);

			} else {
				/* tree is right-heavy */
				s = t->right->left;
				if (s) {
					while (s->left) s=s->left;
					moved = s->parent;
					s->parent->left=s->right;
					if (s->right) {
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
	
				newSubTree(t->parent, tree,t,s);

			}
		} else {
			/* left subtree only */
			newSubTree(t->parent, tree,t,t->left);
		}
	} else if (t->right) {
		/* right subtree only */
		newSubTree(t->parent, tree,t,t->right);
    
	} else {
		/* no subtrees */
		newSubTree(t->parent, tree,t,NULL);
	}
	if(moved) {
		setDepth(moved);
		rebalance(moved, tree);
	}
}



AVLtreeNode* AVLsearch(AVLtreeNode* t, void* key, int (*cmp)(void*,void*), void* (*getKey)(void*))
{
	int x;
	if ((x = (*cmp)((*getKey)(t),key)) == 0) {
		return t;
	} else if (x < 0) {
		if (t->right)
			return AVLsearch(t->right, key, cmp, getKey);
		else
			return NULL;
	} else {
		if (t->left)
			return AVLsearch(t->left, key, cmp, getKey);
		else
			return NULL;
	}
}



static long treesize(AVLtreeNode* t) {
	return (1 + (t->left? treesize(t->left) : 0) + (t->right? treesize(t->right) : 0));
	
	
}


