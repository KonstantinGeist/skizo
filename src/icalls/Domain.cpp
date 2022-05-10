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
#include "../icall.h"
#include "../RuntimeHelpers.h"
#include "../ScriptUtils.h"

namespace skizo { namespace script {
using namespace skizo::core;
using namespace skizo::collections;

extern "C" {

// note: soPermArray is allowed to be null, which means "create me a trusted domain"
void* SKIZO_API _so_Domain_runGenericImpl(void* _source, int sourceKind, void* soPermArray)
{
    SKIZO_NULL_CHECK(_source);

    CDomain::DemandPermission("DomainCreationPermission");

    if(sourceKind == E_SOURCEKIND_PATH || sourceKind == E_SOURCEKIND_METHODNAME) {
        CDomain::DemandPermission("FileIOPermission");
    }

    try {
        CDomain* domain = CDomain::ForCurrentThread();

        Auto<CArrayList<const CString*> > daArray (ScriptUtils::ArrayHeaderToStringArray((SArrayHeader*)soPermArray, false));
        // A domain spawned from inside another untrusted domain automatically inherits its permissions and becomes untrusted
        // as well, otherwise allowing an untrusted domain to spawn a trusted domain would compromise our security model.
        if(!domain->SecurityManager().IsTrusted()) {
            if(daArray && daArray->Count() > 0) {
                CDomain::Abort("Untrusted domains aren't allowed to spawn new domains with altered permission sets.");
            }

            const CArrayList<const CString*>* inheritedPerms = domain->SecurityManager().Permissions();
            if(!daArray) {
                daArray.SetPtr(new CArrayList<const CString*>());
            }
            for(int i = 0; i < inheritedPerms->Count(); i++) {
                daArray->Add(inheritedPerms->Array()[i]);
            }
        }

        return domain->CreateRemoteDomain(so_string_of(_source), (ESourceKind)sourceKind, daArray);
    } catch (const SException& e) {
        // Just in case there are unhandled Skizo exceptions.
        printf("ABORT (domain creation): %s\n", e.Message()); // TODO generic error/output interface
        _so_StackTrace_print();
        return nullptr;
    }
}

void SKIZO_API _so_Domain_sleep(int i)
{
    if(i < 1) {
        CDomain::Abort("Argument to Domain::sleep(int) must be equal or greater than 1.");
    }

    CThread::Sleep(i);
}

void* SKIZO_API _so_Domain_name()
{
    CDomain* curDomain = CDomain::ForCurrentThread();
    // m_domainName is guaranteed to be something
    return curDomain->CreateString(curDomain->Name(), true);
}

_so_bool SKIZO_API _so_Domain_isBaseDomain()
{
    return CDomain::ForCurrentThread()->IsBaseDomain();
}

void SKIZO_API _so_Domain_listen(void* soStopPred)
{
    CDomain::ForCurrentThread()->Listen(soStopPred);
}

_so_bool SKIZO_API _so_Domain_isTrusted()
{
    return CDomain::ForCurrentThread()->IsTrusted();
}

void* SKIZO_API _so_Domain_permissions()
{
    CDomain* domain = CDomain::ForCurrentThread();

    Auto<const CArrayList<const CString*> > perms (domain->GetPermissions());
    void* r = nullptr;
    SKIZO_GUARD_BEGIN
        r = domain->CreateArray(perms);
    SKIZO_GUARD_END
    return r;
}

// **************************************************************

_so_bool SKIZO_API _so_DomainHandle_isAliveImpl(void* _handle)
{
    return ((CDomainHandle*)_handle)->IsAlive()? _so_TRUE: _so_FALSE;
}

void SKIZO_API _so_DomainHandle_dtorImpl(void* handle)
{
    if(handle) {
        ((CDomainHandle*)handle)->Unref();
    }
}

_so_bool SKIZO_API _so_DomainHandle_waitImpl(void* _handle, int timeout)
{
    return ((CDomainHandle*)_handle)->Wait(timeout);
}

void SKIZO_API _so_Domain_exportObject(void* name, void* obj)
{
    SKIZO_NULL_CHECK(name);
    // NOTE `obj` can be null.

    CDomain::ForCurrentThread()->ExportObject(so_string_of(name), obj);
}

void* SKIZO_API _so_DomainHandle_importObjectImpl(void* daHandle, void* soHandle, void* name)
{
    CDomainHandle* handle = (CDomainHandle*)daHandle;
    return handle->ImportObject(soHandle, name);
}

void SKIZO_API _so_Domain_addDependency(void* intrfc, void* impl)
{
    SKIZO_NULL_CHECK(intrfc);
    SKIZO_NULL_CHECK(impl);
    CDomain::ForCurrentThread()->Activator().AddDependency(so_string_of(intrfc), so_string_of(impl));
}

void* SKIZO_API _so_Domain_getDependency(void* intrfc)
{
    SKIZO_NULL_CHECK(intrfc);
    return CDomain::ForCurrentThread()->Activator().GetDependency(so_string_of(intrfc));
}

void* SKIZO_API _so_Domain_createInstance(void* intrfc)
{
    SKIZO_NULL_CHECK(intrfc);
    return CDomain::ForCurrentThread()->Activator().CreateInstance(so_string_of(intrfc));
}

}

} }
