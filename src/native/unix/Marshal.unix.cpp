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

#include "../../Marshal.h"

namespace skizo { namespace core {

int so_wcslen_16bit(const so_char16* str)
{
    const so_char16* s = str;
    while (*s) s++;
    return s - str;
}

so_char16* so_wcscpy_16bit(so_char16* dst, const so_char16* src)
{
   so_char16 *p = dst;
   while ((*p++ = *src++));
   return dst;
}

so_char16* so_wmemcpy_16bit(so_char16* __restrict__ s1, const so_char16* __restrict__ s2, int n)
{
    so_char16* orig_s1 = s1;

    if (s1 == NULL || s2 == NULL || n == 0) {
        return orig_s1;
    }

    for (; n > 0; --n ) {
        *s1++ = *s2++;
    }

    return orig_s1;
}

int so_wcscmp_16bit(const so_char16* cs, const so_char16* ct)
{
    while(*cs == *ct) {
        if(*cs == 0) {
            return 0;
        }
        cs++;
        ct++;
    }

    return *cs - *ct;
}

} }
