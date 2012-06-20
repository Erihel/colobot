// * This file is part of the COLOBOT source code
// * Copyright (C) 2001-2008, Daniel ROUX & EPSITEC SA, www.epsitec.ch
// * Copyright (C) 2012, Polish Portal of Colobot (PPC)
// *
// * This program is free software: you can redistribute it and/or modify
// * it under the terms of the GNU General Public License as published by
// * the Free Software Foundation, either version 3 of the License, or
// * (at your option) any later version.
// *
// * This program is distributed in the hope that it will be useful,
// * but WITHOUT ANY WARRANTY; without even the implied warranty of
// * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// * GNU General Public License for more details.
// *
// * You should have received a copy of the GNU General Public License
// * along with this program. If not, see  http://www.gnu.org/licenses/.

/** @defgroup MathFuncModule math/func.h
   Contains common math functions.
 */

#pragma once

#include "const.h"

#include <cmath>
#include <cstdlib>


// Math module namespace
namespace Math
{

/* @{ */ // start of group

//! Compares \a a and \a b within \a tolerance
inline bool IsEqual(float a, float b, float tolerance = TOLERANCE)
{
  return fabs(a - b) < tolerance;
}

//! Compares \a a to zero within \a tolerance
inline bool IsZero(float a, float tolerance = TOLERANCE)
{
  return IsEqual(a, 0.0f, tolerance);
}

//! Minimum
inline float Min(float a, float b)
{
  if ( a <= b ) return a;
  else          return b;
}

inline float Min(float a, float b, float c)
{
  return Min( Min(a, b), c );
}

inline float Min(float a, float b, float c, float d)
{
  return Min( Min(a, b), Min(c, d) );
}

inline float Min(float a, float b, float c, float d, float e)
{
  return Min( Min(a, b), Min(c, d), e );
}

//! Maximum
inline float Max(float a, float b)
{
  if ( a >= b ) return a;
  else          return b;
}

inline float Max(float a, float b, float c)
{
  return Max( Max(a, b), c );
}

inline float Max(float a, float b, float c, float d)
{
  return Max( Max(a, b), Max(c, d) );
}

inline float Max(float a, float b, float c, float d, float e)
{
  return Max( Max(a, b), Max(c, d), e );
}

//! Returns the normalized value (0 .. 1)
inline float Norm(float a)
{
  if ( a < 0.0f ) return 0.0f;
  if ( a > 1.0f ) return 1.0f;
  return a;
}

//! Swaps two integers
inline void Swap(int &a, int &b)
{
  int c = a;
  a = b;
  b = c;
}

//! Swaps two real numbers
inline void Swap(float &a, float &b)
{
  float c = a;
  a = b;
  b = c;
}

//! Returns the modulo of a floating point number
/** Mod(8.1, 4) = 0.1
    Mod(n, 1) = fractional part of n */
inline float Mod(float a, float m)
{
  return a - ((int)(a/m))*m;
}

//! Returns a random value between 0 and 1.
inline float Rand()
{
  return (float)rand()/RAND_MAX;
}

//! Returns a normalized angle, that is in other words between 0 and 2 * PI
inline float NormAngle(float angle)
{
  angle = Mod(angle, PI*2.0f);
  if ( angle < 0.0f )
    return PI*2.0f + angle;

  return angle;
}

//! Test if a angle is between two terminals
inline bool TestAngle(float angle, float min, float max)
{
  angle = NormAngle(angle);
  min   = NormAngle(min);
  max   = NormAngle(max);

  if ( min > max )
    return ( angle <= max || angle >= min );

  return ( angle >= min && angle <= max );
}

//! Calculates a value (radians) proportional between a and b (degrees)
inline float PropAngle(int a, int b, float p)
{
  float aa = (float)a * DEG_TO_RAD;
  float bb = (float)b * DEG_TO_RAD;

  return aa+p*(bb-aa);
}

//! Calculates the angle to rotate the angle \a a to the angle \a g
/** A positive angle is counterclockwise (CCW). */
inline float Direction(float a, float g)
{
  a = NormAngle(a);
  g = NormAngle(g);

  if ( a < g )
  {
    if ( a+PI*2.0f-g < g-a )  a += PI*2.0f;
  }
  else
  {
    if ( g+PI*2.0f-a < a-g )  g += PI*2.0f;
  }

  return g-a;
}

//! Managing the dead zone of a joystick.
/**
\verbatimin:   -1            0            1
--|-------|----o----|-------|-->
             <---->
              dead
out:  -1       0         0       1\endverbatim */
inline float Neutral(float value, float dead)
{
  if ( fabs(value) <= dead )
  {
    return 0.0f;
  }
  else
  {
    if ( value > 0.0f )  return (value-dead)/(1.0f-dead);
    else                 return (value+dead)/(1.0f-dead);
  }
}

//! Gently advances a desired value from its current value
/** Over time, the progression is more rapid. */
inline float Smooth(float actual, float hope, float time)
{
  float future = actual + (hope-actual)*time;

  if ( hope > actual )
  {
    if ( future > hope )  future = hope;
  }
  if ( hope < actual )
  {
    if ( future < hope )  future = hope;
  }

  return future;
}

//! Bounces any movement
/**
\verbatimout
 |
1+------o-------o---
 |    o | o   o |  | bounce
 |   o  |   o---|---
 |  o   |       |
 | o    |       |
-o------|-------+----> progress
0|      |       1
 |<---->|middle\endverbatim */
inline float Bounce(float progress, float middle = 0.3f, float bounce = 0.4f)
{
  if ( progress < middle )
  {
    progress = progress/middle;  // 0..1
    return 0.5f+sinf(progress*PI-PI/2.0f)/2.0f;
  }
  else
  {
    progress = (progress-middle)/(1.0f-middle);  // 0..1
    return (1.0f-bounce/2.0f)+sinf((0.5f+progress*2.0f)*PI)*(bounce/2.0f);
  }
}

/* @} */ // end of group

}; // namespace Math