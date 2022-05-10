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

#ifndef RANDOM_H_INCLUDED
#define RANDOM_H_INCLUDED

#include "Object.h"

namespace skizo { namespace core {

/**
 * Represents a pseudo-random number generator, a device that produces a sequence
 * of numbers that meet certain statistical requirements for randomness.
 */
class CRandom final: public CObject
{
public:
    /**
     * Creates a new random number generator using a seed.
     *
     * @param seed A number used to calculate a starting value for the pseudo-random
     *             number sequence. If a negative number is specified, the absolute
     *             value of the number is used.
     */
    explicit CRandom(int seed = 0);

    /**
     * Returns a random number within a specified range.
     *
     * @param min The inclusive lower bound of the random number returned.
     * @param max The exclusive upper bound of the random number returned.
     * maxValue must be greater than or equal to minValue.
     * @return A 32-bit integer greater than or equal to minValue and less than
     * maxValue; that is, the range of return values includes minValue but not
     * maxValue. If minValue equals maxValue, minValue is returned.
     */
    int NextInt(int min, int max);

    /**
     * Returns a nonnegative random number.
     *
     * @noteremark Returns the next pseudorandom, uniformly distributed int value
     * from this random number generator's sequence. All 2^32 possible int values
     * are produced with (approximately) equal probability.
     * @return An integer greater than or equal to zero and less than INT_MAX.
     */
    int NextInt();

    /**
     * Returns the next pseudorandom, uniformly distributed double value between
     * 0.0 and 1.0 from this random number generator's sequence.
     */
    double NextDouble();

    virtual const CString* ToString() const override;

private:
    int m_seedArray[56];
    int m_inext, m_inextp;
};

} }

#endif // RANDOM_H_INCLUDED
