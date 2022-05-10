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

#ifndef NATIVEHEADERS_H_INCLUDED
#define NATIVEHEADERS_H_INCLUDED

namespace skizo { namespace core {
    class CString;
} }

namespace skizo { namespace script {

/**
 * Apply this attribute to every field imported from Skizo to C.
 */
#define SKIZO_FIELD __attribute__ ((aligned(sizeof(void*))))

/**
 * This pair of macros turns internal Skizo exceptions into publicly visible Skizo aborts.
 * For use in ICalls that wrap Skizo methods: we don't want to leak arbitrary Skizo exceptions outside
 * of ICalls.
 */
#define SKIZO_GUARD_BEGIN try {
#define SKIZO_GUARD_END } catch(const skizo::core::SException& _e) { skizo::script::CDomain::Abort(const_cast<char*>(_e.Message())); }

/**
 * Gets the pointer to the boxed data in a boxed class (simply skips the vtable).
 */
#define SKIZO_GET_BOXED_DATA(obj) ((char*)obj + sizeof(void*))

/**
 * Skips the vtable and the item count for arrays.
 */
#define SKIZO_GET_ARRAY_DATA(arr) ((char*)arr + sizeof(void*) + sizeof(int))

/**
 * Skips the vtable.
 */
#define SKIZO_GET_OBJECT_DATA(obj) ((char*)obj + sizeof(void*))

struct SObjectHeader
{
    /**
     * VTables are used for:
     *  1) dispatching virtual methods (uses pointers to functions starting from 1st index)
     *  2) storing a bit mark for the GC (modifies the vtable pointer's last bit)
     *  3) getting the class of the object (the pointer is stored at 0th index)
     */
    void** vtable;
};

/**
 * @warning Keep in sync with Domain::initBasicClasses
 */
struct SStringHeader
{
    void** vtable;

    /**
     * Wraps the usual Skizo string.
     */
    const skizo::core::CString* pStr SKIZO_FIELD;
};

/**
 * Extracts the wrapped Skizo string from an allocated object's header.
 */
#define so_string_of(ptr) (((SStringHeader*)(ptr))->pStr)

/**
 * @warning Don't change the layout of this struct! _soX_newArray depends on this when calculating the sizeof
 */
struct SArrayHeader
{
    void** vtable;
    int length;
    char firstItem SKIZO_FIELD;
};

/**
 * @warning Don't change the layout of this struct! At least _soX_abort_e depends on it!
 */
struct SErrorHeader
{
    void** vtable;
    SStringHeader* message;
};

/**
 * @warning Should be synchronized with CDomain::resolveFailableStruct(..)
 */
struct SFailableHeader
{
    SErrorHeader* error;

    /**
     * The value the failable wraps.
     */
    union {
        /**
         * For reference objects: the reference to the object on the heap.
         */
        void* refData;

        /**
         * For valuetypes: the first byte of the wrapped object (embedded).
         */
        char valData;
    };
};

/**
 * @warning Should be synchronized with the definition at base/Map.skizo
 */
struct SMapHeader
{
    void** vtable;
    struct SkizoMapObject* mapObj SKIZO_FIELD;
};

/**
 * @warning Should be synchronized with the rest of code.
 */
struct SEnumHeader
{
    void** vtable;
    int intValue SKIZO_FIELD;
    SStringHeader* stringValue SKIZO_FIELD;
};

/**
 * @warning Should be synchronized with the rest of code.
 */
struct SEventHeader
{
    void** vtable;
    SArrayHeader* array SKIZO_FIELD;
};

/**
 * @warning Should be synchronized with the rest of code.
 */
struct SRange
{
    int from SKIZO_FIELD;
    int to SKIZO_FIELD;
};

/**
 * @warning Should be synchronized with the rest of code (with the 'domain' module as well)
 */
struct SDomainHandleHeader
{
    void** vtable;

    /**
     * The Skizo object we wrap.
     */
    class CDomainHandle* wrapped SKIZO_FIELD;
};

/**
 * @warning Should be synchronized with the rest of code (for example, Domain::resolveForeignProxy(..))
 * Used by CDomainHandle::ImportObject
 */
struct SForeignProxyHeader
{
    void** vtable;
    
    /**
     * GC-allocated DomainHandle object.
     */
    SDomainHandleHeader* hDomain SKIZO_FIELD;
    
    /**
     * GC-allocated string with the name of the object.
     */
    SStringHeader* name SKIZO_FIELD;
};

/**
 * @warning Should be synchronized with the rest of code.
 * @note For method class implementations only. Top method classes don't have the "codeOffset" field.
 */
struct SClosureHeader
{
    void** vtable;
    void* env;
    
    /**
     * To remember the result of "Marshal::codeOffset".
     */
    void* codeOffset;
};

/**
 * The header for Skizo objects of type "Type" which wrap CClass instances.
 * @warning Make sure the layout is in sync with the Skizo side.
 */
struct STypeHeader
{
    void** vtable;
    CClass* typeHandle SKIZO_FIELD;
};

} }

#endif // NATIVEHEADERS_H_INCLUDED
