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

#include "ByteBuffer.h"
#include "Contract.h"
#include "CoreUtils.h"
#include "Exception.h"

namespace skizo { namespace io {
using namespace skizo::core;

void CByteBuffer::appendBytesGeneric(const so_byte* bytes, so_long count)
{
#ifdef SKIZO_CONTRACT
    const so_long cctSize = this->Size();
#endif

    if(((float)(m_size + count) / (float)m_cap) >= SKIZO_BYTEBUFFER_GROW_FACTOR) {
        m_cap = m_cap * 2 + count;
        m_bytes = CoreUtils::ReallocArray<so_byte>(m_bytes, m_size, m_cap *= 2);
    }

    if(bytes) {
        memcpy(m_bytes + m_size, bytes, count);
    }

    m_size += count;

#ifdef SKIZO_CONTRACT
    // Post-condition.
    SKIZO_REQ_EQUALS(cctSize + count, this->Size());
#endif
}

void CByteBuffer::AppendBytes(const so_byte* bytes, so_long count)
{
    SKIZO_REQ_NOT(m_isFixed, EC_INVALID_STATE);
    SKIZO_REQ_NOT_NEG(count);

    appendBytesGeneric(bytes, count);
}

void CByteBuffer::AppendByte(so_byte b)
{
    const so_byte bs[1] = { b };
    this->AppendBytes(bs, 1);
}

void CByteBuffer::Clear()
{
    SKIZO_REQ_NOT(m_isFixed, EC_INVALID_STATE);

    m_size = 0;
}

CByteBuffer::CByteBuffer(so_long cap)
{
    SKIZO_REQ_NOT_NEG(cap);

    m_size = 0;
    m_cap = cap? cap: SKIZO_DEF_BYTEBUFFER_CAP;
    m_initCap = m_cap;
    m_bytes = new so_byte[m_cap];
    m_isFixed = false;
}

CByteBuffer::~CByteBuffer()
{
    SKIZO_REQ_PTR(m_bytes);
    delete [] m_bytes;
}

void CByteBuffer::SetFixed(bool b)
{
    m_isFixed = b;
}

} }
