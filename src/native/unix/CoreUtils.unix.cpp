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

#include "../../CoreUtils.h"
#include "../../String.h"

namespace skizo { namespace core { namespace CoreUtils {
using namespace skizo::core;

const CString* MemorySizeToString(so_long sz)
{
    return CString::Format("%l KB", sz / 1024);
}

void ShowMessage(const CString* msg, bool isFatal)
{
    msg->DebugPrint();
    printf("\n");
}

} } }
