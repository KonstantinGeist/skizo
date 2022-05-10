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

#ifndef STREAM_H_INCLUDED
#define STREAM_H_INCLUDED

#include "Object.h"

#define E_VARIANTTYPEEX_NULL 6666
#define E_VARIANTTYPEEX_STRING (E_VARIANTTYPEEX_NULL + 1)

namespace skizo { namespace io {

enum EByteOrder
{
    E_BYTEORDER_HOST,
    E_BYTEORDER_NETWORK
};

/**
 * Base class for all streams.
 *
 * A stream can both Read and Write (depends on the access mode in CFileStream). CStream and its subclasses like
 * CFileStream only deal with byte-by-byte reading and writing. Automatically closes the stream on destruction.
 */
class CStream: public skizo::core::CObject
{
public:
    /**
     * Gets a value indicating whether the current stream supports reading.
     */
    virtual bool CanRead() const = 0;

    /**
     * Gets a value indicating whether the current stream supports writing.
     */
    virtual bool CanWrite() const = 0;

    /**
     * Gets a value indicating whether the current stream supports seeking.
     * Default implementation returns false.
     */
    virtual bool CanSeek() const;

    /**
     * Reads data from the stream.
     *
     * @remarks @see Read(char*, so_long)
     * @param buf a buffer to read data into
     * @param count number of bytes to read, also the size of the buffer
     * @return the total number of bytes read into the buffer. This can be less
     * than the number of bytes requested if that many bytes are not currently
     * available, or zero (0) if the end of the stream has been reached.
     */
    virtual so_long Read(char* buf, so_long count) = 0;

    /**
     * Same as Read(char*, so_long), with a new parameter added.
     *
     * @param allowPartial explicitly specifies whether the read operation is
     * allowed to return partial data. See remarks.
     * @note Default implementation simply calls Read(char*, so_long).
     * @remarks The contract of Read(char*, so_long) says that the function
     * should return as many bytes as specified by parameter 'count', unless
     * there are no more bytes in the stream, or an error occured. Some types of
     * streams, however, for example, CSocketStream (which depends on the design
     * of TCP/IP), by design can return less bytes than specified, even when no
     * error occured. If allowPartial is set to true, the function returns
     * partial data as it is. If allowPartial is set to false, this function
     * should internally combine multiple partial reads into what is perceived
     * as one read by the user. However, parameter 'allowPartial' can be ignored
     * by implementations where such difference in behavior isn't important.
     */
    virtual so_long Read(char* buf, so_long count, bool allowPartial);

    /**
     * Writes data to the stream.
     *
     * @param buf a buffer to write data from
     * @param count number of bytes to write, also the size of the buffer
     * @return the total number of bytes written into the buffer. This can be
     * less than the number of bytes requested if that many bytes are not
     * currently available, or zero (0) if the end of the stream has been reached.
     */
    virtual so_long Write(const char* buf, so_long count) = 0;

    /**
     * Same as Read(char*, so_long, bool)
     */
    virtual so_long Write(const char* buf, so_long count, bool allowPartial);

    /**
     * Sets the current position of the stream.
     *
     * @param pos the new position
     * @throw EC_INVALID_STATE if the new position is greater than the size of
     * the stream. Optional. Default implementation throws EC_NOT_IMPLEMENTED
     */
    virtual void SetPosition(so_long pos);

    /**
     * Gets the current position of the stream.
     * Default implementation throws EC_NOT_IMPLEMENTED
     */
    virtual so_long GetPosition() const;

    /**
     * Total size of the stream. May throw if the stream can't seek or under other
     * circumstances (the stream doesn't support it).
     * Default implementation throws EC_NOT_IMPLEMENTED
     */
    virtual so_long Size() const;

    /**
     * Reads this stream into another stream.
     */
    void ReadTo(CStream* stream, so_long sz);

    /**
     * Flushes internal buffers, if any. The default implementation is a no-op.
     */
    virtual void Flush();
};

} }

#endif // STREAM_H_INCLUDED
