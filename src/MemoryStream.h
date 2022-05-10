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

#ifndef MEMORYSTREAM_H_INCLUDED
#define MEMORYSTREAM_H_INCLUDED

#include "Stream.h"
#include "ByteBuffer.h"

namespace skizo { namespace io {

/**
 * An implementation of CStream which reads/writes from/to memory.
 */
class CMemoryStream: public CStream
{
public:
    explicit CMemoryStream(CByteBuffer* bb = nullptr);

    virtual bool CanRead() const override;
    virtual bool CanWrite() const override;
    virtual bool CanSeek() const override;
    virtual so_long Read(char* buf, so_long count) override;
    virtual so_long Write(const char* buf, so_long count) override;
    virtual void SetPosition(so_long pos) override;
    virtual so_long GetPosition() const override;
    virtual so_long Size() const override;

    /**
     * Pointer to the memory block at the current position.
     */
    so_byte* CurrentBytes() const;

    /**
     * Clears the underlying byte buffer.
     */
    void Clear();

private:
    skizo::core::Auto<CByteBuffer> m_bb;
    so_long m_position;

    void ensureBuffer(so_long end);
};

} }

#endif // MEMORYSTREAM_H_INCLUDED
