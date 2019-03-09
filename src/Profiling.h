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

#ifndef PROFILING_H_INCLUDED
#define PROFILING_H_INCLUDED

#include "ArrayList.h"
#include "Domain.h"

namespace skizo { namespace script {

/**
 * @note Native methods, methods defined in primitives (int, bool etc.), inlined methods, methods that were never
 * called are omitted.
 */
class CProfilingInfo: public skizo::core::CObject
{
    friend class CDomain;

public:
    void SortByTotalTimeInMs();
    void SortByAverageTimeInMs();
    void SortByNumberOfCalls();

    /**
     * Prints the profiling info to the console.
     */
    void DumpToConsole() const;

    /**
     * Dumps the profiling info into file "profile.txt" in the current directory.
     */
    void DumpToDisk() const;

protected: // internal
    skizo::core::Auto<const CDomain> m_domain;
    skizo::core::Auto<skizo::collections::CArrayList<CMethod*> > m_methods;
    so_long m_totalTime;
    CProfilingInfo(const CDomain* domain);

    void _dumpImpl(bool dumpToDisk) const;
};

} }

#endif // PROFILING_H_INCLUDED
