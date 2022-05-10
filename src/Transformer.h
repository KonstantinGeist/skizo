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

#ifndef TRANSFORMER_H_INCLUDED
#define TRANSFORMER_H_INCLUDED

namespace skizo { namespace script {
class CDomain;

// Transformation phase.
// * Infers, resolves and verifies types for all expressions.
// * Transforms anonymous methods into helper classes.
//
// The phase is done after the parser phase (after all classes have been parsed and loaded)
// but before the emitter phase.
void SkizoTransform(class CDomain* domain);

} }

#endif // TRANSFORMER_H_INCLUDED
