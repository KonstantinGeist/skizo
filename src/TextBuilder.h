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

#ifndef TEXTBUILDER_H_INCLUDED
#define TEXTBUILDER_H_INCLUDED

#include "FastByteBuffer.h"
#include "StringSlice.h"
#include "TypeRef.h"

namespace skizo { namespace script {

#define SKIZO_OUTPUTBUFFER_INITSIZE 4096

/**
 * A fancy string builder that also accepts metadata as parameters.
 *
 * Supported formats:
 *
 *  %d - int
 *  %f - float
 *  %p - pointer
 *  %s - StringSlice
 *  %t - TypeRef (for C code emission)
 *  %S - char*
 *  %o - CObject* (calls CObject::ToString())
 *  %T - TypeRef (prints nice names)
 *  %C - CClass* (prints nice names)
 */
struct STextBuilder
{
public:
    explicit STextBuilder(int initialSize = SKIZO_OUTPUTBUFFER_INITSIZE);
    ~STextBuilder();

    void Prepend(const char* format, ...);
    void Append(STextBuilder& other);
    void Emit(const char* format, ...);

    /**
     * @note Code isn't appendable after this call, because this call adds null termination
     * to the buffer to make it C-compatible. Call Clear() to clear the buffer.
     */
    char* Chars();

    void Clear();

    /**
     * Clears whatever was before, formats the string using this builder instance
     * as a temporary buffer and returns a newly allocated string which is to be
     * released with CString::FreeUtf8(..) (as SoDomainAbortException expects it).
     *
     * To be used by Domain::formatMessage(..)
     */
    char* ClearFormat(const char* format, va_list vl);

    const skizo::core::CString* ToString();

private:
    SFastByteBuffer m_buffer;
    SFastByteBuffer m_tmpBuffer;

    // A conversion buffer to convert from UTF16 Unicode to mangled ASCII.
    char* m_convBuffer;

    void append(const char* buffer);
    void append(const char* buffer, size_t sz);
    void emitImpl(const char* format, va_list vl);
    void emitTypeRef(const STypeRef* typeRef);
    void emitStringSlice(const SStringSlice* ss);
};

} }

#endif // TEXTBUILDER_H_INCLUDED
