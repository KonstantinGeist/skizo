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

#ifndef SOURCEKIND_H_INCLUDED
#define SOURCEKIND_H_INCLUDED

namespace skizo { namespace script {

/**
 * @warning Keep in sync with the definitions in the corresponding .skizo file.
 */
enum ESourceKind
{
    /**
     * The given string is treated as the code itself.
     */
    E_SOURCEKIND_STRINGSOURCE = 0,

    /**
     * The given string is treated as a path to the code.
     */
    E_SOURCEKIND_PATH = 1,

    /**
     * The given string is treated as a name of the method that contains required code.
     */
    E_SOURCEKIND_METHODNAME = 2
};

} }

#endif // SOURCEKIND_H_INCLUDED
