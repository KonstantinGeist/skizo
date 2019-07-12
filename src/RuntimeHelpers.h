// *****************************************************************************
//
//  Copyright (c) Konstantin Geist. All rights reserved.
//
//  The use and distribution terms for this software are contained in the file
//  named License.txt, which can be found in the root of this distribution.
//  By using this software in any fashion, you are agreeing to be bound by the
//  terms of this license.
//
//  You must not remove this notice, or any other, from this software.
//
// *****************************************************************************

#ifndef RUNTIMEHELPERS_H_INCLUDED
#define RUNTIMEHELPERS_H_INCLUDED

// Runtime helpers which help the code to transition from Skizo code to native and vice versa.
// NOTE Runtime helpers remove some of the compilation load from TCC as they're precompiled.

#include "TypeRef.h"
#include "skizoscript.h"

namespace skizo { namespace core {
    class CString;
} }

namespace skizo { namespace script {

/**
 * A watch iterator for debugging.
 */
class CWatchIterator: public skizo::core::CObject
{
    const class CMethod* pMethod;
    int CurIndex;
    void** LocalRefs;
    int Size;

    char* Name;

public:
    CWatchIterator(const CMethod* _pMethod, void** localRefs, int size);
    virtual ~CWatchIterator();

    bool NextWatch(SKIZO_WATCHINFO* watchInfo);
};

class CDomain;
struct SMemoryManager;

/**
 * Internal.
 * Allows to unwind the virtual stack state (frames, debugging info) to a remembered
 * state.
 */
struct SVirtualUnwinder
{   
public:
    explicit SVirtualUnwinder(CDomain* domain);

    void Remember();
    void Unwind();

private:
    CDomain* m_domain;
    int m_stackFrameCnt;
    int m_debugDataStackCnt;
};

extern "C" {

/**
 * This function is to be used by the emitted machine code. The pointer to the memory manager
 * is inlined in the emitted code for speed.
 *
 * @warning The allocated piece of memory must have a properly set vtable at the zeroth offset
 * which the GC will try to access.
 */
void* SKIZO_API _soX_gc_alloc(SMemoryManager* mm, int sz, void** vtable);

/**
 * A specialized version of _soX_gc_alloc which creates a closure environment. The vtable
 * is also generated on demand.
 */
void* _soX_gc_alloc_env(SMemoryManager* mm, void* objClass);

/**
 * This function is to be used by the emitted machine code. Registers root sets which are
 * usually static variables. This function accepts a list of references to static variables
 * which are allocated on the heap.
 * @note The items in the lists must be references to variable locations, not the actual values.
 */
void SKIZO_API _soX_gc_roots(SMemoryManager* mm, void** rootRefs, int count);

/**
 * Initializes a static valuetype variable and GC-roots all its heap references.
 */
void SKIZO_API _soX_static_vt(SMemoryManager* mm, void* obj, void* objClass);

// ***********
//   RUNTIME
// ***********

/**
 * Registers the generated vtable for a given class (in emitted code).
 */
void SKIZO_API _soX_regvtable(void* klass, void** vtable);

/**
 * Patches vtables of string literals (emitted in prolog code after registering vtables).
 * See icalls/string.cpp for more info.
 */
void SKIZO_API _soX_patchstrings();

/**
 * Used by emitted code to check in runtime if a downcast is correct.
 */
void* SKIZO_API _soX_downcast(void* targetClass, void* objptr);

/**
 * Used by emitted code to unbox a value from a given interface. Aborts the runtime if the given
 * interface does not wrap the expected value type.
 *
 * @param vt A pointer for the space on stack the function will copy the value to.
 * @param vtSize The size of the value.
 * @param vtClass A pointer to CClass which describes the expected target value type.
 * @param intrfcObj A pointer to the input object cast to an interface.
 */
void SKIZO_API _soX_unbox(void* vt, int vtSize, void* vtClass, void* intrfcObj);

/**
 * Dispatches methods in runtime. The driving force behind interface calls.
 * pMethod is used to retrieve the name of the method and the signature.
 * Found method is stored in a per-class cache.
 */
void* SKIZO_API _soX_findmethod(void* objptr, void* pMethod);

/**
 * Another dynamic dispatcher, except it relies on data provided in 'msg' (typed to CDomainMessage*)
 * Used in server stubs for cross-domain method calls.
 * Like everything about remoting (cross-domain method calls), implemented in Remoting.cpp
 */
void* SKIZO_API _soX_findmethod2(void* objptr, void* msg);

/**
 * Dynamically checks if obj is of type 'type' (including interfaces and subclasses/superclasses).
 */
_so_bool SKIZO_API _soX_is(void* obj, void* type);

/**
 * Compares two arenas of memory for bitwise equality.
 * Used in value content comparisons for valuetypes ('===' syntax), among other things.
 */
_so_bool SKIZO_API _soX_biteq(void* a, void* b, int sz);

/**
 * Zeroes out a memory arena.
 */
void SKIZO_API _soX_zero(void* a, int sz);

/**
 * If soft debugging is enabled, this function is inserted in every method to keep track of all locals
 * in a given context.
 * pMethod refers to the current CMethod*, the same object which is referred to by _soX_pushframe et alia.
 * localRefs is a stack-allocated buffer which holds references to all variables in this method
 * (including 'this' and captured ones). The names of the locals, and their count -- everything is
 * inferred from pMethod which already contains all the relevant information.
 */
void SKIZO_API _soX_reglocals(void** localRefs, int sz);

/**
 * De-registers (pops) the locals pushed by the previous corresponding _soX_reglocals(..).
 */
void SKIZO_API _soX_unreglocals();

/** If soft debugging is enabled, this function is inserted for every "break" statement.
 * The logic on each break depends on the registered debugging callback. There are also built-in
 * debugging callbacks, if none were registered.
 * Implemented in Debugging.cpp
 */
void SKIZO_API _soX_break();

// WARNING resolveArrayClass,
//         _resolveNullableStruct
//         resolveFailableStruct
//         emitExplicitNullCheck
//         emitEnumFromInt
//     and emitAssertExpr depend on these exact hardcoded values!
#define SKIZO_ERRORCODE_RANGECHECK 0
#define SKIZO_ERRORCODE_NULLABLENULLCHECK 1
#define SKIZO_ERRORCODE_NULLDEREFERENCE 2
#define SKIZO_ERRORCODE_ASSERT_FAILED 3
#define SKIZO_ERRORCODE_FAILABLE_FAILURE 4
#define SKIZO_ERRORCODE_OUT_OF_MEMORY 5 // used by the GC
#define SKIZO_ERRORCODE_DISALLOWED_CALL 6 // used by the emitter when emitting ecalls inside untrusted domains
#define SKIZO_ERRORCODE_STACK_OVERFLOW 7
#define SKIZO_ERRORCODE_TYPE_INITIALIZATION_ERROR 8

/**
 * A macro for checking if a value is null and aborting appropriately (used in ICalls primarily).
 */
#define SKIZO_NULL_CHECK(obj) if(!obj) _soX_abort0(SKIZO_ERRORCODE_NULLDEREFERENCE);

void SKIZO_API _soX_abort0(int errCode);
void SKIZO_API _soX_abort(void* msg);

/**
 * Accepts a runtime-allocated error object and aborts by printing the message property of the error
 * object. If the property is null, prints a generic message.
 */
void SKIZO_API _soX_abort_e(void* errObj);

/**
 * A special invoker of 2-stage static constructor code (equivalent to v(1)).
 * Sets the class as uninitialized on abort.
 */
void SKIZO_API _soX_cctor(void* pClass, void* cctor);

/**
 * Inserted for all static methods of types that have static constructors: checks if the type is
 * is initialized or failed to do so (abort in the static constructor).
 */
void SKIZO_API _soX_checktype(void* pClass);

/**
 * Creates a new array.
 * @note "vtable" is the vtable of the generated array class, not the element type!
 */
void* SKIZO_API _soX_newarray(void* domain, int arrayLength, void** vtable);

/**
 * If SDomainCreation::StackTraceInfo is set true, the emitter emits this code at the beginning
 * of every method.
 */
void SKIZO_API _soX_pushframe(void* domain, void* pMethod);

/**
 * If SDomainCreation::StackTraceInfo is set true, the emitter emits this code at the end of
 * every method.
 *
 * @see _soX_pushframe
 */
void SKIZO_API _soX_popframe(void* domain);

/**
 * Same as _soX_pushframe(..), except also profiles the method.
 * @note returns the tick count to be used by _soX_popframe_prf(..)
 */
int SKIZO_API _soX_pushframe_prf(void* domain, void* pMethod);

/**
 * Same as _soX_popframe(..), except also profiles the method.
 * @note Accepts the tick count returned by the previous _soX_pushframe_prf(..).
 */
void SKIZO_API _soX_popframe_prf(void* domain, int tc);

/**
 * Adds a new handler to an event.
 */
void SKIZO_API _soX_addhandler(void* event, void* handler);

/**
 * Checks in runtime if "b" is zero -- and aborts in that case.
 * Avoids crashing the whole process (with all the domains inside) due to divide by zero
 * (OS-specific handlers are hard to deal with).
 */
int SKIZO_API _so_int_op_divide(int a, int b);

// *************
//   Remoting.
// *************

/**
 * Creates a new message, serializes passed arguments to it and sends it via the provided handle
 * (Skizo object! See SDomainHandleHeader).
 * 'pMethod' contains both the name of the method as well as type information of all arguments.
 * 'args' is a list of pointers to actual values.
 * 'blockingRet' -- it's a blocking call which returns the result via the passed ptr.
 * Implemented in Remoting.cpp
 */
void SKIZO_API _soX_msgsnd_sync(void* hDomain, void* soObjName, void* pMethod, void** args, void* blockingRet);

/**
 * Used in the emitter. Every foreign proxy generates also "server stubs", special thunks of code
 * that convert messages into native C stack frames.
 * 'args' is a stack-allocated buffer which holds temporary variables which will be passed to the target native
 * function.
 * 'daMsg' is CDomainMessage*
 * 'pMethod' is a hardcoded pointer to method metadata of type CMethod, used to correctly read
 * the contents of the message.
 */
void SKIZO_API _soX_unpack(void** args, void* daMsg, void* pMethod);

}

} }

#endif // RUNTIMEHELPERS_H_INCLUDED
