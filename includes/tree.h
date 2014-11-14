
#ifndef TREE_H
#define TREE_H

#ifdef __DEBUG__
#define HEAP_MAGIC 0xab9fd283
#endif

struct tree_node
{
#ifdef __DEBUG__
    int magic;
#endif
    long long priority;
    struct tree_node *parent;
    struct tree_node *children[2];
    struct list_head list;
    void *ctxt;
};

#endif // !TREE_H
