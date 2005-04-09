
/*
 *  Tri.h: Triangles...
 *
 *  Written by:
 *   Steven G. Parker & David Weinstein
 *   Department of Computer Science
 *   University of Utah
 *   April 1994
 *
 *  Copyright (C) 1994 SCI Group
 */

#ifndef SCI_Geom_Tri_h
#define SCI_Geom_Tri_h 1

#include <Geom/Geom.h>
#include <Geometry/Point.h>

class GeomTri : public GeomObj {
    int x_cross(double p1[2], double p2[2], double p[2]);
public:
    Point p1;
    Point p2;
    Point p3;
    Vector n;

    GeomTri(const Point&, const Point&, const Point&);
    GeomTri(const GeomTri&);
    virtual ~GeomTri();

    virtual GeomObj* clone();
    virtual void get_bounds(BBox&);
    virtual void get_bounds(BSphere&);

#ifdef SCI_OPENGL
    virtual void objdraw(DrawInfoOpenGL*, Material*);
#endif
    virtual void objdraw(DrawInfoX11*, Material*);
    virtual double depth(DrawInfoX11*);
    virtual void get_hit(Vector&, Point&);
    virtual void make_prims(Array1<GeomObj*>& free,
			    Array1<GeomObj*>& dontfree);
    virtual void preprocess();
    virtual void intersect(const Ray& ray, Material*,
			   Hit& hit);
    virtual Vector normal(const Point& p, const Hit&);
};

#endif /* SCI_Geom_Tri_h */
