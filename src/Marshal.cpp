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

#include "Marshal.h"

namespace skizo { namespace core { namespace Marshal {

/*
 * Base64 encoding/decoding (RFC1341)
 * Copyright (c) 2005-2011, Jouni Malinen <j@w1.fi>
 *
 * This software may be distributed under the terms of the BSD license.
 */

static const so_byte base64_table[65] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

so_byte* EncodeBase64(const so_byte* src, size_t len, size_t* out_len)
{
    so_byte *out, *pos;
    const so_byte *end, *in;
    size_t olen;
    int line_len;

    olen = len * 4 / 3 + 4; // 3-byte blocks to 4-byte
    olen += olen / 72;      // line feeds
    olen++;                 // nul termination
    if(olen < len) {
        return nullptr;     // integer overflow
    }
    out = new so_byte[olen];

    end = src + len;
    in = src;
    pos = out;
    line_len = 0;
    while(end - in >= 3) {
        *pos++ = base64_table[in[0] >> 2];
        *pos++ = base64_table[((in[0] & 0x03) << 4) | (in[1] >> 4)];
        *pos++ = base64_table[((in[1] & 0x0f) << 2) | (in[2] >> 6)];
        *pos++ = base64_table[in[2] & 0x3f];
        in += 3;
        line_len += 4;
        if (line_len >= 72) {
            *pos++ = '\n';
            line_len = 0;
        }
    }

    if(end - in) {
        *pos++ = base64_table[in[0] >> 2];
        if(end - in == 1) {
            *pos++ = base64_table[(in[0] & 0x03) << 4];
            *pos++ = '=';
        } else {
            *pos++ = base64_table[((in[0] & 0x03) << 4) |
                          (in[1] >> 4)];
            *pos++ = base64_table[(in[1] & 0x0f) << 2];
        }
        *pos++ = '=';
        line_len += 4;
    }

    if(line_len) {
        *pos++ = '\n';
    }

    *pos = '\0';
    if(out_len) {
        *out_len = pos - out;
    }
    return out;
}

so_byte* DecodeBase64(const so_byte* src, size_t len, size_t* out_len)
{
    so_byte dtable[256], *out, *pos, block[4], tmp;
    size_t i, count, olen;
    int pad = 0;

    memset(dtable, 0x80, 256);
    for(i = 0; i < sizeof(base64_table) - 1; i++) {
        dtable[base64_table[i]] = (so_byte) i;
    }
    dtable[int('=')] = 0;

    count = 0;
    for(i = 0; i < len; i++) {
        if(dtable[src[i]] != 0x80) {
            count++;
        }
    }

    if(count == 0 || count % 4) {
        return nullptr;
    }

    olen = count / 4 * 3;
    pos = out = new so_byte[olen];

    count = 0;
    for(i = 0; i < len; i++) {
        tmp = dtable[src[i]];
        if (tmp == 0x80) {
            continue;
        }

        if(src[i] == '=') {
            pad++;
        }
        block[count] = tmp;
        count++;
        if(count == 4) {
            *pos++ = (block[0] << 2) | (block[1] >> 4);
            *pos++ = (block[1] << 4) | (block[2] >> 2);
            *pos++ = (block[2] << 6) | block[3];
            count = 0;
            if (pad) {
                if(pad == 1) {
                    pos--;
                } else if(pad == 2) {
                    pos -= 2;
                } else {
                    // Invalid padding.
                    delete [] out;
                    return nullptr;
                }
                break;
            }
        }
    }

    *out_len = pos - out;
    return out;
}

} } }
