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

#include "../RuntimeHelpers.h"
#include <math.h>

namespace skizo { namespace script {
using namespace std;

extern "C" {

float SKIZO_API _so_Math_sqrt(float f)
{
    return sqrtf(f);
}

int SKIZO_API _so_Math_abs(int i)
{
    return i < 0? -i: i;
}

float SKIZO_API _so_Math_fabs(float  v)
{
    return fabs(v);
}

float SKIZO_API _so_Math_sin(float f)
{
    return sinf(f);
}

float SKIZO_API _so_Math_cos(float f)
{
    return cosf(f);
}

float SKIZO_API _so_Math_acos(float f)
{
    return acosf(f);
}

float SKIZO_API _so_Math_fmod(float x, float y)
{
    return (float)fmod(x, y);
}

float SKIZO_API _so_Math_floor(float f)
{
    return floor(f);
}

float SKIZO_API _so_Math_min(float a, float b)
{
    return a < b? a: b;
}

float SKIZO_API _so_Math_max(float a, float b)
{
    return a > b? a: b;
}

}

} }
