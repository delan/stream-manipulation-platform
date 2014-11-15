
/**
 * HOW TO:
 *
 * == Header file:
 * 
 *  #include "class.h"
 *  #define CLASS_NAME(a,b) a## NameOfClass ##b
 *
 *  // Classes with no super-class should have 'Object' as their super class
 *  CLASS(SuperClass)
 *      int METHOD(MethodName, int arg1, int arg2);
 *
 *      int fieldName;
 *  END_CLASS
 *  #undef CLASS_NAME // NameOfClass
 *
 *  == C file:
 *
 *  #include "hdr_file.h"
 *
 *  #define CLASS_NAME(a,b) a## NameOfClass ##b
 *  // constructor implementation (if it needs to be over-ridden)
 *  NameOfClass METHOD_IMPL(construct)
 *  {
 *      // the base object is the object where the method being
 *      // called was declared - The constructor is declared in Object
 *      SUPER_CALL(Object, this, construct);
 *      // class initiation code..
 *      return this;
 *  }
 *
 *  int METHOD_IMPL(MethodName, int arg1, int arg2)
 *  {
 *      return this->fieldName + arg1 + arg2;
 *  }
 *
 *  VIRTUAL(SuperClass)
 *  {
 *      // over-rides the default constructor
 *      VMETHOD_BASE(Object, construct);
 *      VMETHOD(MethodName);
 *
 *      // sets the initial value of a field when a new object is created
 *      VFIELD(fieldName) = 5;
 *  }
 *  #undef CLASS_NAME // NameOfClass
 *
 *  == Using a class:
 *
 *  int main()
 *  {
 *      NameOfClass obj = NEW(NameOfClass);
 *
 *      printf("%d\n", CALL(obj, MethodName, 5, 7));
 *      obj->fieldName = 1;
 *      printf("%d\n", CALL(obj, MethodName, 1, 3));
 *
 *      DELETE(obj);
 *
 *      return 0;
 *  }
 *
 */

#ifndef CLASS_H
#define CLASS_H

#include "util.h"

/* merely decares the pointer type such that it can be used
 * where there's a dependency cycle */
#define DECLARE_CLASS(name) typedef struct name *name

/* declares the required typedefs, and starts defining the struct.
 * Additional members / methods are declared in the object within
 * the CLASS and END_CLASS macros */
#define CLASS(spr)                                                          \
typedef struct CLASS_NAME(,) *CLASS_NAME(,);                                \
typedef struct CLASS_NAME(,) CLASS_NAME(,_t);                               \
extern CLASS_NAME(,_t) CLASS_NAME(__,);                                     \
void CLASS_NAME(__init_class_,)(void);                                      \
struct CLASS_NAME(,) {                                                      \
    spr ## _t super;                                                        \
    spr __super__;         

/* Ends a class definition */
#define END_CLASS   };

/* Used inside CLASS .. END_CLASS macros to declare a method
 * inside a class. Usage:
 * return_type METHOD(method_name, arg_types...) */
#define METHOD(name, ...)                                                   \
    (*name)(CLASS_NAME(,) this, ## __VA_ARGS__)

#if 0
#define CONSTRUCT(type, ptr, ...)                                           \
    (__init_class_ ## type(),                                               \
     memcpy(ptr, &__ ## type, sizeof(type ## _t)),                          \
     (type)((void*(*)())((Object)&__ ## type)->construct)                   \
        ((Object)ptr, ## __VA_ARGS__))
#endif

/* Allocates memory and copies into it the class identity structure.
 * The objects constructor is also called */
#define NEW(type, ...)                                                      \
    (__init_class_ ## type(),                                               \
    (type)((void*(*)())((Object)&__ ## type)->construct)(                   \
        (Object)memdup(&__ ## type, sizeof(type ## _t)), ## __VA_ARGS__))

#if 0
#define DECONSTRUCT(obj)                                                    \
    ((Object)obj)->deconstruct((Object)obj)
#endif

/* Calls can objects deconstructor, and then frees the memory */
#define DELETE(obj)                                                         \
    (((Object)obj)->deconstruct((Object)obj),                               \
    free(obj),                                                              \
    obj = NULL)

/* Helper to define a method implementation. Usage:
 * return_type METHOD_IMPL(method_name, method_args..)
 * {
 *     ... 
 * }
 */
#define METHOD_IMPL(name, ...)                                              \
    CLASS_NAME(,_ ## name)(CLASS_NAME(,) this, ## __VA_ARGS__)

/* Defines a function which sets up the 'identity' copy of this object.
 * A copy of this is taken every time an instance of the option is created.
 * VMETHOD.. and VFIELD.. helper functions fill in the methods / fields,
 * however for more complicated operations, this is merely a function. The
 * identity object is pointed to by 'this'. */
#define VIRTUAL(spr)                                                        \
CLASS_NAME(,_t) CLASS_NAME(__,);                                            \
void CLASS_NAME(__init_class_,)() {                                         \
    static int __initted = 0;                                               \
    if(__initted)                                                           \
        return;                                                             \
    __init_class_ ## spr();                                                 \
    __initted = 1;                                                          \
    CLASS_NAME(,) this = CLASS_NAME(&__,);                                  \
    this->__super__ = &__ ## spr;                                           \
    memcpy(&this->super, &__ ## spr, sizeof(spr ## _t));

/* Ends a class definition */
#define END_VIRTUAL }

/* For use between VIRTUAL .. END_VIRTUAL. Helper function to set
 * a method. Alternatively, a method can be set merely with:
 * this->method_name = method_implementation */
#define VMETHOD(name)                                                       \
    this->name = (void*) CLASS_NAME(,_ ## name)

/* For use between VIRTUAL .. END_VIRTUAL. Helper function to over-ride
 * a method. Alternatively, a method can be set merely with:
 * ((BaseClass)this)->method_name = method_implementation */
#define VMETHOD_BASE(base, name)                                            \
    ((base)this)->name = (void*) CLASS_NAME(,_ ## name)

/* Usage: VFIELD(name) = value; */
#define VFIELD(name)                                                        \
    this->name
#define VFIELD_BASE(base, name)                                             \
    ((base)this)->name

/* The 'NEW' macro automatically initiates a class. If the identity
 * copy of a class needs to be set up before a new instance is created
 * via 'NEW', use this */
#define INIT_CLASS(name)                                                    \
    __init_class_ ## name()

/* Calls a method that doesn't exist in the class declaration
 * (i.e, it's private) */
#define PRIV_CALL(me, func, ...)                                            \
    CLASS_NAME(,_ ## func)(me, ## __VA_ARGS__);
/* Calls a method in an objects class */
#define CALL(me, func, ...)                                                 \
    (me)->func(me, ## __VA_ARGS__)
/* Calls a method that is declared by a base class of the object */
#define SUPER_CALL(base, me, func, ...)                                     \
    ((base)(me)->__super__)->func((base)me, ## __VA_ARGS__)

/* The 'Object' class, which all other classes will inherit from.
 * This is defined manually as it's somewhat different to a normal class */
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
