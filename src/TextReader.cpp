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

#include "TextReader.h"
#include "ByteBuffer.h"
#include "Contract.h"
#include "Stream.h"
#include "String.h"

namespace skizo { namespace io {
using namespace skizo::core;

CTextReader::CTextReader(CStream* wrapped)
    : m_lineNumber(0),
      m_bb(nullptr)
{
    // Pre-condition.
    SKIZO_REQ(wrapped->CanRead(), EC_ILLEGAL_ARGUMENT);

    memset(m_buffer, 0, sizeof(m_buffer));

    m_wrapped = wrapped;
    wrapped->Ref();
}

CTextReader::~CTextReader()
{
    m_wrapped->Unref();
    if(m_bb) {
        m_bb->Unref();
    }
}

// NOTE supports only UTF8 BOM.
static bool startsWithBom(CByteBuffer* bb)
{
    if(bb->Size() >= 3) {
        so_byte* bytes = bb->Bytes();
        return (bytes[0] == 0xEF
             && bytes[1] == 0xBB
             && bytes[2] == 0xBF);
    } else {
        return false;
    }
}

const CString* CTextReader::ReadLine()
{
    if(!m_bb) {
        m_bb = new CByteBuffer();
    } else {
        m_bb->Clear();
    }

    /* To be sure. */
    memset(m_buffer, 0, sizeof(m_buffer));

    so_long bytesRead;

again:
    {}
    bytesRead = m_wrapped->Read((char*)m_buffer, sizeof(m_buffer));

    /* End of file. */
    if(bytesRead == 0) {
        return nullptr;
    }

    char isR = 0;

    /* OK, now the buffer can contain multiple lines.
       Search for one line ('\n') and move the file pointer back. */
    so_long cnt = 0, i;
    for(i = 0; i < bytesRead; i++) {

        if(m_buffer[i] == '\r' || m_buffer[i] == '\n') {
            if(m_buffer[i] == '\r') {
                isR = 1;
            }

            /* The string is empty because CRLF is in the very beginning. */
            /* Do not forget to adjust the file pointer. */
            if(i == 0) {
                m_wrapped->SetPosition(m_wrapped->GetPosition() - (bytesRead - 1 - isR));
                goto last_line;
            }

            cnt = i;
            break;
        }

    }

    // The buffer contains no '\n's. Appends the whole buffer and repeat.
    if(cnt == 0) {
        m_bb->AppendBytes(m_buffer, bytesRead);

        if(bytesRead == sizeof(m_buffer)) {
            goto again;
        }
    } else {
        m_bb->AppendBytes(m_buffer, cnt);

        // Sets it back.
        m_wrapped->SetPosition(m_wrapped->GetPosition() - (bytesRead - cnt - 1 - isR));
    }

last_line:
    if(m_bb->Size() == 0) {
        // TODO use intern
        return CString::CreateEmptyString();
    }

    // Null termination in the resulting utf8 string.
    m_bb->AppendByte((so_byte)0);

    char* cs = (char*)m_bb->Bytes();
    // Checks for BOM.
    if(m_lineNumber == 0 && startsWithBom(m_bb)) {
        // Skips BOM.
        cs += 3;
    }

    // File streams deals only with UTF8.
    const CString* r = CString::FromUtf8(cs);
    m_lineNumber++;
    return r;
}

} }
