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

#include "../Random.h"
#include "../Domain.h"

namespace skizo { namespace script {
using namespace skizo::core;

extern "C" {

void* SKIZO_API _so_Random_createImpl()
{
    return static_cast<void*>(new CRandom());
}

void* SKIZO_API _so_Random_createFromSeedImpl(int seed)
{
    return static_cast<void*>(new CRandom(seed));
}

void SKIZO_API _so_Random_destroyImpl(void* pSelf)
{
    if(pSelf) // guideline to check for self in dtors
        static_cast<CRandom*>(pSelf)->Unref();
}

int SKIZO_API _so_Random_nextIntImpl(void* pSelf, int min, int max)
{
    if(min > max) {
        CDomain::Abort("Min must be less than max.");
    }

    return static_cast<CRandom*>(pSelf)->NextInt(min, max);
}

float SKIZO_API _so_Random_nextFloatImpl(void* pSelf)
{
    return (float)static_cast<CRandom*>(pSelf)->NextDouble();
}

}

} }
