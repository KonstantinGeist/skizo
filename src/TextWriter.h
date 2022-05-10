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

#ifndef STREAMWRITER_H_INCLUDED
#define STREAMWRITER_H_INCLUDED

#include "Object.h"

namespace skizo { namespace io {
class CStream;

/**
 * A buffered wrapper around any kind of CStream that can write a sequential
 * series of characters.
 *
 * @warning Currently supports only UTF8.
 */
class CTextWriter: public skizo::core::CObject
{
public:
    /**
     * Initializes a new instance of CTextWriter.
     *
     * @warning The wrapped stream should support writing to it.
     * @param wrapped the stream to wrap.
     * @throws EC_ILLEGAL_ARGUMENT if the wrapped stream doesn't support writing.
     */
    explicit CTextWriter(CStream* wrapped);

    virtual ~CTextWriter();

    /**
     * Forces any buffered output bytes to be written out.
     */
    void Flush();

    /**
     * Writes a string.
     *
     * @param str string to be written
     */
    void Write(const skizo::core::CString* str);

    /**
     * Writes a character.
     *
     * @param c character to be written
     */
    void Write(so_char16 c);

    /**
     * Writes a C buffer.
     *
     * @note Simply flushes the internal buffer and redirects the call to the
     * underlying stream.
     * @param cs the buffer to write
     * @param sz the size of the buffer
     */
    void Write(const char* cs, so_long sz);

    /**
     * Appends a new line.
     *
     * @note The appended value is platform-dependent.
     */
    void WriteLine();

private:
    CStream* m_wrapped;
};

} }

#endif // STREAMWRITER_H_INCLUDED
