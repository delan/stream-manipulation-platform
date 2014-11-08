
#ifndef CLASS_H
#define CLASS_H

#include "util.h"

#define DECLARE_CLASS(name) typedef struct name *name
#define CLASS(spr)        \
typedef struct CLASS_NAME(,) *CLASS_NAME(,);      \
typedef struct CLASS_NAME(,) CLASS_NAME(,_t); \
extern CLASS_NAME(,_t) CLASS_NAME(__,);   \
void CLASS_NAME(__init_class_,)(void);   \
struct CLASS_NAME(,) {                   \
    spr ## _t super;          \
    spr __super__;         

#define END_CLASS   };

#define METHOD(name, ...)      \
    (*name)(CLASS_NAME(,) this, ## __VA_ARGS__)

#if 0
#define CONSTRUCT(type, ptr, ...)  \
    (__init_class_ ## type(), \
     memcpy(ptr, &__ ## type, sizeof(type ## _t)), \
     (type)((void*(*)())((Object)&__ ## type)->construct) \
        ((Object)ptr, ## __VA_ARGS__))
#endif

#define NEW(type, ...)         \
    (__init_class_ ## type(),  \
    (type)((void*(*)())((Object)&__ ## type)->construct)( \
        (Object)memdup(&__ ## type, sizeof(type ## _t)), ## __VA_ARGS__))

#if 0
#define DECONSTRUCT(obj)    \
    ((Object)obj)->deconstruct((Object)obj)
#endif

#define DELETE(obj)    \
    (((Object)obj)->deconstruct((Object)obj), \
    free(obj))

#define METHOD_IMPL(name, ...)  \
    CLASS_NAME(,_ ## name)(CLASS_NAME(,) this, ## __VA_ARGS__)

#define VIRTUAL(spr)    \
CLASS_NAME(,_t) CLASS_NAME(__,);          \
void CLASS_NAME(__init_class_,)() {  \
    static int __initted = 0;   \
    if(__initted)               \
        return;                 \
    __init_class_ ## spr();   \
    __initted = 1;              \
    CLASS_NAME(,) this = CLASS_NAME(&__,);    \
    this->__super__ = &__ ## spr;  \
    memcpy(&this->super, &__ ## spr, sizeof(spr ## _t));
#define END_VIRTUAL }

#define VMETHOD(name)     \
    this->name = (void*) CLASS_NAME(,_ ## name)
#define VMETHOD_BASE(base, name)     \
    ((base)this)->name = (void*) CLASS_NAME(,_ ## name)

#define VFIELD(name)            \
    this->name
#define VFIELD_BASE(base, name)       \
    ((base)this)->name

#define INIT_CLASS(name)        \
    __init_class_ ## name()

#define PRIV_CALL(me, func, ...)    \
    CLASS_NAME(,_ ## func)(me, ## __VA_ARGS__);
#define CALL(me, func, ...)     \
    (me)->func(me, ## __VA_ARGS__)
#define SUPER_CALL(base, me, func, ...) \
    ((base)(me)->__super__)->func((base)me, ## __VA_ARGS__)

typedef struct Object *Object;
typedef struct Object Object_t;
extern Object_t __Object;
struct Object
{
    void *__super__;
    Object (*construct)(Object);
    void (*deconstruct)(Object);
};

void __init_class_Object(void);

#endif
