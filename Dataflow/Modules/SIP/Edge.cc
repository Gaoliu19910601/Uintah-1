//static char *id="@(#) $Id"

/*
 *  Edge.cc:  Edge Detection algorithms module
 *
 *  Written by:
 *    Scott Morris
 *    August 1997
 */

#include <Core/Containers/Array1.h>
#include <Dataflow/Network/Module.h>
#include <Dataflow/Ports/GeometryPort.h>
#include <Dataflow/Ports/ScalarFieldPort.h>
#include <Core/Datatypes/ScalarFieldRG.h>
#include <Core/Datatypes/ScalarFieldRGchar.h>
#include <Dataflow/Ports/ColorMapPort.h>
#include <Core/Geom/GeomGrid.h>
#include <Core/Geom/GeomGroup.h>
#include <Core/Geom/GeomLine.h>
#include <Core/Geom/Material.h>
#include <Core/Geometry/Point.h>
#include <Core/Math/MinMax.h>
#include <Core/Malloc/Allocator.h>
#include <Core/TclInterface/TCLvar.h>
#include <Core/Thread/Parallel.h>
#include <Core/Thread/Thread.h>
#include <math.h>
#include <iostream>
using std::cerr;


namespace SCIRun {



class Edge : public Module {
   ScalarFieldIPort *inscalarfield;
   ScalarFieldIPort *inscalarfield2;
   ScalarFieldOPort *outscalarfield;
   ScalarFieldOPort *outscalarfield2;
   int gen,genblur;
   int ty;  // type of function to run
  
   ScalarFieldRG* newgrid;
   ScalarFieldRG* tempgrid;
   ScalarFieldRG* blur;
   ScalarFieldRG* orient;
   ScalarFieldRG* rg;
   ScalarFieldRG* d1;
   ScalarFieldRGchar* mu;
   TCLstring funcname;
   TCLdouble t1;

   int compute_orient;
  
   double matrix[9]; // matrix for convolution...

   double normal,tt1,tt2;
   double thresh,thresh2;

   int np; // number of proccesors
  
public:
   Edge(const clString& id);
   virtual ~Edge();
   virtual void execute();

   void tcl_command( TCLArgs&, void *);

   void do_parallel(int proc);
   void do_canny(int proc);
   void do_canny_link(int proc);
};

extern "C" Module* make_Edge(const clString& id)
{
   return scinew Edge(id);
}

//static clString module_name("Edge");

Edge::Edge(const clString& id)
: Module("Edge", id, Filter), funcname("funcname",id,this),
  t1("t1", id, this)
{
    // Create the input ports
    // Need a scalar field
  
    inscalarfield = scinew ScalarFieldIPort( this, "Scalar Field",
					ScalarFieldIPort::Atomic);
    // now add the blurred input port
    inscalarfield2 = scinew ScalarFieldIPort( this, "Scalar Field",
					ScalarFieldIPort::Atomic);

    add_iport( inscalarfield);
    add_iport( inscalarfield2);
    
    // Create the output port
    outscalarfield = scinew ScalarFieldOPort( this, "Scalar Field",
					ScalarFieldIPort::Atomic);
    add_oport( outscalarfield);

    outscalarfield2 = scinew ScalarFieldOPort( this, "Scalar Field",
					ScalarFieldIPort::Atomic);
    add_oport( outscalarfield2);
    
    newgrid=new ScalarFieldRG;
    orient=new ScalarFieldRG;
    tempgrid=new ScalarFieldRG;

    mu=new ScalarFieldRGchar;
    
    normal = 1;
    gen=genblur=99;  // ?
}

Edge::~Edge()
{
}

double sector(double theta)  // return sector of orientation.. (Gradient dir)
{
  double phi=theta+3.14159;
  phi=phi*180/3.14159;  // convert to degrees
  if (((phi>0) && (phi<22.5)) || ((phi>180-22.5) && (phi<180+22.5)) ||
      (phi>360-22.5)) return 0;
  if (((phi>22.5) && (phi<45+22.5)) || ((phi>180+22.5) && (phi<225+22.5)))
    return 1;
  if (((phi>90-22.5) && (phi<90+22.5)) || ((phi>270-22.5) && (phi<270+22.5)))
    return 2;
  if (((phi>315-225) && (phi<315+22.5)) || ((phi>135-22.5) && (phi<135+22.5)))
    return 3;
  return 0;
}

void Edge::do_parallel(int proc)
{
  int start = (newgrid->grid.dim1()-1)*proc/np;
  int end   = (proc+1)*(newgrid->grid.dim1()-1)/np;

  if (!start) start++;
  if (end == newgrid->grid.dim1()-1) end--;

  /* Reverse boundaries - 1 -> newgrid->grid1.dim1()-1 and
     start -> end */

  double xx,yy;
  int x,y;
  
  switch (ty) { 
  case 1:    // Roberts
    for(x=start; x<end; x++)                
      for(y=1; y<newgrid->grid.dim2()-1; y++){  
        xx = rg->grid(x+1,y,0) - rg->grid(x,y+1,0);
	yy = rg->grid(x,y,0) - rg->grid(x+1,y+1,0);
	newgrid->grid(x,y,0) = sqrt(xx*xx+yy*yy)*normal;
	orient->grid(x,y,0) = atan2(xx,yy);
      }
    break; 
  case 2:    // Sobel
    for(x=start; x<end; x++)                
      for(y=1; y<newgrid->grid.dim2()-1; y++){  
        xx = (-rg->grid(x-1,y-1,0) + rg->grid(x+1,y-1,0) \
	      -2*rg->grid(x-1,y,0) + 2*rg->grid(x+1,y,0) \
	      -rg->grid(x-1,y+1,0) + rg->grid(x+1,y+1,0));
	yy = (rg->grid(x-1,y-1,0) + 2*rg->grid(x,y-1,0) \
	    + rg->grid(x+1,y-1,0) - rg->grid(x-1,y+1,0) \
	     -2*rg->grid(x,y+1,0) - rg->grid(x+1,y+1,0));
	newgrid->grid(x,y,0) = sqrt(xx*xx+yy*yy)*normal;
	orient->grid(x,y,0) = atan2(xx,yy);
      }
    break;
  case 4:    // Laplacian
    for(x=start; x<end; x++)                
      for(y=1; y<newgrid->grid.dim2()-1; y++){
	newgrid->grid(x,y,0) = (rg->grid(x-1,y-1,0)) +
				rg->grid(x,y-1,0) \
			      + rg->grid(x+1,y-1,0) + rg->grid(x-1,y,0) \
				-8*rg->grid(x,y,0) + rg->grid(x+1,y,0) \
			      + rg->grid(x-1,y+1,0) + rg->grid(x,y+1,0) \
			      + rg->grid(x+1,y+1,0)*normal;
      }
    break;
  case 3:   // Prewitt
    for (x=start; x<end; x++)
      for (y=1; y<newgrid->grid.dim2()-1; y++){  
        xx = (-rg->grid(x-1,y-1,0) + rg->grid(x+1,y-1,0) \
	      -rg->grid(x-1,y,0) + rg->grid(x+1,y,0) \
	      -rg->grid(x-1,y+1,0) + rg->grid(x+1,y+1,0));
	yy = (rg->grid(x-1,y-1,0) + rg->grid(x,y-1,0) \
	      + rg->grid(x+1,y-1,0) - rg->grid(x-1,y+1,0) \
	      -rg->grid(x,y+1,0) - rg->grid(x+1,y+1,0));
	newgrid->grid(x,y,0) = sqrt(xx*xx+yy*yy)*normal;
	orient->grid(x,y,0) = atan2(xx,yy);
      }
    break;
  case 5:    // Canny 
    for (x=start; x<end; x++)
      for (y=1; y<newgrid->grid.dim2()-1; y++){  
	xx = (blur->grid(x+1,y,0) - blur->grid(x,y,0) +
	      blur->grid(x+1,y+1,0) - blur->grid(x,y+1,0)) / 2;
	yy = (blur->grid(x,y,0) - blur->grid(x,y+1,0) +
	      blur->grid(x+1,y,0) - blur->grid(x+1,y+1,0)) / 2;
	tempgrid->grid(x,y,0) = sqrt(xx*xx+yy*yy);
	orient->grid(x,y,0) = atan2(xx,yy);
	mu->grid(x,y,0) = sector(orient->grid(x,y,0));
      }
    break;
  }
}

// Need another paralell procedure here that will look in the direction of the
// orientation and make sure it is a local maximum.  Ok?

void Edge::do_canny(int proc)
{
  int start = (newgrid->grid.dim1()-1)*proc/np;
  int end   = (proc+1)*(newgrid->grid.dim1()-1)/np;

  if (!start) start++;
  if (end == newgrid->grid.dim1()-1) end--;  

  for(int x=start; x<end; x++)                
    for(int y=1; y<newgrid->grid.dim2()-1; y++) {
      switch (mu->grid(x,y,0)) {
      case 0:
	if ((tempgrid->grid(x,y,0) <= tempgrid->grid(x,y+1,0)) ||
	    ((tempgrid->grid(x,y,0)) <= (tempgrid->grid(x,y-1,0))))
	  newgrid->grid(x,y,0) = 0; else
	    newgrid->grid(x,y,0) = tempgrid->grid(x,y,0);
        break;
      case 1:
	if ((tempgrid->grid(x,y,0)<=tempgrid->grid(x+1,y+1,0)) ||
	    (tempgrid->grid(x,y,0)<=tempgrid->grid(x-1,y-1,0)))
	  newgrid->grid(x,y,0) = 0;  else
	    newgrid->grid(x,y,0) = tempgrid->grid(x,y,0);
        break;
      case 2:
	if ((tempgrid->grid(x,y,0)<=tempgrid->grid(x+1,y,0)) ||
	    (tempgrid->grid(x,y,0)<=tempgrid->grid(x-1,y,0)))
	  newgrid->grid(x,y,0) = 0;  else
	    newgrid->grid(x,y,0) = tempgrid->grid(x,y,0);
	break;
      case 3:
	if ((tempgrid->grid(x,y,0)<=tempgrid->grid(x+1,y-1,0)) ||
	    (tempgrid->grid(x,y,0)<=tempgrid->grid(x-1,y+1,0)))
	  newgrid->grid(x,y,0) = 0;  else
	    newgrid->grid(x,y,0) = tempgrid->grid(x,y,0);
        break;
      }
      
      if (newgrid->grid(x,y,0)>thresh)
	d1->grid(x,y,0)=newgrid->grid(x,y,0);
      if (newgrid->grid(x,y,0)<thresh2)  // This used to be thresh2
	newgrid->grid(x,y,0)=0;
//      newgrid->grid(x,y,0)=tempgrid->grid(x,y,0);
    }
}

// Do the canny double thresholding and linking to filter out noise

void Edge::do_canny_link(int proc)
{
  int start = (newgrid->grid.dim1()-1)*proc/np;
  int end   = (proc+1)*(newgrid->grid.dim1()-1)/np;

  if (!start) start++;
  if (end == newgrid->grid.dim1()-1) end--;

  for(int x=start; x<end; x++)                
    for(int y=1; y<newgrid->grid.dim2()-1; y++) {
      if (newgrid->grid(x,y,0)>0) 
	switch (mu->grid(x,y,0)) {
	case 0:
	  if (d1->grid(x,y+1,0) > 0) newgrid->grid(x,y+1,0)=d1->grid(x,y+1,0);
	  if (d1->grid(x,y-1,0) > 0) newgrid->grid(x,y-1,0)=d1->grid(x,y-1,0);
	  break;
	case 1: 
	  if (d1->grid(x+1,y+1,0) > 0) newgrid->grid(x+1,y+1,0)=
					 d1->grid(x+1,y+1,0);
	  if (d1->grid(x-1,y-1,0) > 0) newgrid->grid(x-1,y-1,0)=
					 d1->grid(x-1,y-1,0);  
	  break;
	case 2:
	  if (d1->grid(x+1,y,0) > 0) newgrid->grid(x+1,y,0)=d1->grid(x+1,y,0);
	  if (d1->grid(x-1,y,0) > 0) newgrid->grid(x-1,y,0)=d1->grid(x-1,y,0);
	  break;
	case 3:
	  if (d1->grid(x-1,y+1,0) > 0) newgrid->grid(x-1,y+1,0)=
					 d1->grid(x-1,y+1,0);
	  if (d1->grid(x+1,y-1,0) > 0) newgrid->grid(x+1,y-1,0)=
					 d1->grid(x+1,y-1,0);
	  break;
	} 
    }
}

void Edge::execute()
{
    // get the scalar field...if you can

    ScalarFieldHandle sfield;
    if (!inscalarfield->get( sfield ))
	return;

    rg=sfield->getRG();
    
    if(!rg){
      cerr << "Edge cannot handle this field type\n";
      return;
    }

    // Figure out what algorithm to use
    
    clString ft(funcname.get());
    
    compute_orient=1;
    if (ft=="Roberts") ty = 1; 
    if (ft=="Sobel") ty = 2;
    if (ft=="Prewitt") ty = 3;
    if (ft=="Canny") ty = 5;
    if (ft=="Laplacian") {
      ty = 4;
      for (int x=0;x<9;x++)
	matrix[x]=1;
      matrix[4]=-8;
      compute_orient=0;
    }

    if (ty==5) {  // look for blurred input for canny edge detector
      ScalarFieldHandle sfield2;
      // get the blurred scalar field...if you can
      
      if (!inscalarfield2->get( sfield2 )) {
	cerr << "Canny requires a blurred image on the left input port..\n";
	return;
      }  
      blur=sfield2->getRG();
      
      if(!blur){
	cerr << "Edge cannot handle this field type\n";
	return;
      }
      thresh=t1.get();
      thresh2=thresh*2;

      /*
      int nx=blur->grid.dim1();
      int ny=blur->grid.dim2();
      int nz=blur->grid.dim3();
      
      if (genblur!=blur->generation) {
	newgrid=new ScalarFieldRG;
	orient=new ScalarFieldRG;
	d1 = new ScalarFieldRG;
	tempgrid=new ScalarFieldRG;
	mu=new ScalarFieldRGchar;
	
	newgrid->resize(nx,ny,nz);
	orient->resize(nx,ny,nz);
	d1->resize(nx,ny,nz);
	tempgrid->resize(nx,ny,nz);
	mu->resize(nx,ny,nz);
	
	genblur=blur->generation;
      }
*/
      
    }
      
/*      

      if (genblur!=newgrid->generation) {
	newgrid=new ScalarFieldRG(*blur);
	orient=new ScalarFieldRG(*blur);
	d1 = new ScalarFieldRG(*blur);
	tempgrid=new ScalarFieldRG(*blur);
	mu=new ScalarFieldRGchar;
      }
*/
      
    
    

    int nx=rg->grid.dim1();
    int ny=rg->grid.dim2();
    int nz=rg->grid.dim3();

    
/*    if (gen!=rg->generation) { */
      newgrid=new ScalarFieldRG;
      orient=new ScalarFieldRG;
      d1 = new ScalarFieldRG;
      tempgrid=new ScalarFieldRG;
      mu=new ScalarFieldRGchar;

      newgrid->resize(nx,ny,nz);
      orient->resize(nx,ny,nz);
      d1->resize(nx,ny,nz);
      tempgrid->resize(nx,ny,nz);
      mu->resize(nx,ny,nz);

      gen=rg->generation;

/*    }   */

 
    
    cerr << "--Edge--\nConvolving with " << ft << ".\n";
    cerr << "Scaling by : " << normal << ".\n--ENDEdge--\n";
    
    if (ty==5) {
      if ((nx!=blur->grid.dim1()) || (ny!=blur->grid.dim2())) {
	cerr << "Blurred image must be the same size as input image..\n";
	cerr << "Resample it w/ Subsample module..\n";
	return;
      }
    }
    
    np = Thread::numProcessors();

    np = 1;
    
    Thread::parallel(Parallel<Edge>(this, &Edge::do_parallel),
		     np, true);

    if (ty==5) {  // Run the extra canny stuff
	Thread::parallel(Parallel<Edge>(this, &Edge::do_canny),
			 np, true);
	Thread::parallel(Parallel<Edge>(this, &Edge::do_canny_link),
			 np, true);
    }
      
    outscalarfield->send( newgrid );
    if (compute_orient) 
      outscalarfield2->send( orient );
}

void Edge::tcl_command(TCLArgs& args, void* userdata)
{
  if (args[1] == "initmatrix") { // initialize the scale factor
    args[2].get_double(tt1);
    args[3].get_double(tt2);
    if (tt2)
      normal=(tt1/tt2); else normal=0;
  } else {
    Module::tcl_command(args, userdata);
  }
}

} // End namespace SCIRun

