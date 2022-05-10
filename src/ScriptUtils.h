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

#ifndef SCRIPTUTILS_H_INCLUDED
#define SCRIPTUTILS_H_INCLUDED

#include "StringSlice.h"
#include "TypeRef.h"

namespace skizo { namespace core {
    struct CString;
} }

namespace skizo { namespace script {
    struct SArrayHeader;
    class CExpression;
    class CClass;
    class CConst;
    class CDomain;
    class CField;
    class CLocal;
    class CMethod;
    class CToken;
    struct SMetadataSource;
} }

namespace skizo { namespace script { namespace ScriptUtils {

// FailXXX functions are wrappers around CDomain::Abort(..) which are raised when parsing/compiling code
// (as opposed to runtime errors).

void Fail_(const char* msg, const skizo::core::CString* filePath, int lineNumber);
void Fail_(const char* msg, const skizo::script::SMetadataSource* source);
void FailT(const char* msg, const skizo::script::CToken* faultyToken);
void FailE(const char* msg, const skizo::script::CExpression* faultyExpr);
void FailC(const char* msg, const skizo::script::CClass* faultyPClass);
void FailF(const char* msg, const skizo::script::CField* faultyField);
void FailM(const char* msg, const skizo::script::CMethod* faultyMethod);
void FailCnst(const char* msg, const skizo::script::CConst* faultyKonst);
void FailL(const char* msg, const skizo::script::CLocal* faultyLocal);

void Warn_(const char* msg, const skizo::script::SMetadataSource* source);
void WarnE(const char* msg, const skizo::script::CExpression* faultyExpr);

/**
 * Takes an input string and converts escape characters to target values.
 * For use by CStringLiteralExpression and CCharLiteralExpression.
 */
const skizo::core::CString* EscapeString(const skizo::core::CString* input);

/**
 * Does the opposite of ::EscapeString
 */
const skizo::core::CString* UnescapeString(const skizo::core::CString* input);

/**
 * Generates a param name where it's missing...
 */
SStringSlice NParamName(const skizo::script::CDomain* pDomain, int index);

/**
 * Converts a raw GC-allocated array to a list of strings.
 */
skizo::collections::CArrayList<const skizo::core::CString*>* ArrayHeaderToStringArray(const SArrayHeader* soArray, bool allowNulls = true);

} } }

namespace skizo { namespace script {

/**
 * Returns a void slice if nothing found.
 */
SStringSlice PrimitiveOperatorToNeutralName(const SStringSlice& primOp, CDomain* domain);
SStringSlice NeutralNameToPrimitiveOperator(const SStringSlice& nn, CDomain* domain);

/**
 * Generic function which is a foundation for %primType%::equals
 */
bool BoxedEquals(void* ptrToValue, size_t valueSize, void* otherObj, EPrimType targetType);

} }

#endif // SCRIPTUTILS_H_INCLUDED
