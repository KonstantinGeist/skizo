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

#include "../../Path.h"
#include "../../String.h"

namespace skizo { namespace io { namespace Path {
using namespace skizo::core;

const CString* GetFullPath(const CString* path)
{
    // TODO uses only MAX_PATH; must prepend "\\?\" to allow
    // paths bigger than MAX_PATH
    WCHAR buf[MAX_PATH];
    GetFullPathName((LPWSTR)path->Chars(),
                    MAX_PATH,
                    buf,
                    NULL);

    Auto<const CString> preR (CString::FromUtf16((so_char16*)buf));
    return Path::Normalize(preR);
}

} } }
