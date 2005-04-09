

/*
 *  Roe.cc:  The Geometry Viewer Window
 *
 *  Written by:
 *   David Weinstein
 *   Department of Computer Science
 *   University of Utah
 *   April 1994
 *
 *  Copyright (C) 1994 SCI Group
 */

#include <Modules/Salmon/Salmon.h>
#include <Modules/Salmon/Roe.h>
#include <Modules/Salmon/Renderer.h>
#include <Classlib/Debug.h>
#include <Classlib/NotFinished.h>
#include <Classlib/Timer.h>
#include <Math/Expon.h>
#include <Math/MiscMath.h>
#include <Geometry/BBox.h>
#include <Geometry/Transform.h>
#include <Geometry/Vector.h>
#include <Geom/Geom.h>
#include <Geom/Pick.h>
#include <Geom/PointLight.h>
#include <Malloc/Allocator.h>
#include <Math/Trig.h>
#include <TCL/TCLTask.h>
#include <iostream.h>
#include <stdio.h>
#include <string.h>
#include <strstream.h>

#define MouseStart 0
#define MouseEnd 1
#define MouseMove 2

const int MODEBUFSIZE = 100;
static DebugSwitch autoview_sw("Roe", "autoview");

Roe::Roe(Salmon* s, const clString& id)
: manager(s), id(id),
  view("view", id, this),
  homeview(Point(.55, .5, 0), Point(.55, .5, .5), Vector(0,1,0), 25),
  bgcolor("bgcolor", id, this), shading("shading", id, this)
{
    bgcolor.set(Color(0,0,0));
    view.set(homeview);
    TCL::add_command(id+"-c", this, 0);
    current_renderer=0;
    modebuf=scinew char[MODEBUFSIZE];
    modecommand=scinew char[MODEBUFSIZE];
    maxtag=0;
}

#ifdef OLDUI
void Roe::orthoCB(CallbackData*, void*) {
    evl->lock();
    make_current();
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(-8, 8, -6, 6, -100, 100);
    // determine the bounding box so we can scale the view
    get_bounds(bb);
    if (bb.valid()) {
	Point cen(bb.center());
	double cx=cen.x();
	double cy=cen.y();
	double xw=cx-bb.min().x();
	double yw=cy-bb.min().y();
	double scl=4/Max(xw,yw);
	glScaled(scl,scl,scl);
    }
    glMatrixMode(GL_MODELVIEW);
    evl->unlock();
    need_redraw=1;
}

void Roe::perspCB(CallbackData*, void*) {
    evl->lock();
    make_current();
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluPerspective(90, 1.33, 1, 100);
    glMatrixMode(GL_MODELVIEW);
    evl->unlock();
    need_redraw=1;
}

#endif

void Roe::itemAdded(SceneItem* si)
{
    ObjTag* vis;
    if(!visible.lookup(si->name, vis)){
	// Make one...
	vis=scinew ObjTag;
	vis->visible=scinew TCLvarint(si->name, id, this);
	vis->visible->set(1);
	vis->tagid=maxtag++;
	visible.insert(si->name, vis);
	char buf[1000];
	ostrstream str(buf, 1000);
	str << id << " addObject " << vis->tagid << " \"" << si->name << "\"" << '\0';
	TCL::execute(str.str());
    } else {
	char buf[1000];
	ostrstream str(buf, 1000);
	str << id << " addObject2 " << vis->tagid << '\0';
	TCL::execute(str.str());
    }
    // invalidate the bounding box
    bb.reset();
    need_redraw=1;
}

void Roe::itemDeleted(SceneItem *si)
{
    ObjTag* vis;
    if(!visible.lookup(si->name, vis)){
	cerr << "Where did that object go???" << endl;
    } else {
	char buf[1000];
	ostrstream str(buf, 1000);
	str << id << " removeObject " << vis->tagid << '\0';
	TCL::execute(str.str());
    }
    // invalidate the bounding box
    bb.reset();
    need_redraw=1;
}

// need to fill this in!   
#ifdef OLDUI
void Roe::itemCB(CallbackData*, void *gI) {
    GeomItem *g = (GeomItem *)gI;
    g->vis = !g->vis;
    need_redraw=1;
}

void Roe::spawnChCB(CallbackData*, void*)
{
  double mat[16];
  glGetDoublev(GL_MODELVIEW_MATRIX, mat);

  kids.add(scinew Roe(manager, mat, mtnScl));
  kids[kids.size()-1]->SetParent(this);
  for (int i=0; i<geomItemA.size(); i++)
      kids[kids.size()-1]->itemAdded(geomItemA[i]->geom, geomItemA[i]->name);

}
#endif
    
Roe::~Roe()
{
    delete[] modebuf;
    delete[] modecommand;
}

#ifdef OLDUI
void Roe::fogCB(CallbackData*, void*) {
    evl->lock();
    make_current();
    if (!glIsEnabled(GL_FOG)) {
	glEnable(GL_FOG);
    } else {
	glDisable(GL_FOG);
    }
    evl->unlock();
    need_redraw=1;
}

void Roe::point1CB(CallbackData*, void*)
{
    evl->lock();
    make_current();
    if (!glIsEnabled(GL_LIGHT0)) {
	glEnable(GL_LIGHT0);
    } else {
	glDisable(GL_LIGHT0);
    }
    evl->unlock();
    need_redraw=1;
}

void Roe::point2CB(CallbackData*, void*)
{
    evl->lock();
    make_current();
    if (!glIsEnabled(GL_LIGHT1)) {
	glEnable(GL_LIGHT1);
    } else {
	glDisable(GL_LIGHT1);
    }
    evl->unlock();
    need_redraw=1;
}

void Roe::head1CB(CallbackData*, void*)
{
    evl->lock();
    make_current();
    if (!glIsEnabled(GL_LIGHT2)) {
	glEnable(GL_LIGHT2);
    } else {
	glDisable(GL_LIGHT2);
    }
    evl->unlock();
    need_redraw=1;
}
#endif

void Roe::get_bounds(BBox& bbox)
{
    bbox.reset();
    HashTableIter<int, PortInfo*> iter(&manager->portHash);
    for (iter.first(); iter.ok(); ++iter) {
	HashTable<int, SceneItem*>* serHash=iter.get_data()->objs;
	HashTableIter<int, SceneItem*> serIter(serHash);
	for (serIter.first(); serIter.ok(); ++serIter) {
	    SceneItem *si=serIter.get_data();
	    // Look up the name to see if it should be drawn...
	    ObjTag* vis;
	    if(visible.lookup(si->name, vis)){
		if(vis->visible->get()){
		    if(si->lock)
			si->lock->read_lock();
		    si->obj->get_bounds(bbox);
		    if(si->lock)
			si->lock->read_unlock();
		}
	    } else {
		cerr << "Warning: object " << si->name << " not in visibility database...\n";
		si->obj->get_bounds(bbox);
	    }
	}
    }
}

void Roe::rotate(double /*angle*/, Vector /*v*/, Point /*c*/)
{
    NOT_FINISHED("Roe::rotate");
#ifdef OLDUI
    evl->lock();
    make_current();
    double temp[16];
    glGetDoublev(GL_MODELVIEW_MATRIX, temp);
    glPopMatrix();
    glLoadIdentity();
    glTranslated(c.x(), c.y(), c.z());
    glRotated(angle,v.x(), v.y(), v.z());
    glTranslated(-c.x(), -c.y(), -c.z());
    glMultMatrixd(temp);
    for (int i=0; i<kids.size(); i++)
	kids[i]->rotate(angle, v, c);
    evl->unlock();
#endif
    need_redraw=1;
}

void Roe::rotate_obj(double /*angle*/, const Vector& /*v*/, const Point& /*c*/)
{
    NOT_FINISHED("Roe::rotate_obj");
#ifdef OLDUI
    evl->lock();
    make_current();
    glTranslated(c.x(), c.y(), c.z());
    glRotated(angle, v.x(), v.y(), v.z());
    glTranslated(-c.x(), -c.y(), -c.z());
    for(int i=0; i<kids.size(); i++)
	kids[i]->rotate(angle, v, c);
    evl->unlock();
#endif
    need_redraw=1;
}

void Roe::translate(Vector /*v*/)
{
    NOT_FINISHED("Roe::translate");
#ifdef OLDUI
    evl->lock();
    make_current();
    double temp[16];
    glGetDoublev(GL_MODELVIEW_MATRIX, temp);
    glPopMatrix();
    glLoadIdentity();
    glTranslated(v.x()*mtnScl, v.y()*mtnScl, v.z()*mtnScl);
    glMultMatrixd(temp);
    for (int i=0; i<kids.size(); i++)
	kids[i]->translate(v);
    evl->unlock();
#endif
    need_redraw=1;
}

void Roe::scale(Vector /*v*/, Point /*c*/)
{
    NOT_FINISHED("Roe::scale");
#ifdef OLDUI
    evl->lock();
    make_current();
    glTranslated(c.x(), c.y(), c.z());
    glScaled(v.x(), v.y(), v.z());
    glTranslated(-c.x(), -c.y(), -c.z());
    mtnScl*=v.x();
    for (int i=0; i<kids.size(); i++)
	kids[i]->scale(v, c);
    evl->unlock();
#endif
    need_redraw=1;
}


void Roe::mouse_translate(int action, int x, int y, int, int)
{
    switch(action){
    case MouseStart:
	last_x=x;
	last_y=y;
	total_x = 0;
	total_y = 0;
	update_mode_string("translate: ");
	break;
    case MouseMove:
	{
	    int xres=current_renderer->xres;
	    int yres=current_renderer->yres;
	    double xmtn=double(last_x-x)/double(xres);
	    double ymtn=-double(last_y-y)/double(yres);
	    last_x = x;
	    last_y = y;
	    // Get rid of roundoff error for the display...
	    if (Abs(total_x) < .001) total_x = 0;
	    if (Abs(total_y) < .001) total_y = 0;

	    View tmpview(view.get());
	    double aspect=double(xres)/double(yres);
	    double znear, zfar;
	    if(!current_renderer->compute_depth(this, tmpview, znear, zfar))
		return; // No objects...
	    double zmid=(znear+zfar)/2.;
	    Vector u,v;
	    tmpview.get_viewplane(aspect, zmid, u, v);
	    double ul=u.length();
	    double vl=v.length();
	    Vector trans(u*xmtn+v*ymtn);

	    total_x+=ul*xmtn;
	    total_y+=vl*ymtn;

	    // Translate the view...
	    tmpview.eyep+=trans;
	    tmpview.lookat+=trans;

	    // Put the view back...
	    view.set(tmpview);

	    need_redraw=1;
	    ostrstream str(modebuf, MODEBUFSIZE);
	    str << "translate: " << total_x << ", " << total_y << '\0';
	    update_mode_string(str.str());
	}
	break;
    case MouseEnd:
	update_mode_string("");
	break;
    }
}

void Roe::mouse_scale(int action, int x, int y, int, int)
{
    switch(action){
    case MouseStart:
	{
	    update_mode_string("scale: ");
	    last_x=x;
	    last_y=y;
	    total_scale=1;
	}
	break;
    case MouseMove:
	{
	    double scl;
	    double xmtn=last_x-x;
	    double ymtn=last_y-y;
	    xmtn/=30;
	    ymtn/=30;
	    last_x = x;
	    last_y = y;
	    if (Abs(xmtn)>Abs(ymtn)) scl=xmtn; else scl=ymtn;
	    if (scl<0) scl=1/(1-scl); else scl+=1;
	    total_scale*=scl;

	    View tmpview(view.get());
	    tmpview.fov=RtoD(2*Atan(scl*Tan(DtoR(tmpview.fov/2.))));
	    view.set(tmpview);
	    need_redraw=1;
	    ostrstream str(modebuf, MODEBUFSIZE);
	    str << "scale: " << total_x*100 << "%" << '\0';
	    update_mode_string(str.str());
	}
	break;
    case MouseEnd:
	update_mode_string("");
	break;
    }	
}

void Roe::mouse_rotate(int action, int x, int y, int, int)
{
    switch(action){
    case MouseStart:
	{
	    update_mode_string("rotate:");
	    last_x=x;
	    last_y=y;

	    // Find the center of rotation...
	    View tmpview(view.get());
	    int xres=current_renderer->xres;
	    int yres=current_renderer->yres;
	    double aspect=double(xres)/double(yres);
	    double znear, zfar;
	    rot_point_valid=0;
	    if(!current_renderer->compute_depth(this, tmpview, znear, zfar))
		return; // No objects...
	    double zmid=(znear+zfar)/2.;

	    Point ep(0, 0, zmid);
	    rot_point=tmpview.eyespace_to_objspace(ep, aspect);
	    rot_view=tmpview;
	    rot_point_valid=1;
	}
	break;
    case MouseMove:
	{
	    int xres=current_renderer->xres;
	    int yres=current_renderer->yres;
	    double xmtn=double(last_x-x)/double(xres);
	    double ymtn=double(last_y-y)/double(yres);

	    double xrot=xmtn*360.0;
	    double yrot=ymtn*360.0;

	    if(!rot_point_valid)
		break;
	    // Rotate the scene about the rot_point
	    Transform transform;
	    Vector transl(Point(0,0,0)-rot_point);
	    transform.pre_translate(transl);
	    View tmpview(rot_view);
	    Vector u,v;
	    double aspect=double(xres)/double(yres);
	    tmpview.get_viewplane(aspect, 1, u, v);
	    u.normalize();
	    v.normalize();
	    transform.pre_rotate(DtoR(yrot), u);
	    transform.pre_rotate(DtoR(xrot), v);
	    transform.pre_translate(-transl);

	    Point top(tmpview.eyep+tmpview.up);
	    top=transform.project(top);
	    tmpview.eyep=transform.project(tmpview.eyep);
	    tmpview.lookat=transform.project(tmpview.lookat);
	    tmpview.up=top-tmpview.eyep;

	    view.set(tmpview);

	    need_redraw=1;
	    update_mode_string("rotate:");
	}
	break;
    case MouseEnd:
	update_mode_string("");
	break;
    }
}

void Roe::mouse_pick(int action, int x, int y, int state, int btn)
{
    BState bs;
    bs.shift=1; // Always for widgets...
    bs.control= ((state&4)!=0);
    bs.alt= ((state&8)!=0);
    bs.btn=btn;
    switch(action){
    case MouseStart:
	{
	    total_x=0;
	    total_y=0;
	    total_z=0;
	    last_x=x;
	    last_y=current_renderer->yres-y;
	    current_renderer->get_pick(manager, this, x, y,
				       pick_obj, pick_pick);

	    if (pick_obj){
		NOT_FINISHED("update mode string for pick");
		pick_pick->pick(this,bs);
		need_redraw=1;
	    } else {
		update_mode_string("pick: none");
	    }
	}
	break;
    case MouseMove:
	{
	    if (!pick_obj || !pick_pick) break;
// project the center of the item grabbed onto the screen -- take the z
// component and unprojec the last and current x, y locations to get a 
// vector in object space.
	    y=current_renderer->yres-y;
	    BBox itemBB;
	    pick_obj->get_bounds(itemBB);
	    View tmpview(view.get());
	    Point cen(itemBB.center());
	    double depth=tmpview.depth(cen);
	    Vector u,v;
	    int xres=current_renderer->xres;
	    int yres=current_renderer->yres;
	    double aspect=double(xres)/double(yres);
	    tmpview.get_viewplane(aspect, depth, u, v);
	    int dx=x-last_x;
	    int dy=y-last_y;
	    double ndx=(2*dx/(double(xres)-1));
	    double ndy=(2*dy/(double(yres)-1));
	    Vector motionv(u*ndx+v*ndy);

	    double maxdot=0;
	    int prin_dir=-1;
	    for (int i=0; i<pick_pick->nprincipal(); i++) {
		double pdot=Dot(motionv, pick_pick->principal(i));
		if(pdot > maxdot){
		    maxdot=pdot;
		    prin_dir=i;
		}
	    }
	    if(prin_dir != -1){
		double dist=motionv.length();
		Vector mtn(pick_pick->principal(prin_dir)*dist);
		total_x+=mtn.x();
		total_y+=mtn.y();
		total_z+=mtn.z();
		if (Abs(total_x) < .0001) total_x=0;
		if (Abs(total_y) < .0001) total_y=0;
		if (Abs(total_z) < .0001) total_z=0;
		need_redraw=1;
		update_mode_string("picked someting...");
		pick_pick->moved(prin_dir, dist, mtn, bs);
		need_redraw=1;
	    } else {
		update_mode_string("Bad direction...");
	    }
	    last_x = x;
	    last_y = y;
	}
	break;
    case MouseEnd:
	if(pick_pick){
	    pick_pick->release(bs);
	    need_redraw=1;
	}
	pick_pick=0;
	pick_obj=0;
	update_mode_string("");
	break;
    }
}

#ifdef OLDUI
void Roe::attach_dials(CallbackData*, void* ud)
{
    int which=(int)ud;
    switch(which){
    case 0:
	Dialbox::attach_dials(dbcontext_st);
	break;
    }
}

void Roe::DBscale(DBContext*, int, double value, double delta, void*)
{
    double oldvalue=value-delta;
    double f=Exp10(value)/Exp10(oldvalue);
    BBox bb;
    get_bounds(bb);
    Point p(0,0,0);
    if(bb.valid())
	p=bb.center();
    mtnScl*=f;
    scale(Vector(f, f, f), p);
    need_redraw=1;
}

void Roe::DBtranslate(DBContext*, int, double, double delta,
		      void* cbdata)
{
    int which=(int)cbdata;
    Vector v(0,0,0);
    switch(which){
    case 0:
	v.x(delta);
	break;
    case 1:
	v.y(delta);
	break;
    case 2:
	v.z(delta);
	break;
    }
    translate(v);
    need_redraw=1;
}

void Roe::DBrotate(DBContext*, int, double, double delta,
		      void* cbdata)
{
    int which=(int)cbdata;
    Vector v(0,0,0);
    switch(which){
    case 0:
	v=Vector(1,0,0);
	break;
    case 1:
	v=Vector(0,1,0);
	break;
    case 2:
	v=Vector(0,0,1);
	break;
    }
    BBox bb;
    get_bounds(bb);
    Point p(0,0,0);
    if(bb.valid())
	p=bb.center();
    rotate_obj(delta, v, p);
    need_redraw=1;
}
#endif

void Roe::redraw_if_needed()
{
    if(need_redraw){
	need_redraw=0;
	redraw();
    }
}

#ifdef OLDUI
void Roe::redraw_perf(CallbackData*, void*)
{
    evl->lock();
    Window w=XtWindow(*perf);
    Display* dpy=XtDisplay(*buttons);
    XClearWindow(dpy, w);
    XSetForeground(dpy, gc, fgcolor->pixel());
    XSetFont(dpy, gc, perffont->font->fid);
    int ascent=perffont->font->ascent;
    int height=ascent+perffont->font->descent;
    XDrawString(dpy, w, gc, 0, ascent, perf_string1(), perf_string1.len());
    XDrawString(dpy, w, gc, 0, height+ascent, perf_string2(), perf_string2.len());
    evl->unlock();
}
#endif

void Roe::tcl_command(TCLArgs& args, void*)
{
    if(args.count() < 2){
	args.error("Roe needs a minor command");
	return;
    }
    if(args[1] == "dump_roe"){
	if(args.count() != 3){
	    args.error("Roe::dump_roe needs an output file name!");
	    return;
	}
	// We need to dispatch this one to the remote thread
	// We use an ID string instead of a pointer in case this roe
	// gets killed by the time the redraw message gets dispatched.
	if(manager->mailbox.nitems() >= manager->mailbox.size()-1){
	    cerr << "Redraw event dropped, mailbox full!\n";
	} else {
	    manager->mailbox.send(scinew SalmonMessage(id, args[2]));
	}
    } else if(args[1] == "startup"){
	// Fill in the visibility database...
	HashTableIter<int, PortInfo*> iter(&manager->portHash);
	for (iter.first(); iter.ok(); ++iter) {
	    HashTable<int, SceneItem*>* serHash=iter.get_data()->objs;
	    HashTableIter<int, SceneItem*> serIter(serHash);
	    for (serIter.first(); serIter.ok(); ++serIter) {
		SceneItem *si=serIter.get_data();
		itemAdded(si);
	    }
	}
    } else if(args[1] == "setrenderer"){
	if(args.count() != 6){
	    args.error("setrenderer needs a renderer name, etc");
	    return;
	}
	Renderer* r=get_renderer(args[2]);
	if(!r){
	    args.error("Unknown renderer!");
	    return;
	}
	if(current_renderer)
	    current_renderer->hide();
	current_renderer=r;
	args.result(r->create_window(this, args[3], args[4], args[5]));
    } else if(args[1] == "redraw"){
	// We need to dispatch this one to the remote thread
	// We use an ID string instead of a pointer in case this roe
	// gets killed by the time the redraw message gets dispatched.
	if(manager->mailbox.nitems() >= manager->mailbox.size()-1){
	    cerr << "Redraw event dropped, mailbox full!\n";
	} else {
	    manager->mailbox.send(scinew SalmonMessage(id));
	}
    } else if(args[1] == "anim_redraw"){
	// We need to dispatch this one to the remote thread
	// We use an ID string instead of a pointer in case this roe
	// gets killed by the time the redraw message gets dispatched.
	if(manager->mailbox.nitems() >= manager->mailbox.size()-1){
	    cerr << "Redraw event dropped, mailbox full!\n";
	} else {
	    if(args.count() != 6){
		args.error("anim_redraw wants tbeg tend nframes framerate");
		return;
	    }
	    double tbeg;
	    if(!args[2].get_double(tbeg)){
		args.error("Can't figure out tbeg");
		return;
	    } 
	    double tend;
	    if(!args[3].get_double(tend)){
		args.error("Can't figure out tend");
		return;
	    }
	    int nframes;
	    if(!args[4].get_int(nframes)){
		args.error("Can't figure out nframes");
		return;
	    }
	    double framerate;
	    if(!args[5].get_double(framerate)){
		args.error("Can't figure out framerate");
		return;
	    }	    
	    manager->mailbox.send(scinew SalmonMessage(id, tbeg, tend,
						    nframes, framerate));
	}
    } else if(args[1] == "mtranslate"){
	do_mouse(&Roe::mouse_translate, args);
    } else if(args[1] == "mrotate"){
	do_mouse(&Roe::mouse_rotate, args);
    } else if(args[1] == "mscale"){
	do_mouse(&Roe::mouse_scale, args);
    } else if(args[1] == "mpick"){
	do_mouse(&Roe::mouse_pick, args);
    } else if(args[1] == "sethome"){
	homeview=view.get();
    } else if(args[1] == "gohome"){
	view.set(homeview);
	manager->mailbox.send(scinew SalmonMessage(id)); // Redraw
    } else if(args[1] == "autoview"){
	BBox bbox;
	get_bounds(bbox);
	autoview(bbox);
    } else if(args[1] == "dolly"){
	if(args.count() != 3){
	    args.error("dolly needs an amount");
	    return;
	}
	double amount;
	if(!args[2].get_double(amount)){
	    args.error("Can't figure out amount");
	    return;
	}
	View cv(view.get());
	Vector lookdir(cv.eyep-cv.lookat);
	lookdir*=amount;
	cv.eyep=cv.lookat+lookdir;
	animate_to_view(cv, 1.0);
    } else {
	args.error("Unknown minor command for Roe");
    }
}

void Roe::do_mouse(MouseHandler handler, TCLArgs& args)
{
    if(args.count() != 5 && args.count() != 7){
	args.error(args[1]+" needs start/move/end and x y");
	return;
    }
    int action;
    if(args[2] == "start"){
	action=MouseStart;
    } else if(args[2] == "end"){
	action=MouseEnd;
    } else if(args[2] == "move"){
	action=MouseMove;
    } else {
	args.error("Unknown mouse action");
	return;
    }
    int x,y;
    if(!args[3].get_int(x)){
	args.error("error parsing x");
	return;
    }
    if(!args[4].get_int(y)){
	args.error("error parsing y");
	return;
    }
    int state;
    int btn;
    if(args.count() == 7){
       if(!args[5].get_int(state)){
	  args.error("error parsing state");
	  return;

       }
       if(!args[6].get_int(btn)){
	  args.error("error parsing btn");
	  return;
       }
    }

    // We have to send this to the salmon thread...
    if(manager->mailbox.nitems() >= manager->mailbox.size()-1){
	cerr << "Mouse event dropped, mailbox full!\n";
    } else {
	manager->mailbox.send(scinew RoeMouseMessage(id, handler, action, x, y, state, btn));
    }
}

void Roe::autoview(const BBox& bbox)
{
    if(bbox.valid()){
        View cv(view.get());
        // Animate lookat point to center of BBox...
        cv.lookat=bbox.center();
        animate_to_view(cv, 2.0);
        
        // Move forward/backwards until entire view is in scene...
        Vector lookdir(cv.lookat-cv.eyep);
        double old_dist=lookdir.length();
        PRINTVAR(autoview_sw, old_dist);
        double old_w=2*Tan(DtoR(cv.fov/2.))*old_dist;
        PRINTVAR(autoview_sw, old_w);
        Vector diag(bbox.diagonal());
        double w=diag.length();
        PRINTVAR(autoview_sw, w);
        double dist=old_dist*w/old_w;
        PRINTVAR(autoview_sw, dist);
        cv.eyep=cv.lookat-lookdir*dist/old_dist;
        PRINTVAR(autoview_sw, cv.eyep);
        animate_to_view(cv, 2.0);
    }
}

void Roe::redraw()
{
    need_redraw=0;
    reset_vars();

    // Get animation variables
    double ct;
    if(!get_tcl_doublevar(id, "current_time", ct)){
	manager->error("Error reading current_time");
	return;
    }
    current_renderer->redraw(manager, this, ct, ct, 1, 0);
}

void Roe::redraw(double tbeg, double tend, int nframes, double framerate)
{
    need_redraw=0;
    reset_vars();

    // Get animation variables
    current_renderer->redraw(manager, this, tbeg, tend, nframes, framerate);
}

void Roe::update_mode_string(const char* msg)
{
    ostrstream str(modecommand, MODEBUFSIZE);    
    str << id << " updateMode \"" << msg << "\"" << '\0';
    TCL::execute(str.str());
}

TCLView::TCLView(const clString& name, const clString& id, TCL* tcl)
: TCLvar(name, id, tcl), eyep("eyep", str(), tcl),
  lookat("lookat", str(), tcl), up("up", str(), tcl),
  fov("fov", str(), tcl)
{
}

TCLView::~TCLView()
{
}

View TCLView::get()
{
    TCLTask::lock();
    View v(eyep.get(), lookat.get(), up.get(), fov.get());
    TCLTask::unlock();
    return v;
}

void TCLView::set(const View& view)
{
    TCLTask::lock();
    eyep.set(view.eyep);
    lookat.set(view.lookat);
    up.set(view.up);
    fov.set(view.fov);
    TCLTask::unlock();
}

RoeMouseMessage::RoeMouseMessage(const clString& rid, MouseHandler handler,
				 int action, int x, int y, int state, int btn)
: MessageBase(MessageTypes::RoeMouse), rid(rid), handler(handler),
  action(action), x(x), y(y), state(state), btn(btn)
{
}

RoeMouseMessage::~RoeMouseMessage()
{
}

void Roe::animate_to_view(const View& v, double /*time*/)
{
    NOT_FINISHED("Roe::animate_to_view");
    view.set(v);
    manager->mailbox.send(scinew SalmonMessage(id));
}

Renderer* Roe::get_renderer(const clString& name)
{
    // See if we already have one like that...
    Renderer* r;
    if(!renderers.lookup(name, r)){
	// Create it...
	r=Renderer::create(name);
	if(r)
	    renderers.insert(name, r);
    }
    return r;
}

void Roe::force_redraw()
{
    need_redraw=1;
}

void Roe::do_for_visible(Renderer* r, RoeVisPMF pmf)
{
    HashTableIter<int, PortInfo*> iter(&manager->portHash);
    for (iter.first(); iter.ok(); ++iter) {
	HashTable<int, SceneItem*>* serHash=iter.get_data()->objs;
	HashTableIter<int, SceneItem*> serIter(serHash);
	for (serIter.first(); serIter.ok(); ++serIter) {
	    SceneItem *si=serIter.get_data();
	    // Look up the name to see if it should be drawn...
	    ObjTag* vis;
	    if(visible.lookup(si->name, vis)){
		if(vis->visible->get()){
		    if(si->lock)
			si->lock->read_lock();
		    (r->*pmf)(manager, this, si->obj);
		    if(si->lock)
			si->lock->read_unlock();
		}
	    } else {
		cerr << "Warning: object " << si->name << " not in visibility database...\n";
	    }
	}
    }
}

void Roe::set_current_time(double time)
{
    set_tclvar(id, "current_time", to_string(time));
}
