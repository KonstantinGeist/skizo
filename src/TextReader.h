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

#ifndef STREAMREADER_H_INCLUDED
#define STREAMREADER_H_INCLUDED

#include "Object.h"

namespace skizo { namespace core {
    class CString;
} }

namespace skizo { namespace io {
class CByteBuffer;
class CStream;

#define SKIZO_STREAMREADER_BUFSIZE 1024

/**
 * A buffered wrapper around any kind of CStream that can read a sequential
 * series of characters (strings).
 *
 * @warning Currently supports only UTF8.
 */
class CTextReader: public skizo::core::CObject
{
public:
    /**
     * Initializes a new instance of CTextReader.
     *
     * @warning The wrapped stream should support reading from it.
     * @param wrapped the stream to wrap.
     * @throws EC_ILLEGAL_ARGUMENT if the wrapped stream doesn't support reading.
     */
    explicit CTextReader(CStream* wrapped);

    virtual ~CTextReader();

    /**
     * Reads a line of characters from the current stream and returns the data
     * as a string. A line is defined as a sequence of characters followed by
     * \\n or \\r\\n (platform-dependent).
     */
    const skizo::core::CString* ReadLine();

private:
    CStream* m_wrapped;
    int m_index;
    int m_lineNumber; // A hack to remove BOM's.
    so_byte m_buffer[SKIZO_STREAMREADER_BUFSIZE];

    CByteBuffer* m_bb;
};

} }

#endif // STREAMREADER_H_INCLUDED
