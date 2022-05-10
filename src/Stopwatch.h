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

#ifndef STOPWATCH_H_INCLUDED
#define STOPWATCH_H_INCLUDED

#include "basedefs.h"

namespace skizo { namespace core {
class CString;

/**
 * An object that measures elapsed time.
 */
struct SStopwatch
{
private:
    bool m_started;

#ifdef SKIZO_WIN
    double m_hiFreq;

    union {
        so_long m_startTicks;
        so_long m_fallbackStartTicks;
    };
#else
    so_long m_startTicks;
#endif

public:
    SStopwatch();

    /**
     * Starts measuring elapsed time for an interval.
     */
    void Start();

    /**
     * Gets the total elapsed time measured.
     */
    so_long End();
    
    int GetHashCode() const { return 0; }
    const CString* ToString() const { return nullptr; } // TODO
};

} }

#endif // STOPWATCH_H_INCLUDED
