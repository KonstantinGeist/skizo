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

#include "../../Guid.h"
#include <objbase.h>

namespace skizo { namespace core { namespace Guid {

#define MS_GUID_SIZE 40

static void toLower(so_char16* guidStr)
{
    while(*guidStr++) {
        const so_char16 c = *guidStr;
        if(c >= SKIZO_CHAR('A') && c <= SKIZO_CHAR('F')) {
            *guidStr = SKIZO_CHAR('a') + (c - SKIZO_CHAR('A'));
        }
    }
}

const CString* NewGuid()
{
    GUID guid;
    const HRESULT hRes = CoCreateGuid(&guid);
    if(hRes != S_OK) {
        memset(&guid, 0, sizeof(guid));
    }

    so_char16 guidStr[MS_GUID_SIZE];
    StringFromGUID2(guid, reinterpret_cast<wchar_t*>(&guidStr[0]), MS_GUID_SIZE);

    // Skips { and } added by the Microsoft implementation.
    so_char16* guidStrStart = &guidStr[1];
    guidStrStart[MS_GUID_SIZE - 4] = 0;
    // The Microsoft implementation outputs in upper case.
    toLower(guidStr);

    return CString::FromUtf16(guidStrStart);
}

} } }