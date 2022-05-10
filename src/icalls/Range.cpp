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
#include "../NativeHeaders.h"
#include "../RuntimeHelpers.h"

namespace skizo { namespace script {

extern "C" {

void SKIZO_API _so_Range_loop(SRange range, void* rangeLooper)
{
    SKIZO_NULL_CHECK(rangeLooper);

    typedef void (SKIZO_API * _FRangeLooper)(void*, int);
    _FRangeLooper rangeLoopFunc = (_FRangeLooper)so_invokemethod_of(rangeLooper);

    for(int i = range.from; i < range.to; i++) {
         rangeLoopFunc(rangeLooper, i);
    }
}

// Same as "loop", except has a stepping variable.
void SKIZO_API _so_Range_step(SRange range, int step, void* rangeLooper)
{
    SKIZO_NULL_CHECK(rangeLooper);

    typedef void (SKIZO_API * _FRangeLooper)(void*, int);
    _FRangeLooper rangeLoopFunc = (_FRangeLooper)so_invokemethod_of(rangeLooper);

    for(int i = range.from; i < range.to; i += step) {
         rangeLoopFunc(rangeLooper, i);
    }
}

}

} }
