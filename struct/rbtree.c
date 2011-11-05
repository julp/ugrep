#include "common.h"
#include "rbtree.h"

#define MAINTAIN_FIRST_LAST

static int ucol_key_cmp(const void *k1, const void *k2)
{
    return strcmp((const char *) k1, (const char *) k2);
}

static int ucol_key_cmp_r(const void *k1, const void *k2)
{
    return strcmp((const char *) k2, (const char *) k1);
}

typedef enum {
    BLACK = 0,
    RED   = 1
} RBTreeColor;

struct _RBTreeNode {
    void *key;
    void *computed_key;
    void *value;
    RBTreeColor color;
    RBTreeNode *left;
    RBTreeNode *right;
    RBTreeNode *parent;
};

struct _RBTree {
    RBTreeNode *nil;
    RBTreeNode *root;
#ifdef MAINTAIN_FIRST_LAST
    RBTreeNode *first;
    RBTreeNode *last;
#endif /* MAINTAIN_FIRST_LAST */
    func_cmp_t cmp_func;
    func_dtor_t key_dtor_func;
    func_dtor_t value_dtor_func;
    dup_t key_duper;
    dup_t value_duper;
    //func_dtor_t computed_key_dtor_func;
    func_key_compute_t key_compute_func;
    void *private_compute_data;
};

static RBTreeNode *rbtreenode_new(void *key, void *value)
{
    RBTreeNode *node;

    node = mem_new(*node);
    node->right = node->left = node->parent = NULL;
    node->color = RED;
    node->key = key;
    node->value = value;
    node->computed_key = NULL;

    return node;
}

static RBTreeNode *rbtreenode_nil_new(void)
{
    RBTreeNode *node;

    node = mem_new(*node);
    node->right = node->left = node->parent = node;
    node->color = BLACK;
    node->key = NULL;
    node->value = NULL;
    node->computed_key = NULL;

    return node;
}

RBTree *rbtree_new(
    func_cmp_t cmp_func,
    dup_t key_duper, dup_t value_duper,
    func_dtor_t key_dtor_func, func_dtor_t value_dtor_func
) /* NONNULL(1) WARN_UNUSED_RESULT */ {
    RBTree *tree;

    require_else_return_null(NULL != cmp_func);

    tree = mem_new(*tree);
    tree->key_duper = key_duper;
    tree->value_duper = value_duper;
    tree->root = tree->nil = rbtreenode_nil_new();
#ifdef MAINTAIN_FIRST_LAST
    tree->first = tree->last = tree->nil;
#endif
    tree->cmp_func = cmp_func;
    tree->key_dtor_func = key_dtor_func;
    tree->value_dtor_func = value_dtor_func;
    //tree->computed_key_dtor_func = NULL;
    tree->private_compute_data = NULL;

    return tree;
}

RBTree *rbtree_collated_new(
    UCollator *ucol, int inversed,
    dup_t key_duper, dup_t value_duper,
    func_dtor_t key_dtor_func, func_dtor_t value_dtor_func
) /* NONNULL(1) WARN_UNUSED_RESULT */ {
    RBTree *tree;

    require_else_return_null(NULL != ucol);

    tree = rbtree_new(inversed ? ucol_key_cmp_r : ucol_key_cmp, key_duper, value_duper, key_dtor_func, value_dtor_func);
    tree->private_compute_data = ucol;
    tree->key_compute_func = ustring_to_collation_key;

    return tree;
}

int rbtree_empty(RBTree *tree) /* NONNULL() */
{
    require_else_return_zero(NULL != tree);

    return tree->nil == tree->root;
}

static void rbtree_rotate_left(RBTree *tree, RBTreeNode *node) /* NONNULL() */
{
    RBTreeNode *p;

    p = node->right;
    node->right = p->left;
    if (p->left != tree->nil) {
        p->left->parent = node;
    }
    p->parent = node->parent;
    if (node->parent == tree->nil) {
        tree->root = p;
    } else if (node == node->parent->left) {
        node->parent->left = p;
    } else {
        node->parent->right = p;
    }
    p->left = node;
    node->parent = p;
}

static void rbtree_rotate_right(RBTree *tree, RBTreeNode *node) /* NONNULL() */
{
    RBTreeNode *p;

    p = node->left;
    node->left = p->right;
    if (p->right != tree->nil) {
        p->right->parent = node;
    }
    p->parent = node->parent;
    if (node->parent == tree->nil) {
        tree->root = p;
    } else if (node == node->parent->right) {
        node->parent->right = p;
    } else {
        node->parent->left = p;
    }
    p->right = node;
    node->parent = p;
}

static RBTreeNode *rbtreenode_max(RBTree *tree, RBTreeNode *node) /* NONNULL() */
{
    while (node->right != tree->nil) {
        node = node->right;
    }

    return node;
}

static RBTreeNode *rbtreenode_previous(RBTree *tree, RBTreeNode *node) /* NONNULL() */
{
    if (node->left != tree->nil) {
        return rbtreenode_max(tree, node->left);
    } else {
        RBTreeNode *y;

        y = node->parent;
        while (y != tree->nil && node == y->left) {
            node = y;
            y = y->parent;
        }

        return y;
    }
}

static RBTreeNode *rbtreenode_min(RBTree *tree, RBTreeNode *node) /* NONNULL() */
{
    while (node->left != tree->nil) {
        node = node->left;
    }

    return node;
}

static RBTreeNode *rbtreenode_next(RBTree *tree, RBTreeNode *node) /* NONNULL() */
{
    if (node->right != tree->nil) {
        return rbtreenode_min(tree, node->right);
    } else {
        RBTreeNode *y;

        y = node->parent;
        while (y != tree->nil && node == y->right) {
            node = y;
            y = y->parent;
        }

        return y;
    }
}

int rbtree_insert(RBTree *tree, void *key, void *value, uint32_t flags, void **oldvalue) /* NONNULL(1) */
{
    int cmp;
    void *computed_key;
    RBTreeNode *y, *x, *new;

    computed_key = NULL;
    if (NULL != tree->key_compute_func) {
        computed_key = tree->key_compute_func(key, tree->private_compute_data);
    }
    y = tree->nil;
    x = tree->root;
    while (x != tree->nil) {
        y = x;
        if (NULL != tree->key_compute_func) {
            cmp = tree->cmp_func(computed_key, x->computed_key);
        } else {
            cmp = tree->cmp_func(key, x->key);
        }
        if (0 == cmp) {
            if (flags & RBTREE_INSERT_ON_DUP_KEY_FETCH) {
                *oldvalue = x->value;
            } else {
                if (flags & RBTREE_INSERT_ON_DUP_KEY_NO_DTOR) {
                    if (NULL != tree->value_dtor_func) {
                        tree->value_dtor_func(x->value);
                    }
                    x->value = clone(tree->value_duper, value);
                }
                if (flags & RBTREE_INSERT_ON_DUP_KEY_OVERWRITE) {
                    x->value = clone(tree->value_duper, value);
                }
            }
            if (NULL != computed_key) {
                free(computed_key);
            }
            return 0;
        } else if (cmp < 0) {
            x = x->left;
        } else /*if (cmp > 0)*/ {
            x = x->right;
        }
    }
    new = rbtreenode_new(clone(tree->key_duper, key), clone(tree->value_duper, value));
    if (NULL != tree->key_compute_func) {
        new->computed_key = computed_key;
    }
    new->parent = y;
    new->left = tree->nil;
    new->right = tree->nil;
    //new->color = RED; // done in rbtreenode_new
    if (y == tree->nil) {
        tree->root = new;
#ifdef MAINTAIN_FIRST_LAST
        tree->first = tree->last = new;
#endif /* MAINTAIN_FIRST_LAST */
    } else {
        if (NULL != tree->key_compute_func) {
            cmp = tree->cmp_func(new->computed_key, y->computed_key);
        } else {
            cmp = tree->cmp_func(new->key, y->key);
        }
        if (cmp < 0) {
            y->left = new;
#ifdef MAINTAIN_FIRST_LAST
            if (y == tree->first) {
                tree->first = new;
            }
#endif /* MAINTAIN_FIRST_LAST */
        } else /*if (cmp > 0)*/ {
            y->right = new;
#ifdef MAINTAIN_FIRST_LAST
            if (y == tree->last) {
                tree->last = new;
            }
#endif /* MAINTAIN_FIRST_LAST */
        }
    }

    while (RED == new->parent->color) {
        RBTreeNode *y;

        if (new->parent == new->parent->parent->left) {
            y = new->parent->parent->right;
            if (RED == y->color) {
                new->parent->color = BLACK;
                y->color = BLACK;
                new->parent->parent->color = RED;
                new = new->parent->parent;
            } else {
                if (new == new->parent->right) {
                    new = new->parent;
                    rbtree_rotate_left(tree, new);
                }
                new->parent->color = BLACK;
                new->parent->parent->color = RED;
                rbtree_rotate_right(tree, new->parent->parent);
            }
        } else /*if (new->parent == new->parent->parent->right)*/ {
            y = new->parent->parent->left;
            if (RED == y->color) {
                new->parent->color = BLACK;
                y->color = BLACK;
                new->parent->parent->color = RED;
                new = new->parent->parent;
            } else {
                if (new == new->parent->left) {
                    new = new->parent;
                    rbtree_rotate_right(tree, new);
                }
                new->parent->color = BLACK;
                new->parent->parent->color = RED;
                rbtree_rotate_left(tree, new->parent->parent);
            }
        }
    }
    tree->root->color = BLACK;

    return 1;
}

RBTreeNode *rbtree_lookup(RBTree *tree, void *key) /* NONNULL(1) */
{
    int cmp;
    RBTreeNode *y, *x;
    void *computed_key;

    computed_key = NULL;
    if (NULL != tree->key_compute_func) {
        computed_key = tree->key_compute_func(key, tree->private_compute_data);
    }
    y = tree->nil;
    x = tree->root;
    while (x != tree->nil) {
        y = x;
        if (NULL != tree->key_compute_func) {
            cmp = tree->cmp_func(computed_key, x->computed_key);
        } else {
            cmp = tree->cmp_func(key, x->key);
        }
        if (0 == cmp) {
            if (NULL != computed_key) {
                free(computed_key);
            }
            return x;
        } else if (cmp < 0) {
            x = x->left;
        } else /*if (cmp > 0)*/ {
            x = x->right;
        }
    }
    if (NULL != computed_key) {
        free(computed_key);
    }

    return NULL;
}

int rbtree_get(RBTree *tree, void *key, void **value) /* NONNULL(1, 3) */
{
    RBTreeNode *node;

    if (NULL != (node = rbtree_lookup(tree, key))) {
        *value = node->value;
        return 1;
    }

    return 0;
}

int rbtree_exists(RBTree *tree, void *key) /* NONNULL(1) */
{
    return NULL != rbtree_lookup(tree, key);
}

int rbtree_replace(RBTree *tree, void *key, void *newvalue)  /* NONNULL(1) */
{
    RBTreeNode *node;

    if (NULL != (node = rbtree_lookup(tree, key))) {
        if (NULL != tree->value_dtor_func) {
            tree->value_dtor_func(node->value);
        }
        node->value = clone(tree->value_duper, newvalue);
        return 1;
    }

    return 0;
}

int rbtree_min(RBTree *tree, void **key, void **value) /* NONNULL(1) */
{
    require_else_return_zero(NULL != tree);

#ifdef MAINTAIN_FIRST_LAST
    if (tree->first == tree->nil) {
        return 0;
    } else {
        if (NULL != key) {
            *key = tree->first->key;
        }
        if (NULL != value) {
            *value = tree->first->value;
        }

        return 1;
    }
#else
    if (tree->root == tree->nil) {
        return 0;
    } else {
        RBTreeNode *min;

        min = rbtreenode_min(tree, tree->root); // on peut faire directement le test min != tree->nil ?
        if (NULL != key) {
            *key = min->key;
        }
        if (NULL != value) {
            *value = min->value;
        }

        return 1;
    }
#endif /* MAINTAIN_FIRST_LAST */
}

int rbtree_max(RBTree *tree, void **key, void **value) /* NONNULL(1) */
{
    require_else_return_zero(NULL != tree);

#ifdef MAINTAIN_FIRST_LAST
    if (tree->last == tree->nil) {
        return 0;
    } else {
        if (NULL != key) {
            *key = tree->last->key;
        }
        if (NULL != value) {
            *value = tree->last->value;
        }

        return 1;
    }
#else
    if (tree->root == tree->nil) {
        return 0;
    } else {
        RBTreeNode *max;

        max = rbtreenode_max(tree, tree->root);
        if (NULL != key) {
            *key = max->key;
        }
        if (NULL != value) {
            *value = max->value;
        }

        return 1;
    }
#endif /* MAINTAIN_FIRST_LAST */
}

static void rbtree_transplante(RBTree *tree, RBTreeNode *u, RBTreeNode *v) /* NONNULL() */
{
    if (u->parent == tree->nil) {
        tree->root = v;
    } else if (u == u->parent->left) {
        u->parent->left = v;
    } else {
        u->parent->right = v;
    }
    v->parent = u->parent;
}

int rbtree_remove(RBTree *tree, void *key) /* NONNULL(1) */
{
    RBTreeNode *y, *x, *w, *z;
    RBTreeColor ycolor;

    if (NULL == (z = rbtree_lookup(tree, key))) {
        return 0;
    }
#ifdef MAINTAIN_FIRST_LAST
    if (z == tree->first) {
        tree->first = rbtreenode_next(tree, z);
    } else if (z == tree->last) {
        tree->last = rbtreenode_previous(tree, z);
    }
#endif /* MAINTAIN_FIRST_LAST */
    y = z;
    ycolor = y->color;
    if (z->left == tree->nil) {
        x = z->right;
        rbtree_transplante(tree, z, z->right);
    } else if (z->right == tree->nil) {
        x = z->left;
        rbtree_transplante(tree, z, z->left);
    } else {
        y = rbtreenode_min(tree, z->right);
        ycolor = y->color;
        x = y->right;
        if (y->parent == z) {
            x->parent = y;
        } else {
            rbtree_transplante(tree, y, y->right);
            y->right = z->right;
            y->right->parent = y;
        }
        rbtree_transplante(tree, z, y);
        y->left = z->left;
        y->left->parent = y;
        y->color = z->color;
    }
    if (BLACK == ycolor) {
        while (x != tree->root && BLACK == x->color) {
            if (x == x->parent->left) {
                w = x->parent->right;
                if (RED == w->color) {
                    w->color = BLACK;
                    x->parent->color = RED;
                    rbtree_rotate_left(tree, x->parent);
                    w = x->parent->right;
                }
                if (BLACK == w->left->color && BLACK == w->right->color) {
                    w->color = RED;
                    x = x->parent;
                } else {
                    if (BLACK == w->right->color) {
                        w->left->color = BLACK;
                        w->color = RED;
                        rbtree_rotate_right(tree, w);
                        w = x->parent->right;
                    }
                    w->color = x->parent->color;
                    x->parent->color = BLACK;
                    w->right->color = BLACK;
                    rbtree_rotate_left(tree, x->parent);
                    x = tree->root;
                }
            } else {
                w = x->parent->left;
                if (RED == w->color) {
                    w->color = BLACK;
                    x->parent->color = RED;
                    rbtree_rotate_right(tree, x->parent);
                    w = x->parent->left;
                }
                if (BLACK == w->right->color && BLACK == w->left->color) {
                    w->color = RED;
                    x = x->parent;
                } else {
                    if (BLACK == w->left->color) {
                        w->right->color = BLACK;
                        w->color = RED;
                        rbtree_rotate_left(tree, w);
                        w = x->parent->left;
                    }
                    w->color = x->parent->color;
                    x->parent->color = BLACK;
                    w->left->color = BLACK;
                    rbtree_rotate_right(tree, x->parent);
                    x = tree->root;
                }
            }
        }
        x->color = BLACK;
    }
    if (NULL != tree->value_dtor_func) {
        tree->value_dtor_func(z->value);
    }
    if (NULL != tree->key_dtor_func) {
        tree->key_dtor_func(z->key);
    }
    if (NULL != z->computed_key) {
        free(z->computed_key);
    }
    free(z);

    return 1;
}

static void _rbtree_destroy(RBTree *tree, RBTreeNode *node) /* NONNULL(1) */
{
    require_else_return(NULL != tree);

    if (node != tree->nil) {
        _rbtree_destroy(tree, node->right);
        _rbtree_destroy(tree, node->left);
        if (NULL != tree->value_dtor_func) {
            tree->value_dtor_func(node->value);
        }
        if (NULL != tree->key_dtor_func) {
            tree->key_dtor_func(node->key);
        }
        if (NULL != node->computed_key) {
            free(node->computed_key);
        }
        free(node);
    }
}

void rbtree_clear(RBTree *tree) /* NONNULL() */
{
    require_else_return(NULL != tree);

    _rbtree_destroy(tree, tree->root);
    tree->root = tree->nil;
}

void rbtree_destroy(RBTree *tree) /* NONNULL() */
{
    require_else_return(NULL != tree);

    _rbtree_destroy(tree, tree->root);
    free(tree->nil);
    free(tree);
}

static void _rbtree_traverse_in_order(RBTree *tree, RBTreeNode *node, func_apply_t trav_func) /* NONNULL(2) */
{
    if (node != tree->nil) {
        _rbtree_traverse_in_order(tree, node->left, trav_func);
        trav_func(node->key, node->value);
        _rbtree_traverse_in_order(tree, node->right, trav_func);
    }
}

#ifdef DEBUG
static void rbtreenode_toustring(RBTree *tree, RBTreeNode *node, UString *ustr, toUString toustring, int indent) /* NONNULL(2) */
{
    if (node != tree->nil) {
        toustring(ustr, node->key, node->value);
        u_fprintf(ustderr, RED == node->color ? "%*s" RED("%S") "\n" : "%*s%S\n", indent * 4, "", ustr->ptr);
        rbtreenode_toustring(tree, node->left, ustr, toustring, indent + 1);
        rbtreenode_toustring(tree, node->right, ustr, toustring, indent + 1);
    }
}

void rbtree_debug(RBTree *tree, toUString toustring) /* NONNULL() */
{
    UString *ustr;

    if (tree->root != tree->nil) {
        ustr = ustring_new();
        rbtreenode_toustring(tree, tree->root, ustr, toustring, 0);
        ustring_destroy(ustr);
    }
}
#endif /* DEBUG */

static void _rbtree_traverse_pre_order(RBTree *tree, RBTreeNode *node, func_apply_t trav_func) /* NONNULL(2) */
{
    if (node != tree->nil) {
        trav_func(node->key, node->value);
        _rbtree_traverse_pre_order(tree, node->left, trav_func);
        _rbtree_traverse_pre_order(tree, node->right, trav_func);
    }
}

static void _rbtree_traverse_post_order(RBTree *tree, RBTreeNode *node, func_apply_t trav_func) /* NONNULL(2) */
{
    if (node != tree->nil) {
        _rbtree_traverse_post_order(tree, node->left, trav_func);
        _rbtree_traverse_post_order(tree, node->right, trav_func);
        trav_func(node->key, node->value);
    }
}

void rbtree_traverse(RBTree *tree, traverse_mode_t mode, func_apply_t trav_func) /* NONNULL() */
{
    require_else_return(tree != NULL);
    require_else_return(trav_func != NULL);

    switch (mode) {
        case IN_ORDER:
            _rbtree_traverse_in_order(tree, tree->root, trav_func);
            break;
        case PRE_ORDER:
            _rbtree_traverse_pre_order(tree, tree->root, trav_func);
            break;
        case POST_ORDER:
            _rbtree_traverse_post_order(tree, tree->root, trav_func);
            break;
    }
}

// #define TEST 1
#ifdef TEST
# define POINTER_TO_INT(p)   ((int) (long) (p))
# define INT_TO_POINTER(i)   ((void *) (long) (i))

int int_cmp(const void *k1, const void *k2)
{
    return (POINTER_TO_INT(k1) - POINTER_TO_INT(k2));
}

void int_print(const void *k, void *UNUSED(v))
{
    printf("%d\n", POINTER_TO_INT(k));
}

static void _rbtree_node_print(RBTree *tree, RBTreeNode *node, int indent)
{
    if (node != tree->nil) {
        printf("%*s<%d> (%s)\n", indent * 4, "", POINTER_TO_INT(node->key), node->color == BLACK ? "noir" : "rouge");
        _rbtree_node_print(tree, node->left, indent + 1);
        _rbtree_node_print(tree, node->right, indent + 1);
        printf("%*s</%d>\n", indent * 4, "", POINTER_TO_INT(node->key));
    }
}

static void rbtree_print(RBTree *tree) /* NONNULL() */
{
    require_else_return(tree != NULL);

    _rbtree_node_print(tree, tree->root, 0);
}

int rbtree_main(void)
{
    size_t i;
    void *x;
    RBTree *tree;
    //int values[] = { 27, 25, 22, 17, 10, 15, 13, 11, 8, 6, 1 };
    //int values[] = { 13, 8, 17, 1, 11, 15, 25, 6, 22, 27 };
    /* http://wn.com/Binary_search_tree_insertion_demo */
    int values[] = { 10, 85, 15, 70, 20, 60, 30, 50, 65, 80, 90, 40, 5, 55 };

    tree = rbtree_new(int_cmp, NODUP, NODUP, NULL, NULL);
    for (i = 0; i < ARRAY_SIZE(values); i++) {
        printf("Try inserting: %d\n", values[i]);
        rbtree_insert(tree, INT_TO_POINTER(values[i]), NULL, RBTREE_INSERT_ON_DUP_KEY_PRESERVE, NULL);
    }
    rbtree_print(tree);
    printf("\n----------\n\n");
    rbtree_traverse(tree, IN_ORDER, int_print);

    /*printf("Remove = %d\n", rbtree_remove(tree, INT_TO_POINTER(10)));

    rbtree_print(tree);
    printf("\n----------\n\n");
    rbtree_traverse(tree, IN_ORDER, int_print);*/

    printf("15 : %d\n", rbtree_exists(tree, INT_TO_POINTER(15)));
    printf("5 : %d\n", rbtree_exists(tree, INT_TO_POINTER(5)));
    rbtree_min(tree, &x, NULL);
    printf("MIN = %d\n", POINTER_TO_INT(x));
    rbtree_max(tree, &x, NULL);
    printf("MAX = %d\n", POINTER_TO_INT(x));

    rbtree_remove(tree, INT_TO_POINTER(5));

    rbtree_min(tree, &x, NULL);
    printf("MIN = %d\n", POINTER_TO_INT(x));
    rbtree_max(tree, &x, NULL);
    printf("MAX = %d\n", POINTER_TO_INT(x));

    rbtree_destroy(tree);

    return EXIT_SUCCESS;
}
#endif /* TEST */
