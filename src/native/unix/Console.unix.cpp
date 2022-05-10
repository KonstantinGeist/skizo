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

#include "../../Console.h"
#include "../../Exception.h"
#include "../../String.h"

#include <stdio.h>

namespace skizo { namespace core { namespace Console {
using namespace skizo::core;

const CString* ReadLine()
{
    const CString* r;

    // Assumes GNU C.
    char* buffer = NULL;
    int read;
    size_t len;
    read = getline(&buffer, &len, stdin);
    if (read < 1 || !buffer) {
        r = CString::CreateEmptyString();
    } else {
        len = strlen(buffer);

        // Removes the delimiter.
        if(buffer[len - 1] == '\n') {
            buffer[len - 1] = 0;
        }

        r = CString::FromUtf8(buffer);
    }
    free(buffer);

    return r;
}

void Write(const CString* str)
{
    // Unlike on Windows, it is enough to just use "printf", as the console already supports
    // Unicode.
    Utf8Auto cStr (str->ToUtf8());
    printf("%s", cStr.Ptr());
}

void WriteLine(const skizo::core::CString* str)
{
    Utf8Auto cStr (str->ToUtf8());
    printf("%s\n", cStr.Ptr());
}

void SetForeColor(EConsoleColor color)
{
    switch(color) {
        case E_CONSOLECOLOR_RED:
            printf("\x1b[31m");
            break;
        case E_CONSOLECOLOR_YELLOW:
            printf("\x1b[33m");
            break;
        case E_CONSOLECOLOR_GREEN:
            printf("\x1b[32m");
            break;
        case E_CONSOLECOLOR_BLUE:
            printf("\x1b[34m");
            break;
        default:
            printf("\x1b[0m"); // resets the color
            break;
    }
}

} } }
