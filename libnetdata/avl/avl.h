// SPDX-License-Identifier: LGPL-3.0-or-later

#ifndef _AVL_H
#define _AVL_H 1

#include "../libnetdata.h"

/* Maximum AVL tree height. */
#ifndef AVL_MAX_HEIGHT
#define AVL_MAX_HEIGHT 92
#endif

#ifndef AVL_WITHOUT_PTHREADS
#include <pthread.h>

// #define AVL_LOCK_WITH_MUTEX 1

#ifdef AVL_LOCK_WITH_MUTEX
#define AVL_LOCK_INITIALIZER NETDATA_MUTEX_INITIALIZER
#else /* AVL_LOCK_WITH_MUTEX */
#define AVL_LOCK_INITIALIZER NETDATA_RWLOCK_INITIALIZER
#endif /* AVL_LOCK_WITH_MUTEX */

#else /* AVL_WITHOUT_PTHREADS */
#define AVL_LOCK_INITIALIZER
#endif /* AVL_WITHOUT_PTHREADS */

/* Data structures */

/* One element of the AVL tree */
typedef struct avl {
    struct avl *avl_link[2];  /* Subtrees. */
    signed char avl_balance;       /* Balance factor. */
} avl;

/* An AVL tree */
typedef struct avl_tree {
    avl *root;
    int (*compar)(void *a, void *b);
} avl_tree;

typedef struct avl_tree_lock {
    avl_tree avl_tree;

#ifndef AVL_WITHOUT_PTHREADS
#ifdef AVL_LOCK_WITH_MUTEX
    netdata_mutex_t mutex;
#else /* AVL_LOCK_WITH_MUTEX */
    netdata_rwlock_t rwlock;
#endif /* AVL_LOCK_WITH_MUTEX */
#endif /* AVL_WITHOUT_PTHREADS */
} avl_tree_lock;

/* Public methods */

/* Insert element a into the AVL tree t
 * returns the added element a, or a pointer the
 * element that is equal to a (as returned by t->compar())
 * a is linked directly to the tree, so it has to
 * be properly allocated by the caller.
 */
avl *avl_insert_lock(avl_tree_lock *tree, avl *item) NEVERNULL WARNUNUSED;
avl *avl_insert(avl_tree *tree, avl *item) NEVERNULL WARNUNUSED;

/* Remove an element a from the AVL tree t
 * returns a pointer to the removed element
 * or NULL if an element equal to a is not found
 * (equal as returned by t->compar())
 */
avl *avl_remove_lock(avl_tree_lock *tree, avl *item) WARNUNUSED;
avl *avl_remove(avl_tree *tree, avl *item) WARNUNUSED;

/* Find the element into the tree that equal to a
 * (equal as returned by t->compar())
 * returns NULL is no element is equal to a
 */
avl *avl_search_lock(avl_tree_lock *tree, avl *item);
avl *avl_search(avl_tree *tree, avl *item);

/* Initialize the avl_tree_lock
 */
void avl_init_lock(avl_tree_lock *tree, int (*compar)(void *a, void *b));
void avl_init(avl_tree *tree, int (*compar)(void *a, void *b));


int avl_traverse_lock(avl_tree_lock *tree, int (*callback)(void *entry, void *data), void *data);
int avl_traverse(avl_tree *tree, int (*callback)(void *entry, void *data), void *data);

#endif /* avl.h */
