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

#include "init.h"
#include "Application.h"
#include "Domain.h"
#include "Security.h"
#include "Thread.h"
#include <assert.h>

namespace skizo { namespace core {

using namespace skizo::script;

static bool g_isSkizoInitialized = false;

void InitSkizo()
{
    assert(!g_isSkizoInitialized);

    // Various initializers below depend on CObject-derived classes such as
    // CHashMap, so we have to set to g_isSkizoInitialized early on so that
    // CHashMap's inherited constructors (from CObject) did not fail on
    // assert(IsCoreInitialized()). CHashMap's are safe to use as they do not
    // depend on other types and therefore do not introduce intialization fiasco
    // problems.
    g_isSkizoInitialized = true;

//#ifdef SKIZO_WIN
    // Required for ShellExecuteEx(..)
    //CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
//#endif

    __InitThread();
    __InitThreadNative();
    __InitApplication();
    __InitDomain();
    __InitSecurity();
}

void DeinitSkizo()
{
    assert(g_isSkizoInitialized);

    __DeinitSecurity();
    __DeinitDomain();
    __DeinitApplication();
    __DeinitThreadNative();
    __DeinitThread();

//#ifdef SKIZO_WIN
    // Paired with CoInitializeEx(..) above.
    //CoUninitialize();
//#endif

    g_isSkizoInitialized = false;
}

bool IsSkizoInitialized()
{
    return g_isSkizoInitialized;
}

} }
