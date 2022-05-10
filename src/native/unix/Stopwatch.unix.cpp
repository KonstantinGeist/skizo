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
#include "../../Application.h"
#include "../../Exception.h"

namespace skizo { namespace core {

SStopwatch::SStopwatch()
    : m_started(false)
{
}

void SStopwatch::Start()
{
    m_started = true;

    m_startTicks = Application::TickCount();
}

// TODO safeguard from tickcount rollover
so_long SStopwatch::End()
{
    if(!m_started) {
        SKIZO_THROW(EC_INVALID_STATE);
    }

    m_started = false;

    so_long curTicks = Application::TickCount();
    return curTicks - m_startTicks;
}

} }
