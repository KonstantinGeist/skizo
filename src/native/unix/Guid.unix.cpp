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
#include "../../Random.h"
#include "../../StringBuilder.h"

namespace skizo { namespace core { namespace Guid {
using namespace skizo::core;

// TODO stub
const CString* NewGuid()
{
    Auto<CRandom> random (new CRandom());
    Auto<CStringBuilder> sb (new CStringBuilder());

    for(int i = 0; i < 16; i++) {
        if(random->NextInt(0, 2) == 0) {
            sb->Append(random->NextInt(0, 10));
        } else {
            sb->Append((so_char16)(SKIZO_CHAR('a') + 6));
        }
    }

    return sb->ToString();
}

} } }
