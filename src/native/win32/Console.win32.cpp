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
#include "../../String.h"

namespace skizo { namespace core { namespace Console {
using namespace skizo::core;

static HANDLE hStdin = NULL;
static HANDLE hStdout = NULL;

const CString* ReadLine()
{
    if(!hStdin) {
        hStdin = GetStdHandle(STD_INPUT_HANDLE);
    }

    if(hStdin) {
        TCHAR buf[256];
        DWORD dwCount = 0;

        ReadConsole(hStdin, &buf, sizeof(buf) / sizeof(TCHAR) - 1, &dwCount, NULL);

        if(dwCount >= 2 &&
            '\n' == buf[dwCount - 1] &&
            '\r' == buf[dwCount - 2]) {
            buf[dwCount - 2] = '\0';
        } else if(dwCount > 0) {
            buf[dwCount] = '\0';
        }

        return CString::FromUtf16((so_char16*)buf);
    } else {
        return CString::CreateEmptyString();
    }
}

void Write(const CString* str)
{
    if(!hStdout) {
        hStdout = GetStdHandle(STD_OUTPUT_HANDLE);
    }

    if(hStdout) {
        DWORD dwCount = 0;
        WriteConsole(hStdout, (TCHAR*)str->Chars(), str->Length(), &dwCount, NULL);
    }
}

void WriteLine(const skizo::core::CString* str)
{
    Write(str);
    printf("\n");
}

void SetForeColor(EConsoleColor color)
{
    if(!hStdout) {
        hStdout = GetStdHandle(STD_OUTPUT_HANDLE);
    }

    WORD attr = 0;

    switch(color) {
        case E_CONSOLECOLOR_RED:
            attr = FOREGROUND_RED | FOREGROUND_INTENSITY;
            break;
        case E_CONSOLECOLOR_YELLOW:
            attr = FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_INTENSITY;
            break;
        case E_CONSOLECOLOR_GREEN:
            attr = FOREGROUND_GREEN | FOREGROUND_INTENSITY;
            break;
        case E_CONSOLECOLOR_BLUE:
            attr = FOREGROUND_BLUE | FOREGROUND_INTENSITY;
            break;
        default: // WHITE
            attr = FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE;
            break;
    }

    SetConsoleTextAttribute(hStdout, attr);
}

} } }
