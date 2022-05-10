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

#include "CoreUtils.h"
#include "Exception.h"
#include "Contract.h"
#include "Marshal.h"
#include "Object.h"
#include "String.h"

#include "limits.h"

// htonl & al.
#ifdef SKIZO_WIN
    #include <winsock2.h>
#endif
#ifdef SKIZO_X
    #include <arpa/inet.h>
#endif

namespace skizo { namespace core { namespace CoreUtils {
using namespace skizo::core::Marshal;

bool ValidateRange(int startIndex, int* rangeCount, int totalCount)
{
    int rc = *rangeCount;

    /* --- */
    if(rc == 0) {
        rc = totalCount - startIndex;
    }
    if(rc < 0
    || rc > totalCount
    || (rc + startIndex) > totalCount)
    {
        return false;
    }
    /* --- */

    *rangeCount = rc;
    return true;
}

bool AreObjectsEqual(const CObject* obj1, const CObject* obj2)
{
    if(!obj1 || !obj2) {
        return obj1 == obj2;
    } else {
        return obj1->Equals(obj2);
    }
}

so_char16* __int_ToWBuffer(int n, so_char16* buf, int* out_count)
{
    if(n == INT_MIN) {
        const char* const mincs = "-2147483648";
        const size_t minlen = strlen(mincs) + 1;

        for(size_t i = 0; i < minlen; i++) {
            buf[i] = mincs[i];
        }

        return buf;
    }

    so_char16* ptr = buf + (SKIZO_TOWBUFFER_BUFSZ - 1);

    const bool neg = (n < 0);
    if(neg) {
        n = -n;
    }

    *ptr-- = 0;

    do {
        *ptr-- = (n % 10) + '0';
        n /= 10;
    }
    while(n);

    if(neg) {
        *ptr-- = '-';
    }

    ++ptr;

    if(out_count) {
        *out_count = (SKIZO_TOWBUFFER_BUFSZ - 1) - (int)(ptr - buf);
    }

    return ptr;
}

const CString* BoolToString(bool b)
{
    return CString::FromUtf8(b? "true": "false");
}

const CString* IntToString(int i)
{
    so_char16 tmp[SKIZO_TOWBUFFER_BUFSZ];
    const so_char16* const r = __int_ToWBuffer(i, tmp, 0);
    return CString::FromUtf16(r);
}

const CString* LongToString(so_long l)
{
    char tmp[64];
#ifdef SKIZO_WIN
    // NOTE MinGW uses Microsoft's C runtime, which uses a bit different formatting.

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat"
    sprintf(tmp, "%I64d", l);
#pragma GCC diagnostic pop
#else
    SKIZO_REQ_EQUALS(sizeof(so_long), sizeof(long long));
    sprintf(tmp, "%lld", l);
#endif

    return CString::FromUtf8(tmp);
}

const CString* PtrToString(void* ptr)
{
    char tmp[32];
    sprintf(tmp, "%p", ptr);
    return CString::FromUtf8(tmp);
}

#define SKIZO_PRECISION_LIMIT 32
const CString* FloatToString(float f, int precision, bool noTrailingZeros)
{
    if(precision >= SKIZO_PRECISION_LIMIT) {
        precision = SKIZO_PRECISION_LIMIT;
    }

    if(precision < 0) {
        SKIZO_THROW(EC_ILLEGAL_ARGUMENT);
    }

    char formatStr[128];
    if(precision == 0) {
        strcpy(formatStr, "%f");
    } else {
        sprintf(formatStr, "A.%df", precision);
        formatStr[0] = '%';
    }

    char str[SKIZO_PRECISION_LIMIT]  = { 0 };
    sprintf(str, formatStr, f);

    if(noTrailingZeros && precision == 0) {
        // Now remove unnecessary zeros.
        const int length = strlen(str);
        for(int i = length - 1; i >= 0; i--) {
            char c = str[i];
            if(c == '0') {
                str[i] = 0;
            } else {
                // It's a point or something else.
                // We don't directly compare to "." here as different OS's might
                // have different formatting.
                if(c < '0' || c > '9') {
                    str[i] = 0;
                }
                break;
            }
        }
    }

    return CString::FromUtf8(str);
}

so_char16 CharToUpperCase(so_char16 c)
{
    Auto<const CString> tmp (CString::FromChar(c));
    tmp.SetPtr(tmp->ToUpperCase());
    SKIZO_REQ_POS(tmp->Length());
    return tmp->Chars()[0];
}

so_char16 CharToLowerCase(so_char16 c)
{
    Auto<const CString> tmp (CString::FromChar(c));
    tmp.SetPtr(tmp->ToLowerCase());
    SKIZO_REQ_POS(tmp->Length());
    return tmp->Chars()[0];
}

bool IsCharUpperCase(so_char16 c)
{
    return CharToUpperCase(c) == c;
}

bool IsCharLowerCase(so_char16 c)
{
    return CharToLowerCase(c) == c;
}

// TODO currently only works with spaces and tabs and '\r'
bool IsWhiteSpace(so_char16 c)
{
    return c == SKIZO_CHAR(' ') || c == SKIZO_CHAR('\t') || c == SKIZO_CHAR('\r');
}

bool IsDigit(so_char16 c)
{
    return c >= SKIZO_CHAR('0') && c <= SKIZO_CHAR('9');
}

bool IsLetter(so_char16 c)
{
    // TODO currently only supports the Latin alphabet
    return (c >= SKIZO_CHAR('a') && c <= SKIZO_CHAR('z'))
        || (c >= SKIZO_CHAR('A') && c <= SKIZO_CHAR('Z'));
}

bool IsControl(so_char16 c)
{
    // http://www.cplusplus.com/reference/cctype/iscntrl/
    // For the standard ASCII character set (used by the "C" locale), control characters are
    // those between ASCII codes 0x00 (NUL) and 0x1f (US), plus 0x7f (DEL).
    return (c <= 0x1f) || (c == 0x7f);
}

void ThrowHelper(int exCode, const char* msg, const char* file, int line)
{
#ifdef SKIZO_DEBUG_MODE
    // FIX cppcheck reports "possible null dereference" for msg argument
    fprintf(stderr, "EXCEPTION: '%s' (code=%d at %s:%d)\n",
                     msg? msg: "no specific message, see the error code",
                     exCode,
                     file,
                     line);
#endif

    throw SException((EExceptionCode)exCode, msg);
}

void ShowMessage(const char* msg, bool isFatal)
{
    Auto<const CString> daMsg (CString::FromUtf8(msg));
    ShowMessage(daMsg, isFatal);
}

// ***************

// TODO for linux?
void ValidatePath(const CString* path)
{
    if(!path) {
        SKIZO_THROW_WITH_MSG(EC_ILLEGAL_ARGUMENT, "Invalid path.");
    }

    const int length = path->Length();
    for (int i = 0; i < length; i++) {
        const so_char16 c = path->Chars()[i];

        switch(c) {
            case SKIZO_CHAR('"'):
            case SKIZO_CHAR('<'):
            case SKIZO_CHAR('>'):
            case SKIZO_CHAR('|'):
                {
                    SKIZO_THROW_WITH_MSG(EC_BAD_FORMAT, "Invalid path.");
                }
                break;

            case SKIZO_CHAR('\\'):
                {
                    SKIZO_THROW_WITH_MSG(EC_BAD_FORMAT, "Only normalized paths accepted (backward slash found).");
                }
                break;

            default:
                if(c < 32) {
                    SKIZO_THROW_WITH_MSG(EC_BAD_FORMAT, "Invalid path.");
                }
                break;
        }
    }
}

so_uint32 ByteOrderHostToNetwork(so_uint32 c)
{
    return htonl(c);
}

so_uint32 ByteOrderNetworkToHost(so_uint32 c)
{
    return ntohl(c);
}

void DumpMemory(const void* mem, size_t sz)
{
    auto p = (const unsigned char* const)mem;

    for (size_t i = 0; i < sz; ++i) {
        printf("%02x ", p[i]);
    }

    printf("\n");
}

void SegFault()
{
    *(volatile int*)0 = 0;
}

}

} }
