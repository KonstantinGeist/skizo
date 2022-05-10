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

#include "Random.h"
#include "Application.h"
#include "Contract.h"
#include "String.h"
#include <limits.h>

namespace skizo { namespace core {

#define MSEED 161803398
#define MBIG INT_MAX

CRandom::CRandom(int seed)
{
    int mj, mk;

    /* Set seedArray to nulls? */

    if(seed == 0) {
        seed = Application::TickCount();
    }

    if (seed == INT_MIN) {
        mj = MSEED - abs(INT_MIN + 1);
    } else {
        mj = MSEED - abs(seed);
    }

    m_seedArray[55] = mj;
    mk = 1;

    int i;
    for (i = 1; i < 55; i++) { /*  [1, 55] is special (Knuth) */
        const int ii = (21 * i) % 55;
        m_seedArray[ii] = mk;
        mk = mj - mk;
        if (mk < 0) {
            mk += MBIG;
        }
        mj = m_seedArray[ii];
    }

    int k;
    for (k = 1; k < 5; k++) {
        for (i = 1; i < 56; i++) {
            m_seedArray[i] -= m_seedArray[1 + (i + 30) % 55];
            if (m_seedArray[i] < 0) {
                m_seedArray[i] += MBIG;
            }
        }
    }

    m_inext = 0;
    m_inextp = 31;
}

int CRandom::NextInt(int min, int max)
{
    SKIZO_REQ(max > min, EC_ILLEGAL_ARGUMENT);

    /* special case: a difference of one (or less) will always return the minimum
       e.g. -1,-1 or -1,0 will always return -1 */
    unsigned int diff = (unsigned int) (max - min);
    if (diff <= 1) {
        return min;
    }

    return (int)((unsigned int)(NextDouble() * diff) + min);
}

int CRandom::NextInt()
{
    return (int)(NextDouble () * INT_MAX);
}

double CRandom::NextDouble()
{
    int retVal;

    if (++m_inext  >= 56) {
        m_inext  = 1;
    }
    if (++m_inextp >= 56) {
        m_inextp = 1;
    }

    retVal = m_seedArray[m_inext] - m_seedArray[m_inextp];

    if(retVal < 0) {
        retVal += MBIG;
    }

    m_seedArray[m_inext] = retVal;

    return retVal * (1.0 / MBIG);
}

const CString* CRandom::ToString() const
{
    return CString::FromASCII("<random>");
}

} }
