//static char *id="@(#) $Id$";

/*
 *  Tri.cc: Triangles...
 *
 *  Written by:
 *   Steven G. Parker & David Weinstein
 *   Department of Computer Science
 *   University of Utah
 *   April 1994
 *
 *  Copyright (C) 1994 SCI Group
 */

#include <SCICore/Geom/GeomTri.h>
#include <SCICore/Util/NotFinished.h>
#include <SCICore/Containers/String.h>
#include <SCICore/Geom/GeomRaytracer.h>
#include <SCICore/Geometry/BBox.h>
#include <SCICore/Geometry/BSphere.h>
#include <SCICore/Geometry/Ray.h>
#include <SCICore/Malloc/Allocator.h>
#include <SCICore/Math/MinMax.h>
#include <iostream.h>

namespace SCICore {
namespace GeomSpace {

using namespace Geometry;

Persistent* make_GeomTri()
{
    return scinew GeomTri(Point(0,0,0), Point(1,0,0), Point(0,1,0));
}

PersistentTypeID GeomTri::type_id("GeomTri", "GeomObj", make_GeomTri);

GeomTri::GeomTri(const Point& p1, const Point& p2, const Point& p3)
: n(Cross(p3-p1, p2-p1))
{
    if(n.length2() > 0){
	n.normalize();
    } else {
	cerr << "Degenerate triangle!!!\n" << endl;
    }
    add(p1);
    add(p2);
    add(p3);
}

GeomTri::GeomTri(const Point& p1, const Point& p2, const Point& p3,
		 const MaterialHandle& mat1, const MaterialHandle& mat2,
		 const MaterialHandle& mat3)
: n(Cross(p3-p1, p2-p1))
{
    if(n.length2() > 0){
	n.normalize();
    } else {
	cerr << "Degenerate triangle!!!\n" << endl;
    }
    add(p1, mat1);
    add(p2, mat2);
    add(p3, mat3);
}

GeomTri::GeomTri(const GeomTri &copy)
: GeomVertexPrim(copy), n(copy.n)
{
}

GeomTri::~GeomTri()
{
}

GeomObj* GeomTri::clone()
{
    return scinew GeomTri(*this);
}

#ifdef BSPHERE
void GeomTri::get_bounds(BSphere& bs)
{
    Point p1(verts[0]->p);
    Point p2(verts[1]->p);
    Point p3(verts[2]->p);
    Vector e1(p2-p1);
    Vector e2(p3-p2);
    Vector e3(p1-p3);
    double e1l2=e1.length2();
    double e2l2=e2.length2();
    double e3l2=e3.length2();
    Point cen;
    double rad=0;
    int do_circum=0;
    if(e1l2 > e2l2 && e1l2 > e3l2){
	cen=Interpolate(p1, p2, 0.5);
	double rad2=e1l2/4.;
	double dist2=(p3-cen).length2();
	if(dist2 > rad2){
	    do_circum=1;
	} else {
	    rad=Sqrt(rad2);
	}
    } else if(e2l2 > e1l2 && e2l2 > e3l2){
	cen=Interpolate(p2, p3, 0.5);
	double rad2=e2l2/4.;
	double dist2=(p1-cen).length2();
	if(dist2 > rad2){
	    do_circum=1;
	} else {
	    rad=Sqrt(rad2);
	}
    } else {
	cen=Interpolate(p3, p1, 0.5);
	double rad2=e3l2/4.;
	double dist2=(p2-cen).length2();
	if(dist2 > rad2){
	    do_circum=1;
	} else {
	    rad=Sqrt(rad2);
	}
    }
    if(do_circum){
	double d1=-Dot(e3, e1);
	double d2=-Dot(e2, e1);
	double d3=-Dot(e3, e2);
	double c1=d2*d3;
	double c2=d3*d1;
	double c3=d1*d2;
	double c=c1+c2+c3;
	double r2circ=0.25*(d1+d2)*(d2+d3)*(d3+d1)/c;

	// Use the circumcircle radius...
	cen=AffineCombination(p1, (c2+c3)/(2*c),
			      p2, (c3+c1)/(2*c),
			      p3, (c1+c2)/(2*c));
	rad=Sqrt(r2circ);
    }
    ASSERT((cen-p1).length() <= rad*1.00001);
    ASSERT((cen-p2).length() <= rad*1.00001);
    ASSERT((cen-p3).length() <= rad*1.00001);
    bs.extend(cen, rad);
}
#endif

void GeomTri::make_prims(Array1<GeomObj*>&,
			  Array1<GeomObj*>& dontfree)
{
    GeomTri* tri=this;
    dontfree.add(tri);
}

void GeomTri::preprocess()
{
    // Nothing to be done...
}

void GeomTri::intersect(const Ray& ray, Material* matl, Hit& hit)
{
  using SCICore::Math::Abs;

    double tmp=Dot(n, ray.direction());
    if(tmp > -1.e-6 && tmp < 1.e-6)return; // Parallel to plane
    Point p1(verts[0]->p);
    Point p2(verts[1]->p);
    Point p3(verts[2]->p);
    Vector v=p1-ray.origin();
    double t=Dot(n, v)/tmp;
    if(t<1.e-6)return;
    if(hit.hit() && t > hit.t())return;
    Point p=ray.origin()+ray.direction()*t;
    double pp1[2], pp2[2], pp3[2] , pt[2];
    double nx=Abs(n.x());
    double ny=Abs(n.y());
    double nz=Abs(n.z());
    if(nx > ny && nx > nz){
	pp1[0]=p1.y(); pp1[1]=p1.z();
	pp2[0]=p2.y(); pp2[1]=p2.z();
	pp3[0]=p3.y(); pp3[1]=p3.z();
	pt[0]=p.y(); pt[1]=p.z();
    } else if(ny > nz && ny > nx){
	pp1[0]=p1.z(); pp1[1]=p1.x();
	pp2[0]=p2.z(); pp2[1]=p2.x();
	pp3[0]=p3.z(); pp3[1]=p3.x();
	pt[0]=p.z(); pt[1]=p.x();
    } else {
	pp1[0]=p1.x(); pp1[1]=p1.y();
	pp2[0]=p2.x(); pp2[1]=p2.y();
	pp3[0]=p3.x(); pp3[1]=p3.y();
	pt[0]=p.x(); pt[1]=p.y();
    }

    int nc=x_cross(pp1, pp2, pt);
    nc+=x_cross(pp2, pp3, pt);
    if(nc==2)return;
    nc+=x_cross(pp3, pp1, pt);
    if(nc==1){
	// We hit!!!
	hit.hit(t, this, matl);
    }
}

int GeomTri::x_cross(double p1[2], double p2[2], double p[2])
{
    // Cut off left
    if(p[0] < p1[0] && p[0] <= p2[0])
	return 0;
    // Cut off botton
    if(p[1] < p1[1] && p[1] <= p2[1])
	return 0;
    // Cut off top
    if(p[1] > p1[1] && p[1] >= p2[1])
	return 0;
    // If on right, then it definitely crosses
    if(p[0] > p1[0] && p[0] >= p2[0])
	return 1;
    // General case...
    double delta=p2[1]-p1[1];
    if(delta < 1.e-6 && delta > -1.e-6)
	return 0;
    double yt=p[1]-p1[1];
    double x=yt/delta*(p2[0]-p1[0])+p1[0];
    if(p[0] < x)
	return 0; // p is left of edge
    else
	return 1; // p is right of edge
}

Vector GeomTri::normal(const Point&, const Hit&)
{
    return n;
}

#define GEOMTRI_VERSION 1

void GeomTri::io(Piostream& stream)
{
    stream.begin_class("GeomTri", GEOMTRI_VERSION);
    GeomVertexPrim::io(stream);
    stream.end_class();
}

bool GeomTri::saveobj(ostream&, const clString&, GeomSave*)
{
    NOT_FINISHED("GeomTri::saveobj");
    return false;
}

} // End namespace GeomSpace
} // End namespace SCICore

//
// $Log$
// Revision 1.2  1999/08/17 06:39:16  sparker
// Merged in modifications from PSECore to make this the new "blessed"
// version of SCIRun/Uintah.
//
// Revision 1.1  1999/07/27 16:56:46  mcq
// Initial commit
//
// Revision 1.1.1.1  1999/04/24 23:12:22  dav
// Import sources
//
//
