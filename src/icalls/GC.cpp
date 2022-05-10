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

void SKIZO_API _so_GC_collect()
{
    CDomain* curDomain = CDomain::ForCurrentThread();
    curDomain->MemoryManager().CollectGarbage(false);
}

void SKIZO_API _so_GC_addRoot(void* obj)
{
    SKIZO_NULL_CHECK(obj);

    SKIZO_GUARD_BEGIN
        CDomain::ForCurrentThread()->MemoryManager().AddGCRoot(obj);
    SKIZO_GUARD_END
}

void SKIZO_API _so_GC_removeRoot(void* obj)
{
    SKIZO_NULL_CHECK(obj);

    SKIZO_GUARD_BEGIN
        CDomain::ForCurrentThread()->MemoryManager().RemoveGCRoot(obj);
    SKIZO_GUARD_END
}

void SKIZO_API _so_GC_addMemoryPressure(int i)
{
    CDomain* curDomain = CDomain::ForCurrentThread();
    if(i < 0) {
        curDomain->Abort("Value must be positive.");
    }
    curDomain->MemoryManager().AddMemoryPressure(i);
}

void SKIZO_API _so_GC_removeMemoryPressure(int i)
{
    CDomain* curDomain = CDomain::ForCurrentThread();
    if(i < 0) {
        curDomain->Abort("Value must be positive.");
    }
    curDomain->MemoryManager().RemoveMemoryPressure(i);
}

// TODO include string literals
_so_bool SKIZO_API _so_GC_isValidObject(void* obj)
{
    if(obj) {
        return CDomain::ForCurrentThread()->MemoryManager().IsValidObject(obj);
    } else {
        return false;
    }
}

}

} }
