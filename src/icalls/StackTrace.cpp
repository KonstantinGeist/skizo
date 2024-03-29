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

namespace skizo { namespace script {
using namespace skizo::core;

extern "C" {

void SKIZO_API _so_StackTrace_print()
{
    const CDomain* domain = CDomain::ForCurrentThreadRelaxed();

    // NOTE This can be used when domain creation fails, so we check for null.
    if(domain) {
        domain->PrintStackTrace();
    } else {
        printf("<stack trace information not available>\n");
    }
}

}

} }
