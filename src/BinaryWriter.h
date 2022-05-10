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

#ifndef BINARYWRITER_H_INCLUDED
#define BINARYWRITER_H_INCLUDED

#include "Stream.h"
#include "Variant.h"

namespace skizo { namespace core {
    class CString;
} }

namespace skizo { namespace io {

/**
 * A wrapper around a CStream object that writes primitive data types as binary
 * values.
 */
class SBinaryWriter
{
public:
    /**
     * Initializes a binary writer with a stream object.
     */
    SBinaryWriter(CStream* stream, EByteOrder byteOrder = E_BYTEORDER_HOST);

    /**
     * Returns the underlying stream.
     */
    CStream* Stream() const;

    /**
     * Writes a 4-byte signed integer from the current stream and advances the
     * current position of the stream by 4 bytes.
     */
    void WriteInt(int i);

    /**
     * Writes a 8-byte signed integer from the current stream and advances the
     * current position of the stream by 8 bytes.
     */
    void WriteLong(so_long l);

    /**
     * Writes a 4-byte floating point value from the current stream and advances
     * the current position of the stream by 4 bytes.
     */
    void WriteFloat(float f);

    /**
     * Converts b to an integer and calls WriteInt(..)
     */
    void WriteBool(bool b);

    /**
     * Writes a UTF16 buffer to the stream.
     *
     * @param str the string to write to the stream.
     * @param lengthHeader if true, appends a 32-bit integer header that specifies
     * the length of the string in advance. Optional.
     *
     * @note `str` can be null if lengthHeader==true. An empty string is implied.
     */
    void WriteUTF16(const skizo::core::CString* str, bool lengthHeader = false);

    /**
     * Writes a UTF8 buffer to the stream.
     *
     * @param str the string to write to the stream.
     * @param lengthHeader if true, appends a 32-bit integer header that specifies
     *        the length of the string in advance. Optional.
     */
    void WriteUTF8(const char* str, so_long size, bool lengthHeader = false);

    /**
     * Writes a short UTF8 string prepended with a 8-bit string length header
     * which can't be more than 255 characters long. The string in the stream
     * is not null-terminated; it is, however, null-terminated in the supplied
     * input buffer.
     */
    void WriteUtf8(const char str[256]);

    /**
     * Writes a 4-byte unsigned integer from the current stream and advances the
     * current position of the stream by 4 bytes.
     */
    void WriteUInt32(so_uint32 i);

    /**
     * Writes a 2-byte unsigned integer from the current stream and advances the
     * current position of the stream by 2 bytes.
     */
    void WriteUInt16(so_uint16 i);

    /**
     * Writes a 1-byte unsigned integer from the current stream and advances the
     * current position of the stream by 1 bytes.
     */
    void WriteByte(so_byte b);

    /**
     * Writes a 2-byte character from the current stream and advances the current
     * position of the stream by 2 bytes.
     */
    void WriteChar(so_char16 c);

    /**
     * Writes a tagged variant.
     * For E_VARIANTTYPE_OBJECT, supports only strings.
     */
    void WriteVariant(const skizo::core::SVariant& value);

    /**
     * Calls the Flush method of the underlying stream.
     */
    void Flush();

private:
    CStream* m_stream;
    EByteOrder m_byteOrder;
};

} }

#endif // BINARYWRITER_H_INCLUDED
