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

#include "Abort.h"
#include "Class.h"
#include "Contract.h"
#include "Domain.h"
#include "Guid.h"
#include "FileSystem.h"
#include "NativeHeaders.h"
#include "Path.h"
#include "RuntimeHelpers.h"
#include "SharedHeaders.h"
#include "Security.h"
#include "String.h"
#include <assert.h>

namespace skizo { namespace script {
using namespace skizo::core;
using namespace skizo::collections;
using namespace skizo::io;

SSecurityManager::SSecurityManager()
    : m_isTrusted(true),
      m_permissions(new CArrayList<const CString*>())
{
}

// ****************
//   Permissions.
// ****************

void SSecurityManager::SetTrusted(bool value)
{
    // Tries to elevate from untrusted to trusted?
    // There's only one way: trusted => untrusted.
    if(value && !m_isTrusted) {
        return;
    }

    m_isTrusted = value;
}

void SSecurityManager::AddPermission(const CString* permission)
{
    m_permissions->Add(permission);
}

void SSecurityManager::DemandPermission(void* soPermObj) const
{
    // Should be checked for null in upper layers (icalls + emitted machine code).
    SKIZO_REQ_PTR(soPermObj);

    // The whole thing is ignored for trusted domains.
    if(!m_isTrusted) {
        const CClass* pClass = so_class_of(soPermObj);
        const SStringSlice permissionName (pClass->FlatName());

        // Iterates over the registered permissions.
        for(int i = 0; i < m_permissions->Count(); i++) {
            const CString* p = m_permissions->Array()[i];

            if(permissionName.Equals(p)) {
                return;
            }
        }

        CDomain::Abort("Code access denied.");
    }
}

bool SSecurityManager::IsPermissionGranted(const char* name) const
{
    // Iterates over the registered permissions.
    for(int i = 0; i < m_permissions->Count(); i++) {
        const CString* p = m_permissions->Array()[i];

        if(p->EqualsASCII(name)) {
            return true;
        }
    }

    return false;
}

// Same as SSecurityManager::DemandPermission(..) just above, except uses chars instead of objects, because we
// don't want to mess with allocating objects from inside C/C++ code.
void SSecurityManager::DemandPermission(const char* name) const
{
    if(!m_isTrusted) {
        if(!IsPermissionGranted(name)) {
            CDomain::Abort("Code access denied.");
        }
    }
}

void CDomain::DemandPermission(const char* name)
{
    CDomain::ForCurrentThread()->m_securityMngr.DemandPermission(name);
}

bool CDomain::IsTrusted() const
{
    return m_securityMngr.IsTrusted();
}

const CArrayList<const CString*>* CDomain::GetPermissions() const
{
    CArrayList<const CString*>* r = new CArrayList<const CString*>();
    r->AddRange(m_securityMngr.Permissions());
    return r;
}

// ************
//   SecureIO
// ************

// Remembers the current directory once and for all: native code may call
// SetCurrentDirectory(..) and spoil all the fun (changing the current directory
// is a bad pattern in multithreaded code anyway).
struct SCurrentDirectoryManager
{
    Auto<const CString> CurrentDirectory;
    Auto<const CString> BaseModuleDirectory;

    SCurrentDirectoryManager()
    {
        CurrentDirectory.SetPtr(FileSystem::GetCurrentDirectory());

        Auto<const CString> modules (CString::FromUtf8(SKIZO_BASE_MODULE_PATH));
        BaseModuleDirectory.SetPtr(Path::Combine(CurrentDirectory, modules));
    }

};

static SCurrentDirectoryManager* g_curDirMgr = nullptr;

void __InitSecurity()
{
    assert(!g_curDirMgr);
    g_curDirMgr = new SCurrentDirectoryManager();
}

void __DeinitSecurity()
{
    assert(g_curDirMgr);
    delete g_curDirMgr;
    g_curDirMgr = nullptr;
}

const CString* SSecurityManager::CurrentDirectory() const
{
    if(!m_isTrusted) {
        // Untrusted domains report their secure directory.
        SKIZO_REQ_PTR(m_securePath);
        return m_securePath;
    } else {
        SKIZO_REQ_PTR(g_curDirMgr->CurrentDirectory);
        return g_curDirMgr->CurrentDirectory;
    }
}

const CString* SSecurityManager::BaseModuleFullPath() const
{
    return g_curDirMgr->BaseModuleDirectory;
}

// Validates a path is secure:
//    1) / is disallowed for Windows.
//    2) parent paths (..) are disallowed for Windows because dangerous (interestingly, Microsoft's IIS does the same)
//    3) tries to check if a path contains invalid nulls in the middle (to avoid trunking)
#define PATH_NOT_SECURE "Path can't be proven to be secure."
static void validatePathIsSecure(const CString* path)
{
    const int length = path->Length();
    const so_char16* chars = path->Chars();
    for(int i = 0; i < length; i++) {
        const so_char16 c = chars[i];

    #ifdef SKIZO_WIN
        if(c == 0 || c == SKIZO_CHAR('/')) {
            CDomain::Abort(PATH_NOT_SECURE);
        } else if((c == SKIZO_CHAR('.')) && ((i + 1) < length) && (chars[i + 1] == SKIZO_CHAR('.'))) {
            CDomain::Abort(PATH_NOT_SECURE);
        }
    #else
        #error "Not implemented."
    #endif
    }
}

void SSecurityManager::InitSecureIO()
{
    if(!m_isTrusted && IsPermissionGranted("FileIOPermission")) {
        Auto<const CString> shortName (Guid::NewGuid());
        Auto<const CString> prefix (CString::FromUtf8(SKIZO_SECURE_PATH));
        m_securePath.SetPtr(Path::Combine(prefix, shortName));
        m_securePath.SetPtr(Path::GetFullPath(m_securePath));

        SKIZO_GUARD_BEGIN // a guard just in case
            FileSystem::CreateDirectory(m_securePath);
        SKIZO_GUARD_END
    }
}

void SSecurityManager::DeinitSecureIO()
{
    if(!m_isTrusted && m_securePath) {
        try {
            FileSystem::DeleteDirectory(m_securePath);
            m_securePath.SetPtr(nullptr);
        } catch(const SException& e) {
            // Ignores exceptions.
        }
    }
}

void CDomain::DemandFileIOPermission(const CString* path)
{
    CDomain::ForCurrentThread()->m_securityMngr.DemandFileIOPermission(path);
}

void SSecurityManager::DemandFileIOPermission(const CString* path) const
{
    SKIZO_REQ_PTR(path); // TODO is it OK to throw here?

    if(!m_isTrusted) {
        this->DemandPermission("FileIOPermission");

        // secure path is already full path
        // NOTE: validatePathIsSecure is called on the path in this->GetFullPath(..)
        Auto<const CString> fullPath (this->GetFullPath(path));
        if(!fullPath->StartsWith(m_securePath)) {
            CDomain::Abort("File access outside of the allowed directory denied.");
        }
    }
}

// Skizo's GetFullPath depends on the process-wide current directory. Native code in one domain might
// change the current directory using WinAPI calls and break GetFullPath in other domains.
// What we do here is implement a domain-aware equivalent of GetFullPath.
const CString* SSecurityManager::GetFullPath(const CString* path) const
{
    // Makes sure the given path doesn't make use of non-secure elements, like ".." for parents or
    // invalid characters.
    validatePathIsSecure(path);

    Auto<const CString> probe (Path::GetFullPath(path)); // probes if it's full path using Skizo's GetFullPath
    if(probe->Equals(path)) {
        // The probe is identical to path: the path is already full (normalized).
        path->Ref();
        return path;
    } else {
        // It's a short path: combine it with the domain-aware current directory.
        return Path::Combine(g_curDirMgr->CurrentDirectory, path); // ?
    }
}

extern "C" {

// TODO move to icalls/Domain.cpp
typedef void (SKIZO_API * _FAction)(void*);
void* SKIZO_API _so_Domain_try(void* soAction)
{
    SKIZO_NULL_CHECK(soAction);

    CDomain* domain = CDomain::ForCurrentThread();
    const char* errorMsg = nullptr;
    Auto<const CString> stackTraceInfo;

    // Virtual frames should be correctly unwound in Domain::try(..)
    // Anywhere else, it's not needed, as aborts usually terminate the domain entirely.
    SVirtualUnwinder unwinder (domain);
    unwinder.Remember();

    try {
        // Calls the action closure.
        _FAction actionFunc = (_FAction)so_invokemethod_of(soAction);
        actionFunc(soAction);
    } catch (const SoDomainAbortException& e) {
        errorMsg = e.Message; // TPDP wrpmg access
        stackTraceInfo.SetPtr(domain->GetStackTraceInfo()); // the stack is untouched at this point
    } catch (...) {
        errorMsg = "Unknown internal error.";
        stackTraceInfo.SetPtr(domain->GetStackTraceInfo()); // the stack is untouched at this point
    }

    if(errorMsg) {

        // Unwinds virtual stacks back to where it must be in case it failed.
        unwinder.Unwind();

        Auto<const CString> daErrorMsg (CString::Format("%s\n%o", errorMsg, (CObject*)stackTraceInfo.Ptr()));
        return domain->CreateString(daErrorMsg);
    } else {
        return nullptr;
    }
}

}

} }
