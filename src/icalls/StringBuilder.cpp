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

#include "../StringBuilder.h"
#include "../Domain.h"
#include "../NativeHeaders.h"
#include "../RuntimeHelpers.h"

namespace skizo { namespace script {
using namespace skizo::core;

extern "C" {

void* SKIZO_API _so_StringBuilder_createImpl(int cap)
{
    if(cap <= 0) {
        CDomain::Abort("Capacity must be a positive value.");
    }

    return new CStringBuilder(cap);
}

void SKIZO_API _so_StringBuilder_destroyImpl(void* pSelf)
{
    if(pSelf) { // exception in the ctor produces a dangling pointer
        ((CStringBuilder*)pSelf)->Unref(); // TODO gets repetitive
    }
}

void SKIZO_API _so_StringBuilder_appendImpl(void* pSelf, void* str)
{
    SKIZO_NULL_CHECK(str);

    ((CStringBuilder*)pSelf)->Append(so_string_of(str));
}

void* SKIZO_API _so_StringBuilder_toStringImpl(void* pSelf)
{
    Auto<const CString> str (((CStringBuilder*)pSelf)->ToString());
    return CDomain::ForCurrentThread()->CreateString(str, false);
}

int SKIZO_API _so_StringBuilder_lengthImpl(void* pSelf)
{
    return ((CStringBuilder*)pSelf)->Length();
}

void SKIZO_API _so_StringBuilder_clearImpl(void* pSelf)
{
    ((CStringBuilder*)pSelf)->Clear();
}

}

} }
