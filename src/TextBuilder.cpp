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

#include "TextBuilder.h"
#include "Class.h"
#include "Contract.h"
#include "StringSlice.h"
#include <stdarg.h>

#define SKIZO_CONVBUFFER_SIZE 1024

namespace skizo { namespace script {
using namespace skizo::core;

STextBuilder::STextBuilder(int initialSize)
    : m_buffer(initialSize),
      m_tmpBuffer(initialSize),
      m_convBuffer(nullptr)
{

}

STextBuilder::~STextBuilder()
{
    if(m_convBuffer) {
        delete [] m_convBuffer;
    }
}

char* STextBuilder::Chars()
{
    const so_byte* bs = m_buffer.Bytes();
    int size = m_buffer.Size();

    // Null termination.
    // FIXes the previous code that was overflowing the buffer.
    if(bs[size - 1] != 0) {
        m_buffer.AppendByte(0);
    }

    return (char*)m_buffer.Bytes();
}

void STextBuilder::append(const char* buffer, size_t sz)
{
    m_buffer.AppendBytes((so_byte*)buffer, sz);
}

void STextBuilder::append(const char* buffer)
{
    m_buffer.AppendBytes((so_byte*)buffer, strlen(buffer));
}

void STextBuilder::Append(STextBuilder& other)
{
    m_buffer.AppendBytes(other.m_buffer.Bytes(), other.m_buffer.Size());
}

void STextBuilder::Prepend(const char* format, ...)
{
    va_list vl;
    va_start(vl, format);

    m_tmpBuffer.Clear();
    m_tmpBuffer.AppendBytes(m_buffer.Bytes(), m_buffer.Size());
    m_buffer.Clear();
    emitImpl(format, vl);

    m_buffer.AppendBytes(m_tmpBuffer.Bytes(), m_tmpBuffer.Size());

    va_end(vl);
}

void STextBuilder::Emit(const char* format, ...)
{
    va_list vl;
    va_start(vl, format);

    emitImpl(format, vl);

    va_end(vl);
}

void STextBuilder::emitImpl(const char* format, va_list vl)
{
    int len = strlen(format);

    int lastIndex = 0;
    int objCount = 0;
    char buf[64];
    for(int i = 0; i < len; i++)
    {
        const char c = format[i];

        if(c == '%') {
            if((i - lastIndex) != 0) {
                append(format + lastIndex, i - lastIndex);
            }

            switch(format[i + 1])
            {
                case 'd':
                {
                    const int i = va_arg(vl, int);
                    sprintf(buf, "%d", i);
                    append(buf, strlen(buf));
                }
                break;
                case 'f':
                {
                    const float f = (float)(va_arg(vl, double)); // automatically promoted by C from float to double
                    sprintf(buf, "%f", f);
                    append(buf, strlen(buf));
                }
                break;
                case 'p':
                {
                    const void* ptr = va_arg(vl, void*);
                #ifdef SKIZO_WIN
                    sprintf(buf, "0x%p", ptr);
                #elif SKIZO_X
                    sprintf(buf, "%p", ptr); // already embeds 0x TODO LINUX
                #else
                    SKIZO_REQ_NEVER
                #endif
                    append(buf, strlen(buf));
                }
                break;
                case 's':
                {
                    const SStringSlice* ss = va_arg(vl, SStringSlice*);
                    if(ss) { // safety
                        emitStringSlice(ss);
                    }
                }
                break;
                case 't':
                {
                    const STypeRef* tr = va_arg(vl, STypeRef*);
                    if(tr) { // safety
                        emitTypeRef(tr);
                    }
                }
                break;
                case 'S':
                {
                    const char* p = va_arg(vl, char*);
                    if(p) { // safety
                        append(p, strlen(p));
                    }
                }
                break;
                case 'o':
                {
                    const CObject* obj = va_arg(vl, CObject*);
                    if(obj) {
                        Auto<const CString> objAsStr (obj->ToString());
                        Utf8Auto objAsStrUtf8 (objAsStr->ToUtf8());
                        append(objAsStrUtf8, strlen(objAsStrUtf8));
                    }
                }
                break;
                case 'T':
                {
                    const STypeRef* typeRef = va_arg(vl, STypeRef*);
                    if(typeRef) { // safety
                        if(typeRef->ResolvedClass) {
                            Utf8Auto objAsStrUtf8 (typeRef->ResolvedClass->NiceName()->ToUtf8());
                            append(objAsStrUtf8, strlen(objAsStrUtf8));
                        } else if(!typeRef->ClassName.IsEmpty()) {
                            // The type wasn't resolved but we still want to print its name to the user (an error happened).
                            // This functionality duplicates that of CClass::MakeSureNiceNameGenerated() but we
                            // have no other choice as we don't own a reference to CClass*.
                            for(int k = 0; k < typeRef->ArrayLevel; k++) {
                                append("[");
                            }
                            emitStringSlice(&typeRef->ClassName);
                            for(int k = 0; k < typeRef->ArrayLevel; k++) {
                                append("]");
                            }
                            if(typeRef->Kind == E_TYPEREFKIND_FAILABLE) {
                                append("?");
                            } else if(typeRef->Kind == E_TYPEREFKIND_FOREIGN) {
                                append("*");
                            }
                        } else {
                            append("<unknown>");
                        }
                    }
                }
                break;
                case 'C':
                {
                    CClass* pClass = va_arg(vl, CClass*);
                    if(pClass) { // safety
                        Utf8Auto objAsStrUtf8 (pClass->NiceName()->ToUtf8());
                        append(objAsStrUtf8, strlen(objAsStrUtf8));
                    }
                }
                break;
                default:
                    SKIZO_REQ_NEVER;
                    break;
            }

            lastIndex = i + 2;
            objCount++;
            i++;
        }
    }

    // The remaining string.
    if((len - lastIndex) != 0) {
        append(format + lastIndex, len - lastIndex);
    }
}

void STextBuilder::emitStringSlice(const SStringSlice* ss)
{
    int sslen = ss->End - ss->Start;
    char* buf;
    if(sslen >= SKIZO_CONVBUFFER_SIZE) {
        buf = new char[sslen];
    } else {
        if(!m_convBuffer) {
            m_convBuffer = new char[SKIZO_CONVBUFFER_SIZE];
        }
        buf = m_convBuffer;
    }
    for(int j = 0; j < sslen; j++) {
        const so_char16 c = ss->String->Chars()[ss->Start + j];

        SKIZO_REQ(c < 256 && c != 0, EC_ILLEGAL_ARGUMENT); // ! Unicode not supported.

        buf[j] = (char)c;
    }
    if(sslen >= SKIZO_CONVBUFFER_SIZE) {
        delete [] buf;
    }

    append(buf, sslen);
}

void STextBuilder::emitTypeRef(const STypeRef* typeRef)
{
    switch(typeRef->PrimType) {
        case E_PRIMTYPE_VOID: append("void"); break;
        case E_PRIMTYPE_INT: append("int"); break;
        case E_PRIMTYPE_FLOAT: append("float"); break;
        case E_PRIMTYPE_BOOL: append("_so_bool"); break;
        case E_PRIMTYPE_CHAR: append("_so_char"); break;
        case E_PRIMTYPE_INTPTR: append("void*"); break;

        case E_PRIMTYPE_OBJECT:
            SKIZO_REQ(!typeRef->ClassName.IsEmpty(), EC_ILLEGAL_ARGUMENT);
            SKIZO_REQ_PTR(typeRef->ResolvedClass);

            // All closures share the same structure to minimize C code size.
            if(typeRef->ResolvedClass->SpecialClass() == E_SPECIALCLASS_METHODCLASS) {
                append("struct _soX_0Closure*");
            } else {
                append("struct _so_");
                emitStringSlice(&typeRef->ClassName);

                if(!typeRef->ResolvedClass->IsValueType()) {
                    append("*");
                }
            }
            break;

        default:
            SKIZO_REQ_NEVER
            break;
    }
}

void STextBuilder::Clear()
{
    m_buffer.Clear();
}

char* STextBuilder::ClearFormat(const char* format, va_list vl)
{
    this->Clear();

    emitImpl(format, vl);

    // TODO depends on the internals of CString::ToUtf8 which allocates strings
    // as "new char[N]". Should change this as it may break if CString is changed.

    const char* orig = this->Chars();
    char* r = new char[m_buffer.Size() + 1];
    strcpy(r, orig);
    return r;
}

const CString* STextBuilder::ToString()
{
    char* cs = this->Chars();
    return CString::FromUtf8(cs);
}

} }
