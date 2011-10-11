#ifndef RBTREE_H

# define RBTREE_H

# include <unicode/ucol.h>

typedef int (*func_cmp_t)(const void *, const void *); /* Comparaison callback (like strcmp)*/
typedef void (*func_apply_t)(const void *, void *); /* Foreach callback (value) */
//typedef void (*func_apply_arg_t)(const void *, void *, void *); /* Foreach callback (value, arg) */

typedef struct _RBTree RBTree;
typedef struct _RBTreeNode RBTreeNode;

typedef enum
{
    IN_ORDER,  /* Infixed   */
    PRE_ORDER, /* Prefixed  */
    POST_ORDER /* Postfixed */
} traverse_mode_t;

void rbtree_clear(RBTree *) NONNULL();
RBTree *rbtree_collated_new(UCollator *, func_dtor_t, func_dtor_t, int) NONNULL(1) WARN_UNUSED_RESULT;
void rbtree_destroy(RBTree *) NONNULL();
int rbtree_empty(RBTree *) NONNULL();
int rbtree_insert(RBTree *, void *, void *) NONNULL(1);
int rbtree_insert_node(RBTree *, RBTreeNode *, void *, RBTreeNode *, int) NONNULL(1, 2);
int rbtree_lookup(RBTree *, void *, void **) NONNULL(1);
int rbtree_lookup_node(RBTree *, void *, RBTreeNode **, int *, void **) NONNULL(1, 3, 4, 5);
int rbtree_max(RBTree *, void **, void **) NONNULL(1);
int rbtree_min(RBTree *, void **, void **) NONNULL(1);
RBTree *rbtree_new(func_cmp_t, func_dtor_t, func_dtor_t) NONNULL(1) WARN_UNUSED_RESULT;
int rbtree_remove(RBTree *, void *) NONNULL(1);
int rbtree_replace(RBTree *, void *, void *, int);
void rbtree_traverse(RBTree *, traverse_mode_t, func_apply_t) NONNULL();
// void rbtree_traverse_with_argument(RBTree *, traverse_mode_t, func_apply_arg_t, void *);
# ifdef TEST
INITIALIZER_DECL(rbtree_test);
# endif /* TEST */

#endif /* !RBTREE_H */