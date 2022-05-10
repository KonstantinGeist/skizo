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

#ifndef BYTEBUFFER_H_INCLUDED
#define BYTEBUFFER_H_INCLUDED

#include "Object.h"

namespace skizo { namespace io {

/** Default byte buffer capacity. */
#define SKIZO_DEF_BYTEBUFFER_CAP 512

/** Default grow factor. */
#define SKIZO_BYTEBUFFER_GROW_FACTOR 0.75f

/**
 * An expandable or fixed byte buffer.
 */
class CByteBuffer: public skizo::core::CObject
{
public:
    /**
     * Constructor.
     *
     * @param cap The initial capacity. Optional.
     */
    explicit CByteBuffer(so_long cap = 0);

    virtual ~CByteBuffer();

    /**
     * Appends bytes to the byte buffer.
     *
     * @note If bytes is NULL, increases the buffer anyway, but the content of
     * the newly appended portion is undefined.
     * @param bytes Array to append.
     * @param count Count of bytes.
     * @throws EC_INVALID_STATE if the buffer is fixed
     */
    void AppendBytes(const so_byte* bytes, so_long count);

    /**
     * Appends one byte to the byte buffer.
     *
     * @param byte the byte to write to the buffer
     * @throws EC_INVALID_STATE if the buffer is fixed
     */
    void AppendByte(so_byte byte);

    /**
     * Clears the buffer by setting its size to 0.
     *
     * @throws EC_INVALID_STATE if the buffer is fixed
     */
    void Clear();

    /**
     * Gets the size of the buffer.
     */
    inline so_long Size() const
    {
        return m_size;
    }

    /**
     * Direct access to the data.
     */
    inline so_byte* Bytes() const
    {
        return m_bytes;
    }

    /**
     * Makes the buffer fixed.
     */
    void SetFixed(bool b);

private:
    so_long m_size;
    so_long m_cap;
    so_long m_initCap; // Initial capacity (restored to it when trimmed down).
    so_byte* m_bytes;
    bool m_isFixed;

    void appendBytesGeneric(const so_byte* bytes, so_long count);
};

} }

#endif // BYTEBUFFER_H_INCLUDED
