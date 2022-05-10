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

#ifndef DATETIME_H_INCLUDED
#define DATETIME_H_INCLUDED

#include "Object.h"

namespace skizo { namespace core {
class CString;

/**
 * Specifies whether a SDateTime object represents a local time or a Coordinated
 * Universal Time (UTC).
 */
enum EDateTimeKind
{
    /**
     * The time represented is local time.
     */
    E_DATETIMEKIND_LOCAL = 0,

    /**
     * The time represented is UTC.
     */
    E_DATETIMEKIND_UTC = 1
};

/**
 * Represents an instant in time, typically expressed as a date and time of day.
 */
struct SDateTime
{
public:
#ifdef SKIZO_WIN
    void ToSYSTEMTIME(SYSTEMTIME* out_sysTime) const;
    static SDateTime CreateFromSYSTEMTIME(EDateTimeKind kind, const SYSTEMTIME* sysTime);
#endif

    SDateTime(EDateTimeKind kind,
              unsigned short year,
              unsigned short month,
              unsigned short day,
              unsigned short hour,
              unsigned short minute,
              unsigned short second,
              unsigned short ms);

    SDateTime();

    inline EDateTimeKind Kind() const { return m_kind; }
    inline int Year() const { return m_year; }
    inline int Month() const { return m_month; }
    inline int Day() const { return m_day; }
    inline int Hour() const { return m_hour; }
    inline int Minute() const { return m_minute; }
    inline int Second() const { return m_second; }
    inline int Milliseconds() const { return m_ms; }

    /**
     * Converts the value of the current DateTime object to its equivalent string
     * representation using the formatting conventions of the current culture.
     */
    const CString* ToString() const;

    /**
     * Returns true if the two DateTime objects are completely equal.
     */
    bool Equals(const SDateTime& other) const;

    /**
     * Returns the hash code for this instance.
     */
    int GetHashCode() const;

    /**
     * Gets a SDateTime object that is set to the current date and time on this
     * computer, expressed as the local time.
     */
    static SDateTime Now();

    /**
     * Converts the value of the current DateTime object to local time.
     */
    SDateTime ToLocalTime() const;

    /**
     * Returns true if this DateTime object is after/later than `other` (but not equal).
     */
    bool IsAfter(const SDateTime& other) const;

private:
    EDateTimeKind m_kind;
    unsigned short m_year, m_month, m_day, m_hour, m_minute, m_second, m_ms;

};

} }

#endif // DATETIME_H_INCLUDED
