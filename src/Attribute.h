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

#ifndef ATTRIBUTE_H_INCLUDED
#define ATTRIBUTE_H_INCLUDED

#include "StringSlice.h"

namespace skizo { namespace script {

/**
 * Describes Skizo attributes.
 * @note Everything -- names, integers and floats -- is stored as a string in the attribute.
 * Use SStringSlice::TryParseInt(..) et al. to extract the correct value.
 */
class CAttribute: public skizo::core::CObject
{
public:
    SStringSlice Name;
    SStringSlice Value;
};

} }

#endif // ATTRIBUTE_H_INCLUDED
