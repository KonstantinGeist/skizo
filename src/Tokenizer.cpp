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

#include "Tokenizer.h"
#include "ArrayList.h"
#include "ScriptUtils.h"
#include "Domain.h"

namespace skizo { namespace script {
using namespace skizo::core;
using namespace skizo::collections;

// *****************
//   CToken.
// *****************

CToken::CToken(ETokenKind kind, const SStringSlice& stringSlice, const skizo::core::CString* filePath, int lineNumber)
    : Kind(kind),
      StringSlice(stringSlice),
      FilePath(filePath),
      LineNumber(lineNumber)
{
}

void* CToken::operator new(size_t size)
{
    return SO_FAST_ALLOC(size, E_SKIZOALLOCATIONTYPE_TOKEN);
}

void CToken::operator delete (void *p)
{
    // NOTHING
}

static bool isLiteralOrIdentifier(const CToken* token)
{
    return token->Kind == E_TOKENKIND_IDENTIFIER
        || token->Kind == E_TOKENKIND_STRING_LITERAL
        || token->Kind == E_TOKENKIND_INT_LITERAL
        || token->Kind == E_TOKENKIND_FLOAT_LITERAL;
}

int CToken::GetHashCode() const
{
    if(isLiteralOrIdentifier(this)) {
        int h = 0;
        const so_char16* cs = this->StringSlice.String->Chars();
        for(int i = this->StringSlice.Start; i < this->StringSlice.End; i++) {
            h += cs[i] * 17;
        }
        return h;
    } else {
        return (int)this->Kind;
    }
}

bool CToken::Equals(const CToken* token2) const
{
    if(this->Kind != token2->Kind) {
        return false;
    }

    if(isLiteralOrIdentifier(this)) {
        return this->StringSlice.Equals(token2->StringSlice);
    } else {
        return true;
    }
}

// **********
// Tokenizer.
// **********

namespace Tokenizer {

enum CharType {
    CHARTYPE_NONE,
    CHARTYPE_DIGIT,
    CHARTYPE_LETTER,
    CHARTYPE_PUNCTUATION,
    CHARTYPE_WHITESPACE
};

// *****************
// The table of ops.
// *****************

    #define OPS_SIZE 22
    static char ops[OPS_SIZE] = {
        '+',
        '{',
        '}',
        ':',
        ';',
        '(',
        ')',
        '=',
        '.',
        '-',
        '*',
        '/',
        '^',
        '?',
        '[',
        ']',
        '>',
        '<',
        '%',
        '|',
        '&'
    };
    static ETokenKind ops_kinds[OPS_SIZE] = {
        E_TOKENKIND_PLUS,
        E_TOKENKIND_LBRACE,
        E_TOKENKIND_RBRACE,
        E_TOKENKIND_COLON,
        E_TOKENKIND_SEMICOLON,
        E_TOKENKIND_LPARENTH,
        E_TOKENKIND_RPARENTH,
        E_TOKENKIND_ASSIGNMENT,
        E_TOKENKIND_DOT,
        E_TOKENKIND_MINUS,
        E_TOKENKIND_ASTERISK,
        E_TOKENKIND_DIV,
        E_TOKENKIND_METHOD,
        E_TOKENKIND_FAILABLE_SUFFIX,
        E_TOKENKIND_LBRACKET,
        E_TOKENKIND_RBRACKET,
        E_TOKENKIND_GREATER,
        E_TOKENKIND_LESS,
        E_TOKENKIND_MODULO,
        E_TOKENKIND_BIN_OR,
        E_TOKENKIND_BIN_AND
    };

// ***************************
// The table of composite ops.
// ***************************

    #define COMPOSITEOPS_SIZE 1
    static const char* compositeOps[COMPOSITEOPS_SIZE] = {
        "=="
    };
    static ETokenKind compositeOps_kinds[COMPOSITEOPS_SIZE] = {
        E_TOKENKIND_EQUALS
    };

// **********************
// The table of keywords.
// **********************

    #define KEYWORDS_SIZE 38
    static const char* keywords[KEYWORDS_SIZE] = {
        "this",
        "class",
        "struct",
        "field",
        "property",
        "method",
        "static",
        "auto",
        "true",
        "false",
        "return",
        "null",
        "ctor",
        "dtor",
        "private",
        "protected",
        "public",
        "internal",
        "unsafe",
        "abstract",
        "cast",
        "interface",
        "array",
        "enum",
        "abort",
        "assert",
        "native",
        "import",
        "is",
        "const",
        "ref",
        "alias",
        "break",
        "force",
        "event",
        "boxed",
        "sizeof",
        "extend"
    };
    static ETokenKind keywords_kinds[KEYWORDS_SIZE] = {
        E_TOKENKIND_THIS,
        E_TOKENKIND_CLASS,
        E_TOKENKIND_STRUCT,
        E_TOKENKIND_FIELD,
        E_TOKENKIND_PROPERTY,
        E_TOKENKIND_METHOD,
        E_TOKENKIND_STATIC,
        E_TOKENKIND_AUTO,
        E_TOKENKIND_TRUE,
        E_TOKENKIND_FALSE,
        E_TOKENKIND_RETURN,
        E_TOKENKIND_NULL,
        E_TOKENKIND_CTOR,
        E_TOKENKIND_DTOR,
        E_TOKENKIND_PRIVATE,
        E_TOKENKIND_PROTECTED,
        E_TOKENKIND_PUBLIC,
        E_TOKENKIND_INTERNAL,
        E_TOKENKIND_UNSAFE,
        E_TOKENKIND_ABSTRACT,
        E_TOKENKIND_CAST,
        E_TOKENKIND_INTERFACE,
        E_TOKENKIND_NEWARRAY,
        E_TOKENKIND_ENUM,
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
        E_TOKENKIND_BOXED,
        E_TOKENKIND_SIZEOF,
        E_TOKENKIND_EXTEND
    };

// ***************************

static ETokenKind compositeOpToTokenKind(so_char16 prevC, so_char16 c)
{
    for(int i = 0; i < COMPOSITEOPS_SIZE; i++) {
        const char* compositeOp = compositeOps[i];
        if(compositeOp[0] == (char)prevC && compositeOp[1] == (char)c) {
            return compositeOps_kinds[i];
        }
    }

    return E_TOKENKIND_NONE;
}

static ETokenKind opToTokenKind(so_char16 c)
{
    for(int i = 0; i < OPS_SIZE; i++) {
        if(c == (so_char16)ops[i]) {
            ETokenKind tokenKind = ops_kinds[i];
            return tokenKind;
        }
    }

    return E_TOKENKIND_NONE;
}

static ETokenKind keywordToTokenKind(const SStringSlice& slice)
{
    for(int i = 0; i < KEYWORDS_SIZE; i++) {
        if(slice.EqualsAscii(keywords[i])) {
            return keywords_kinds[i];
        }
    }

    return E_TOKENKIND_IDENTIFIER;
}

// **********
//   Token
// **********

static CToken* createToken(CDomain* domain, // for building errors and getting cached tokens
                           CharType charType, ETokenKind tokenKind, const CString* _string, int start, int end,
                           const CString* filePath, int lineNumber)
{
    SStringSlice stringSlice (_string, start, end);
    const so_char16* string = _string->Chars();

    // Checks for it being a string (the tokenkind of strings is not deduced in the previous stages).
    // A string is easily distinguished by quotes around it.
    if((end - start >= 2) && (string[start] == SKIZO_CHAR('"'))) {
        // Removes the quotes.
        stringSlice.Start++;
        stringSlice.End--;
        tokenKind = E_TOKENKIND_STRING_LITERAL;
    } else if((end - start >= 2) && (string[start] == SKIZO_CHAR('\''))) {
        // Checks for it being a char constant, just like with the string.
        stringSlice.Start++;
        stringSlice.End--;
        tokenKind = E_TOKENKIND_CHAR_LITERAL;

        // The correctness of the token will be tested in parser where we will call EscapeString and then make sure the final
		// string is 1 character long.
    } else if((end - start >= 2) && (string[start] == SKIZO_CHAR('@'))) {
        // Checks for it being a C code fragment, just like with the string.
        stringSlice.Start++;
        stringSlice.End--;
        tokenKind = E_TOKENKIND_CCODE;
    } else if(tokenKind == E_TOKENKIND_NONE) {
        // Automatically deduces the token type.
        switch(charType) {
            case CHARTYPE_DIGIT:
                tokenKind = E_TOKENKIND_INT_LITERAL;
                break;

            case CHARTYPE_LETTER:
                tokenKind = keywordToTokenKind(stringSlice);
                break;

            case CHARTYPE_PUNCTUATION:
            {
                int tokenLength = end - start;

                if(tokenLength == 3) {
                    // 3-letter operators are rare, so we don't use neither "compositeOpToTokenKind"
                    // nor "opToTokenKind" here, and compare here directly.
                    if((string[start] == '=') && (string[start + 1] == '=') && (string[start + 2] == '=')) {
                        tokenKind = E_TOKENKIND_IDENTITYCOMPARISON;
                    } else {
                        ScriptUtils::Fail_(domain->FormatMessage("Operator '%s' not supported", &stringSlice), filePath, lineNumber);
                        return nullptr;
                    }
                } else if(tokenLength == 2) {
                    tokenKind = compositeOpToTokenKind(string[start], string[start + 1]);
                    if(tokenKind == E_TOKENKIND_NONE) {
                        ScriptUtils::Fail_(domain->FormatMessage("Composite operator '%s' not supported.", &stringSlice), filePath, lineNumber);
                        return nullptr;
                    }
                } else if(tokenLength == 1) {
                    tokenKind = opToTokenKind(string[start]);
                    if(tokenKind == E_TOKENKIND_NONE) {
                        ScriptUtils::Fail_(domain->FormatMessage("Operator '%s' not supported", &stringSlice), filePath, lineNumber);
                        return nullptr;
                    }
                } else {
                    ScriptUtils::Fail_(domain->FormatMessage("Operator '%s' not supported", &stringSlice), filePath, lineNumber);
                    return nullptr;
                }
            }
            break;

            default:
            {
                ScriptUtils::Fail_(domain->FormatMessage("Character type of %s not supported.",&stringSlice), filePath, lineNumber);
                return nullptr;
            }
        }
    }

    return new CToken(tokenKind, stringSlice, filePath, lineNumber);
}

// **************
//   GetTokens
// **************

static bool isDigit(char c)
{
    return c >= '0' && c <= '9';
}

static bool isWhitespace(char c)
{
    return (c == ' ') || (c == '\t') || (c == '\n' || (c == '\r'));
}

static bool isLetter(char c)
{
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c == '_');
}

static bool isPunctuation(char c)
{
    // NOTE this code exludes '_' which follows after '^'
    return (c >= '!' && c <= '/') || (c >= ':' && c <= '@') || (c >= '[' && c <= '^') || (c >= '{' && c <= '~');
}

static CharType getCharType(so_char16 c)
{
    if(isDigit(c)) {
        return CHARTYPE_DIGIT;
    } else if(isLetter(c)) {
        return CHARTYPE_LETTER;
    } else if(isPunctuation(c)) {
        return CHARTYPE_PUNCTUATION;
    } else if(isWhitespace(c)) {
        return CHARTYPE_WHITESPACE;
    }

    // TODO?
    ScriptUtils::Fail_("Char not supported in the token stream.", nullptr, 0);
    return CHARTYPE_NONE;
}

bool IsKeyword(const SStringSlice& ident)
{
    for(int i = 0; i < KEYWORDS_SIZE; i++) {
        if(ident.EqualsAscii(keywords[i])) {
            return true;
        }
    }

    return false;
}

bool IsOperator(const SStringSlice& ident)
{
    const int length = ident.End - ident.Start;
    if(length == 1) {
        return opToTokenKind(ident.String->Chars()[ident.Start]) != E_TOKENKIND_NONE;
    } else if(length == 2) {
        return compositeOpToTokenKind(ident.String->Chars()[ident.Start],
                                      ident.String->Chars()[ident.Start + 1]) != E_TOKENKIND_NONE;
    } else {
        // There are no operators with more than 2 characters.
        return false;
    }
}

bool IsKeyword(ETokenKind tokenKind)
{
    for(int i = 0; i < KEYWORDS_SIZE; i++) {
        if(keywords_kinds[i] == tokenKind) {
            return true;
        }
    }

    return false;
}

bool IsOperator(ETokenKind tokenKind)
{
    for(int i = 0; i < OPS_SIZE; i++) {
        if(ops_kinds[i] == tokenKind) {
            return true;
        }
    }
    for(int i = 0; i < COMPOSITEOPS_SIZE; i++) {
        if(compositeOps_kinds[i] == tokenKind) {
            return true;
        }
    }

    return false;
}

const char* NameForTokenKind(ETokenKind kind)
{
    switch(kind) {
        case E_TOKENKIND_NONE: SKIZO_REQ_NEVER; break;
        case E_TOKENKIND_IDENTIFIER: return "identifier";
        case E_TOKENKIND_INT_LITERAL: return "int literal";
        case E_TOKENKIND_FLOAT_LITERAL: return "float literal";
        case E_TOKENKIND_STRING_LITERAL: return "string literal";
        case E_TOKENKIND_CHAR_LITERAL: return "char literal";
        case E_TOKENKIND_DOT: return ".";
        case E_TOKENKIND_PLUS: return "+";
        case E_TOKENKIND_EQUALS: return "==";
        case E_TOKENKIND_LBRACE: return "{";
        case E_TOKENKIND_RBRACE: return "}";
        case E_TOKENKIND_COLON: return ":";
        case E_TOKENKIND_SEMICOLON: return ";";
        case E_TOKENKIND_LPARENTH: return "(";
        case E_TOKENKIND_RPARENTH: return ")";
        case E_TOKENKIND_CLASS: return "class";
        case E_TOKENKIND_STRUCT: return "struct";
        case E_TOKENKIND_EXTEND: return "extend";
        case E_TOKENKIND_FIELD: return "field";
        case E_TOKENKIND_METHOD: return "method";
        case E_TOKENKIND_CTOR: return "ctor";
        case E_TOKENKIND_DTOR: return "dtor";
        case E_TOKENKIND_ASSIGNMENT: return "=";
        case E_TOKENKIND_MINUS: return "-";
        case E_TOKENKIND_ASTERISK: return "*";
        case E_TOKENKIND_DIV: return "/";
        case E_TOKENKIND_PRIVATE: return "private";
        case E_TOKENKIND_PROTECTED: return "protected";
        case E_TOKENKIND_PUBLIC: return "public";
        case E_TOKENKIND_STATIC: return "static";
        case E_TOKENKIND_RETURN: return "return";
        case E_TOKENKIND_THIS: return "this";
        case E_TOKENKIND_CCODE: return "@";
        case E_TOKENKIND_UNSAFE: return "unsafe";
        case E_TOKENKIND_ABSTRACT: return "abstract";
        case E_TOKENKIND_NULL: return "null";
        case E_TOKENKIND_CAST: return "cast";
        case E_TOKENKIND_INTERFACE: return "interface";
        case E_TOKENKIND_TRUE: return "true";
        case E_TOKENKIND_FALSE: return "false";
        case E_TOKENKIND_FAILABLE_SUFFIX: return "?";
        case E_TOKENKIND_LBRACKET: return "[";
        case E_TOKENKIND_RBRACKET: return "]";
        case E_TOKENKIND_NEWARRAY: return "array";
        case E_TOKENKIND_GREATER: return ">";
        case E_TOKENKIND_LESS: return "<";
        case E_TOKENKIND_AUTO: return "auto";
        case E_TOKENKIND_ENUM: return "enum";
        case E_TOKENKIND_MODULO: return "%";
        case E_TOKENKIND_IDENTITYCOMPARISON: return "===";
        case E_TOKENKIND_ABORT: return "abort";
        case E_TOKENKIND_ASSERT: return "assert";
        case E_TOKENKIND_NATIVE: return "native";
        case E_TOKENKIND_IMPORT: return "import";
        case E_TOKENKIND_IS: return "is";
        case E_TOKENKIND_CONST: return "const";
        case E_TOKENKIND_REF: return "ref";
        case E_TOKENKIND_ALIAS: return "alias";
        case E_TOKENKIND_BREAK: return "break";
        case E_TOKENKIND_FORCE: return "force";
        case E_TOKENKIND_EVENT: return "event";
		case E_TOKENKIND_PROPERTY: return "property";
		case E_TOKENKIND_BOXED: return "boxed";
		case E_TOKENKIND_SIZEOF: return "sizeof";
		case E_TOKENKIND_BIN_OR: return "|";
		case E_TOKENKIND_BIN_AND: return "&";
        default: SKIZO_REQ_NEVER; break;
    }
    
    return nullptr;
}

CArrayList<CToken*>* Tokenize(CDomain* domain, const CString* filePath, const CString* code)
{
    int lineNumber = 1;
    const int strLength = code->Length();
    const so_char16* str = code->Chars();

    CharType prevType = CHARTYPE_NONE;
    Auto<CArrayList<CToken*> > r (new CArrayList<CToken*>(512));
    ETokenKind compositeToken = E_TOKENKIND_NONE,
               prevCompositeToken = E_TOKENKIND_NONE;
    bool doubleQuote = false;
    bool singleQuote = false;
    bool comment = false;
    bool ccodeOn = false;

    int prevCharIndex = 0;
    so_char16 prevC = 0;
    for(int i = 0; i < (strLength + 1); i++) {
        const so_char16 c = (i < strLength)? str[i]: SKIZO_CHAR(' ');

        if(c == SKIZO_CHAR('\n')) {
            lineNumber++;
        }

        // ***************************************
        // A C code block allows arbitrary C code.
        // ***************************************

        if(ccodeOn) {
            if(c == SKIZO_CHAR('@')) {
                ccodeOn = false;
            }
            continue;
        }

        // **************************************************************************************************
        // A string allows any kind of characters. Because of this, strings are managed separately from other
        // composite tokens, to avoid calling getCharType(..) on a non-supported character.
        // **************************************************************************************************

        if(doubleQuote) {
            if(c == SKIZO_CHAR('"')) {
                doubleQuote = false;
            }
            continue;
        }

        // *********************************
        // Same as above for char constants.
        // *********************************

        if(singleQuote) {
            if(c == SKIZO_CHAR('\'')) {
                singleQuote = false;
            }
            continue;
        }

        // ***************************
        // Same as above for comments.
        // ***************************

        if(comment) {
            // Manual comparison to the RCOMMENT
            if(c == SKIZO_CHAR('*') && (i + 1 < strLength) && str[i + 1] == SKIZO_CHAR('/')) {
                comment = false;
                i++;
            }
            continue;
        } else // Manual comparison to the LCOMMENT
        if(c == SKIZO_CHAR('/') && (i + 1 < strLength) && str[i + 1] == SKIZO_CHAR('*')) {
            comment = true;
            i++;
            continue;
        }

        // **********************

        if(c > 255) {
            ScriptUtils::Fail_(domain->FormatMessage("Non-ASCII is supported only for string literals and comments (char with index %d encountered).", (int)c),
                               filePath,
                               lineNumber);
        }

        CharType curType;

        // Special case for unary minus. We'd use a more Skizo syntax, but people are used to prefixed unary notation.
        if(c == SKIZO_CHAR('-') && (i < strLength + 1) && isDigit(str[i + 1])) {
            curType = CHARTYPE_DIGIT;
        } else {
            // General char type extraction.
            curType = getCharType(c);
        }

        // *********************************************************************
        // This section matches composite tokens. If a composite token is found,
        // the token stream is expected to end with tokens of required types.
        // *********************************************************************

        prevCompositeToken = compositeToken;

        if(prevType == CHARTYPE_DIGIT && c == SKIZO_CHAR('.')) {
            compositeToken = E_TOKENKIND_FLOAT_LITERAL;
        } else if(prevType == CHARTYPE_LETTER && curType == CHARTYPE_DIGIT) {
            compositeToken = E_TOKENKIND_IDENTIFIER;
        } else {
            // Checks if required tokens are in place + whether the composite
            // token ends here.
            switch(compositeToken) {
                case E_TOKENKIND_IDENTIFIER:
                    // An identifier can alternate between LETTER (including '_') && DIGIT multiple times until
                    // a whitespace or a punctuation is encountered.
                    if(curType == CHARTYPE_PUNCTUATION || curType == CHARTYPE_WHITESPACE) {
                        compositeToken = E_TOKENKIND_NONE;
                    }
                    break;

                case E_TOKENKIND_FLOAT_LITERAL:
                    // The float literal can have multiple digits after '.' until we encounter a punctuation or a space.
                    // Anything else is forbidden.
                    if(curType == CHARTYPE_PUNCTUATION || curType == CHARTYPE_WHITESPACE) {
                        compositeToken = E_TOKENKIND_NONE;
                    } else if(curType != CHARTYPE_DIGIT) {
                        ScriptUtils::Fail_("A whitespace or punctuation expected (float literal).", filePath, lineNumber);
                    }
                    break;
                default: break; // Silences the compiler.
            }
        }

        // **********************************************************************
        // Greenlight: if true, then the token ends here. Otherwise wait for more
        // characters.
        // **********************************************************************

        bool greenLight = false;
        if(compositeToken == E_TOKENKIND_NONE) {
            if(prevType != curType) {
                greenLight = true;
            }
            if(prevType == CHARTYPE_PUNCTUATION) {
                if(compositeOpToTokenKind(prevC, c) == E_TOKENKIND_NONE) {
                    greenLight = true;
                }
            }
        }

        // **********************************
        // Adds the token + some bookkeeping.
        // **********************************

        if(greenLight) {
            // Whitespaces are discarded immediately.
            // NOTE The token to be added here is the previous token. The current character does no belong to this
            // token!
            if(prevType != CHARTYPE_WHITESPACE && prevType != CHARTYPE_NONE) {
                Auto<CToken> token (createToken(domain,
                                                     prevType,
                                                     prevCompositeToken,
                                                     code,
                                                     prevCharIndex,
                                                     i,
                                                     filePath,
                                                     lineNumber));
                r->Add(token);
            }

            // **********************************
            if(!doubleQuote && !singleQuote && (c == SKIZO_CHAR('@'))) {
                ccodeOn = !ccodeOn;
            } else if(c == SKIZO_CHAR('"')) {
                doubleQuote = !doubleQuote;
            } else if(c == SKIZO_CHAR('\'')) {
                singleQuote = !singleQuote;
            }
            // **********************************

            prevCharIndex = i;
        }

        prevType = curType;
        prevC = c;
    }

    if(doubleQuote) {
        ScriptUtils::Fail_("Unexpected end of the string token.", filePath, lineNumber);
        return nullptr;
    }
    if(singleQuote) {
        ScriptUtils::Fail_("Unexpected end of the char constant token.", filePath, lineNumber);
        return nullptr;
    }
    if(ccodeOn) {
        ScriptUtils::Fail_("Unexpected end of the C code.", filePath, lineNumber);
        return nullptr;
    }

    r->Ref();
    return r;
}

}

} }
