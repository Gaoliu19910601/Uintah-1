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


/*
Name:		Shaun Ramsey
Location:	University of Utah
Email:		ramsey@cs.utah.edu

This file contains the information necessary to do uv mapping for a sphere.
It will be used in ImageMaterial for texture mapping as well as in
RamseyImage for BumpMapping information. Remember that whenever you create a
sphere which may or may not use these options its always good code to include
the change in mappying to a UVSphere map.
*/

#include <Packages/rtrt/Core/UVSphere.h>
#include <Packages/rtrt/Core/UV.h>
#include <Packages/rtrt/Core/vec.h>

using namespace rtrt;
using namespace SCIRun;

Persistent* uvs_maker() {
  return new UVSphere();
}

// initialize the static member type_id
PersistentTypeID UVSphere::type_id("UVSphere", "Object", uvs_maker);


UVSphere::UVSphere(Material *matl, Point c, double r, const Vector &up,
                   const Vector &right) : 
  Object(matl,this), 
  cen(c), 
  up(up), 
  right(right),
  radius(r)
{
}

UVSphere::~UVSphere()
{
}

void UVSphere::preprocess(double, int&, int&)
{
    up.normalize();
    // Set up unit transformation
    xform.load_identity();
    xform.pre_translate(-cen.asVector());
    xform.rotate(Vector(0,0,1),up);
    xform.pre_scale(Vector(1./radius, 1./radius, 1./radius));
    ixform.load_identity();
    ixform.pre_scale(Vector(radius, radius, radius));
    ixform.rotate(up, Vector(0,0,1));
    ixform.pre_translate(cen.asVector());
}

void UVSphere::intersect(Ray& ray, HitInfo& hit, DepthStats* st,
                         PerProcessorContext*)
{
  Vector v=xform.project(ray.direction());
  double dist_scale = v.normalize();
  Ray xray(xform.project(ray.origin()), v);
  Vector xOC=-xform.project(ray.origin()).asVector();
  double tca=Dot(xOC, xray.direction());
  double l2oc=xOC.length2();
  double rad2=1;
  st->sphere_isect++;
  if(l2oc <= rad2){
    // Inside the sphere
    double t2hc=rad2-l2oc+tca*tca;
    double thc=sqrt(t2hc);
    double t=tca+thc;
    hit.hit(this, t/dist_scale);
    st->sphere_hit++;
    return;
  } else {
    if(tca < 0.0){
      // Behind ray, no intersections...
      return;
    } else {
      double t2hc=rad2-l2oc+tca*tca;
      if(t2hc <= 0.0){
        // Ray misses, no intersections
        return;
      } else {
        double thc=sqrt(t2hc);
        hit.hit(this, (tca-thc)/dist_scale);
        hit.hit(this, (tca+thc)/dist_scale);
        st->sphere_hit++;
        return;
      }
    }
  }	
}
#if 1
// Maybe this could be improved - steve
void UVSphere::light_intersect(Ray& ray, HitInfo& hit, Color&,
                               DepthStats* st, PerProcessorContext*)
{
  Vector v=xform.project(ray.direction());
  double dist_scale = v.normalize();
  Ray xray(xform.project(ray.origin()), v);
  Vector xOC=-xform.project(ray.origin()).asVector();
  double tca=Dot(xOC, xray.direction());
  double l2oc=xOC.length2();
  double rad2=1;
  st->sphere_isect++;
  if(l2oc <= rad2){
    // Inside the sphere
    double t2hc=rad2-l2oc+tca*tca;
    double thc=sqrt(t2hc);
    double t=tca+thc;
    hit.shadowHit(this, t/dist_scale);
    st->sphere_light_hit++;
    return;
  } else {
    if(tca < 0.0){
      // Behind ray, no intersections...
      return;
    } else {
      double t2hc=rad2-l2oc+tca*tca;
      if(t2hc <= 0.0){
	// Ray misses, no intersections
	return;
      } else {
	double thc=sqrt(t2hc);
	hit.shadowHit(this, (tca-thc)/dist_scale);
	hit.shadowHit(this, (tca+thc)/dist_scale);
	st->sphere_light_hit++;
	return;
      }
    }
  }	
}
#endif

Vector UVSphere::normal(const Point& hitpos, const HitInfo&)
{
  Vector n(hitpos-cen);
  n/=radius;
  return n;
}

void UVSphere::compute_bounds(BBox& bbox, double offset)
{
  bbox.extend(cen, radius+offset);
}

//vv = acos(m.z()/radius) / M_PI;
//uu = acos(m.x() / (radius * sin (M_PI*vv)))* over2pi;
void UVSphere::uv(UV& uv, const Point& hitpos, const HitInfo&)  
{
  Vector m(xform.project(hitpos).asVector());
  double uu,vv,theta,phi;  
  double angle = m.z();
  if(angle<-1)
    angle=-1;
  else if(angle>1)
    angle=1;
  theta = acos(angle);
  phi = atan2(m.y(), m.x());
  if(phi < 0) phi += 6.28318530718;
  uu = phi * .159154943092; // 1_pi
  vv = (M_PI - theta) * .318309886184; // 1 / ( 2 * pi )
  uv.set( uu,vv);
}

void UVSphere::get_frame(const Point &hitpos, const HitInfo&hit,
                         const Vector &norm,  Vector &pu, Vector &pv)
{
  UV uv_m;
  float u,v;
  double phi,theta;
  uv(uv_m,hitpos,hit);
  u = uv_m.u();
  v = uv_m.v();
  phi = 6.28318530718 * u;
  theta = -(M_PI*v) + M_PI;
  pu = Vector(-6.28318530718* radius * sin(phi) * sin(theta),
	      6.28318530718 * radius * sin(phi) * cos(theta), 	0);
  pv = Vector(M_PI * radius * cos(phi) * cos(theta),
	      M_PI * radius * cos(phi) * sin(theta),
	      -1 * M_PI * radius * sin(phi));
  VXV3(pu,norm,pv);
  VXV3(pv,norm,pu);
}

const int UVSPHERE_VERSION = 1;

void 
UVSphere::io(SCIRun::Piostream &str)
{
  str.begin_class("UVSphere", UVSPHERE_VERSION);
  Object::io(str);
  UVMapping::io(str);
  Pio(str, cen);
  Pio(str, up);
  Pio(str, right);
  Pio(str, radius);
  Pio(str, xform);
  Pio(str, ixform);
  str.end_class();
}

namespace SCIRun {
void Pio(SCIRun::Piostream& stream, rtrt::UVSphere*& obj)
{
  SCIRun::Persistent* pobj=obj;
  stream.io(pobj, rtrt::UVSphere::type_id);
  if(stream.reading()) {
    obj=dynamic_cast<rtrt::UVSphere*>(pobj);
    //ASSERT(obj != 0)
  }
}
} // end namespace SCIRun
