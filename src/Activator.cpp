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

#include "Activator.h"
#include "Contract.h"
#include "Domain.h"
#include "HashMap.h"

namespace skizo { namespace script {
using namespace skizo::core;
using namespace skizo::collections;

// TODO resolve circular dependencies

// Hardcoded to avoid JIT-compiling. A lot of dependencies is a bad design anyway.
// WARNING Match SKIZO_MAX_DEPENDENCY_COUNT to the number of switch cases.
#define SKIZO_MAX_DEPENDENCY_COUNT 10

static void verifyClassIsSuitable(CClass* klass)
{
    if(klass->SpecialClass() != E_SPECIALCLASS_NONE || !klass->IsRefType()) {
        CDomain::Abort("Dependency injection supports only simple by-reference classes.");
    }
}

struct ActivatorPrivate
{
    CDomain* m_domain;
    Auto<CHashMap<void*, CClass*>> m_interfaceToImplMap;
    Auto<CHashMap<void*, void*>> m_classToInstanceMap;

    mutable void* m_args[SKIZO_MAX_DEPENDENCY_COUNT];

    explicit ActivatorPrivate(CDomain* domain);
    CClass* classForName(const CString* className, const char* errorMsg) const;
    void* createInstance(CClass* klass, bool cacheInstance) const;
    bool hasSuitableParams(const CMethod* ctor) const;
    const CMethod* findSuitableCtor(const CClass* klass) const;
    void prepareArgs(const CMethod* ctor) const;
};

SActivator::SActivator(CDomain* domain)
    : p(new ActivatorPrivate(domain))
{
}

SActivator::~SActivator()
{
    delete p;
}

ActivatorPrivate::ActivatorPrivate(CDomain* domain)
    : m_domain(domain),
      m_interfaceToImplMap(new CHashMap<void*, CClass*>()),
      m_classToInstanceMap(new CHashMap<void*, void*>())
{
}

CClass* ActivatorPrivate::classForName(const CString* className, const char* errorMsg) const
{
    CClass* klass = m_domain->ClassByNiceName(className);
    if(!klass) {
        CDomain::Abort(errorMsg);
        return nullptr;
    }
    return klass;
}

bool ActivatorPrivate::hasSuitableParams(const CMethod* ctor) const
{
    const CArrayList<CParam*>* params = ctor->Signature().Params;
    if(params->Count() > SKIZO_MAX_DEPENDENCY_COUNT) {
        return false;
    }

    for(int i = 0; i < params->Count(); i++) {
        const STypeRef& typeRef = params->Array()[i]->Type;

        if(typeRef.Kind != E_TYPEREFKIND_NORMAL
        || typeRef.ArrayLevel > 0
        || !m_interfaceToImplMap->Contains((void*)typeRef.ResolvedClass))
        {
            return false;
        }
    }

    return true;
}

const CMethod* ActivatorPrivate::findSuitableCtor(const CClass* klass) const
{
    const CArrayList<CMethod*>* ctors = klass->InstanceCtors();
    for(int i = 0; i < ctors->Count(); i++) {
        const CMethod* ctor = ctors->Array()[i];

        if(hasSuitableParams(ctor)) {
            return ctor;
        }
    }

    return nullptr;
}

void ActivatorPrivate::prepareArgs(const CMethod* ctor) const
{
    const CArrayList<CParam*>* params = ctor->Signature().Params;
    for(int i = 0; i < params->Count(); i++) {
        CClass* klass = params->Array()[i]->Type.ResolvedClass;

        void* obj = createInstance(klass, true); // cacheInstance=true
        m_args[i] = obj;
    }
}

void* ActivatorPrivate::createInstance(CClass* klass, bool cacheInstance) const
{
    CClass* classToCacheFor = klass;
    void* obj = nullptr;
    if(cacheInstance && m_classToInstanceMap->TryGet((void*)classToCacheFor, &obj)) {
        return obj;
    }

    if(klass->SpecialClass() == E_SPECIALCLASS_INTERFACE) {
        if(!m_interfaceToImplMap->TryGet((void*)klass, &klass)) {
            CDomain::Abort("No dependency registered for the interface.");
        }
        klass->Unref();
    }
    verifyClassIsSuitable(klass);

    const CMethod* ctor = findSuitableCtor(klass);
    if(!ctor) {
        CDomain::Abort("No suitable constructor is found for this class or one of the dependencies.");
    }

    prepareArgs(ctor);

    void* pFunc = m_domain->GetFunctionPointer(ctor);
    if(!pFunc) {
        CDomain::Abort("Constructor without implementation."); // just in case
    }

    // Hardcoded to avoid JIT-compiling.
    typedef void* (SKIZO_API * FCtor0)();
    typedef void* (SKIZO_API * FCtor1)(void*);
    typedef void* (SKIZO_API * FCtor2)(void*, void*);
    typedef void* (SKIZO_API * FCtor3)(void*, void*, void*);
    typedef void* (SKIZO_API * FCtor4)(void*, void*, void*, void*);
    typedef void* (SKIZO_API * FCtor5)(void*, void*, void*, void*, void*);
    typedef void* (SKIZO_API * FCtor6)(void*, void*, void*, void*, void*, void*);
    typedef void* (SKIZO_API * FCtor7)(void*, void*, void*, void*, void*, void*, void*);
    typedef void* (SKIZO_API * FCtor8)(void*, void*, void*, void*, void*, void*, void*, void*);
    typedef void* (SKIZO_API * FCtor9)(void*, void*, void*, void*, void*, void*, void*, void*, void*);
    typedef void* (SKIZO_API * FCtor10)(void*, void*, void*, void*, void*, void*, void*, void*, void*, void*);

    // WARNING Match the number of switch cases to SKIZO_MAX_DEPENDENCY_COUNT.
    switch(ctor->Signature().Params->Count()) {
        case 0: obj = ((FCtor0)pFunc)(); break;
        case 1: obj = ((FCtor1)pFunc)(m_args[0]); break;
        case 2: obj = ((FCtor2)pFunc)(m_args[0], m_args[1]); break;
        case 3: obj = ((FCtor3)pFunc)(m_args[0], m_args[1], m_args[2]); break;
        case 4: obj = ((FCtor4)pFunc)(m_args[0], m_args[1], m_args[2], m_args[3]); break;
        case 5: obj = ((FCtor5)pFunc)(m_args[0], m_args[1], m_args[2], m_args[3], m_args[4]); break;
        case 6: obj = ((FCtor6)pFunc)(m_args[0], m_args[1], m_args[2], m_args[3], m_args[4], m_args[5]); break;
        case 7: obj = ((FCtor7)pFunc)(m_args[0], m_args[1], m_args[2], m_args[3], m_args[4], m_args[5], m_args[6]); break;
        case 8: obj = ((FCtor8)pFunc)(m_args[0], m_args[1], m_args[2], m_args[3], m_args[4], m_args[5], m_args[6], m_args[7]); break;
        case 9: obj = ((FCtor9)pFunc)(m_args[0], m_args[1], m_args[2], m_args[3], m_args[4], m_args[5], m_args[6], m_args[7], m_args[8]); break;
        case 10: obj = ((FCtor10)pFunc)(m_args[0], m_args[1], m_args[2], m_args[3], m_args[4], m_args[5], m_args[6], m_args[7], m_args[8], m_args[9]); break;
        default: SKIZO_REQ_NEVER
    }

    m_domain->MemoryManager().AddGCRoot(obj);
    if(cacheInstance) {
        m_classToInstanceMap->Set((void*)classToCacheFor, obj);
    }
    return obj;
}

void SActivator::AddDependency(CClass* interface, CClass* impl)
{
    verifyClassIsSuitable(impl);
    if(interface->SpecialClass() != E_SPECIALCLASS_INTERFACE) {
        CDomain::Abort("Not an interface.");
    }
    if(!impl->DoesImplementInterface(interface)) {
        CDomain::Abort("The interface isn't implemented by the class.");
    }

    p->m_interfaceToImplMap->Set((void*)interface, impl);
}

void SActivator::AddDependency(const CString* interfaceName, const CString* implName)
{
    CClass* interface = p->classForName(interfaceName, "Interface not found.");
    CClass* impl = p->classForName(implName, "Class not found.");

    this->AddDependency(interface, impl);
}

void* SActivator::CreateInstance(CClass* klass) const
{
    return p->createInstance(klass, false);
}

void* SActivator::CreateInstance(const CString* className) const
{
    CClass* klass = p->classForName(className, "Class not found.");
    return p->createInstance(klass, false);
}

void* SActivator::GetDependency(CClass* interface) const
{
    return p->createInstance(interface, true);
}

void* SActivator::GetDependency(const CString* interfaceName) const
{
    CClass* interface = p->classForName(interfaceName, "Interface not found.");
    return p->createInstance(interface, true);
}

} }