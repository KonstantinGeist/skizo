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

#include "../../DateTime.h"
#include "../../String.h"
#include "../../Exception.h"

#include <time.h>

namespace skizo { namespace core {

SDateTime SDateTime::Now()
{
    time_t t = time(NULL);
    struct tm tm = *localtime(&t);

    return SDateTime(
            E_DATETIMEKIND_LOCAL,
            tm.tm_year + 1900,
            tm.tm_mon + 1,
            tm.tm_mday,
            tm.tm_hour,
            tm.tm_min,
            tm.tm_sec,
            0); // 'ms' not supported
}

const CString* SDateTime::ToString() const
{
    return CString::Format("%d-%d-%d %d:%d:%d",
                           m_year, m_month, m_day, m_hour, m_minute, m_second);
}

SDateTime SDateTime::ToLocalTime() const
{
    if(m_kind == E_DATETIMEKIND_LOCAL)
        return *this;
    else {
        // TODO dummy
        return *this;
    }
}

} }
