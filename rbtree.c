#include "common.h"
#include "rbtree.h"

typedef void *(*func_hash_t)(const void *, const void *);

static int ucol_key_cmp(const void *k1, const void *k2)
{
    return strcmp((const char *) k1, (const char *) k2);
}

static int ucol_key_cmp_r(const void *k1, const void *k2)
{
    return strcmp((const char *) k2, (const char *) k1);
}

static void *ucol_compute_key(const void *value, const void *data)
{
    uint8_t *key;
    int32_t key_len;
    const UString *ustr;
    const UCollator *ucol;

    ustr = (const UString *) value;
    ucol = (const UCollator *) data;
    key_len = ucol_getSortKey(ucol, ustr->ptr, ustr->len, NULL, 0);
    key = mem_new_n(*key, key_len + 1);
    ensure(key_len == ucol_getSortKey(ucol, ustr->ptr, ustr->len, key, key_len));
    key[key_len] = 0;

    return key;
}

typedef enum {
    BLACK = 0,
    RED   = 1
} RBTColor;

struct _RBTreeNode
{
    void *key;
    void *hash;
    void *value;
    struct _RBTreeNode *left;
    struct _RBTreeNode *right;
    struct _RBTreeNode *parent;
    RBTColor color;
};

/*static const RBTreeNode nil = {
    NULL,
    NULL,
    &nil,
    &nil,
    &nil,
    BLACK
};*/

struct _RBTree
{
    RBTreeNode *root;
    RBTreeNode *last;
    RBTreeNode *first;
    func_cmp_t cmp_func;
    void *priv_data_hash;
    func_hash_t hash_func;
    func_dtor_t key_dtor_func;
    func_dtor_t value_dtor_func;
};

static RBTreeNode *_rbtreenode_new(void *key, void *data)
{
    RBTreeNode *node;

    node = mem_new(*node);
    node->left = NULL;
    node->right = NULL;
    node->parent = NULL;
    node->color = RED;
    node->key = key;
    node->hash = NULL;
    node->value = data;

    return node;
}

RBTree *rbtree_new(func_cmp_t cmp_func, func_dtor_t key_dtor_func, func_dtor_t value_dtor_func) /* NONNULL(1) WARN_UNUSED_RESULT */
{
    RBTree *tree;

    require_else_return_null(NULL != cmp_func);

    tree = mem_new(*tree);
    tree->priv_data_hash = NULL;
    tree->root = tree->first = tree->last = NULL;
    tree->hash_func = NULL;
    tree->cmp_func = cmp_func;
    tree->key_dtor_func = key_dtor_func;
    tree->value_dtor_func = value_dtor_func;

    return tree;
}

RBTree *rbtree_hashed_new(func_cmp_t cmp_func, func_hash_t hash_func, func_dtor_t key_dtor_func, func_dtor_t value_dtor_func) /* NONNULL(1,2) WARN_UNUSED_RESULT */
{
    RBTree *tree;

    require_else_return_null(NULL != cmp_func);
    require_else_return_null(NULL != hash_func);

    tree = rbtree_new(cmp_func, key_dtor_func, value_dtor_func);
    tree->hash_func = hash_func;

    return tree;
}

RBTree *rbtree_collated_new(UCollator *ucol, func_dtor_t key_dtor_func, func_dtor_t value_dtor_func, int inversed) /* NONNULL(1) WARN_UNUSED_RESULT */
{
    RBTree *tree;

    require_else_return_null(NULL != ucol);

    tree = rbtree_new(inversed ? ucol_key_cmp_r : ucol_key_cmp, key_dtor_func, value_dtor_func);
    tree->priv_data_hash = ucol;
    tree->hash_func = ucol_compute_key;

    return tree;
}

int rbtree_empty(RBTree *tree) /* NONNULL() */
{
    require_else_return_zero(NULL != tree);

    return NULL == tree->root;
}

static void _rbtreenode_rotate_left(RBTree *tree, RBTreeNode *node) /* NONNULL() */
{
    RBTreeNode *p;

    require_else_return(NULL != tree);
    require_else_return(NULL != node);

    p = node->right;
    node->right = p->left;
    if (NULL != p->left) {
        p->left->parent = node;
    }
    p->parent = node->parent;
    if (NULL == node->parent) {
        tree->root = p;
    } else if (node == node->parent->left) {
        node->parent->left = p;
    } else {
        node->parent->right = p;
    }
    p->left = node;
    node->parent = p;
}

static void _rbtreenode_rotate_right(RBTree *tree, RBTreeNode *node) /* NONNULL() */
{
    RBTreeNode *p;

    require_else_return(NULL != tree);
    require_else_return(NULL != node);

    p = node->left;
    node->left = p->right;
    if (NULL != p->right) {
        p->right->parent = node;
    }
    p->parent = node->parent;
    if (NULL == node->parent) {
        tree->root = p;
    } else if (node == node->parent->right) {
        node->parent->right = p;
    } else {
        node->parent->left = p;
    }
    p->right = node;
    node->parent = p;
}

enum {
    RIGHT,
    LEFT
};

int rbtree_lookup_node(RBTree *tree, void *key, RBTreeNode **parent, int *side, void **res) /* NONNULL(1, 3, 4, 5) */
{
    void *hash;
    RBTreeNode *node = tree->root;

    require_else_return_zero(tree != NULL);
    require_else_return_zero(parent != NULL);
    require_else_return_zero(side != NULL);
    require_else_return_zero(res != NULL);

    *parent = NULL;
    *side = RIGHT;

    if (NULL != tree->hash_func) {
        hash = tree->hash_func(key, tree->priv_data_hash);
    }
    while (NULL != node) {
        int cmp;

        if (NULL != tree->hash_func) {
            cmp = tree->cmp_func(hash, node->hash);
        } else {
            cmp = tree->cmp_func(key, node->key);
        }
        if (0 == cmp) {
            *res = node->value;
            return 1;
        }
        *parent = node;
        if (cmp > 0) {
            *side = LEFT;
            node = node->left;
        } else {
            *side = RIGHT;
            node = node->right;
        }
    }

    *res = _rbtreenode_new(key, NULL);
    ((RBTreeNode *)(*res))->hash = hash;
    return 0;
}

int rbtree_insert_node(RBTree *tree, RBTreeNode *new, void *value, RBTreeNode *parent, int side) /* NONNULL(1, 2) */
{
    require_else_return_zero(NULL != tree);
    require_else_return_zero(NULL != new);

    if (NULL != tree->hash_func && NULL == new->hash) {
        new->hash = tree->hash_func(new->key, tree->priv_data_hash);
    }
    new->parent = parent;
    new->value = value;
    if (NULL == parent) {
        tree->first = tree->last = tree->root = new;
    } else if (LEFT == side) {
        parent->left = new;
        if (parent == tree->first) {
            tree->first = new;
        }
    } else {
        parent->right = new;
        if (parent == tree->last) {
            tree->last = new;
        }
    }

    while (NULL != (parent = new->parent) && RED == parent->color) {
        RBTreeNode *grandparent;

        grandparent = parent->parent;
        if (parent == grandparent->left) {
            RBTreeNode *uncle;

            uncle = grandparent->right;
            if (NULL != uncle && RED == uncle->color) {
                parent->color = BLACK;
                uncle->color = BLACK;
                grandparent->color = RED;
                new = grandparent;
            } else {
                if (new == parent->right) {
                    _rbtreenode_rotate_left(tree, parent);
                    new = parent;
                    parent = new->parent;
                }
                parent->color = BLACK;
                grandparent->color = RED;
                _rbtreenode_rotate_right(tree, grandparent);
            }
        } else {
            RBTreeNode *uncle;

            uncle = grandparent->left;
            if (NULL != uncle && RED == uncle->color) {
                parent->color = BLACK;
                uncle->color = BLACK;
                grandparent->color = RED;
                new = grandparent;
            } else {
                if (new == parent->left) {
                    _rbtreenode_rotate_right(tree, parent);
                    new = parent;
                    parent = new->parent;
                }
                parent->color = BLACK;
                grandparent->color = RED;
                _rbtreenode_rotate_left(tree, grandparent);
            }
        }
    }
    tree->root->color = BLACK;

    return 1;
}

int rbtree_insert(RBTree *tree, void *key, void *value) /* NONNULL(1) */
{
    int side;
    void *new;
    RBTreeNode *parent;

    require_else_return_zero(NULL != tree);

    if (0 == rbtree_lookup_node(tree, key, &parent, &side, &new)) {
        return rbtree_insert_node(tree, (RBTreeNode *) new, value, parent, side);
    } else {
        return 0;
    }
}

static void _rbtree_destroy(RBTree *tree, RBTreeNode *node) /* NONNULL(1) */
{
    require_else_return(NULL != tree);

    if (NULL != node) {
        _rbtree_destroy(tree, node->right);
        _rbtree_destroy(tree, node->left);
        if (NULL != tree->value_dtor_func) {
            tree->value_dtor_func(node->value);
        }
        if (NULL != tree->key_dtor_func) {
            tree->key_dtor_func(node->key);
        }
        if (NULL != tree->hash_func) {
            free(node->hash);
        }
        free(node);
    }
}

void rbtree_clear(RBTree *tree) /* NONNULL() */
{
    require_else_return(NULL != tree);

    _rbtree_destroy(tree, tree->root);
    tree->root = NULL;
}

void rbtree_destroy(RBTree *tree) /* NONNULL() */
{
    require_else_return(NULL != tree);

    _rbtree_destroy(tree, tree->root);
    free(tree);
}

static void _rbtree_traverse_in_order(RBTreeNode *node, func_apply_t trav_func) /* NONNULL(2) */
{
    if (NULL != node) {
        _rbtree_traverse_in_order(node->left, trav_func);
        trav_func(node->key, node->value);
        _rbtree_traverse_in_order(node->right, trav_func);
    }
}

static void _rbtree_traverse_pre_order(RBTreeNode *node, func_apply_t trav_func) /* NONNULL(2) */
{
    if (NULL != node) {
        trav_func(node->key, node->value);
        _rbtree_traverse_pre_order(node->left, trav_func);
        _rbtree_traverse_pre_order(node->right, trav_func);
    }
}

static void _rbtree_traverse_post_order(RBTreeNode *node, func_apply_t trav_func) /* NONNULL(2) */
{
    if (NULL != node) {
        _rbtree_traverse_post_order(node->left, trav_func);
        _rbtree_traverse_post_order(node->right, trav_func);
        trav_func(node->key, node->value);
    }
}

void rbtree_traverse(RBTree *tree, traverse_mode_t mode, func_apply_t trav_func) /* NONNULL() */
{
    require_else_return(tree != NULL);
    require_else_return(trav_func != NULL);

    switch (mode) {
        case IN_ORDER:
            _rbtree_traverse_in_order(tree->root, trav_func);
            break;
        case PRE_ORDER:
            _rbtree_traverse_pre_order(tree->root, trav_func);
            break;
        case POST_ORDER:
            _rbtree_traverse_post_order(tree->root, trav_func);
            break;
    }
}

#if 0
static void _rbtree_traverse_in_order_with_argument(RBTreeNode *node, func_apply_arg_t trav_func, void *arg) /* NONNULL(2) */
{
    if (NULL != node) {
        _Rbtree_traverse_in_order_with_argument(node->left, trav_func, arg);
        trav_func(bnode->data, arg);
        _Rbtree_traverse_in_order_with_argument(node->right, trav_func, arg);
    }
}

static void _rbtree_traverse_pre_order_with_argument(RBTreeNode *node, func_apply_arg_t trav_func, void *arg) /* NONNULL(2) */
{
    if (NULL != node) {
        trav_func(node->data, arg);
        _rbtree_traverse_pre_order_with_argument(node->left, trav_func, arg);
        _rbtree_traverse_pre_order_with_argument(node->right, trav_func, arg);
    }
}

static void _rbtree_traverse_post_order_with_argument(RBTreeNode *node, func_apply_arg_t trav_func, void *arg) /* NONNULL(2) */
{
    if (NULL != node) {
        _btree_traverse_post_order_with_argument(node->left, trav_func, arg);
        _btree_traverse_post_order_with_argument(node->right, trav_func, arg);
        trav_func(node->data, arg);
    }
}

void rbtree_traverse_with_argument(RBTree *tree, traverse_mode_t mode, func_apply_arg_t trav_func, void *arg)
{
    require_else_return(tree != NULL);
    require_else_return(trav_func != NULL);

    switch (mode) {
        case IN_ORDER:
            _rbtree_traverse_in_order_with_argument(tree->root, trav_func, arg);
            break;
        case PRE_ORDER:
            _rbtree_traverse_pre_order_with_argument(tree->root, trav_func, arg);
            break;
        case POST_ORDER:
            _rbtree_traverse_post_order_with_argument(tree->root, trav_func, arg);
            break;
    }
}
#endif

static RBTreeNode *_rbtreenode_lookup(RBTree *tree, void *key) /* NONNULL(1) */
{
    RBTreeNode *node;

    node = tree->root;
    while (NULL != node) {
        int cmp;

        if (NULL != tree->hash_func) {
            cmp = tree->cmp_func(key, node->hash);
        } else {
            cmp = tree->cmp_func(key, node->key);
        }
        if (0 == cmp) {
            return node;
        } else if (cmp < 0) {
            node = node->left;
        } else /*if (cmp > 0)*/ {
            node = node->right;
        }
    }

    return NULL;
}

int rbtree_lookup(RBTree *tree, void *key, void **value) /* NONNULL(1) */
{
    RBTreeNode *node;

    require_else_return_false(NULL != tree);

    if (NULL != tree->hash_func) {
        key = tree->hash_func(key, tree->priv_data_hash);
    }
    if (NULL == (node = _rbtreenode_lookup(tree, key))) {
        return 0;
    } else {
        if (NULL != value) {
            *value = node->value;
        }
        return 1;
    }
}

int rbtree_replace(RBTree *tree, void *key, void *value, int call_dtor)
{
    RBTreeNode *node;

    require_else_return_false(NULL != tree);

    if (NULL != tree->hash_func) {
        key = tree->hash_func(key, tree->priv_data_hash);
    }
    if (NULL == (node = _rbtreenode_lookup(tree, key))) {
        return 0;
    } else {
        if (call_dtor) {
            tree->value_dtor_func(node->value);
        }
        node->value = value;
        return 1;
    }
}

int rbtree_remove(RBTree *tree, void *key) /* NONNULL(1) */
{
    RBTreeNode *node;

    require_else_return_false(NULL != tree);

    if (NULL != tree->hash_func) {
        key = tree->hash_func(key, tree->priv_data_hash);
    }
    if (NULL == (node = _rbtreenode_lookup(tree, key))) {
        return 0;
    } else {
        RBTreeNode *next, *parent, *left, *right;
        RBTColor color;

        left = node->left;
        right = node->right;
        parent = node->parent;
        if (NULL == left) {
            next = right;
        } else if (NULL == right) {
            next = left;
        } else {
            for (next = right; NULL != next->left; next = next->left)
                ;
        }
        if (NULL != parent) {
            if (node == parent->left) {
                parent->left = next;
            } else {
                parent->right = next;
            }
        } else {
            tree->root = next;
        }
        if (NULL != left && NULL != right) {
            color = next->color;
            next->color = node->color;
            next->left = left;
            left->parent = next;
            if (next != right) {
                parent = next->parent;
                next->parent = node->parent;
                node = next->right;
                parent->left = node;
                next->right = right;
                right->parent = next;
            } else {
                next->parent = parent;
                parent = next;
                node = next->right;
            }
        } else {
            color = node->color;
            node = next;
        }
        if (NULL != node) {
            node->parent = parent;
        }
        if (RED == color) {
            return 1;
        }
        if (NULL != node && RED == color) {
            node->color = BLACK;
            return 1;
        }
        do {
            if (node == tree->root) {
                break;
            }
            if (node == parent->left) {
                RBTreeNode *sibling;

                sibling = parent->right;
                if (RED == sibling->color) {
                    sibling->color = BLACK;
                    parent->color = RED;
                    _rbtreenode_rotate_left(tree, parent);
                    sibling = parent->right;
                }
                if ((NULL == sibling->left || BLACK == sibling->left->color) && (NULL == sibling->right || BLACK == sibling->right->color)) {
                    sibling->color = RED;
                    node = parent;
                    parent = parent->parent;
                    continue;
                }
                if (NULL == sibling->right || BLACK == sibling->right->color) {
                    sibling->left->color = BLACK;
                    sibling->color = RED;
                    _rbtreenode_rotate_right(tree, sibling);
                    sibling = parent->right;
                }
                sibling->color = parent->color;
                parent->color = BLACK;
                sibling->right->color = BLACK;
                _rbtreenode_rotate_left(tree, parent);
                node = tree->root;
                break;
            } else {
                RBTreeNode *sibling;

                sibling = parent->left;
                if (RED == sibling->color) {
                    sibling->color = BLACK;
                    parent->color = RED;
                    _rbtreenode_rotate_right(tree, parent);
                    sibling = parent->left;
                }
                if ((NULL == sibling->left || BLACK == sibling->left->color) && (NULL == sibling->right || BLACK == sibling->right->color)) {
                    sibling->color = RED;
                    node = parent;
                    parent = parent->parent;
                    continue;
                }
                if (NULL == sibling->left || BLACK == sibling->left->color) {
                    sibling->right->color = BLACK;
                    sibling->color = RED;
                    _rbtreenode_rotate_left(tree, sibling);
                    sibling = parent->left;
                }
                sibling->color = parent->color;
                parent->color = BLACK;
                sibling->left->color = BLACK;
                _rbtreenode_rotate_right(tree, parent);
                node = tree->root;
                break;
            }
        } while (BLACK == node->color);
        if (NULL != node) {
            node->color = BLACK;
        }

        return 1;
    }
}

int rbtree_min(RBTree *tree, void **key, void **value) /* NONNULL(1) */
{
    RBTreeNode *node;

    require_else_return_zero(NULL != tree);

    node = tree->root;
    if (NULL != node) {
        while (NULL != node->left) {
            node = node->left;
        }
        if (NULL != key) {
            *key = node->key;
        }
        if (NULL != value) {
            *value = node->value;
        }
    }

    return NULL != node;
    /*if (NULL != tree->first) {
        if (NULL != key) {
            *key = tree->first->key;
        }
        if (NULL != value) {
            *value = tree->first->value;
        }
    }

    return NULL != tree->first;*/
}

int rbtree_max(RBTree *tree, void **key, void **value) /* NONNULL(1) */
{
    RBTreeNode *node;

    require_else_return_zero(NULL != tree);

    node = tree->root;
    if (NULL != node) {
        while (NULL != node->right) {
            node = node->right;
        }
        if (NULL != key) {
            *key = node->key;
        }
        if (NULL != value) {
            *value = node->value;
        }
    }

    return NULL != node;
    /*if (NULL != tree->last) {
        if (NULL != key) {
            *key = tree->last->key;
        }
        if (NULL != value) {
            *value = tree->last->value;
        }
    }

    return NULL != tree->last;*/
}

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

static void _rbtree_node_print(RBTreeNode *node, int indent)
{
    if (NULL != node) {
        printf("%*s<%d> (%s)\n", indent * 4, "", POINTER_TO_INT(node->key), node->color == BLACK ? "noir" : "rouge");
        _rbtree_node_print(node->left, indent + 1);
        _rbtree_node_print(node->right, indent + 1);
        printf("%*s</%d>\n", indent * 4, "", POINTER_TO_INT(node->key));
    }
}

static void rbtree_print(RBTree *tree) /* NONNULL() */
{
    require_else_return(tree != NULL);

    _rbtree_node_print(tree->root, 0);
}

INITIALIZER_P(rbtree_test)
{
    size_t i;
    void *x;
    RBTree *tree;
    //int values[] = { 27, 25, 22, 17, 10, 15, 13, 11, 8, 6, 1 };
    //int values[] = { 13, 8, 17, 1, 11, 15, 25, 6, 22, 27 };
    /* http://wn.com/Binary_search_tree_insertion_demo */
    int values[] = { 10, 85, 15, 70, 20, 60, 30, 50, 65, 80, 90, 40, 5, 55 };

    tree = rbtree_new(int_cmp, NULL, NULL);
    for (i = 0; i < ARRAY_SIZE(values); i++) {
        printf("Try inserting: %d\n", values[i]);
        rbtree_insert(tree, INT_TO_POINTER(values[i]), NULL);
    }
    rbtree_print(tree);
    printf("\n----------\n\n");
    rbtree_traverse(tree, IN_ORDER, int_print);

    /*printf("Remove = %d\n", rbtree_remove(tree, INT_TO_POINTER(10)));

    rbtree_print(tree);
    printf("\n----------\n\n");
    rbtree_traverse(tree, IN_ORDER, int_print);*/

    printf("15 : %d\n", rbtree_lookup(tree, INT_TO_POINTER(15), NULL));
    printf("5 : %d\n", rbtree_lookup(tree, INT_TO_POINTER(5), NULL));
    rbtree_min(tree, &x, NULL);
    printf("MIN = %d\n", POINTER_TO_INT(x));
    rbtree_max(tree, &x, NULL);
    printf("MAX = %d\n", POINTER_TO_INT(x));

    rbtree_destroy(tree);
}
#endif /* TEST */
