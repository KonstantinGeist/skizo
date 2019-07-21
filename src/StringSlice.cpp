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

#include "StringSlice.h"
#include "Exception.h"
#include "Marshal.h"
#include "ScriptUtils.h"

namespace skizo { namespace script {
using namespace skizo::core;

// ******************
// SkizoStringSlice.
// ******************

SStringSlice::SStringSlice()
    : String(nullptr),
      Start(0),
      End(0)
{
}

SStringSlice::SStringSlice(const CString* str, int start, int end)
    : Start(start), End(end)
{
    this->String = str;
}

SStringSlice::SStringSlice(const CString* str)
{
    if(str) {
        this->String = str;
        this->Start = 0;
        this->End = str->Length();
    } else {
        // HACK Conforms to the requirements of Skizo hashmaps.
        this->String = nullptr;
        this->Start = 0;
        this->End = 0;
    }
}

SStringSlice::SStringSlice(const SStringSlice& slice)
    : Start(slice.Start), End(slice.End)
{
    this->String = slice.String;
}

bool SStringSlice::Equals(const SStringSlice& slice) const
{
    const int length = this->End - this->Start;
    if((slice.End - slice.Start) != length) {
        return false;
    }

    const so_char16* slice1cs = this->String->Chars();
    const so_char16* slice2cs = slice.String->Chars();
    for(int i = 0; i < length; i++) {
        if(slice1cs[this->Start + i] != slice2cs[slice.Start + i]) {
            return false;
        }
    }

    return true;
}

bool SStringSlice::Equals(const CString* str) const
{
    const int strLen = str->Length();
    if((this->End - this->Start) != strLen) {
        return false;
    }
    for(int i = 0; i < strLen; i++) {
        if(str->Chars()[i] != this->String->Chars()[i + this->Start]) {
            return false;
        }
    }
    return true;
}

bool SStringSlice::EqualsAscii(const char* str) const
{
    const int strLen = strlen(str);
    if((this->End - this->Start) != strLen) {
        return false;
    }

    const so_char16* slicecs = this->String->Chars();
    for(int i = 0; i < strLen; i++) {
        if(str[i] != (char)slicecs[this->Start + i]) {
            return false;
        }
    }

    return true;
}

bool SStringSlice::Equals(const so_char16* str) const
{
    const int strLen = so_wcslen_16bit(str);
    if((this->End - this->Start) != strLen) {
        return false;
    }

    const so_char16* slicecs = this->String->Chars();
    for(int i = 0; i < strLen; i++) {
        if(str[i] != slicecs[this->Start + i]) {
            return false;
        }
    }

    return true;
}

bool SStringSlice::StartsWithAscii(const char* str) const
{
    const int strLen = strlen(str);
    if((this->End - this->Start) < strLen) {
        return false;
    }
    const so_char16* slicecs = this->String->Chars();
    for(int i = 0; i < strLen; i++) {
        if(str[i] != (char)slicecs[this->Start + i]) {
            return false;
        }
    }
    return true;
}

const CString* SStringSlice::ToString() const
{
    const int count = this->End - this->Start;
    if(this->String && count) {
        return this->String->Substring(this->Start, count);
    } else {
        return CString::CreateEmptyString();
    }
}

char* SStringSlice::ToUtf8() const
{
    Auto<const CString> tmp (this->ToString());
    return tmp->ToUtf8();
}

void SStringSlice::DebugPrint() const
{
    Auto<const CString> str (this->ToString());
    str->DebugPrint();
}

void SStringSlice::SetEmpty()
{
    this->String = nullptr;
    this->Start = 0;
    this->End = 0;
}

bool SStringSlice::IsEmpty() const
{
    return (this->End - this->Start) == 0;
}

int SStringSlice::ParseInt(const CToken* errorToken) const
{
    try {
        Auto<const CString> str (this->ToString());
        return str->ParseInt();
    } catch(const SException& e) {
        ScriptUtils::FailT("Integer constant is too small or too large.", errorToken);
        return 0;
    }
}

bool SStringSlice::TryParseInt(int* out_i) const
{
    Auto<const CString> str (this->ToString());
    return str->TryParseInt(out_i);
}

float SStringSlice::ParseFloat(const CToken* errorToken) const
{
    try {
        Auto<const CString> str (this->ToString());
        return str->ParseFloat();
    } catch(SException& e) {
        ScriptUtils::FailT("Float constant is too small or too large.", errorToken);
        return 0.0f;
    }
}

} }
