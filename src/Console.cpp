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

#include "Console.h"
#include "String.h"
#include <stdarg.h>

namespace skizo { namespace core {

namespace Console {

void Write(const char* format, ...)
{
    Auto<const CString> r;

    va_list vl;
    va_start(vl, format);

    r.SetPtr(CString::FormatImpl(format, vl));

    // A no-op on most systems, no problem if the wrapped Format(..)
    // throws an exception and this macro isn't called.
    va_end(vl);

    Write(r);    
}

void WriteLine(const char* format, ...)
{
    Auto<const CString> r;

    va_list vl;
    va_start(vl, format);

    r.SetPtr(CString::FormatImpl(format, vl));

    // A no-op on most systems, no problem if the wrapped Format(..)
    // throws an exception and this macro isn't called.
    va_end(vl);

    WriteLine(r);
}

}

} }
