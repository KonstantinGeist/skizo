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

#include "../Application.h"
#include "../Domain.h"

namespace skizo { namespace script {
using namespace skizo::core;

extern "C" {

// TODO rather implement it as "_so_Application_getString"
void* SKIZO_API _so_Application_NEWLINE()
{
    CDomain* curDomain = CDomain::ForCurrentThread();

    const CString* r = Application::PlatformString(E_PLATFORMSTRING_NEWLINE);
    return curDomain->CreateString(r, true);
}

void SKIZO_API _so_Application_exit(int code)
{
    CDomain* curDomain = CDomain::ForCurrentThread();

    if(curDomain->IsBaseDomain() && curDomain->IsTrusted()) {
        Application::Exit(code);
    } else {
        CDomain::Abort("Only a trusted base domain is allowed to call Application::exit(..)");
    }
}

void* SKIZO_API _so_Application_exeFileName()
{
    CDomain::DemandPermission("FileIOPermission");

    Auto<const CString> exeFileName (Application::GetExeFileName());
    return CDomain::ForCurrentThread()->CreateString(exeFileName);
}

int SKIZO_API _so_Application_processorCount()
{
    CDomain::DemandPermission("EnvironmentPermission");

    return Application::GetProcessorCount();
}

int SKIZO_API _so_Application_tickCount()
{
    //CDomain::DemandPermission("EnvironmentPermission");

    return Application::TickCount();
}

void* SKIZO_API _so_Application_osVersion()
{
    CDomain* curDomain = CDomain::ForCurrentThread();
    curDomain->SecurityManager().DemandPermission("EnvironmentPermission");

    Auto<const CString> osVersion (Application::GetOSVersion());
    return curDomain->CreateString(osVersion);
}

}

} }
