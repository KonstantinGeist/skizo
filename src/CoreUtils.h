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

#ifndef COREUTILS_H_INCLUDED
#define COREUTILS_H_INCLUDED

/*
 * Various utilities which can't be properly categorized.
 */

#include "basedefs.h"

namespace skizo { namespace core {
class CObject;
class CString;
} }

namespace skizo { namespace core { namespace CoreUtils {

#define SKIZO_TOWBUFFER_BUFSZ 32

    // **************
    //  Validation.
    // **************

/**
 * Internal function.
 */
bool ValidateRange(int startIndex, int* rangeCount, int totalCount);

/**
 * Returns true if the object objects obj1 and obj2 are equal by calling ::Equals(..).
 * Unlike obj1->Equals(obj2), deals well with obj1 or/nd obj2 being null.
 */
bool AreObjectsEqual(const skizo::core::CObject* obj1, const skizo::core::CObject* obj2);

/**
 * Converts an integer to a char16 string. Used by IntToString.
 *
 * @warning buf MUST be 16 wide chars.
 * @warning Only out_count can be NULL, the rest isn't checked.
 */
so_char16* __int_ToWBuffer(int n, so_char16* buf, int* out_count);

    // ******************
    //  Array functions.
    // ******************

/**
 * Reallocates an array, used by collections.
 */
template <class T>
T* ReallocArray(T* oldArr, int oldSize, int newSize)
{
    T* newArr = new T[newSize];
    for(int i = 0; i < oldSize; i++)
        newArr[i] = oldArr[i]; // NOTE required for correctly handling by-value structs
    delete [] oldArr;
    return newArr;
}

    // **************
    //  Exceptions.
    // **************

/**
 * This method allows to intercept all exceptions, if needed, or add some
 * additional functionality (in the future).
 *
 * Don't call directly, rather use SKIZO_THROW* macros instead.
 */
void ThrowHelper(int exCode, const char* msg, const char* file, int line);

    // ***********
    //   Atomic.
    // ***********

/**
 * Increments a specified variable and stores the result, as an atomic operation.
 */
inline int AtomicIncrement(so_atomic_int* vari)
{
	// NOTE: uses 'relaxed' just like std::shared_ptr
	return vari->fetch_add(1, std::memory_order_relaxed) + 1;
}

/**
 * Increments a specified variable and stores the result, as an atomic operation.
 */
inline int AtomicDecrement(so_atomic_int* vari)
{
	// NOTE: uses 'relaxed' just like std::shared_ptr
	return vari->fetch_sub(1, std::memory_order_relaxed) - 1;
}

/**
 * Sets an integer variable to the specified value as an atomic operation.
 */
inline void AtomicWrite(so_atomic_int* vari, int v)
{
	*vari = v;
}

/**
 * Atomically reads an integer variable.
 */
inline int AtomicRead(so_atomic_int* vari)
{
    return (int)(*vari);
}

    // ***************
    //   Characters.
    // ***************

/**
 * Returns true if c is a number; false otherwise.
 */
bool IsDigit(so_char16 c);

/**
 * Returns true if c is a whitespace; false otherwise.
 */
bool IsWhiteSpace(so_char16 c);

/**
 * Returns true if c is a letter; false otherwise.
 */
bool IsLetter(so_char16 c);

/**
 * Indicates whether a specified Unicode character is categorized as a control
 * character.
 */
bool IsControl(so_char16 c);

    // ***************
    //   Primitives.
    // ***************

inline int IntCompareFunction(int x, int y)
{
    return x - y;
}

/**
 * Converts an integer (most likely 32 bit) to its string representation.
 *
 * @param i the integer value to convert
 */
const skizo::core::CString* IntToString(int i);

/**
 * Converts an integer (most likely 64 bit) to its string representation.
 *
 * @param i the integer value to convert
 */
const skizo::core::CString* LongToString(so_long l);

/**
 * Converts a float to its string representation.
 *
 * @param f the float value to convert
 */
const skizo::core::CString* FloatToString(float f, int precision = 0, bool noTrailingZeros = false);

/**
 * Converts a boolean to its string representation.
 *
 * @param b the boolean value to convert
 */
const skizo::core::CString* BoolToString(bool b);

/**
 * Converts a pointer to its string representation.
 *
 * @param b the pointer value to convert
 */
const skizo::core::CString* PtrToString(void* ptr);

/**
 * Converts the value of a Unicode character to its uppercase equivalent.
 */
so_char16 CharToUpperCase(so_char16 c);

/**
 * Converts the value of a Unicode character to its lowercase equivalent.
 */
so_char16 CharToLowerCase(so_char16 c);

/**
 * Returns true if the character is uppercase.
 *
 * @note The implementation makes use of CharToUpperCase.
 * @note May allocate memory.
 * @note May return true for characters which do not have upper case/lower case
 * distinction.
 */
bool IsCharUpperCase(so_char16 c);

/**
 * Returns true if the character is lowercase.
 *
 * @note The implementation makes use of CharToLowerCase.
 * @note May allocate memory.
 * @note May return true for characters which do not have upper case/lower case
 * distinction.
 */
bool IsCharLowerCase(so_char16 c);

/**
 * Checks if a specified path is a valid path.
 */
void ValidatePath(const CString* path);

    // *********
    //   GUI.
    // *********

/**
 * Shows a simple UI message (for low-level diagnostics).
 *
 * @param msg The message to show.
 * @param isFatal specifies whether the error is fatal
 *
 * @note Allocates memory (may create a new thread), so not a good
 * way to respond to out-of-memory errors.
 */
void ShowMessage(const CString* msg, bool isFatal);
void ShowMessage(const char* msg, bool isFatal);

// **************************************************************

#ifdef SKIZO_WIN
/**
 * Throws an exception based on the last WinAPI error.
 *
 * @note Windows-only.
 */
void ThrowWin32Error();
#endif

    // ***********
    //   Memory.
    // ***********

/**
 * Takes a value (amount of memory in bytes) and makes a user-friendly string.
 * The value is treated as a binary size (2^N). Localized.
 */
const skizo::core::CString* MemorySizeToString(so_long sz);

/**
 * Dumps the contents of the memory pointed to by "mem" (of the size "sz") to
 * the console. For debugging.
 */
void DumpMemory(const void* mem, size_t sz);

/**
 * Intentionally segfaults to get trapped inside a debugger or to test how
 * the application responds to segmentation faults.
 */
void SegFault();

/**
 * Converts the value from host to TCP/IP network byte order.
 */
so_uint32 ByteOrderHostToNetwork(so_uint32 c);

/**
 * Converts the value from TCP/IP network to host byte order.
 */
so_uint32 ByteOrderNetworkToHost(so_uint32 c);

// *************************

} 

} }

#endif // COREUTILS_H_INCLUDED
