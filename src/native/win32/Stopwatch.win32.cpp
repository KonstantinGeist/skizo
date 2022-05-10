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

#include "../../Stopwatch.h"
#include "../../Exception.h"
#include "../../Application.h"

namespace skizo { namespace core {

SStopwatch::SStopwatch()
    : m_started(false)
{
    LARGE_INTEGER li;
    if(QueryPerformanceFrequency(&li) && li.QuadPart > 0) {
        m_hiFreq = double(li.QuadPart) / 1000.0;
    } else {
        m_hiFreq = 0.0;
    }
}

void SStopwatch::Start()
{
    m_started = true;

    if(m_hiFreq) { //-V550
        LARGE_INTEGER li;
        QueryPerformanceCounter(&li);
        m_startTicks = li.QuadPart;
    } else {
        m_fallbackStartTicks = Application::TickCount();
    }
}

// TODO safeguard from tickcount rollover
so_long SStopwatch::End()
{
    if(!m_started) {
        SKIZO_THROW(EC_INVALID_STATE);
    }
    m_started = false;

    if(m_hiFreq) { //-V550
        LARGE_INTEGER li;
        QueryPerformanceCounter(&li);
        return (so_long)((double(li.QuadPart - m_startTicks) / m_hiFreq) + 0.5);
    } else {
        so_long curTicks = Application::TickCount();
        return curTicks - m_fallbackStartTicks;
    }
}

} }
