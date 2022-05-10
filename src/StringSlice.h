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

#ifndef SKIZOSTRINGSLICE_H_INCLUDED
#define SKIZOSTRINGSLICE_H_INCLUDED

#include "String.h"

namespace skizo { namespace script {

class CToken;

/**
 * A string slice is basically a lightweight substring which never allocates memory.
 * Used throughout the code as metadata refer to the original code for identifiers.
 */
struct SStringSlice
{
    const skizo::core::CString* String;
    int Start, End;

    SStringSlice(const skizo::core::CString* str);
    SStringSlice();
    SStringSlice(const skizo::core::CString* str, int start, int end);
    SStringSlice(const SStringSlice& slice);
    bool Equals(const skizo::core::CString* str) const;
    bool Equals(const SStringSlice& slice) const;
    bool Equals(const so_char16* chars) const;
    bool EqualsAscii(const char* str) const;
    bool StartsWithAscii(const char* str) const;
    void DebugPrint() const;
    void SetEmpty();
    bool IsEmpty() const;
    int ParseInt(const CToken* errorToken) const;
    float ParseFloat(const CToken* errorToken) const;
    bool TryParseInt(int* out_i) const;
    const skizo::core::CString* ToString() const;

    // Uses CString::ToUtf8(), so all the standard conventions like Utf8Auto or CString::FreeUtf8(..) are appliable.
    char* ToUtf8() const;
};

inline void SKIZO_REF(SStringSlice& v) { }
inline void SKIZO_UNREF(SStringSlice& v) { }
inline bool SKIZO_EQUALS(const SStringSlice& v1, const SStringSlice& v2)
{
    return v1.Equals(v2);
}
inline int SKIZO_HASHCODE(const SStringSlice& v)
{
    int h = 0;
    for(int i = v.Start; i < v.End; i++) {
        h += v.String->Chars()[i] * 31;
    }
    return h;
}
inline bool SKIZO_IS_NULL(const SStringSlice& v)
{
    return false;
}

} }

#endif // SKIZOSTRINGSLICE_H_INCLUDED
