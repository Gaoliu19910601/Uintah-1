//static char *id="@(#) $Id"

/*
 *  ViewHist.cc:  Histogram Viewer
 *
 *  Written by:
 *    Scott Morris
 *    October 1997
 */

#include <Containers/Array1.h>
#include <Util/NotFinished.h>
#include <Dataflow/Module.h>
#include <Datatypes/GeometryPort.h>
#include <Datatypes/ScalarFieldPort.h>
#include <Datatypes/ScalarFieldRG.h>
#include <Datatypes/ColorMapPort.h>
#include <Geom/GeomGrid.h>
#include <Geom/GeomGroup.h>
#include <Geom/GeomLine.h>
#include <Geom/Material.h>
#include <Geometry/Point.h>
#include <Math/MinMax.h>
#include <Malloc/Allocator.h>
#include <TclInterface/TCLvar.h>
#include <Multitask/Task.h>
#include <math.h>

namespace SCIRun {
namespace Modules {

using namespace PSECore::Dataflow;
using namespace PSECore::Datatypes;

using namespace SCICore::TclInterface;
using namespace SCICore::Multitask;
using namespace SCICore::Math;

class ViewHist : public Module {
   ScalarFieldIPort *inscalarfield;
   ScalarFieldOPort *outscalarfield;
   int gen;

   ScalarFieldRG* newgrid;
   ScalarFieldRG*  rg;

   double min,max;       // Max and min values of the scalarfield


   int np; // number of proccesors
  
public:
   ViewHist(const clString& id);
   ViewHist(const ViewHist&, int deep);
   virtual ~ViewHist();
   virtual Module* clone(int deep);
   virtual void execute();

//   void tcl_command( TCLArgs&, void *);

   void do_ViewHist(int proc);

};

extern "C" {
Module* make_ViewHist(const clString& id)
{
   return scinew ViewHist(id);
}
}

//static clString module_name("ViewHist");

ViewHist::ViewHist(const clString& id)
: Module("ViewHist", id, Filter)
{
    // Create the input ports
    // Need a scalar field
  
    inscalarfield = scinew ScalarFieldIPort( this, "Scalar Field",
					ScalarFieldIPort::Atomic);
    add_iport( inscalarfield);

    // Create the output port
    outscalarfield = scinew ScalarFieldOPort( this, "Scalar Field",
					ScalarFieldIPort::Atomic);
    add_oport( outscalarfield);
    newgrid=new ScalarFieldRG;
}

ViewHist::ViewHist(const ViewHist& copy, int deep)
: Module(copy, deep)
{
   NOT_FINISHED("ViewHist::ViewHist");
}

ViewHist::~ViewHist()
{
}

Module* ViewHist::clone(int deep)
{
   return scinew ViewHist(*this, deep);
}

void ViewHist::do_ViewHist(int proc)    
{
  int start = (rg->grid.dim2())*proc/np;
  int end   = (proc+1)*(rg->grid.dim2())/np;

  for(int x=start; x<end; x++) {
    int y;
    for(y=0; y<rg->grid.dim1(); y++)
     newgrid->grid(y,x,0)=0;
    for(y=0; y<((rg->grid(0,x,0)/max)*500); y++) {
      newgrid->grid(y,x,0)=255;
    }
  }
}

static void start_ViewHist(void* obj,int proc)
{
  ViewHist* img = (ViewHist*) obj;

  img->do_ViewHist(proc);
}

void ViewHist::execute()
{
    // get the scalar field...if you can

    ScalarFieldHandle sfield;
    if (!inscalarfield->get( sfield ))
	return;

    rg=sfield->getRG();
    
    if(!rg){
      cerr << "ViewHist cannot handle this field type\n";
      return;
    }
    gen=rg->generation;
    
    if (gen!=newgrid->generation){
    //  newgrid=new ScalarFieldRGint;
      // New input
    }
    newgrid=new ScalarFieldRG;

    rg->compute_minmax();
    rg->get_minmax(min,max);

    cerr << "ViewHist min/max : " << min << " " << max << "\n";
    
    newgrid->resize(500,rg->grid.dim2(),1);

    np = Task::nprocessors();    
      
    Task::multiprocess(np, start_ViewHist, this);

    outscalarfield->send( newgrid );
}

/*
void ViewHist::tcl_command(TCLArgs& args, void* userdata)
{
  Module::tcl_command(args, userdata);
}
*/

} // End namespace Modules
} // End namespace SCIRun

//
// $Log$
// Revision 1.3  1999/08/25 03:49:00  sparker
// Changed SCICore/CoreDatatypes to SCICore/Datatypes
// Changed PSECore/CommonDatatypes to PSECore/Datatypes
// Other Misc. directory tree updates
//
// Revision 1.2  1999/08/17 06:40:04  sparker
// Merged in modifications from PSECore to make this the new "blessed"
// version of SCIRun/Uintah.
//
// Revision 1.1  1999/07/27 16:58:56  mcq
// Initial commit
//
// Revision 1.1  1999/04/29 22:26:36  dav
// Added image files to SCIRun
//
//
