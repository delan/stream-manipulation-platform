
#include "class.h"

#define CLASS_NAME(a,b) a##Object##b
static Object METHOD_IMPL(construct)
{
    return this;
}

static void METHOD_IMPL(deconstruct){}

Object_t __Object;

void __init_class_Object()
{
    static int initted = 0;
    if(initted)
        return;
    initted = 1;
    Object this = &__Object;
    this->__super__ = NULL;
    VMETHOD(construct);
    VMETHOD(deconstruct);
}
#undef CLASS_NAME
