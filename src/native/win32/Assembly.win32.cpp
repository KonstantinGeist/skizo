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

#include "../../Assembly.h"
#include "../../Contract.h"
#include "../../HashMap.h"
#include "../../Path.h"

namespace skizo { namespace reflection {
using namespace skizo::core;
using namespace skizo::collections;
using namespace skizo::io;

struct AssemblyPrivate
{
    HMODULE m_hModule;
    Auto<CHashMap<const char*, void*> > m_pFuncCache;

    AssemblyPrivate()
        : m_hModule(NULL),
          m_pFuncCache(new CHashMap<const char*, void*>())
    {
    }

    ~AssemblyPrivate()
    {
        SHashMapEnumerator<const char*, void*> mapEnum (m_pFuncCache);
        const char* k;
        while(mapEnum.MoveNext(&k, nullptr)) {
            delete [] k;
        }
    }
};

CAssembly::CAssembly()
    : p (new AssemblyPrivate())
{
    
}

CAssembly::~CAssembly()
{
    FreeLibrary(p->m_hModule);

    delete p;
}

CAssembly* CAssembly::Load(const CString* _path)
{
    Auto<const CString> path (Path::ChangeExtension(_path, "dll"));

    HMODULE hModule = LoadLibrary(reinterpret_cast<LPCTSTR>(path->Chars()));
    if(!hModule) {
        SKIZO_THROW_WITH_MSG(EC_PATH_NOT_FOUND, "Failed to load assembly.");
    }

    auto r = new CAssembly();
    r->p->m_hModule = hModule;
    return r;
}

void* CAssembly::getFunctionImpl(const char* name) const
{
    SKIZO_REQ_PTR(name)

    void* r;

    if(!p->m_pFuncCache->TryGet(name, &r)) {
        r = (void*)GetProcAddress(p->m_hModule, name);
        if(!r) {
            SKIZO_THROW_WITH_MSG(EC_KEY_NOT_FOUND, "Failed to load function.");
        }

        char* nameCopy = new char[strlen(name) + 1]; // to be destroyed in ~AssemblyPrivate
        strcpy(nameCopy, name);
        p->m_pFuncCache->Set(nameCopy, r);
    }

    return r;
}

const CString* CAssembly::GetAssemblyName(const CString* path)
{
    if(path->EndsWithASCII(".dll") || path->EndsWithASCII(".DLL")) {
        return Path::ChangeExtension(path, (const CString*)nullptr);
    } else {
        return nullptr;
    }
}

} }
