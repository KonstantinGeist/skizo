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

#ifndef CASTINFO_H_INCLUDED
#define CASTINFO_H_INCLUDED

namespace skizo { namespace script {

/**
 * There are several types of casts, divided into two major groups: implicit and explicit.
 *
 * @note If a cast type is denoted as "implicit conversion", that means STransformer::insertImplicitConversionIfAny(..)
 * inserts helper expressions which convert one value to another (usually, it's ctors or special static methods).
 */
enum ECastType
{
    E_CASTTYPE_UPCAST,                // same=>same, child=>parent
    E_CASTTYPE_DOWNCAST,              // parent=>child; checks in runtime if the target type is correct
    E_CASTTYPE_VALUE_TO_FAILABLE,     // value=>failable; inserts a ctor (implicit conversion)
    E_CASTTYPE_ERROR_TO_FAILABLE,     // error=>failable; inserts a ctor (implicit conversion)
    E_CASTTYPE_BOX,                   // byvalue=>byref(interface); should box the value
    E_CASTTYPE_UNBOX                  // byref(interface)=>byvalue; unboxes the value
};

/**
 * Result of STypeRef::GetCastInfo(..) or CClass::getCastInfo(..).
 * Specifies if two types are assignable/castable at all, and if they are, what cast type
 * it is.
 */
struct SCastInfo
{
    /**
     * What kind of cast is it? There are multiple types, such as UPCAST, DOWNCAST etc. (see ECastType)
     */
    ECastType CastType;

    /**
     * Is it castable at all?
     */
    bool IsCastable;

    SCastInfo()
        : CastType(E_CASTTYPE_UPCAST),
          IsCastable(false) 
    {
    }

    explicit SCastInfo(ECastType castType)
        : CastType(castType),
          IsCastable(true)
    {
    }

    /**
     * Some cast types require explicit casting only. For example, DOWNCAST must always be an explicit cast, as it
       * can abort if types don't match.
     */
    bool DoesRequireExplicitCast() const
    {
        return (CastType == E_CASTTYPE_DOWNCAST || CastType == E_CASTTYPE_UNBOX);
    }
};

} }

#endif // CASTINFO_H_INCLUDED
