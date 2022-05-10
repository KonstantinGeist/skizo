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

#include "Class.h"
#include "Abort.h"
#include "ArrayList.h"
#include "Const.h"
#include "Contract.h"
#include "Domain.h"
#include "ModuleDesc.h"
#include "StringBuilder.h"
#include "ScriptUtils.h"

namespace skizo { namespace script {
using namespace skizo::core;
using namespace skizo::collections;

    // ***********************
    //      Ctors & dtors.
    // ***********************

SGCInfo::SGCInfo()
    : GCMap(nullptr),
      GCMapSize(0),
      ContentSize(0),
      SizeForUse(0)
{
}

SGCInfo::~SGCInfo()
{
    if(GCMap) {
        delete [] GCMap;
    }
}

CClass::CClass(class CDomain* declaringDomain)
    : m_declaringDomain(declaringDomain),
      m_primType(E_PRIMTYPE_OBJECT),
      m_flags(E_CLASSFLAGS_EMIT_VTABLE | E_CLASSFLAGS_IS_INITIALIZED),
      m_specialClass(E_SPECIALCLASS_NONE),
      m_baseClass(0),
      m_access(E_ACCESSMODIFIER_PUBLIC),
      m_pRuntimeObj(nullptr),
      m_pvtbl(nullptr),
      m_invokeMethod(nullptr),
      m_instanceFields(new CArrayList<CField*>()),
      m_staticFields(new CArrayList<CField*>()),
      m_instanceCtors(new CArrayList<CMethod*>()),
      m_instanceMethods(new CArrayList<CMethod*>()),
      m_instanceMethodMap(new CHashMap<SStringSlice, CMethod*>()),
      m_staticMethods(new CArrayList<CMethod*>()),
      m_nameSet(new CHashMap<SStringSlice, CMember*>()),
      m_dtorImpl(nullptr),
      m_hashCodeImpl(nullptr),
      m_equalsImpl(nullptr)
{
}

CClass::~CClass()
{
    // Some vtables are generated on demand (such as in _soX_gc_alloc_env), so we deallocate
    // it here manually. Other classes link the vtables through the baseline C compiler and register them
    // by _soX_regvtable.
    if(m_pvtbl && FreeVTable()) {
        delete [] m_pvtbl;
    }

    m_instanceFields->Clear();
    m_staticFields->Clear();
    m_instanceCtors->Clear();
    m_staticCtor.SetPtr(nullptr);
    m_instanceMethods->Clear();
    m_instanceMethodMap->Clear();
    m_staticMethods->Clear();
    m_instanceDtor.SetPtr(nullptr);
    m_staticDtor.SetPtr(nullptr);
    m_consts.SetPtr(nullptr);
    m_nameSet->Clear();
}

void* CClass::operator new(size_t size)
{
    return SO_FAST_ALLOC(size, E_SKIZOALLOCATIONTYPE_CLASS);
}

void CClass::operator delete (void *p)
{
    // NOTHING
}

    // *****************************
    //        Basic data.
    // *****************************

CClass* CClass::ResolvedBaseClass() const
{
    return m_baseClass.ResolvedClass;
}

bool CClass::IsClassHierarchyRoot() const
{
    return m_baseClass.IsVoid();
}

CClass* CClass::ResolvedWrappedClass() const
{
    return m_wrappedClass.ResolvedClass;
}

bool CClass::IsErrorClass() const
{
    if(this == m_declaringDomain->ErrorClass()) {
        return true;
    } else {
        return this->IsSubclassOf(m_declaringDomain->ErrorClass());
    }
}

bool CClass::IsByValue() const
{
    return (m_primType != E_PRIMTYPE_OBJECT) || IsValueType();
}

STypeRef CClass::ToTypeRef() const
{
    STypeRef typeRef;
    if(m_primType == E_PRIMTYPE_OBJECT) {
        typeRef.SetObject(m_flatName);
    } else {
        typeRef.ClassName = m_flatName;
        typeRef.SetPrimType(m_primType);
    }
    typeRef.ResolvedClass = const_cast<CClass*>(this);
    return typeRef;
}

    // *******************************
    //        For reflection.
    // *******************************

// NOTE Special case for boxed values, arrays, failables and closures.
// NOTE The method is used by CDomainHandle::ImportObject(..) so we lock on m_memMngr.ExportedObjs
// just like it does.
// IMPORTANT Don't change anything! Remoting depends on nice names to correctly share type names among
// several domains (underlying flat names can be different).
const CString* CClass::makeSureNiceNameGenerated() const
{
    if(!m_niceName) {

        CMutex* mutex = m_declaringDomain->MemoryManager().ExportedObjsMutex;
        SKIZO_LOCK_AB(mutex) {

            if(!m_niceName) {

                if(m_specialClass == E_SPECIALCLASS_BOXED) {

                    m_wrappedClass.ResolvedClass->makeSureNiceNameGenerated();
                    m_niceName.SetVal(m_wrappedClass.ResolvedClass->m_niceName);

                } else if(m_specialClass == E_SPECIALCLASS_ARRAY) {

                    m_wrappedClass.ResolvedClass->makeSureNiceNameGenerated();

                    Auto<CStringBuilder> sb (new CStringBuilder());
                    sb->Append(SKIZO_CHAR('['));
                    sb->Append(m_wrappedClass.ResolvedClass->m_niceName);
                    sb->Append(SKIZO_CHAR(']'));
                    m_niceName.SetPtr(sb->ToString());

                } else if(m_specialClass == E_SPECIALCLASS_FAILABLE || m_specialClass == E_SPECIALCLASS_FOREIGN) {

                    SKIZO_REQ_PTR(m_wrappedClass.ResolvedClass);
                    m_wrappedClass.ResolvedClass->makeSureNiceNameGenerated();

                    Auto<CStringBuilder> sb (new CStringBuilder());
                    sb->Append(m_wrappedClass.ResolvedClass->m_niceName);
                    sb->Append(m_specialClass == E_SPECIALCLASS_FAILABLE? SKIZO_CHAR('?'): SKIZO_CHAR('*'));
                    m_niceName.SetPtr(sb->ToString());

                } else if(m_specialClass == E_SPECIALCLASS_METHODCLASS && this->IsCompilerGenerated()) {

                    SKIZO_REQ_PTR(m_source.Module);
                    m_niceName.SetPtr(CString::Format("<closure(%o:%d)>",
                                                      (CObject*)m_source.Module->FilePath,
                                                                m_source.LineNumber));

                } else {
                    m_niceName.SetPtr(m_flatName.ToString());
                }
            }

        } SKIZO_END_LOCK_AB(mutex);
    }

    return m_niceName;
}

const CString* CClass::NiceName() const
{
    return makeSureNiceNameGenerated();
}

void CClass::DefICall(const SStringSlice& name, const char* methodSig, bool forceNoHeader)
{
    Auto<CMethod> nMethod (new CMethod(this));
    nMethod->SetName(name);
    nMethod->SetMethodSig(methodSig);
    nMethod->SetSpecialMethod(E_SPECIALMETHOD_NATIVE);
    if(forceNoHeader) {
        nMethod->Flags() |= E_METHODFLAGS_FORCE_NO_HEADER;
    }

    this->RegisterInstanceMethod(nMethod);
}

    // *************************************
    //        Method call mechanisms.
    // *************************************

bool CClass::HasVTable() const
{
    return(!(this->IsStatic()
          || this->IsAbstract()
          || this->IsValueType()
          || m_primType != E_PRIMTYPE_OBJECT));
}

void* CClass::TryGetMethodImplForInterfaceMethod(CMethod* intrfcMethod) const
{
    if(!m_pIntrfcMethodToImplPtr) {
        return nullptr;
    }

    void* methodImpl;
    if(m_pIntrfcMethodToImplPtr->TryGet(intrfcMethod, &methodImpl)) {
        return methodImpl;
    } else {
        return nullptr;
    }
}

void CClass::SetMethodImplForInterfaceMethod(CMethod* intrfcMethod, void* methodImpl)
{
    if(!m_pIntrfcMethodToImplPtr) {
        m_pIntrfcMethodToImplPtr.SetPtr(new CHashMap<void*, void*>());
    }
    m_pIntrfcMethodToImplPtr->Set(intrfcMethod, methodImpl);
}

    // ***************************
    //         Members.
    // ***************************

SHashMapEnumerator<SStringSlice, CMember*> CClass::GetNameSetEnumerator() const
{
    return SHashMapEnumerator<SStringSlice, CMember*>(m_nameSet);
}

void CClass::AddToNameSet(const SStringSlice& name, CMember* member)
{
    m_nameSet->Set(name, member);
}

bool CClass::TryGetInstanceMethodByName(const SStringSlice& name, CMethod** out_method) const
{
    return m_instanceMethodMap->TryGet(name, out_method);
}

    // **************************
    //       GC-related.
    // **************************

// NOTE For simplicity and safety, all fields are aligned to the word size of the machine.
// The majority of the fields are going to be integers or pointers anyway.
// NOTE This can change in the future if we actually finish this project.
void CClass::CalcGCMap()
{
    if(IsSizeCalculated() || IsStatic()) {
        return;
    }

    // NOTE Array classes have special semantics.
    if(m_specialClass == E_SPECIALCLASS_ARRAY) {
        m_flags |= E_CLASSFLAGS_IS_SIZE_CALCULATED;
        m_gcInfo.SizeForUse = sizeof(void*);
        return;
    } else if(m_specialClass == E_SPECIALCLASS_BINARYBLOB) {
        // Binary blobs are set their explicit sizes already by the parser.
        return;
    }

    // ******************************************************************
    m_flags |= E_CLASSFLAGS_IS_SIZE_CALCULATED;
    m_gcInfo.SizeForUse = sizeof(void*);
    // ******************************************************************

    if(m_primType != E_PRIMTYPE_OBJECT) {
        switch(m_primType) {
            case E_PRIMTYPE_INT:
                m_gcInfo.SizeForUse = sizeof(int);
                m_gcInfo.ContentSize = sizeof(void*);
                return;
            case E_PRIMTYPE_FLOAT:
                m_gcInfo.SizeForUse = sizeof(float);
                m_gcInfo.ContentSize = sizeof(void*);
                return;
            case E_PRIMTYPE_BOOL:
                m_gcInfo.SizeForUse = sizeof(_so_bool);
                m_gcInfo.ContentSize = sizeof(void*);
                return;
            case E_PRIMTYPE_CHAR:
                m_gcInfo.SizeForUse = sizeof(_so_char);
                m_gcInfo.ContentSize = sizeof(void*);
                return;
            case E_PRIMTYPE_INTPTR:
                m_gcInfo.SizeForUse = sizeof(void*);
                m_gcInfo.ContentSize = sizeof(void*);
                return;
            case E_PRIMTYPE_VOID:
                return;
            default:
                SKIZO_THROW(EC_NOT_IMPLEMENTED);
                break;
        }
        return;
    }

    int offset = 0;

    // If it's a struct, it has no vtable field.
    // If the class has a base class, then the vtable is already omitted there.
    // Also if the base class has no gcmap, then no offset was generated there, therefore no vtable set.
    if(!IsValueType()) {
        if(m_baseClass.IsVoid() || (!m_baseClass.ResolvedClass->m_gcInfo.GCMap)) {
            offset += sizeof(void*); // Offset for the vtable.
        }
    }

    // Recursively makes sure all the referenced classes in fields and base classes have their maps calculated, too.
    // NOTE Foreign proxies don't inherit fields (unlike vtables), so we ignore it here.
    if(!m_baseClass.IsVoid() && m_specialClass != E_SPECIALCLASS_FOREIGN) {
        m_baseClass.ResolvedClass->CalcGCMap();
    }

    for(int i = 0; i < m_instanceFields->Count(); i++) {
        CField* field = m_instanceFields->Array()[i];
        CClass* pFieldClass = field->Type.ResolvedClass;

        // **************
        // TODO? FIX
        // **************
        if(!pFieldClass) {
            m_declaringDomain->ResolveTypeRef(field->Type);
            pFieldClass = field->Type.ResolvedClass;;
            SKIZO_REQ_PTR(pFieldClass);
        }
        // **************

        pFieldClass->CalcGCMap();
    }

    Auto<CArrayList<int> > gcMap;

    // Prepends the offsets of the base class.
    // NOTE Foreign proxies don't inherit fields (unlike vtables), so we ignore it here.
    if(!m_baseClass.IsVoid() && m_specialClass != E_SPECIALCLASS_FOREIGN) {
        if(m_baseClass.ResolvedClass->m_gcInfo.GCMapSize) {
            if(!gcMap) {
                gcMap.SetPtr(new CArrayList<int>());
            }

            for(int i = 0; i < m_baseClass.ResolvedClass->m_gcInfo.GCMapSize; i++) {
                gcMap->Add(m_baseClass.ResolvedClass->m_gcInfo.GCMap[i]);
            }

            offset = m_baseClass.ResolvedClass->m_gcInfo.ContentSize;
        }
    }

    // Calculates offsets and stores them in the GC map.
    for(int i = 0; i < m_instanceFields->Count(); i++) {
        CField* field = m_instanceFields->Array()[i];
        SKIZO_REQ_PTR(field->Type.ResolvedClass);
        CClass* pFieldClass = field->Type.ResolvedClass;

        field->Offset = offset;

        if(field->Type.IsHeapClass()) {
            if(!gcMap) {
                gcMap.SetPtr(new CArrayList<int>());
            }

            gcMap->Add(offset);
        } else if(field->Type.IsStructClass()) {
            // Structs are inlined into their parents.

            if(pFieldClass->m_gcInfo.GCMap) {
                if(!gcMap) {
                    gcMap.SetPtr(new CArrayList<int>());
                }

                for(int j = 0; j < pFieldClass->m_gcInfo.GCMapSize; j++) {
                    gcMap->Add(offset + pFieldClass->m_gcInfo.GCMap[j]);
                }
            }
        }

        offset += pFieldClass->IsByValue()? pFieldClass->m_gcInfo.ContentSize: sizeof(void*);
    }

    m_gcInfo.ContentSize = offset;

    if(IsValueType()) {
        m_gcInfo.SizeForUse = offset;
    } else {
        m_gcInfo.SizeForUse = sizeof(void*);
    }

    // *****************************************
    if(gcMap) {
        m_gcInfo.GCMapSize = gcMap->Count();
        m_gcInfo.GCMap = new int[m_gcInfo.GCMapSize];
        for(int i = 0; i < gcMap->Count(); i++) {
            m_gcInfo.GCMap[i] = gcMap->Array()[i];
        }
    }
}

static SKIZO_API void closure_dtor(void* soObj)
{
    SClosureHeader* header = (SClosureHeader*)soObj;

    if(header->codeOffset) {
        CDomain::ForCurrentThread()->ThunkManager().FreeThunk(header->codeOffset);
        header->codeOffset = nullptr;
    }
}

// Extracts a pointer to machine code.
void* CClass::DtorImpl() const
{
    if(!m_dtorImpl) {
        if(m_specialClass == E_SPECIALCLASS_METHODCLASS) {
            // Special case for closures: have to cleanup m_codeOffset if any.
            m_dtorImpl = (void*)closure_dtor;
        } else {
            SKIZO_REQ_PTR(m_instanceDtor);

            // TODO performance?
            Auto<CStringBuilder> sb (new CStringBuilder());
            sb->Append(const_cast<char*>("_so_"));
            sb->Append(m_flatName.String, m_flatName.Start, m_flatName.End - m_flatName.Start);
            sb->Append(const_cast<char*>("_dtor"));

            Auto<const CString> daStr (sb->ToString());
            Utf8Auto utf8Str (daStr->ToUtf8());

            m_dtorImpl = m_declaringDomain->GetSymbol(utf8Str);
            SKIZO_REQ_PTR(m_dtorImpl);
        }
    }

    return m_dtorImpl;
}

    // ************************
    //   Member registration.
    // ************************

void CClass::RegisterInstanceMethod(CMethod* method)
{
    SKIZO_REQ_EQUALS(method->MethodKind(), E_METHODKIND_NORMAL);
    SKIZO_REQ(!method->Signature().IsStatic, EC_ILLEGAL_ARGUMENT);
    SKIZO_REQ(!method->Name().IsEmpty(), EC_ILLEGAL_ARGUMENT);

    m_instanceMethods->Add(method);
    m_nameSet->Set(method->Name(), method);

    // !
    m_instanceMethodMap->Set(method->Name(), method);
}

bool CClass::TryRegisterInstanceMethod(CMethod* method)
{
    if(m_nameSet->Contains(method->Name())) {
        return false;
    }

    this->RegisterInstanceMethod(method);
    return true;
}

void CClass::RegisterStaticMethod(CMethod* method)
{
    SKIZO_REQ_EQUALS(method->MethodKind(), E_METHODKIND_NORMAL);
    SKIZO_REQ(method->Signature().IsStatic, EC_ILLEGAL_ARGUMENT);
    SKIZO_REQ(!method->Name().IsEmpty(), EC_ILLEGAL_ARGUMENT);

    m_staticMethods->Add(method);
    m_nameSet->Set(method->Name(), method);
}

bool CClass::TryRegisterStaticMethod(CMethod* method)
{
    if(m_nameSet->Contains(method->Name())) {
        return false;
    }

    this->RegisterStaticMethod(method);
    return true;
}

void CClass::RegisterInstanceCtor(CMethod* method)
{
    SKIZO_REQ_EQUALS(method->MethodKind(), E_METHODKIND_CTOR);

    m_instanceCtors->Add(method);
    m_nameSet->Set(method->Name(), method);
}

void CClass::RegisterInstanceField(CField* field)
{
    SKIZO_REQ(!field->Name.IsEmpty(), EC_ILLEGAL_ARGUMENT);
    SKIZO_REQ(!field->IsStatic, EC_ILLEGAL_ARGUMENT);

    m_instanceFields->Add(field);
    m_nameSet->Set(field->Name, field);
}

void CClass::RegisterStaticField(CField* field)
{
    SKIZO_REQ(!field->Name.IsEmpty(), EC_ILLEGAL_ARGUMENT);
    SKIZO_REQ(field->IsStatic, EC_ILLEGAL_ARGUMENT);

    m_staticFields->Add(field);
    m_nameSet->Set(field->Name, field);
}

void CClass::RegisterConst(CConst* konst)
{
    if(!m_consts) {
        m_consts.SetPtr(new CArrayList<CConst*>());
    }
    m_consts->Add(konst);
    this->VerifyUniqueMemberName(konst->Name);
    m_nameSet->Set(konst->Name, konst);
}

void CClass::AddInstanceCtor(CMethod* method)
{
    m_instanceCtors->Add(method);
}

void CClass::AddStaticMethod(CMethod* method)
{
    m_staticMethods->Add(method);
}

void CClass::AddInstanceMethod(CMethod* method)
{
    m_instanceMethods->Add(method);
}

void CClass::AddInstanceField(CField* field)
{
    m_instanceFields->Add(field);
}

void CClass::AddStaticField(CField* field)
{
    m_staticFields->Add(field);
}

    // **********************
    //   Member resolution.
    // **********************

CMethod* CClass::StaticMethodOrCtor(const SStringSlice& name) const
{
    CMethod* method = this->MyMethod(name, true, E_METHODKIND_NORMAL);
    if(!method) {
        method = this->MyMethod(name, false, E_METHODKIND_CTOR);
    }
    return method;
}

CField* CClass::MyField(const SStringSlice& name, bool isStatic) const
{
    CMember* member;
    if(m_nameSet->TryGet(name, &member)) {
        member->Unref();

        if(member->MemberKind() == E_RUNTIMEOBJECTKIND_FIELD) {
            CField* field = static_cast<CField*>(member);
            if(field->IsStatic == isStatic) {
                return field;
            }
        }
    }

    if(!m_baseClass.IsVoid()) {
        return m_baseClass.ResolvedClass->MyField(name, isStatic);
    }

    return nullptr;
}

CMethod* CClass::MyMethod(const SStringSlice& name, bool isStatic, EMethodKind methodKind) const
{
    CMember* member;
    if(m_nameSet->TryGet(name, &member)) {
        member->Unref();

        if(member->MemberKind() == E_RUNTIMEOBJECTKIND_METHOD) {
            CMethod* method = static_cast<CMethod*>(member);

            if(methodKind == method->MethodKind()) {
                if(methodKind == E_METHODKIND_CTOR) {
                    return method;
                }

                if(method->Signature().IsStatic == isStatic) {
                    return method;
                }
            }
        }
    }

    // *********************************************
    // If nothing found, try expand it to operators.
    // *********************************************

    if(!isStatic && methodKind == E_METHODKIND_NORMAL) {
        SStringSlice operatorName;
        operatorName = PrimitiveOperatorToNeutralName(name, m_declaringDomain);
        // Try again.
        if(!operatorName.IsEmpty()) {
            return this->MyMethod(operatorName, false, E_METHODKIND_NORMAL);
        }
    }

    // *********************************************

    if(methodKind == E_METHODKIND_NORMAL && !m_baseClass.IsVoid()) {
        return m_baseClass.ResolvedClass->MyMethod(name, isStatic, methodKind);
    }

    return nullptr;
}

CConst* CClass::MyConst(const SStringSlice& name) const
{
    CMember* probablyConst;
    if(m_nameSet->TryGet(name, &probablyConst)) {
        probablyConst->Unref();

        if(probablyConst->MemberKind() == E_RUNTIMEOBJECTKIND_CONST) {
            return static_cast<CConst*>(probablyConst);
        } else {
            return nullptr;
        }
    } else {
        return nullptr;
    }
}

    // **************************
    //       Auxiliaries.
    // **************************

void CClass::CheckCyclicDependencyInHierarchy(const CClass* pStartBase) const
{
    if(pStartBase == this) {
        ScriptUtils::FailC("Cyclic dependency found in this class.", this);
    } else {
        if(pStartBase->m_baseClass.ResolvedClass) {
            this->CheckCyclicDependencyInHierarchy(pStartBase->m_baseClass.ResolvedClass);
        }
    }
}

// Takes virtual methods of the parent class and prepends them to the list of virtual methods defined
// in this (child) class. Moves overridden virtual methods from their initial location to the locations
// in the prepended region -- to ensure that methods with the same name end up at the same indices in the
// vtable.
void CClass::MakeSureMethodsFinalized()
{
    if(!this->IsMethodListFinalized()) {
        if(!m_baseClass.IsVoid()) {
            CClass* pBaseClass = m_baseClass.ResolvedClass;
            pBaseClass->MakeSureMethodsFinalized();

            Auto<CArrayList<CMethod*> > newMethodList (new CArrayList<CMethod*>());
            for(int i = 0; i < pBaseClass->m_instanceMethods->Count(); i++) {
                newMethodList->Add(pBaseClass->m_instanceMethods->Array()[i]);
            }

            for(int i = 0; i < m_instanceMethods->Count(); i++) {
                CMethod* newMethod = m_instanceMethods->Array()[i];

                // Looks if there's a method with a similar name in the parent.
                // TODO quadratic complexity
                int baseMethodIndex = -1;
                for(int j = 0; j < newMethodList->Count(); j++) {
                    CMethod* oldMethod = newMethodList->Array()[j];

                    if(newMethod->Name().Equals(oldMethod->Name())) {

                        // When we override stuff, we must be sure that the methods have same signature.
                        if(!newMethod->Signature().Equals(&oldMethod->Signature())) {
                            ScriptUtils::FailM("Overriden and base methods have different signatures.", newMethod);
                            return;
                        }
                        if(oldMethod->Access() == E_ACCESSMODIFIER_PRIVATE) {
                            ScriptUtils::FailM("Can't override a private method.", newMethod);
                            return;
                        }

                        newMethod->Flags() |= E_METHODFLAGS_IS_TRULY_VIRTUAL;
                        oldMethod->Flags() |= E_METHODFLAGS_IS_TRULY_VIRTUAL;

                        newMethod->SetBaseMethod(oldMethod);

                        baseMethodIndex = j;
                        newMethod->SetVTableIndex(j);

                        // Attributes of the base are inherited by overriden methods.
                        // Rationale:
                        //  * if a method class is marked as STDCALL, closures can share this attribute
                        //  * fields defined in a base class, although visible from inside a subclass, have all the
                        //    attributes defined in the base class
                        if(oldMethod->Attributes()) {
                            newMethod->AddAttributes(oldMethod->Attributes());
                        }

                        goto exitInnerLoop;
                    }
                }

            exitInnerLoop:
                if(baseMethodIndex != -1) {
                    newMethodList->Set(baseMethodIndex, newMethod);
                } else {
                    // The method wasn't overridden => set its vtable index to be after all the inheritable methods.
                    newMethod->SetVTableIndex(newMethodList->Count());
                    newMethodList->Add(newMethod);
                }
            }

            m_instanceMethods->Clear();
            m_instanceMethods->AddRange(newMethodList);

        } else {
            for(int i = 0; i < m_instanceMethods->Count(); i++) {
                m_instanceMethods->Array()[i]->SetVTableIndex(i);
            }
        }

        // Copies all the virtual methods into a hashmap for faster interface calls.
        // NOTE Population of the method map is done in CDomain::::boxedClass for boxed
        // classes, because they can be dynamically generated at runtime. If we're, we're
        // in the compilation phase.
        if(m_specialClass != E_SPECIALCLASS_BOXED) {
            for(int i = 0; i < m_instanceMethods->Count(); i++) {
                CMethod* instanceMethod = m_instanceMethods->Array()[i];

                SKIZO_REQ_EQUALS(instanceMethod->VTableIndex(), i);
                m_instanceMethodMap->Set(instanceMethod->Name(), instanceMethod);
                m_nameSet->Set(instanceMethod->Name(), instanceMethod);
            }
        }

        // Checks for non-overriden abstract methods if the class is non-abstract.
        if(!IsAbstract()) {
            for(int i = 0; i < m_instanceMethods->Count(); i++) {
                const CMethod* instanceMethod = m_instanceMethods->Array()[i];

                if(instanceMethod->IsAbstract()) {
                    ScriptUtils::FailM(m_declaringDomain->FormatMessage("A non-abstract class '%C' doesn't implement abstract method '%s' defined in base class '%C'.",
                                                                               this, &instanceMethod->Name(), instanceMethod->DeclaringClass()),
                                       instanceMethod);
                }
            }
        }

        m_flags |= E_CLASSFLAGS_IS_METHODLIST_FINALIZED;
    }
}

void CClass::BorrowAttributes()
{
    if(!this->AttributesBorrowed()) {
        if(!m_baseClass.IsVoid()) {
            m_baseClass.ResolvedClass->BorrowAttributes();

            if(m_baseClass.ResolvedClass->m_attrs) {
                this->AddAttributes(m_baseClass.ResolvedClass->m_attrs);
            }
        }

        m_flags |= E_CLASSFLAGS_ATTRIBUTES_BORROWED;
    }
}

void CClass::AddAttributes(const CArrayList<CAttribute*>* attrs)
{
    if(!m_attrs) {
        m_attrs.SetPtr(new CArrayList<CAttribute*>());
    }
    m_attrs->AddRange(attrs);
}

const CArrayList<CAttribute*>* CClass::Attributes() const
{
    return m_attrs.Ptr();
}

bool CClass::HasBaseDtors() const
{
    for(const CClass* baseClass = m_baseClass.ResolvedClass;
        baseClass;
        baseClass = baseClass->m_baseClass.ResolvedClass)
    {
        if(baseClass->m_instanceDtor) {
            return true;
        }
    }

    return false;
}

void CClass::GetMapMethods(void* obj, FHashCode* out_hashCode, FEquals* out_equals) const
{
    if(m_hashCodeImpl && m_equalsImpl) {
        *out_hashCode = m_hashCodeImpl;
        *out_equals = m_equalsImpl;
        return;
    }

    *out_hashCode = nullptr;
    *out_equals = nullptr;

    for(int i = 0; i < m_instanceMethods->Count(); i++) {
        const CMethod* pMethod = m_instanceMethods->Array()[i];

        if(pMethod->Signature().Params->Count() == 0
        && pMethod->Signature().ReturnType.PrimType == E_PRIMTYPE_INT
        && pMethod->Name().EqualsAscii("hashCode"))
        {
            *out_hashCode = m_hashCodeImpl = (FHashCode)so_virtmeth_of(obj, i);
        }

        if(pMethod->Signature().ReturnType.PrimType == E_PRIMTYPE_BOOL
        && pMethod->Signature().Params->Count() == 1
        && (pMethod->Signature().Params->Item(0)->Type.ResolvedClass->m_flatName.EqualsAscii("any"))
        && pMethod->Name().EqualsAscii("equals"))
        {
            *out_equals = m_equalsImpl = (FEquals)so_virtmeth_of(obj, i);
        }
    }
}

// NOTE This is an abstract class: m_codeOffset isn't generated for here.
CClass* CClass::CreateIncompleteMethodClass(CDomain* pDomain)
{
    Auto<CClass> methodClass (new CClass(pDomain));
    methodClass->SetSpecialClass(E_SPECIALCLASS_METHODCLASS);
    methodClass->Flags() |= E_CLASSFLAGS_IS_ABSTRACT;

    Auto<CMethod> method (new CMethod(methodClass));
    method->SetName(pDomain->NewSlice("invoke"));
    method->Flags() |= E_METHODFLAGS_IS_ABSTRACT;

    methodClass->SetInvokeMethod(method);
    methodClass->RegisterInstanceMethod(method);

    methodClass->Ref();
    return methodClass;
}

    // ***************************************
    //    Type compatibility & consistency.
    // ***************************************

bool CClass::IsMethodClassCompatibleSig(const CMethod* pMethod) const
{
    SKIZO_REQ((m_specialClass == E_SPECIALCLASS_METHODCLASS) && m_invokeMethod, EC_INVALID_STATE);
    return m_invokeMethod->Signature().Equals(&pMethod->Signature());
}

SCastInfo CClass::GetCastInfo(const CClass* other) const
{
    SCastInfo castInfo;

    if(m_specialClass == E_SPECIALCLASS_FAILABLE
    && (this->m_wrappedClass.ResolvedClass == other))
    {

        // A conversion like:
        //   f: float? = 0.0;

        castInfo.IsCastable = true;
        castInfo.CastType = E_CASTTYPE_VALUE_TO_FAILABLE;

    } else if((m_specialClass == E_SPECIALCLASS_FAILABLE) && (other->IsErrorClass())) {

        // A conversion like:
        //   f: float? = (Error create "Unexpected input.");

        castInfo.IsCastable = true;
        castInfo.CastType = E_CASTTYPE_ERROR_TO_FAILABLE;

    } else {
        if(this->IsValueType() || other->IsValueType()) {

            // ***********
            //   STRUCT
            // ***********

            if(this == other) {
                castInfo.IsCastable = true;
                castInfo.CastType = E_CASTTYPE_UPCAST;
            } else if(m_specialClass == E_SPECIALCLASS_INTERFACE) {
                // struct=>interface (boxing)
                if(other->DoesImplementInterface(this)) {
                    castInfo.IsCastable = true;
                    castInfo.CastType = E_CASTTYPE_BOX;
                }
            } else if(other->m_specialClass == E_SPECIALCLASS_INTERFACE) {
                // interface=>struct (unboxing)
                if(this->DoesImplementInterface(other)) {
                    castInfo.IsCastable = true;
                    castInfo.CastType = E_CASTTYPE_UNBOX;
                }
            }

        } else {

            // *********
            //   CLASS
            // *********

            if(this == other) {
                castInfo.IsCastable = true;
            } else if(this->IsSubclassOf(other)) {
                castInfo.IsCastable = true;
                castInfo.CastType = E_CASTTYPE_DOWNCAST;
            } else if(other->IsSubclassOf(this)) {
                castInfo.IsCastable = true;
                castInfo.CastType = E_CASTTYPE_UPCAST;
            } else if((m_specialClass == E_SPECIALCLASS_INTERFACE) && other->DoesImplementInterface(this)) {
                castInfo.IsCastable = true;
                castInfo.CastType = E_CASTTYPE_UPCAST;
            } else if((other->m_specialClass == E_SPECIALCLASS_INTERFACE) && this->DoesImplementInterface(other)) {
                castInfo.IsCastable = true;
                castInfo.CastType = E_CASTTYPE_DOWNCAST;
            }

        }
    }

    // Void (= failure) if not set.
    return castInfo;
}

bool CClass::Is(const CClass* other) const
{
    if(this == other) {
        return true;
    } else if(other->m_specialClass == E_SPECIALCLASS_INTERFACE) {
        return this->DoesImplementInterface(other);
    } else {
        return this->IsSubclassOf(other);
    }
}

bool CClass::IsSubclassOf(const CClass* other) const
{
    if(this == other) {
        return false;
    } else {
        if(!this->m_baseClass.IsVoid()) {
            if(this->m_baseClass.ResolvedClass == other) {
                return true;
            } else {
                return this->m_baseClass.ResolvedClass->IsSubclassOf(other);
            }
        } else {
            return false;
        }
    }
}

// NOTE: boxed classes already have copies of the methods of the class they wrap, so they
// automatically inherit interfaces of their wrappees too.
bool CClass::DoesImplementInterfaceNoCache(const CClass* intrfc) const
{
    SKIZO_REQ_EQUALS(intrfc->m_specialClass, E_SPECIALCLASS_INTERFACE);

    for(int i = 0; i < intrfc->m_instanceMethods->Count(); i++) {
        const CMethod* interfaceMethod = intrfc->m_instanceMethods->Array()[i];
        CMethod* myMethod = nullptr;

        if(this->m_instanceMethodMap->TryGet(interfaceMethod->Name(), &myMethod)) {
            myMethod->Unref();

            if(myMethod->Access() == E_ACCESSMODIFIER_PRIVATE || myMethod->Access() == E_ACCESSMODIFIER_PROTECTED) {
                return false; // the method isn't accessible
            }
            if(myMethod->Access() == E_ACCESSMODIFIER_INTERNAL) {
                // some built-in classes may have no module assigned
                if(!myMethod->Source().Module || !interfaceMethod->Source().Module) {
                    return false; // regarded as non-accessible
                }
                if(!myMethod->Source().Module->Matches(interfaceMethod->Source().Module)) {
                    return false;
                }
            }

            if(!interfaceMethod->Signature().Equals(&myMethod->Signature())) {
                return false;
            }
        } else {
            return false;
        }
    }

    return true;
}

bool CClass::DoesImplementInterface(const CClass* intrfc) const
{
    const void* pIntrfc = static_cast<const void*>(intrfc);

    bool r;
    if(m_interfaceCache && m_interfaceCache->TryGet(pIntrfc, &r)) {
        return r;
    } else {
        if(!m_interfaceCache) {
            m_interfaceCache.SetPtr(new CHashMap<const void*, bool>());
        }

        bool r = this->DoesImplementInterfaceNoCache(intrfc);
        m_interfaceCache->Set(pIntrfc, r);
        return r;
    }
}

// Class context: resolves fields/methods/consts/classes.
SResolvedIdentType CClass::ResolveIdent(const SStringSlice& ident)
{
    SResolvedIdentType r;

    CMember* member;
    if(m_nameSet->TryGet(ident, &member)) {
        member->Unref();

        switch(member->MemberKind()) {
            case E_RUNTIMEOBJECTKIND_FIELD:
                r.EType = E_RESOLVEDIDENTTYPE_FIELD;
                r.AsField_ = static_cast<CField*>(member);
                return r;

            case E_RUNTIMEOBJECTKIND_METHOD:
                r.EType = E_RESOLVEDIDENTTYPE_METHOD;
                r.AsMethod_ = static_cast<CMethod*>(member);
                return r;

            case E_RUNTIMEOBJECTKIND_CONST:
                r.EType = E_RESOLVEDIDENTTYPE_CONST;
                r.AsConst_ = static_cast<CConst*>(member);
                return r;

            default: SKIZO_REQ_NEVER; break;
        }
    }

    // Tries to match against a class name.
    CClass* klass = m_declaringDomain->ClassByFlatName(ident);
    if(klass) {
        r.EType = E_RESOLVEDIDENTTYPE_CLASS;
        r.AsClass_ = klass;
        return r;
    }

    return r; // void
}

void CClass::VerifyUniqueMemberName(const SStringSlice& memberName) const
{
    if(m_nameSet->Contains(memberName)) {
        const SStringSlice tmp (memberName);
        ScriptUtils::FailC(m_declaringDomain->FormatMessage("Class member '%C::%s' defined more than once.", this, &tmp), this);
    }
}

bool CClass::IsMemberDefined(const char* name) const
{
    return m_nameSet->Contains(m_declaringDomain->NewSlice(name));
}

    // ****************************************
    //   Attribute-controled code generation
    // ****************************************

bool CClass::IsPtrWrapper() const
{
    if(m_attrs) {
        for(int i = 0; i < m_attrs->Count(); i++) {
            const CAttribute* attr = m_attrs->Array()[i];

            if(attr->Name.EqualsAscii("ptrWrapper")) {
                if(!attr->Value.IsEmpty()) {
                    // TODO replace with FailA (A for Attribute)
                    ScriptUtils::FailC("'ptrWrapper' attribute must have no value.", this);
                    return false;
                }

                if(IsValueType()) {
                    // TODO replace with FailA (A for Attribute)
                    ScriptUtils::FailC("'ptrWrapper' attribute not allowed for valuetypes.", this);
                    return false;
                }

                return true;
            }
        }
    }

    return false;
}

void CClass::AddPtrWrapperMembers()
{
    SKIZO_REQ(!IsInferred(), EC_INVALID_STATE);

    if(!this->IsMemberDefined("m_ptr")) {
        Auto<CField> fld (new CField());
        fld->Access = E_ACCESSMODIFIER_PRIVATE;
        fld->DeclaringClass = this;
        fld->Name = m_declaringDomain->NewSlice("m_ptr");
        fld->Type.SetPrimType(E_PRIMTYPE_INTPTR);
        this->RegisterInstanceField(fld);
    }

    if(!this->IsMemberDefined("createImpl")) {
        Auto<CMethod> nMethod (new CMethod(this));
        nMethod->SetAccess(E_ACCESSMODIFIER_PRIVATE);
        nMethod->SetName(m_declaringDomain->NewSlice("createImpl"));
        nMethod->Signature().IsStatic = true;
        nMethod->Signature().ReturnType.SetPrimType(E_PRIMTYPE_INTPTR);
        nMethod->SetSpecialMethod(E_SPECIALMETHOD_NATIVE);
        this->RegisterStaticMethod(nMethod);
    }

    if(!this->IsMemberDefined("destroyImpl")) {
        Auto<CMethod> nMethod (new CMethod(this));
        nMethod->SetAccess(E_ACCESSMODIFIER_PRIVATE);
        nMethod->SetName(m_declaringDomain->NewSlice("destroyImpl"));
        nMethod->Signature().IsStatic = true;
        nMethod->SetSpecialMethod(E_SPECIALMETHOD_NATIVE);
        {
            Auto<CParam> param1 (new CParam());
            param1->Type.SetPrimType(E_PRIMTYPE_INTPTR);
            nMethod->Signature().Params->Add(param1);
        }
        this->RegisterStaticMethod(nMethod);
    }

    if(!this->IsMemberDefined("create")) {
        Auto<CMethod> ctor (new CMethod(this));
        ctor->SetMethodKind(E_METHODKIND_CTOR);
        ctor->SetName(m_declaringDomain->NewSlice("create"));
        ctor->Signature().ReturnType = this->ToTypeRef();

        Auto<const CString> tmp (m_flatName.ToString());
        tmp.SetPtr(CString::Format("self->_so_%o_m_ptr = _so_%o_createImpl();\n",
                                   (const CObject*)tmp, (const CObject*)tmp));
        ctor->SetCBody(tmp);

        this->RegisterInstanceCtor(ctor);
    }

    if(!m_instanceDtor) {
        Auto<CMethod> dtor (new CMethod(this));
        dtor->SetMethodKind(E_METHODKIND_DTOR);
        dtor->SetName(m_declaringDomain->NewSlice("dtor"));

        Auto<const CString> tmp (m_flatName.ToString());
        tmp.SetPtr(CString::Format("_so_%o_destroyImpl(self->_so_%o_m_ptr);\n",
                                   (const CObject*)tmp, (const CObject*)tmp));
        dtor->SetCBody(tmp);

        m_instanceDtor.SetVal(dtor);
    }
}

void CClass::AddAccessMethodsForField(CField* pField,
                                      const SStringSlice& propertyName,
                                      EAccessModifier access,
                                      bool forceGetterOnly)
{
    // Generates the getter method.

    {
        this->VerifyUniqueMemberName(propertyName);

        Auto<CMethod> nMethod (new CMethod(this));
        nMethod->SetAccess(access);
        nMethod->SetName(propertyName);
        nMethod->Signature().ReturnType = pField->Type;
        nMethod->Signature().IsStatic = pField->IsStatic;

        // NOTE the getter inherits the attributes of the field.
        if(pField->Attributes) {
            nMethod->AddAttributes(pField->Attributes);
        }

        Auto<CBodyExpression> bodyExpr (new CBodyExpression());
        Auto<CReturnExpression> retExpr (new CReturnExpression());
        retExpr->Expr.SetPtr(new CIdentExpression(pField->Name));
        bodyExpr->Exprs->Add(retExpr);
        nMethod->SetExpression(bodyExpr);

        if(pField->IsStatic) {
            this->RegisterStaticMethod(nMethod);
        } else {
            this->RegisterInstanceMethod(nMethod);
        }
    }

    // Generates the setter method.
    // NOTE None for valuetypes because it's explicitly disallowed to change fields of immutable valuetypes outside of
    // constructors (it doesn't even make sense anyway).

    if(!IsValueType() && !forceGetterOnly) {

        Auto<CMethod> nMethod (new CMethod(this));
        nMethod->SetAccess(access);

        // NOTE the setter inherits the attributes of the field.
        if(pField->Attributes) {
            nMethod->AddAttributes(pField->Attributes);
        }

        // A special case for bool properties that start with "is", we want "setAlive" instead of "setIsAlive"
        int offset = 0;
        if(pField->Type.PrimType == E_PRIMTYPE_BOOL) {
            if((propertyName.End - propertyName.Start) > 2
            && (propertyName.String->Chars()[propertyName.Start] == SKIZO_CHAR('i'))
            && (propertyName.String->Chars()[propertyName.Start + 1] == SKIZO_CHAR('s')))
            {
                offset = 2;
            }
        }

		// **********************************************************************************************
        // Generates the name of the setter.
        // TODO optimize?
		{
			Auto<CStringBuilder> sb (new CStringBuilder());
			sb->Append((char*)"set");
			sb->Append(CoreUtils::CharToUpperCase(propertyName.String->Chars()[propertyName.Start + offset]));

			const int lengthOfTheRest = propertyName.End - propertyName.Start - 1 - offset;
			if(lengthOfTheRest) {
				sb->Append(propertyName.String, propertyName.Start + 1 + offset, lengthOfTheRest);
            }
			nMethod->SetName(m_declaringDomain->NewSlice(sb));
			this->VerifyUniqueMemberName(nMethod->Name());
		}
		// **********************************************************************************************

        SStringSlice valueSS (m_declaringDomain->NewSlice("_0value"));
        {
            Auto<CParam> param1 (new CParam());
            param1->DeclaringMethod = nMethod;
            param1->Name = valueSS;
            param1->Type = pField->Type;
            nMethod->Signature().Params->Add(param1);
        }
        nMethod->Signature().IsStatic = pField->IsStatic;

        Auto<CBodyExpression> bodyExpr (new CBodyExpression());
        Auto<CAssignmentExpression> assExpr (new CAssignmentExpression());
        assExpr->Expr1.SetPtr(new CIdentExpression(pField->Name));
        assExpr->Expr2.SetPtr(new CIdentExpression(valueSS));
        bodyExpr->Exprs->Add(assExpr);
        nMethod->SetExpression(bodyExpr);

        if(pField->IsStatic) {
            this->RegisterStaticMethod(nMethod);
        } else {
            this->RegisterInstanceMethod(nMethod);
        }
    }
}

void CClass::AddEventField(CField* pField)
{
    if(!m_eventFields) {
        m_eventFields.SetPtr(new CArrayList<CField*>());
    }
    m_eventFields->Add(pField);
}

const CArrayList<CField*>* CClass::EventFields() const
{
    return m_eventFields.Ptr();
}

void CClass::ClearEventFields()
{
    m_eventFields.SetPtr(nullptr);
}

bool CClass::TryGetIntAttribute(const char* attrName,
                                CAttribute** out_attr,
                                int* out_int,
                                bool failIfTypesDontMatch) const
{
    if(m_attrs) {
        for(int i = 0; i < m_attrs->Count(); i++) {
            CAttribute* attr = m_attrs->Array()[i];

            if(attr->Name.EqualsAscii(attrName)) {
                if(!attr->Value.IsEmpty()) {
                    int i;
                    if(attr->Value.TryParseInt(&i)) {
                        if(out_attr) {
                            *out_attr = attr;
                        }
                        *out_int = i;
                        return true;
                    } else {
                        if(failIfTypesDontMatch) {
                            // TODO replace with FailA (A for Attribute)
                            ScriptUtils::FailC("The attribute must have an integer value.", this);
                        }
                        return false;
                    }
                } else {
                    return false;
                }
            }
        }
    }

    return false;
}

} }
