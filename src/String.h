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

#ifndef STRING_H_INCLUDED
#define STRING_H_INCLUDED

#include "Object.h"
#include "basedefs.h"

// Forward declarations.
namespace skizo { namespace collections {
        template <class T>
        class CArrayList;
} }

namespace skizo { namespace core {

    // *************************
    //   CStringFormatElement
    // *************************

/**
 * A string format element as used by CString::GetStringFormatElements(..)
 */
class CStringFormatElement final: public CObject
{
    public:
        /**
         * The start offset of the string format element in the original string.
         */
        int StartOffset;

        /**
         * The start of the string format element.
         */
        int Length;

        /**
         * Width for integers and floats. 0 means "don't care"
         */
        int Width;

        /**
         * Precision for floats. 0 means "don't care".
         */
        int Precision;

        const char* Type;
};

    // *************************
    //        CString
    // *************************

/**
 * Represents text as a series of Unicode 16-bit characters (so_char16)
 * terminated with a null code point.
 *
 * @remarks Strings are constant; their values cannot be changed after they are
 * created. Because string objects are immutable they can be shared.
 *
 * @note Each character in a string is defined by a Unicode scalar value, also
 * called a Unicode code point or the ordinal (numeric) value of the Unicode
 * character. Each code point is encoded by using UTF-16 encoding, and the numeric
 * value of each element of the encoding is represented by a so_char16 value. A
 * single so_char16 value usually represents a single code point. However, a code
 * point might require more than one encoded so_char16 element.
 *
 * @note You can use the skizo::core::CStringBuilder class instead of the CString
 * class for operations that make multiple changes to the value of a string.
 *
 */
class CString final: public CObject
{
    friend class CStringBuilder;

public:
    /**
     * Creates an empty buffer.
     * @param size The size of the string in characters.
     * @param out_chars a pointer to the internal buffer. Strings must be
     * immutable, so change the buffer only once, on string creation.
     */
    static const CString* CreateBuffer(int size, so_char16** out_chars);

    /**
     * Returns the length of this string. The length is equal to the number of
     * 16-bit Unicode characters in the string (null terminator is excluded).
     *
     * @warning The length property returns the number of so_char16 values in
     * this instance, not the number of Unicode characters. The reason is that
     * a Unicode character might be represented by more than one so_char16 value.
     */
    int Length() const;

    /**
     * Direct access to the underlying chars. To be read in loops.
     * The underlying buffer is null terminated ("null" is a 16 bit "0x00 0x00" value).
     */
    const so_char16* Chars() const;

    /**
     * Returns the char value at the specified index. An index ranges from 0 to
     * Length() - 1.
     *
     * @throws EC_OUT_OF_RANGE index indicate a position not within this string
     */
    int CharAt(int index) const;

    /**
     * Indicates whether the specified string is null or an empty string, that
     * is, the value of this instance is the zero-length string, "".
     *
     * @param str true if the value parameter is null or an empty string;
     *            otherwise, false.
     */
    static bool IsNullOrEmpty(const CString* str);

    /**
     * Converts a single so_char16 value into a 1-char string containing the
     * character.
     */
    static const CString* FromChar(so_char16 c);

    /**
     * Converts a null-terminated sequence of characters in the UTF8 encoding
     * to a CString.
     */
    static const CString* FromUtf8(const char* chars);

    /**
     * Converts a null-terminated sequence of wide chars in the UTF16 encoding
     * to a CString.
     */
    static const CString* FromUtf16(const so_char16* chars);

    /**
     * Converts a null-terminated sequence of characters in the ASCII encoding
     * to a CString. This method is preferred over FromUtf8 in cases where
     * a string literal is known to only have basic Latin characters.
     */
    static const CString* FromASCII(const char* chars);

    /**
     * Frees a temporary UTF8 string which was returned by one of the
     * ToUtf8 methods.
     *
     * @note Typically used for communication with platform-dependent code.
     */
    static void FreeUtf8(char* chars);

    /**
     * Frees a temporary "C library" string which was returned by the
     * ToCLibString().
     *
     * @note Typically used for communication with C libraries.
     */
    static void FreeCLibString(char* chars);

    /**
     * Converts the string to a UTF8 encoded C buffer.
     *
     * @warning Free the buffer explicitly with ::FreeUtf8()
     */
    char* ToUtf8() const;

    /**
     * Converts the string to a C library-compatible string. On Windows, the C
     * library expects the strings to be in ANSI code pages, not UTF8. On Linux,
     * it's UTF8. This function is used for interop with libraries that expect
     * C library calls instead of OS-specific methods.
     */
    char* ToCLibString() const;

    /**
     * Clones a UTF8 string in a manner which makes it compatible with ::FreeUtf8(..)
     */
    static char* CloneUtf8(const char* chars);

    /**
     * Concatenates the current string with one or two strings.
     *
     * @param str2 the first string to concatenate.
     * @param str3 the second string to concatenate. Can be null.
     */
    const CString* Concat(const CString* str2, const CString* str3 = nullptr) const;

    /**
     * Concatenates the current string with one or two UTF8-encoded C strings.
     *
     * @param str2 the first string to concatenate.
     * @param str3 the second string to concatenate. Can be null.
     */
    const CString* Concat(const char* str2, const char* str3 = nullptr) const;

    /**
     * Retrieves a substring from this instance. The substring starts at a specified
     * character position and has a specified length.
     *
     * @param start the zero-based starting character position of a substring in
     * this instance.
     * @param count the number of characters in the substring.
     * @throws EC_OUT_OF_RANGE start+count indicate a position not within
     *         this string
     * @throws EC_OUT_OF_RANGE start or count is less than zero.
     */
    const CString* Substring(int start = 0, int count = 0) const;

    /**
     * Returns a new string in which all the characters of the specified type
     * are deleted.
     */
    const CString* Remove(so_char16 c) const;

    /**
     * Replaces all instances of oldCar with newChar and returns a new string.
     */
    const CString* Replace(so_char16 oldChar, so_char16 newChar) const;

    /**
     * Returns a new string that left-aligns the characters in this string by
     * padding them with spaces on the right, for a specified total length.
     *
     * @param count The number of padding characters.
     */
    const CString* PadRight(int count) const;

    /**
     * Tests if this string starts with the specified prefix.
     * @param cs prefix
     * @return true if the character sequence represented by the argument is a prefix
     *         of the character sequence represented by this string; false otherwise.
     */
    bool StartsWith(const CString* cs) const;

    /**
     * See StartsWith(const CString*).
     */
    bool StartsWith(const char* str) const;

    /**
     * Same as StartsWith(const char*), except more efficient: no internal
     * conversions are performed, as ASCII is assumed.
     */
    bool StartsWithASCII(const char* str) const;

    /**
     * Tests if this string ends with the specified suffix.
     * @param cs suffix
     * @return true if the character sequence represented by the argument is a suffix
     *              of the character sequence represented by this object; false otherwise.
     */
    bool EndsWith(const CString* cs) const;

    /**
     * Same as EndsWith(const CString*)
     */
    bool EndsWith(const char* str) const;

    /**
     * Same as EndsWith(const char*), except more efficient: no internal
     * conversions are performed, as ASCII is assumed.
     */
    bool EndsWithASCII(const char* str) const;

    /**
     * Returns a hash code for this string. The hash code for a string is computed as:
     *
     * \code{.cpp}int h = 0;
     * for(int i = 0; i < length; i++) {
     *   h = ((h << 5) - h) + chars[i];
     * }
     * return h;
     * \endcode
     */
    virtual int GetHashCode() const override;

    /**
     * Returns the index within this string of the first occurrence of the
     * specified character. If no such character occurs in this string, then
     * -1 is returned.
     *
     * @param c a character
     * @param start the zero-based index to start search from.
     * @param count the number of characters in the substring to search in
     * @return -1 if nothing found; the index of the first occurence of the
     * specified character otherwise
     */
    int FindChar(so_char16 c, int start = 0, int count = 0) const;

    /**
     * Same as FindChar, except performs the search from the end.
     */
    int FindLastChar(so_char16 c, int start = 0, int count = 0) const;

    /**
     * Finds the specified substring and returns its offset inside this string; 
     * -1 if not found.
     */
    int FindSubstring(const CString* substring) const;

    /**
     * Finds the specified substring and returns its offset inside this string;
     * -1 if not found.
     *
     * @param sourceStart the start offset to start the search from
     */
    int FindSubstring(const CString* substring, int sourceStart) const;

    /**
     * Finds the specified ASCII substring and returns its offset inside this
     * string; -1 if not found.
     *
     * @param sourceStart the start offset to start the search from
     */
    int FindSubstringASCII(const char* substring, int sourceStart = 0) const;

    /**
     * Converts all of the characters in this string to lower case and returns
     * a new string.
     *
     * @warning Not guaranteed to be linguistically sensitive; for example,
     * always maps uppercase I to lowercase I ("i"), even when the current
     * language is Turkish or Azeri (which require a dotted I) under Windows.
     */
    const CString* ToLowerCase() const;

    /**
     * Converts all of the characters in this string to upper case and returns
     * a new string.
     *
     * @see ToLowerCase
     */
    const CString* ToUpperCase() const;

    /**
     * Compares this string to an object.
     *
     * @param obj the object to compare to
     * @return true if obj is a string, it is not null, and it has the same
     * content as this string.
     */
    virtual bool Equals(const CObject* obj) const override;

    /**
     * See Equals(CObject*)
     */
    bool Equals(const CString* str) const;

    /**
     * Compares the current string with a C buffer expecting that the buffer
     * contains only Latin-1 characters.
     *
     * @note To be used in scenarios when strings are guaranteed to have no
     * characters outside the Latin-1 set.
     * @note Does not allocate any temporary buffers, to be used in critical
     * situations.
     * @return the result is true if the argument contains the same Latin-1
     * characters as this string.
     */
    bool EqualsASCII(const char* cs) const;

    /**
     * Returns itself.
     */
    virtual const CString* ToString() const override;

    /**
     * Parses a format string (for example, "My name is %s, I'm %d years old")
     * and returns a list of format elements ("%s", "%d"), including their
     * positions in the string + additional information specific to the string
     * format element.
     */
    static skizo::collections::CArrayList<CStringFormatElement*>* GetStringFormatElements
            (const CString* s);

    /**
     * Replaces every mask in a specified string with the string representation
     * of a corresponding object in a specified array.
     * @param format the format string with contains data and masks
     * @param ... an array of values
     * @return returns a formatted string.
     *
     * @note Supported masks:
     * \li \%d an integer (32 bit)
     * \li \%l a "long" integer (64 bit)
     * \li \%f a float
     * \li \%s a null-terminated C string (UTF8)
     * \li \%o a CObject-derived object, the result of calling the CObject::ToString() method
     *         is inserted
     * \li \%p pointer (should be cast to void*)
     *
     * @warning Cast every CObject-derived object to CObject before using in this method.
     * @note Double '%%' is translated to a single '%'.
     */
    static const CString* Format(const CString* format, ...);
    static const CString* Format(const char* format, ...);

    // WARNING Have to name it as FormatImpl because if we leave it as Format
    // then the compiler implicitly casts the first char array argument to va_list.
    static const CString* FormatImpl(const CString* format, va_list vl);
    static const CString* FormatImpl(const char* format, va_list vl);

    /**
     * Tries to find a quoted string inside this string. If it succeeds, it removes
     * the quotes and any spaces from the string. If it fails, returns false.
     *
     * @param result The point to a variable which will store the result.
     * @return true if successful; false otherwise.
     */
    bool TryParseString(const CString** result) const;

    /**
     * Converts the string representation of a number to its 32-bit signed integer
     * equivalent.
     *
     * @param out_n The pointer to a variable which will store the result.
     * @param startIndex a zero-based index to start parsing from
     * @param count the number of characters to parse
     * @return true if parsing was successful
     */
    bool TryParseInt(int* out_n, int startIndex = 0, int count = 0) const;

    /**
     * See TryParseInt.
     *
     * @return the result of parsing
     * @throw EC_BAD_FORMAT couldn't parse the string as an integer
     */
    int ParseInt(int startIndex = 0, int count = 0) const;

    /**
     * See TryParseFloat.
     *
     * @return the result of parsing
     * @throw EC_BAD_FORMAT couldn't parse the string as a float
     */
    float ParseFloat() const;

    /**
     * Converts the string representation of a number to its float equivalent.
     *
     * @param out_n The pointer to a variable which will store the result.
     * @return true if parsing was successful
     */
    bool TryParseFloat(float* out) const;

    /**
     * Converts the string representation of a boolean to its bool equivalent.
     *
     * @param out_n The pointer to a variable which will store the result.
     * @return true if parsing was successful
     */
    bool TryParseBool(bool* out_b) const;

    /**
     * See TryParseBool.
     *
     * @return the result of parsing
     * @throw EC_BAD_FORMAT couldn't parse the string as a float
     */
    bool ParseBool() const;

    /**
     * Creates and returns a copy of this string.
     */
    const CString* Clone() const;

    /**
     * Returns a string array that contains the substrings in this instance
     * that are delimited by the specified character.
     *
     * @param c the delimiter
     */
    skizo::collections::CArrayList<const CString*>* Split(so_char16 c) const;

    /**
     * See ::Split(so_char16)
     */
    skizo::collections::CArrayList<const CString*>* Split(const CString* del) const;

    /**
     * Prints the string to the console. For debugging.
     */
    void DebugPrint() const;

    /**
     * Same as CompareTo(const CObject*) but avoids a cast.
     */
    int CompareTo(const CString* other) const;

    /**
     * Removes all leading and trailing white-space characters from the current
     * string object.
     */
    const CString* Trim() const;

    /**
     * Creates an empty string.
     */
    static const CString* CreateEmptyString();

    /**
     * Frees a temporary UTF32 string which was returned by one of the
     * ToUtf32 methods.
     *
     * @warning Required for FreeType; don't delete.
     * @note Typically used for communication with platform-dependent code.
     */
    static void FreeUtf32(so_uint32* chars);

    /**
     * Converts the string to a UTF32 encoded C buffer.
     *
     * @warning Free the buffer explicitly with ::FreeUtf32()
     */
    so_uint32* ToUtf32() const;
    
private:
    mutable int m_length;
    mutable so_char16 m_chars;

    static void* operator new (size_t size, int charCount);
    static void operator delete (void *p);

    static const CString* createBuffer(int size);
    void unsafeSetBuffer(const so_char16* cs, int len) const;
};

    // *************************
    //       Utf8Auto
    // *************************

/**
 * Smart pointer for UTF8 strings returned by CString::ToUtf8(..)
 */
struct Utf8Auto
{
public:
    explicit Utf8Auto(char* utf8)
        : m_utf8(utf8)
    {
    }

    ~Utf8Auto()
    {
        if(m_utf8)
            CString::FreeUtf8(m_utf8);
    }

    operator char*() const { return m_utf8; }
    char* Ptr() const { return m_utf8; }

private:
    char* m_utf8;
};

typedef Utf8Auto CLibStringAuto;

} }

#endif // STRING_H_INCLUDED
