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

#include "../Domain.h"
#include "../RuntimeHelpers.h"
#include <assert.h>

namespace skizo { namespace script {
using namespace skizo::core;
using namespace skizo::collections;

extern "C" {

void* SKIZO_API _so_Type_typeHandleOf(void* obj)
{
    SKIZO_NULL_CHECK(obj);

    CClass* r = so_class_of(obj);

    // "Type::of" of boxed valuetypes should return the type for the actual valuetype class. Boxed classes
    // should stay internal and not be exposed.
    if(r->SpecialClass() == E_SPECIALCLASS_BOXED) {
        r = r->ResolvedWrappedClass();
        assert(r);
    }

    return r;
}

void* SKIZO_API _so_Type_nameImpl(void* typeHandle)
{
    CClass* pClass = ((CClass*)typeHandle);
    return CDomain::ForCurrentThread()->CreateString(pClass->NiceName(), true);
}

void* SKIZO_API _so_Type_fromTypeHandleImpl(void* typeHandle)
{
    return ((CClass*)typeHandle)->RuntimeObject();
}

void SKIZO_API _so_Type_setToTypeHandle(void* typeHandle, void* runtimeObj)
{
    ((CClass*)typeHandle)->SetRuntimeObject(runtimeObj);
}

int SKIZO_API _so_Type_allTypeHandles(void* pTypeHandleArr)
{
    CDomain* domain = CDomain::ForCurrentThread();
    const CArrayList<CClass*>* classes = domain->Classes();
    const int klassCount = classes->Count();

    if(pTypeHandleArr) {
        // Checks the integrity of the Skizo code which interoperates with this icall.
        SArrayHeader* arrayHeader = (SArrayHeader*)pTypeHandleArr;
        (void)arrayHeader;
        assert(so_class_of(pTypeHandleArr)->SpecialClass() == E_SPECIALCLASS_ARRAY);
        assert(arrayHeader->length == klassCount);

        // Fixes up the array pointer to point to the beginning of its data.
        CClass** targetArr = (CClass**)((char*)pTypeHandleArr + offsetof(SArrayHeader, firstItem));
        for(int i = 0; i < klassCount; i++) {
            targetArr[i] = classes->Array()[i];
        }
    }

    return klassCount;
}

void* SKIZO_API _so_Type_forNameImpl(void* pClassName)
{
    SKIZO_NULL_CHECK(pClassName);

    return CDomain::ForCurrentThread()->ClassByNiceName(so_string_of(pClassName));
}

void* SKIZO_API _so_Type_getAttributeImpl(void* typeHandle, void* pAttrName)
{
    SKIZO_NULL_CHECK(pAttrName);

    CDomain* domain = CDomain::ForCurrentThread();
    const CClass* pClass = ((CClass*)typeHandle);
    const CString* attrName = so_string_of(pAttrName);

    const CArrayList<CAttribute*>* attrs = pClass->Attributes();
    if(attrs) {
        for(int i = 0; i < attrs->Count(); i++) {
            const CAttribute* attr = attrs->Array()[i];

            if(attr->Name.Equals(attrName)) {
                Auto<const CString> value (attr->Value.ToString());
                return domain->CreateString(value, false);
            }
        }
    }

    return nullptr;
}

_so_bool SKIZO_API _so_Type_getBoolProp(void* typeHandle, void* pName)
{
    SKIZO_NULL_CHECK(pName);

    const CClass* pClass = ((CClass*)typeHandle);
    const CString* name = so_string_of(pName);
    const ESpecialClass specialClass = pClass->SpecialClass();

    if(name->EqualsASCII("isValueType")) {
        return pClass->IsByValue();
    } else if(name->EqualsASCII("isArray")) {
        return specialClass == E_SPECIALCLASS_ARRAY;
    } else if(name->EqualsASCII("isFailable")) {
        return specialClass == E_SPECIALCLASS_FAILABLE;
    } else if(name->EqualsASCII("isForeign")) {
        return specialClass == E_SPECIALCLASS_FOREIGN;
    } else if(name->EqualsASCII("isBoxed")) {
        return specialClass == E_SPECIALCLASS_BOXED;
    } else if(name->EqualsASCII("isMethodClass")) {
        return specialClass == E_SPECIALCLASS_METHODCLASS;
    } else if(name->EqualsASCII("isEventClass")) {
        return specialClass == E_SPECIALCLASS_EVENTCLASS;
    } else if(name->EqualsASCII("isAlias")) {
        return specialClass == E_SPECIALCLASS_ALIAS;
    } else if(name->EqualsASCII("isInterface")) {
        return specialClass == E_SPECIALCLASS_INTERFACE;
    } else if(name->EqualsASCII("isStatic")) {
        return pClass->IsStatic();
    } else if(name->EqualsASCII("isAbstract")) {
        return pClass->IsAbstract();
    } else if(name->EqualsASCII("isCompilerGenerated")) {
        return pClass->IsCompilerGenerated();
    } else {
        CDomain::Abort("Unrecognized bool property (Type::getBoolProp)");
        return false;
    }
}

#define INSTANCE_METHODS 0
#define STATIC_METHODS 1

void* SKIZO_API _so_Type_methodsImpl(void* typeHandle, int kind)
{
    CDomain* curDomain = CDomain::ForCurrentThread();
    CClass* pClass = ((CClass*)typeHandle);

    const CArrayList<CMethod*>* methods;
    if(kind == INSTANCE_METHODS) {
        methods = pClass->InstanceMethods();
    } else if(kind == STATIC_METHODS) {
        methods = pClass->StaticMethods();
    } else {
        methods = nullptr; // to shut up the compiler
        assert(false);
    }

    const int count = methods->Count();
    STypeRef elementTypeRef;
    elementTypeRef.SetPrimType(E_PRIMTYPE_INTPTR);
    void* r = curDomain->CreateArray(elementTypeRef, count);
    for(int i = 0; i < count; i++) {
        CMethod* m = methods->Array()[i];
        curDomain->SetArrayElement(r, i, (void*)&m);
    }

    return r;
}

// Properties are returned as two arrays: one array contains the getters, the other one
// containers the setters. Those two arrays are then recombined again into an array of
// Property objects on the Skizo side.
void* SKIZO_API _so_Type_propertiesImpl(void* typeHandle, _so_bool getters, _so_bool isStatic)
{
    assert(!isStatic); // TODO
    CDomain* curDomain = CDomain::ForCurrentThread();
    CClass* const pClass = ((CClass*)typeHandle);

    Auto<CArrayList<CProperty*> > props (pClass->GetProperties(isStatic));
    const int count = props->Count();
    STypeRef elementTypeRef;
    elementTypeRef.SetPrimType(E_PRIMTYPE_INTPTR);
    void* r = curDomain->CreateArray(elementTypeRef, count);
    for(int i = 0; i < count; i++) {
        CMethod* m = getters? props->Array()[i]->Getter: props->Array()[i]->Setter;
        curDomain->SetArrayElement(r, i, (void*)&m);
    }

    return r;
}

void* SKIZO_API _so_Type_createInstanceImpl(void* typeHandle, void* pCtorName, void* args)
{
    SKIZO_NULL_CHECK(pCtorName);
    const CString* ctorName = so_string_of(pCtorName);

    const CClass* pClass = ((CClass*)typeHandle);
    const CArrayList<CMethod*>* instanceCtors = pClass->InstanceCtors();
    for(int i = 0; i < instanceCtors->Count(); i++) {
        const CMethod* ctor = instanceCtors->Array()[i];

        // A match!
        if(ctor->Name().Equals(ctorName)) {
            // An instance constructor is a simple static method internally.
            return ctor->InvokeDynamic(nullptr, args);
        }
    }

    CDomain::Abort("Type::createInstance(..) failed to find a constructor with the specified name.");
    return nullptr;
}

}

} }
