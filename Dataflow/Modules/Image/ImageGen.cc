//static char *id="@(#) $Id"

/*
 *  ImageGen.cc: ImageGen Reader class
 *  Written by:
 *
 *    Scott Morris
 *    July 1997
 */

#include <Dataflow/Network/Module.h>
#include <Dataflow/Ports/ScalarFieldPort.h>
#include <Core/Datatypes/ScalarField.h>
#include <Core/Datatypes/ScalarFieldRG.h>
#include <Core/Malloc/Allocator.h>
#include <Core/TclInterface/TCLTask.h>
#include <Core/TclInterface/TCLvar.h>
#include <iostream>
using std::cerr;
#include <math.h>

namespace SCIRun {



class ImageGen : public Module {
    ScalarFieldOPort* outport;
    ScalarFieldHandle handle;
    int x,y;
    int width,height;
    TCLdouble amp,period;
    TCLint horizontal,vertical;
  
public:
    ImageGen(const clString& id);
    virtual ~ImageGen();
    virtual void execute();
    void tcl_command( TCLArgs&, void *);
};

inline double abs(double i) { return i < 0 ? -i: i; }
inline double max(double i, double j) { return i > j ? i : j; }
#define pi M_PI

extern "C" Module* make_ImageGen(const clString& id)
{
    return scinew ImageGen(id);
}

ImageGen::ImageGen(const clString& id)
: Module("ImageGen", id, Filter),
  period("period", id, this), amp("amp", id, this),
  horizontal("horizontal", id, this), vertical("vertical", id, this)
{
    // Create the output data handle and port
    outport=scinew ScalarFieldOPort(this, "Output Data", ScalarFieldIPort::Atomic);
    add_oport(outport);
    width=height=512;
}

ImageGen::~ImageGen()
{
}

void ImageGen::execute()
{
  int xdim,ydim,zdim=1;
 
  xdim=width;
  ydim=height;

  cerr << "--ImageGen--\n";
  cerr << "Generating Image..\n";
  cerr << "Dimensions: " << xdim << " " << ydim << "\n";
	
  ScalarFieldRG* sf=new ScalarFieldRG;
  sf->resize(ydim, xdim, zdim);
  sf->compute_bounds();
  Point pmin(0,0,0),pmax(xdim,ydim,zdim);
  sf->set_bounds(pmin,pmax);  // something more reasonable later...

  double per=period.get();
  double am=amp.get();

  // do max here for vertical and horizontal
  double d = 4*pi*pi;
  
  for (x=0; x<width; x++)
    for (y=0; y<height; y++) {
      if (horizontal.get())
	sf->grid(y,x,0) = am*abs(sin((d/per)*(double(y)/height)));
      if ((horizontal.get()) && (vertical.get())) 
	  sf->grid(y,x,0) = max(am*abs(sin((d/per)*(double(x)/width))),
				am*abs(sin((d/per)*(double(y)/height))));
      else if (vertical.get())
	sf->grid(y,x,0) = am*abs(sin((d/per)*(double(x)/width)));
    }
	
  cerr << "--ENDImageGen--\n";
  handle = sf;
  
  outport->send(handle);
}

void ImageGen::tcl_command(TCLArgs& args, void* userdata)
{
  if (args[1] == "setsize") { // initialize something...
    args[2].get_int(width);
    args[3].get_int(height);
  } else {
    Module::tcl_command(args, userdata);
  }
}

} // End namespace SCIRun

