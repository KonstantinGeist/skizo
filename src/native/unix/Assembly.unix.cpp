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

#include "Assembly.h"
#include "Contract.h"
#include "HashMap.h"
#include "Path.h"

#include <dlfcn.h>

namespace skizo { namespace reflection {
using namespace skizo::core;
using namespace skizo::collections;
using namespace skizo::io;

struct AssemblyPrivate
{
    void* m_hModule;
    Auto<CHashMap<const char*, void*> > m_pFuncCache;

    AssemblyPrivate()
        : m_hModule(nullptr), m_pFuncCache(new CHashMap<const char*, void*>())
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
    dlclose(p->m_hModule);

    delete p;
}

CAssembly* CAssembly::Load(const CString* _path)
{
    Auto<const CString> parent (Path::GetParent(_path));
    Auto<const CString> fileName (Path::GetFileName(_path));
    fileName.SetPtr(CString::Format("lib%o.so", static_cast<const CObject*>(fileName.Ptr())));
    Auto<const CString> path (Path::Combine(parent, fileName));

    Utf8Auto cPath (path->ToUtf8());
    void* hModule = dlopen(cPath, RTLD_LAZY);
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
        r = (void*)dlsym(p->m_hModule, name);
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
    if(path->EndsWithASCII(".so")) {
        Auto<const CString> parent (Path::GetParent(path));
        Auto<const CString> fileName (Path::GetFileName(path));
        if(fileName->StartsWithASCII("lib"))
            fileName.SetPtr(fileName->Substring(3));
        fileName.SetPtr(Path::ChangeExtension(fileName, (const CString*)nullptr));

        return Path::Combine(parent, fileName);
    } else {
        return nullptr;
    }
}

} }
