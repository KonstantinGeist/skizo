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

#ifndef FASTBYTEBUFFER_H_INCLUDED
#define FASTBYTEBUFFER_H_INCLUDED

#include "Contract.h"
#include "Exception.h"

namespace skizo { namespace script {

#define SKIZO_FASTBYTEBUFFER_GROW_FACTOR 0.75f
#define SKIZO_FASTBYTEBUFFER_CLEAR_THRESHOLD (1024*8)

class SFastByteBuffer {
public:
    explicit SFastByteBuffer(size_t cap)
    {
        SKIZO_REQ(cap, skizo::core::EC_ILLEGAL_ARGUMENT);

        m_size = 0;
        m_cap = cap;
        m_initCap = m_cap;
        m_bytes = (so_byte*)malloc(m_cap);
        if(!m_bytes) {
            SKIZO_THROW(skizo::core::EC_OUT_OF_RESOURCES);
        }
    }

    ~SFastByteBuffer()
    {
        free(m_bytes);
    }

    /**
     * @note Bytes can be null.
     */
    inline void AppendBytes(const so_byte* bytes, size_t count)
    {
        appendBytesGeneric(bytes, count);
    }

    inline void AppendByte(so_byte b)
    {
        const so_byte bs[1] = { b };
        AppendBytes(bs, 1);
    }

    inline void Clear()
    {
        if(m_size >= SKIZO_FASTBYTEBUFFER_CLEAR_THRESHOLD) {
            free(m_bytes);
            m_bytes = (so_byte*)malloc(m_initCap);
        }

        m_size = 0;
    }

    inline size_t Size() const
    {
        return m_size;
    }

    inline so_byte* Bytes() const
    {
        return m_bytes;
    }

private:
    so_byte* m_bytes;
    size_t m_size;
    size_t m_cap;
    size_t m_initCap; // initial capacity (restored to it when trimmed down)

    inline void appendBytesGeneric(const so_byte* bytes, size_t count)
    {
        if(((float)(m_size + count) / (float)m_cap) >= SKIZO_FASTBYTEBUFFER_GROW_FACTOR) {
            m_cap = m_cap * 2 + count;
            m_bytes = (so_byte*)realloc(m_bytes, m_cap);
            if(!m_bytes) {
                SKIZO_THROW(skizo::core::EC_OUT_OF_RESOURCES);
            }
        }

        if(bytes) {
            memcpy(m_bytes + m_size, bytes, count);
        }

        m_size += count;
    }
};

} }

#endif // FASTBYTEBUFFER_H_INCLUDED
