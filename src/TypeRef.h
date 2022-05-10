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

#ifndef TYPEREF_H_INCLUDED
#define TYPEREF_H_INCLUDED

#include "CastInfo.h"
#include "StringSlice.h"

namespace skizo { namespace script {

// ********************
//   Primitive types.
// ********************

// WARNING Must be identical to the emitted code (see SEmitter::emit)
#define _so_bool so_bool
#define _so_TRUE 1
#define _so_FALSE 0

/**
 * @warning A fix for the probably broken TCC codegen.
 * Returning a short from a function and immediately comparing it to a value
 * doesn't work correctly.
 * Perhaps not a bug, but a GCC<=>TCC interop issue as this happens only for icalls.
 */
#define _so_char int

/**
 * EPrimType is part of STypeRef for quickly creating typerefs from primitive types.
 * @warning Primitive types should support comparison with "==" in the C backend.
 * @warning Some code generation depends on the assumption that argument size is never bigger than the word size.
 */
enum EPrimType
{
    /**
     * A special type that signifies a lack of any types defined (for example, in methods returns) or a lack
     * of any meaningful result in certain functions that return STypeRef (default value).
     */
    E_PRIMTYPE_VOID,

    E_PRIMTYPE_INT,
    E_PRIMTYPE_FLOAT,
    E_PRIMTYPE_BOOL,
    E_PRIMTYPE_CHAR,
    E_PRIMTYPE_INTPTR,

    /**
     * For both valuetypes and reference types.
     */
    E_PRIMTYPE_OBJECT
};

enum ETypeRefKind
{
    E_TYPEREFKIND_NORMAL,

    /**
     * A failable is a special type with the syntax T? which allows to wrap both a possible result of type T
     * and an error of built-in class Error.
     */
    E_TYPEREFKIND_FAILABLE,

    /**
     * A foreign object exists in a separate domain.
     */
    E_TYPEREFKIND_FOREIGN
};

/**
 * A typeref encodes a range of types as defined in the code, from a simple "T" to a complex "[T]?"
 * Most of the metadata reference runtime classes through typerefs. When a typeref is first created by the parser,
 * it's in a non-resolved phase, meaning we only know the name of the type, optionally its array level, and
 * whether it's a failable; the actual runtime class object which describes this type is not found yet ("resolved").
 * This approach allows Skizo code refer to classes which weren't parsed yet. After everything was parsed,
 * the linking phase iterates over all typerefs found in the code (in fields, params etc.) and "resolves"
 * typerefs by finding (or constructing on demand) actual runtime classes to back up such types
 * (see field ::ResolvedClass).
 * A typeref is resolved by Domain::ResolveTypeRef(..) and dependent functions.
 */
struct STypeRef
{
    /**
     * @see STypeRef::SetPrimType
     */
    EPrimType PrimType;

    /**
     * @see STypeRef::SetObject
     */
    SStringSlice ClassName;

    /**
     * An unresolved typeref has this value set to zero.
     */
    class CClass* ResolvedClass;

    /**
     * Normal, Failable or something else?
     */
    ETypeRefKind Kind;

    /**
     * If ArrayLevel==1, the underlying type is [T], if ArrayLevel==2, the underlying type is [[T]], etc.
     */
    int ArrayLevel;

    STypeRef(int v) // Hack for HashMap's SStringSlice(0)
        : PrimType(E_PRIMTYPE_VOID),
          ResolvedClass(nullptr),
          Kind(E_TYPEREFKIND_NORMAL),
          ArrayLevel(0)
    {
    }

    STypeRef()
        : PrimType(E_PRIMTYPE_VOID),
          ResolvedClass(nullptr),
          Kind(E_TYPEREFKIND_NORMAL),
          ArrayLevel(0)
    {
    }

    /**
     * Creates an unresolved typeref from the name of the target type. See STypeRef::SetObject
     */
    explicit STypeRef(const SStringSlice& name);

    /**
     * "Primitive types", basic types like int/float etc., have a convenient way to construct unresolved typerefs from.
     */
    void SetPrimType(EPrimType pt)
    {
        this->PrimType = pt;
        this->ClassName.SetEmpty();
    }

    /**
     * Typerefs for anything other than the primitive types listed in EPrimType should be created by with this method.
     */
    void SetObject(const SStringSlice& objName)
    {
        this->PrimType = E_PRIMTYPE_OBJECT;
        this->ClassName = objName;
    }

    // ***********************************
    //   Support for CHashMap and others
    // ***********************************

    int GetHashCode() const;

    /**
     * @warning never compare ResolvedClass'es as Remoting depends on this
     * method and in different domains same typerefs can be resolved to
     * different SkizoClass objects.
     */
    bool Equals(const STypeRef& other) const
    {
        if(this->PrimType != other.PrimType) {
            return false;
        } else {
            if(this->PrimType == E_PRIMTYPE_OBJECT) {
                return this->ClassName.Equals(other.ClassName);
            } else {
                return true;
            }
        }
    }

    // ************************
    //   Convenience methods.
    // ************************

    bool IsVoid() const;
    bool IsHeapClass() const;
    bool IsNullAssignable() const;
    bool IsFailableStruct() const;
    bool IsStructClass() const;

    bool IsBoxable() const;

    /**
     * @note Returns true for both "Action" and "Action?" if allowFailable=true
     */
    bool IsMethodClass(bool allowFailable = true) const;

    /**
     * @note Returns true for both "[T]" and "[T]?" if allowFailable=true
     */
    bool IsArrayClass(bool allowFailable = true) const;

    // ************************

    /**
     * The cast is supposed to be in the direction "this <= other"
     * This method checks if two types are assignable or castable to each other.
     * Also deals with voids (which means "no inferred type yet").
     *
     * Mostly delegates it to CClass::getCastInfo of wrapped types, hence
     * typerefs must be resolved.
     */
    SCastInfo GetCastInfo(const STypeRef& other) const;

    /**
     * Composite typerefs are: T?, T*, [T].
     */
    bool IsComposite() const;
};

class CForcedTypeRef: public skizo::core::CObject
{
public:
    STypeRef TypeRef;
    const skizo::core::CString* FilePath;
    int LineNumber;

    CForcedTypeRef(const STypeRef& typeRef, const skizo::core::CString* filePath, int lineNumber)
        : TypeRef(typeRef),
          FilePath(filePath),
          LineNumber(lineNumber)
    {
    }
};

void SKIZO_REF(STypeRef& v);
void SKIZO_UNREF(STypeRef& v);
bool SKIZO_EQUALS(const STypeRef& v1, const STypeRef& v2);
bool SKIZO_IS_NULL(const STypeRef& v);
int SKIZO_HASHCODE(const STypeRef& v);

} }

#endif // TYPEREF_H_INCLUDED
