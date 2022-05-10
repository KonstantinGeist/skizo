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

#include "Variant.h"
#include "Contract.h"
#include "CoreUtils.h"
#include "String.h"

namespace skizo { namespace core {

#define tryDeleteObject() if(m_type == E_VARIANTTYPE_OBJECT && m_union.m_asObject)\
                             m_union.m_asObject->Unref()

SVariant::SVariant(const SVariant& copy)
{
    if(copy.m_type == E_VARIANTTYPE_OBJECT && copy.m_union.m_asObject) {
        copy.m_union.m_asObject->Ref();
    }

    m_type = copy.m_type;
    m_union = copy.m_union;
}

SVariant& SVariant::operator=(const SVariant& copy)
{
    // NOTE We should ref before unrefing for the case
    // when SetObject is called on the same object twice.

    if(copy.m_type == E_VARIANTTYPE_OBJECT && copy.m_union.m_asObject) {
        copy.m_union.m_asObject->Ref();
    }

    if(m_type == E_VARIANTTYPE_OBJECT && m_union.m_asObject) {
        m_union.m_asObject->Unref();
    }

    m_type = copy.m_type;
    m_union = copy.m_union;

    return *this;
}

void SVariant::SetObject(const CObject* value)
{
    // NOTE We should ref before unrefing for the case
    // when SetObject is called on the same object twice.

    if(value) {
        value->Ref();
    }

    if(m_type == E_VARIANTTYPE_OBJECT && m_union.m_asObject) {
        m_union.m_asObject->Unref();
    }

    m_type = E_VARIANTTYPE_OBJECT;
    m_union.m_asObject = value;
}

SVariant::SVariant(void* v) //-V730
{
    // Default value.
    m_type = E_VARIANTTYPE_NOTHING;
}

SVariant::SVariant() //-V730
{
    // Default value.
    m_type = E_VARIANTTYPE_NOTHING;
}

SVariant::~SVariant()
{
    tryDeleteObject();
}

void SVariant::SetInt(int value)
{
    tryDeleteObject();

    m_type = E_VARIANTTYPE_INT;
    m_union.m_asInt = value;
}

void SVariant::SetBool(bool value)
{
    tryDeleteObject();

    m_type = E_VARIANTTYPE_BOOL;
    m_union.m_asBool = value;
}

void SVariant::SetFloat(float value)
{
    tryDeleteObject();

    m_type = E_VARIANTTYPE_FLOAT;
    m_union.m_asFloat = value;
}

void SVariant::SetNothing()
{
    tryDeleteObject();
    m_type = E_VARIANTTYPE_NOTHING;
}

int SVariant::IntValue() const
{
    SKIZO_REQ(m_type == E_VARIANTTYPE_INT, EC_TYPE_MISMATCH);

    return m_union.m_asInt;
}

bool SVariant::BoolValue() const
{
    SKIZO_REQ(m_type == E_VARIANTTYPE_BOOL, EC_TYPE_MISMATCH);

    return m_union.m_asBool;
}

float SVariant::FloatValue() const
{
    SKIZO_REQ(m_type == E_VARIANTTYPE_FLOAT, EC_TYPE_MISMATCH);

    return m_union.m_asFloat;
}

CObject* SVariant::ObjectValue() const
{
    SKIZO_REQ(m_type == E_VARIANTTYPE_OBJECT, EC_TYPE_MISMATCH);

    return const_cast<CObject*>(m_union.m_asObject);
}

bool SVariant::Equals(const SVariant& other) const
{
    if(m_type != other.m_type)
        return false;

    switch(m_type) {
        case E_VARIANTTYPE_BOOL: return m_union.m_asBool == other.m_union.m_asBool;
        case E_VARIANTTYPE_FLOAT: return m_union.m_asFloat == other.m_union.m_asFloat; //-V550
        case E_VARIANTTYPE_INT: return m_union.m_asInt == other.m_union.m_asInt;
        case E_VARIANTTYPE_NOTHING: return true;
        case E_VARIANTTYPE_OBJECT: return CoreUtils::AreObjectsEqual(m_union.m_asObject, other.m_union.m_asObject);
        case E_VARIANTTYPE_BLOB: return m_union.m_asBlob == other.m_union.m_asBlob;
        default:
        {
            SKIZO_THROW(EC_NOT_IMPLEMENTED);
        }
    }

    return false; // Silences the compiler.
}

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wstrict-aliasing"
int SVariant::GetHashCode() const
{
    switch(m_type) {
        case E_VARIANTTYPE_BOOL: return m_union.m_asBool;
        case E_VARIANTTYPE_FLOAT:
            SKIZO_REQ_EQUALS(sizeof(int32_t), sizeof(float));
            return (int)(*reinterpret_cast<const int32_t*>(&m_union.m_asFloat));
        case E_VARIANTTYPE_INT: return m_union.m_asInt;
        case E_VARIANTTYPE_NOTHING: return 0;
        case E_VARIANTTYPE_OBJECT: return m_union.m_asObject?
                                                m_union.m_asObject->GetHashCode()
                                              : 0;
        case E_VARIANTTYPE_BLOB: return *((int*)&m_union.m_asBlob);
        default:
        {
            SKIZO_THROW(EC_NOT_IMPLEMENTED);
        }
    }

    return 0; // Silences the compiler.
}
#pragma GCC diagnostic pop

const CString* SVariant::ToString() const
{
    switch(m_type) {
        case E_VARIANTTYPE_BOOL: return CoreUtils::BoolToString(m_union.m_asBool);
        case E_VARIANTTYPE_FLOAT: return CoreUtils::FloatToString(m_union.m_asFloat, 0, true);
        case E_VARIANTTYPE_INT: return CoreUtils::IntToString(m_union.m_asInt);
        case E_VARIANTTYPE_NOTHING: return CString::FromASCII("<nothing>");
        case E_VARIANTTYPE_OBJECT: return m_union.m_asObject?
                                                m_union.m_asObject->ToString()
                                              : CString::CreateEmptyString();
        case E_VARIANTTYPE_BLOB: return CString::FromASCII("<blob>");
        default:
        {
            SKIZO_THROW(EC_NOT_IMPLEMENTED);
        }
    }

    return nullptr; // Silences the compiler.
}

void SVariant::SetBlob(void* blob)
{
    m_type = E_VARIANTTYPE_BLOB;
    m_union.m_asBlob = blob;
}

void* SVariant::BlobValue() const
{
    SKIZO_REQ(m_type == E_VARIANTTYPE_BLOB, EC_TYPE_MISMATCH);

    return m_union.m_asBlob;
}

} }
