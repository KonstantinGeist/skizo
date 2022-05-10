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
#include <assert.h>

namespace skizo { namespace script {
using namespace skizo::core;

extern "C" {

void* SKIZO_API _so_Arrays_clone(void* arr)
{
    if(!arr) {
        return nullptr;
    }

    const CClass* arrClass = so_class_of(arr);
    if(arrClass->SpecialClass() != E_SPECIALCLASS_ARRAY) {
        CDomain::Abort("Arrays::clone expects only arrays.");
    }
    assert(arrClass->VirtualTable());
    assert(arrClass->ResolvedWrappedClass());
    const size_t itemSize = arrClass->ResolvedWrappedClass()->GCInfo().SizeForUse;

    // Copies array's contents.
    const SArrayHeader* arrHeader = (SArrayHeader*)arr;
    void* r = CDomain::ForCurrentThread()->CreateArray(arrHeader->length, arrClass->VirtualTable());
    const void* oldArrData = SKIZO_GET_ARRAY_DATA(arr);
    void* newArrData = SKIZO_GET_ARRAY_DATA(r);
    // This is much faster than doing it in Skizo itself.
    memcpy(newArrData, oldArrData, arrHeader->length * itemSize);

    return r;
}

}

} }
