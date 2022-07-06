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

#include "../DateTime.h"
#include "../Domain.h"

namespace skizo { namespace script {
using namespace skizo::core;

extern "C" {

// WARNING! Should be synchronized with datetime.skizo!
struct SkizoDateTime
{
    _so_bool isUtc SKIZO_FIELD;
    int year SKIZO_FIELD;
    int month SKIZO_FIELD;
    int day SKIZO_FIELD;
    int hour SKIZO_FIELD;
    int minute SKIZO_FIELD;
    int second SKIZO_FIELD;
    int ms SKIZO_FIELD;
};

static SDateTime dateTimeSkizoToSkizo(SkizoDateTime* inputDt)
{
    return SDateTime(inputDt->isUtc? E_DATETIMEKIND_UTC: E_DATETIMEKIND_LOCAL,
                          (unsigned short)inputDt->year,
                          (unsigned short)inputDt->month,
                          (unsigned short)inputDt->day,
                          (unsigned short)inputDt->hour,
                          (unsigned short)inputDt->minute,
                          (unsigned short)inputDt->second,
                          (unsigned short)inputDt->ms);
}

static void dateTimeSkizoToSkizo2(SDateTime& src, SkizoDateTime* dst)
{
    dst->isUtc = (src.Kind() == E_DATETIMEKIND_UTC);
    dst->year = src.Year();
    dst->month = src.Month();
    dst->day = src.Day();
    dst->hour = src.Hour();
    dst->minute = src.Minute();
    dst->second = src.Second();
    dst->ms = src.Milliseconds();
}

void SKIZO_API _so_DateTime_verify(void* _dt)
{
    SkizoDateTime* inputDt = (SkizoDateTime*)_dt;

    SKIZO_GUARD_BEGIN
        dateTimeSkizoToSkizo(inputDt);
    SKIZO_GUARD_END
}

void SKIZO_API _so_DateTime_toLocalTimeImpl(void* _src, void* _dst)
{
    SKIZO_GUARD_BEGIN
        SDateTime src (dateTimeSkizoToSkizo((SkizoDateTime*)_src));
        src = (src.ToLocalTime());
        dateTimeSkizoToSkizo2(src, (SkizoDateTime*)_dst);
    SKIZO_GUARD_END
}

void* SKIZO_API _so_DateTime_toStringImpl(void* soDt)
{
    SDateTime daDt (dateTimeSkizoToSkizo((SkizoDateTime*)soDt));
    Auto<const CString> r;
    SKIZO_GUARD_BEGIN
        r.SetPtr(daDt.ToString());
    SKIZO_GUARD_END
    return CDomain::ForCurrentThread()->CreateString(r);
}

void SKIZO_API _so_DateTime_nowImpl(void* soDt)
{
    SKIZO_GUARD_BEGIN
        SDateTime daNow (SDateTime::Now());
        dateTimeSkizoToSkizo2(daNow, (SkizoDateTime*)soDt);
    SKIZO_GUARD_END
}

}

} }
