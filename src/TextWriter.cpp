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

#include "TextWriter.h"
#include "Application.h"
#include "Contract.h"
#include "Stream.h"
#include "String.h"

namespace skizo { namespace io {
using namespace skizo::core;

// Currently CFileStream is also buffered (with help of CRT),
// however CTextWriter buffers ouput in utf16, so we
// don't need to marshal strings on each call.

CTextWriter::CTextWriter(CStream* wrapped)
    : m_wrapped(wrapped)
{
    // Pre-condition.
    SKIZO_REQ(wrapped->CanWrite(), EC_ILLEGAL_ARGUMENT);

    wrapped->Ref();
}

CTextWriter::~CTextWriter()
{
    //Flush();
    m_wrapped->Unref();
}

void CTextWriter::Flush()
{
    // Do nothing.
}

void CTextWriter::Write(const CString* str)
{
    char* const cs = str->ToUtf8();
    m_wrapped->Write(cs, strlen(cs));
    CString::FreeUtf8(cs);
}

void CTextWriter::Write(so_char16 c)
{
    m_wrapped->Write((char*)&c, sizeof(c));
}

void CTextWriter::Write(const char* cs, so_long sz)
{
    this->Flush();
    m_wrapped->Write(cs, sz);
}

void CTextWriter::WriteLine()
{
    this->Write(Application::PlatformString(E_PLATFORMSTRING_NEWLINE));
}

} }
