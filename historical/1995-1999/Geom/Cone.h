
/*
 *  Cone.h: Cone object
 *
 *  Written by:
 *   Steven G. Parker & David Weinstein
 *   Department of Computer Science
 *   University of Utah
 *   April 1994
 *
 *  Copyright (C) 1994 SCI Group
 */

#ifndef SCI_Geom_Cone_h
#define SCI_Geom_Cone_h 1

#include <Geom/Geom.h>
#include <Geometry/Point.h>
#include <Geometry/Vector.h>

class GeomCone : public GeomObj {
protected:
    Vector v1;
    Vector v2;
    double tilt;
    double height;
    Vector zrotaxis;
    double zrotangle;
public:
    Point bottom;
    Point top;
    Vector axis;
    double bot_rad;
    double top_rad;
    int nu;
    int nv;

    void adjust();
    void move(const Point&, const Point&, double, double, int nu=20, int nv=4);

    GeomCone(int nu=20, int nv=10);
    GeomCone(const Point&, const Point&, double, double, int nu=20, int nv=4);
    GeomCone(const GeomCone&);
    virtual ~GeomCone();

    virtual GeomObj* clone();
    virtual void get_bounds(BBox&);
    virtual void get_bounds(BSphere&);

#ifdef SCI_OPENGL
    virtual void draw(DrawInfoOpenGL*, Material*, double time);
#endif
    virtual void make_prims(Array1<GeomObj*>& free,
			    Array1<GeomObj*>& dontfree);
    virtual void preprocess();
    virtual void intersect(const Ray& ray, Material*,
			   Hit& hit);
    virtual void io(Piostream&);
    static PersistentTypeID type_id;
    virtual bool saveobj(ostream&, const clString& format, GeomSave*);
};

class GeomCappedCone : public GeomCone {
    int nvdisc1;
    int nvdisc2;
public:
    GeomCappedCone(int nu=20, int nv=10, int nvdisc1=4, int nvdisc2=4);
    GeomCappedCone(const Point&, const Point&, double, double, 
		   int nu=20, int nv=4, int nvdisc1=4, int nvdisc2=4);
    GeomCappedCone(const GeomCappedCone&);
    virtual ~GeomCappedCone();

    virtual GeomObj* clone();
#ifdef SCI_OPENGL
    virtual void draw(DrawInfoOpenGL*, Material*, double time);
#endif
    virtual void make_prims(Array1<GeomObj*>& free,
			    Array1<GeomObj*>& dontfree);
    virtual void preprocess();
    virtual void intersect(const Ray& ray, Material*,
			   Hit& hit);
    virtual void io(Piostream&);
    static PersistentTypeID type_id;
    virtual bool saveobj(ostream&, const clString& format, GeomSave*);
};

#endif /* SCI_Geom_Cone_h */

