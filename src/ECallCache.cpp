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

#include "ECallCache.h"
#include "Domain.h"

namespace skizo { namespace script {
using namespace skizo::core;
using namespace skizo::collections;

SECallCache::~SECallCache()
{
    SHashMapEnumerator<SStringSlice, void*> cacheEnum (&m_moduleCache);
    SStringSlice name;
    void* handle;
    while(cacheEnum.MoveNext(&name, &handle)) {
    #ifdef SKIZO_WIN
        FreeLibrary((HMODULE)handle);
    #else
        #error "Not implemented."
    #endif
    }
}

void* SECallCache::SkizoGetLibrary(const SStringSlice& name_) const
{
    void* r;
    if(m_moduleCache.TryGet(name_, &r)) {
        return r;
    }

#ifdef SKIZO_WIN
    Auto<const CString> libName (name_.ToString());
    r = LoadLibrary((WCHAR*)(libName->Chars()));
#else
    #error "Not implemented."
#endif

    if(!r) {
        Auto<const CString> str1 (CString::Format("Native module '%o' not found.", (CObject*)libName.Ptr()));
        char* str2 = str1->ToUtf8();
        CDomain::Abort(str2, true);
        return nullptr;
    }

    m_moduleCache.Set(name_, r);
    return r;
}

void* SECallCache::SkizoGetProcedure(void* nmodule, const SStringSlice& _procName) const
{
#ifdef SKIZO_WIN
    Utf8Auto procName (_procName.ToUtf8());
    void* r = (void*)GetProcAddress((HMODULE)nmodule, procName);
#else
    #error "Not implemented."
#endif

    if(!r) {
        Auto<const CString> str1 (CString::Format("Native procedure '%s' not found.", procName.Ptr()));
        char* str2 = str1->ToUtf8();
        CDomain::Abort(str2, true);
        return nullptr;
    }

    return r;
}

} }
