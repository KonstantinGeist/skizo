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

#ifndef STRINGBUILDER_H_INCLUDED
#define STRINGBUILDER_H_INCLUDED

#include "Object.h"
#include "Variant.h"

namespace skizo { namespace core {
class CString;

/**
 * Represents a mutable string of characters. The builder automatically expands
 * or shrinks when necessary.
 *
 * @note This class represents a string-like object whose value is a mutable
 * sequence of characters. The value is said to be mutable because it can be
 * modified after creation by appending, removing, replacing, or inserting
 * characters. For comparison, see the CString class.
 */
class CStringBuilder final: public CObject
{
public:
    /**
     * Constructs a string builder with no characters in it and an initial capacity
     * specified by the cap argument.
     *
     * @param cap the initial capacity.
     */
    explicit CStringBuilder(int cap = 32);

    virtual ~CStringBuilder();

    /**
     * Returns the current capacity. The capacity is the amount of storage
     * available for newly inserted characters, beyond which an allocation will occur.
     *
     * @return the current capacity.
     */
    int Capacity() const;

    /**
     * Returns the length of this character sequence. The length is the number
     * of 16-bit chars in the sequence.
     *
     * @return the number of chars in this sequence.
     */
    int Length() const;

    /**
     * Direct access to the underlying chars.
     *
     * @warning The buffer is not null terminated.
     */
    so_char16* Chars() const;

    /**
     * Appends the string representation of the CObject argument.
     * The argument is converted to a string as if by the method CObject::ToString,
     * and the characters of that string are then appended to this sequence.
     *
     * @param obj - a CObject.
     */
    void Append(const CObject* obj);

    /**
     * Appends a variant value by calling its .ToString() method.
     */
    void Append(const SVariant& v);

    /**
     * Appends the specified string to this character sequence in the specified
     * range (optionally).
     *
     * @param str The specified string.
     * @param start Start index into the specified string.
     * @param count Number of characters to copy from the specified string to
     * this string builder.
     */
    void Append(const CString* str, int start = 0, int count = 0);

    /**
     * Appends the string representation of the int argument to this sequence.
     * The argument is converted to a string as if by the method IntToString,
     * and the characters of that string are then appended to this sequence.
     *
     * @param i an int
     */
    void Append(int i);

    /**
     * Appends the string representation of the long argument to this sequence.
     * The argument is converted to a string as if by the method LongToString,
     * and the characters of that string are then appended to this sequence.
     *
     * @param l a long int
     */
    void Append(so_long l);

    /**
     * Appends the string representation of the float argument to this sequence.
     * The argument is converted to a string as if by the method FloatToString,
     * and the characters of that string are then appended to this sequence.
     *
     * @param f an float
     */
    void Append(float f);

    /**
     * Appends the string representation of the char argument to this sequence.
     * The argument is appended to the contents of this sequence. The length of
     * this sequence increases by 1. The overall effect is exactly as if the
     * argument were converted to a string by the method CString::FromChar and
     * the character in that string were then appended to this character sequence.
     *
     * @param c a char.
     */
    void Append(so_char16 c);

    /**
     * Appends a wide char string ended with null, with the effect of repeatedly
     * calling Append(so_char16).
     */
    void Append(so_char16* c);

    /**
     * Appends an null-terminated utf8 string ended with null.
     */
    void Append(const char* c);

    /**
     * Optimized version which appends an ASCII string.
     */
    void AppendASCII(const char* c);

    /**
     * Appends a formatted string by calling CString::Format and appending the
     * resulting chars to this character sequence.
     */
    void AppendFormat(const char* format, ...);

    /**
     * Appends a formatted string by calling CString::Format and appending the
     * resulting chars to this character sequence.
     */
    void AppendFormat(const CString* format, ...);

    /**
     * Returns a string representing the data in this sequence. A new CString
     * object is allocated and initialized to contain the character sequence
     * currently represented by this object. This CString is then returned.
     * Subsequent changes to this sequence do not affect the contents of the
     * CString.
     */
    virtual const CString* ToString() const override;

    /**
     * Sets the length of the sequence.
     *
     * @warning Current implemenation only truncates.
     * @param length The new length of the sequence.
     * @throws EC_OUT_OF_RANGE if length < 0
     * @throws EC_NOT_IMPLEMENTED if length > Length()
     */
    void SetLength(int length);

    /**
     * Appends the default line terminator to the end of the current
     * CStringBuilder object. The new line terminator is a constant customized
     * specifically for the current platform.
     */
    void AppendLine();

    /**
     * Removes all characters from the current StringBuilder instance.
     * It is a convenience method that is equivalent to calling SetLength(0)
     */
    void Clear();

    /**
     * Removes the characters in a substring of this sequence starting from the
     * specified offset and with the specified count.
     */
    void Remove(int startOffset, int count = 1);

    /**
     * Inserts the string representation of the char argument into this character
     * sequence at the index specified by startOffset argument.
     */
    void Insert(int startOffset, so_char16 c);

    /**
     * Inserts the string value into this character sequence at the index specified
     * by startOffset argument.
     */
    void Insert(int startOffset, const CString* str);

    /**
     * Determines if the content of this string builder is identical to the content
     * of a string. Allows to avoid allocating another string instance.
     */
    bool Equals(const CString* str) const;

private:
    // Character count.
    int m_count;

    // Capacity.
    int m_cap;

    // Underlying chars.
    so_char16* m_chars;

    void expandIfNeeded(int toExpand);
};

} }

#endif // STRINGBUILDER_H_INCLUDED
