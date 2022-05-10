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

#include "ref.h"
#include "Object.h"
#include "CoreUtils.h"
#include "Marshal.h"

namespace skizo { namespace collections {
using namespace skizo::core;
using namespace skizo::core::Marshal;

    // **********
    //   Object
    // **********

bool SKIZO_EQUALS(const CObject* obj1, const CObject* obj2)
{
    return CoreUtils::AreObjectsEqual(obj1, obj2);
}

    // ***************
    //   const char*
    // ***************

bool SKIZO_EQUALS(const char* a, const char* b)
{
    if(!a || !b) {
        return a == b;
    } else {
        return strcmp(a, b) == 0;
    }
}

int SKIZO_HASHCODE(const char* a)
{
    int i = 0;
    int h = 0;
    while(char c = a[i++]) {
        h = ((h << 5) - h) + c;
    }

    return h;
}

    // **************
    //   so_char16*
    // **************

bool SKIZO_EQUALS(const so_char16* cs1, const so_char16* cs2)
{
    return so_wcscmp_16bit(cs1, cs2) == 0;
}

int SKIZO_HASHCODE(const so_char16* cs)
{
    int i = 0;
    int h = 0;
    while(so_char16 c = cs[i++]) {
        h = ((h << 5) - h) + c;
    }

    return h;
}

} }
