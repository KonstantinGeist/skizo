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

#include "TypeRef.h"
#include "Class.h"
#include "Contract.h"

namespace skizo { namespace script {

// ************
//   TypeRef.
// ************

void SKIZO_REF(STypeRef& v) { }
void SKIZO_UNREF(STypeRef& v) { }
bool SKIZO_IS_NULL(const STypeRef& v) { return false; }

bool SKIZO_EQUALS(const STypeRef& v1, const STypeRef& v2)
{
    return v1.PrimType == v2.PrimType
        && v1.ClassName.Equals(v2.ClassName)
        && v1.Kind == v2.Kind
        && v1.ArrayLevel == v2.ArrayLevel;
}

int SKIZO_HASHCODE(const STypeRef& v)
{
    int h = 0;
    h *= ((int)v.PrimType) * 27;
    if(v.PrimType == E_PRIMTYPE_OBJECT) {
        h *= (SKIZO_HASHCODE(v.ClassName)) * 13;
    } else if(v.Kind == E_TYPEREFKIND_FAILABLE) {
        h *= 37;
    } else if(v.Kind == E_TYPEREFKIND_FOREIGN) {
        h *= 31;
    }
    if(v.ArrayLevel) {
        h *= v.ArrayLevel * 23;
    }
    return h;
}

STypeRef::STypeRef(const SStringSlice& name)
    : PrimType(E_PRIMTYPE_VOID),
      ResolvedClass(nullptr),
      Kind(E_TYPEREFKIND_NORMAL),
      ArrayLevel(0)
{
    if(name.IsEmpty()) {
        return;
    }

    if(name.EqualsAscii("void")) {
        this->SetPrimType(E_PRIMTYPE_VOID);
    } else if(name.EqualsAscii("int")) {
        this->SetPrimType(E_PRIMTYPE_INT);
    } else if(name.EqualsAscii("float")) {
        this->SetPrimType(E_PRIMTYPE_FLOAT);
    } else if(name.EqualsAscii("bool")) {
        this->SetPrimType(E_PRIMTYPE_BOOL);
    } else if(name.EqualsAscii("char")) {
        this->SetPrimType(E_PRIMTYPE_CHAR);
    } else if(name.EqualsAscii("intptr")) {
        this->SetPrimType(E_PRIMTYPE_INTPTR);
    } else {
        this->SetObject(name);
    }
}

int STypeRef::GetHashCode() const
{
    if(this->PrimType == E_PRIMTYPE_OBJECT) {
        return SKIZO_HASHCODE(this->ClassName);
    } else {
        return (int)this->PrimType;
    }
}

bool STypeRef::IsVoid() const
{
    return this->PrimType == E_PRIMTYPE_VOID;
}

bool STypeRef::IsHeapClass() const
{
    if(this->PrimType == E_PRIMTYPE_OBJECT) {
        SKIZO_REQ_PTR(this->ResolvedClass);
        return !this->ResolvedClass->IsValueType();
    } else {
        return false;
    }
}

bool STypeRef::IsNullAssignable() const
{
    if(this->Kind == E_TYPEREFKIND_FAILABLE || this->ResolvedClass->SpecialClass() == E_SPECIALCLASS_FAILABLE) {
        return this->ResolvedClass->WrappedClass().IsNullAssignable();
    } else {
        return this->IsHeapClass() || (this->PrimType == E_PRIMTYPE_INTPTR);
    }
}

bool STypeRef::IsFailableStruct() const
{
    SKIZO_REQ_PTR(this->ResolvedClass);
    return this->ResolvedClass->SpecialClass() == E_SPECIALCLASS_FAILABLE;
}

bool STypeRef::IsStructClass() const
{
    if(this->PrimType == E_PRIMTYPE_OBJECT) {
        SKIZO_REQ_PTR(this->ResolvedClass);
        return this->ResolvedClass->IsValueType();
    } else {
        return false;
    }
}

bool STypeRef::IsMethodClass(bool allowFailable) const
{
    if(this->PrimType == E_PRIMTYPE_OBJECT) {
        SKIZO_REQ_PTR(this->ResolvedClass);
        if(this->ResolvedClass->SpecialClass() == E_SPECIALCLASS_METHODCLASS) {
            return true;
        } else {
            return allowFailable
                 && this->ResolvedClass->SpecialClass() == E_SPECIALCLASS_FAILABLE
                 && this->ResolvedClass->ResolvedWrappedClass()->SpecialClass() == E_SPECIALCLASS_METHODCLASS;
        }
    } else {
        return false;
    }
}

bool STypeRef::IsArrayClass(bool allowFailable) const
{
    if(this->PrimType == E_PRIMTYPE_OBJECT) {
        SKIZO_REQ_PTR(this->ResolvedClass);
        if(this->ResolvedClass->SpecialClass() == E_SPECIALCLASS_ARRAY) {
            return true;
        } else {
            return allowFailable
                 && this->ResolvedClass->SpecialClass() == E_SPECIALCLASS_FAILABLE
                 && this->ResolvedClass->ResolvedWrappedClass()->SpecialClass() == E_SPECIALCLASS_ARRAY;
        }
    } else {
        return false;
    }
}

bool STypeRef::IsBoxable() const
{
    switch(this->PrimType) {
        case E_PRIMTYPE_INT:
        case E_PRIMTYPE_FLOAT:
        case E_PRIMTYPE_BOOL:
        case E_PRIMTYPE_CHAR:
        case E_PRIMTYPE_INTPTR:
            return true;
        case E_PRIMTYPE_OBJECT:
            SKIZO_REQ_PTR(this->ResolvedClass);
            return this->ResolvedClass->IsValueType()
                && this->ResolvedClass->SpecialClass() == E_SPECIALCLASS_NONE;
        default:
            return false;
    }
}

SCastInfo STypeRef::GetCastInfo(const STypeRef& other) const
{
    SCastInfo castInfo;

    if(this->PrimType == E_PRIMTYPE_VOID || other.PrimType == E_PRIMTYPE_VOID) {
        return castInfo; // VOID means "no type inferred yet" here
    } else {
        // The typerefs should be resolved to flat types.
        SKIZO_REQ_PTR(this->ResolvedClass);
        SKIZO_REQ_PTR(other.ResolvedClass);
        SKIZO_REQ_EQUALS(this->ArrayLevel, 0);
        SKIZO_REQ_EQUALS(other.ArrayLevel, 0);
        SKIZO_REQ_EQUALS(this->Kind, E_TYPEREFKIND_NORMAL);
        SKIZO_REQ_EQUALS(other.Kind, E_TYPEREFKIND_NORMAL);

        castInfo = this->ResolvedClass->GetCastInfo(other.ResolvedClass);
    }

    return castInfo;
}

bool STypeRef::IsComposite() const
{
    return (Kind != E_TYPEREFKIND_NORMAL) || (ArrayLevel > 0);
}

} }
