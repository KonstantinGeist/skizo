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

#include "skizoscript.h"
#include "Abort.h"
#include "Domain.h"
#include "Profiling.h"
#include "RuntimeHelpers.h"
#include "String.h"

extern "C" {
using namespace skizo::core;
using namespace skizo::script;

#define SKIZO_GUARD_BEGIN_AB try {
#define SKIZO_GUARD_END_AB return SKIZO_SUCCESS; } catch (...) { return SKIZO_FAILURE; }

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wreturn-local-addr"
void* __attribute__ ((noinline)) SKIZO_API SKIZOGetStackBase(int reserved)
{
    // Machination to shut up the compiler about returning an address of an argument.
    void* r = &reserved;
    return r;
}
#pragma GCC diagnostic pop

void SKIZO_API SKIZOInitDomainCreation(SKIZO_DOMAINCREATION* domainCreation,
                                     char* source,
                                     so_bool useSourceAsPath,
                                     void* stackBase)
{
    memset(domainCreation, 0, sizeof(SKIZO_DOMAINCREATION));
    domainCreation->source = source;
    domainCreation->useSourceAsPath = useSourceAsPath;
    domainCreation->stackBase = stackBase;

    domainCreation->stackTraceEnabled = true;
    domainCreation->explicitNullCheck = true;
}

skizo_domain SKIZO_API SKIZOCreateDomain(SKIZO_DOMAINCREATION* cDC)
{
    if(!cDC->source || !cDC->stackBase) {
        return nullptr;
    }

    Auto<const CString> source, name;
    SDomainCreation domainCreation;

    try {
        source.SetPtr(CString::FromUtf8(cDC->source));
        domainCreation.Source = source;
        domainCreation.UseSourceAsPath = cDC->useSourceAsPath;

        if(cDC->name) {
            name.SetPtr(CString::FromUtf8(cDC->name));
            domainCreation.Name = name;
        }

        domainCreation.StackBase = cDC->stackBase;

        domainCreation.MinGCThreshold = cDC->minGCThreshold;
        domainCreation.DumpCCode = cDC->dumpCCode;
        domainCreation.StackTraceEnabled = cDC->stackTraceEnabled;
        domainCreation.ProfilingEnabled = cDC->profilingEnabled;
        domainCreation.SoftDebuggingEnabled = cDC->softDebuggingEnabled;
        domainCreation.GCStatsEnabled = cDC->gcStatsEnabled;
        domainCreation.ExplicitNullCheck = cDC->explicitNullCheck;
        domainCreation.InlineBranching = cDC->inlineBranching;
        domainCreation.SafeCallbacks = cDC->safeCallbacks;
        domainCreation.BreakpointCallback = cDC->breakpointCallback;

        for(int i = 0; i < cDC->searchPathCount; i++) {
            SKIZO_NULL_CHECK(cDC->searchPaths[i]);

            domainCreation.AddSearchPath(cDC->searchPaths[i]);
        }

        for(int i = 0; i < cDC->icallCount; i++) {
            SKIZO_NULL_CHECK(cDC->icallNames[i]);
            SKIZO_NULL_CHECK(cDC->icallImpls[i]);

            domainCreation.registerICall(cDC->icallNames[i], cDC->icallImpls[i]);
        }

        domainCreation.IsUntrusted = cDC->isUntrusted;
        for(int i = 0; i < cDC->permissionCount; i++) {
            SKIZO_NULL_CHECK(cDC->permissions[i]);

            Auto<const CString> permission (CString::FromUtf8(cDC->permissions[i]));
            domainCreation.AddPermission(permission);
        }

        return (skizo_domain)CDomain::CreateDomain(domainCreation);
    } catch (...) {
        return nullptr;
    }
}

skizo_result SKIZO_API SKIZOCloseDomain(skizo_domain domain)
{
    if(!domain) {
        return SKIZO_FAILURE;
    }
    ((CDomain*)domain)->Unref();
    return SKIZO_SUCCESS;
}

skizo_result SKIZO_API SKIZOInvokeEntryPoint(skizo_domain domain)
{
    if(!domain) {
        return SKIZO_FAILURE;
    }

    skizo_result r;
    try {
        //bool r = ((CDomain*)domain)->InvokeEntryPoint();
        //return r? SKIZO_SUCCESS: SKIZO_FAILURE;
        const bool b = ((CDomain*)domain)->InvokeEntryPoint();
        r = b? SKIZO_SUCCESS: SKIZO_FAILURE;
    } catch(const SoDomainAbortException& e) {
        r = SKIZO_FAILURE;
    }

    return r;
}

const char* SKIZO_API SKIZOGetLastError()
{
    return CDomain::GetLastError();
}

skizo_result SKIZO_API SKIZOAbort(const char* msg)
{
    CDomain::Abort(msg);
    return SKIZO_FAILURE;
}

// **********************
//   Memory management.
// **********************

skizo_result SKIZO_API SKIZOAddGCRoot(skizo_domain domain, void* pObj)
{
    if(!domain) {
        return SKIZO_FAILURE;
    }
    
    SKIZO_GUARD_BEGIN_AB
        ((CDomain*)domain)->MemoryManager().AddGCRoot(pObj);
    SKIZO_GUARD_END_AB
}

skizo_result SKIZO_API SKIZORemoveGCRoot(skizo_domain domain, void* pObj)
{
    if(!domain) {
        return SKIZO_FAILURE;
    }
    
    SKIZO_GUARD_BEGIN_AB
        ((CDomain*)domain)->MemoryManager().RemoveGCRoot(pObj);
    SKIZO_GUARD_END_AB
}

skizo_result SKIZO_API SKIZOCollectGarbage(skizo_domain domain)
{
    if(!domain) {
        return SKIZO_FAILURE;
    }
    
    SKIZO_GUARD_BEGIN_AB
        ((CDomain*)domain)->MemoryManager().CollectGarbage();
    SKIZO_GUARD_END_AB
}

skizo_profdata SKIZO_API SKIZOGetProfilingData(skizo_domain domain)
{
    if(!domain) {
        return nullptr;
    }
    return (skizo_profdata)((CDomain*)domain)->GetProfilingInfo();
}

skizo_result SKIZO_API SKIZOCloseProfilingData(skizo_profdata profdata)
{
    if(!profdata) {
        return SKIZO_FAILURE;
    }
    ((CProfilingInfo*)profdata)->Unref();
    return SKIZO_SUCCESS;
}

skizo_result SKIZO_API SKIZOSortProfilingData(skizo_profdata profdata, SKIZO_PROFDATA_SORT sort)
{
    if(!profdata) {
        return SKIZO_FAILURE;
    }
    CProfilingInfo* profinfo = (CProfilingInfo*)profdata;
    try {
        switch(sort) {
            case SKIZO_PROFDATA_SORT_TOTALTIME:
                profinfo->SortByTotalTimeInMs();
                break;
            case SKIZO_PROFDATA_SORT_AVERAGETIME:
                profinfo->SortByAverageTimeInMs();
                break;
            case SKIZO_PROFDATA_SORT_NUMBEROFCALLS:
                profinfo->SortByNumberOfCalls();
                break;
            default:
                return SKIZO_FAILURE;
        }
    } catch (const SException& e) {
        return SKIZO_FAILURE;
    }
    return SKIZO_SUCCESS;
}

skizo_result SKIZO_API SKIZODumpProfilingData(skizo_profdata profdata, SKIZO_PROFDATA_DUMP dump)
{
    if(!profdata) {
        return SKIZO_FAILURE;
    }
    CProfilingInfo* profinfo = (CProfilingInfo*)profdata;
    try {
        switch(dump) {
            case SKIZO_PROFDATA_DUMP_TO_CONSOLE:
                profinfo->DumpToConsole();
                break;
            case SKIZO_PROFDATA_DUMP_TO_DISK:
                profinfo->DumpToDisk();
                break;
            default:
                return SKIZO_FAILURE;
        }
    } catch (const SException& e) {
        return SKIZO_FAILURE;
    }
    return SKIZO_SUCCESS;
}

skizo_result SKIZO_API SKIZONextWatch(skizo_watchiterator wi, SKIZO_WATCHINFO* watchInfo)
{
    if(!wi) {
        return SKIZO_FAILURE;
    }
    CWatchIterator* watchIterator = (CWatchIterator*)wi;
    return watchIterator->NextWatch(watchInfo)? SKIZO_SUCCESS: SKIZO_FAILURE;
}

// **********************
//   Work with objects.
// **********************

void* SKIZO_API SKIZOViewStringData(void* pObj)
{
    if(pObj) {
        const CClass* klass = so_class_of(pObj);
        if(klass != CDomain::ForCurrentThread()->StringClass()) {
            return nullptr;
        }

        const CString* str = so_string_of(pObj);
        if(!str) {
            return nullptr; // another precaution, just in case
        }
        return (void*)str->Chars();
    } else {
        return nullptr;
    }
}

}
