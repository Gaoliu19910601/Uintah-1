
/*
 *  PointLight.cc:  A Point light source
 *
 *  Written by:
 *   Steven G. Parker
 *   Department of Computer Science
 *   University of Utah
 *   September 1994
 *
 *  Copyright (C) 1994 SCI Group
 */

#include <Core/Geom/PointLight.h>
#include <Core/Geom/GeomSphere.h>

namespace SCIRun {

Persistent* make_PointLight()
{
    return new PointLight("", Point(0,0,0), Color(0,0,0));
}

PersistentTypeID PointLight::type_id("PointLight", "Light", make_PointLight);

PointLight::PointLight(const clString& name,
		       const Point& p, const Color& c)
: Light(name), p(p), c(c)
{
}

PointLight::~PointLight()
{
}

#define POINTLIGHT_VERSION 1

void PointLight::io(Piostream& stream)
{

    stream.begin_class("PointLight", POINTLIGHT_VERSION);
    // Do the base class first...
    Light::io(stream);
    Pio(stream, p);
    Pio(stream, c);
    stream.end_class();
}

} // End namespace SCIRun

