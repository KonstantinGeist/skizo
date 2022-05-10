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

#include "../Domain.h"
#include "../ScriptUtils.h"

namespace skizo { namespace script {
using namespace skizo::core;

extern "C" {

void* SKIZO_API _so_char_toString(_so_char c)
{
    CDomain* domain = CDomain::ForCurrentThread();
    const so_char16 dcs[2] = { (so_char16)c, 0 };

    void* r = nullptr;
    SKIZO_GUARD_BEGIN
        Auto<const CString> _r (CString::FromUtf16(dcs));
        r = domain->CreateString(_r, false);
    SKIZO_GUARD_END
    return r;
}

int SKIZO_API _so_char_hashCode(_so_char c)
{
    return (int)c;
}

_so_bool SKIZO_API _so_char_equals(_so_char c, void* otherObj)
{
    return BoxedEquals(&c, sizeof(_so_char), otherObj, E_PRIMTYPE_CHAR);
}

}

} }
