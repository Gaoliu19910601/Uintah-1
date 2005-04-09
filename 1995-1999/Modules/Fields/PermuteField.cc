/*
 *  PermuteField.cc:  Rotate and flip field to get it into "standard" view
 *
 *  Written by:
 *   David Weinstein
 *   Department of Computer Science
 *   University of Utah
 *   December 1995
 *
 *  Copyright (C) 1995 SCI Group
 */

#include <Classlib/NotFinished.h>
#include <Classlib/String.h>
#include <Dataflow/Module.h>
#include <Datatypes/ScalarFieldRGdouble.h>
#include <Datatypes/ScalarFieldRGfloat.h>
#include <Datatypes/ScalarFieldRGint.h>
#include <Datatypes/ScalarFieldRGshort.h>
#include <Datatypes/ScalarFieldRGuchar.h>
#include <Datatypes/ScalarFieldPort.h>
#include <Malloc/Allocator.h>
#include <Math/MiscMath.h>
#include <TCL/TCLTask.h>
#include <TCL/TCLvar.h>
#include <TCL/TCL.h>
#include <tcl/tcl/tcl.h>
#include <tcl/tk/tk.h>
#include <iostream.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

class PermuteField : public Module {
    ScalarFieldIPort *iport;
    ScalarFieldOPort *oport;
    ScalarFieldHandle sfOH;		// output bitFld
    ScalarFieldHandle last_sfIH;	// last input fld

    TCLstring xmap;
    TCLstring ymap;
    TCLstring zmap;

    int xxmap;
    int yymap;
    int zzmap;

    int tcl_execute;
public:
    PermuteField(const clString& id);
    PermuteField(const PermuteField&, int deep);
    virtual ~PermuteField();
    virtual Module* clone(int deep);
    virtual void execute();
    void set_str_vars();
    void tcl_command( TCLArgs&, void * );
    clString makeAbsMapStr();
};

extern "C" {
Module* make_PermuteField(const clString& id)
{
    return scinew PermuteField(id);
}
}

PermuteField::PermuteField(const clString& id)
: Module("PermuteField", id, Source), tcl_execute(0),
  xmap("xmap", id, this), ymap("ymap", id, this), zmap("zmap", id, this),
  xxmap(1), yymap(2), zzmap(3)
{
    // Create the input port
    iport = scinew ScalarFieldIPort(this, "SFRG", ScalarFieldIPort::Atomic);
    add_iport(iport);
    oport = scinew ScalarFieldOPort(this, "SFRG",ScalarFieldIPort::Atomic);
    add_oport(oport);
}

PermuteField::PermuteField(const PermuteField& copy, int deep)
: Module(copy, deep), tcl_execute(0),
  xmap("xmap", id, this), ymap("ymap", id, this), zmap("zmap", id, this),
  xxmap(1), yymap(2), zzmap(3)
{
    NOT_FINISHED("PermuteField::PermuteField");
}

PermuteField::~PermuteField()
{
}

Module* PermuteField::clone(int deep)
{
    return scinew PermuteField(*this, deep);
}

void PermuteField::execute()
{
    ScalarFieldHandle sfIH;
    iport->get(sfIH);
    if (!sfIH.get_rep()) return;
    if (!tcl_execute && (sfIH.get_rep() == last_sfIH.get_rep())) return;
    ScalarFieldRGBase *sfrgb;
    if ((sfrgb=sfIH->getRGBase()) == 0) return;

    ScalarFieldRGdouble *ifd, *ofd;
    ScalarFieldRGfloat *iff, *off;
    ScalarFieldRGint *ifi, *ofi;
    ScalarFieldRGshort *ifs, *ofs;
    ScalarFieldRGuchar *ifc, *ofc;

    ScalarFieldRGBase *ofb;

    ifd=sfrgb->getRGDouble();
    iff=sfrgb->getRGFloat();
    ifi=sfrgb->getRGInt();
    ifs=sfrgb->getRGShort();
    ifc=sfrgb->getRGUchar();

    ofd=0;
    off=0;
    ofs=0;
    ofi=0;
    ofc=0;

    if (sfIH.get_rep() != last_sfIH.get_rep()) {	// new field came in
	int nx=sfrgb->nx;
	int ny=sfrgb->ny;
	int nz=sfrgb->nz;
	Point min;
	Point max;
	sfrgb->get_bounds(min, max);
	clString map=makeAbsMapStr();

	int basex, basey, basez, incrx, incry, incrz;
	if (xxmap>0) {basex=0; incrx=1;} else {basex=nx-1; incrx=-1;}
	if (yymap>0) {basey=0; incry=1;} else {basey=ny-1; incry=-1;}
	if (zzmap>0) {basez=0; incrz=1;} else {basez=nz-1; incrz=-1;}

 	if (map=="123") {
	    if (ifd) {
		ofd=scinew ScalarFieldRGdouble(); 
		ofd->resize(nx,ny,nz);
		ofb=ofd;
	    } else if (iff) {
		off=scinew ScalarFieldRGfloat(); 
		off->resize(nx,ny,nz);
		ofb=off;
	    } else if (ifi) {
		ofi=scinew ScalarFieldRGint(); 
		ofi->resize(nx,ny,nz);
		ofb=ofi;
	    } else if (ifs) {
		ofs=scinew ScalarFieldRGshort(); 
		ofs->resize(nx,ny,nz);
		ofb=ofs;
	    } else if (ifc) {
		ofc=scinew ScalarFieldRGuchar(); 
		ofc->resize(nx,ny,nz);
		ofb=ofc;
	    }
	    ofb->set_bounds(Point(min.x(), min.y(), min.z()), 
			    Point(max.x(), max.y(), max.z()));
	    for (int i=0, ii=basex; i<nx; i++, ii+=incrx)
		for (int j=0, jj=basey; j<ny; j++, jj+=incry)
		    for (int k=0, kk=basez; k<nz; k++, kk+=incrz)
			if (ofd) ofd->grid(i,j,k)=ifd->grid(ii,jj,kk);
			else if (off) off->grid(i,j,k)=iff->grid(ii,jj,kk);
			else if (ofi) ofi->grid(i,j,k)=ifi->grid(ii,jj,kk);
			else if (ofs) ofs->grid(i,j,k)=ifs->grid(ii,jj,kk);
			else if (ofc) ofc->grid(i,j,k)=ifc->grid(ii,jj,kk);
	} else if (map=="132") {
	    if (ifd) {
		ofd=scinew ScalarFieldRGdouble(); 
		ofd->resize(nx,nz,ny);
		ofb=ofd;
	    } else if (iff) {
		off=scinew ScalarFieldRGfloat(); 
		off->resize(nx,nz,ny);
		ofb=off;
	    } else if (ifi) {
		ofi=scinew ScalarFieldRGint(); 
		ofi->resize(nx,nz,ny);
		ofb=ofi;
	    } else if (ifs) {
		ofs=scinew ScalarFieldRGshort(); 
		ofs->resize(nx,nz,ny);
		ofb=ofs;
	    } else if (ifc) {
		ofc=scinew ScalarFieldRGuchar(); 
		ofc->resize(nx,nz,ny);
		ofb=ofc;
	    }
	    ofb->set_bounds(Point(min.x(), min.z(), min.y()), 
			    Point(max.x(), max.z(), max.y()));
	    for (int i=0, ii=basex; i<nx; i++, ii+=incrx)
		for (int j=0, jj=basey; j<ny; j++, jj+=incry)
		    for (int k=0, kk=basez; k<nz; k++, kk+=incrz)
			if (ofd) ofd->grid(i,k,j)=ifd->grid(ii,jj,kk);
			else if (off) off->grid(i,k,j)=iff->grid(ii,jj,kk);
			else if (ofi) ofi->grid(i,k,j)=ifi->grid(ii,jj,kk);
			else if (ofs) ofs->grid(i,k,j)=ifs->grid(ii,jj,kk);
			else if (ofc) ofc->grid(i,k,j)=ifc->grid(ii,jj,kk);
	} else if (map=="213") {
	    if (ifd) {
		ofd=scinew ScalarFieldRGdouble(); 
		ofd->resize(ny,nx,nz);
		ofb=ofd;
	    } else if (iff) {
		off=scinew ScalarFieldRGfloat(); 
		off->resize(ny,nx,nz);
		ofb=off;
	    } else if (ifi) {
		ofi=scinew ScalarFieldRGint(); 
		ofi->resize(ny,nx,nz);
		ofb=ofi;
	    } else if (ifs) {
		ofs=scinew ScalarFieldRGshort(); 
		ofs->resize(ny,nx,nz);
		ofb=ofs;
	    } else if (ifc) {
		ofc=scinew ScalarFieldRGuchar(); 
		ofc->resize(ny,nx,nz);
		ofb=ofc;
	    }
	    ofb->set_bounds(Point(min.y(), min.x(), min.z()), 
			    Point(max.y(), max.x(), max.z()));
	    for (int i=0, ii=basex; i<nx; i++, ii+=incrx)
		for (int j=0, jj=basey; j<ny; j++, jj+=incry)
		    for (int k=0, kk=basez; k<nz; k++, kk+=incrz)
			if (ofd) ofd->grid(i,j,k)=ifd->grid(jj,ii,kk);
			else if (off) off->grid(j,i,k)=iff->grid(ii,jj,kk);
			else if (ofi) ofi->grid(j,i,k)=ifi->grid(ii,jj,kk);
			else if (ofs) ofs->grid(j,i,k)=ifs->grid(ii,jj,kk);
			else if (ofc) ofc->grid(j,i,k)=ifc->grid(ii,jj,kk);
	} else if (map=="231") {
	    if (ifd) {
		ofd=scinew ScalarFieldRGdouble(); 
		ofd->resize(ny,nz,nx);
		ofb=ofd;
	    } else if (iff) {
		off=scinew ScalarFieldRGfloat(); 
		off->resize(ny,nz,nx);
		ofb=off;
	    } else if (ifi) {
		ofi=scinew ScalarFieldRGint(); 
		ofi->resize(ny,nz,nx);
		ofb=ofi;
	    } else if (ifs) {
		ofs=scinew ScalarFieldRGshort(); 
		ofs->resize(ny,nz,nx);
		ofb=ofs;
	    } else if (ifc) {
		ofc=scinew ScalarFieldRGuchar(); 
		ofc->resize(ny,nz,nx);
		ofb=ofc;
	    }
	    ofb->set_bounds(Point(min.y(), min.z(), min.x()), 
			    Point(max.y(), max.z(), max.x()));
	    for (int i=0, ii=basex; i<nx; i++, ii+=incrx)
		for (int j=0, jj=basey; j<ny; j++, jj+=incry)
		    for (int k=0, kk=basez; k<nz; k++, kk+=incrz)
			if (ofd) ofd->grid(j,k,i)=ifd->grid(ii,jj,kk);
			else if (off) off->grid(j,k,i)=iff->grid(ii,jj,kk);
			else if (ofi) ofi->grid(j,k,i)=ifi->grid(ii,jj,kk);
			else if (ofs) ofs->grid(j,k,i)=ifs->grid(ii,jj,kk);
			else if (ofc) ofc->grid(j,k,i)=ifc->grid(ii,jj,kk);
	} else if (map=="312") {
	    if (ifd) {
		ofd=scinew ScalarFieldRGdouble();
		ofd->resize(nz,nx,ny);
		ofb=ofd;
	    } else if (iff) {
		off=scinew ScalarFieldRGfloat(); 
		off->resize(nz,nx,ny);
		ofb=off;
	    } else if (ifi) {
		ofi=scinew ScalarFieldRGint(); 
		ofi->resize(nz,nx,ny);
		ofb=ofi;
	    } else if (ifs) {
		ofs=scinew ScalarFieldRGshort(); 
		ofs->resize(nz,nx,ny);
		ofb=ofs;
	    } else if (ifc) {
		ofc=scinew ScalarFieldRGuchar(); 
		ofc->resize(nz,nx,ny);
		ofb=ofc;
	    }
	    ofb->set_bounds(Point(min.z(), min.x(), min.y()), 
			    Point(max.z(), max.x(), max.y()));
	    for (int i=0, ii=basex; i<nx; i++, ii+=incrx)
		for (int j=0, jj=basey; j<ny; j++, jj+=incry)
		    for (int k=0, kk=basez; k<nz; k++, kk+=incrz)
			if (ofd) ofd->grid(k,i,j)=ifd->grid(ii,jj,kk);
			else if (off) off->grid(k,i,j)=iff->grid(ii,jj,kk);
			else if (ofi) ofi->grid(k,i,j)=ifi->grid(ii,jj,kk);
			else if (ofs) ofs->grid(k,i,j)=ifs->grid(ii,jj,kk);
			else if (ofc) ofc->grid(k,i,j)=ifc->grid(ii,jj,kk);
	} else if (map=="321") {
	    if (ifd) {
		ofd=scinew ScalarFieldRGdouble(); 
		ofd->resize(nz,ny,nx);
		ofb=ofd;
	    } else if (iff) {
		off=scinew ScalarFieldRGfloat(); 
		off->resize(nz,ny,nx);
		ofb=off;
	    } else if (ifi) {
		ofi=scinew ScalarFieldRGint(); 
		ofi->resize(nz,ny,nx);
		ofb=ofi;
	    } else if (ifs) {
		ofs=scinew ScalarFieldRGshort(); 
		ofs->resize(nz,ny,nx);
		ofb=ofs;
	    } else if (ifc) {
		ofc=scinew ScalarFieldRGuchar(); 
		ofc->resize(nz,ny,nx);
		ofb=ofc;
	    }
	    ofb->set_bounds(Point(min.z(), min.y(), min.x()), 
			    Point(max.z(), max.y(), max.x()));
	    for (int i=0, ii=basex; i<nx; i++, ii+=incrx)
		for (int j=0, jj=basey; j<ny; j++, jj+=incry)
		    for (int k=0, kk=basez; k<nz; k++, kk+=incrz)
			if (ofd) ofd->grid(k,j,i)=ifd->grid(kk,jj,ii);
			else if (off) off->grid(k,j,i)=iff->grid(kk,jj,ii);
			else if (ofi) ofi->grid(k,j,i)=ifi->grid(kk,jj,ii);
			else if (ofs) ofs->grid(k,j,i)=ifs->grid(kk,jj,ii);
			else if (ofc) ofc->grid(k,j,i)=ifc->grid(kk,jj,ii);

//	    outFld->resize(nz,ny,nx);
//	    outFld->grid(i,j,k)=sfrg->grid(kk,jj,ii);

	} else {
	    cerr << "ERROR: PermuteField::execute() doesn't recognize map code: "<<map<<"\n";
	}
	sfOH=ofb;
    }
    oport->send(sfOH);
    tcl_execute=0;
}
	    
void PermuteField::set_str_vars() {
    if (xxmap==1) xmap.set("x <- x+");
    if (xxmap==-1) xmap.set("x <- x-");
    if (xxmap==2) xmap.set("x <- y+");
    if (xxmap==-2) xmap.set("x <- y-");
    if (xxmap==3) xmap.set("x <- z+");
    if (xxmap==-3) xmap.set("x <- z-");
    if (yymap==1) ymap.set("y <- x+");
    if (yymap==-1) ymap.set("y <- x-");
    if (yymap==2) ymap.set("y <- y+");
    if (yymap==-2) ymap.set("y <- y-");
    if (yymap==3) ymap.set("y <- z+");
    if (yymap==-3) ymap.set("y <- z-");
    if (zzmap==1) zmap.set("z <- x+");
    if (zzmap==-1) zmap.set("z <- x-");
    if (zzmap==2) zmap.set("z <- y+");
    if (zzmap==-2) zmap.set("z <- y-");
    if (zzmap==3) zmap.set("z <- z+");
    if (zzmap==-3) zmap.set("z <- z-");
}

clString PermuteField::makeAbsMapStr() {
    return to_string(Abs(xxmap))+to_string(Abs(yymap))+to_string(Abs(zzmap));
}

void PermuteField::tcl_command(TCLArgs& args, void* userdata) {
    if (args[1] == "send") {
	tcl_execute=1;
	want_to_execute();
    } else if (args[1] == "flipx") {
	reset_vars();
	xxmap*=-1;
	set_str_vars();
	last_sfIH=0;
	want_to_execute();
    } else if (args[1] == "flipy") {
	reset_vars();
	yymap*=-1;
	set_str_vars();
	last_sfIH=0;
	want_to_execute();
    } else if (args[1] == "flipz") {
	reset_vars();
	zzmap*=-1;
	set_str_vars();
	last_sfIH=0;
	want_to_execute();
    } else if (args[1] == "cyclePos") {
	reset_vars();
	int tmp=xxmap;
	xxmap=yymap;
	yymap=zzmap;
	zzmap=tmp;
	set_str_vars();
	last_sfIH=0;
	want_to_execute();
    } else if (args[1] == "cycleNeg") {
	reset_vars();
	int tmp=zzmap;
	zzmap=yymap;
	yymap=xxmap;
	xxmap=tmp;
	set_str_vars();
	last_sfIH=0;
	want_to_execute();
    } else if (args[1] == "reset") {
	reset_vars();
	xxmap=1;
	yymap=2;
	zzmap=3;
	set_str_vars();
	last_sfIH=0;
	want_to_execute();
    } else if (args[1] == "swapXY") {
	reset_vars();
	int tmp=xxmap;
	xxmap=yymap;
	yymap=tmp;
	set_str_vars();
	last_sfIH=0;
	want_to_execute();
    } else if (args[1] == "swapYZ") {
	reset_vars();
	int tmp=yymap;
	yymap=zzmap;
	zzmap=tmp;
	set_str_vars();
	last_sfIH=0;
	want_to_execute();
    } else if (args[1] == "swapXZ") {
	reset_vars();
	int tmp=xxmap;
	xxmap=zzmap;
	zzmap=tmp;
	set_str_vars();
	last_sfIH=0;
	want_to_execute();
    } else {
        Module::tcl_command(args, userdata);
    }
}
