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

#include "ECallDesc.h"
#include "Contract.h"
#include "Domain.h"
#include "ECallCache.h"
#include "Exception.h"

namespace skizo { namespace script {
using namespace skizo::core;

bool SECallDesc::IsValid() const
{
    // FIX Previously used also "EntryPoint" but this is not reliable as this call desc may not have
    // been resolved yet somewhere in the transformer phase.
    return !this->ModuleName.IsEmpty();
}

void SECallDesc::Resolve(CMethod* method)
{
    if(!this->ImplPtr) {
        SKIZO_REQ(this->IsValid(), EC_INVALID_STATE);

        const CDomain* domain = method->DeclaringClass()->DeclaringDomain();
        const SECallCache& eCallCache = domain->ECallCache();

        void* lib = eCallCache.SkizoGetLibrary(this->ModuleName);
        void* implPtr = eCallCache.SkizoGetProcedure(lib, this->EntryPoint);

        this->ImplPtr = implPtr;
    }
}

} }
