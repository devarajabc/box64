#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

#ifdef RBTREE_TEST
#define rbtreeMalloc malloc
#define rbtreeFree free
#else
#include "custommem.h"
#include "debug.h"
#include "rbtree.h"
#include "rbtree_helper.h"

#if 0
#define rbtreeMalloc box_malloc
#define rbtreeFree box_free
#else
#define rbtreeMalloc customMalloc
#define rbtreeFree customFree
#endif
#endif
/*Compares two nodes and returns true if node A is strictly less than node B,
 * based on the tree's defined sorting criteria. Returns false otherwise.
 */
bool compare (const rb_node_t *a, const rb_node_t *b)
{
    return get_start(a) < get_start(b) ? true : false;   
}
//Helper functions
rb_t* rbtree_init(const char* name) {
    rb_t* tree = rbtreeMalloc(sizeof(rb_t));
    tree->root = NULL;
    tree->cmp_func = &compare;
    tree->name = name?name:"(rbtree)";
    return tree;
}
static inline rb_node_t *get_child(rb_node_t *n, rb_side_t side)
{
    if (side == RB_RIGHT)
        return n->children[RB_RIGHT];

    /* Mask out the least significant bit (LSB) to retrieve the actual left
     * child pointer.
     *
     * The LSB of the left child pointer is used to store metadata (e.g., the
     * color bit). By masking out the LSB with & ~1UL, the function retrieves
     * the actual pointer value without the metadata bit, ensuring the correct
     * child node is returned.
     */
    uintptr_t l = (uintptr_t) n->children[RB_LEFT];
    l &= ~1UL;
    return (rb_node_t *) l;
}
static inline uintptr_t get_start(rb_node_t *n)
{
    rbnode *Node = container_of(n, rbnode, node);
    return Node->start;
}

static inline uintptr_t get_end(rb_node_t *n)
{
    rbnode *Node = container_of(n, rbnode, node);
    return Node->end;
}

static inline uint32_t get_data(rb_node_t *n)
{
    rbnode *Node = container_of(n, rbnode, node);
    return Node->data;
}

rb_node_t *__rb_get_local_minmax(rb_node_t *n, rb_side_t side)
{
    for ( ; n && get_child(n, side); n = get_child(n, side))
        ;
    return n;
}

static inline rb_node_t *rb_get_local_min(rb_node_t *n)
{
    return __rb_get_local_minmax(n, RB_LEFT);
}

static inline rb_node_t *rb_get_local_max(rb_node_t *n)
{
    return __rb_get_local_minmax(n, RB_RIGHT);
}

void update_end(rb_node_t *n, uintptr_t new_end)
{
    rbnode *Node = container_of(n, rbnode, node);
    Node->end = new_end;
}

void update_start(rb_node_t *n, uintptr_t new_start)
{
    rbnode *Node = container_of(n, rbnode, node);
    Node->start = new_start;
}

void update_data(rb_node_t *n, uint32_t new_data)
{
    rbnode *Node = container_of(n, rbnode, node);
    Node->data = new_data;
}

static rb_node_t *pred_node(rb_node_t *node) {
    if (!node) return NULL;
    if (node->children[RB_LEFT]) {//node->left
        //node = node->left;
        node = get_child(node, RB_LEFT);
        return rb_get_local_max(node);
    }
    // else {
    //    while (node->parent && node->meta & IS_LEFT) node = node->parent;
    //    return node->parent;
    //}
}

static rb_node_t *succ_node(rb_node_t *node) {
    if (!node) return NULL;
    if (node->children[RB_RIGHT]) {//node->right
        //node = node->right;
        node = get_child(node, RB_RIGHT);
        return rb_get_local_min(node);
    } 
    //else {
    // while (node->parent && !(node->meta & IS_LEFT)) node = node->parent;
    //return node->parent;
    //}
}

static rb_node_t *find_addr(rb_t *tree, uintptr_t addr) {
    rb_node_t *node = tree->root;
    while (node) {
        if ((get_start(node) <= addr) && (get_end(node) > addr)) return node;
        if (addr < get_start(node)) node = get_child(node, RB_LEFT);
        else node = get_child(node, RB_RIGHT);
    }
    return NULL;
}

uintptr_t rb_get_righter(rb_t* tree){
    dynarec_log(LOG_DEBUG, "rb_get_righter(%s);\n", tree->name);
    if (!tree->root) return 0;
    rb_node_t* node = rb_get_max(tree);
    return get_start(node);
}
// Memory range management

rb_t* rbtree_init(const char* name) {
    rb_t* tree = rbtreeMalloc(sizeof(rb_t));
    tree->root = NULL;
    tree->cmp_func = &compare;
    tree->name = name?name:"(rbtree)";
    return tree;
}

static inline void delete_rbnode(rbnode *root) {
    if (!root) return;
    delete_rbnode(root->left);
    delete_rbnode(root->right);
    rbtreeFree(root);
}

void rbtree_delete(rbtree_t *tree) {
    delete_rbnode(tree->root);
    rbtreeFree(tree);
}

static int remove_node(rb_t *tree, rb_node_t *node){
    rb_remove(tree, node);
}
static int add_range(rb_t *tree, uintptr_t start, uintptr_t end, uint64_t data){
    rbnode *node = rbtreeMalloc(sizeof(*node));
    if (!node) return -1;
    node->start = start;
    node->end = end;
    node->data = data;
    rb_insert(tree, node);
}


int rb_set(rb_t *tree, uintptr_t start, uintptr_t end, uint32_t data) {
dynarec_log(LOG_DEBUG, "set %s: 0x%lx, 0x%lx, 0x%x\n", tree->name, start, end, data);
    if (!tree->root) {
        return add_range(tree, start, end, data);
    }
    
    rb_node_t *node = tree->root, *prev = NULL, *last = NULL;
     while (node) {
        if (get_start(node) < start) {//if  node->start < start
            prev = node;
            node = get_child(node, RB_RIGHT); //node->right
        } else if (get_start(node) == start) { // if node->start == start
            if (node->children[RB_LEFT]) {// if node->left
                prev = rb_get_local_min(node->children[RB_LEFT]);//get the local min of the left subtree
            }
            if (node->children[RB_RIGHT]) {
                last = rb_get_local_max(node->children[RB_RIGHT]);//get the local max of the right subtree
            }
            break;
        } else {
            last = node;
            node = get_child(node, RB_LEFT);//node->left
        }
    }
    //rb_node_t *node = tree->root, *prev = NULL, *last = NULL;
    // Handle the "start" address => get three node : prev, node and last
    // prev is the largest node starting strictly before start, or NULL if there is none
    // node is the node starting exactly at start, or NULL if there is none
    // last is the smallest node starting strictly after start, or NULL if there is none
    // Note that prev may contain start
    if (prev && (get_end(prev) >= start) && (get_data(prev) == data)) {  
        if (end <= get_end(prev)) return 0; // Nothing to do!
        if (node && (get_end(node) > end)) {
            update_start(node, end); //node->start = end
            update_end(prev, end); //prev->end = end
            return 0;
        } else if (node && (get_end(node) == end)) {
            remove_node(tree, node);
            update_end(prev, end);//prev->end = end;
            return 0;
        } else if (node) { 
            remove_node(tree, node); 
        }
        while (last && (get_start(last)< end) && (get_end(last) <= end)) {//last && (last->start < end) && (last->end <= end)
            // Remove the entire node
            node = last;
            last = succ_node(last);
            remove_node(tree, node);
        }
        if (last && (get_start(last) <= end) && (get_data(last) == data)) {
            // Merge node and last
            //prev->end = last->end;
            update_end(prev, get_end(last));
            remove_node(tree, last);
            return 0;
        }
        if (last && (get_start(last) < end)) update_start(last, end);//last->start = end;
        //prev->end = end;
        update_end(prev, end);
        return 0;
    } else if (prev && (get_end(prev) > start)) {
        if (get_end(prev)> end) {
            // Split in three
            // Note that here, succ(prev) = last and node = NULL
            int ret;
            ret = add_range(tree, end, get_end(prev),get_data(prev));
            ret = ret ? ret : add_range(tree, start, end, data);
            update_end(prev, start);
            return ret;
        }
        // Cut prev and continue
        //prev->end = start;
        update_end(prev, start);
    }

    if (node) {
        // Change node
        if (get_end(node) >= end) {
            if (get_data(node) == data) return 0; // Nothing to do!
            // Cut node
            if (get_end(node) > end) {
                int ret = add_range(tree, end, get_end(node),get_data(node));
                update_end(node, end);//node->end = end;
                update_data(node, data);//node->data = data;
                return ret;
            }
            // Fallthrough
        }
        
        // Overwrite and extend node
        while (last && (get_start(last) < end) && (get_end(last) <= end)) {
            // Remove the entire node
            prev = last;
            last = succ_node(last);
            remove_node(tree, prev);
        }
        if (last && (get_start(last) <= end) && (get_data(last) == data)) {
            // Merge node and last
            remove_node(tree, node);
            update_start(last, start);//last->start = start;
            return 0;
        }
        if (last && (get_start(last) < end)) update_start(last, end);//last->start = end
        if (get_end(node) < end) update_end(node,end);//node->end = end
        update_data(node, data);//node->data = data;
        return 0;
    }
   

    while (last && (get_start(last) < end) && (get_end(last) <= end)) {
            // Remove the entire node
            prev = last;
            last = succ_node(last);
            remove_node(tree, prev);
    }
    if (!last) {
        // Add a new node next to prev, the largest node of the tree
        // It exists since the tree is nonempty
        return add_range(tree, start, end, data);
    }
    if (last && (get_start(last) <= end) && (get_data(last) == data)) {
        // Extend
        update_start(last, start);//last->start = start;
        return 0;
    } else if (get_start(last) < end) {
        // Cut
        update_start(last, end);//last->start = end
    }
    // Probably 'last->left ? prev : last' is enough
    return add_range(tree, start, end, data);
}

int rb_unset(rbtree_t *tree, uintptr_t start, uintptr_t end) {
// printf("rb_unset( "); rbtree_print(tree); printf(" , 0x%lx, 0x%lx);\n", start, end); fflush(stdout);
dynarec_log(LOG_DEBUG, "unset: %s 0x%lx, 0x%lx);\n", tree->name, start, end);
    if (!tree->root) return 0;

    rbnode *node = tree->root, *prev = NULL, *next = NULL;
    while (node) {
        if (node->start < start) {
            prev = node;
            node = node->right;
        } else if (node->start == start) {
            if (node->left) {
                prev = node->left;
                while (prev->right) prev = prev->right;
            }
            if (node->right) {
                next = node->right;
                while (next->left) next = next->left;
            }
            break;
        } else {
            next = node;
            node = node->left;
        }
    }

    if (node) {
        if (node->end > end) {
            node->start = end;
            return 0;
        } else if (node->end == end) {
            remove_node(tree, node);
            return 0;
        } else {
            remove_node(tree, node);
        }
    } else if (prev && (prev->end > start)) {
        if (prev->end > end) {
            // Split prev
            int ret = add_range_next_to(tree, prev->right ? next : prev, end, prev->end, prev->data);
            prev->end = start;
            return ret;
        } else if (prev->end == end) {
            prev->end = start;
            return 0;
        } // else fallthrough
    }
    while (next && (next->start < end) && (next->end <= end)) {
        // Remove the entire node
        node = next;
        next = succ_node(next);
        remove_node(tree, node);
    }
    if (next && (next->start < end)) {
        // next->end > end: cut the node
        next->start = end;
    }
    return 0;
}



