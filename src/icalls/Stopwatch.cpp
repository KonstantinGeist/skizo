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
#include "../Stopwatch.h"

namespace skizo { namespace script {
using namespace skizo::core;

extern "C" {

void* SKIZO_API _so_Stopwatch_startImpl()
{
    SStopwatch _sw;
    CBoxedStruct<SStopwatch>* sw = new CBoxedStruct<SStopwatch>(_sw);
    sw->Value.Start();
    return sw;
}

int SKIZO_API _so_Stopwatch_endImpl(void* pSelf)
{
    // TODO casts long to int?
    int r = 0;
    SKIZO_GUARD_BEGIN
        r = (int)static_cast<CBoxedStruct<SStopwatch>*>(pSelf)->Value.End();
    SKIZO_GUARD_END
    return r;
}

void SKIZO_API _so_Stopwatch_destroyImpl(void* pSelf)
{
    if(pSelf) { // guideline to check for self in dtors
        static_cast<CBoxedStruct<SStopwatch>*>(pSelf)->Unref();
    }
}

}

} }
