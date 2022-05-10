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

#include "Application.h"
#include "Contract.h"
#include "String.h"

namespace skizo { namespace core {

static const CString* g_NewLine = nullptr;
static const CString* g_FileSeparator = nullptr;

// ***********************************
//   INITIALIZATION/DEINITIALIZATION
// ***********************************

void __InitApplication()
{
#ifdef SKIZO_X
    g_NewLine = CString::FromASCII("\n");
    g_FileSeparator = CString::FromASCII("/");
#endif

#ifdef SKIZO_WIN
    g_NewLine = CString::FromASCII("\r\n");
    g_FileSeparator = CString::FromASCII("\\");
#endif
}

void __DeinitApplication()
{
    SKIZO_REQ_PTR(g_NewLine);
    SKIZO_REQ_PTR(g_FileSeparator);

    g_NewLine->Unref();
    g_FileSeparator->Unref();
    g_NewLine = nullptr;
    g_FileSeparator = nullptr;
}

// ***********************************

} }

namespace skizo { namespace core { namespace Application {

const CString* PlatformString(EPlatformString ps)
{
    SKIZO_REQ(ps == E_PLATFORMSTRING_NEWLINE || ps == E_PLATFORMSTRING_FILE_SEPARATOR,
              skizo::core::EC_ILLEGAL_ARGUMENT);

    switch(ps) {
        case E_PLATFORMSTRING_NEWLINE:
            return g_NewLine;

        case E_PLATFORMSTRING_FILE_SEPARATOR:
            return g_FileSeparator;
    }

    return g_NewLine;
}

void Exit(int code)
{
    exit(code);
}

}

} } 
