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

void* SKIZO_API _so_intptr_toString(void* ptr)
{
    CDomain* domain = CDomain::ForCurrentThread();
    void* r = nullptr;
    SKIZO_GUARD_BEGIN
        Auto<const CString> source (CoreUtils::PtrToString(ptr));
        r = domain->CreateString(source, false);
    SKIZO_GUARD_END
    return r;
}

int SKIZO_API _so_intptr_hashCode(void* ptr)
{
    return (int)ptr;
}

_so_bool SKIZO_API _so_intptr_equals(void* ptr, void* otherObj)
{
    return BoxedEquals(&ptr, sizeof(void*), otherObj, E_PRIMTYPE_INTPTR);
}

}

} }
