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

// **************************************************************************************************
// Define DA_ADVANCED_LEAK_DETECTOR, DA_TRACK_ALLOCATIONS and uncomment the line below to debug leaks
// in the VM (it will print a list of unreleased Skizo objects upon base domain teadown).
//#define SKIZO_DEBUG_LEAKS
// **************************************************************************************************

#include <stdio.h>
#include "src/Abort.h"
#include "src/ApplicationOptions.h"
#include "src/ArrayList.h"
#include "src/Console.h"
#include "src/Domain.h"
#include "src/FileSystem.h"
#include "src/init.h"
#include "src/Profiling.h"
#include "src/RuntimeHelpers.h"

using namespace skizo::core;
using namespace skizo::io;
using namespace skizo::collections;
using namespace skizo::script;

// **************
//   Utilities.
// **************

static void addOptionDescr(CArrayList<CApplicationOptionDescription*>& descrs, const char* name, const char* desc, const char* defaultValue)
{
    Auto<CApplicationOptionDescription> descr (new CApplicationOptionDescription(name, desc, defaultValue));
    descrs.Add(descr);
}

// ******************
//   SKIZOLaunchMain
// ******************

static int mainImpl(int argc, char **argv)
{
  int r = 0;

  {
    #ifndef SKIZO_DEBUG_MODE
        #ifdef SKIZO_BASIC_LEAK_DETECTOR
        SuppressBasicLeakDetector(true);
        #endif
    #endif

    // ********************
    // Parses the commands.
    // ********************

    CArrayList<CApplicationOptionDescription*> descrs;
    addOptionDescr(descrs, "source", "specifies the main file to interpret", 0);
    addOptionDescr(descrs, "paths", "paths where to look for modules", 0);
    addOptionDescr(descrs, "help", "prints this information", "false");
    addOptionDescr(descrs, "dump", "dumps emitted code", "false");
    addOptionDescr(descrs, "profile", "profiles the program during execution", "false");
    addOptionDescr(descrs, "stacktraces", "registers stacktraces for diagnostics, enables stackoverflow detection", "true");
    addOptionDescr(descrs, "softdebug", "soft debugging enabled", "false");
    addOptionDescr(descrs, "nullcheck", "explicit null check", "true");
    addOptionDescr(descrs, "safecallbacks", "closures passed as C callbacks to native code are checked for being called in correct domains", "false");
    addOptionDescr(descrs, "permissions", "makes the base domain untrusted and specifies a list of permissions", 0);
    addOptionDescr(descrs, "inline", "inlines branching", "true");
    addOptionDescr(descrs, "maxgcmemory", "sets maximum GC memory", "134217728");
    addOptionDescr(descrs, "gcstats", "gc stats on every garbage collection", "false");

    Auto<const CString> source;
    Auto<CArrayList<const CString*> > searchPaths;
    Auto<CArrayList<const CString*> > permissions;
    bool dumpCode, profilingEnabled, stackTraceEnabled, softDebuggingEnabled,
         explicitNullCheck, safeCallbacks, doinline, gcstats;
    bool isSecure = false;
    int maxGCMemory = -1;

    try {

        Auto<CApplicationOptions> options (CApplicationOptions::GetOptions(&descrs));

        bool helpRequired = (options->GetBoolOption("help"));
        if(helpRequired || options->Size() == 0) {
            printf("SkizoScript 1.0\n\n"); // TODO extract the version from the runtime
            options->PrintHelp();

            if(options->Size() == 0) {
                return 0;
            }
        }

        source.SetPtr(options->GetStringOption("source"));
        if(!source) {
            printf("No source specified.\n");
            return 1;
        }

        Auto<const CString> _searchPaths (options->GetStringOption("paths"));
        if(CString::IsNullOrEmpty(_searchPaths)) {
            searchPaths.SetPtr(new CArrayList<const CString*>());
        } else {
            searchPaths.SetPtr(_searchPaths->Split(SKIZO_CHAR(';')));
        }
        dumpCode = options->GetBoolOption("dump");

        Auto<const CString> _permissions (options->GetStringOption("permissions"));
        if(!CString::IsNullOrEmpty(_permissions)) {
            isSecure = true;
            permissions.SetPtr(_permissions->Split(SKIZO_CHAR(';')));
        }

        profilingEnabled = options->GetBoolOption("profile");
        stackTraceEnabled = options->GetBoolOption("stacktraces");
        softDebuggingEnabled = options->GetBoolOption("softdebug");
        explicitNullCheck = options->GetBoolOption("nullcheck");
        safeCallbacks = options->GetBoolOption("safecallbacks");
        doinline = options->GetBoolOption("inline");
        maxGCMemory = options->GetIntOption("maxgcmemory");
        if(maxGCMemory < 1 && maxGCMemory != -1) {
            printf("Min GC threshold must be greater than zero.\n");
            return 1;
        }
        gcstats = options->GetBoolOption("gcstats");

    } catch(SException& e) {
        printf("%s\n", e.Message());
        return 1;
    }

    #ifdef SKIZO_BASIC_LEAK_DETECTOR
    if(profilingEnabled) {
        SuppressBasicLeakDetector(false);
    }
    #endif

    // *******************************
    // Creates the domain and runs it.
    // *******************************

    SDomainCreation domainCreation;
    domainCreation.StackBase = &argc;
    domainCreation.Source = source;

    CArrayList<char*> pSearchPaths;
    for(int i = 0; i < searchPaths->Count(); i++) {
        pSearchPaths.Add(searchPaths->Array()[i]->ToUtf8());
    }
    for(int i = 0; i < pSearchPaths.Count(); i++) {
        domainCreation.AddSearchPath(pSearchPaths.Array()[i]);
    }

    if(isSecure) {
        domainCreation.IsUntrusted = true;
        for(int i = 0; i < permissions->Count(); i++) {
            domainCreation.AddPermission(permissions->Array()[i]);
        }
    }

    domainCreation.DumpCCode = dumpCode;
    domainCreation.ProfilingEnabled = profilingEnabled;
    domainCreation.StackTraceEnabled = stackTraceEnabled;
    domainCreation.SoftDebuggingEnabled = softDebuggingEnabled;
    domainCreation.ExplicitNullCheck = explicitNullCheck;
    domainCreation.SafeCallbacks = safeCallbacks;
    domainCreation.InlineBranching = doinline;
    if(maxGCMemory != -1) {
        domainCreation.MaxGCMemory = maxGCMemory;
    }
    domainCreation.GCStatsEnabled = gcstats;

    Auto<CDomain> domain;

    try {
        try {
            domain.SetPtr(CDomain::CreateDomain(domainCreation));
        } catch (SoDomainAbortException& e) {
            // Catches aborts during domain creation.
            printf("%s\n", e.Message);
            return 1;
        }

        // RunMain automatically catches aborts.
        if(!domain->InvokeEntryPoint()) {
            r = 1;
        }

    } catch(SException& e) {
        // Catches potential, uncaught Skizo exceptions.
        printf("%s\n", e.Message());
        return 1;
    }

    for(int i = 0; i < pSearchPaths.Count(); i++) {
        CString::FreeUtf8(pSearchPaths.Array()[i]);
    }

    if(profilingEnabled) {
        Auto<CProfilingInfo> profInfo (domain->GetProfilingInfo());
        profInfo->SortByAverageTimeInMs();
        profInfo->DumpToDisk();
        printf("\n====================================================================\n"
                 "Profiling information saved to profile.txt in the current directory.\n"
                 "====================================================================\n");
    }
  }

  #ifdef SKIZO_DEBUG_LEAKS
    CThread::DisassociateMainThreadUnsafe();
    CoreUtils::DumpHeap();
  #endif

  return r;
}

int main(int argc, char **argv)
{
    InitSkizo();
    int r = mainImpl(argc, argv);
    DeinitSkizo();
    return r;
}
