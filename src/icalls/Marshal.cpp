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

#include "../Marshal.h"
#include "../Domain.h"
#include "../Class.h"
#include "../NativeHeaders.h"
#include "../RuntimeHelpers.h"
#include "../String.h"

namespace skizo { namespace script {
using namespace skizo::core;

extern "C" {

void* SKIZO_API _so_Marshal_stringToUtf16(void* _str)
{
    if(!_str) {
        return nullptr;
    }

    const CString* str = so_string_of(_str);
    const int strLen = str->Length();
    so_char16* r = new so_char16[strLen + 1];
    so_wcscpy_16bit(r, str->Chars());
    return r;
}

void SKIZO_API _so_Marshal_freeUtf16String(void* pstr)
{
    if(pstr) {
        delete [] (so_char16*)pstr;
    }
}

void* SKIZO_API _so_Marshal_stringToUtf8(void* str)
{
    if(!str) {
        return nullptr;
    }

    try {
        return so_string_of(str)->ToUtf8();
    } catch(const SException& e) {
        // BAD_FORMAT most likely
        return nullptr;
    }
}

int SKIZO_API _so_Marshal_sizeOfUtf8String(void* str)
{
    SKIZO_NULL_CHECK(str);

    return strlen((char*)str);
}

void SKIZO_API _so_Marshal_freeUtf8String(void* pstr)
{
    if(pstr) {
        CString::FreeUtf8((char*)pstr);
    }
}

void* SKIZO_API _so_Marshal_utf8ToString(void* _str)
{
    if(!_str) {
        return nullptr;
    }

    try {
        Auto<const CString> str (CString::FromUtf8((char*)_str));
        return CDomain::ForCurrentThread()->CreateString(str, false);
    } catch(const SException& e) {
        // BAD_FORMAT most likely
        return nullptr;
    }
}

void* SKIZO_API _so_Marshal_utf16ToString(void* _str)
{
    if(!_str) {
        return nullptr;
    }

    try {
        Auto<const CString> str (CString::FromUtf16((so_char16*)_str));
        return CDomain::ForCurrentThread()->CreateString(str, false);
    } catch(const SException& e) {
        // BAD_FORMAT most likely
        return nullptr;
    }
}

void SKIZO_API _so_Marshal_nativeMemoryToArray(void* pArray, void* soArray)
{
    SKIZO_NULL_CHECK(pArray);
    SKIZO_NULL_CHECK(soArray);

	// Retrieves the array header and verifies everything is correct.
    const SArrayHeader* soArrayHeader = (SArrayHeader*)soArray;
    const CClass* klass = so_class_of(soArray);
    if(klass->SpecialClass() != E_SPECIALCLASS_ARRAY) {
        CDomain::Abort("Marshal::nativeMemoryToArray expects an array class as its 2nd argument.");
    }

    // Fixes up the array pointer to point to the beginning of its data and copies data.
    soArray = (char*)soArray + offsetof(SArrayHeader, firstItem);
    const size_t arraySize = soArrayHeader->length * klass->ResolvedWrappedClass()->GCInfo().SizeForUse;
    memcpy(soArray, pArray, arraySize);
}

/*void SKIZO_API _so_Marshal_arrayToNativeBlock(void* soArray, void* pArray)
{
    SArrayHeader* soArrayHeader = (SArrayHeader*)soArray;
    CClass* klass = so_class_of(soArray);
    if(klass->SpecialClass() != E_SPECIALCLASS_ARRAY) {
        CDomain::Abort("Marshal::arrayToNativeBlock expects an array class as 1nd argument.");
    }
    // Fixes up the array pointer to point to the beginning of its data.
    soArray = (char*)soArray + offsetof(SArrayHeader, firstItem);
    size_t arraySize = soArrayHeader->length * klass->m_wrappedClass.ResolvedClass->m_sizeForUse;
    memcpy(pArray, soArray, arraySize);
}*/

// ****************************************************************************************************
#define SKIZO_MEMSAFETY_HEADER 123456789

// Native memory is allocated with a safety header, a special magic value right before the actual pointer.
// When freeNativeMemory is called, the runtime checks if the header is present. If there's none -- the pointer
// is incorrect.
void* SKIZO_API _so_Marshal_allocNativeMemory(int size)
{
    if(size <= 0) {
        CDomain::Abort("Memory size must be greater than 0.");
    }

    void* r = malloc(size + sizeof(void*));
    if(!r) {
        _soX_abort0(SKIZO_ERRORCODE_OUT_OF_MEMORY);
    }

    // The header.
    *((int*)r) = SKIZO_MEMSAFETY_HEADER;

    return ((char*)r) + sizeof(void*);
}

void SKIZO_API _so_Marshal_freeNativeMemory(void* ptr)
{
    SKIZO_NULL_CHECK(ptr);

    char* adjusted = (char*)ptr - sizeof(void*);
    if(*((int*)adjusted) != SKIZO_MEMSAFETY_HEADER) {
        CDomain::Abort("Trying to free a corrupt pointer.");
    }
    *((int*)adjusted) = 0; // To prevent double free()'s.

    free(adjusted);
}

// ****************************************************************************************************

void* SKIZO_API _so_Marshal_dataOffset(void* soObj)
{
    SKIZO_NULL_CHECK(soObj);
    const CClass* klass = so_class_of(soObj);

    if(klass->SpecialClass() == E_SPECIALCLASS_ARRAY) {
        return SKIZO_GET_ARRAY_DATA(soObj);
    } else if(klass->FlatName().EqualsAscii("string")) { // TODO faster?
        return (void*)((SStringHeader*)soObj)->pStr->Chars();
    } else {
        return SKIZO_GET_OBJECT_DATA(soObj);
    }
}

void* SKIZO_API _so_Marshal_codeOffset(void* soObj)
{
    return CDomain::ForCurrentThread()->ThunkManager().GetClosureThunk(soObj);
}

// ****************************************************************************

void SKIZO_API _so_Marshal_copyMemory(void* dst, void* src, int size)
{
    SKIZO_NULL_CHECK(dst);
    SKIZO_NULL_CHECK(src);
    if(size < 0) {
        CDomain::Abort("Only positive sizes allowed in Marshal:copyMemory(..)");
    }

    memmove(dst, src, size);
}

// ****************************************************************************

void* SKIZO_API _so_Marshal_offset(void* ptr, int offset)
{
    return (void*)((char*)ptr + offset);
}

int SKIZO_API _so_Marshal_readByte(void* ptr)
{
    SKIZO_NULL_CHECK(ptr);

    return *((unsigned char*)ptr);
}

int SKIZO_API _so_Marshal_readInt(void* ptr)
{
    SKIZO_NULL_CHECK(ptr);

    return *((int*)ptr);
}

void SKIZO_API _so_Marshal_writeByte(void* ptr, int value)
{
    SKIZO_NULL_CHECK(ptr);

    if(value < 0 || value > 255) {
        CDomain::Abort("The value must be in the range [0, 255] (Marshal::writeByte).");
    }

    *((unsigned char*)ptr) = (unsigned char)value;
}

void SKIZO_API _so_Marshal_writeInt(void* ptr, int value)
{
    SKIZO_NULL_CHECK(ptr);

    *((int*)ptr) = value;
}

void* SKIZO_API _so_Marshal_readIntPtr(void* ptr)
{
    SKIZO_NULL_CHECK(ptr);

    return *((void**)ptr);
}

void SKIZO_API _so_Marshal_writeIntPtr(void* ptr, void* value)
{
    SKIZO_NULL_CHECK(ptr);

    *((void**)ptr) = value;
}

float SKIZO_API _so_Marshal_readFloat(void* ptr)
{
    SKIZO_NULL_CHECK(ptr);

    return *((float*)ptr);
}

void SKIZO_API _so_Marshal_writeFloat(void* ptr, float value)
{
    SKIZO_NULL_CHECK(ptr);

    *((float*)ptr) = value;
}

int SKIZO_API _so_Marshal_pointerSize()
{
    return sizeof(void*);
}

}

} }
