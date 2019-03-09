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

#ifndef EXCEPTION_H_INCLUDED
#define EXCEPTION_H_INCLUDED

#include "Object.h"
#include "CoreUtils.h"

namespace skizo { namespace core {

/** @brief
 * Throw skizo exceptions with this macro. Calls are redirected to
 * skizo::core::CoreUtils::ThrowHelper(..)
 */
#define SKIZO_THROW(ec) skizo::core::CoreUtils::ThrowHelper(ec, 0, __FILE__, __LINE__)

/** Same as SKIZO_THROW, except allows to provide a message. */
#define SKIZO_THROW_WITH_MSG(ec, msg) skizo::core::CoreUtils::ThrowHelper(ec, msg, __FILE__, __LINE__)

class CString;

/**
 * Instead of defining numerous exception classes, skizo uses the concept of exception codes for trivial system exceptions.
 */
enum EExceptionCode 
{
    /**
     * No errors found.
     */
    EC_OK = 0,

    /**
     * A custom user-defined exception not covered by any of the default exception
     * codes.
     */
    EC_CUSTOM = 1,

    /**
     * The expected functionality was not implemented.
     */
    EC_NOT_IMPLEMENTED = 2,

    /**
     * A platform-dependent error was encountered.
     */
    EC_PLATFORM_DEPENDENT = 3,

    /**
     * Invalid state of the object while calling a method/performing an action.
     */
    EC_INVALID_STATE = 4,

    /**
     * Marshal (conversion) error.
     */
    EC_MARSHAL_ERROR = 5,

    /**
     * Illegal argument passed to a method.
     */
    EC_ILLEGAL_ARGUMENT = 6,

    /**
     * The given value was outside of the range of allowed values.
     */
    EC_OUT_OF_RANGE = 7,

    /**
     * Given path not found (does not exist or is not available).
     */ 
    EC_PATH_NOT_FOUND = 8,

    /**
     * Bad format: the data is corrupt.
     */
    EC_BAD_FORMAT = 9,

    /**
     * Execution error happened while trying to execute a script.
     */
    EC_EXECUTION_ERROR = 10,

    /**
     * Specified key not found.
     */
    EC_KEY_NOT_FOUND = 11,

    /**
     * A type mismatch detected.
     */
    EC_TYPE_MISMATCH = 12,

    /**
     * The program ran out of system resources.
     */
    EC_OUT_OF_RESOURCES = 13,

    /**
     * Concurrent modification to a collection detected: trying to modify data
     * while reading it.
     */
    EC_CONCURRENT_MODIFICATION = 14,

    /**
     * Timeout reached when attempting to perform an action.
     */
    EC_TIMEOUT = 15,

    /**
     * Connection lost.
     */
    EC_CONNECTION_LOST = 16,

    /**
     * The given number of arguments does not match the expected number of arguments.
     */
    EC_PARAMETER_COUNT_MISMATCH = 17,

    /**
     * Missing member (for scripting).
     */
    EC_MISSING_MEMBER = 18,

    /**
     * Method or action was performed on a wrong thread.
     */
    EC_WRONG_THREAD = 19,

    /**
     * Contract was not satisfied (invalid argument).
     */
    EC_CONTRACT_UNSATISFIED = 20,

    /**
     * Access denied to perform the action.
     */
	EC_ACCESS_DENIED = 21
};

/**
 * Represents errors that occur during application execution.
 * Throw exceptions with the SKIZO_THROW macro (or SKIZO_THROW_WITH_MSG).
 *
 * @note Custom user exceptions that inherit from SException must set the
 * exception code to EC_CUSTOM in the constructor.
 */
struct SException
{
private:
    EExceptionCode m_eCode;
    const char* m_msg;

protected:
    void InitBase(EExceptionCode eCode, const char* msg);

public:
    SException()
        : m_eCode(EC_OK),
          m_msg(nullptr)
    {
    }

    /**
     * @param eCode see skizo::core::EExceptionCode
     * @param msg see Message()
     * @param stackTrace Internal.
     */

    SException(EExceptionCode eCode, const char* msg = nullptr);

    /**
     * @see EExceptionCode
     */
    inline EExceptionCode Code() const
    {
        return m_eCode;
    }

    /**
     * A message which was provided when raising the exception.
     */
    inline const char* Message() const
    {
        return m_msg;
    }

    /**
     * Creates and returns a string representation of the current exception.
     */
    const CString* ToString() const;
};

} }

#endif // EXCEPTION_H_INCLUDED
