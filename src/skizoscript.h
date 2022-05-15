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

#ifndef SKIZOSCRIPT_H_INCLUDED
#define SKIZOSCRIPT_H_INCLUDED

#include "SharedHeaders.h"

// *******************************************
//      C interface for Skizo embedding.
// *******************************************

#ifdef __cplusplus
extern "C" {
#endif

	// ******************************************
	//   			   Types.
	// ******************************************

typedef void* skizo_domain;
typedef void* skizo_class;
typedef void* skizo_profdata;
typedef void* skizo_watchiterator;
typedef int skizo_result;
#define SKIZO_SUCCESS 0
#define SKIZO_FAILURE -1

// **************
//   Debugging.
// **************

typedef struct {
    char* name;
    skizo_class klass;
    void* varPtr; // NOTE pointer to the variable, not the value itself!
} SKIZO_WATCHINFO;

typedef struct {
    skizo_domain Domain;
    // WARNING This watch iterator is invalid outside of breakpoint callbacks.
    skizo_watchiterator WatchIterator;
} SKIZO_BREAKPOINTINFO;

typedef void (SKIZO_API * SKIZO_BREAKPOINTCALLBACK)(SKIZO_BREAKPOINTINFO* info);

// **************
//   Profiling.
// **************

typedef enum {
    SKIZO_PROFDATA_SORT_TOTALTIME,
    SKIZO_PROFDATA_SORT_AVERAGETIME,
    SKIZO_PROFDATA_SORT_NUMBEROFCALLS
} SKIZO_PROFDATA_SORT;

typedef enum {
    SKIZO_PROFDATA_DUMP_TO_CONSOLE,
    SKIZO_PROFDATA_DUMP_TO_DISK
} SKIZO_PROFDATA_DUMP;

// ********************
//   Domain creation.
// ********************

typedef struct {
    char* source;
    so_bool useSourceAsPath;
    char* name;
    void* stackBase;
    int maxGCMemory;
    so_bool dumpCCode;
    so_bool stackTraceEnabled;
    so_bool profilingEnabled;
    so_bool softDebuggingEnabled;
    so_bool gcStatsEnabled;
    so_bool explicitNullCheck;
    so_bool safeCallbacks;
    so_bool inlineBranching;
    SKIZO_BREAKPOINTCALLBACK breakpointCallback;
    int searchPathCount;
    char** searchPaths;
    int icallCount;
    char** icallNames;
    void** icallImpls;
    so_bool isUntrusted;
    int permissionCount;
    char** permissions;
} SKIZO_DOMAINCREATION;

	// **********************************************
	//   			   Functions.
	// **********************************************

/** A special helper function for systems/languages that have no easy way to get the stack base.
 * @param reserved can be any value.
 * @note The retrieved pointer is valid only if this function is top level, the same level as SKIZORunMain(..).
 */
void* SKIZO_API SKIZOGetStackBase(int reserved);

void SKIZO_API SKIZOInitDomainCreation(SKIZO_DOMAINCREATION* domainCreation,
                                     char* source,
                                     so_bool useSourceAsPath,
                                     void* stackBase);
skizo_domain SKIZO_API SKIZOCreateDomain(SKIZO_DOMAINCREATION* domainCreation);
skizo_result SKIZO_API SKIZOCloseDomain(skizo_domain domain);
skizo_result SKIZO_API SKIZOInvokeEntryPoint(skizo_domain domain);
const char* SKIZO_API SKIZOGetLastError();

skizo_result SKIZO_API SKIZOAbort(const char* msg);

// **********************
//   Memory Management.
// **********************

skizo_result SKIZO_API SKIZOAddGCRoot(skizo_domain domain, void* pObj);
skizo_result SKIZO_API SKIZORemoveGCRoot(skizo_domain domain, void* pObj);
skizo_result SKIZO_API SKIZOCollectGarbage(skizo_domain domain);

// **********************
//   Work with objects.
// **********************

/** Takes a pointer to the Utf16 string data from pObj, which should be a valid string object created
 * by the current domain.
 * Returns 0 if pObj is null or is not a string.
 * @warning If the object isn't GC-rooted, the returned value can get garbage-collected and become corrupt.
 */
void* SKIZO_API SKIZOViewStringData(void* pObj);

// **************
//   Profiling.
// **************

skizo_profdata SKIZO_API SKIZOGetProfilingData(skizo_domain domain);
skizo_result SKIZO_API SKIZOCloseProfilingData(skizo_profdata profdata);
skizo_result SKIZO_API SKIZOSortProfilingData(skizo_profdata profinfo, SKIZO_PROFDATA_SORT sort);
skizo_result SKIZO_API SKIZODumpProfilingData(skizo_profdata profinfo, SKIZO_PROFDATA_DUMP dump);

// ****************
//   Debugging.
// ****************

skizo_result SKIZO_API SKIZONextWatch(skizo_watchiterator watchiter, SKIZO_WATCHINFO* watchInfo);

// ******
//   ?
// ******

int SKIZOLaunchMain(int argc, char **argv);

#ifdef __cplusplus
}
#endif

#endif // SKIZOSCRIPT_H_INCLUDED
