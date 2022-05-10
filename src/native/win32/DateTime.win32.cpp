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

namespace skizo { namespace core {

SDateTime SDateTime::CreateFromSYSTEMTIME(EDateTimeKind kind, const SYSTEMTIME* sysTime)
{
    return SDateTime(
            kind,
            sysTime->wYear,
            sysTime->wMonth, sysTime->wDay, sysTime->wHour,
            sysTime->wMinute, sysTime->wSecond, sysTime->wMilliseconds);
}

void SDateTime::ToSYSTEMTIME(SYSTEMTIME* sysTime) const
{
    sysTime->wYear = m_year;
    sysTime->wMonth = m_month;
    sysTime->wDayOfWeek = 0; // ?
    sysTime->wDay = m_day;
    sysTime->wHour = m_hour;
    sysTime->wMinute = m_minute;
    sysTime->wSecond = m_second;
    sysTime->wMilliseconds = m_ms;
}

SDateTime SDateTime::Now()
{
    SYSTEMTIME utcTime, localTime;
    GetSystemTime(&utcTime);
    SystemTimeToTzSpecificLocalTime(0, &utcTime, &localTime);
    return CreateFromSYSTEMTIME(E_DATETIMEKIND_LOCAL, &localTime);
}

// Why not use the localization functionality Windows provides?
const CString* SDateTime::ToString() const
{
    SYSTEMTIME sysTime;
    ToSYSTEMTIME(&sysTime);

    WCHAR dateBuf[256]; // Must suffice.
    GetDateFormat(LOCALE_USER_DEFAULT, 0, &sysTime, 0, dateBuf, 256);

    WCHAR timeBuf[256]; // Must suffice.
    GetTimeFormat(LOCALE_USER_DEFAULT, 0, &sysTime, 0, timeBuf, 256);

    Auto<const CString> daDateStr (CString::FromUtf16((so_char16*)&dateBuf[0]));
    Auto<const CString> daTimeStr (CString::FromUtf16((so_char16*)&timeBuf[0]));

    return CString::Format("%o %o",
                            static_cast<const CObject*>(daDateStr.Ptr()),
                            static_cast<const CObject*>(daTimeStr.Ptr()));
}

SDateTime SDateTime::ToLocalTime() const
{
    if(m_kind == E_DATETIMEKIND_LOCAL) {
        return *this;
    } else {
        SYSTEMTIME utcTime, localTime;
        ToSYSTEMTIME(&utcTime);
        SystemTimeToTzSpecificLocalTime(0, &utcTime, &localTime);
        return CreateFromSYSTEMTIME(E_DATETIMEKIND_LOCAL, &localTime);
    }
}

} }
