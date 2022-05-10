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

#include "../../String.h"
#include "../../Exception.h"

#include <string.h>
#include <assert.h>
#include <cwctype>

namespace skizo { namespace core {

char* CString::ToCLibString() const
{
    return this->ToUtf8();
}

const CString* CString::ToLowerCase() const
{
    so_char16* obuf;
    const so_char16* ibuf = this->Chars();
    int length = this->Length();

    const CString* r = CString::CreateBuffer(length, &obuf);

    for(int i = 0; i < length; i++) {
        // WARNING Truncates 4-byte values to 2-byte, OK for
        // most cases.
        so_char16 ochar = (so_char16)towlower((wint_t)ibuf[i]);
        obuf[i] = ochar;
    }

    return r;
}

const CString* CString::ToUpperCase() const
{
    so_char16* obuf;
    const so_char16* ibuf = this->Chars();
    int length = this->Length();

    const CString* r = CString::CreateBuffer(length, &obuf);

    for(int i = 0; i < length; i++) {
        // WARNING Truncates 4-byte values to 2-byte, OK for
        // most cases.
        so_char16 ochar = (so_char16)towupper((wint_t)ibuf[i]);
        obuf[i] = ochar;
    }

    return r;
}

int CString::CompareTo(const CString* that) const
{
    Utf8Auto thisCObj (this->ToUtf8());
    Utf8Auto thatCObj (that->ToUtf8());

    return strcoll(thisCObj.Ptr(), thatCObj.Ptr());
}

} }
