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



#ifndef PARALLELOGRAM_H
#define PARALLELOGRAM_H 1

#include <Packages/rtrt/Core/Object.h>
#include <Packages/rtrt/Core/UVMapping.h>
#include <Core/Geometry/Point.h>

namespace rtrt {
class Parallelogram;
}
namespace SCIRun {
void Pio(Piostream&, rtrt::Parallelogram*&);
}

namespace rtrt {

class Parallelogram : public Object, public UVMapping {
protected:
  Point anchor;
  Vector u,v;
  Vector n;
  double d;
  double d1;
  double d2;
  Vector un, vn;
  double du, dv;
public:
  Parallelogram() : Object(0), UVMapping() {}
  Parallelogram(Material* matl, const Point& cen, const Vector& u, 
		const Vector& v);
  virtual ~Parallelogram();

  //! Persistent I/O.
  static  SCIRun::PersistentTypeID type_id;
  virtual void io(SCIRun::Piostream &stream);
  friend void SCIRun::Pio(SCIRun::Piostream&, Parallelogram*&);

  virtual void intersect(Ray& ray, HitInfo& hit, DepthStats* st,
			 PerProcessorContext*);
  virtual void light_intersect(Ray& ray, HitInfo& hit, Color& atten,
			       DepthStats* st, PerProcessorContext* ppc);
  virtual Vector normal(const Point&, const HitInfo& hit);
  virtual void uv(UV& uv, const Point&, const HitInfo& hit);
  virtual void compute_bounds(BBox&, double offset);

  Point  get_anchor() { return anchor; }
  Vector get_u() { return u; }
  Vector get_v() { return v; }
};

} // end namespace rtrt

#endif
