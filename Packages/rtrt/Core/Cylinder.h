/*

The MIT License

Copyright (c) 1997-2009 Center for the Simulation of Accidental Fires and 
Explosions (CSAFE), and  Scientific Computing and Imaging Institute (SCI), 
University of Utah.

License for the specific language governing rights and limitations under
Permission is hereby granted, free of charge, to any person obtaining a 
copy of this software and associated documentation files (the "Software"),
to deal in the Software without restriction, including without limitation 
the rights to use, copy, modify, merge, publish, distribute, sublicense, 
and/or sell copies of the Software, and to permit persons to whom the 
Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included 
in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS 
OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, 
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL 
THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER 
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING 
FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER 
DEALINGS IN THE SOFTWARE.

*/



#ifndef Cylinder_H
#define Cylinder_H 1

#include <Core/Geometry/Point.h>
#include <Core/Geometry/Vector.h>
#include <Packages/rtrt/Core/Object.h>
#include <Packages/rtrt/Core/UVMapping.h>
#include <Core/Geometry/Transform.h>
#include <cstdlib>

namespace rtrt {
  class Cylinder;
}

namespace SCIRun {
  void Pio(Piostream&, rtrt::Cylinder*&);
}

namespace rtrt {

using SCIRun::Vector;
using SCIRun::Point;
using SCIRun::Transform;

class Cylinder : public Object {
protected:
  Point top;
  Point bottom;
  double radius;
  Transform xform;
  Transform ixform;
public:
  Cylinder(Material* matl, const Point& bottom, const Point& top, 
	   double radius);
  virtual ~Cylinder();

  Cylinder() : Object(0) {} // for Pio.

  //! Persistent I/O.
  static  SCIRun::PersistentTypeID type_id;
  virtual void io(SCIRun::Piostream &stream);
  friend void SCIRun::Pio(SCIRun::Piostream&, Cylinder*&);

  virtual void intersect(Ray& ray, HitInfo& hit, DepthStats* st,
			   PerProcessorContext*);
  virtual void preprocess(double maxradius, int& pp_offset, int& scratchsize);
  virtual Vector normal(const Point&, const HitInfo& hit);
  virtual void compute_bounds(BBox&, double offset);
  virtual void print(ostream& out);
};

} // end namespace rtrt

#endif
