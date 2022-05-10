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

#ifndef ACCESSMODIFIER_H_INCLUDED
#define ACCESSMODIFIER_H_INCLUDED

namespace skizo { namespace script {

enum EAccessModifier
{
	/**
     * Accessible by the declaring class only.
     */
    E_ACCESSMODIFIER_PRIVATE = 0,

	/**
     * Accessible by subclasses and from extended code.
     */
    E_ACCESSMODIFIER_PROTECTED = 1,

	/**
     * Accessible by anyone in all modules.
     */
    E_ACCESSMODIFIER_PUBLIC = 2,

	/**
     * Accessibly by anyone in the same module.
     */
    E_ACCESSMODIFIER_INTERNAL = 3
};

} }

#endif // ACCESSMODIFIER_H_INCLUDED
