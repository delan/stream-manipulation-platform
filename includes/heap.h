
#ifndef HEAP_H
#define HEAP_H

#include "class.h"
#include "list.h"
#include "tree.h"

DECLARE_CLASS(Heap);

#define CLASS_NAME(a,b) a## Heap ##b
CLASS(Object)
    void METHOD(put, struct tree_node *heap, long long priority);
    struct tree_node *METHOD(pop);
    struct tree_node *METHOD(peek);
    void METHOD(remove, struct tree_node *heap);

    struct list_head node_list;

    int (*comparator)(long long, long long);
END_CLASS
#undef CLASS_NAME // Heap

#endif
