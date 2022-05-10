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

#include "Exception.h"
#include "String.h"
#include "StringBuilder.h"

namespace skizo { namespace core {

void SException::InitBase(EExceptionCode eCode, const char* msg)
{
    m_eCode = eCode;
    m_msg = msg;

    if(!msg) {
        switch(eCode) {
            case EC_NOT_IMPLEMENTED:
                m_msg = "The method or operation is not implemented.";
                break;

            case EC_PLATFORM_DEPENDENT:
                m_msg = "A platform-dependent error occured while running one of the methods.";
                break;

            case EC_INVALID_STATE:
                m_msg = "Operation is not valid due to the current state of the object.";
                break;

            case EC_MARSHAL_ERROR:
                m_msg = "Marshalling failed (invalid input).";
                break;

            case EC_ILLEGAL_ARGUMENT:
                m_msg = "Value does not fall within the expected range.";
                break;

            case EC_PATH_NOT_FOUND:
                m_msg = "Unable to find the specified path (device, file or directory).";
                break;

            case EC_OUT_OF_RANGE:
                m_msg = "Specified argument was out of the range of valid values.";
                break;

            case EC_EXECUTION_ERROR:
                m_msg = "Execution engine failed.";
                break;

            case EC_TYPE_MISMATCH:
                m_msg = "Type mismatch.";
                break;

            case EC_CONCURRENT_MODIFICATION:
                m_msg = "Concurrent modification of a collection.";
                break;

            case EC_TIMEOUT:
                m_msg = "Operation timeout.";
                break;

            case EC_CONNECTION_LOST:
                m_msg = "Connection lost.";
                break;

            case EC_PARAMETER_COUNT_MISMATCH:
                m_msg = "The number of parameters for an invocation does not match the number expected.";
                break;

            case EC_KEY_NOT_FOUND:
                m_msg = "Key not found";
                break;

            case EC_MISSING_MEMBER:
                m_msg = "Request member is missing.";
                break;

            case EC_WRONG_THREAD:
                m_msg = "Method called from a wrong thread.";
                break;

            case EC_CONTRACT_UNSATISFIED:
                m_msg = "Contract left unsatisfied.";
                break;

            case EC_ACCESS_DENIED:
                m_msg = "Access denied.";
                break;

            default: break;
        }
    }
}

SException::SException(EExceptionCode eCode, const char* msg)
{
    this->InitBase(eCode, msg);
}

const CString* SException::ToString() const
{
    Auto<CStringBuilder> sb (new CStringBuilder());

    if(m_msg) {
        sb->Append(SKIZO_CHAR('\''));
        Auto<const CString> t (CString::FromUtf8((char*)m_msg));
        sb->Append(t);
        sb->Append(SKIZO_CHAR('\''));
    } else {
        sb->Append("Error Code: ");
        sb->Append(m_eCode);
        sb->Append(SKIZO_CHAR('.'));
    }

    return sb->ToString();
}

} }
