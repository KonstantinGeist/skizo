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
#include "../RuntimeHelpers.h"

namespace skizo { namespace script {
extern "C" {

void SKIZO_API _so_Permission_demandImpl(void* soPermObj)
{
    SKIZO_NULL_CHECK(soPermObj);
    CDomain::ForCurrentThread()->SecurityManager().DemandPermission(soPermObj);
}

}
} }
