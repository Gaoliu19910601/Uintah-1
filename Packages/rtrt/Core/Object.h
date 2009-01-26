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




#ifndef OBJECT_H
#define OBJECT_H 1

#include <Packages/rtrt/Core/Array1.h>
#include <Packages/rtrt/Core/Color.h>

#include <Core/Geometry/Transform.h>
#include <Core/Persistent/Persistent.h>

#include <sgi_stl_warnings_off.h>
#include <string>
#include <sgi_stl_warnings_on.h>

namespace SCIRun {
  class Point;
  class Vector;
  class Transform;
}

namespace rtrt {

using SCIRun::Vector;
using SCIRun::Point;
using SCIRun::Transform;
using std::string;

struct DepthStats;

class HitInfo;
class Material;
class Ray;
class Light;
class BBox;
class PerProcessorContext;
class UVMapping;
class Object;
class Grid2;

template<class T> class Array1;
}
namespace SCIRun {
void Pio(Piostream&, rtrt::Object*&);
}

namespace rtrt {

/*
 * Each object now has a unique identification number
 * This is used when animating objects.
 * On start-up each object will have an odd number,
 * the first bit being used to identify whether it has
 * moved outside the initial spatial subdivision.
 * Used in conjunction with the Grid2.cc implementation
 */

class Object : public virtual SCIRun::Persistent {
 protected:
  Material  *matl;
  UVMapping *uv;
  Grid2     *animGrid;      
  unsigned long number; // id number
protected:
  bool was_preprocessed;
public:
  Object(Material* matl, UVMapping* uv=0);
  virtual ~Object();

  inline void set_grid_position (int inside, int outside) {
    number = (number & ~3) + inside + (outside << 1);
  }
  inline unsigned long get_id () {
    return number;
  }

  //! Persistent I/O.
  static  SCIRun::PersistentTypeID type_id;
  virtual void io(SCIRun::Piostream &stream);
  friend void SCIRun::Pio(SCIRun::Piostream&, Object*&);

  inline Material  * get_matl() const { return matl; }
  inline void        set_matl(Material* new_matl) { matl=new_matl; }
  inline UVMapping * get_uvmapping() { return uv; }
  inline void        set_uvmapping(UVMapping* uv) { this->uv=uv; }

  virtual void intersect(Ray& ray, HitInfo& hit, DepthStats* st,
			 PerProcessorContext*)=0;
  virtual void light_intersect(Ray& ray, HitInfo& hit, Color& atten,
			       DepthStats* st, PerProcessorContext* ppc);
  virtual void softshadow_intersect(Light* light, Ray& ray,
				    HitInfo& hit, double dist, Color& atten,
				    DepthStats* st, PerProcessorContext* ppc);
  virtual void multi_light_intersect(Light* light, const Point& orig,
				     const Array1<Vector>& dirs,
				     const Array1<Color>& attens,
				     double dist,
				     DepthStats* st, PerProcessorContext* ppc);
  virtual Vector normal(const Point&, const HitInfo& hit)=0;
  //    virtual void get_frame(const Point &p, Vector &n, Vector &u, Vector &v);
  virtual void animate(double t, bool& changed);
  virtual void preprocess(double maxradius, int& pp_offset, int& scratchsize);
  virtual void compute_bounds(BBox& bbox, double offset)=0;
  virtual void collect_prims(Array1<Object*>& prims);
  virtual void print(ostream& out);
  virtual void transform(Transform&) {}

  // stuff for Grid2
  virtual void allow_animation ();
  virtual void disallow_animation ();
  virtual void remove(Object* obj, const BBox& bbox); // For dynamic updates
  virtual void insert(Object* obj, const BBox& bbox); // Idem
  virtual void rebuild ();
  virtual void recompute_bbox ();
  virtual void set_scene (Object *);
  virtual void update(const Vector& update);
  virtual void set_anim_grid(Grid2 *g) { animGrid = g; }
  virtual Grid2 *get_anim_grid() { return animGrid; }

  // This function should return TRUE when the point in question
  // (ray.v * t + ray.t0) can be mapped to a value by the object.
  // It returns FALSE otherwise.
  virtual bool interior_value( double& /*value*/, const Ray &/*ray*/,
			       const double /*t*/)
  { return false; }; 
  virtual bool interior_valuepair( double& /*value1*/, double &/*value2*/,
				   const Ray &/*ray*/, const double /*t*/)
  { return false; }; 

  const string& get_name() const;
  void set_name(const string &s);
};

} // end namespace rtrt

#endif
