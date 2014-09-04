#ifndef RBTREE_H

# define RBTREE_H

# include <unicode/ucol.h>

# define RBTREE_INSERT_ON_DUP_KEY_PRESERVE  (0)
# define RBTREE_INSERT_ON_DUP_KEY_FETCH     (1<<0)
# define RBTREE_INSERT_ON_DUP_KEY_OVERWRITE (1<<1) /* no sense with FETCH */
# define RBTREE_INSERT_ON_DUP_KEY_NO_DTOR   (1<<2) /* no sense with FETCH */

typedef int (*func_cmp_t)(const void *, const void *); /* Comparaison callback (like strcmp) */
typedef void (*func_apply_t)(const void *, void *);    /* Foreach callback (value) */

typedef enum
{
    IN_ORDER,  /* Infixed   */
    PRE_ORDER, /* Prefixed  */
    POST_ORDER /* Postfixed */
} traverse_mode_t;

typedef struct {
    uint8_t *key;
    int32_t key_len;
} RBKey;

typedef struct _RBTree RBTree;
typedef struct _RBTreeNode RBTreeNode;

void rbkey_destroy(RBKey *);
int ucol_key_cmp(const void *k1, const void *k2);
int ucol_key_cmp_r(const void *k1, const void *k2);

void rbtree_clear(RBTree *) NONNULL();
# ifdef DEBUG
void rbtree_debug(RBTree *, toUString)  NONNULL();
# endif /* DEBUG */
void rbtree_destroy(RBTree *) NONNULL();
int rbtree_empty(RBTree *) NONNULL();
int rbtree_exists(RBTree *, void *) NONNULL(1);
int rbtree_get(RBTree *, void *, void **) NONNULL(1, 3);
int rbtree_insert(RBTree *, void *, void *, uint32_t, void **) NONNULL(1);
RBTreeNode *rbtree_lookup(RBTree *, void *) NONNULL(1);
int rbtree_max(RBTree *, void **, void **) NONNULL(1);
int rbtree_min(RBTree *, void **, void **) NONNULL(1);
RBTree *rbtree_new(func_cmp_t, dup_t, dup_t, func_dtor_t, func_dtor_t) NONNULL(1) WARN_UNUSED_RESULT;
int rbtree_remove(RBTree *, void *) NONNULL(1);
int rbtree_replace(RBTree *, void *, void *) NONNULL(1);
void rbtree_traverse(RBTree *, traverse_mode_t, func_apply_t) NONNULL();

#endif /* !RBTREE_H */
