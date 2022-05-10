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

#include "BinaryReader.h"
#include "String.h"
#include "Contract.h"

namespace skizo { namespace io {
using namespace skizo::core;

SBinaryReader::SBinaryReader(CStream* stream, EByteOrder byteOrder)
    : m_stream(stream), m_byteOrder(byteOrder)
{
}

CStream* SBinaryReader::Stream() const
{
    return m_stream;
}

so_byte SBinaryReader::ReadByte()
{
    so_byte b;
    const so_long sz = m_stream->Read((char*)&b, sizeof(so_byte));
    if(sz != sizeof(so_byte)) {
        SKIZO_THROW(EC_BAD_FORMAT);
    }

    return b;
}

so_char16 SBinaryReader::ReadChar()
{
    so_char16 c;
    const so_long sz = m_stream->Read((char*)&c, sizeof(so_char16));
    if(sz != sizeof(so_char16)) {
        SKIZO_THROW(EC_BAD_FORMAT);
    }

    return c;
}

so_long SBinaryReader::ReadLong()
{
    // TODO ?
    SKIZO_REQ_EQUALS(m_byteOrder, E_BYTEORDER_HOST);

    so_long l;
    const so_long sz = m_stream->Read((char*)&l, sizeof(so_long));
    if(sz != sizeof(so_long)) {
        SKIZO_THROW(EC_BAD_FORMAT);
    }

    return l;
}

int SBinaryReader::ReadInt()
{
    int i;
    const so_long sz = m_stream->Read((char*)&i, sizeof(int));
    if(sz != sizeof(int)) {
        SKIZO_THROW(EC_BAD_FORMAT);
    }

    return (m_byteOrder == E_BYTEORDER_NETWORK)? CoreUtils::ByteOrderNetworkToHost(i): i;
}

so_uint16 SBinaryReader::ReadUInt16()
{
    so_char16 r = ReadChar();
    return *((so_uint16*)&r);
}

so_uint32 SBinaryReader::ReadUInt32()
{
    int r = ReadInt();
    return *((so_uint32*)&r);
}

float SBinaryReader::ReadFloat()
{
    float f;
    const so_long sz = m_stream->Read((char*)&f, sizeof(float));
    if(sz != sizeof(float)) {
        SKIZO_THROW(EC_BAD_FORMAT);
    }

    // TODO network
    return f;
}

bool SBinaryReader::ReadBool()
{
    return ReadInt(); // TODO?
}

void SBinaryReader::ReadUtf8(char out_buf[256])
{
    so_byte header8;
    so_long sz = m_stream->Read((char*)&header8, sizeof(so_byte));
    if(sz != sizeof(so_byte)) {
        SKIZO_THROW(EC_BAD_FORMAT);
    }

    const int header = header8;
    // Can't use 256 because we need space for the null terminator.
    if(header == 0 || header == 256) {
        SKIZO_THROW(EC_BAD_FORMAT);
    }

    sz = m_stream->Read(out_buf, header);
    if(sz != header) {
        SKIZO_THROW(EC_BAD_FORMAT);
    }

    // The space for the null terminator was secured by not allowing 256 character
    // long strings.
    out_buf[header] = 0;
}

const CString* SBinaryReader::ReadUTF16()
{
    #define SKIZO_UTF16BUF_SIZE 256
    const int length = this->ReadInt();
    if(length >= (SKIZO_UTF16BUF_SIZE - 1)) { // "-1" for null termination
        SKIZO_THROW(EC_NOT_IMPLEMENTED);
    } else if(length < 0) {
        SKIZO_THROW(EC_BAD_FORMAT);
    } else if(length == 0) {
        return CString::CreateEmptyString();
    }
    so_char16 buf[SKIZO_UTF16BUF_SIZE];
    const so_long sz = m_stream->Read(reinterpret_cast<char*>(&buf[0]), (so_long)(sizeof(so_char16) * length));
    if(sz != so_long(sizeof(so_char16) * length)) {
        SKIZO_THROW(EC_BAD_FORMAT);
    }
    buf[length] = 0; // null termination
    return CString::FromUtf16(&buf[0]);
}

char* SBinaryReader::ReadUTF8()
{
    const int length = this->ReadInt();
    if(length <= 0) {
        SKIZO_THROW(EC_BAD_FORMAT);
    }
    char* const r = new char[length + 1];
    const so_long sz = m_stream->Read(reinterpret_cast<char*>(&r[0]), length);
    if(sz != length) {
        SKIZO_THROW(EC_BAD_FORMAT);
    }
    r[length] = 0; // null termination
    return r;
}

SVariant SBinaryReader::ReadVariant()
{
    const so_uint32 typeEx = this->ReadUInt32();
    const EVariantType type = (EVariantType)typeEx;
    SVariant value;

    // Reads the next data based on the type.
    switch(type) {
        case E_VARIANTTYPE_NOTHING:
            // Read nothing.
            break;

        case E_VARIANTTYPE_INT:
            value.SetInt(this->ReadInt());
            break;

        case E_VARIANTTYPE_BOOL:
            value.SetBool(this->ReadBool());
            break;

        case E_VARIANTTYPE_FLOAT:
            value.SetFloat(this->ReadFloat());
            break;

        default:
            if(typeEx == E_VARIANTTYPEEX_STRING) {
                const so_uint32 length = this->ReadUInt32();
                char* const utf8Str = new char[length + 1]; // null termination wasn't recorded
                m_stream->Read(utf8Str, length);
                utf8Str[length] = 0;
                Auto<const CString> str (CString::FromUtf8(utf8Str));
                value.SetObject(static_cast<const CObject*>(str.Ptr()));
                delete [] utf8Str;
            } else {
                SKIZO_THROW(EC_NOT_IMPLEMENTED);
            }
            break;
    }

    return value;
}

} }
