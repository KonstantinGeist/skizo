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

#ifndef TYPERESOLUTION_H_INCLUDED
#define TYPERESOLUTION_H_INCLUDED

#include "Class.h"
#include "Contract.h"
#include "Domain.h"
#include "ScriptUtils.h"
#include "StringBuilder.h"

// Type resolution code is moved to a separate file more better readability.

namespace skizo { namespace script {
using namespace skizo::core;
using namespace skizo::collections;

bool CDomain::ResolveTypeRef(STypeRef& typeRef)
{
    if(typeRef.ResolvedClass) {
        return true;
    }

    if(typeRef.PrimType == E_PRIMTYPE_OBJECT) {
        if(!typeRef.ResolvedClass) {
            CClass* klass;
            if(!m_klassMap.TryGet(typeRef.ClassName, &klass)) {
                return false;
            }
            typeRef.ResolvedClass = klass;
            klass->Unref();

            // **********************
            //   Alias redirection.
            // **********************

            // Aliases are resolved first, so "klass->m_wrappedClass" must be resolved already.
            if(klass->SpecialClass() == E_SPECIALCLASS_ALIAS) {
                // If an alias refers to an alias, this field can be zero.
                if(!klass->ResolvedWrappedClass()) {
                    return false;
                }
                typeRef = klass->WrappedClass();
            }
        }
    } else {
        // FIX Primtypes are also set their "ClassName".
        CClass* resolvedClass = m_primKlassMap.Item((int)typeRef.PrimType);
        typeRef.ClassName = resolvedClass->FlatName();
        typeRef.ResolvedClass = resolvedClass;
    }

    if(typeRef.Kind == E_TYPEREFKIND_FAILABLE) {
        // Support for "[T]?"
        if(typeRef.ArrayLevel > 0) {
            typeRef.Kind = E_TYPEREFKIND_NORMAL;
            if(!resolveArrayClass(typeRef)) {
                return false;
            }
            // typeRef.ArrayLevel is now 0
            // restore the 'Kind' flag
            typeRef.Kind = E_TYPEREFKIND_FAILABLE;
        }

        resolveFailableStruct(typeRef);
        typeRef.Kind = E_TYPEREFKIND_NORMAL;
    } else if(typeRef.Kind == E_TYPEREFKIND_FOREIGN) {
        // Support for "[T]?"
        if(typeRef.ArrayLevel > 0) {
            typeRef.Kind = E_TYPEREFKIND_NORMAL;
            if(!resolveArrayClass(typeRef)) {
                return false;
            }
            // typeRef.ArrayLevel is now 0
            // restore the 'Kind' flag
            typeRef.Kind = E_TYPEREFKIND_FOREIGN;
        }

        if(!resolveForeignProxy(typeRef)) {
            return false;
        }
        typeRef.Kind = E_TYPEREFKIND_NORMAL;
    }

    if(typeRef.ArrayLevel > 0) {
        return resolveArrayClass(typeRef);
    } else {
        return true;
    }
}

bool CDomain::resolveArrayClass(STypeRef& arrayTypeRef)
{
    SKIZO_REQ_POS(arrayTypeRef.ArrayLevel);

    CClass* foundClass;
    if(m_arrayClassMap.TryGet(arrayTypeRef, &foundClass)) {
        foundClass->Unref();

        // NOTE Important. [int] is first "int:arrayLevel=1", after resolution it's "object:arrayLevel=0"
        arrayTypeRef.ArrayLevel = 0;
        arrayTypeRef.PrimType = E_PRIMTYPE_OBJECT;
        arrayTypeRef.ResolvedClass = foundClass;
        arrayTypeRef.ClassName = foundClass->FlatName();
        // *******
        arrayTypeRef.Kind = E_TYPEREFKIND_NORMAL;
        // *******
        return true;
    }

    STypeRef subTypeRef = arrayTypeRef;
    subTypeRef.ArrayLevel--;
    subTypeRef.ResolvedClass = nullptr; // forces resolution
    subTypeRef.Kind = E_TYPEREFKIND_NORMAL;
    if(!ResolveTypeRef(subTypeRef)) {
        return false;
    }
    SKIZO_REQ_PTR(subTypeRef.ResolvedClass);

    Auto<CClass> klass (new CClass(this));
    klass->SetWrappedClass(subTypeRef);
    klass->Flags() |= E_CLASSFLAGS_IS_COMPGENERATED;
    klass->SetSpecialClass(E_SPECIALCLASS_ARRAY);

    Auto<const CString> generatedName (CString::Format("0Array_%d", NewUniqueId()));
    klass->SetFlatName(this->NewSlice(generatedName));

    m_arrayClassMap.Set(arrayTypeRef, klass);

    switch(subTypeRef.PrimType) {
        case E_PRIMTYPE_VOID:
            return false;
        case E_PRIMTYPE_INT:
            klass->SetStructDef(this->NewSlice("void** _soX_vtable;\n"
                                         "int _soX_length;\n"
                                         "int _soX_firstItem _soX_ALIGNED;\n"));
            break;
        case E_PRIMTYPE_FLOAT:
            klass->SetStructDef(this->NewSlice("void** _soX_vtable;\n"
                                         "int _soX_length;\n"
                                         "float _soX_firstItem _soX_ALIGNED;\n"));
            break;
        case E_PRIMTYPE_BOOL:
            klass->SetStructDef(this->NewSlice("void** _soX_vtable;\n"
                                         "int _soX_length;\n"
                                         "_so_bool _soX_firstItem _soX_ALIGNED;\n"));
            break;
        case E_PRIMTYPE_CHAR:
            klass->SetStructDef(this->NewSlice("void** _soX_vtable;\n"
                                         "int _soX_length;\n"
                                         "_so_char _soX_firstItem _soX_ALIGNED;\n"));
            break;
        case E_PRIMTYPE_INTPTR:
            klass->SetStructDef(this->NewSlice("void** _soX_vtable;\n"
                                         "int _soX_length;\n"
                                         "void* _soX_firstItem _soX_ALIGNED;\n"));
            break;
        case E_PRIMTYPE_OBJECT:
        {
            // **********************************************************
            // **********************************************************

            // TODO optimize, make it nicer
            SKIZO_REQ_PTR(subTypeRef.ResolvedClass);
            {
                Auto<CStringBuilder> sb (new CStringBuilder());

                // All closures share the same structure in C code to minimize C code size.
                if(subTypeRef.ResolvedClass->SpecialClass() == E_SPECIALCLASS_METHODCLASS) {
                    sb->AppendASCII("void** _soX_vtable;\n"
                                    "int _soX_length;\n"
                                    "struct _soX_0Closure");
                } else {
                    Auto<const CString> classNameAsSkizoString (subTypeRef.ClassName.ToString());
                    sb->AppendASCII("void** _soX_vtable;\n"
                                    "int _soX_length;\n"
                                    "struct _so_");
                    sb->Append(classNameAsSkizoString);
                }

                if((arrayTypeRef.ArrayLevel == 1 && !arrayTypeRef.ResolvedClass->IsValueType()) || (arrayTypeRef.ArrayLevel > 1)) {
                    sb->Append(SKIZO_CHAR('*'));
                }
                sb->AppendASCII(" _soX_firstItem _soX_ALIGNED;\n");
                Auto<const CString> cBody (sb->ToString());
                klass->SetStructDef(this->NewSlice(cBody));
            }

            // **********************************************************
            // **********************************************************

        }
        break;
        default:
            SKIZO_THROW(EC_NOT_IMPLEMENTED);
            break;
    }

    // ******************
    //   0Array_%d::get
    // ******************

    {
        Auto<CMethod> nMethod (new CMethod(klass));
        nMethod->SetName(this->NewSlice("get"));
        nMethod->Signature().ReturnType = subTypeRef;
        {
            Auto<CParam> param (new CParam());
            param->Name = this->NewSlice("index");
            param->Type.SetPrimType(E_PRIMTYPE_INT);
            nMethod->Signature().Params->Add(param);
        }

        // 0 == #define SKIZO_ERRORCODE_RANGECHECK
        // See icall.h
        nMethod->SetCBody("if(l_index < 0 || l_index >= self->_soX_length) _soX_abort0(0);\n"
                             "return (&self->_soX_firstItem)[l_index];\n");
        klass->RegisterInstanceMethod(nMethod);
    }

    // ******************
    //   0Array_%d::set
    // ******************

    {
        Auto<CMethod> nMethod (new CMethod(klass));
        nMethod->SetName(this->NewSlice("set"));
        nMethod->Signature().ReturnType = subTypeRef;
        {
            Auto<CParam> param1 (new CParam());
            param1->Name = this->NewSlice("index");
            param1->Type.SetPrimType(E_PRIMTYPE_INT);
            nMethod->Signature().Params->Add(param1);

            Auto<CParam> param2 (new CParam());
            param2->Name = this->NewSlice("value");
            param2->Type = subTypeRef;
            nMethod->Signature().Params->Add(param2);
        }

        // 0 == #define SKIZO_ERRORCODE_RANGECHECK
        // See icall.h
        nMethod->SetCBody("if(l_index < 0 || l_index >= self->_soX_length) _soX_abort0(0);\n"
                             "(&self->_soX_firstItem)[l_index] = l_value;\n");
        klass->RegisterInstanceMethod(nMethod);
    }

    // *********************
    //   0Array_%d::length
    // *********************

    {
        Auto<CMethod> nMethod (new CMethod(klass));
        nMethod->SetName(this->NewSlice("length"));
        nMethod->Signature().ReturnType.SetPrimType(E_PRIMTYPE_INT);
        // If the method is called non-dynamically, it's implemented as a fast macro.
        nMethod->SetCBody("return self->_soX_length;\n");
        klass->RegisterInstanceMethod(nMethod);
    }

    // *********************

    RegisterClass(klass);

    // NOTE Important. [int] is first "int:arrayLevel=1", after resolution it's "object:arrayLevel=0"
    arrayTypeRef.ArrayLevel = 0;
    arrayTypeRef.ClassName = klass->FlatName();
    arrayTypeRef.PrimType = E_PRIMTYPE_OBJECT;
    arrayTypeRef.ResolvedClass = klass;
    // *******
    arrayTypeRef.Kind = E_TYPEREFKIND_NORMAL;
    // *******

    return true;
}

void CDomain::resolveFailableStruct(STypeRef& typeRef)
{
    SKIZO_REQ_PTR(typeRef.ResolvedClass);
    SKIZO_REQ_EQUALS(typeRef.Kind, E_TYPEREFKIND_FAILABLE);

    const CClass* inputClass = typeRef.ResolvedClass;

    // Already resolved (just to be sure).
    if(inputClass->SpecialClass() == E_SPECIALCLASS_FAILABLE) {
        return;
    }

    CClass* foundClass;
    if(m_failableClassMap.TryGet(inputClass->FlatName(), &foundClass)) {
        foundClass->Unref();

        typeRef.ResolvedClass = foundClass;
        typeRef.ClassName = foundClass->FlatName();
        typeRef.PrimType = E_PRIMTYPE_OBJECT;
        return;
    }

    Auto<CClass> klass (new CClass(this));
    klass->SetSpecialClass(E_SPECIALCLASS_FAILABLE);
    EClassFlags& classFlags = klass->Flags();
    classFlags |= E_CLASSFLAGS_IS_COMPGENERATED;
    classFlags |= E_CLASSFLAGS_IS_VALUETYPE;
    klass->SetWrappedClass(typeRef);
    klass->WrappedClass().Kind = E_TYPEREFKIND_NORMAL;

    Auto<const CString> generatedName (CString::Format("0Failable_%d", NewUniqueId()));
    klass->SetFlatName(this->NewSlice(generatedName));

    // TODO if we use complex types here, rather cache by STypeRef instead of just the name.
    m_failableClassMap.Set(inputClass->FlatName(), klass);

    // WARNING The order is important, as other code relies on it!
    // For example, SFailableHeader

    // ********************
    //   The error field.
    // ********************

    const SStringSlice errorSlice (this->NewSlice("Error"));

    {
        Auto<CField> valueField (new CField());
        valueField->DeclaringClass = klass;
        valueField->Name = this->NewSlice("m_error");
        valueField->Type.SetObject(errorSlice);
        klass->RegisterInstanceField(valueField);
    }

    // ********************
    //   The value field.
    // ********************
    {
        Auto<CField> valueField (new CField());
        valueField->DeclaringClass = klass;
        valueField->Name = this->NewSlice("m_value");
        valueField->Type = klass->WrappedClass();
        klass->RegisterInstanceField(valueField);
    }

    // *********************************************************
    //   The constructor which creates a failable from a value.
    // *********************************************************

    {
        Auto<CMethod> ctor (new CMethod(klass));
        ctor->SetMethodKind(E_METHODKIND_CTOR);
        ctor->SetName(this->NewSlice("createFromValue"));
        ctor->Signature().ReturnType = klass->ToTypeRef();
        {
            Auto<CParam> param (new CParam());
            param->Name = this->NewSlice("_soX_value");
            param->Type = klass->WrappedClass();
            ctor->Signature().Params->Add(param);
        }
        // The language design allows inline C code, so why not use it here?
        ctor->SetCBody("self.m_value = l__soX_value;\n");
        klass->RegisterInstanceCtor(ctor);
    }

    // ***********************************************************
    //   The constructor which creates a failable from an error.
    // ***********************************************************

    {
        Auto<CMethod> ctor (new CMethod(klass));
        ctor->SetMethodKind(E_METHODKIND_CTOR);
        ctor->SetName(this->NewSlice("createFromError"));
        ctor->Signature().ReturnType = klass->ToTypeRef();
        {
            Auto<CParam> param (new CParam());
            param->Name = this->NewSlice("_soX_value");
            param->Type.SetObject(errorSlice);
            ctor->Signature().Params->Add(param);
        }
        ctor->SetCBody("self.m_error = l__soX_value;\n");
        klass->RegisterInstanceCtor(ctor);
    }

    // ******************
    //   success getter
    // ******************

    {
        Auto<CMethod> nMethod (new CMethod(klass));
        nMethod->SetName(this->NewSlice("success"));
        nMethod->Signature().ReturnType.SetPrimType(E_PRIMTYPE_BOOL);
        nMethod->SetCBody("return self.m_error == 0;\n");
        klass->RegisterInstanceMethod(nMethod);
    }

    // *************************************
    //   unwrap method
    //   NOTE: aborts if there's no value.
    // *************************************

    {
        Auto<CMethod> nMethod (new CMethod(klass));
        nMethod->SetName(this->NewSlice("unwrap"));
        nMethod->Signature().ReturnType = klass->WrappedClass();
        // 4 == #define SKIZO_ERRORCODE_FAILABLE_FAILURE
        // See icall.h
        nMethod->SetCBody("if(self.m_error) _soX_abort_e(self.m_error);\n"
                             "return self.m_value;\n");
        klass->RegisterInstanceMethod(nMethod);
    }

    // *******************
    //   error (getter).
    // *******************
    {
        Auto<CMethod> nMethod (new CMethod(klass));
        nMethod->SetName(this->NewSlice("error"));
        nMethod->Signature().ReturnType = m_errorClass->ToTypeRef();
        // 4 == #define SKIZO_ERRORCODE_FAILABLE_FAILURE
        // See icall.h
        nMethod->SetCBody("return self.m_error;\n");
        klass->RegisterInstanceMethod(nMethod);
    }

    // ********************

    RegisterClass(klass);

    // After the failable struct is resolved, it's just like any other ordinary valuetype afterwards internally.
    typeRef.ResolvedClass = klass;
    typeRef.ClassName = klass->FlatName();
    typeRef.PrimType = E_PRIMTYPE_OBJECT;
}

bool CDomain::resolveForeignProxy(STypeRef& typeRef)
{
    SKIZO_REQ_PTR(typeRef.ResolvedClass);
    CClass* inputClass = typeRef.ResolvedClass;
    SKIZO_REQ_NOT_EQUALS(inputClass->SpecialClass(), E_SPECIALCLASS_FAILABLE); // not possible by now as syntax doesn't allow

    // Already resolved (just to be sure).
    if(inputClass->SpecialClass() == E_SPECIALCLASS_FOREIGN) {
        return true;
    }

    if(inputClass == m_stringClass) {
        ScriptUtils::FailC("Strings are a special case: they're shared among domains (string* found).", inputClass);
        return false;
    }

    // Maybe we already generated a name for this class?
    CClass* foundClass;
    if(m_foreignProxyMap->TryGet(inputClass->FlatName(), &foundClass)) {
        foundClass->Unref();

        typeRef.ResolvedClass = foundClass;
        typeRef.ClassName = foundClass->FlatName();
        typeRef.PrimType = E_PRIMTYPE_OBJECT;
        return true;
    }

    // No-no to valuetypes.
    if(inputClass->IsValueType()) {
        // TODO better error
        ScriptUtils::FailC("Valuetypes can't be foreign.", inputClass);
        return false;
    }
    if(inputClass->IsStatic()) {
        // TODO better error
        ScriptUtils::FailC("Static classes can't be foreign.", inputClass);
        return false;
    }
    if(!inputClass->StructDef().IsEmpty() || inputClass->SpecialClass() == E_SPECIALCLASS_BINARYBLOB) {
        // TODO better error
        ScriptUtils::FailC("Classes with native layouts (including binary blobs) can't be foreign.", inputClass);
        return false;
    }

    // Constructs the proxy itself.
    Auto<CClass> klass (new CClass(this));
    klass->Flags() |= E_CLASSFLAGS_IS_COMPGENERATED;
    klass->SetSpecialClass(E_SPECIALCLASS_FOREIGN);

    // NOTE We also inherit from the wrapped class to make them vtable-compatible.
    klass->SetBaseClass(typeRef);
    klass->BaseClass().Kind = E_TYPEREFKIND_NORMAL;
    klass->SetWrappedClass(klass->BaseClass());

    // Generates the name and registers it.
    Auto<const CString> generatedName (CString::Format("0Foreign_%d", NewUniqueId()));
    klass->SetFlatName(this->NewSlice(generatedName));
    m_foreignProxyMap->Set(inputClass->FlatName(), klass);
    RegisterClass(klass);

    // NOTE We use "any" here because user code might not have imported "domain" module.
    // Using T* without importing "domain" would produce compile-time errors which expose
    // the internals, something like "Instance field string*::m_hdomain of unknown type 'DomainHandle'".
    const SStringSlice domainHandleSlice (this->NewSlice("any"));

    // WARNING IMPORTANT synchronize the fields with SForeignProxyHeader defined in NativeHeaders.h
    // WARNNIG Fields should be explicitly described so that the GC could track the references.
    // Emitting fields in any other ways is not an option.

    // ****************************
    //   The domain handle field.
    // ****************************

    {
        Auto<CField> hDomainField (new CField());
        hDomainField->DeclaringClass = klass;
        hDomainField->Name = this->NewSlice("m_hdomain");
        hDomainField->Type.SetObject(domainHandleSlice);
        hDomainField->Access = E_ACCESSMODIFIER_PRIVATE;
        klass->RegisterInstanceField(hDomainField);
    }

    // **************************
    //   The object name field.
    // **************************

    {
        Auto<CField> nameField (new CField());
        nameField->DeclaringClass = klass;
        nameField->Name = this->NewSlice("m_name");
        nameField->Type = m_stringClass->ToTypeRef();
        nameField->Access = E_ACCESSMODIFIER_PRIVATE;
        klass->RegisterInstanceField(nameField);
    }

    // ***************************************************************************
    // Generates synchronous method wrappers.
    // NOTE They have no bodies because they are specially handled in the emitter.
    // ***************************************************************************

    const CArrayList<CMethod*>* instanceMethods = inputClass->InstanceMethods();
    for(int i = 0; i < instanceMethods->Count(); i++) {
        const CMethod* inputMethod = instanceMethods->Array()[i];

        Auto<CMethod> newMethod (new CMethod(klass));
        newMethod->SetName(inputMethod->Name());
        newMethod->Signature().ReturnType = inputMethod->Signature().ReturnType;
        newMethod->SetSpecialMethod(E_SPECIALMETHOD_FOREIGNSYNC);

        int paramCount = inputMethod->Signature().Params->Count();
        for(int j = 0; j < paramCount; j++) {
            Auto<CParam> paramCopy (inputMethod->Signature().Params->Array()[j]->Clone());
            paramCopy->IsCaptured = false; // ?!
            paramCopy->DeclaringMethod = newMethod;

            // Some built-in icalls don't provide param names. Generate them, then (otherwise, TCC fails).
            if(paramCopy->Name.IsEmpty()) {
                paramCopy->Name = ScriptUtils::NParamName(this, j);
            }

            newMethod->Signature().Params->Add(paramCopy);
        }
        klass->RegisterInstanceMethod(newMethod);
    }

    // ***************************************************************************************************
    // After the foreign proxy is resolved, it's just like any other ordinary class afterwards internally.
    // ***************************************************************************************************

    typeRef.ResolvedClass = klass;
    typeRef.ClassName = klass->FlatName();
    typeRef.PrimType = E_PRIMTYPE_OBJECT;

    return true;
}

CClass* CDomain::BoxedClass(const STypeRef& typeRef, bool mustBeAlreadyCreated)
{
    SKIZO_REQ_PTR(typeRef.ResolvedClass);
    SKIZO_REQ(typeRef.ResolvedClass->IsValueType(), EC_ILLEGAL_ARGUMENT);

    const CClass* inputClass = typeRef.ResolvedClass;

    CClass* r;
    if(m_boxedClassMap->TryGet(inputClass->FlatName(), &r)) {
        r->Unref();
        return r;
    }

    SKIZO_REQ(!mustBeAlreadyCreated, EC_ILLEGAL_ARGUMENT);

    Auto<CClass> klass (new CClass(this));
    EClassFlags& classFlags = klass->Flags();
    classFlags |= E_CLASSFLAGS_IS_COMPGENERATED;
    classFlags |= E_CLASSFLAGS_FREE_VTABLE;
    classFlags &= ~E_CLASSFLAGS_EMIT_VTABLE;
    klass->SetSpecialClass(E_SPECIALCLASS_BOXED);
    klass->SetWrappedClass(typeRef);

    Auto<const CString> generatedName (CString::Format("0Boxed_%d", NewUniqueId()));
    klass->SetFlatName(this->NewSlice(generatedName));

    // TODO if we use complex types here, rather cache by STypeRef instead of just the name.
    m_boxedClassMap->Set(inputClass->FlatName(), klass);

    // ********************
    //   The value field.
    // ********************

    Auto<CField> valueField (new CField());
    valueField->DeclaringClass = klass;
    valueField->Name = this->NewSlice("m_value");
    valueField->Type = typeRef;
    klass->RegisterInstanceField(valueField);

    // ********************
    //   The constructor.
    // ********************

    Auto<CMethod> ctor (new CMethod(klass));
    ctor->SetMethodKind(E_METHODKIND_CTOR);
    ctor->SetName(this->NewSlice("create"));
    ctor->Signature().ReturnType = klass->ToTypeRef();
    {
        Auto<CParam> param (new CParam());
        param->Type = typeRef;
        ctor->Signature().Params->Add(param);
    }

    ctor->SetSpecialMethod(E_SPECIALMETHOD_BOXED_CTOR);
    m_thunkMngr.AddMethod(ctor);
    klass->RegisterInstanceCtor(ctor);

    // *****************************************
    //   Wraps methods of the original struct.
    // *****************************************

    const CArrayList<CMethod*>* instanceMethods = inputClass->InstanceMethods();
    for(int i = 0; i < instanceMethods->Count(); i++) {
        const CMethod* inputMethod = instanceMethods->Array()[i];

        // Ignores operators (by spec!), because they're useless for boxed objects:
        // say, int::op_equals is defined as "int::op_equals(int i"). Are we supposed to compare a by-ref
        // boxed class to a value type "int"? Instead, use "equals" which accepts "any" for generic scenarios.
        if(inputMethod->Name().StartsWithAscii("op_")) {
            continue;
        }

        Auto<CMethod> newMethod (new CMethod(klass));
        newMethod->SetName(inputMethod->Name());
        newMethod->Signature().ReturnType = inputMethod->Signature().ReturnType;

        const int paramCount = inputMethod->Signature().Params->Count();
        for(int j = 0; j < paramCount; j++) {
            Auto<CParam> paramCopy (inputMethod->Signature().Params->Array()[j]->Clone());
            paramCopy->IsCaptured = false; // ?!
            paramCopy->DeclaringMethod = newMethod;

            // Some built-in icalls don't provide param names 'coz we're lazy. Generate 'em, then (otherwise, TCC fails).
            if(paramCopy->Name.IsEmpty()) {
                Auto<const CString> newParamName (CString::Format("_soX_param_%d", j));
                paramCopy->Name = this->NewSlice(newParamName);
            }

            newMethod->Signature().Params->Add(paramCopy);
        }

        newMethod->SetSpecialMethod(E_SPECIALMETHOD_BOXED_METHOD); // special treatment by the emitter
        // TODO don't add if the linking stage is over
        m_thunkMngr.AddMethod(newMethod);
        klass->RegisterInstanceMethod(newMethod);
    }

    // ********************

    // The class can be created dynamically. Previously, this was called during the transformation phase
    // just like all the other classes.
    // Now we have to do it here, to make sure method wrappers have vtable indices assigned and such.
    klass->MakeSureMethodsFinalized();

    RegisterClass(klass);
    return klass;
}

} }

#endif // TYPERESOLUTION_H_INCLUDED
