
/*
 *  IsoSurfaceSP.cc:  The first module!
 *
 *  Written by:
 *   Steven G. Parker
 *   Department of Computer Science
 *   University of Utah
 *   March 1994
 *
 *  Copyright (C) 1994 SCI Group
 */

#include <Core/Tester/RigorousTest.h>
#include <Core/Containers/BitArray1.h>
#include <Core/Containers/Queue.h>
#include <Core/Containers/Stack.h>
#include <Dataflow/Network/Module.h>
#include <Dataflow/Ports/ColorMapPort.h>
#include <Dataflow/Ports/GeometryPort.h>
#include <Core/Datatypes/Mesh.h>
#include <Core/Datatypes/ScalarField.h>
#include <Core/Datatypes/ScalarFieldRG.h>
#include <Core/Datatypes/ScalarFieldUG.h>
#include <Dataflow/Ports/ScalarFieldPort.h>
#include <Dataflow/Ports/SurfacePort.h>
#include <Core/Datatypes/TriSurface.h>
#include <Core/Geom/GeomTimeGroup.h>
#include <Core/Geom/Material.h>
#include <Core/Geom/GeomTriangles.h>
#include <Core/Geom/GeomTriStrip.h>
#include <Core/Geometry/Point.h>
#include <Core/Geometry/Plane.h>
#include <Core/Malloc/Allocator.h>
#include <Core/Math/Expon.h>
#include <Core/Math/MiscMath.h>
#include <Core/TclInterface/TCLvar.h>
#include <Dataflow/Widgets/ArrowWidget.h>
#include <iostream>
using std::cerr;
#include <sstream>
using std::ostringstream;

namespace SCIRun {


class IsoSurfaceMRSG : public Module {
    ScalarFieldIPort* infield;
    ColorMapIPort* incolormap;

    GeometryOPort* ogeom;
    TCLdouble isoval;

    TCLdouble fstart;
    TCLdouble fend;

    TCLint   nframes;
    TCLint   dointerp;

    int IsoSurfaceMRSG_id;

    double old_min;
    double old_max;
    Point old_bmin;
    Point old_bmax;
    int sp;
    TCLint show_progress;

    MaterialHandle matl;

    int iso_cube(int, int, int, double, GeomTrianglesP*, ScalarFieldRG*);

    void iso_reg_grid(ScalarFieldRG*, double, GeomTrianglesP*);

    int init;
    Point ov[9];
    Point v[9];
public:
    IsoSurfaceMRSG(const clString& id);
    virtual ~IsoSurfaceMRSG();
    virtual void execute();
};

#define FACE4 8
#define FACE3 4
#define FACE2 2
#define FACE1 1
#define ALLFACES (FACE1|FACE2|FACE3|FACE4)

// below determines wether to normalzie normals or not

#ifdef SCI_NORM_OGL
#define NORMALIZE_NORMALS 0
#else
#define NORMALIZE_NORMALS 1
#endif
 
struct MCubeTable {
    int which_case;
    int permute[8];
    int nbrs;
};

#include "mcube.h"

extern "C" Module* make_IsoSurfaceMRSG(const clString& id) {
  return new IsoSurfaceMRSG(id);
}

//static clString module_name("IsoSurfaceMRSG");
static clString surface_name("IsoSurfaceMRSG");

IsoSurfaceMRSG::IsoSurfaceMRSG(const clString& id)
: Module("IsoSurfaceMRSG", id, Filter),
  isoval("isoval", id, this),
  show_progress("show_progress", id, this),
  fstart("fmin",id,this),
  fend("fmax",id,this),
  nframes("nframe",id,this),
  dointerp("dointerp",id,this)
{
    // Create the input ports
    infield=scinew ScalarFieldIPort(this, "Field", ScalarFieldIPort::Atomic);
    add_iport(infield);

    incolormap=scinew ColorMapIPort(this, "Color Map", ColorMapIPort::Atomic);
    add_iport(incolormap);
    

    // Create the output port
    ogeom=scinew GeometryOPort(this, "Geometry", GeometryIPort::Atomic);
    add_oport(ogeom);

    isoval.set(1);

    matl=scinew Material(Color(0,0,0), Color(0,.8,0),
		      Color(.7,.7,.7), 50);
    IsoSurfaceMRSG_id=0;

    old_min=old_max=0;
    old_bmin=old_bmax=Point(0,0,0);

    init=1;
}

IsoSurfaceMRSG::~IsoSurfaceMRSG()
{
}

void IsoSurfaceMRSG::execute()
{
    if(IsoSurfaceMRSG_id){
	ogeom->delObj(IsoSurfaceMRSG_id);
    }
    ScalarFieldHandle field;
    if(!infield->get(field))
	return;
    ColorMapHandle cmap;
    int have_colormap=incolormap->get(cmap);

    double min, max;
    field->get_minmax(min, max);
    if(min != old_min || max != old_max){
	ostringstream str;
	str << id << " set_minmax " << min << " " << max;
	TCL::execute(str.str().c_str());
	old_min=min;
	old_max=max;
    }
    Point bmin, bmax;
    field->get_bounds(bmin, bmax);
    sp=show_progress.get();

    double iv=isoval.get();

    double fmin = fstart.get();
    double fmax = fend.get();
    int    nf = nframes.get();
    int    doi = dointerp.get();

    cerr << fmin << " " << fmax << " min/max\n";

    GeomTimeGroup* tgroup=scinew GeomTimeGroup;
    GeomObj* topobj=tgroup;
    if(have_colormap){
	// Paint entire surface based on colormap
	topobj=scinew GeomMaterial(tgroup, cmap->lookup(iv));
    } else {
	// Default material
	topobj=scinew GeomMaterial(tgroup, matl);
    }


    ScalarFieldRG* regular_grid=field->getRG();

    if(regular_grid){
      BBox box;

      if (regular_grid->is_augmented) {
	int np=4;
	Point p[100];
	
	p[0] = (*regular_grid->aug_data.get_rep())(0,0,0);
	p[1] = (*regular_grid->aug_data.get_rep())(regular_grid->nx-1,
						   regular_grid->ny-1,
						   regular_grid->nz-1);
	
	p[2] = (*regular_grid->aug_data.get_rep())(0,
						   regular_grid->ny-1,
						   regular_grid->nz-1);
	p[3] = (*regular_grid->aug_data.get_rep())(0,
						   0,
						   regular_grid->nz-1);
	p[4] = (*regular_grid->aug_data.get_rep())(regular_grid->nx-1,
						   0,
						   regular_grid->nz-1);
	p[5] = (*regular_grid->aug_data.get_rep())(regular_grid->nx-1,
						   0,
						   0);
	p[6] = (*regular_grid->aug_data.get_rep())(regular_grid->nx-1,
						   regular_grid->ny-1,
						   0);
	
	p[7] = (*regular_grid->aug_data.get_rep())(0,
						   regular_grid->ny-1,
						   0);
	
	
	p[8] = (*regular_grid->aug_data.get_rep())(0,
						   regular_grid->ny/2,
						   regular_grid->nz/2);
	
	p[9] = (*regular_grid->aug_data.get_rep())(regular_grid->nx/2,
						   0,
						   regular_grid->nz/2);
	
	p[10] = (*regular_grid->aug_data.get_rep())(regular_grid->nx/2,
						    regular_grid->ny/2,
						    0);
	cerr << "Just before crap...\n";
	for(int i=0;i<np;i++) {
	  box.extend(p[i]);
	  cerr << p[i] << "\n";
	}

	cerr << "Augmented: ";
	
      } else {
	box.extend(bmin);
	box.extend(bmax);
      }
      
      cerr << box.min() << " " << box.max() << "\n";;

      tgroup->setbbox(box);
      
      // we have to find how many...
      ScalarFieldRG* tmp = regular_grid;
      int count=0;
      while(tmp) {
	count++;
	tmp = (ScalarFieldRG*)tmp->next;
      }

      ScalarFieldRG junk_grid;  // this is the one you blend...
      
      Array1<double> interp_times;

      int interp_index=0;

      if (doi) { // set stuff up...
	junk_grid.resize(regular_grid->nx,
			 regular_grid->ny,
			 regular_grid->nz);
	junk_grid.set_bounds(bmin,bmax);  // has to match
	interp_times.resize(nf); // resize it
	for(int i=0;i<nf;i++) {
	  interp_times[i] = fmin + i/(nf-1.0)*(fmax-fmin);
	  cerr << i << " " << interp_times[i] << "\n";
	}
      }

      tmp = regular_grid; // keep this two back...

      for(int i=0;i<count;i++) {
	if (regular_grid) {
	  double time = i/(1.0*count);
	  if ((time >= fmin) && (time <= fmax)) {
	    cerr << "Trying to do " << i << "\n";
	    if (!doi || (i == count-1)) { // nothing to blend with...
	      cerr << "What is happening???\n";
	      GeomTrianglesP* group = scinew GeomTrianglesP;
	      iso_reg_grid(regular_grid, iv, group);
	      tgroup->add(group,time);
	    } else { // do a interpolation...
	      cerr << "In Interpolation...\n";
	      while((interp_index < nf)&&
		    (interp_times[interp_index] <= (time + 1.0/count))) {
		ScalarFieldRG *work=regular_grid;

		double ts = time, te = time + 1.0/(1.0*count);

		if (time > interp_times[interp_index]) {
		  te = time;
		  ts = time - 1.0/count;
		  work = tmp; // go back one...
		  cerr << "Funky -> ";
		}
		
		double weight = count*(interp_times[interp_index]-ts);
		
		cerr << "Really interpolating:";
		cerr << interp_times[interp_index] << " ";
		cerr << ts << " " << te << " " << weight << " ";
		cerr << (void *)work << " " << (void *)work->next << "\n";
		// weight between this two fields, now blend...
		
		for(int ii=0;ii<junk_grid.nx;ii++)
		  for(int jj=0;jj<junk_grid.ny;jj++)
		    for(int kk=0;kk<junk_grid.nz;kk++) {
		      double gs = work->grid(ii,jj,kk);
		      double ge = ((ScalarFieldRG*)work->next)->grid(ii,jj,kk);
		      junk_grid.grid(ii,jj,kk) = (1-weight)*gs + weight*ge;
		    }
		GeomTrianglesP* group = scinew GeomTrianglesP;
		iso_reg_grid(&junk_grid, iv, group);
		tgroup->add(group,interp_times[interp_index]);
		++interp_index;
	      }
	    }
	  } // haven't gotten in the range yet..
	  
	  tmp = regular_grid; // 1 back...
	  regular_grid = (ScalarFieldRG*)regular_grid->next; // go to the next one...
	  
	
	} else {
	  cerr << "Fucked up: " << count << "\n";
	}
      }
    } else {
	error("I can't IsoSurfaceMRSG this type of field...");
    }

    cerr << "Done, handing off???\n";

    if(tgroup->size() == 0){
	delete tgroup;
	IsoSurfaceMRSG_id=0;
    } else {
	IsoSurfaceMRSG_id=ogeom->addObj(topobj, surface_name);
    }
}


int IsoSurfaceMRSG::iso_cube(int i, int j, int k, double isoval,
			     GeomTrianglesP* group, ScalarFieldRG* field)
{
    double oval[9];
    oval[1]=field->grid(i, j, k)-isoval;
    oval[2]=field->grid(i+1, j, k)-isoval;
    oval[3]=field->grid(i+1, j+1, k)-isoval;
    oval[4]=field->grid(i, j+1, k)-isoval;
    oval[5]=field->grid(i, j, k+1)-isoval;
    oval[6]=field->grid(i+1, j, k+1)-isoval;
    oval[7]=field->grid(i+1, j+1, k+1)-isoval;
    oval[8]=field->grid(i, j+1, k+1)-isoval;
    ov[1]=field->get_point(i,j,k);
    ov[2]=field->get_point(i+1, j, k);
    ov[3]=field->get_point(i+1, j+1, k);
    ov[4]=field->get_point(i, j+1, k);
    ov[5]=field->get_point(i, j, k+1);
    ov[6]=field->get_point(i+1, j, k+1);
    ov[7]=field->get_point(i+1, j+1, k+1);
    ov[8]=field->get_point(i, j+1, k+1);
    int mask=0;
    int idx;
    for(idx=1;idx<=8;idx++){
	if(oval[idx]<0)
	    mask|=1<<(idx-1);
    }
    MCubeTable* tab=&mcube_table[mask];
    double val[9];
    for(idx=1;idx<=8;idx++){
	val[idx]=oval[tab->permute[idx-1]];
	v[idx]=ov[tab->permute[idx-1]];
    }
    int wcase=tab->which_case;
    switch(wcase){
    case 0:
	break;
    case 1:
	{
	    Point p1(Interpolate(v[1], v[2], val[1]/(val[1]-val[2])));
	    Point p2(Interpolate(v[1], v[5], val[1]/(val[1]-val[5])));
	    Point p3(Interpolate(v[1], v[4], val[1]/(val[1]-val[4])));
	    group->add(p1, p2, p3);
	}
	break;
    case 2:
	{
	    Point p1(Interpolate(v[1], v[5], val[1]/(val[1]-val[5])));
	    Point p2(Interpolate(v[2], v[6], val[2]/(val[2]-val[6])));
	    Point p3(Interpolate(v[2], v[3], val[2]/(val[2]-val[3])));
	    (group->add(p1, p2, p3));

	    Point p4(Interpolate(v[1], v[4], val[1]/(val[1]-val[4])));
	    (group->add(p3, p4, p1));
	}
	break;
    case 3:
	{
	    Point p1(Interpolate(v[1], v[2], val[1]/(val[1]-val[2])));
	    Point p2(Interpolate(v[1], v[5], val[1]/(val[1]-val[5])));
	    Point p3(Interpolate(v[1], v[4], val[1]/(val[1]-val[4])));
	    (group->add(p1, p2, p3));

	    Point p4(Interpolate(v[3], v[2], val[3]/(val[3]-val[2])));
	    Point p5(Interpolate(v[3], v[7], val[3]/(val[3]-val[7])));
	    Point p6(Interpolate(v[3], v[4], val[3]/(val[3]-val[4])));
	    group->add(p4, p5, p6);
	}
	break;
    case 4:
	{
	    Point p1(Interpolate(v[1], v[2], val[1]/(val[1]-val[2])));
	    Point p2(Interpolate(v[1], v[5], val[1]/(val[1]-val[5])));
	    Point p3(Interpolate(v[1], v[4], val[1]/(val[1]-val[4])));
	    (group->add(p1, p2, p3));

	    Point p4(Interpolate(v[7], v[3], val[7]/(val[7]-val[3])));
	    Point p5(Interpolate(v[7], v[8], val[7]/(val[7]-val[8])));
	    Point p6(Interpolate(v[7], v[6], val[7]/(val[7]-val[6])));
	    (group->add(p4, p5, p6));

	}
	break;
    case 5:
	{
	    Point p1(Interpolate(v[2], v[1], val[2]/(val[2]-val[1])));
	    Point p2(Interpolate(v[2], v[3], val[2]/(val[2]-val[3])));
	    Point p3(Interpolate(v[5], v[1], val[5]/(val[5]-val[1])));
	    (group->add(p1, p2, p3));

	    Point p4(Interpolate(v[5], v[8], val[5]/(val[5]-val[8])));
	    (group->add(p4, p3, p2));

	    Point p5(Interpolate(v[6], v[7], val[6]/(val[6]-val[7])));
	    (group->add(p5, p4, p2));
	}
	break;
    case 6:
	{
	    Point p1(Interpolate(v[1], v[5], val[1]/(val[1]-val[5])));
	    Point p2(Interpolate(v[2], v[6], val[2]/(val[2]-val[6])));
	    Point p3(Interpolate(v[2], v[3], val[2]/(val[2]-val[3])));
	    (group->add(p1, p2, p3));

	    Point p4(Interpolate(v[1], v[4], val[1]/(val[1]-val[4])));
	    (group->add(p3, p4, p1));

	    Point p5(Interpolate(v[7], v[3], val[7]/(val[7]-val[3])));
	    Point p6(Interpolate(v[7], v[8], val[7]/(val[7]-val[8])));
	    Point p7(Interpolate(v[7], v[6], val[7]/(val[7]-val[6])));
	    (group->add(p5, p6, p7));
	}
	break;
    case 7:
	{
	    Point p1(Interpolate(v[2], v[1], val[2]/(val[2]-val[1])));
	    Point p2(Interpolate(v[2], v[3], val[2]/(val[2]-val[3])));
	    Point p3(Interpolate(v[2], v[6], val[2]/(val[2]-val[6])));
	    (group->add(p1, p2, p3));

	    Point p4(Interpolate(v[4], v[1], val[4]/(val[4]-val[1])));
	    Point p5(Interpolate(v[4], v[3], val[4]/(val[4]-val[3])));
	    Point p6(Interpolate(v[4], v[8], val[4]/(val[4]-val[8])));
	    (group->add(p4, p5, p6));

	    Point p7(Interpolate(v[7], v[8], val[7]/(val[7]-val[8])));
	    Point p8(Interpolate(v[7], v[6], val[7]/(val[7]-val[6])));
	    Point p9(Interpolate(v[7], v[3], val[7]/(val[7]-val[3])));
	    (group->add(p7, p8, p9));
	}
	break;
    case 8:
	{
	    Point p1(Interpolate(v[1], v[4], val[1]/(val[1]-val[4])));
	    Point p2(Interpolate(v[2], v[3], val[2]/(val[2]-val[3])));
	    Point p3(Interpolate(v[6], v[7], val[6]/(val[6]-val[7])));
	    (group->add(p1, p2, p3));

	    Point p4(Interpolate(v[5], v[8], val[5]/(val[5]-val[8])));
	    (group->add(p4, p1, p3));

	}
	break;
    case 9:
	{
	    Point p1(Interpolate(v[1], v[2], val[1]/(val[1]-val[2])));
	    Point p2(Interpolate(v[6], v[2], val[6]/(val[6]-val[2])));
	    Point p3(Interpolate(v[6], v[7], val[6]/(val[6]-val[7])));
	    (group->add(p1, p2, p3));

	    Point p4(Interpolate(v[8], v[7], val[8]/(val[8]-val[7])));
	    (group->add(p1, p3, p4));

	    Point p5(Interpolate(v[1], v[4], val[1]/(val[1]-val[4])));
	    (group->add(p1, p4, p5));

	    Point p6(Interpolate(v[8], v[4], val[8]/(val[8]-val[4])));
	    (group->add(p5, p4, p6));
	}
	break;
    case 10:
	{
	    Point p1(Interpolate(v[1], v[2], val[1]/(val[1]-val[2])));
	    Point p2(Interpolate(v[4], v[3], val[4]/(val[4]-val[3])));
	    Point p3(Interpolate(v[1], v[5], val[1]/(val[1]-val[5])));
	    (group->add(p1, p2, p3));

	    Point p4(Interpolate(v[4], v[8], val[4]/(val[4]-val[8])));
	    (group->add(p2, p4, p3));

	    Point p5(Interpolate(v[6], v[2], val[6]/(val[6]-val[2])));
	    Point p6(Interpolate(v[6], v[5], val[6]/(val[6]-val[5])));
	    Point p7(Interpolate(v[7], v[3], val[7]/(val[7]-val[3])));
	    (group->add(p5, p6, p7));

	    Point p8(Interpolate(v[7], v[8], val[7]/(val[7]-val[8])));
	    (group->add(p2, p8, p3));
	}
	break;
    case 11:
	{
	    Point p1(Interpolate(v[1], v[2], val[1]/(val[1]-val[2])));
	    Point p2(Interpolate(v[6], v[2], val[6]/(val[6]-val[2])));
	    Point p3(Interpolate(v[7], v[3], val[7]/(val[7]-val[3])));
	    (group->add(p1, p2, p3));

	    Point p4(Interpolate(v[5], v[8], val[5]/(val[5]-val[8])));
	    (group->add(p1, p3, p4));

	    Point p5(Interpolate(v[1], v[4], val[1]/(val[1]-val[4])));
	    (group->add(p1, p4, p5));

	    Point p6(Interpolate(v[7], v[8], val[7]/(val[7]-val[8])));
	    (group->add(p4, p3, p6));

	}
	break;
    case 12:
	{
	    Point p1(Interpolate(v[2], v[1], val[2]/(val[2]-val[1])));
	    Point p2(Interpolate(v[2], v[3], val[2]/(val[2]-val[3])));
	    Point p3(Interpolate(v[5], v[1], val[5]/(val[5]-val[1])));
	    (group->add(p1, p2, p3));

	    Point p4(Interpolate(v[5], v[8], val[5]/(val[5]-val[8])));
	    (group->add(p3, p2, p4));

	    Point p5(Interpolate(v[6], v[7], val[6]/(val[6]-val[7])));
	    (group->add(p4, p2, p5));

	    Point p6(Interpolate(v[4], v[1], val[4]/(val[4]-val[1])));
	    Point p7(Interpolate(v[4], v[3], val[4]/(val[4]-val[3])));
	    Point p8(Interpolate(v[4], v[8], val[4]/(val[4]-val[8])));
	    (group->add(p6, p7, p8));

	}
	break;
    case 13:
	{
	    Point p1(Interpolate(v[1], v[2], val[1]/(val[1]-val[2])));
	    Point p2(Interpolate(v[1], v[5], val[1]/(val[1]-val[5])));
	    Point p3(Interpolate(v[1], v[4], val[1]/(val[1]-val[4])));
	    (group->add(p1, p2, p3));

	    Point p4(Interpolate(v[3], v[2], val[3]/(val[3]-val[2])));
	    Point p5(Interpolate(v[3], v[7], val[3]/(val[3]-val[7])));
	    Point p6(Interpolate(v[3], v[4], val[3]/(val[3]-val[4])));
	    (group->add(p4, p5, p6));

	    Point p7(Interpolate(v[6], v[2], val[6]/(val[6]-val[2])));
	    Point p8(Interpolate(v[6], v[7], val[6]/(val[6]-val[7])));
	    Point p9(Interpolate(v[6], v[5], val[6]/(val[6]-val[5])));
	    (group->add(p7, p8, p9));

	    Point p10(Interpolate(v[8], v[5], val[8]/(val[8]-val[5])));
	    Point p11(Interpolate(v[8], v[7], val[8]/(val[8]-val[7])));
	    Point p12(Interpolate(v[8], v[4], val[8]/(val[8]-val[4])));
	    (group->add(p10, p11, p12));
	}
	break;
    case 14:
	{
	    Point p1(Interpolate(v[2], v[1], val[2]/(val[2]-val[1])));
	    Point p2(Interpolate(v[2], v[3], val[2]/(val[2]-val[3])));
	    Point p3(Interpolate(v[6], v[7], val[6]/(val[6]-val[7])));
	    (group->add(p1, p2, p3));

	    Point p4(Interpolate(v[8], v[4], val[8]/(val[8]-val[4])));
	    (group->add(p1, p3, p4));

	    Point p5(Interpolate(v[5], v[1], val[5]/(val[5]-val[1])));
	    (group->add(p1, p4, p5));

	    Point p6(Interpolate(v[8], v[7], val[8]/(val[8]-val[7])));
	    (group->add(p3, p6, p4));
	}
	break;
    default:
	error("Bad case in marching cubes!\n");
	break;
    }
    return(tab->nbrs);
}

void IsoSurfaceMRSG::iso_reg_grid(ScalarFieldRG* field, double isoval,
				  GeomTrianglesP* group)
{
    int nx=field->nx;
    int ny=field->ny;
    int nz=field->nz;
    for(int i=0;i<nx-1;i++){
      update_progress(i, nx);
	for(int j=0;j<ny-1;j++){
	    for(int k=0;k<nz-1;k++){
		iso_cube(i,j,k, isoval, group, field);
	    }
	    if(sp && abort_flag)
		return;
	}
    }
}

} // End namespace SCIRun

