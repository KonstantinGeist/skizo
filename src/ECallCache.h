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

#ifndef ECALLCACHE_H_INCLUDED
#define ECALLCACHE_H_INCLUDED

#include "HashMap.h"
#include "StringSlice.h"

namespace skizo { namespace script {

/**
 * Embedded into CDomain; used to cache native module-related data.
 * For example, on Windows, we don't want to reload a library for every ecall.
 */
struct SECallCache
{
    /**
     * On domain teardown, unloads native modules.
     */
    ~SECallCache();

    /**
     * OS-specific code for locating ECalls in external nmodules.
     * @note Calls ScriptUtils::FailXXX on error.
     */
    void* SkizoGetLibrary(const SStringSlice& name) const;
    void* SkizoGetProcedure(void* nmodule, const SStringSlice& procName) const;

private:
    // Maps nmodule names to OS-dependent handles.
    mutable skizo::collections::CHashMap<SStringSlice, void*> m_moduleCache;
};

} }

#endif // ECALLCACHE_H_INCLUDED
