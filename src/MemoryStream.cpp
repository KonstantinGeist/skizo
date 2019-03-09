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

#include "MemoryStream.h"
#include "Contract.h"
//#include "Exception.h"

namespace skizo { namespace io {
using namespace skizo::core;

CMemoryStream::CMemoryStream(CByteBuffer* bb)
    : m_position(0)
{
    if(bb) {
        m_bb.SetVal(bb);
    } else {
        m_bb.SetPtr(new CByteBuffer());
    }
}

bool CMemoryStream::CanRead() const
{
    return true;
}

bool CMemoryStream::CanWrite() const
{
    return true;
}

bool CMemoryStream::CanSeek() const
{
    return true;
}

void CMemoryStream::ensureBuffer(so_long end)
{
    if(end >= m_bb->Size()) {
        m_bb->AppendBytes(0, end - m_bb->Size());
    }
}

void CMemoryStream::SetPosition(so_long pos)
{
    ensureBuffer(pos + sizeof(so_long));
    m_position = pos;
}

so_long CMemoryStream::GetPosition() const
{
    return m_position;
}

so_long CMemoryStream::Size() const
{
    return m_bb->Size();
}

so_long CMemoryStream::Read(char* buf, so_long count)
{
    SKIZO_REQ_NOT_NEG(count);

    // While writes expand the stream, reads past the stream
    // are truncated.
    if((m_position + count) >= m_bb->Size()) {
        count = m_bb->Size() - m_position;

        SKIZO_REQ((m_position + count) <= m_bb->Size(), EC_OUT_OF_RANGE);
    }

    memcpy(buf, (reinterpret_cast<char*>(m_bb->Bytes())) + m_position, count);
    m_position += count;
    return count;
}

so_long CMemoryStream::Write(const char* buf, so_long count)
{
    SKIZO_REQ_NOT_NEG(count);

    ensureBuffer(m_position + count);
    memcpy((reinterpret_cast<char*>(m_bb->Bytes())) + m_position, buf, count);
    m_position += count;
    return count;
}

so_byte* CMemoryStream::CurrentBytes() const
{
    return m_bb->Bytes() + m_position;
}

void CMemoryStream::Clear()
{
    m_bb->Clear();
    m_position = 0;
}

} }
