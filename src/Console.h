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

#ifndef CONSOLE_H_INCLUDED
#define CONSOLE_H_INCLUDED

/*
 * Console (the standard output).
 * These functions are required because Windows's printf(..) et al. have a poor support
 * for non-Latin codepages when used with MinGW (at least).
 */

namespace skizo { namespace core {
    class CString;
} }

/**
 * Supported console colors.
 */
enum EConsoleColor
{
    E_CONSOLECOLOR_WHITE,
    E_CONSOLECOLOR_RED,
    E_CONSOLECOLOR_GREEN,
    E_CONSOLECOLOR_BLUE,
    E_CONSOLECOLOR_YELLOW
};

namespace skizo { namespace core { namespace Console {

/**
 * Reads the next line of characters from the standard input stream.
 *
 * @note Compatible with Windows' command line (uses the OEM encoding).
 * @return The next line of characters from the input stream, or null if no
 * more lines are available. The delimiter is not included.
 */
const skizo::core::CString* ReadLine();

/**
 * Writes the string value to the standard output stream.
 *
 * @note Compatible with Windows' command line (uses the OEM encoding).
 */
void Write(const skizo::core::CString* str);

void Write(const char* format, ...);

/**
 * Writes the string value, followed by the current line terminator, to the
 * standard output stream
 */
void WriteLine(const skizo::core::CString* str);

void WriteLine(const char* format, ...);

/**
 * Changes the text color of the console.
 */
void SetForeColor(EConsoleColor color);

}

} }

#endif // CONSOLE_H_INCLUDED
