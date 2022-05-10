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

#ifndef EMITTER_H_INCLUDED
#define EMITTER_H_INCLUDED

#include "TextBuilder.h"

namespace skizo { namespace script {
class CDomain;

/**
 * Emits expressions after parsing and transforming.
 */
void SkizoEmit(CDomain* domain, STextBuilder& cb);

} }

#endif // EMITTER_H_INCLUDED
