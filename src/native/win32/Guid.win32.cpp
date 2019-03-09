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

const CString* NewGuid()
{
    GUID guid = { 0 };
    wchar_t szGuidW[80] = { 0 };
    CoCreateGuid(&guid);
    StringFromGUID2(guid, szGuidW, 40);

    return CString::FromUtf16(reinterpret_cast<so_char16*>(&szGuidW[0]));
}

} } }
