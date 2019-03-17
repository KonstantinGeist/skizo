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

#include "String.h"
#include "ArrayList.h"
#include "Console.h"
#include "Contract.h"
#include "CoreUtils.h"
#include "Exception.h"
#include "Marshal.h"
#include "StringBuilder.h"
#include <stdarg.h>

namespace skizo { namespace core {
using namespace skizo::core::Marshal;
using namespace skizo::collections;

void* CString::operator new (size_t size, int charCount)
{
    SKIZO_REQ_POS(size)

    /* 1 char for null-termination is already prepended as m_chars. */
    void* r = malloc(size + charCount * sizeof(so_char16));
    if(!r) {
        throw std::bad_alloc();
    }
    return r;
}

void CString::operator delete (void *p)
{
    SKIZO_REQ_PTR(p)

    free(p);
}

const CString* CString::createBuffer(int charCount)
{
    const CString* r = new (charCount) CString;
    r->m_length = charCount;
    (&r->m_chars)[charCount] = 0;
    return r;
}
#define createEmptyString() CString::createBuffer(0)

const CString* CString::FromChar(so_char16 c)
{
    const CString* r = createBuffer(1);
    (&r->m_chars)[0] = c;
    return r;
}

int CString::Length() const
{
    return m_length;
}

const so_char16* CString::Chars() const
{
    return &m_chars;
}

int CString::CharAt(int index) const
{
    SKIZO_REQ_RANGE_D(index, 0, m_length)

    return (&m_chars)[index];
}

bool CString::IsNullOrEmpty(const CString* str)
{
    return str == 0 || str->m_length == 0;
}

const CString* CString::FromUtf16(const so_char16* chars)
{
    SKIZO_REQ_PTR(chars)

    const int wcharCount = so_wcslen_16bit(chars);
    const CString* r = createBuffer(wcharCount);
    so_wcscpy_16bit(&r->m_chars, chars);
    return r;
}

const CString* CString::FromASCII(const char* chars)
{
    SKIZO_REQ_PTR(chars)

    const int charCount = strlen(chars);
    const CString* r = createBuffer(charCount);

    for(int i = 0; i < charCount; i++) {
        (&r->m_chars)[i] = (so_char16)chars[i];
    }

    return r;
}

void CString::FreeCLibString(char* chars)
{
    FreeUtf8(chars);
}

char* CString::CloneUtf8(const char* chars)
{
    char* clone = new char[strlen(chars) + 1];
    strcpy(clone, chars);
    return clone;
}

const CString* CString::Concat(const CString* str2, const CString* str3) const
{
    int totalLen = m_length + str2->m_length;
    if(str3) {
        totalLen += str3->m_length;
    }

    const CString* buffer = createBuffer(totalLen);

    so_wcscpy_16bit(&buffer->m_chars, &m_chars);
    so_wcscpy_16bit(&buffer->m_chars + m_length, &str2->m_chars);

    if(str3) {
        so_wcscpy_16bit(&buffer->m_chars + m_length + str2->m_length, &str3->m_chars);
    }

    // Post-condition.
    SKIZO_REQ_EQUALS(buffer->Length(),
                     this->Length() + str2->Length() + (str3? str3->Length(): 0))

    return buffer;
}

const CString* CString::Concat(const char* str2, const char* str3) const
{
    const CString* s_str2 = FromUtf8(str2);
    const CString* s_str3 = str3? FromUtf8(str3): nullptr;
    const CString* r = Concat(s_str2, s_str3);

    s_str2->Unref();
    if(s_str3) {
        s_str3->Unref();
    }
    return r;
}

const CString* CString::Substring(int start, int count) const
{
    if(!CoreUtils::ValidateRange(start, &count, m_length)) {
        SKIZO_THROW(EC_OUT_OF_RANGE);
    }

    if(start == 0 && count == m_length) {
        this->Ref();
        // It's OK, strings are guaranteed to be immutable.
        return const_cast<const CString*>(this);
    }

    const CString* buffer = createBuffer(count);
    so_wmemcpy_16bit(&buffer->m_chars, &m_chars + start, count);

    return buffer;
}

const CString* CString::Remove(so_char16 c) const
{
    Auto<CStringBuilder> sb (new CStringBuilder());

    for(int i = 0; i < this->Length(); i++) {
        const so_char16 c2 = this->Chars()[i];

        if(c2 != c) {
            sb->Append(c2);
        }
    }

    return sb->ToString();
}

const CString* CString::Replace(so_char16 oldChar, so_char16 newChar) const
{
    // Shortcut.
    const int charIndex = this->FindChar(oldChar);
    if(charIndex == -1) {
        this->Ref();
        return this;
    }

    Auto<CStringBuilder> sb (new CStringBuilder());

    for(int i = 0; i < this->Length(); i++) {
        const so_char16 c = this->Chars()[i];

        if(c == oldChar) {
            sb->Append(newChar);
        } else {
            sb->Append(c);
        }
    }

    return sb->ToString();
}

int CString::GetHashCode() const
{
    int h = 0;
    for(int i = 0; i < m_length; i++) {
        /* "((h << 5) - h)" is optimization for 31 * i. */
        h = ((h << 5) - h) + (&m_chars)[i];
    }

    return h;
}

bool CString::StartsWith(const CString* str) const
{
    if(str->m_length == 0 || m_length < str->m_length) {
        return false;
    }

    int i;
    for(i = 0; i < str->m_length; i++) {
        if((&str->m_chars)[i] != (&m_chars)[i]) {
            return false;
        }
    }

    return true;
}

bool CString::StartsWith(const char* cs) const
{
    const CString* str = CString::FromUtf8(cs);
    bool r = StartsWith(str);
    str->Unref();
    return r;
}

bool CString::StartsWithASCII(const char* str) const
{
    const int len = strlen(str);
    if(len > m_length) {
        return false;
    }

    for(int i = 0; i < len; i++) {
        if((&m_chars)[i] != (so_char16)str[i]) {
            return false;
        }
    }

    return true;
}

bool CString::EndsWithASCII(const char* str) const
{
    const int len = strlen(str);
    if(len == 0 || this->Length() < len) {
        return false;
    }

    const so_char16* const selfp = this->Chars() + (this->Length() - len);
    for(int i = 0; i < len; i++) {
        if((so_char16)str[i] != selfp[i]) {
            return false;
        }
    }

    return true;
}

bool CString::EndsWith(const CString* str) const
{
    if(str->Length() == 0 || this->Length() < str->Length()) {
        return false;
    }

    const so_char16* const selfp = this->Chars() + (this->Length() - str->Length());
    for(int i = 0; i < str->Length(); i++) {
        if((str->Chars()[i]) != selfp[i]) {
            return false;
        }
    }

    return true;
}

bool CString::EndsWith(const char* cs) const
{
    const CString* str = CString::FromUtf8(cs);
    bool r = EndsWith(str);
    str->Unref();
    return r;
}

const CString* CString::PadRight(int count) const
{
    SKIZO_REQ_POS(count)

    const CString* r = createBuffer(m_length + count);
    so_wcscpy_16bit(&r->m_chars, &m_chars);
    for(int i = 0; i < count; i++) {
        (&r->m_chars)[m_length + i] = SKIZO_CHAR(' ');
    }

    return r;
}

int CString::FindChar(so_char16 c, int start, int count) const
{
    if(!CoreUtils::ValidateRange(start, &count, m_length)) {
        SKIZO_THROW(EC_OUT_OF_RANGE);
    }

    for(int i = start; i < start + count; i++) {
        if((&m_chars)[i] == c) {
            return i;
        }
    }

    // Not found.
    return -1;
}

int CString::FindLastChar(so_char16 c, int start, int count) const
{
    if(!CoreUtils::ValidateRange(start, &count, m_length)) {
        SKIZO_THROW(EC_OUT_OF_RANGE);
    }

    for(int i = start + count; i >= start; --i) {
        if((&m_chars)[i] == c) {
            return i;
        }
    }

    // Not found.
    return -1;
}

int CString::FindSubstring(const CString* substring) const
{
    const int targetOffset = 0,
              sourceOffset = 0;
    const int targetCount = substring->Length(),
              sourceCount = this->Length();

    // *************
    // Verification.
    // *************
    if(sourceCount == 0) {
        return (targetCount == 0 ? sourceCount : -1);
    }

    if(targetCount == 0) {
        return 0;
    }
    // *************

    const int max = sourceOffset + (sourceCount - targetCount);
    const so_char16* const source = &m_chars;
    so_char16* target = &substring->m_chars;
    so_char16 first = target[0];

    for(int i = sourceOffset; i <= max; i++) {
        /* Look for first character. */
        if(source[i] != first) {
            while (++i <= max && source[i] != first);
        }

        /* Found first character, now look at the rest of v2 */
        if(i <= max) {
            int j = i + 1;
            const int end = j + targetCount - 1;
            for (int k = targetOffset + 1; j < end && source[j] == target[k]; j++, k++);
            if(j == end) {
                /* Found whole string. */
                return i - sourceOffset;
            }
        }
    }

    return -1;
}

int CString::FindSubstring(const CString* substring, int sourceStart) const
{
    SKIZO_REQ_RANGE_D(sourceStart, 0, this->Length())

    if(substring->Length() == 0) {
        return 0;
    }

    const int sourceLength = this->Length();
    const int substringLength = substring->Length();
    const so_char16* const sourceChars = this->Chars();
    const so_char16* const substringChars = substring->Chars();

    for(int i = sourceStart; i < sourceLength; i++) {
        for(int j = 0; j < substringLength; j++) {
            if(i + j >= sourceLength) {
                goto out_of_this;
            }
            if(sourceChars[i + j] != substringChars[j]) {
                goto out_of_this;
            }
        }
        return i;
        out_of_this:
        { }
    }

    return -1;
}

int CString::FindSubstringASCII(const char* substring, int sourceStart) const
{
    SKIZO_REQ_RANGE_D(sourceStart, 0, this->Length())

    if(!substring || !substring[0]) {
        return 0;
    }

    const int sourceLength = this->Length();
    const int substringLength = strlen(substring);
    const so_char16* const sourceChars = this->Chars();

    for(int i = sourceStart; i < sourceLength; i++) {
        for(int j = 0; j < substringLength; j++) {
            if(i + j >= sourceLength) {
                goto out_of_this;
            }
            if(sourceChars[i + j] != (so_char16)substring[j]) {
                goto out_of_this;
            }
        }
        return i;
        out_of_this:
        { }
    }

    return -1;
}

const CString* CString::ToString() const
{
    this->Ref();
    // This is OK, strings are guaranteed to be immutable.
    return const_cast<const CString*>(this);
}

static CStringFormatElement* createElement(const CString* s, int startOffset, int length, const char* type)
{
    Auto<CStringFormatElement> e (new CStringFormatElement());
    e->StartOffset = startOffset - 1; // Rewinds back because the arguments' startOffset starts after '%'.
    e->Length = length + 2; // % + 1 letter typecode. The arguments' length is only what is between them.

    if(length == 0) {
        e->Width = 0;
        e->Precision = 0;
    } else {
        // Currently, if there are flags for an int type, it should start with zero
        // to comply with the standards of C (width is always padded with zeros in skizo currently).
        if(strcmp(type, "d") == 0 || strcmp(type, "l") == 0) {
            if(s->Chars()[startOffset] != SKIZO_CHAR('0')) {
                return nullptr;
            }
            // Now let's extract the number.
            // TryParse interprets 0 as "don't care", not as an actual length; guard against it
            // in advance.
            if(length - 1 == 0) {
                return nullptr;
            }
            if(!s->TryParseInt(&e->Width, startOffset + 1, length - 1)) {
                return nullptr;
            }
        } else if(strcmp(type, "f") == 0) {
            // A float specification consists of two parts: width and precision.
            // Both parts can be zero (which means "don't care).

            // Searches for ".".
            int dotIndex = -1;
            for(int i = 0; i < length; i++) {
                if(s->Chars()[startOffset + i] == SKIZO_CHAR('.')) {
                    dotIndex = startOffset + i;
                    break;
                }
            }

            if(dotIndex == -1) {
                // Couldn't find the obligatory '.'
                return nullptr;
            }

            if(dotIndex - startOffset == 0 || startOffset + length - dotIndex - 1 == 0) {
                return nullptr;
            }
            if(!s->TryParseInt(&e->Width, startOffset, dotIndex - startOffset)) {
                return nullptr;
            }
            if(!s->TryParseInt(&e->Precision, dotIndex + 1, startOffset + length - dotIndex - 1)) {
                return nullptr;
            }
        } else {
            e->Width = 0;
            e->Precision = 0;
            // NOTE 'o', 's' and 'p' do not care about width/precision, so they are ignored here.
        }
    }
    e->Type = type;
    e->Ref();
    return e;
}

CArrayList<CStringFormatElement*>* CString::GetStringFormatElements(const CString* s)
{
    CArrayList<CStringFormatElement*>* list = new CArrayList<CStringFormatElement*>();

    int i = 0;
    while(i < s->Length()) {
        const so_char16 c = s->Chars()[i];

        if(c == SKIZO_CHAR('%')) {
            if(i < s->Length() - 1 && s->Chars()[i + 1] == SKIZO_CHAR('%')) {
                i++; // Skips the next '%'.
                goto end;
            }

            // After '%' was found, searches for a type mark.
            for(int j = i; j < s->Length(); j++) {
                // Even though we have found the type mark, it still can be invalid.
                const char* type = 0;
                if(s->Chars()[j] == SKIZO_CHAR('d')) {
                    type = "d";
                } else if(s->Chars()[j] == SKIZO_CHAR('l')) {
                    type = "l";
                } else if(s->Chars()[j] == SKIZO_CHAR('f')) {
                    type = "f";
                } else if(s->Chars()[j] == SKIZO_CHAR('o')) {
                    type = "o";
                } else if(s->Chars()[j] == SKIZO_CHAR('s')) {
                    type = "s";
                } else if(s->Chars()[j] == SKIZO_CHAR('p')) {
                    type = "p";
                }

                if(type) {
                    Auto<CStringFormatElement> e (createElement(s, i + 1, j - i - 1, type));
                    if(e) {
                        list->Add(e);
                        i = j;
                    }
                    goto end;
                }
            }
        }

    end:
        i++;
    }

    return list;
}

const CString* CString::Format(const CString* format, ...)
{
    const CString* r;

    va_list vl;
    va_start(vl, format);

    r = CString::FormatImpl(format, vl);

    // A no-op on most systems, no problem if the wrapped Format(..)
    // throws an exception and this macro isn't called.
    va_end(vl);
    return r;
}

const CString* CString::FormatImpl(const char* format, va_list vl)
{
    Auto<const CString> tmp /* = */(CString::FromUtf8(format));
    return CString::FormatImpl(tmp, vl);
}

const CString* CString::Format(const char* format, ...)
{
    const CString* r;

    va_list vl;
    va_start(vl, format);

    r = CString::FormatImpl(format, vl);

    va_end(vl);
    return r;
}

const CString* CString::FormatImpl(const CString* format, va_list vl)
{
    Auto<CStringBuilder> sb (new CStringBuilder());
    Auto<CArrayList<CStringFormatElement*> > es (GetStringFormatElements(format));
    int lastIndex = 0;

    for(int i = 0; i < es->Count(); i++) {
        CStringFormatElement* e = es->Item(i);

        if((e->StartOffset - lastIndex) != 0) {
            sb->Append(format, lastIndex, e->StartOffset - lastIndex);
        }
        lastIndex = e->StartOffset + e->Length;

        if(strcmp(e->Type, "d") == 0) {

            if(e->Width == 0) {
                sb->Append(va_arg(vl, int));
            } else {
                Auto<const CString> intAsStr (CoreUtils::IntToString(va_arg(vl, int)));
                const int diff = e->Width - intAsStr->Length();
                // Pads.
                if(diff > 0) {
                    for(int j = 0; j < diff; j++) {
                        sb->Append(SKIZO_CHAR('0'));
                    }
                }
                sb->Append(intAsStr);
            }

        } else if(strcmp(e->Type, "l") == 0) {

            if(e->Width == 0) {
                sb->Append(va_arg(vl, so_long));
            } else {
                Auto<const CString> longAsStr (CoreUtils::LongToString(va_arg(vl, so_long)));
                const int diff = e->Width - longAsStr->Length();
                // Pads.
                if(diff > 0) {
                    for(int j = 0; j < diff; j++) {
                        sb->Append(SKIZO_CHAR('0'));
                    }
                }
                sb->Append(longAsStr);
            }

        } else if(strcmp(e->Type, "f") == 0) {

            // NOTE 'float' is promoted to 'double' when passed through '...'.
            if(e->Width == 0 && e->Precision == 0) {
                sb->Append((float)va_arg(vl, double));
            } else {
                // TODO width ignored by now in floats
                const float f = (float)va_arg(vl, double);
                Auto<const CString> floatAsStr (CoreUtils::FloatToString(f, e->Precision, true));
                sb->Append(floatAsStr);
            }

        } else if(strcmp(e->Type, "o") == 0) {

            sb->Append(va_arg(vl, CObject*));

        } else if(strcmp(e->Type, "s") == 0) {

            sb->Append(va_arg(vl, char*));

        } else if(strcmp(e->Type, "p") == 0) {

            char buf[64]; // Must suffice.
        #ifdef SKIZO_WIN
            sprintf(buf, "0x%p", (va_arg(vl, void*)));
        #elif SKIZO_X
            sprintf(buf, "%p", (va_arg(vl, void*)));
        #else
            SKIZO_REQ_NEVER
        #endif
            sb->Append(buf);

        }
    }

    // The remaining string.
    if((format->Length() - lastIndex) != 0) {
        sb->Append(format, lastIndex, format->Length() - lastIndex);
    }

    return sb->ToString();
}

bool CString::Equals(const CString* str) const
{
    if(!str) {
        return false;
    }

    if(this == str) {
        return true;
    }

    if(m_length != str->m_length) {
        return false;
    }

    // TODO use something like so_wcscmp_16bit
    for(int i = 0; i < m_length; i++) {
        if((&m_chars)[i] != (&str->m_chars)[i]) {
            return false;
        }
    }

    return true;
}

bool CString::Equals(const CObject* obj) const
{
    if(!obj) {
        return false;
    }

    const CString* str = dynamic_cast<const CString*>(obj);
    return this->Equals(str);
}

bool CString::TryParseString(const CString** result) const
{
    int nl = m_length,
             startIndex = 0;
    int endIndex = nl;

    const so_char16* const chars = &m_chars;

    for(int i = 0; i < nl; i++) {
        if(chars[i] == SKIZO_CHAR('\"')) {
            startIndex = i;
            break;
        } else if(chars[i] != SKIZO_CHAR(' ')) {
            return false;
        }
    }

    for(int i = m_length; i-- > 0;) {
        if(chars[i] == SKIZO_CHAR('\"')) {
            endIndex = i;
            break;
        } else if(chars[i] != SKIZO_CHAR(' ')) {
            return false;
        }
    }

    if(startIndex >= endIndex) {
        return false;
    }

    const CString* preReady = this->Substring(startIndex + 1, endIndex - startIndex - 1);
    *result = preReady;

    return true;
}

float CString::ParseFloat() const
{
    float r;
    if(!TryParseFloat(&r)) {
        SKIZO_THROW(EC_BAD_FORMAT);
        return 0.0f;
    }
    return r;
}

#define SKIZO_PARSEFLOAT_FASTPATH_TRIGGER (32-1) // minus null-termination
bool CString::TryParseFloat(float* out) const
{
    so_byte fastBuf[SKIZO_PARSEFLOAT_FASTPATH_TRIGGER + 1]; // plus null-termination
    bool fastPath;
    char* cs;
    int csLen;
    bool error = false;

    if(m_length > SKIZO_PARSEFLOAT_FASTPATH_TRIGGER) {
        fastPath = false;
    } else {
        // Starts copying to the fast buffer immediately; if it encounters a non-ASCII
        // character in the process, immediately cancels the fast path.
        fastPath = true;

        for(int i = 0; i < m_length; i++) {
            so_char16 c = (&m_chars)[i];

            if(c < 256) {
                if(c == SKIZO_CHAR(',')) {
                    c = SKIZO_CHAR('.');
                }

                fastBuf[i] = (so_byte)c;
            } else {
                fastPath = false;
                break;
            }
        }

        fastBuf[m_length] = 0; // null-termination
    }

    if(fastPath) {
        cs = reinterpret_cast<char*>(&fastBuf[0]);
        csLen = m_length; // proven to be ASCII, same length
    } else {
        cs = this->ToUtf8();
        csLen = strlen(cs);

        for(int i = 0; i < csLen; i++) {
            if(cs[i] == ',') {
                cs[i] = '.';
            }
        }
    }

    char* endptr;
    const float r = strtof(cs, &endptr);
    error = *endptr != '\0';

    if(!fastPath) {
        CString::FreeUtf8(cs);
    }

    if(error) {
        return false;
    } else {
        *out = r;
        return true;
    }
}

bool CString::TryParseBool(bool* b) const
{
    // TODO would be good to avoid allocations here
    Auto<const CString> workingCopy (this->Trim());
    if(workingCopy->EqualsASCII("true") || workingCopy->EqualsASCII("True")) {
        *b = true;
        return true;
    } else if(workingCopy->EqualsASCII("false") || workingCopy->EqualsASCII("False")) {
        *b = false;
        return true;
    } else {
        return false;
    }
}

bool CString::ParseBool() const
{
    bool b;
    if(this->TryParseBool(&b)) {
        return b;
    } else {
        SKIZO_THROW(EC_BAD_FORMAT);
        return false; // to shut up the compiler
    }
}

// TODO trim first?
int CString::ParseInt(int startIndex, int count) const
{
    int r;
    if(TryParseInt(&r, startIndex, count)) {
        return r;
    } else {
        SKIZO_THROW(EC_BAD_FORMAT);
        return -1; // to shut up the compiler
    }
}

// TODO return false on overflow! bad code, use sprintf
// the main reason for this half-assed re-implementation was that sprintf expects char*
// and we're UTF16.
bool CString::TryParseInt(int* out_n, int startIndex, int count) const
{
    if(!CoreUtils::ValidateRange(startIndex, &count, m_length)) {
        return false;
    }

    const so_char16* const chars = &m_chars;

    so_long result = 0;
    int sign = 1;
    int position = 1;

    /* Phases: 0 -- can contain some extra whitespaces (after the actual data);
               1 -- must contain only digits (in the middle of the string);
               2 -- can contain minus or whitespaces and nothing more (in the beginning);
               3 -- can contain only whitespaces (before the actual data). */
    int phase = 0;

    int i;
    for(i = startIndex + count; i-- > startIndex;) {
        const so_char16 c = chars[i];

        if((c == SKIZO_CHAR(' ') || c == SKIZO_CHAR('\t'))) {

            if(phase == 1) {
                phase = 2;
            } else {
                continue;
            }

        } else if(c == SKIZO_CHAR('-')) {

            if(phase != 1) { /* Something like "123-", "123- " or "--123". */
                return false;
            } else {
                sign = -1;
                phase = 2;
            }

        } else if(c < SKIZO_CHAR('0') || c > SKIZO_CHAR('9')) {

            return false; /* Something like "12m3". */

        } else { /* It's 0..9! */

            if(phase == 2 || phase == 3) {
                return false; /* Something like "1-123" or "1 123". */
            } else if(phase == 0) {
                phase = 1; /* Keep up the work, compy! */
            }

            result += (c - SKIZO_CHAR('0')) * position;

            if(position == 1) {
                position = 10;
            } else {
                position *= 10;
            }

        }
    }

    result *= sign;

    if(result < SKIZO_INT32_MIN || result > SKIZO_INT32_MAX) {
        SKIZO_THROW(EC_BAD_FORMAT);
    }

    *out_n = (int)result;
    return true;
}

void CString::unsafeSetBuffer(const so_char16* cs, int len) const
{
    if(cs) {
        for(int i = 0; i < len; i++) {
            (&m_chars)[i] = cs[i];
        }
    }

    m_length = len;
    (&m_chars)[len] = 0;
}

const CString* CString::Clone() const
{
    const CString* r = CString::createBuffer(m_length);
    r->unsafeSetBuffer(&m_chars, m_length);

    // Post-condition.
    SKIZO_REQ_EQUALS(this->Length(), r->Length())

    return r;
}

CArrayList<const CString*>* CString::Split(so_char16 c) const
{
    // Resulting array.
    CArrayList<const CString*>* r = new CArrayList<const CString*>();

    // Tmp variable for substrings.
    const CString* substr = nullptr;

    int found = 0, lastFound = 0;
    int i = 0;
    while((found = this->FindChar(c, found, 0)) != -1) {
        // FIX
        if(found - lastFound == 0) {
            substr = createEmptyString();
        } else {
            substr = this->Substring(lastFound, found - lastFound);
        }
        r->Add(substr);
        substr->Unref();

        i++;
        if(++found == m_length) {
            break;
        }
        lastFound = found;
    }

    /* ...and the rest. */
    if(found != m_length) {
        substr = this->Substring(lastFound, m_length - lastFound);
        r->Add(substr);
        substr->Unref();
    }

    return r;
}

CArrayList<const CString*>* CString::Split(const CString* del) const
{
    int i = -1;
    Auto<CArrayList<int> > indices (new CArrayList<int>());
    CArrayList<const CString*>* r = new CArrayList<const CString*>();

    while((i = this->FindSubstring(del, i + 1)) != -1) {
        indices->Add(i);
    }

    indices->Add(this->Length());

    int startIndex = 0;
    for(i = 0; i < indices->Count(); i++) {
        const int endIndex = indices->Array()[i];
        if(startIndex - endIndex == 0) {
            Auto<const CString> tmp (CString::CreateEmptyString());
            r->Add(tmp);
        } else {
            Auto<const CString> tmp (this->Substring(startIndex, endIndex - startIndex));
            r->Add(tmp);
        }
        startIndex = endIndex + del->Length();
    }

    return r;
}

bool CString::EqualsASCII(const char* cs) const
{
    if((int)strlen(cs) != m_length) {
        return false;
    }

    for(int i = 0; i < m_length; i++) {
        if((&m_chars)[i] != (so_char16)cs[i]) {
            return false;
        }
    }

    return true;
}

void CString::DebugPrint() const
{
    Console::Write(this);
}

const CString* CString::CreateBuffer(int size, so_char16** out_chars)
{
    SKIZO_REQ_NOT_NEG(size)
    SKIZO_REQ_PTR(out_chars)

    const CString* r = createBuffer(size);
    *out_chars = const_cast<so_char16*>(r->Chars());
    return r;
}

const CString* CString::Trim() const
{
    const so_char16* chars = &m_chars;

    int endOffset;
    int startOffset;

    startOffset = 0;
    while (startOffset < m_length && CoreUtils::IsWhiteSpace(chars[startOffset])) {
        startOffset++;
    }

    endOffset = m_length - 1;
    while (endOffset >= startOffset && CoreUtils::IsWhiteSpace(chars[endOffset])) {
        endOffset--;
    }

    const int length = endOffset - startOffset + 1;
    if (length == m_length) {
        this->Ref();
        // IT's OK, the string is guaranteed to be immutable anyway.
        return const_cast<const CString*>(this);
    }

    // Post-condition: a trimmed string must be smaller, or at least equal.
    SKIZO_REQ(length <= this->Length(), EC_CONTRACT_UNSATISFIED)

    if(length == 0) {
        return createEmptyString();
    }

    return this->Substring(startOffset, length);
}

const CString* CString::CreateEmptyString()
{
    return createEmptyString();
}

    // *****************************
    //   UTF16 => UTF32 conversion
    // *****************************

// WARNIG Required for FreeType.

/*
* Copyright 2001-2004 Unicode, Inc.
*
* Disclaimer
*
* This source code is provided as is by Unicode, Inc. No claims are
* made as to fitness for any particular purpose. No warranties of any
* kind are expressed or implied. The recipient agrees to determine
* applicability of information provided. If this file has been
* purchased on magnetic or optical media from Unicode, Inc., the
* sole remedy for any claim will be exchange of defective media
* within 90 days of receipt.
*
* Limitations on Rights to Redistribute This Code
*
* Unicode, Inc. hereby grants the right to freely use the information
* supplied in this file in the creation of products supporting the
* Unicode Standard, and to make copies of this file in any form
* for internal or external distribution as long as this notice
* remains attached.
*/

#define UNI_SUR_HIGH_START  (so_uint32)0xD800
#define UNI_SUR_HIGH_END    (so_uint32)0xDBFF
#define UNI_SUR_LOW_START   (so_uint32)0xDC00
#define UNI_SUR_LOW_END     (so_uint32)0xDFFF
#define UNI_REPLACEMENT_CHAR (UTF32)0x0000FFFD

#define UNI_MAX_BMP (UTF32)0x0000FFFF
#define UNI_MAX_UTF16 (UTF32)0x0010FFFF

typedef unsigned int so_uint32;
typedef unsigned short so_char16;

static const int halfShift  = 10; /* used for shifting by 10 bits */
static const so_uint32 halfBase = 0x0010000UL;
static const so_uint32 halfMask = 0x3FFUL;

typedef unsigned int    UTF32;  /* at least 32 bits */
typedef unsigned short UTF16;  /* at least 16 bits */
typedef unsigned char UTF8;   /* typically 8 bits */

static const UTF8 firstByteMark[7] = { 0x00, 0x00, 0xC0, 0xE0, 0xF0, 0xF8, 0xFC };

static const char trailingBytesForUTF8[256] = {
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1, 1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
    2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2, 3,3,3,3,3,3,3,3,4,4,4,4,5,5,5,5
};

static const UTF32 offsetsFromUTF8[6] = { 0x00000000UL, 0x00003080UL, 0x000E2080UL,
                     0x03C82080UL, 0xFA082080UL, 0x82082080UL };

enum ConversionResult {
    conversionOK,           /* conversion successful */
    sourceExhausted,        /* partial character in source, but hit end */
    targetExhausted,        /* insuff. room in target for conversion */
    sourceIllegal           /* source sequence is illegal/malformed */
};

enum ConversionFlags {
    strictConversion = 0,
    lenientConversion
};

static ConversionResult ConvertUTF16toUTF32(const so_char16** sourceStart,
                        const so_char16* sourceEnd,
                        so_uint32** targetStart,
                        so_uint32* targetEnd,
                        ConversionFlags flags)
{
    ConversionResult result = conversionOK;
    const so_char16* source = *sourceStart;
    so_uint32* target = *targetStart;
    so_uint32 ch, ch2;
    while (source < sourceEnd) {
        const so_char16* oldSource = source; /*  In case we have to back up because of target overflow. */
        ch = *source++;
        /* If we have a surrogate pair, convert to so_uint32 first. */
        if (ch >= UNI_SUR_HIGH_START && ch <= UNI_SUR_HIGH_END) {
            /* If the 16 bits following the high surrogate are in the source buffer... */
            if (source < sourceEnd) {
                ch2 = *source;
                /* If it's a low surrogate, convert to so_uint32. */
                if (ch2 >= UNI_SUR_LOW_START && ch2 <= UNI_SUR_LOW_END) {
                    ch = ((ch - UNI_SUR_HIGH_START) << halfShift)
                    + (ch2 - UNI_SUR_LOW_START) + halfBase;
                    ++source;
                } else if (flags == strictConversion) { /* it's an unpaired high surrogate */
                    --source; /* return to the illegal value itself */
                    result = sourceIllegal;
                    break;
                }
            } else { /* We don't have the 16 bits following the high surrogate. */
                --source; /* return to the high surrogate */
                result = sourceExhausted;
                break;
            }
        } else if (flags == strictConversion) {
            /* UTF-16 surrogate values are illegal in UTF-32 */
            if (ch >= UNI_SUR_LOW_START && ch <= UNI_SUR_LOW_END) {
                --source; /* return to the illegal value itself */
                result = sourceIllegal;
                break;
            }
        }
        if (target >= targetEnd) {
            source = oldSource; /* Back up source pointer! */
            result = targetExhausted; break;
        }
        *target++ = ch;
    }
    //*sourceStart = source; <== This caused the algorithm not to work.
    //*targetStart = target;

    return result;
}

so_uint32* CString::ToUtf32() const
{
    const so_char16* sourceUtf16 = &m_chars;
    so_uint32* targetUtf32 = new so_uint32[m_length + 1];
    so_uint32* originalTarget = targetUtf32;

    ConversionResult cr = ConvertUTF16toUTF32(&sourceUtf16, sourceUtf16 + m_length,
                                              &targetUtf32, targetUtf32 + m_length,
                                              strictConversion);
    if(cr == conversionOK) {
        //*targetUtf32 = 0;
        return originalTarget;
    } else {
        delete [] originalTarget;

        const char* msg;
        switch(cr) {
            case conversionOK:    msg = "Conversion successful."; break;
            case sourceExhausted: msg = "Partial character in source, but hit end."; break;
            case targetExhausted: msg = "Insufficient room in target for conversion."; break;
            case sourceIllegal:   msg = "Source sequence is illegal/malformed."; break;
            default:              msg = "UTF16 to UTF32 conversion error.";
        }
        SKIZO_THROW_WITH_MSG(EC_MARSHAL_ERROR, msg);
        return nullptr;
    }
}

void CString::FreeUtf32(so_uint32* ds)
{
    delete [] ds; // guranteed to work with nullptr under the standard
}

void CString::FreeUtf8(char* chars)
{
    delete [] chars; // guranteed to work with nullptr under the standard
}

// NOTE The Windows implementation will use the OS-provided functions.

    // ****************************
    //   UTF16 => UTF8 conversion
    // ****************************

// WARNIG Required for interop with C, file access and other libraries which expect UTF8.

/*
* Copyright 2001-2004 Unicode, Inc.
*
* Disclaimer
*
* This source code is provided as is by Unicode, Inc. No claims are
* made as to fitness for any particular purpose. No warranties of any
* kind are expressed or implied. The recipient agrees to determine
* applicability of information provided. If this file has been
* purchased on magnetic or optical media from Unicode, Inc., the
* sole remedy for any claim will be exchange of defective media
* within 90 days of receipt.
*
* Limitations on Rights to Redistribute This Code
*
* Unicode, Inc. hereby grants the right to freely use the information
* supplied in this file in the creation of products supporting the
* Unicode Standard, and to make copies of this file in any form
* for internal or external distribution as long as this notice
* remains attached.
*/

static ConversionResult ConvertUTF16toUTF8 (
        const UTF16** sourceStart, const UTF16* sourceEnd,
        UTF8** targetStart, UTF8* targetEnd, ConversionFlags flags) {
    ConversionResult result = conversionOK;
    const UTF16* source = *sourceStart;
    UTF8* target = *targetStart;
    while (source < sourceEnd) {
        UTF32 ch;
        unsigned short bytesToWrite = 0;
        const UTF32 byteMask = 0xBF;
        const UTF32 byteMark = 0x80;
        const UTF16* oldSource = source; /* In case we have to back up because of target overflow. */
        ch = *source++;
        /* If we have a surrogate pair, convert to UTF32 first. */
        if (ch >= UNI_SUR_HIGH_START && ch <= UNI_SUR_HIGH_END) {
            /* If the 16 bits following the high surrogate are in the source buffer... */
            if (source < sourceEnd) {
                UTF32 ch2 = *source;
                /* If it's a low surrogate, convert to UTF32. */
                if (ch2 >= UNI_SUR_LOW_START && ch2 <= UNI_SUR_LOW_END) {
                    ch = ((ch - UNI_SUR_HIGH_START) << halfShift)
                        + (ch2 - UNI_SUR_LOW_START) + halfBase;
                    ++source;
                } else if (flags == strictConversion) { /* it's an unpaired high surrogate */
                    --source; /* return to the illegal value itself */
                    result = sourceIllegal;
                    break;
                }
            } else { /* We don't have the 16 bits following the high surrogate. */
                --source; /* return to the high surrogate */
                result = sourceExhausted;
                break;
            }
        } else if (flags == strictConversion) {
            /* UTF-16 surrogate values are illegal in UTF-32 */
            if (ch >= UNI_SUR_LOW_START && ch <= UNI_SUR_LOW_END) {
                --source; /* return to the illegal value itself */
                result = sourceIllegal;
                break;
            }
        }
        /* Figure out how many bytes the result will require */
        if (ch < (UTF32)0x80) {      bytesToWrite = 1;
        } else if (ch < (UTF32)0x800) {     bytesToWrite = 2;
        } else if (ch < (UTF32)0x10000) {   bytesToWrite = 3;
        } else if (ch < (UTF32)0x110000) {  bytesToWrite = 4;
        } else {                            bytesToWrite = 3;
                                            ch = UNI_REPLACEMENT_CHAR;
        }

        target += bytesToWrite;
        if (target > targetEnd) {
            source = oldSource; /* Back up source pointer! */
            target -= bytesToWrite; result = targetExhausted; break;
        }
        switch (bytesToWrite) { /* note: everything falls through. */
            case 4: *--target = (UTF8)((ch | byteMark) & byteMask); ch >>= 6;
            case 3: *--target = (UTF8)((ch | byteMark) & byteMask); ch >>= 6;
            case 2: *--target = (UTF8)((ch | byteMark) & byteMask); ch >>= 6;
            case 1: *--target =  (UTF8)(ch | firstByteMark[bytesToWrite]);
        }
        target += bytesToWrite;
    }
    *sourceStart = source;
    *targetStart = target;
    return result;
}

char* CString::ToUtf8() const
{
    const so_char16* sourceUtf16 = &m_chars;

    // NOTE Allocates the maximum possible size. It's an unnecessary overheard to try get the real size,
    // considering that returned strings are mostly temporarily and created on the fly.
    // As for the memory overheard -- most allocators round allocations anyway.
    UTF8* targetUtf8 = new UTF8[m_length * 4 + 1];
    UTF8* originalTarget = targetUtf8;

    ConversionResult cr = ConvertUTF16toUTF8(&sourceUtf16, sourceUtf16 + m_length,
                                             &targetUtf8, targetUtf8 + m_length * 4 + 1,
                                             strictConversion);
    if(cr == conversionOK) {
        *targetUtf8 = 0;
        return (char*)originalTarget;
    } else {
        delete [] originalTarget;

        const char* msg;
        switch(cr) {
            case conversionOK:    msg = "Conversion successful."; break;
            case sourceExhausted: msg = "Partial character in source, but hit end."; break;
            case targetExhausted: msg = "Insufficient room in target for conversion."; break;
            case sourceIllegal:   msg = "Source sequence is illegal/malformed."; break;
            default:              msg = "UTF16 to UTF8 conversion error.";
        }
        SKIZO_THROW_WITH_MSG(EC_MARSHAL_ERROR, msg);
        return nullptr;
    }
}

    // ****************************
    //   UTF8 => UTF16 conversion
    // ****************************

// WARNIG Required for interop with C, file access and other libraries which expect UTF8.

/*
* Copyright 2001-2004 Unicode, Inc.
*
* Disclaimer
*
* This source code is provided as is by Unicode, Inc. No claims are
* made as to fitness for any particular purpose. No warranties of any
* kind are expressed or implied. The recipient agrees to determine
* applicability of information provided. If this file has been
* purchased on magnetic or optical media from Unicode, Inc., the
* sole remedy for any claim will be exchange of defective media
* within 90 days of receipt.
*
* Limitations on Rights to Redistribute This Code
*
* Unicode, Inc. hereby grants the right to freely use the information
* supplied in this file in the creation of products supporting the
* Unicode Standard, and to make copies of this file in any form
* for internal or external distribution as long as this notice
* remains attached.
*/

static bool isLegalUTF8(const UTF8 *source, int length) {
    UTF8 a;
    const UTF8 *srcptr = source+length;
    switch (length) {
    default: return false;
        /* Everything else falls through when "true"... */
    case 4: if ((a = (*--srcptr)) < 0x80 || a > 0xBF) return false;
    case 3: if ((a = (*--srcptr)) < 0x80 || a > 0xBF) return false;
    case 2: if ((a = (*--srcptr)) < 0x80 || a > 0xBF) return false;

        switch (*source) {
            /* no fall-through in this inner switch */
            case 0xE0: if (a < 0xA0) return false; break;
            case 0xED: if (a > 0x9F) return false; break;
            case 0xF0: if (a < 0x90) return false; break;
            case 0xF4: if (a > 0x8F) return false; break;
            default:   if (a < 0x80) return false;
        }

    case 1: if (*source >= 0x80 && *source < 0xC2) return false;
    }
    if (*source > 0xF4) return false;
    return true;
}

ConversionResult ConvertUTF8toUTF16 (
        const UTF8** sourceStart, const UTF8* sourceEnd,
        UTF16** targetStart, UTF16* targetEnd, ConversionFlags flags)
{
    ConversionResult result = conversionOK;
    const UTF8* source = *sourceStart;
    UTF16* target = *targetStart;
    while (source < sourceEnd) {
        UTF32 ch = 0;
        const unsigned short extraBytesToRead = trailingBytesForUTF8[*source];
        if (extraBytesToRead >= sourceEnd - source) {
            result = sourceExhausted; break;
        }
        /* Do this check whether lenient or strict */
        if (!isLegalUTF8(source, extraBytesToRead+1)) {
            result = sourceIllegal;
            break;
        }
        /*
         * The cases all fall through. See "Note A" below.
         */
        switch (extraBytesToRead) {
            case 5: ch += *source++; ch <<= 6; /* remember, illegal UTF-8 */
            case 4: ch += *source++; ch <<= 6; /* remember, illegal UTF-8 */
            case 3: ch += *source++; ch <<= 6;
            case 2: ch += *source++; ch <<= 6;
            case 1: ch += *source++; ch <<= 6;
            case 0: ch += *source++;
        }
        ch -= offsetsFromUTF8[extraBytesToRead];

        if (target >= targetEnd) {
            source -= (extraBytesToRead+1); /* Back up source pointer! */
            result = targetExhausted; break;
        }
        if (ch <= UNI_MAX_BMP) { /* Target is a character <= 0xFFFF */
            /* UTF-16 surrogate values are illegal in UTF-32 */
            if (ch >= UNI_SUR_HIGH_START && ch <= UNI_SUR_LOW_END) {
                if (flags == strictConversion) {
                    source -= (extraBytesToRead+1); /* return to the illegal value itself */
                    result = sourceIllegal;
                    break;
                } else {
                    *target++ = UNI_REPLACEMENT_CHAR;
                }
            } else {
                *target++ = (UTF16)ch; /* normal case */
            }
        } else if (ch > UNI_MAX_UTF16) {
            if (flags == strictConversion) {
                result = sourceIllegal;
                source -= (extraBytesToRead+1); /* return to the start */
                break; /* Bail out; shouldn't continue */
            } else {
                *target++ = UNI_REPLACEMENT_CHAR;
            }
        } else {
            /* target is a character in range 0xFFFF - 0x10FFFF. */
            if (target + 1 >= targetEnd) {
                source -= (extraBytesToRead+1); /* Back up source pointer! */
                result = targetExhausted; break;
            }
            ch -= halfBase;
            *target++ = (UTF16)((ch >> halfShift) + UNI_SUR_HIGH_START);
            *target++ = (UTF16)((ch & halfMask) + UNI_SUR_LOW_START);
        }
    }
    *sourceStart = source;
    *targetStart = target;
    return result;
}

const CString* CString::FromUtf8(const char* chars)
{
    SKIZO_REQ_PTR(chars)

    const UTF8* sourceUtf8 = (UTF8*)chars;
    const int len = strlen(chars);

    // NOTE Once again, allocates the string's size using conservative estimations.
    // NOTE Null terminator already prepended.
    const CString* r = createBuffer(len);
    UTF16* targetUtf16 = (UTF16*)(&r->m_chars);
    UTF16* originalTarget = targetUtf16;

    const ConversionResult cr = ConvertUTF8toUTF16(&sourceUtf8, sourceUtf8 + len,
                                                   &targetUtf16, targetUtf16 + len,
                                                    strictConversion);
    if(cr == conversionOK) {
        // Set the real null terminator.
        *targetUtf16 = 0;
        // Set the real size.
        r->m_length = (targetUtf16 - originalTarget);
        return r;
    } else {
        r->Unref();

        const char* msg;
        switch(cr) {
            case conversionOK:    msg = "Conversion successful."; break;
            case sourceExhausted: msg = "Partial character in source, but hit end."; break;
            case targetExhausted: msg = "Insufficient room in target for conversion."; break;
            case sourceIllegal:   msg = "Source sequence is illegal/malformed."; break;
            default:              msg = "UTF16 to UTF8 conversion error.";
        }
        SKIZO_THROW_WITH_MSG(EC_MARSHAL_ERROR, msg);
        return nullptr;
    }
}

} }
