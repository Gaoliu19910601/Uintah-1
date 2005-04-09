
/*
 *  Line.cc:  Line object
 *
 *  Written by:
 *   Steven G. Parker & David Weinstein
 *   Department of Computer Science
 *   University of Utah
 *   April 1994
 *
 *  Copyright (C) 1994 SCI Group
 */

#include <Geom/Line.h>
#include <Classlib/NotFinished.h>
#include <Geometry/BBox.h>
#include <Geometry/BSphere.h>

GeomLine::GeomLine(const Point& p1, const Point& p2)
: GeomObj(0), p1(p1), p2(p2)
{
}

GeomLine::GeomLine(const GeomLine& copy)
: GeomObj(0), p1(copy.p1), p2(copy.p2)
{
}

GeomLine::~GeomLine()
{
}

GeomObj* GeomLine::clone()
{    return new GeomLine(*this);
}

void GeomLine::get_bounds(BBox& bb)
{
    bb.extend(p1);
    bb.extend(p2);
}

void GeomLine::get_bounds(BSphere& bs)
{
    Point cen(Interpolate(p1, p2, 0.5));
    double rad=(p2-p1).length()/2.;
    bs.extend(cen, rad);
}

void GeomLine::make_prims(Array1<GeomObj*>&,
			  Array1<GeomObj*>& dontfree)
{
    GeomLine* line=this;
    dontfree.add(line);
}

void GeomLine::preprocess()
{
    NOT_FINISHED("GeomLine::preprocess");
}

void GeomLine::intersect(const Ray&, Material*,
			 Hit&)
{
    NOT_FINISHED("GeomLine::intersect");
}
