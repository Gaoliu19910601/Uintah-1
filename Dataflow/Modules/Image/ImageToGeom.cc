//static char *id="@(#) $Id"

/*
 *  ImageToGeom.cc:  Unfinished modules
 *
 *  Written by:
 *   Steven G. Parker
 *   Department of Computer Science
 *   University of Utah
 *   March 1994
 *
 *  Copyright (C) 1994 SCI Group
 */

#include <Dataflow/Network/Module.h>
#include <Dataflow/Ports/GeometryPort.h>
#include <Dataflow/Ports/ImagePort.h>
#include <Core/Geom/GeomGrid.h>
#include <Core/Malloc/Allocator.h>
#include <Core/TclInterface/TCLvar.h>

namespace SCIRun {

class ImageToGeom : public Module {
  ImageIPort* iport;
  GeometryOPort* ogeom;
  double scale;
  int have_scale;
public:
  TCLint format;
  TCLdouble heightscale;
  TCLint downsample;
  ImageToGeom(const clString& id);
  virtual ~ImageToGeom();
  virtual void execute();

  int oldid;
};

extern "C" Module* make_ImageToGeom(const clString& id)
{
  return scinew ImageToGeom(id);
}

ImageToGeom::ImageToGeom(const clString& id)
  : Module("ImageToGeom", id, Filter), format("format", id, this),
    heightscale("heightscale", id, this), downsample("downsample", id, this)
{
  iport=scinew ImageIPort(this, "Image", ImageIPort::Atomic);
  add_iport(iport);
  // Create the output port
  ogeom=scinew GeometryOPort(this, "Geom object", ImageIPort::Atomic);
  add_oport(ogeom);
  oldid=0;
  have_scale=0;
}

ImageToGeom::~ImageToGeom()
{
}

void ImageToGeom::execute()
{
  ImageHandle image;
  if(!iport->get(image))
    return;
  //cerr << "generation=" << image->generation << endl;
  int form=format.get();
  GeomObj* obj=0;
#if 0
  double hs(heightscale.get());
#endif
  switch(form){
  case 0:
    {
#if 0
      int xres=image->xres();
      int yres=image->yres();
      GeomGrid* grid=new GeomGrid(image->xres(), image->yres(),
				  Point(0,0,0),
				  Vector(xres-1,0,0),
				  Vector(0,yres-1,0),
				  GeomGrid::WithNormals);
      for(int j=0;j<yres;j++){
	for(int i=0;i<xres;i++){
	  float value=image->getr(i,j);
	  grid->setn(i,j,hs*value);
	}
      }
      grid->compute_normals();
      obj=grid;
#endif
    }
    break;
  default:
    error("Unknown format");
    break;
  }

  if(oldid != 0)
    ogeom->delObj(oldid);
  if(obj != 0){	
    oldid=ogeom->addObj(obj, "Image");
  } else {
    oldid=0;
  }
  ogeom->flushViews();
}

} // End namespace SCIRun

