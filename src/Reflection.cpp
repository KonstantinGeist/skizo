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

#include "Contract.h"
#include "CoreUtils.h"
#include "Domain.h"
#include "FastByteBuffer.h"
#include "Method.h"
#include "RuntimeHelpers.h"

namespace skizo { namespace script {
using namespace skizo::core;
using namespace skizo::collections;

void* CMethod::InvokeDynamic(void* thisObj, void* args) const
{
    // **********************************
    //   Initialization & verification.
    // **********************************

    // Basic checks.

    if(m_declaringClass->SpecialClass() == E_SPECIALCLASS_INTERFACE) {
        CDomain::Abort("Interface methods aren't invokable.");
    }

    if(m_methodKind != E_METHODKIND_NORMAL && m_methodKind != E_METHODKIND_CTOR) {
        CDomain::Abort("Only normal methods and constructors allowed.");
    }
    if(this->IsAbstract()) {
        CDomain::Abort("Abstract methods aren't invokable.");
    }

    CClass* thisClass = nullptr;

    if(thisObj) {
        if(m_sig.IsStatic || m_methodKind == E_METHODKIND_CTOR) {
            CDomain::Abort("Static methods and constructors don't accept 'this'.");
        }

        thisClass = so_class_of(thisObj);

        if(thisClass->SpecialClass() == E_SPECIALCLASS_BOXED && m_declaringClass->SpecialClass() != E_SPECIALCLASS_BOXED) {
            // *********************************************************************
            // For boxed "this", we have a trick. The thing is, Method::invoke boxes
            // everything on sight, including "this". So what we have is a valuetype
            // boxed to "any". There's no need to unbox it back again, as the boxed
            // wrapper already implements the functionality we need. We simply need
            // to call the boxed wrapper's method instead of the real valuetype's
            // method. It will undergo all the checks above once again, though...
            // *********************************************************************
            // First make sure it's the correct valuetype.
            if(thisClass->ResolvedWrappedClass() != m_declaringClass) {
                CDomain::Abort("Object does not match target type.");
            }
            // Find the corresponding boxed class method and redirect the call to it.
            CMethod* boxedMethod = thisClass->MyMethod(m_name, false, E_METHODKIND_NORMAL);
            SKIZO_REQ_PTR(boxedMethod);
            return boxedMethod->InvokeDynamic(thisObj, args);
            // *********************************************************************
        }

        if(thisClass->SpecialClass() == E_SPECIALCLASS_BINARYBLOB) {
            CDomain::Abort("Binary blobs not supported yet.");
        }

        if(thisClass != m_declaringClass) {
            if(thisClass->IsSubclassOf(m_declaringClass)) {
                // Automatically perform dynamic method dispatch on the type of the target.
                CMethod* overridenMethod = thisClass->MyMethod(this->m_name, false, E_METHODKIND_NORMAL);
                if(overridenMethod && (overridenMethod != this)) { // call only if overriden
					return overridenMethod->InvokeDynamic(thisObj, args);
                }
            } else {
                CDomain::Abort("Object does not match target type.");
            }
        }

    } else {
        // thisObj == null
        if(!m_sig.IsStatic && m_methodKind != E_METHODKIND_CTOR) {
            // Aborts preventively.
            _soX_abort0(SKIZO_ERRORCODE_NULLDEREFERENCE);
        }
    }

    // Requires ReflectionPermission for calls on non-public methods because non-public
    // code may not demand any permissions at all relying on public methods to deal with it.
    if(m_access != E_ACCESSMODIFIER_PUBLIC) {
        CDomain::DemandPermission("ReflectionPermission");
    }

    // Argument array.
    SArrayHeader* arrayHeader = (SArrayHeader*)args;
    const int passedCount = arrayHeader? arrayHeader->length: 0;
    if(m_sig.Params->Count() != passedCount) {
        CDomain::Abort("Argument count mismatch.");
    }

    CDomain* domain = m_declaringClass->DeclaringDomain();

    // Checks argument types and constructs an argument buffer to be passed to the thunk.
    SFastByteBuffer argBuf (32);

    // It was checked above that "thisObj" is always non-null for instance methods.
    // I.e. this condition means "if this call is an instance call".
    if(thisObj) {
        void* pData = &thisObj;
        size_t sizeForUse = thisClass->GCInfo().SizeForUse;

        // WARNING TODO x86-only?
        // Arguments on stack should be at least the word size.
        if(sizeForUse < sizeof(void*)) {
            sizeForUse = sizeof(void*);
        }

        argBuf.AppendBytes((so_byte*)pData, sizeForUse);
    }

    for(int i = 0; i < passedCount; i++) {
        void* arg = ((void**)&arrayHeader->firstItem)[i];
        const CParam* param = m_sig.Params->Array()[i];

        if(arg) {
            CClass* argClass = so_class_of(arg);
            void* pData = &arg;

            if(argClass->SpecialClass() == E_SPECIALCLASS_BINARYBLOB) {
                CDomain::Abort("Binary blobs not supported yet.");
            }

            CClass* paramType = param->Type.ResolvedClass;

            // For boxed valuetypes, we need the wrapped class of the argument, but only if the parameter type
            // itself is a valuetype; otherwise, the method is designed to accept boxed values and not direct values.
            if(argClass->SpecialClass() == E_SPECIALCLASS_BOXED && paramType->IsValueType()) {
                argClass = argClass->ResolvedWrappedClass();
                SKIZO_REQ_PTR(argClass);

                pData = SKIZO_GET_BOXED_DATA(arg);
            }

            if(!argClass->Is(paramType)) {
                CDomain::Abort("Argument type mismatch.");
            }

            size_t sizeForUse = argClass->GCInfo().SizeForUse;

            // WARNING TODO x86-only?
            // Arguments on stack should be at least the word size.
            if(sizeForUse < sizeof(void*)) {
                sizeForUse = sizeof(void*);
            }

            argBuf.AppendBytes((so_byte*)pData, sizeForUse);

        } else {

            // The value is null, so the precise CClass for reference types is irrelevant.
            // However, it's an error to pass null where the method expects a valuetype.
            // TODO add support for failables, i.e. automatic conversion null => failable?

            if(param->Type.ResolvedClass->IsValueType()) {
                CDomain::Abort("Can't convert null to a valuetype.");
            }

            argBuf.AppendBytes((so_byte*)&arg, sizeof(void*));
        }
    }

    // For returned values, we reserve some space in the arg buffer itself.
    const CClass* retClass = m_sig.ReturnType.ResolvedClass;
    SKIZO_REQ_PTR(retClass);
    size_t retOffset = 0; // the offset in the arg buffer where the returned value will be written
                          // to by the thunk
    if(retClass->PrimitiveType() != E_PRIMTYPE_VOID) {
        retOffset = argBuf.Size();

        size_t sizeForUse = retClass->GCInfo().SizeForUse;

        // WARNING TODO x86-only?
        // Arguments on stack should be at least the word size.
        if(sizeForUse < sizeof(void*)) {
            sizeForUse = sizeof(void*);
        }

        // NOTE Uses this portion of the buffer to return the value to internally in case it uses non primitive valuetypes.
        if(retClass->PrimitiveType() == E_PRIMTYPE_OBJECT && retClass->IsValueType()) {
            argBuf.AppendBytes(0, sizeForUse);
            memset(argBuf.Bytes() + retOffset, 0, sizeForUse); // zeros out the return buffer just to be sure
        }
    }

    /*FILE* f = fopen("argbuffer.bin", "wb");
    fwrite(argBuf.Bytes(), argBuf.Size(), 1, f);
    fclose(f);*/
    //exit(1);

    void* pThunk = domain->ThunkManager().GetReflectionThunk(this);
    // Some types of methods may be not supported.
    if(!pThunk) {
        CDomain::Abort("Can not dynamically invoke the method (signature not supported yet).");
    }

    // Calls the target method through a thunk.
    return ((FReflectionThunk)pThunk)((void*)argBuf.Bytes());
}

// **************************
//   isGetter & isSetterFor
// **************************

bool CMethod::IsGetter(bool isStatic) const
{
    // Verifies the signature.
    if(m_sig.IsStatic != isStatic) {
        return false;
    }
    if(m_sig.Params->Count() != 0) {
        return false;
    }
    if(m_sig.ReturnType.PrimType == E_PRIMTYPE_VOID) {
        return false;
    }

    return true;
}

bool CMethod::IsSetterFor(const CMethod* getter) const
{
    if(m_sig.IsStatic != getter->m_sig.IsStatic) {
        return false;
    }
    if(m_sig.Params->Count() != 1) {
        return false;
    }
    if(m_sig.ReturnType.PrimType != E_PRIMTYPE_VOID) {
        return false;
    }

    const CClass* klass1 = getter->m_sig.ReturnType.ResolvedClass;
    const CClass* klass2 = m_sig.Params->Array()[0]->Type.ResolvedClass;
    SKIZO_REQ_PTR(klass1 && klass2);
    if(klass1 != klass2) {
        return false;
    }

    // ************************************
    //   Checks the name.
    // ************************************

    if(!m_name.StartsWithAscii("set")) {
        return false;
    }
    const int getterNameLength = getter->m_name.End - getter->m_name.Start;
    const int setterNameLength = this->m_name.End - this->m_name.Start;
    if(setterNameLength != (getterNameLength + 3)) { // '3' for "set"
        return false;
    }
    // Checks non-lowercase stuff.
    for(int i = 0; i < getterNameLength - 1; i++) {
        if(this->m_name.String->Chars()[this->m_name.Start + i + 3 + 1]
         != getter->m_name.String->Chars()[getter->m_name.Start + i + 1])
        {
            return false;
        }
    }
    
    // Checks the first letter. It should be  upper case for the setter and lower case for the getter.
    const so_char16 frstChar1 = CoreUtils::CharToLowerCase(this->m_name.String->Chars()[this->m_name.Start + 3]);
    const so_char16 frstChar2 = getter->m_name.String->Chars()[getter->m_name.Start];
    if(frstChar1 != frstChar2) {
        return false;
    }

    return true;
}

CArrayList<CProperty*>* CClass::GetProperties(bool isStatic) const
{
    SKIZO_REQ_NOT(isStatic, EC_ILLEGAL_ARGUMENT); // TODO

    auto r = new CArrayList<CProperty*>();

    for(int i = 0; i < m_instanceMethods->Count(); i++) {
        CMethod* potentialGetter = m_instanceMethods->Array()[i];

        if(potentialGetter->IsGetter(isStatic)) {

            for(int j = 0; j < m_instanceMethods->Count(); j++) {
                CMethod* potentialSetter = m_instanceMethods->Array()[j];

                if(potentialSetter->IsSetterFor(potentialGetter)) {
                    Auto<CProperty> prop (new CProperty());
                    prop->Getter.SetVal(potentialGetter);
                    prop->Setter.SetVal(potentialSetter);
                    r->Add(prop);
                    goto exit_outer_loop;
                }
            }

            exit_outer_loop: { }
        }
    }

    return r;
}

// TODO for base classes, should verify that they stem from the base directory
bool CDomain::isClassLoaded(const char* _className) const
{
    Auto<const CString> className (CString::FromUtf8(_className));
    SStringSlice str (className, 0, className->Length());

    return m_klassMap.Contains(str);
}

} }
