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

#include "StringBuilder.h"
#include "Application.h"
#include "Contract.h"
#include "CoreUtils.h"
#include "Marshal.h"
#include "String.h"
#include <stdarg.h>

namespace skizo { namespace core {
using namespace skizo::core::Marshal;

CStringBuilder::CStringBuilder(int cap)
{
    SKIZO_REQ_POS(cap);

    m_count = 0;
    m_cap = cap;
    m_chars = new so_char16[m_cap];
}

CStringBuilder::~CStringBuilder()
{
    SKIZO_REQ_PTR(m_chars);
    delete [] m_chars;
}

void CStringBuilder::Append(const CString* str, int start, int count)
{
    // Pre-condition.
    if(!str || !CoreUtils::ValidateRange(start, &count, str->Length())) {
        SKIZO_THROW(EC_ILLEGAL_ARGUMENT);
    }

#ifdef SKIZO_CONTRACT
    int cctLength = this->Length();
#endif

    /* Expands the underlying char array when needed. */
    expandIfNeeded(count);

    /* Copies chars from str to self basing on current count
       (pointer arithmetic involved). */
    so_wmemcpy_16bit(m_chars + m_count, &str->m_chars + start, count);

    /* Adjusts count. */
    m_count += count;

    // Post-condition.
#ifdef SKIZO_CONTRACT
    SKIZO_REQ_EQUALS(cctLength + count, this->Length());
#endif
}

void CStringBuilder::expandIfNeeded(int toExpand)
{
    if((float)(m_count + toExpand) / (float)m_cap >= 0.75f) {
        so_char16* oldChars = m_chars;
        m_chars = new so_char16[m_cap = m_cap * 2 + toExpand];
        so_wmemcpy_16bit(m_chars, oldChars, m_count);
        delete [] oldChars;
    }
}

const CString* CStringBuilder::ToString() const
{
    const CString* buffer = CString::createBuffer(m_count);

    /* Since string builder doesn't have a null termination, we can't
       use wcscpy. */
    so_wmemcpy_16bit(&buffer->m_chars, m_chars, m_count);

    return buffer;
}

int CStringBuilder::Length() const
{
    return m_count;
}

int CStringBuilder::Capacity() const
{
    return m_cap;
}

so_char16* CStringBuilder::Chars() const
{
    return m_chars;
}

void CStringBuilder::Append(int i)
{
    so_char16 buf[SKIZO_TOWBUFFER_BUFSZ];
    int out_cnt;
    so_char16* ptr = CoreUtils::__int_ToWBuffer(i, buf, &out_cnt);

    expandIfNeeded(out_cnt);

    so_wmemcpy_16bit(m_chars + m_count, ptr, out_cnt); // wcscpy appends null termination what we don't want.

    m_count += out_cnt;
}

void CStringBuilder::Append(so_long l)
{
    Auto<const CString> lAsStr (CoreUtils::LongToString(l));
    this->Append(lAsStr);
}

void CStringBuilder::Append(float f)
{
    Auto<const CString> str (CoreUtils::FloatToString(f, 0, true));
    this->Append(str);
}

void CStringBuilder::Append(so_char16* c)
{
    const int len = so_wcslen_16bit(c);

    expandIfNeeded(len);
    so_wmemcpy_16bit(m_chars + m_count, c, len); // see Append(int)

    m_count += len;
}

void CStringBuilder::Append(const char* c)
{
    Auto<const CString> tmp (CString::FromUtf8(c));
    this->Append(tmp);
}

void CStringBuilder::AppendASCII(const char* c)
{
    SKIZO_REQ_PTR(c);

    const int len = strlen(c);
    for(int i = 0; i < len; i++) {
        this->Append((so_char16)c[i]);
    }
}

void CStringBuilder::Append(so_char16 c)
{
    expandIfNeeded(1);

    m_chars[m_count] = c;
    m_count++;
}

void CStringBuilder::Append(const CObject* obj)
{
    if(obj) {
        Auto<const CString> str (obj->ToString());
        this->Append(str);
    }
}

void CStringBuilder::Append(const SVariant& v)
{
    Auto<const CString> str (v.ToString());
    this->Append(str);
}

void CStringBuilder::SetLength(int cnt)
{
    // Pre-condition.
    SKIZO_REQ_NOT_NEG(cnt);
    SKIZO_REQ(cnt <= m_count, EC_NOT_IMPLEMENTED);

    m_count = cnt;
}

void CStringBuilder::AppendLine()
{
    this->Append(Application::PlatformString(E_PLATFORMSTRING_NEWLINE));
}

void CStringBuilder::AppendFormat(const char* format, ...)
{
    va_list vl;
    va_start(vl, format);
    Auto<const CString> r (CString::FormatImpl(format, vl));
    va_end(vl);

    Append(r);
}

void CStringBuilder::AppendFormat(const CString* format, ...)
{
    va_list vl;
    va_start(vl, format);
    Auto<const CString> r (CString::FormatImpl(format, vl));
    va_end(vl);

    Append(r);
}

void CStringBuilder::Clear()
{
    this->SetLength(0);

    // Post-condition.
    SKIZO_REQ_EQUALS(this->Length(), 0);
}

void CStringBuilder::Remove(int startOffset, int count)
{
    // Pre-condition.
    SKIZO_REQ(startOffset >= 0
        && count > 0
        && startOffset < m_count
        && startOffset + count <= m_count,
        EC_ILLEGAL_ARGUMENT);

#ifdef SKIZO_CONTRACT
    const int cctLength = this->Length();
#endif

    memcpy(m_chars + startOffset,
           m_chars + startOffset + count,
          (m_count - (startOffset + count)) * sizeof(so_char16));

    m_count -= count;

#ifdef SKIZO_CONTRACT
    // Post-condition.
    SKIZO_REQ_EQUALS(this->Length(), cctLength - count);
#endif
}

void CStringBuilder::Insert(int startOffset, so_char16 c)
{
    // Pre-condition.
    SKIZO_REQ(startOffset >= 0 && startOffset <= m_count, EC_ILLEGAL_ARGUMENT);

    if(startOffset == m_count) {
        this->Append(c);
    } else {
        expandIfNeeded(1);

        // Move anything after startOffset forward by 1
        memmove(m_chars + startOffset + 1,
                m_chars + startOffset,
               (m_count - startOffset) * sizeof(so_char16));

        m_chars[startOffset] = c;
        m_count++;
    }
}

void CStringBuilder::Insert(int startOffset, const CString* str)
{
    // Pre-condition.
    SKIZO_REQ(startOffset >= 0 && startOffset <= m_count, EC_ILLEGAL_ARGUMENT);

    if(CString::IsNullOrEmpty(str)) {
        return;
    }

    if(startOffset == m_count) {
        this->Append(str);
    } else {
        expandIfNeeded(str->Length());

        // Moves anything after startOffset forward by the number of chars in str.
        memmove(m_chars + startOffset + str->Length(),
                m_chars + startOffset,
               (m_count - startOffset) * sizeof(so_char16));

        for(int i = 0; i < str->Length(); i++) {
            m_chars[startOffset + i] = str->Chars()[i];
        }

        m_count += str->Length();
    }
}

bool CStringBuilder::Equals(const CString* str) const
{
    if(m_count != str->Length()) {
        return false;
    }

    for(int i = 0; i < m_count; i++) {
        if(m_chars[i] != str->Chars()[i]) {
            return false;
        }
    }

    return true;
}

} }
