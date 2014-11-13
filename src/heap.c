
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "class.h"
#include "heap.h"
#include "debug.h"

#define CLASS_NAME(a,b) a## Heap ##b
static inline void force_link(struct tree_node *h, struct tree_node *prev)
{
    if(h->parent)
    {
        if(h->parent->children[0] == prev)
            h->parent->children[0] = h;
        if(h->parent->children[1] == prev)
            h->parent->children[1] = h;
    }
    if(h->children[0])
        h->children[0]->parent = h;
    if(h->children[1])
        h->children[1]->parent = h;
}

static inline void swap(struct tree_node *h1, struct tree_node *h2)
{
#define swp(a, b, c, field)   \
    if((a) != (b)->field)   \
        (a)->field = (b)->field;    \
    else    \
        (a)->field = c  \

    struct tree_node tmp;
    tmp.parent = h1->parent;
    tmp.children[0] = h1->children[0];
    tmp.children[1] = h1->children[1];
    tmp.list = h1->list;

    swp(h1, h2, h2, parent);
    swp(h1, h2, h2, children[0]);
    swp(h1, h2, h2, children[1]);
    swp(&h1->list, &h2->list, &h2->list, next);
    swp(&h1->list, &h2->list, &h2->list, prev);

    swp(h2, &tmp, h1, parent);
    swp(h2, &tmp, h1, children[0]);
    swp(h2, &tmp, h1, children[1]);
    swp(&h2->list, &tmp.list, &h1->list, next);
    swp(&h2->list, &tmp.list, &h1->list, prev);

    h1->list.prev->next = &h1->list;
    h1->list.next->prev = &h1->list;

    h2->list.prev->next = &h2->list;
    h2->list.next->prev = &h2->list;

    force_link(h1, h2);
    force_link(h2, h1);

#undef swp
}

static void METHOD_IMPL(construct, int (*comp)(long long,long long))
{
    SUPER_CALL(Object, this, construct);
    INIT_LIST_HEAD(&this->node_list);
    this->comparator = comp;
}

static void METHOD_IMPL(shuffle, struct tree_node *heap)
{
    while(1)
    {
        if(heap->parent)
        {
            if(this->comparator(heap->priority, heap->parent->priority) < 0)
            {
                swap(heap, heap->parent);
                continue;
            }
        }
        if(heap->children[1])
        {
            if(this->comparator(
                heap->children[0]->priority, 
                heap->children[1]->priority) < 0)
            {
                if(this->comparator(
                    heap->children[0]->priority,
                    heap->priority) < 0)
                {
                    swap(heap, heap->children[0]);
                    continue;
                }
            } else if(this->comparator(
                heap->children[1]->priority,
                heap->priority) < 0)
            {
                swap(heap, heap->children[1]);
                continue;
            }
        }
        if(heap->children[0])
        {
            if(this->comparator(heap->children[0]->priority, heap->priority) < 0)
            {
                swap(heap, heap->children[0]);
                continue;
            }
        }
        break;
    }
}

static void METHOD_IMPL(put, struct tree_node *heap, int priority)
{
    heap->priority = priority;
    heap->ctxt = this;

    if(list_empty(&this->node_list))
    {
        list_add(&heap->list, &this->node_list);
        heap->parent = NULL;
        heap->children[0] = NULL;
        heap->children[1] = NULL;
        return;
    }
    struct tree_node *parent = 
        list_tail(struct tree_node, &this->node_list, list);
    if(parent->parent)
    {
        parent = parent->parent;
        if(parent->children[1])
            parent = list_first(struct tree_node, &parent->list, list);
    }

    list_add_tail(&heap->list, &this->node_list);
    heap->parent = parent;
    ASSERT(heap->parent != parent);
    if(!parent->children[0])
        parent->children[0] = heap;
    else
        parent->children[1] = heap;
    heap->children[0] = NULL;
    heap->children[1] = NULL;

    PRIV_CALL(this, shuffle, heap);
}

static void METHOD_IMPL(remove, struct tree_node *del)
{
    struct tree_node *new_head = list_tail(
        struct tree_node,
        &this->node_list,
        list);

    if(new_head == del)
    {
        list_del(&del->list);
        memset(del, '\0', sizeof(*del));
        return;
    }

    if(new_head->parent->children[0] == new_head)
        new_head->parent->children[0] = NULL;
    else if(new_head->parent->children[1] == new_head)
        new_head->parent->children[1] = NULL;

    new_head->parent = del->parent;
    new_head->children[0] = del->children[0];
    new_head->children[1] = del->children[1];
    if(new_head->children[0] == new_head)
        new_head->children[0] = NULL;
    if(new_head->children[1] == new_head)
        new_head->children[1] = NULL;
    if(new_head->parent)
    {
        if(new_head->parent->children[0] == del)
            new_head->parent->children[0] = del;
        if(new_head->parent->children[1] == del)
            new_head->parent->children[1] = del;
    }
    if(new_head->children[0])
        new_head->children[0]->parent = new_head;
    if(new_head->children[1])
        new_head->children[1]->parent = new_head;

    list_del(&new_head->list);
    list_add(&new_head->list, &del->list);
    list_del(&del->list);
    memset(del, '\0', sizeof(struct tree_node));

    PRIV_CALL(this, shuffle, new_head);
}

static struct tree_node *METHOD_IMPL(pop)
{
    struct tree_node *ret = CALL(this, peek);
    if(!ret)
        return NULL;
    CALL(this, remove, ret);
    return ret;
}

static struct tree_node *METHOD_IMPL(peek)
{
    if(list_empty(&this->node_list))
        return NULL;

    return list_first(
        struct tree_node,
        &this->node_list,
        list);
}

VIRTUAL(Object)
    VMETHOD_BASE(Object, construct);
    VMETHOD(put);
    VMETHOD(peek);
    VMETHOD(pop);
    VMETHOD(remove);
END_VIRTUAL
#undef CLASS_NAME // Heap
