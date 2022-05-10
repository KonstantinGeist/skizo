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

#ifndef METHODFLAGS_H_INCLUDED
#define METHODFLAGS_H_INCLUDED

namespace skizo { namespace script {

typedef int EMethodFlags;
#define E_METHODFLAGS_NONE 0

/**
 * For closures.
 */
#define E_METHODFLAGS_IS_ANONYMOUS (1 << 0)

/**
 * NEVER USE DIRECTLY. Only unsafe contexts allow inline C code, access to Marshal, 'ref' and 'this' in dtors.
 */
#define E_METHODFLAGS_IS_UNSAFE (1 << 1)
#define E_METHODFLAGS_IS_ABSTRACT (1 << 2)

/**
 * A trick to convert virtual methods to non-virtual. After parsing the source code, the runtime knows
 * which virtual methods exactly were never overriden. We can call them without the vtable overhead.
 */
#define E_METHODFLAGS_IS_TRULY_VIRTUAL (1 << 3)

/**
 * Forces creation of a virtual call helper for this method.
 * VCH is created if a method is truly virtual and is a root of an hierarchy. However, this logic falls apart
 * if a base method is never overriden, yet it's called by some code that is never actually used (or because
 * "null" is passed), leaving us with relocation errors.
 */
#define E_METHODFLAGS_WAS_EVER_CALLED (1 << 4)

/**
 * Used to avoid calling CMethod::resolveECallAttributes(..) over and over again.
 */
#define E_METHODFLAGS_ECALL_ATTRIBUTES_RESOLVED (1 << 5)

/**
 * If one of nested methods refers to "self", then we must remember to put "self" to the closure environment
 * of the parent method.
 * Locals and params are marked captured in their CParam/CLocal objects; "self" as a local is an implied
 * one which has no corresponding CLocal, so we have to store information about it in a separate field of the
 * CMethod structure.
 * @note Obviously, meaningless for static methods.
 */
#define E_METHODFLAGS_IS_SELF_CAPTURED (1 << 6)

/**
 * Don't generate C headers for this method.
 */
#define E_METHODFLAGS_FORCE_NO_HEADER (1 << 7)

/**
 * Used by CMethod::shouldEmitReglocalsCode() and, ultimately, the emitter to see if we need to emit calls to
 * _soX_reglocals(..)/_soX_unreglocals() for this method. If there are no break expressions in this method, there's no need
 * to keep track of locals.
 */
#define E_METHODFLAGS_HAS_BREAK_EXPRS (1 << 8)

/**
 * Subclasses and parent classes can share methods. In that case, we need to find a way to remember that a method was
 * already inferred.
 */
#define E_METHODFLAGS_IS_INFERRED (1 << 9)

#define E_METHODFLAGS_IS_INLINABLE (1 << 10)

#define E_METHODFLAGS_COMPILER_GENERATED (1 << 11)

} }

#endif // METHODFLAGS_H_INCLUDED
