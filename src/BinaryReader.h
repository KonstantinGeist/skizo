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

#ifndef BINARYREADER_H_INCLUDED
#define BINARYREADER_H_INCLUDED

#include "Stream.h"
#include "Variant.h"

namespace skizo { namespace io {

/**
 * A wrapper around a CStream object that reads primitive data types as binary values.
 */
class SBinaryReader
{
public:
    /**
     * Initializes a binary reader with a stream object.
     */
    SBinaryReader(CStream* stream, EByteOrder byteOrder = E_BYTEORDER_HOST);

    /**
     * Returns the underlying stream.
     */
    CStream* Stream() const;

    /**
     * Reads a 1-byte unsigned integer from the current stream and advances the
     * current position of the stream by 1 byte.
     */
    so_byte ReadByte();

    /**
     * Reads a 2-byte char from the current stream and advances the current
     * position of the stream by 2 bytes.
     */
    so_char16 ReadChar();

    /**
     * Reads a 4-byte signed integer from the current stream and advances the
     * current position of the stream by 4 bytes.
     */
    int ReadInt();

    /**
     * Reads a 8-byte signed integer from the current stream and advances the
     * current position of
     * the stream by 8 bytes.
     */
    so_long ReadLong();

    so_uint16 ReadUInt16();

    so_uint32 ReadUInt32();

    /**
     * Reads a 4-byte floating point value from the current stream and advances
     * the current position of the stream by 4 bytes.
     */
    float ReadFloat();

    /**
     * Calls ReadInt and converts the result to a boolean.
     */
    bool ReadBool();

    /**
     * Reads a short UTF8 string prepended with a 8-bit string length header
     * which can't be more than 255 characters long. The string in the stream
     * is not null-terminated; it is, however, null-terminated in the supplied
     * output buffer.
     */
    void ReadUtf8(char out_buf[256]);

    /**
     * Reads a length-prepended UTF16 string.
     */
    const skizo::core::CString* ReadUTF16();

    /**
     * Reads a length-prepended UTF8 string. Delete the array with CString::FreeUtf8(..)
     */
    char* ReadUTF8();

    /**
     * Reads a tagged variant. For E_VARIANTTYPE_OBJECT, supports only strings.
     */
    skizo::core::SVariant ReadVariant();

private:
    CStream* m_stream;
    EByteOrder m_byteOrder;
};

} }

#endif // BINARYREADER_H_INCLUDED
