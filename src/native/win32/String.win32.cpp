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
#include "../../Contract.h"
#include "../../Exception.h"

namespace skizo { namespace core {

// FIX fixes bad MinGW headers
#ifndef LINGUISTIC_IGNORECASE
    #define LINGUISTIC_IGNORECASE 16
#endif
#ifndef SORT_DIGITSASNUMBERS
    #define SORT_DIGITSASNUMBERS 8
#endif

char* CString::ToCLibString() const
{
    char* buffer;
    if(m_length == 0) {
        buffer = new char[1];
        buffer[0] = 0;
        return buffer;
    }

    const int charCount = WideCharToMultiByte(CP_ACP,
                                              0, //MB_ERR_INVALID_CHARS,
                                             (WCHAR*)&m_chars,
                                             -1,   // The string is null terminated.
                                              0, 0, // These two zeros tell that we just probe the size.
                                              NULL,
                                              NULL);
    if(!charCount) {
        SKIZO_THROW(EC_BAD_FORMAT);
        return nullptr; // Satisfies static analysis.
    }

    buffer = new char[charCount]; // Null termination is included.

    const int h = WideCharToMultiByte(CP_ACP,
                                      0, //MB_ERR_INVALID_CHARS,
                                     (WCHAR*)&m_chars,
                                     -1,   // The string is null terminated.
                                      buffer,
                                      charCount,
                                      NULL,
                                      NULL);
    if(!h || h == 0xFFFD) {
        delete [] buffer;
        SKIZO_THROW(EC_BAD_FORMAT);
        return nullptr; // Satisfies static analysis.
    }

    return buffer;
}

const CString* CString::ToLowerCase() const
{
    // Windows' CharLower() converts characters in-place, so we have to clone
    // our immutable string.
    const CString* r = this->Clone();
    CharLower((LPTSTR)r->Chars());
    return r;
}

const CString* CString::ToUpperCase() const
{
    // Windows' CharUpper() converts characters in-place, so we have to clone
    // our immutable string.
    const CString* r = this->Clone();
    CharUpper((LPTSTR)r->Chars());
    return r;
}

int CString::CompareTo(const CString* other) const
{
    const int r = CompareString(LOCALE_USER_DEFAULT,
                                LINGUISTIC_IGNORECASE
                                | SORT_DIGITSASNUMBERS, // necessary for sorting in PathChooser!
                               (LPCTSTR)&this->m_chars,
                                m_length,
                               (LPCTSTR)&other->m_chars,
                                other->m_length);
    SKIZO_REQ(r, EC_MARSHAL_ERROR);
    if(r == CSTR_LESS_THAN) {
        return -1;
    } else if(r == CSTR_GREATER_THAN) {
        return 1;
    } else {
        return 0;
    }
}

} }
