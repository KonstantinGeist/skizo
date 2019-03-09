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

#ifndef NULLABLE_H_INCLUDED
#define NULLABLE_H_INCLUDED

#include "Exception.h"

namespace skizo { namespace core {

/**
 * Represents a value type that can be assigned null.
 */
template <typename T>
struct SNullable
{
public:
    SNullable(const T& v)
        : m_value(v), m_hasValue(true)
    {
    }

    SNullable() //-V730
        : m_hasValue(false)
    {
    }

    /**
     * Gets the value of the current SNullable<T> object if it has been assigned a
     * valid underlying value. Returns a copy of the value.
     *
     * @warning Throws if no value was assigned.
     */
    inline T Value() const
    {
        if(!m_hasValue) {
            SKIZO_THROW_WITH_MSG(EC_INVALID_STATE, "The nullable has no value.");
        }

        return m_value;
    }

    /**
     * Gets the value of the current SNullable<T> object if it has been assigned a
     * valid underlying value. Returns a reference to the value stored inside the
     * nullable instance.
     *
     * @warning Throws if no value was assigned.
     */
    inline const T& ValueRef() const
    {
        if(!m_hasValue) {
            SKIZO_THROW_WITH_MSG(EC_INVALID_STATE, "The nullable has no value.");
        }

        return m_value;
    }

    /**
     * Gets the value of the current SNullable<T> object if it has been assigned a
     * valid underlying value. Returns a pointer to the value stored inside the
     * nullable instance, or nullptr if no value was assigned.
     */
    inline T* ValuePtr()
    {
        return m_hasValue? &m_value: nullptr;
    }

    /**
     * Gets a value indicating whether the current SNullable<T> object has a valid
     * value.
     */
    inline bool HasValue() const
    {
        return m_hasValue;
    }

    /**
     * Sets a new value to the SNullable<T>.
     */
    inline void SetValue(const T& v)
    {
        m_value = v;
        m_hasValue = true;
    }

    /**
     * Removes the value from the SNullable<T> object. HasValue() reports false
     * afterwards.
     */
    inline void SetNull()
    {
        m_hasValue = false;
    }

private:
    T m_value;
    bool m_hasValue;
};

} }

#endif // NULLABLE_H_INCLUDED
