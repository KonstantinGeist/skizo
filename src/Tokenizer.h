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

#ifndef TOKENIZER_H_INCLUDED
#define TOKENIZER_H_INCLUDED

#include "StringSlice.h"

namespace skizo { namespace core {
    class CString;
} }

namespace skizo { namespace script {
class CDomain;

/**
 * @warning When adding new kinds of tokens, don't forget to update:
 *    * NameForTokenKind(..)
 *    * keywords[KEYWORDS_SIZE]
 * in Tokenizer.cpp
 */
enum ETokenKind
{
    E_TOKENKIND_NONE,
    E_TOKENKIND_IDENTIFIER,
    E_TOKENKIND_INT_LITERAL,
    E_TOKENKIND_FLOAT_LITERAL,
    E_TOKENKIND_STRING_LITERAL,
    E_TOKENKIND_CHAR_LITERAL,
    E_TOKENKIND_DOT,
    E_TOKENKIND_PLUS,
    E_TOKENKIND_EQUALS,
    E_TOKENKIND_LBRACE,
    E_TOKENKIND_RBRACE,
    E_TOKENKIND_COLON,
    E_TOKENKIND_SEMICOLON,
    E_TOKENKIND_LPARENTH,
    E_TOKENKIND_RPARENTH,
    E_TOKENKIND_CLASS,
    E_TOKENKIND_STRUCT,
    E_TOKENKIND_EXTEND,
    E_TOKENKIND_FIELD,
    E_TOKENKIND_METHOD,
    E_TOKENKIND_CTOR,
    E_TOKENKIND_DTOR,
    E_TOKENKIND_ASSIGNMENT,
    E_TOKENKIND_MINUS,
    E_TOKENKIND_ASTERISK, // named ASTERISK instead of MUL because we reuse it in typerefs
    E_TOKENKIND_DIV,
    E_TOKENKIND_PRIVATE,
    E_TOKENKIND_PROTECTED,
    E_TOKENKIND_PUBLIC,
    E_TOKENKIND_INTERNAL,
    E_TOKENKIND_STATIC,
    E_TOKENKIND_RETURN,
    E_TOKENKIND_THIS,
    E_TOKENKIND_CCODE,
    E_TOKENKIND_UNSAFE,
    E_TOKENKIND_ABSTRACT,
    E_TOKENKIND_NULL,
    E_TOKENKIND_CAST,
    E_TOKENKIND_INTERFACE,
    E_TOKENKIND_TRUE,
    E_TOKENKIND_FALSE,
    E_TOKENKIND_FAILABLE_SUFFIX,
    E_TOKENKIND_LBRACKET,
    E_TOKENKIND_RBRACKET,
    E_TOKENKIND_NEWARRAY,
    E_TOKENKIND_GREATER,
    E_TOKENKIND_LESS,
    E_TOKENKIND_AUTO,
    E_TOKENKIND_ENUM,
    E_TOKENKIND_MODULO,
    E_TOKENKIND_IDENTITYCOMPARISON,
    E_TOKENKIND_ABORT,
    E_TOKENKIND_ASSERT,
    E_TOKENKIND_NATIVE,
    E_TOKENKIND_IMPORT,
    E_TOKENKIND_IS,
    E_TOKENKIND_CONST,
    E_TOKENKIND_REF,
    E_TOKENKIND_ALIAS,
    E_TOKENKIND_BREAK,
    E_TOKENKIND_FORCE,
    E_TOKENKIND_EVENT,
    E_TOKENKIND_PROPERTY,
    E_TOKENKIND_BOXED,
    E_TOKENKIND_SIZEOF,

    E_TOKENKIND_BIN_OR,
    E_TOKENKIND_BIN_AND,

    // !
    E_TOKENKIND_COUNT_DONT_USE
};

// ****************
//   SkizoToken
// ****************

class CToken: public skizo::core::CObject
{
public:
    // NOTE Be careful, assumes that CDomain was already created for the current thread.
    void* operator new(size_t size);
    void operator delete (void *p);

    ETokenKind Kind;
    SStringSlice StringSlice;

    const skizo::core::CString* FilePath;
    int LineNumber;

    CToken(ETokenKind kind,
           const SStringSlice& stringSlice,
           const skizo::core::CString* filePath,
           int lineNumber);

    virtual int GetHashCode() const override;
    bool Equals(const CToken* token) const;
};

class CDomain;

namespace Tokenizer {

    skizo::collections::CArrayList<CToken*>* Tokenize(skizo::script::CDomain* domain,
                                                      const skizo::core::CString* filePath, // tokens want filePath info attached for nicer errors
                                                      const skizo::core::CString* code);
    bool IsKeyword(const SStringSlice& ident);
    bool IsOperator(const SStringSlice& ident);
    bool IsKeyword(ETokenKind tokenKind);
    bool IsOperator(ETokenKind tokenKind);

    // For error reporting.
    const char* NameForTokenKind(ETokenKind kind);
}

} }

#endif // TOKENIZER_H_INCLUDED
