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

#include "DateTime.h"
#include "Contract.h"

namespace skizo { namespace core {

SDateTime::SDateTime(
              EDateTimeKind kind,
              unsigned short year,
              unsigned short month,
              unsigned short day,
              unsigned short hour,
              unsigned short minute,
              unsigned short second,
              unsigned short ms)
        : m_kind(kind),
          m_year(year), m_month(month), m_day(day),
          m_hour(hour), m_minute(minute), m_second(second), m_ms(ms)
{
    SKIZO_REQ(!(year < 1601 || year > 30827 ||
           month == 0 || month > 12 ||
           day == 0 || day > 31 ||
           hour > 23 || minute > 59 || second > 59 ||
           ms > 999),
           EC_ILLEGAL_ARGUMENT);
}

SDateTime::SDateTime()
    : m_kind(E_DATETIMEKIND_UTC), m_year(1970), m_month(1), m_day(1), m_hour(1),
      m_minute(1), m_second(1), m_ms(1)
{
}

bool SDateTime::Equals(const SDateTime& other) const
{
    if(this->m_kind != other.m_kind) {
        return false;
    }
    if(this->m_year != other.m_year) {
        return false;
    }
    if(this->m_month != other.m_month) {
        return false;
    }
    if(this->m_day != other.m_day) {
        return false;
    }
    if(this->m_hour != other.m_hour) {
        return false;
    }
    if(this->m_minute != other.m_minute) {
        return false;
    }
    if(this->m_second != other.m_second) {
        return false;
    }
    if(this->m_ms != other.m_ms) {
        return false;
    }

    return true;
}

int SDateTime::GetHashCode() const
{
    int hash = 17;
    hash = hash * 23 + m_year;
    hash = hash * 23 + m_month;
    hash = hash * 23 + m_day;
    hash = hash * 23 + m_hour;
    hash = hash * 23 + m_minute;
    hash = hash * 23 + m_second;
    hash = hash * 23 + m_ms;
    return hash;
}

bool SDateTime::IsAfter(const SDateTime& other) const
{
    if(this->m_kind != other.m_kind) {
        SKIZO_THROW_WITH_MSG(EC_ILLEGAL_ARGUMENT, "Cannot compare DateTime objects of different kinds.");
    }

    if(this->m_year < other.m_year) {
        return false;
    }
    if(this->m_month < other.m_month) {
        return false;
    }
    if(this->m_day < other.m_day) {
        return false;
    }
    if(this->m_hour < other.m_hour) {
        return false;
    }
    if(this->m_minute < other.m_minute) {
        return false;
    }
    if(this->m_second < other.m_second) {
        return false;
    }
    if(this->m_ms < other.m_ms) {
        return false;
    }

    return !this->Equals(other);
}

} }
