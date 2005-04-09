/*
 *  Streamline.cc:  Generate streamlines from a field...
 *
 *  Written by:
 *   Steven G. Parker
 *   Department of Computer Science
 *   University of Utah
 *   March 1994
 *
 *  Copyright (C) 1994 SCI Group
 */

#include <Classlib/NotFinished.h>
#include <Math/Trig.h>
#include <Dataflow/Module.h>
#include <Dataflow/ModuleList.h>
#include <Datatypes/ColormapPort.h>
#include <Datatypes/GeometryPort.h>
#include <Datatypes/ScalarField.h>
#include <Datatypes/ScalarFieldRG.h>
#include <Datatypes/ScalarFieldUG.h>
#include <Datatypes/ScalarFieldPort.h>
#include <Datatypes/VectorField.h>
#include <Datatypes/VectorFieldPort.h>
#include <Geom/Cylinder.h>
#include <Geom/Disc.h>
#include <Geom/Geom.h>
#include <Geom/Material.h>
#include <Geom/TriStrip.h>
#include <Geom/Group.h>
#include <Geom/Pick.h>
#include <Geom/Polyline.h>
#include <Geom/Sphere.h>
#include <Geom/VCTriStrip.h>
#include <Geometry/Point.h>
#include <TCL/TCLvar.h>

#include <iostream.h>

class Streamline : public Module {
    VectorFieldIPort* infield;
    ColormapIPort* incolormap;
    ScalarFieldIPort* incolorfield;
    GeometryOPort* ogeom;

    TCLstring widgettype;
    clString oldwidgettype;
    TCLstring markertype;
    TCLdouble lineradius;
    TCLstring algorithm;

    TCLvardouble stepsize;
    TCLvarint maxsteps;

    TCLdouble range_min;
    TCLdouble range_max;

    int need_p1;
    Point p1;
    Point p2;
    Point p3;
    Point p4;
    double slider1_dist;
    double slider2_dist;
    TCLPoint sp1;
    TCLPoint sp2;
    TCLdouble ring_radius;
    TCLdouble sdist1;

    GeomGroup* widget;
    GeomSphere* widget_p1;
    GeomSphere* widget_p2;
    GeomSphere* widget_p3;
    GeomSphere* widget_p4;
    GeomCylinder* widget_edge1;
    GeomCylinder* widget_edge2;
    GeomCylinder* widget_edge3;
    GeomCylinder* widget_edge4;
    GeomGroup* widget_slider1;
    GeomGroup* widget_slider2;
    GeomCylinder* widget_slider1body;
    GeomCylinder* widget_slider2body;
    GeomDisc* widget_slider1cap1;
    GeomDisc* widget_slider1cap2;
    GeomDisc* widget_slider2cap1;
    GeomDisc* widget_slider2cap2;
    GeomPick* pick_p1;
    GeomPick* pick_p2;
    GeomPick* pick_p3;
    GeomPick* pick_p4;
    GeomPick* pick_edge1;
    GeomPick* pick_edge2;
    GeomPick* pick_edge3;
    GeomPick* pick_edge4;
    GeomPick* pick_slider1;
    GeomPick* pick_slider2;
    int widget_id;
    double widget_scale;

    double old_smin, old_smax;

    int streamline_id;

    MaterialHandle widget_point_matl;
    MaterialHandle widget_edge_matl;
    MaterialHandle widget_slider_matl;
    MaterialHandle widget_highlight_matl;
    MaterialHandle matl;

    virtual void geom_moved(int, double, const Vector&, void*);
    virtual void geom_release(void*);
public:
    Streamline(const clString& id);
    Streamline(const Streamline&, int deep);
    virtual ~Streamline();
    virtual Module* clone(int deep);
    virtual void execute();
};

static Module* make_Streamline(const clString& id)
{
    return new Streamline(id);
}

static RegisterModule db1("Fields", "Streamline", make_Streamline);
static RegisterModule db2("Visualization", "Streamline", make_Streamline);

static clString widget_name("Streamline Widget");
static clString streamline_name("Streamline");

class SLine {
    Point p;
    GeomPolyline* line;
    int outside;
public:
    SLine(GeomGroup*, const Point&);
    int advance(const VectorFieldHandle&, int rk4, double);
};

// ---------------------------------------------------
class SRibbon                // Stream Ribbon class
{
    Point p;                // initial point
    Point lp, rp;           // left and right tracer 
    Vector perturb;         // initial perturb vector
    GeomTriStrip* tri;      // triangle trip
    GeomVCTriStrip* vtri;   // Vertex colored tri strip
    double width;           // the fixed width of Ribbons
    int outside;            
    int first;
public:
    SRibbon(GeomGroup*, const Point&, double, const Vector&, int); 
    int advance(const VectorFieldHandle&, int rk4, double,
		const ScalarFieldHandle&, const ColormapHandle&,
		int, const MaterialHandle&, double, double); 
}; 

SRibbon::SRibbon(GeomGroup* group, const Point& p, double w, 
		 const Vector& ptb, int have_sfield)
: p(p), perturb(ptb), width(w), first(1)
{
    if(!have_sfield){
	tri=new GeomTriStrip;
	group->add(tri);
	vtri=0;
    } else {
	vtri=new GeomVCTriStrip;
	group->add(vtri);
	tri=0;
    }
    outside=0;    
    lp = p + perturb; 
    rp = p - perturb;    
}

int SRibbon::advance(const VectorFieldHandle& field, int rk4,
		     double stepsize, const ScalarFieldHandle& sfield,
		     const ColormapHandle& cmap, int have_sfield,
		     const MaterialHandle& outmatl,
		     double smin, double smax)
{
    if(first){
	Vector v1;
	if(!field->interpolate(lp, v1)){
	    outside=1;
	    return 0;
	}

	Vector v2;
	if(!field->interpolate(rp, v2)){
	    outside=1;
	    return 0;
	}
	Vector tmp(lp-rp);
	if(tmp.length2() < 1.e-6){
	    outside=1;
	    return 0;
	}
	Vector diag ( tmp.normal() * width ); // only want fixed sized Ribbon
	diag = stepsize<0?-diag:diag;
	Point p1 = p +  diag;  
	Point p2 = p -  diag; 
	Vector n1 ( Cross(v1, diag) ); // the normals here are not percise,
	Vector n2 ( Cross(v2, diag) ); // need to be improved .....

	if(!have_sfield){
	    tri->add(p1, n1); tri->add(p2, n2); 
	} else {
	    double sval;
	    if(sfield->interpolate(p1, sval)){
		MaterialHandle matl(cmap->lookup(sval, smin, smax));
		vtri->add(p1, n1, matl);
		vtri->add(p2, n2, matl);
	    } else {
		vtri->add(p1, n1, outmatl);
		vtri->add(p2, n2, outmatl);
	    }
	}
	first=0;
    }

    if(outside)
	return 0;
    if(rk4){
	    NOT_FINISHED("SRibbon::advance for RK4");
    } else {
        Vector v1, v2; 
        Vector tmp, diag, n1, n2;

	if(!field->interpolate(lp, v1)){
	    outside=1;
	    return 0;
	}
	lp+=(v1*stepsize);  // trace the left point

	if(!field->interpolate(rp, v2)){
	    outside=1;
	    return 0;
	}
	rp+=(v2*stepsize);  // trace the right point
 
        tmp = lp-rp; 
        p = lp + tmp/2.0;  // define the center point        
	if(tmp.length2() < 1.e-6){
	    outside=1;
	    return 0;
	}
        diag = tmp.normal() * width; // only want fixed sized Ribbon
	diag = stepsize<0?-diag:diag;
        Point p1 = p +  diag;  
        Point p2 = p -  diag; 
        n1 = Cross(v1, diag); // the normals here are not percise,
        n2 = Cross(v2, diag); // need to be improved .....

	if(!have_sfield){
	    tri->add(p1, n1); tri->add(p2, n2); 
	} else {
	    double sval;
	    if(sfield->interpolate(p1, sval)){
		MaterialHandle matl(cmap->lookup(sval, smin, smax));
//		vtri->add(p1, n1, matl);
//		vtri->add(p2, n2, matl);
		vtri->add(p1, -n1, matl);
		vtri->add(p2, -n2, matl);
	    } else {
		vtri->add(p1, n1, outmatl);
		vtri->add(p2, n2, outmatl);
	    }
	}
    }
    return 1;
}

// ------------------------------------------------------


Streamline::Streamline(const clString& id)
: Module(streamline_name, id, Filter),
  widgettype("source", id, this),
  markertype("markertype", id, this), lineradius("lineradius", id, this),
  algorithm("algorithm", id, this), stepsize("stepsize", id, this),
  sp1("sp1", id, this), sp2("sp2", id, this),
  ring_radius("ring_radius", id, this), 
  sdist1("sdist1", id, this),
  maxsteps("maxsteps", id, this), range_min("range_min", id, this),
  range_max("range_max", id, this)
{
    // Create the input ports
    infield=new VectorFieldIPort(this, "Vector Field", ScalarFieldIPort::Atomic);
    add_iport(infield);
    incolorfield=new ScalarFieldIPort(this, "Color Field", ScalarFieldIPort::Atomic);
    add_iport(incolorfield);
    incolormap=new ColormapIPort(this, "Colormap", ColormapIPort::Atomic);
    add_iport(incolormap);

    // Create the output port
    ogeom=new GeometryOPort(this, "Geometry", GeometryIPort::Atomic);
    add_oport(ogeom);

    need_p1=0;
    p1=Point(-70,0,30);
    p2=Point(-90,0,30);
    slider1_dist=.5;
    widget_point_matl=new Material(Color(0,0,0), Color(.54, .60, 1),
				   Color(.5,.5,.5), 20);
    widget_edge_matl=new Material(Color(0,0,0), Color(.54, .60, .66),
				  Color(.5,.5,.5), 20);
    widget_slider_matl=new Material(Color(0,0,0), Color(.83, .60, .66),
				    Color(.5,.5,.5), 20);
    widget_highlight_matl=new Material(Color(0,0,0), Color(.7,.7,.7),
				       Color(0,0,.6), 20);
    matl=new Material(Color(0,0,0), Color(0,0,.6),
			  Color(0,0,0.5), 20);
    widget_id=0;
    streamline_id=0;

    old_smin=old_smax=0;
}

Streamline::Streamline(const Streamline& copy, int deep)
: Module(copy, deep),
  widgettype("source", id, this),
  markertype("markertype", id, this), lineradius("lineradius", id, this),
  algorithm("algorithm", id, this), stepsize("stepsize", id, this),
  sp1("sp1", id, this), sp2("sp2", id, this),
  ring_radius("ring_radius", id, this), 
  sdist1("sdist1", id, this),
  maxsteps("maxsteps", id, this), range_min("range_min", id, this),
  range_max("range_max", id, this)
{
    NOT_FINISHED("Streamline::Streamline");
}

Streamline::~Streamline()
{
}

Module* Streamline::clone(int deep)
{
    return new Streamline(*this, deep);
}

void Streamline::execute()
{
    if(streamline_id)
	cerr << "Deleting streamline: " << streamline_id << endl;
    if(streamline_id)
	ogeom->delObj(streamline_id);
    VectorFieldHandle field;
    if(!infield->get(field))
	return;
    ScalarFieldHandle sfield;
    int have_sfield=incolorfield->get(sfield);
    ColormapHandle cmap;
    int have_cmap=incolormap->get(cmap);
    if(have_sfield && !have_cmap)
	have_sfield=0;
    double smin, smax;
    if(have_sfield){
	sfield->get_minmax(smin, smax);
	if(old_smin != smin || old_smax != smax){
	    range_min.set(smin);
	    range_max.set(smax);
	    old_smin=smin;
	    old_smax=smax;
	}
	smin=range_min.get();
	smax=range_max.get();
    }
    smin=range_min.get();
    smax=range_max.get();
    MaterialHandle outmatl(new Material(Color(0,0,0), Color(1,1,1), Color(1,1,1), 10.0));
    double linerad=lineradius.get();
    p1=sp1.get();
    p2=sp2.get();
    slider1_dist=sdist1.get();
    if(need_p1){
	Point min, max;
	field->get_bounds(min, max);
	Point cen=Interpolate(min, max, 0.5);
	double s=field->longest_dimension();
	p1=cen-Vector(1,0,0)*(s/5);
	p2=cen+Vector(1,0,0)*(s/5);
	p3=cen+Vector(0,1,0)*(s/10);
	p4=cen+Vector(1,1,0)*(s/10);
	Vector sp(p2-p1);
	slider1_dist=slider2_dist=sp.length()/10;
	need_p1=0;
	sp1.set(p1);
	sp2.set(p2);
	sdist1.set(slider1_dist);
    }
    widget_scale=0.05*field->longest_dimension();
    if(widgettype.get() != oldwidgettype){
	oldwidgettype=widgettype.get();
	if(widget_id)
	    cerr << "Deleting widget... (id=" << widget_id << ")" << endl;
	if(widget_id)
	    ogeom->delObj(widget_id);
	widget=new GeomGroup;
	if(widgettype.get() == "Point"){
	    widget_p1=new GeomSphere(p1, 1*widget_scale);
	    GeomMaterial* matl=new GeomMaterial(widget_p1, widget_point_matl);
	    GeomPick* pick=new GeomPick(matl, this, Vector(1,0,0),
					Vector(0,1,0),
					Vector(0,0,1));
	    pick->set_highlight(widget_highlight_matl);
	    widget->add(pick);
	} else if(widgettype.get() == "Ring"){
	    // NOTHING TO DO -- too lazy to build a widget...
	} else if(widgettype.get() == "Line"){
	    GeomGroup* pts=new GeomGroup;
	    widget_p1=new GeomSphere(p1, 1*widget_scale);
	    pick_p1=new GeomPick(widget_p1, this);
	    pick_p1->set_highlight(widget_highlight_matl);
	    pick_p1->set_cbdata((void*)1);
	    pts->add(pick_p1);
	    widget_p2=new GeomSphere(p2, 1*widget_scale);
	    pick_p2=new GeomPick(widget_p2, this);
	    pick_p2->set_highlight(widget_highlight_matl);
	    pick_p2->set_cbdata((void*)2);
	    pts->add(pick_p2);
	    GeomMaterial* m1=new GeomMaterial(pts, widget_point_matl);
	    widget_edge1=new GeomCylinder(p1, p2, 0.5*widget_scale);
	    pick_edge1=new GeomPick(widget_edge1, this);
	    pick_edge1->set_highlight(widget_highlight_matl);
	    pick_edge1->set_cbdata((void*)3);
	    GeomMaterial* m2=new GeomMaterial(pick_edge1, widget_edge_matl);
	    widget_slider1=new GeomGroup;
	    Vector spvec(p2-p1);
	    spvec.normalize();
	    Point sp(p1+spvec*slider1_dist);
	    Point sp2(sp+spvec*(widget_scale*0.5));
	    Point sp1(sp-spvec*(widget_scale*0.5));
	    widget_slider1body=new GeomCylinder(sp1, sp2, 1*widget_scale);
	    widget_slider1cap1=new GeomDisc(sp2, spvec, 1*widget_scale);
	    widget_slider1cap2=new GeomDisc(sp1, -spvec, 1*widget_scale);
	    widget_slider1->add(widget_slider1body);
	    widget_slider1->add(widget_slider1cap1);
	    widget_slider1->add(widget_slider1cap2);
 	    pick_slider1=new GeomPick(widget_slider1, this);
 	    pick_slider1->set_highlight(widget_highlight_matl);
 	    pick_slider1->set_cbdata((void*)4);
 	    GeomMaterial* m3=new GeomMaterial(pick_slider1, widget_slider_matl);
 	    widget->add(m1);
 	    widget->add(m2);
 	    widget->add(m3);
	    Vector v1,v2;
	    spvec.find_orthogonal(v1, v2);
 	    pick_p1->set_principal(spvec, v1, v2);
 	    pick_p2->set_principal(spvec, v1, v2);
 	    pick_edge1->set_principal(spvec, v1, v2);
 	    pick_slider1->set_principal(spvec);
	} else if(widgettype.get() == "Square"){
	    NOT_FINISHED("Square widget");
	} else {
	    error("Unknown widget type: "+widgettype.get());
	}
	widget_id=ogeom->addObj(widget, widget_name);
    }

    // Upedate the widget...
    if(widgettype.get() == "Point"){
	widget_p1->move(p1, 1*widget_scale);
    } else if(widgettype.get() == "Ring"){
	// NOTHING TO DO YET, cuz I didn't build a widget!
    } else if(widgettype.get() == "Line"){
	widget_p1->move(p1, 1*widget_scale);
	widget_p2->move(p2, 1*widget_scale);
	widget_edge1->move(p1, p2, 0.5*widget_scale);
	Vector spvec(p2-p1);
	spvec.normalize();
	Point sp(p1+spvec*slider1_dist);
	Point sp2(sp+spvec*(widget_scale*0.5));
	Point sp1(sp-spvec*(widget_scale*0.5));
	widget_slider1body->move(sp1, sp2, 1*widget_scale);
	widget_slider1cap1->move(sp2, spvec, 1*widget_scale);
	widget_slider1cap2->move(sp1, -spvec, 1*widget_scale);
	Vector v1,v2;
	spvec.find_orthogonal(v1, v2);
#if 0
	widget_p1->get_pick()->set_principal(spvec, v1, v2);
	widget_p2->get_pick()->set_principal(spvec, v1, v2);
	widget_edge1->get_pick()->set_principal(spvec, v1, v2);
	widget_slider1->get_pick()->set_principal(spvec);
#endif
	NOT_FINISHED("Rake");
	widget->reset_bbox();
    } else if(widgettype.get() == "Square"){
	NOT_FINISHED("Square widget");
    }

    GeomGroup* group=new GeomGroup;
    GeomMaterial* matlobj=new GeomMaterial(group, matl);

    // Temporary algorithm...
    if(markertype.get() == "Line"){
	Array1<SLine*> slines;
	if(widgettype.get() == "Point"){
	    slines.add(new SLine(group, p1));
	} else if(widgettype.get() == "Line"){
	    Vector line(p2-p1);
	    double l=line.length();
	    for(double x=0;x<=l;x+=slider1_dist){
		slines.add(new SLine(group, p1+line*(x/l)));
	    }
	} else if(widgettype.get() == "Ring"){
	    Vector n(p2-p1);
	    if (n.length()==0) n.x(1);	// force it to be nonzero!
	    Vector v1,v2;
	    n.find_orthogonal(v1,v2);
	    double rad=ring_radius.get();
	    // we're using slider1_dist here to be the radian separation btwn
	    // streamlines.
	    for(double theta=0; theta<2*Pi; theta+=slider1_dist){
		slines.add(new SLine(group, 
				     p1+((v1*(Cos(theta))+
					  v2*(Sin(theta)))*rad)));
	    }
	} else if(widgettype.get() == "Square"){
	    Vector line1(p2-p1);
	    Vector line2(p3-p1);
	    double l1=line1.length();
	    double l2=line2.length();
	    for(double x=0;x<=l1;x+=slider1_dist){
		Point p(p1+line1*(x/l1));
		for(double y=0;y<=l2;y+=slider2_dist){
		    slines.add(new SLine(group, p+line2*(y/l2)));
		}
	    }
	}

	int n=0;
	int groupid=0;
	int alg=(algorithm.get()=="RK4");
	for(int i=0;i<maxsteps.get();i++){
	    int oldn=n;
	    double ss=stepsize.get();
	    for(int i=0;i<slines.size();i++)
		n+=slines[i]->advance(field, alg, ss);
	    if(abort_flag || n==oldn)
		break;
	    if(n>500){
		if(!ogeom->busy()){
		    n=0;
		    if(groupid)
			cerr << "Deleting group: " << groupid << endl;
		    if(groupid)
			ogeom->delObj(groupid);
		    groupid=ogeom->addObj(matlobj->clone(), streamline_name);
		    ogeom->flushViews();
		}
	    }
	}
	if(groupid)
	    cerr << "Deleting group: " << groupid << endl;
	if(groupid)
	    ogeom->delObj(groupid);
	cerr << "n=" << n << endl;
    } else if(markertype.get() == "Ribbon"){
	Array1<SRibbon*> sribbons;
	if(widgettype.get() == "Point"){
	    sribbons.add(new SRibbon(group, p1, widget_scale,
				     Vector(0,widget_scale/20,0), have_sfield));
	} else if(widgettype.get() == "Line"){
	    Vector line(p2-p1);
	    double l=line.length();
	    Vector nline(line/50);
	    for(double x=0;x<=l;x+=slider1_dist){
		sribbons.add(new SRibbon(group, p1+line*(x/l),
					 slider1_dist*linerad, nline, have_sfield));
	    }
	} else if(widgettype.get() == "Ring"){
	    Vector n(p2-p1);
	    if (n.length()==0) n.x(1);	// force it to be nonzero!
	    Vector v1,v2;
	    n.find_orthogonal(v1,v2);
	    double rad=ring_radius.get();
	    // we're using slider1_dist here to be the radian separation btwn
	    // streamlines.
	    for(double theta=0; theta<2*Pi; theta+=slider1_dist){
		sribbons.add(new SRibbon(group, 
					 p1+((v1*(Cos(theta))+
					      v2*(Sin(theta)))*rad),
					 slider1_dist*10,
					 (v1*(Cos(theta-slider1_dist/2.)-
					      Cos(theta+slider1_dist/2.))+
					  v2*(Sin(theta-slider1_dist/2.)-
					      Sin(theta+slider1_dist/2.)))*rad,
					 have_sfield));
;
	    }
	} else if(widgettype.get() == "Square"){
	    NOT_FINISHED("Square with ribbons");
#if 0
	    Vector line1(p2-p1);
	    Vector line2(p3-p1);
	    double l1=line1.length();
	    double l2=line2.length();
	    for(double x=0;x<=l1;x+=slider1_dist){
		Point p(p1+line1*(x/l1));
		for(double y=0;y<=l2;y+=slider2_dist){
		    sribbons.add(new SRibbons(group, p+line2*(y/l2)));
		}
	    }
#endif 
	}

	int n=0;
	int groupid=0;
	int alg=(algorithm.get()=="RK4");
	for(int i=0;i<maxsteps.get();i++){
	    int oldn=n;
	    double ss=stepsize.get();
	    for(int i=0;i<sribbons.size();i++)
		n+=sribbons[i]->advance(field, alg, ss, sfield,
					cmap, have_sfield, outmatl,
					smin, smax);
	    if(abort_flag || n==oldn)
		break;
	    if(n>500){
		if(!ogeom->busy()){
		    n=0;
		    if(groupid)
			cerr << "Deleting group: " << groupid << endl;
		    if(groupid)
			ogeom->delObj(groupid);
		    groupid=ogeom->addObj(matlobj->clone(), streamline_name);
		    ogeom->flushViews();
		}
	    }
	}
	if(groupid)
	    cerr << "Deleting group: " << groupid << endl;
	if(groupid)
	    ogeom->delObj(groupid);
    } else {
	error("Unknown markertype: "+markertype.get());
    }
    if(group->size() == 0){
	delete group;
	streamline_id=0;
    } else {
	streamline_id=ogeom->addObj(matlobj, streamline_name);
    }
}

void Streamline::geom_moved(int axis, double dist, const Vector& delta,
			    void* cbdata)
{
    if(widgettype.get() == "Point"){
	p1+=delta;
	widget_p1->move(p1, 1*widget_scale);
    } else if(widgettype.get() == "Line"){
	switch((int)cbdata){
	case 1:
	    p1+=delta;
	    break;
	case 2:
	    p2+=delta;
	    break;
	case 3:
	    p1+=delta;
	    p2+=delta;
	    break;
	case 4:
	    {
		if(axis==0){
		    slider1_dist+=dist;
		} else {
		    slider1_dist-=dist;
		}
		Vector pv(p2-p1);
		double l=pv.length();
		if(slider1_dist < 0.01*l)
		    slider1_dist=0.01*l;
		else if(slider1_dist > l)
		    slider1_dist=l;
		break;
	    }
	}
	// Reconfigure...
	widget_p1->move(p1, 1*widget_scale);
	widget_p2->move(p2, 1*widget_scale);
	widget_edge1->move(p1, p2, 0.5*widget_scale);
	Vector spvec(p2-p1);
	spvec.normalize();
	Point sp(p1+spvec*slider1_dist);
	Point sp2(sp+spvec*(widget_scale*0.5));
	Point sp1(sp-spvec*(widget_scale*0.5));
	widget_slider1body->move(sp1, sp2, 1*widget_scale);
	widget_slider1cap1->move(sp2, spvec, 1*widget_scale);
	widget_slider1cap2->move(sp1, -spvec, 1*widget_scale);
	Vector v1,v2;
	spvec.find_orthogonal(v1, v2);
	pick_p1->set_principal(spvec, v1, v2);
	pick_p2->set_principal(spvec, v1, v2);
	pick_edge1->set_principal(spvec, v1, v2);
	pick_slider1->set_principal(spvec);
	widget->reset_bbox();
    } else if(widgettype.get() == "Square"){
	NOT_FINISHED("Square widget");
    } else {
	error("Unknown widgettype in Streamline");
    }
}

void Streamline::geom_release(void*)
{
    if(!abort_flag){
	abort_flag=1;
	want_to_execute();
    }
}

SLine::SLine(GeomGroup* group, const Point& p)
: p(p), line(new GeomPolyline)
{
    group->add(line);
    outside=0;
    line->pts.add(p);
}


int SLine::advance(const VectorFieldHandle& field, int rk4,
		   double stepsize)
{
    if(outside)
	return 0;
    if(rk4){
	    NOT_FINISHED("SLine::advance for RK4");
    } else {
	Vector v;
	if(!field->interpolate(p, v)){
	    outside=1;
	    return 0;
	}
	p+=(v*stepsize);
    }
    line->pts.add(p);
    return 1;
}



