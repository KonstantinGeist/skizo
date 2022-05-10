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

#ifndef CONTRACT_H_INCLUDED
#define CONTRACT_H_INCLUDED

/*
 * Contract macros to verify arguments.
 *
 * The good thing about these contract macros is that they can be defined to
 * no-op for release mode. They always are in debug mode (if SKIZO_DEBUG_MODE is on).
 * You can enable contracts for release mode by enabling the SKIZO_CONTRACT define.
 */

#include "Exception.h"

// Better leave contract everywhere including releases, for better remote
// diagnostics & safer code.
#define SKIZO_CONTRACT

#ifdef SKIZO_CONTRACT

    // ****************************************************************************************
    //   These can be redefined to either throw exceptions (to try fix an error dynamically),
    //   use asserts if we're more pessimistic, or to no-op in release.
    // ****************************************************************************************

    // The contracts' implementations tell for themselves.

    #define SKIZO_REQ(x, ec) if(!(x)) { SKIZO_THROW(ec); }
    #define SKIZO_REQ_NOT(x, ec) if(x) { SKIZO_THROW(ec); }
    #define SKIZO_REQ_WITH_MSG(x, ec, msg) if(!(x)) { SKIZO_THROW_WITH_MSG(ec, msg); }
    #define SKIZO_REQ_NOT_NEG(x) SKIZO_REQ_WITH_MSG(x >= 0, skizo::core::EC_ILLEGAL_ARGUMENT, "Input cannot be negative.")
    #define SKIZO_REQ_POS(x) SKIZO_REQ_WITH_MSG(x > 0, skizo::core::EC_ILLEGAL_ARGUMENT, "Input must be positive.")
    #define SKIZO_REQ_EQUALS(x, y) if((x) != (y)) { SKIZO_THROW_WITH_MSG(skizo::core::EC_CONTRACT_UNSATISFIED, "Equality condition unsatisfied."); }
    #define SKIZO_REQ_NOT_EQUALS(x, y) if((x) == (y)) { SKIZO_THROW_WITH_MSG(skizo::core::EC_CONTRACT_UNSATISFIED, "Inequality condition unsatisfied."); }
    #define SKIZO_REQ_PTR(x) SKIZO_REQ_WITH_MSG(x, skizo::core::EC_ILLEGAL_ARGUMENT, "Input cannot be null pointer.")

#else

    // **********
    //   No-op.
    // **********

    #define SKIZO_REQ(x, ec)
    #define SKIZO_REQ_NOT(x, ec)
    #define SKIZO_REQ_WITH_MSG(x, ec, msg)
    #define SKIZO_REQ_NOT_NEG(x)
    #define SKIZO_REQ_POS(x)
    #define SKIZO_REQ_EQUALS(x, y)
    #define SKIZO_REQ_NOT_EQUALS(x, y)
    #define SKIZO_REQ_PTR(x)

#endif

// Suffix _D ('D" for "Dynamic") means that it should always be a dynamic
// exception, even if the compilation mode for contracts is "no-op" (unless dynamic
// checks are forced to be no-op as well). Important for collections.
#define SKIZO_REQ_RANGE_D(value, minIncl, maxExcl) \
                SKIZO_REQ((value) >= (minIncl) && (value) < (maxExcl), skizo::core::EC_OUT_OF_RANGE)

#define SKIZO_REQ_NEVER SKIZO_THROW_WITH_MSG(skizo::core::EC_CONTRACT_UNSATISFIED, "Should never be reached.");

#endif // CONTRACT_H_INCLUDED
