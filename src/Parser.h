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

#ifndef PARSER_H_INCLUDED
#define PARSER_H_INCLUDED

namespace skizo { namespace core {
    class CString;
} }

namespace skizo { namespace script {
class CDomain;

/**
 * @note Parse(..) is called separately for every "import".
 */
void SkizoParse(skizo::script::CDomain* domain,
                const skizo::core::CString* filePath,
                const skizo::core::CString* code,
                bool isBaseModule);

} }

#endif // PARSER_H_INCLUDED
