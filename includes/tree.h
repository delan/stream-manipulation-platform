
#ifndef TREE_H
#define TREE_H

struct tree_node
{
    long long priority;
    struct tree_node *parent;
    struct tree_node *children[2];
    struct list_head list;
    void *ctxt;
};

#endif // !TREE_H
