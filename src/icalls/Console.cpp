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

#include "../Console.h"
#include "../Domain.h"

namespace skizo { namespace script {
using namespace skizo::core;

extern "C" {

// TODO security?
void* SKIZO_API _so_Console_readLine()
{
    CDomain* domain = CDomain::ForCurrentThread();

    void* r = nullptr;
    SKIZO_GUARD_BEGIN
        Auto<const CString> daString (Console::ReadLine());
        r = domain->CreateString(daString);
    SKIZO_GUARD_END
    return r;
}

}

} }
