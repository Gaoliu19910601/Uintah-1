/*
 *  ContourSet.cc: The ContourSet Data type
 *
 *  Written by:
 *   David Weinstein
 *   Department of Computer Science
 *   University of Utah
 *   July 1994
 *
 *  Copyright (C) 1994 SCI Group
 */

#include <ContourSet.h>
#include <Surface.h>
#include <Geom.h>
#include <Classlib/String.h>
#include <Geometry/Transform.h>

#define Sqr(x) ((x)*(x))

static Persistent* make_ContourSet(){
    return new ContourSet;
}

PersistentTypeID ContourSet::typeid("ContourSet", "Datatype", make_ContourSet);

Vector mmult(double *m, const Vector &v) {
    double x[3], y[3];
    x[0]=v.x();x[1]=v.y();x[2]=v.z();
    for (int i=0; i<3; i++) {
	y[i]=0;
	for (int j=0; j<3; j++) {
	    y[i]+=m[j*4+i]*x[j];
	}
    }
    return Vector(y[0],y[1],y[2]);
}

ContourSet::ContourSet()
{
    basis[0]=Vector(1,0,0);
    basis[1]=Vector(0,1,0);
    basis[2]=Vector(0,0,1);
    origin=Vector(0,0,0);
    space=1;
};

ContourSet::ContourSet(const ContourSet &copy)
: space(copy.space), origin(copy.origin)
{
    basis[0]=copy.basis[0];
    basis[1]=copy.basis[1];
    basis[2]=copy.basis[2];
}

ContourSet::~ContourSet() {
}

void ContourSet::translate(const Vector &v) {
    origin=origin+v;
}

void ContourSet::scale(double sc) {
    basis[0]=basis[0]*sc;
    basis[1]=basis[1]*sc;
    basis[2]=basis[2]*sc;
}

// just takes the (dx, dy, dz) vector as input -- read off dials...
void ContourSet::rotate(const Vector &rot) {
    Transform tran;
    tran.rotate(rot.x(), Vector(1,0,0));
    tran.rotate(rot.y(), Vector(0,1,0));
    tran.rotate(rot.z(), Vector(0,0,1));
    basis[0]=tran.project(basis[0]);
    basis[1]=tran.project(basis[1]);
    basis[2]=tran.project(basis[2]);
}

#define CONTOUR_SET_VERSION 1

void ContourSet::io(Piostream& stream) 
{
    int version=stream.begin_class("ContourSet", CONTOUR_SET_VERSION);
    Pio(stream, contours);
    Pio(stream, basis[0]);
    Pio(stream, basis[1]);
    Pio(stream, basis[2]);
    Pio(stream, origin);
    Pio(stream, space);
    Pio(stream, name);
    stream.end_class();
}
