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

#ifndef MARSHAL_H_INCLUDED
#define MARSHAL_H_INCLUDED

#include "basedefs.h"

namespace skizo { namespace core {

/*
*    For Linux: versions of wcslen/wcscpy/wmemcpy that are guaranteed to work
*    on 16-bit chars (the standard ones work on 32-bit chars).
*    For Windows: wchar_t's are always 16 bits, so we just use wrappers around
*    standard functions here.
*/
#ifdef SKIZO_X
    int so_wcslen_16bit(const so_char16* str);
    so_char16* so_wcscpy_16bit(so_char16* dst, const so_char16* src);
    so_char16* so_wmemcpy_16bit(so_char16* __restrict__ s1, const so_char16* __restrict__ s2, int n);
    int so_wcscmp_16bit(const so_char16* s1, const so_char16* s2);
#endif
#ifdef SKIZO_WIN
    #include <wchar.h>

    /**
     * Retrieves the length of a 16-bit string.
     */
    #define so_wcslen_16bit(str) (int)wcslen((const wchar_t*)str)

    /**
     * Copies a 16-bit string from src to dst.
     */
    #define so_wcscpy_16bit(dst, src) wcscpy((wchar_t*)dst, (wchar_t*)src)

    /**
     * Copies a 16-bit memory block.
     */
    #define so_wmemcpy_16bit(s1, s2, n)wmemcpy((wchar_t*)s1, (wchar_t*)s2, (size_t)n)

    /**
     * Compares the 16-bit blocks s1 and s2 and returns an integer which describes their
     * relationship.
     */
    #define so_wcscmp_16bit(s1, s2) wcscmp((wchar_t*)s1, (wchar_t*)s2)
#endif

namespace Marshal {
    /**
     * Encodes Base64.
     *
     * @param src data to be encoded
     * @param len length of the data to be encoded
     * @param out_len Pointer to output length variable, or null if not used
     *
     * @returns allocated buffer of out_len bytes of encoded data, or null on failure
     *
     * Caller is responsible for freeing the returned buffer (delete []).
     * Returned buffer is null terminated to make it easier to use as a C string.
     * The null terminator is not included in out_len.
     */
    so_byte* EncodeBase64(const so_byte* src, size_t len, size_t* out_len);

    /**
     * Decodes Base64.
     *
     * @param src data to be decoded
     * @param len length of the data to be decoded
     * @param out_len pointer to output length variable
     *
     * @return allocated buffer of out_len bytes of decoded data, or null on failure
     *
     * Caller is responsible for freeing the returned buffer (delete [])
     */
    so_byte* DecodeBase64(const so_byte* src, size_t len, size_t* out_len);

} } }

#endif // MARSHAL_H_INCLUDED
