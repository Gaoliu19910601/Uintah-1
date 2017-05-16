/*
 * The MIT License
 *
 * Copyright (c) 1997-2017 The University of Utah
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */


/*
 *  FloatPoint.cc: ?
 *
 *  Written by:
 *   Author ?
 *   Department of Computer Science
 *   University of Utah
 *   Date ?
 *
 */

#include <Core/Geometry/FloatPoint.h>
#include <Core/Geometry/FloatVector.h>
#include <Core/Util/Assert.h>
#include <Core/Math/MinMax.h>
#include <Core/Math/MiscMath.h>
#include <iostream>
#include <cstdio>

using namespace std;

namespace Uintah {

int FloatPoint::operator==(const FloatPoint& p) const
{
    return p.x_ == x_ && p.y_ == y_ && p.z_ == z_;
}

int FloatPoint::operator!=(const FloatPoint& p) const
{
    return p.x_ != x_ || p.y_ != y_ || p.z_ != z_;
}

FloatPoint::FloatPoint(float x, float y, float z, float w)
{
    if(w==0){
	cerr << "degenerate point!" << endl;
	x_=y_=z_=0;
    } else {
	x_=x/w;
	y_=y/w;
	z_=z/w;
    }
}

ostream& operator<<( ostream& os, const FloatPoint& p )
{
  os << '[' << p.x() << ' ' << p.y() << ' ' << p.z() << ']';
  return os;
}

istream& operator>>( istream& is, FloatPoint& v)
{
    float x, y, z;
    char st;
  is >> st >> x >> st >> y >> st >> z >> st;
  v=FloatPoint(x,y,z);
  return is;
}

int
FloatPoint::Overlap( float a, float b, float e )
{
  float hi, lo, h, l;
  
  hi = a + e;
  lo = a - e;
  h  = b + e;
  l  = b - e;

  if ( ( hi > l ) && ( lo < h ) )
    return 1;
  else
    return 0;
}
  
int
FloatPoint::InInterval( FloatPoint a, float epsilon )
{
  if ( Overlap( x_, a.x(), epsilon ) &&
      Overlap( y_, a.y(), epsilon )  &&
      Overlap( z_, a.z(), epsilon ) )
    return 1;
  else
    return 0;
}

} // End namespace Uintah

