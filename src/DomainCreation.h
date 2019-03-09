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

#ifndef DOMAINCREATION_H_INCLUDED
#define DOMAINCREATION_H_INCLUDED

#include "ArrayList.h"
#include "HashMap.h"
#include "options.h"
#include "skizoscript.h"
#include "String.h"

namespace skizo { namespace script {

/**
 * Passed to CDomain::CreateDomain
 */
struct SDomainCreation
{
    friend class CDomain;

public:
    /**
     * The path to the source: the main module
     */
    const skizo::core::CString* Source;

    /**
     * If true, ::Source is a path to the module on disk.
     * If false, ::Source is a string which already contains code.
     */
    bool UseSourceAsPath;

    /**
     * The name of the domain. Can be null.
     */
    const skizo::core::CString* Name;

    /**
     * The entrypoint class. If it's null, "Program" is assumed.
     * If EntryPointMethod is null, this value is ignored.
     */
    const skizo::core::CString* EntryPointClass;

    /**
     * The entrypoint method. If it's null, "main" is assumed.
     * If EntryPointClass is null, this value is ignored.
     */
    const skizo::core::CString* EntryPointMethod;

    /**
     * If the domain is marked as untrusted, Permission::demand(..) takes permissions added with ::AddPermission(..)
     * into consideration.
     * Otherwise, treats as the domain as trusted, completely ignoring permission demands.
     * False by default for code hosted from C/C++.
     */
    bool IsUntrusted;

    /**
     * Helps the garbage collector locate the stack limits.
     * Reference an argument of the top function of the current thread ("main" function in case it's the main thread).
     */
    void* StackBase;

    /**
     * See SMemoryManager::Threshold
     */
    so_long MinGCThreshold;

    /**
     * Creates a file named "skizodump.c" in the current directory where it dumps emitted C code.
     */
    bool DumpCCode;

    /**
     * Collecting stacktrace information slows down scripts (up to 15x times). False by default.
    // NOTE Stack trace information may omit some frames if some of method calls were inlined.
    */
    bool StackTraceEnabled;

    /**
     * Profiles the current domain:
     * a) method enter/method leave
     * b) TODO
     */
    bool ProfilingEnabled;

    /**
     * Enables soft-debugging.
     */
    bool SoftDebuggingEnabled;

    /**
     * Explicit null check, as it's clear from the name, explicitly checks in every instance method if 'this' is null
     * (in generated machine code) except for valuetypes as they're never null.
     * Implicit null check relies on more efficient memory protection mechanisms the OS provides, but they're less
	 * reliable and harder to debug.
     *
     * Explicit null check can be turned off in applications practically proved to be stable/safe enough to get away
	 * without this feature.
     *
     * NOTE Ignored by native methods, as they check for null explicitly using the SKIZO_NULL_CHECK macro
	 * disregarding whether this flag is set or not.
     *
     * True by default.
     */
    bool ExplicitNullCheck;

    /**
     * When SafeCallbacks=true, closures that are passed to native code get checked if they are accessed in correct
     * domains before being called. Trying to call a closure outside of its native domain is dangerous and may result
     * in a process crash.
     */
    bool SafeCallbacks;

    /**
     * An option just to compare the impact of branch inlining. Usually should be true by default.
     */
    bool InlineBranching;

    /**
     * Registers a new icall. Every native method defined in the Skizo code must have a corresponding ICall.
     */
    void registerICall(const char* name, void* impl);

    void AddPermission(const skizo::core::CString* permission);

    /**
     * Adds a library path to search assemblies in.
     */
    void AddSearchPath(const char* path);

	/**
     * Every 'break' statement ends up in this callback.
     */
    SKIZO_BREAKPOINTCALLBACK BreakpointCallback;

    /**
     * See the comments for SKIZO_DOMAINCOMPILATIONCALLBACK in skizoscript.h
     */
    SKIZO_DOMAINCOMPILATIONCALLBACK CompilationCallback;

    /**
     * Used to diagnose GC problems.
     */
    bool GCStatsEnabled;

    SDomainCreation()
        : Source(nullptr),
          UseSourceAsPath(true),
          Name(nullptr),
          EntryPointClass(nullptr),
          EntryPointMethod(0),
          IsUntrusted(false),
          StackBase(nullptr),
          MinGCThreshold(SKIZO_MIN_GC_THRESHOLD),
          DumpCCode(false),
          StackTraceEnabled(false),
          ProfilingEnabled(false),
          SoftDebuggingEnabled(false),
          ExplicitNullCheck(true),
          SafeCallbacks(false),
          InlineBranching(true),
          BreakpointCallback(nullptr),
          CompilationCallback(nullptr),
          GCStatsEnabled(false),
          iCalls(new skizo::collections::CHashMap<const char*, void*>()),
          searchPaths(new skizo::collections::CArrayList<const char*>()),
          permissions(new skizo::collections::CArrayList<const skizo::core::CString*>())
    {
    }

protected: // internal
    skizo::core::Auto<skizo::collections::CHashMap<const char*, void*> > iCalls;
    skizo::core::Auto<skizo::collections::CArrayList<const char*> > searchPaths;
    skizo::core::Auto<skizo::collections::CArrayList<const skizo::core::CString*> > permissions;
};

} }

#endif // DOMAINCREATION_H_INCLUDED
