#define protected public
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

#include <PSECommon/Modules/Salmon/Salmon.h>
#include <PSECommon/Modules/Salmon/Roe.h>
#include <PSECommon/Modules/Salmon/Renderer.h>
#include <PSECommon/Modules/Salmon/Ball.h>
#include <PSECommon/Modules/Salmon/BallMath.h>
#include <SCICore/Util/Debug.h>
#include <SCICore/Util/NotFinished.h>
#include <SCICore/Util/Timer.h>
#include <SCICore/Math/Expon.h>
#include <SCICore/Math/MiscMath.h>
#include <SCICore/Persistent/Pstreams.h>
#include <SCICore/Geometry/BBox.h>
#include <SCICore/Geometry/Transform.h>
#include <SCICore/Geometry/Vector.h>
#include <SCICore/Geom/GeomObj.h>
#include <SCICore/Geom/Material.h>
#include <SCICore/Geom/GeomOpenGL.h>
#include <SCICore/Geom/GeomPick.h>
#include <SCICore/Geom/PointLight.h>
#include <SCICore/Geom/GeomScene.h>
#include <SCICore/Geom/GeomSphere.h>
#include <SCICore/Geom/GeomCone.h>      
#include <SCICore/Geom/GeomCylinder.h>  
#include <SCICore/Geom/GeomGroup.h>     
#include <SCICore/Geom/Material.h>
#include <SCICore/Malloc/Allocator.h>
#include <SCICore/Math/Trig.h>
#include <SCICore/TclInterface/TCLTask.h>
#include <SCICore/TclInterface/TCLvar.h>
#include <SCICore/Thread/CrowdMonitor.h>
#include <SCICore/Thread/FutureValue.h>
#include <iostream>
using std::cerr;
using std::endl;
#include <stdio.h>
#include <string.h>
#include <sstream>
using std::ostringstream;

#define MouseStart 0
#define MouseEnd 1
#define MouseMove 2

namespace PSECommon {
namespace Modules {

using namespace SCICore::GeomSpace;  
using SCICore::Math::Abs;
using SCICore::Geometry::Cross;
using SCICore::Geometry::Dot;
using SCICore::Containers::to_string;
using SCICore::GeomSpace::BState;
using SCICore::GeomSpace::GeomScene;
using SCICore::PersistentSpace::BinaryPiostream;
using SCICore::PersistentSpace::TextPiostream;

//static DebugSwitch autoview_sw("Roe", "autoview");
static Roe::MapClStringObjTag::iterator viter;

void add_pt( Roe *roe, Point p, double s=.2 )
{
  GeomSphere *obj = scinew GeomSphere;
  obj->move(p, s);
  roe->roe_objs.add(obj);
  roe->need_redraw = 1;
}

Roe::Roe(Salmon* s, const clString& id)
  : manager(s),
    id(id),
    view("view", id, this),
    homeview(Point(.55, .5, 0), Point(.0, .0, .0), Vector(0,1,0), 25),
    bgcolor("bgcolor", id, this), 
    shading("shading", id, this),
    do_stereo("do_stereo", id, this), 
    // --  BAWGL -- 
    do_bawgl("do_bawgl", id, this),  
    // --  BAWGL -- 
    drawimg("drawimg", id, this),
    saveprefix("saveprefix", id, this),
    curFrame(0),curName("movie"),pos("pos", id, this),
    caxes("caxes", id, this),iaxes("iaxes", id, this), 
    doingMovie(false),makeMPEG(false)
{
  inertia_mode=0;
  bgcolor.set(Color(0,0,0));
  view.set(homeview);
  TCL::add_command(id+"-c", this, 0);
  current_renderer=0;
  maxtag=0;
  mouse_obj=0;
  ball = new BallData();
  ball->Init();
  // --  BAWGL -- 
  bawgl = new SCIBaWGL();
  // --  BAWGL -- 
  roe_objs.add( createGenAxes() );     
  roe_objs_draw.add(0);              
  roe_objs_draw[0] = 1;
  // XXX - UniCam addition:
  // initialize focus sphere for UniCam
  // the focus sphere is a sphere object -- let's color it blue.
  // XXX - note to Utah-- it took a long time to figure out how to
  // create a simple sphere & get it drawn.  I still can't easily
  // control it's color.  I know i need to use a GeomMaterial, but
  // when i include that file above in Roe.cc & try to instantiate
  // one w/ scinew (like in other example code), it claims
  // GeomMaterial is an unknown type-- even though it's declaration
  // has been included!?  very strange.  anyway, gave up on color.
  // XXX - update:  Ah ha, i think i understand why it couldn't find the
  // type GeomMaterial-- it isn't enough to just include that classes
  // header file, you need to also have the declaration:
  //       using <class name>
  // In the case of GeomMaterial, it would look like:
  //       #include <SCICore/Geom/Material.h>
  //       using SCICore::GeomSpace::GeomMaterial;
  // 
  focus_sphere      = scinew GeomSphere;
  is_dot            = 0;

}

clString Roe::set_id(const clString& new_id)
{
  clString old_id=id;
  id=new_id;
  return old_id;
}

void Roe::itemAdded(GeomSalmonItem* si)
{
  ObjTag* vis;
    
  viter = visible.find(si->name);
  if(viter==visible.end()){
    // Make one...
    vis=scinew ObjTag;
    vis->visible=scinew TCLvarint(si->name, id, this);
    vis->visible->set(1);
    vis->tagid=maxtag++;
    visible[si->name] = vis;
    ostringstream str;
    str << id << " addObject " << vis->tagid << " \"" << si->name << "\"";
    TCL::execute(str.str().c_str());
  } else {
    vis = (*viter).second;
    ostringstream str;
    str << id << " addObject2 " << vis->tagid;
    TCL::execute(str.str().c_str());
  }
  // invalidate the bounding box
  bb.reset();
  need_redraw=1;
}

void Roe::itemDeleted(GeomSalmonItem *si)
{
  ObjTag* vis;
    
  viter = visible.find(si->name);
  if (viter == visible.end()) { // if not found
    cerr << "Where did that object go???" << endl;
  }
  else {
    vis = (*viter).second;
    ostringstream str;
    str << id << " removeObject " << vis->tagid;
    TCL::execute(str.str().c_str());
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
  TCL::delete_command( id+"-c" );
}

void Roe::get_bounds(BBox& bbox)
{
  bbox.reset();

  GeomIndexedGroup::IterIntGeomObj iter = manager->ports.getIter();
    
  for ( ; iter.first != iter.second; iter.first++) {
	
    GeomIndexedGroup::IterIntGeomObj serIter =
      ((GeomSalmonPort*)((*iter.first).second))->getIter();

				// items in the scen are all
				// GeomSalmonItem's...
    for ( ; serIter.first != serIter.second; serIter.first++) {
      GeomSalmonItem *si=(GeomSalmonItem*)((*serIter.first).second);
	    
				// Look up the name to see if it
				// should be drawn...
      ObjTag* vis;
	    
      viter = visible.find(si->name);
	    
      if (viter != visible.end()) { // if found
	vis = (*viter).second;
	if (vis->visible->get()) {
	  if(si->lock) si->lock->readLock();
	  si->get_bounds(bbox);
	  if(si->lock) si->lock->readUnlock();
	}
      }
      else {
	cerr << "Warning: object " << si->name
	     << " not in visibility database...\n";
	si->get_bounds(bbox);
      }
    }
  }

  // XXX - START - ASF ADDED FOR UNICAM
  //   cerr << "roe_objs.size() = " << roe_objs.size() << endl;
  int objs_size = roe_objs.size();
  int draw_size = roe_objs_draw.size();
  for(int i=0;i<roe_objs.size();i++) {
    
    if (i<draw_size && roe_objs_draw[i])
      roe_objs[i]->get_bounds(bbox);
  }
  // XXX - END   - ASF ADDED FOR UNICAM
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

void Roe::mouse_translate(int action, int x, int y, int, int, int)
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
      tmpview.eyep(tmpview.eyep()+trans);
      tmpview.lookat(tmpview.lookat()+trans);

      // Put the view back...
      view.set(tmpview);

      need_redraw=1;
      ostringstream str;
      str << "translate: " << total_x << ", " << total_y;
      update_mode_string(str.str().c_str());
    }
    break;
  case MouseEnd:
    update_mode_string("");
    break;
  }
}

#if 0
void Roe::mouse_scale(int action, int x, int y, int, int, int)
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
      tmpview.fov(RtoD(2*Atan(scl*Tan(DtoR(tmpview.fov()/2.)))));

      view.set(tmpview);
      need_redraw=1;
      ostringstream str;
      str << "scale: " << total_x*100 << "%";
      update_mode_string(str.str().c_str());
    }
    break;
  case MouseEnd:
    update_mode_string("");
    break;
  }	
}
#else
float Roe::WindowAspect()
{
  float w = current_renderer->xres;
  float h = current_renderer->yres;

  return w/h;
}

void Roe::MyTranslateCamera(Vector offset)
{
  View tmpview(view.get());

  tmpview.eyep  (tmpview.eyep  () + offset);
  tmpview.lookat(tmpview.lookat() + offset);

  view.set(tmpview);
  need_redraw=1;
}

// surprised this wasn't definied somewhere in SCICore?
Point operator*(Transform &t, const Point &d)
{
  float result[4], tmp[4];
  result[0] = result[1] = result[2] = result[3] = 0;
  tmp[0] = d(0);
  tmp[1] = d(1);
  tmp[2] = d(2);
  tmp[3] = 1.0;

  double mat[16];
  t.get(mat);

  for(int i=0;i<4;i++) {
    for(int j=0;j<4;j++) {
      result[i] += mat[4*i + j] * tmp[j];
    }
  }

  return Point(result[0], result[1], result[2]);
}

Vector operator*(Transform &t, const Vector &d)
{
  float result[4], tmp[4];
  result[0] = result[1] = result[2] = result[3] = 0;
  tmp[0] = d(0);
  tmp[1] = d(1);
  tmp[2] = d(2);
  tmp[3] = 0.0;

  double mat[16];
  t.get(mat);

  for(int i=0;i<4;i++) {
    for(int j=0;j<4;j++) {
      result[i] += mat[4*i + j] * tmp[j];
    }
  }

  return Vector(result[0], result[1], result[2]);
}

void Roe::MyRotateCamera( Point  center,
                          Vector axis,
                          double angle )  // radians
{
  View tmpview(view.get());

  Point Origin(0,0,0);

  Transform mat;
  mat.load_identity();
  mat.pre_translate(Origin - center);
  mat.pre_rotate   (angle, axis);
  mat.pre_translate(center - Origin);

  Point  p = tmpview.eyep();
  Point  a = tmpview.lookat();
  Vector u = tmpview.up();

  tmpview.eyep  (mat * p);
  tmpview.lookat(mat * a);
  tmpview.up    (mat * u);

  view.set(tmpview);
  need_redraw=1;
}

void Roe::NormalizeMouseXY( int X, int Y, float *NX, float *NY )
{
  double w = current_renderer->xres;
  double h = current_renderer->yres;

  *NX = double(X) / w;
  *NY = double(Y) / h;

  *NY = 1.0 - *NY;

  *NX = -1.0 + 2.0 * (*NX);
  *NY = -1.0 + 2.0 * (*NY);
}

Vector Roe::CameraToWorld(Vector v)
{
  View tmpview(view.get());

  Vector z_axis,y_axis,x_axis;

  y_axis = tmpview.up();
  z_axis = tmpview.eyep() - tmpview.lookat();
  z_axis.normalize();
  x_axis = Cross(y_axis,z_axis);
  x_axis.normalize();
  y_axis = Cross(z_axis,x_axis);
  y_axis.normalize();

  Transform mat(tmpview.eyep(), x_axis, y_axis, z_axis);

  return mat * v;
}

void Roe::choose(int X, int Y)
{
  //   MyTranslateCamera(Vector(0.1,0,0));
  //   BBox bbox;
  //   get_bounds(bbox);
  //   Point ctr = (bbox.valid() ? bbox.center() : Point(0,0,0));
  //   MyRotateCamera(ctr,
  //                  Vector(0,1,0),
  //                  1 * M_PI/180.0);

  //   cerr << CameraToWorld(Vector(1,0,0)) << endl;
  //   cerr << CameraToWorld(Vector(0,1,0)) << endl;
  //   cerr << CameraToWorld(Vector(0,0,1)) << endl;

  //   float nx, ny;
  //   NormalizeMouseXY(x, y, &nx, &ny);
  //   cerr << nx << "\t" << ny << endl;

  int   te[2];  // pixel location
  te[0] = X;
  te[1] = Y;

  float curpt[2];
  NormalizeMouseXY(X, Y, &curpt[0], &curpt[1]);
  
  float delta[2];
  delta[0] = curpt[0] - _last_pos[0];
  delta[1] = curpt[1] - _last_pos[1];
  _last_pos[0] = te[0];
  _last_pos[1] = te[1];

  double tdelt(the_time() - _dtime);

  _dist += sqrt(delta[0] * delta[0] + delta[1] * delta[1]);

  float sdelt[2];
  sdelt[0] = te[0] - _start_pix[0];
  sdelt[1] = te[1] - _start_pix[1];

  int xa=0,ya=1;
  if (getenv("FLIP_CAM_MANIP")) {
    int tmp = xa;
    xa = ya;
    ya = xa;
  }
     
  float len = sqrt(sdelt[0] * sdelt[0] + sdelt[1] * sdelt[1]);
  if (fabs(sdelt[ya])/len > 0.9 && tdelt > 0.05) {
    unicam_state = UNICAM_ZOOM;
    //     ptr->set_old(_start_pix);
  } else if (tdelt < 0.1 && _dist < 0.03)
    return;
  else {
    if (fabs(sdelt[xa])/len > 0.6 )
      unicam_state = UNICAM_PAN;
    else unicam_state = UNICAM_ZOOM;
    //     ptr->set_old(_start_pix);
  }
}

void Roe::rot   (int x, int y)
{
  //  float myTEST = X;
  //  cerr << "myTEST = " << myTEST << "\t" << "X = " << X << endl;
  Point center = focus_sphere->cen;

  //   this->ComputeWorldToDisplay(center[0], center[1], center[2], cpt);
  // XXX - this code did not seem to work to return normalized window
  // XXX - coordinates.
  // //   float cpt[3];
  // //   View tmpview(view.get());
  // //   Point tmp = tmpview.objspace_to_eyespace(center, WindowAspect());
  // //   cpt[0] = tmp(0);
  // //   cpt[1] = tmp(1);
  // //   NormalizeMouseXY(cpt[0], cpt[1], &cpt[0], &cpt[1]);
  float cpt[3];
  NormalizeMouseXY(_down_x, _down_y, &cpt[0], &cpt[1]);

  double radsq = pow(1.0+fabs(cpt[0]),2); // squared rad of virtual cylinder

  //   XYpt        tp    = ptr->old(); 
  //   XYpt        te    = ptr->cur();
  float tp[2], te[2];
  NormalizeMouseXY(_last_pix[0], _last_pix[1], &tp[0], &tp[1]);
  NormalizeMouseXY(x, y, &te[0], &te[1]);
  _last_pix[0] = x;
  _last_pix[1] = y;

  //    Wvec   op  (tp[0], 0, 0);             // get start and end X coordinates
  //    Wvec   oe  (te[0], 0, 0);             //    of cursor motion
  float op[3], oe[3];
  op[0] = tp[0];
  op[1] = 0;
  op[2] = 0;
  oe[0] = te[0];
  oe[1] = 0;
  oe[2] = 0;

  //   double opsq = op * op, oesq = oe * oe;
  double opsq = op[0] * op[0], oesq = oe[0] * oe[0];

  double lop  = opsq > radsq ? 0 : sqrt(radsq - opsq);
  double loe  = oesq > radsq ? 0 : sqrt(radsq - oesq);

  //   Wvec   nop  = Wvec(op[0], 0, lop).normalize();
  //   Wvec   noe  = Wvec(oe[0], 0, loe).normalize();
  Vector nop = Vector(op[0], 0, lop).normal();
  Vector noe = Vector(oe[0], 0, loe).normal();

  //   double dot  = nop * noe;
  double dot = Dot(nop, noe);

  if (fabs(dot) > 0.0001) {
    //       data->rotate(Wline(data->center(), Wvec::Y),
    //                    -2*acos(clamp(dot,-1.,1.)) * Sign(te[0]-tp[0]));

    double angle = -2*acos(clamp(dot,-1.,1.)) * Sign(te[0]-tp[0]);
    MyRotateCamera(center, Vector(0,1,0), angle);


    // 2nd part of rotation
    View tmpview(view.get());

    //       Wvec   dvec  = data->from() - data->center();
    Point  from = tmpview.eyep();
    Vector dvec = (from - center);
     
    double rdist = te[1]-tp[1];
    //      double tdist = acos(Wvec::Y * dvec.normalize());
    Vector Yvec(0,1,0);

    double tdist = acos(clamp(Dot(Yvec, dvec.normal()), -1., 1.));

    //       CAMdataptr   dd = new CAMdata(*data);
    //       Wline raxe(data->center(),data->right_v());
    //       data->rotate(raxe, rdist);
    Vector right_v = (film_pt(1, 0) - film_pt(0, 0)).normal();

    MyRotateCamera(center, right_v, rdist);

    tmpview = view.get(); // update tmpview params given last rotation
    tmpview.up(Vector(0,1,0));
    view.set(tmpview);

    //       if (data->right_v() * dd->right_v() < 0)
    //          *data = *dd;
  }
}

void Roe::zoom  (int X, int Y)
{
  float cn[2], ln[2];
  NormalizeMouseXY(X, Y, &cn[0], &cn[1]);
  NormalizeMouseXY(_last_pix[0], _last_pix[1], &ln[0], &ln[1]);

  float delta[2];
  delta[0] = cn[0] - ln[0];
  delta[1] = cn[1] - ln[1];
  _last_pix[0] = X;
  _last_pix[1] = Y;

  // PART A: Zoom in/out
  // (assume perspective projection for now..)
  View tmpview(view.get());
  
  Vector movec   = (_down_pt - tmpview.eyep());
  Vector movec_n = movec.normal(); // normalized movec

  Vector trans1  = movec_n * (movec.length() * delta[1] * -4);

  MyTranslateCamera(trans1);


  // PART B: Pan left/right.
  // Camera has moved, update tmpview..
  tmpview = view.get();

  movec   = (_down_pt - tmpview.eyep());  // (recompute since cam changed)
  Vector at_v  = film_dir(0,0);
  double depth = Dot(movec, at_v);

  Vector right_v = film_pt(1, 0, depth) - film_pt(-1, 0,depth);
  Vector up_v    = film_pt(0, 1, depth) - film_pt( 0,-1,depth);

  Vector trans2  = right_v * (-delta[0]/2);

  MyTranslateCamera(trans2);
}

void Roe::pan   (int X, int Y)
{
  float cn[2], ln[2];
  NormalizeMouseXY(X, Y, &cn[0], &cn[1]);
  NormalizeMouseXY(_last_pix[0], _last_pix[1], &ln[0], &ln[1]);

  float delta[2];
  delta[0] = cn[0] - ln[0];
  delta[1] = cn[1] - ln[1];
  _last_pix[0] = X;
  _last_pix[1] = Y;

  View tmpview(view.get());

  Vector movec   = (_down_pt - tmpview.eyep());

  Vector at_v  = film_dir(0,0);
  double depth = Dot(movec, at_v);

  Vector right_v = film_pt(1, 0, depth) - film_pt(-1, 0,depth);
  Vector up_v    = film_pt(0, 1, depth) - film_pt( 0,-1,depth);

  // add_pt(this, film_pt(0,0,depth), .01);
  // add_pt(this, film_pt(1,0,depth), .01);
  // add_pt(this, film_pt(0,1,depth), .01);

  Vector trans = (right_v * (-delta[0]/2) +
                  up_v    * (-delta[1]/2));

  MyTranslateCamera(trans);
}

void Roe::ShowFocusSphere()
{
  for(int i=0;i<roe_objs.size();i++)
    if (roe_objs[i] == focus_sphere) {
      return;
    }

  roe_objs.add(focus_sphere);
  roe_objs_draw.add(0);              
  roe_objs_draw[roe_objs_draw.size()-1] = 1;
}

void Roe::HideFocusSphere()
{
  for(int i=0;i<roe_objs.size();i++)
    if (roe_objs[i] == focus_sphere) {
      roe_objs.remove(i);
      roe_objs_draw.remove(i);
    }
}

// XXX - obsolete-- delete this method below.
// This method returns the world space point under the pixel <x, y>
// XXX - in addition to drawing the simple sphere 'focus sphere', picking
// was difficult to do too.  initially the problem was understanding how
// the example in 'mouse_pick(..)' worked (parameters, what was returned).
Point Roe::GetPointUnderCursor( int x, int y )
{
  return Point(0,0,0);
}

Vector Roe::film_dir   (double x, double y)
{
  View tmpview(view.get());
  Point at = tmpview.eyespace_to_objspace(Point( x, y, 1), WindowAspect());
  return (at - tmpview.eyep()).normal();
}

Point Roe::film_pt    (double x, double y, double z)
{
  View tmpview(view.get());

  Vector dir = film_dir(x,y);

  return tmpview.eyep() + dir * z;
}

void Roe::mouse_scale(int action, int x, int y, int, int, int)
{
  //   static int first=1;
  //   if (first) {
  //     first = 0;
  //     need_redraw = 1;

  //     Point l = film_pt(-1, 0, 5);
  //     Point r = film_pt( 1, 0, 5);
  //     double s = (l - r).length() / 20.0;

  //     for(int i=0;i<5;i++) {
  //       double u = double(i) / 4.0;

  //       Point p = film_pt(-1.0 + u * 2.0, 0, 5);

  //       GeomSphere *obj = scinew GeomSphere;
  //       obj->move(p, s);
  //       roe_objs.add(obj);
  //     }
  //   }

  //   if (action == MouseStart) {
  //     Point p;

  //     current_renderer->pick_scene(x, y, &p);

  //     Vector at_v = (view.get().lookat() - view.get().eyep()).normal();
  //     Vector vec  = (p - view.get().eyep()) * at_v;
  //     double s = 0.008 * vec.length();

  //     GeomSphere *obj = scinew GeomSphere;
  //     obj->move(p, s);
  //     roe_objs.add(obj);
    
  //     need_redraw = 1;
  //   }
  // return;    

  switch(action){
  case MouseStart:
    {
      extern int CAPTURE_Z_DATA_HACK;
      CAPTURE_Z_DATA_HACK = 1;
      redraw();

      update_mode_string("unicam: ");
      last_x=x;
      last_y=y;

      _dtime    = the_time();
      _dist     = 0;

      // cam manip init
      float curpt[2];
      NormalizeMouseXY(x, y, &curpt[0], &curpt[1]);
      _last_pos[0] = curpt[0];
      _last_pos[1] = curpt[1];

      // XXX - erroneously had 'x' be a capital 'X', which was a
      // bug, but the compiler didn't catch it.  Innocent
      // mistake was not caught by the compiler for some reason,
      // caused bad behavior in user interaction, and eventually
      // was debugged.
      //             _start_pix[0] = _last_pix[0] = X; // doesn't produce error!?
      //             _start_pix[1] = _last_pix[1] = Y; // doesn't produce error!?
      _start_pix[0] = _last_pix[0] = x;
      _start_pix[1] = _last_pix[1] = y;

      // find '_down_pt'  (point in world space under the cursor tip)
      current_renderer->pick_scene(x, y, &_down_pt);
      _down_x = x;
      _down_y = y;
      //             cerr << "_down_x = " << _down_x << endl;
      //             cerr << "_down_y = " << _down_y << endl;
            
      // if someone has already clicked to make a dot and
      // they're not clicking on it now, OR if the user is
      // clicking on the perimeter of the screen, then we want
      // to go into rotation mode.
      if ((fabs(curpt[0]) > .85 || fabs(curpt[1]) > .9) || is_dot) {
	if (is_dot)
	  _center = focus_sphere->cen;
              
	unicam_state = UNICAM_ROT;
      } else {
	unicam_state = UNICAM_CHOOSE;
      }
    }
    break;
  case MouseMove:
    {
      switch (unicam_state) {
      case UNICAM_CHOOSE:   choose(x, y); break;
      case UNICAM_ROT:      rot   (x, y); break;
      case UNICAM_PAN:      pan   (x, y); break;
      case UNICAM_ZOOM:     zoom  (x, y); break;
      }

      need_redraw=1;

      ostringstream str;
      char *unicamMode[] = {"Choose", "Rotate", "Pan", "Zoom"};
      str << "unicam: " << unicamMode[unicam_state];
      update_mode_string(str.str().c_str());
    }
    break;

  case MouseEnd:
    if (unicam_state == UNICAM_ROT && is_dot ) {
      HideFocusSphere();
      is_dot = 0;
    } else if (unicam_state == UNICAM_CHOOSE) {
      if (is_dot) {
	HideFocusSphere();
	is_dot = 0;
      } else {
	// XXX - need to select 's' to make focus_sphere 1/4 or so
	// inches on the screen always...  how?
	Vector at_v = (view.get().lookat() - view.get().eyep()).normal();
	Vector vec  = (_down_pt - view.get().eyep()) * at_v;
	double s = 0.008 * vec.length();

	focus_sphere->move(_down_pt, s);
	ShowFocusSphere();
	is_dot = 1;
      }
    }
        
    need_redraw = 1;

    update_mode_string("");
    break;
  }	
}

#endif

void Roe::mouse_rotate(int action, int x, int y, int, int, int time)
{
  switch(action){
  case MouseStart:
    {
      if(inertia_mode){
	inertia_mode=0;
	redraw();
      }
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
      //             cerr << "zmid = " << zmid << endl;

      Point ep(0, 0, zmid);
      rot_point=tmpview.eyespace_to_objspace(ep, aspect);

      rot_point = tmpview.lookat();
      rot_view=tmpview;
      rot_point_valid=1;

      double rad = 0.8;
      HVect center(0,0,0,1.0);
	
      // we also want to keep the old transform information
      // around (so stuff correlates correctly)
      // OGL uses left handed coordinate system!
	
      Vector z_axis,y_axis,x_axis;

      y_axis = tmpview.up();
      z_axis = tmpview.eyep() - tmpview.lookat();
      eye_dist = z_axis.normalize();
      x_axis = Cross(y_axis,z_axis);
      x_axis.normalize();
      y_axis = Cross(z_axis,x_axis);
      y_axis.normalize();
      tmpview.up(y_axis); // having this correct could fix something?

      prev_trans.load_frame(Point(0.0,0.0,0.0),x_axis,y_axis,z_axis);

      ball->Init();
      ball->Place(center,rad);
      HVect mouse((2.0*x)/xres - 1.0,2.0*(yres-y*1.0)/yres - 1.0,0.0,1.0);
      ball->Mouse(mouse);
      ball->BeginDrag();

      prev_time[0] = time;
      prev_quat[0] = mouse;
      prev_time[1] = prev_time[2] = -100;
      ball->Update();
      last_time=time;
      inertia_mode=0;
      need_redraw = 1;
    }
    break;
  case MouseMove:
    {
      int xres=current_renderer->xres;
      int yres=current_renderer->yres;
      //double aspect=double(xres)/double(yres);

      if(!rot_point_valid)
	break;

      HVect mouse((2.0*x)/xres - 1.0,2.0*(yres-y*1.0)/yres - 1.0,0.0,1.0);
      prev_time[2] = prev_time[1];
      prev_time[1] = prev_time[0];
      prev_time[0] = time;
      ball->Mouse(mouse);
      ball->Update();

      prev_quat[2] = prev_quat[1];
      prev_quat[1] = prev_quat[0];
      prev_quat[0] = mouse;

      // now we should just sendthe view points through
      // the rotation (after centerd around the ball)
      // eyep lookat and up

      View tmpview(rot_view);

      Transform tmp_trans;
      HMatrix mNow;
      ball->Value(mNow);
      tmp_trans.set(&mNow[0][0]);

      Transform prv = prev_trans;
      prv.post_trans(tmp_trans);

      HMatrix vmat;
      prv.get(&vmat[0][0]);

      Point y_a(vmat[0][1],vmat[1][1],vmat[2][1]);
      Point z_a(vmat[0][2],vmat[1][2],vmat[2][2]);

      tmpview.up(y_a.vector());
      tmpview.eyep((z_a*(eye_dist)) + tmpview.lookat().vector());

      view.set(tmpview);
      need_redraw=1;
      update_mode_string("rotate:");

      last_time=time;
      inertia_mode=0;
    }
    break;
  case MouseEnd:
    if(time-last_time < 20){
      // now setup the normalized quaternion
 

      View tmpview(rot_view);
	    
      Transform tmp_trans;
      HMatrix mNow;
      ball->Value(mNow);
      tmp_trans.set(&mNow[0][0]);
	    
      Transform prv = prev_trans;
      prv.post_trans(tmp_trans);
	    
      HMatrix vmat;
      prv.get(&vmat[0][0]);
	    
      Point y_a(vmat[0][1],vmat[1][1],vmat[2][1]);
      Point z_a(vmat[0][2],vmat[1][2],vmat[2][2]);
	    
      tmpview.up(y_a.vector());
      tmpview.eyep((z_a*(eye_dist)) + tmpview.lookat().vector());
	    
      view.set(tmpview);
      prev_trans = prv;

      // now you need to use the history to 
      // set up the arc you want to use...

      ball->Init();
      double rad = 0.8;
      HVect center(0,0,0,1.0);

      ball->Place(center,rad);

      int index=2;

      if (prev_time[index] == -100)
	index = 1;

      ball->vDown = prev_quat[index];
      ball->vNow  = prev_quat[0];
      ball->dragging = 1;
      ball->Update();
	    
      ball->qNorm = ball->qNow.Conj();
      double mag = ball->qNow.VecMag();

      // Go into inertia mode...
      inertia_mode=1;
      need_redraw=1;

      if (mag < 0.00001) { // arbitrary ad-hoc threshold
	inertia_mode = 0;
	need_redraw = 1;
      }
      else {
	double c = 1.0/mag;
	double dt = prev_time[0] - prev_time[index];// time between last 2 events
	ball->qNorm.x *= c;
	ball->qNorm.y *= c;
	ball->qNorm.z *= c;
	angular_v = 2*acos(ball->qNow.w)*1000.0/dt;
	cerr << dt << endl;
      }
    } else {
      inertia_mode=0;
    }
    ball->EndDrag();
    rot_point_valid = 0; // so we don't have to draw this...
    need_redraw = 1;     // always update this...
    update_mode_string("");
    break;
  }
}

// -- BAWGL -- 
static int prevPrinc;
void Roe::bawgl_pick(int action, int iv[3], GLfloat fv[3])
{
  BState bs;
  switch(action) {
  case BAWGL_PICK_START:
    {
	    
      current_renderer->get_pick(manager, this, iv[0], iv[1],
				 pick_obj, pick_pick, pick_n); 
      if (pick_obj){
	update_mode_string(pick_obj);
	pick_pick->set_picked_obj(pick_obj);
	pick_pick->pick(this,bs);
	total_x=0;
	total_y=0;
	total_z=0;
	//need_redraw=1;
      } else {
	update_mode_string("pick: none");
      }

    }
    break;
  case BAWGL_PICK_MOVE:
    {
      if (!pick_obj || !pick_pick) break;
      Vector dir(fv[0],fv[1],fv[2]);
      //float dv= sqrt(fv[0]*fv[0]+fv[1]*fv[1]+fv[2]*fv[2]);
      //pick_pick->moved(0, dv, dir, bs);

      double maxdot=0;
      int prin_dir=-1;
      for (int i=0; i<pick_pick->nprincipal(); i++) {
	double pdot=Dot(dir, pick_pick->principal(i));
	if(pdot > maxdot){
	  maxdot=pdot;
	  prin_dir=i;
	}
      }
      if(prin_dir != -1){
	prevPrinc= prin_dir;
	double dist=dir.length();
	Vector mtn(pick_pick->principal(prin_dir)*dist);
	total_x+=mtn.x();
	total_y+=mtn.y();
	total_z+=mtn.z();
	if (Abs(total_x) < .0001) total_x=0;
	if (Abs(total_y) < .0001) total_y=0;
	if (Abs(total_z) < .0001) total_z=0;
	update_mode_string(pick_obj);
	pick_pick->moved(prin_dir, dist, mtn, bs);
      } else {
	update_mode_string("pick: Bad direction...");
      }
    }
    break;
  case BAWGL_PICK_END:
    {
      if(pick_pick){
	pick_pick->release( bs );
      }
      pick_pick=0;
      pick_obj=0;
      update_mode_string("");
    }
    break;
  }
}
// -- BAWGL --

void Roe::mouse_pick(int action, int x, int y, int state, int btn, int)
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
				 pick_obj, pick_pick, pick_n);

      if (pick_obj){
	update_mode_string(pick_obj);
	pick_pick->set_picked_obj(pick_obj);
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
	update_mode_string(pick_obj);
	pick_pick->moved(prin_dir, dist, mtn, bs);
	need_redraw=1;
      } else {
	update_mode_string("pick: Bad direction...");
      }
      last_x = x;
      last_y = y;
    }
    break;
  case MouseEnd:
    if(pick_pick){
      pick_pick->release( bs );
      need_redraw=1;
    }
    pick_pick=0;
    pick_obj=0;
    update_mode_string("");
    break;
  }
}

void Roe::redraw_if_needed()
{
  if(need_redraw){
    need_redraw=0;
    redraw();
  }
}

void Roe::tcl_command(TCLArgs& args, void*)
{
  if (args.count() < 2) {
    args.error("Roe needs a minor command");
    return;
  }
  
  if (args[1] == "sgi_defined") {
    clString result("");
#ifdef __sgi
#if (_MIPS_SZPTR == 64)
    result += "2";
#else
    result += "1";
#endif
#else
    result += "0";
#endif
    args.result( result );
  } else if (args[1] == "dump_roe") {
    if (args.count() != 4) {
      args.error("Roe::dump_roe needs an output file name and type");
      return;
    }
    // We need to dispatch this one to the
    // remote thread.  We use an ID string
    // instead of a pointer in case this
    // roe gets killed by the time the
    // redraw message gets dispatched.
    manager->mailbox.send(scinew
			  SalmonMessage(MessageTypes::RoeDumpImage, id, args[2], args[3]));
  }else if (args[1] == "startup") {
    // Fill in the visibility database...
    GeomIndexedGroup::IterIntGeomObj iter = manager->ports.getIter();
    
    for ( ; iter.first != iter.second; iter.first++) {
      
      GeomIndexedGroup::IterIntGeomObj serIter =
	((GeomSalmonPort*)((*iter.first).second))->getIter();
      
      for ( ; serIter.first != serIter.second; serIter.first++) {
	GeomSalmonItem *si =
	  (GeomSalmonItem*)((*serIter.first).second);
	itemAdded(si);
      }
    }
  }
  else if (args[1] == "setrenderer") {
    if (args.count() != 6) {
      args.error("setrenderer needs a renderer name, etc");
      return;
    }
    Renderer* r=get_renderer(args[2]);
    if (!r) {
      args.error("Unknown renderer!");
      return;
    }
    if (current_renderer) current_renderer->hide();
    current_renderer=r;
    args.result(r->create_window(this, args[3], args[4], args[5]));
  } else if (args[1] == "redraw") {
    // We need to dispatch this one to the
    // remote thread We use an ID string
    // instead of a pointer in case this
    // roe gets killed by the time the
    // redraw message gets dispatched.
    if(!manager->mailbox.trySend(scinew SalmonMessage(id)))
      cerr << "Redraw event dropped, mailbox full!\n";
  } else if(args[1] == "destroy"){
    manager->delete_roe(this);
  } else if(args[1] == "anim_redraw"){
    // We need to dispatch this one to the
    // remote thread We use an ID string
    // instead of a pointer in case this
    // roe gets killed by the time the
    // redraw message gets dispatched.
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
    if(!manager->mailbox.trySend(scinew SalmonMessage(id, tbeg, tend,
						      nframes, framerate)))
      cerr << "Redraw event dropped, mailbox full!\n";
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
  } else if(args[1] == "Snap") {
    View sv(view.get());
    
    // determine closest eyep position
    Vector lookdir(sv.eyep() - sv.lookat());
    cerr << "lookdir = " << lookdir << endl;
    double distance = lookdir.length();
    
    double x = lookdir.x();
    double y = lookdir.y();
    double z = lookdir.z();
    
    // determine closest up vector position
    if( fabs(x) > fabs(y)) {
      if( fabs(x) > fabs(z)) {
	if(lookdir.x() < 0.0) {
	  distance *= -1;
	  sv.eyep(Point(distance, 0.0, 0.0, 1.0));
	} else {
	  sv.eyep(Point(distance, 0.0, 0.0, 1.0));
	}
      } else if (fabs(z) > fabs(y)) {
	if(lookdir.z() < 0.0) {
	  distance *= -1;
	  sv.eyep(Point(0.0, 0.0, distance, 1.0));
	} else {
	  sv.eyep(Point(0.0, 0.0, distance, 1.0)); 
	}
      }
    } else if( fabs(y) > fabs(z)) {
      if(lookdir.y() < 0.0) {
	distance *= -1;
	sv.eyep(Point(0.0, distance, 0.0, 1.0));
      } else {
	sv.eyep(Point(0.0, distance, 0.0, 1.0));
      }
    } else {
      if(lookdir.z() < 0.0) {
	distance *= -1;
        sv.eyep(Point(0.0, 0.0, distance, 1.0)); 
      } else {
	sv.eyep(Point(0.0, 0.0, distance, 1.0));   
      }
    }
    
    x = sv.up().x();
    y = sv.up().y();
    z = sv.up().z();
    Vector v;
    
    // determine closest up vector position
    if( fabs(x) > fabs(y)) {
      if( fabs(x) > fabs(z)) {
	if(sv.up().x() < 0.0) {
	  Vector v(-1.0, 0.0, 0.0);
	} else {
	  Vector v(1.0, 0.0, 0.0);
	}
      } else if( fabs(z) > fabs(y)) {
	if(sv.up().z() < 0.0) {
	  Vector v(0.0, 0.0, -1.0);
	} else {
	  Vector v(0.0, 0.0, 1.0);
	}
      }
    } else if( fabs(y) > fabs(z)) {
      if(sv.up().y() < 0.0) {
	Vector v(0.0, -1.0, 0.0);
      } else {
	Vector v(0.0, 1.0, 0.0);
      }
    } else {
      if(sv.up().z() < 0.0) {
	Vector v(0.0, 0.0, -1.0);
      } else {
	Vector v(0.0, 0.0, 1.0);
      }
    }
    Vector lookdir2(sv.eyep() - sv.lookat());
    cerr << "lookdir = " << lookdir2 << endl;
    sv.up(v);   // set the up vector
    animate_to_view(sv, 2.0); 
  } else if(args[1] == "Views") {
    View df(view.get());
    // position tells first which axis to look down 
    // (with x1 being the positive x axis and x0 being
    // the negative x axis) and then which axis is up
    // represented the same way
    clString position = pos.get();
    Vector lookdir(df.eyep()-df.lookat()); 
    double distance = lookdir.length();
    if(position == "x1_y1") {
      df.eyep(Point(distance, 0.0, 0.0, 1.0));
      Vector v(0.0, 1.0, 0.0);
      df.up(v);
    } else if(position == "x1_y0") {
      df.eyep(Point(distance, 0.0, 0.0, 1.0));
      Vector v(0.0, -1.0, 0.0);
      df.up(v);
    } else if(position == "x1_z1") {
      df.eyep(Point(distance, 0.0, 0.0, 1.0));
      Vector v(0.0, 0.0, 1.0);
      df.up(v);
    } else if(position == "x1_z0") {
      df.eyep(Point(distance, 0.0, 0.0, 1.0));
      Vector v(0.0, 0.0, -1.0);
      df.up(v);
    } else if(position == "x0_y1") {
      distance *= -1;
      df.eyep(Point(distance, 0.0, 0.0, 1.0));
      Vector v(0.0, 1.0, 0.0);
      df.up(v);
    } else if(position == "x0_y0") {
      distance *= -1;
      df.eyep(Point(distance, 0.0, 0.0, 1.0));
      Vector v(0.0, -1.0, 0.0);
      df.up(v);
    } else if(position == "x0_z1") {
      distance *= -1;
      df.eyep(Point(distance, 0.0, 0.0, 1.0));
      Vector v(0.0, 0.0, 1.0);
      df.up(v);
    } else if(position == "x0_z0") {
      distance *= -1;
      df.eyep(Point(distance, 0.0, 0.0, 1.0));
      Vector v(0.0, 0.0, -1.0);
      df.up(v);
    } else if(position == "y1_x1") {
      df.eyep(Point(0.0, distance, 0.0, 1.0));
      Vector v(1.0, 0.0, 0.0);
      df.up(v);
    } else if(position == "y1_x0") {
      df.eyep(Point(0.0, distance, 0.0, 1.0));
      Vector v(-1.0, 0.0, 0.0);
      df.up(v);
    } else if(position == "y1_z1") {
      df.eyep(Point(distance, 0.0, 0.0, 1.0));
      Vector v(0.0, 0.0, 1.0);
      df.up(v);
    } else if(position == "y1_z0") {
      df.eyep(Point(distance, 0.0, 0.0, 1.0));
      Vector v(0.0, 0.0, -1.0);
      df.up(v);
    } else if(position == "y0_x1") {
      distance *= -1;
      df.eyep(Point(0.0, distance, 0.0, 1.0));
      Vector v(1.0, 0.0, 0.0);
      df.up(v);
    } else if(position == "y0_x0") {
      distance *= -1;
      df.eyep(Point(0.0, distance, 0.0, 1.0));
      Vector v(-1.0, 0.0, 0.0);
      df.up(v);
    } else if(position == "y0_z1") {
      distance *= -1;
      df.eyep(Point(0.0, distance, 0.0, 1.0));
      Vector v(0.0, 0.0, 1.0);
      df.up(v);
    } else if(position == "y0_z0") {
      distance *= -1;
      df.eyep(Point(0.0, distance, 0.0, 1.0));
      Vector v(0.0, 0.0, -1.0);
      df.up(v);
    } else if(position == "z1_x1") {
      df.eyep(Point(0.0, 0.0, distance, 1.0));
      Vector v(1.0, 0.0, 0.0);
      df.up(v);
    } else if(position == "z1_x0") {
      df.eyep(Point(0.0, 0.0, distance, 1.0));
      Vector v(-1.0, 0.0, 0.0);
      df.up(v);
    } else if(position == "z1_y1") {
      df.eyep(Point(0.0, 0.0, distance, 1.0));
      Vector v(0.0, 1.0, 0.0);
      df.up(v);
    } else if(position == "z1_y0") {
      df.eyep(Point(distance, 0.0, 0.0, 1.0));
      Vector v(0.0, -1.0, 0.0);
      df.up(v);
    } else if(position == "z0_x1") {
      distance *= -1;
      df.eyep(Point(0.0, 0.0, distance, 1.0));
      Vector v(1.0, 0.0, 0.0);
      df.up(v);
    } else if(position == "z0_x0") {
      distance *= -1;
      df.eyep(Point(0.0, 0.0, distance, 1.0));
      Vector v(-1.0, 0.0, 0.0);
      df.up(v);
    } else if(position == "z0_y1") {
      distance *= -1;
      df.eyep(Point(0.0, 0.0, distance, 1.0));
      Vector v(0.0, 1.0, 0.0);
      df.up(v);
    } else if(position == "z0_y0") {
      distance *= -1;
      df.eyep(Point(0.0, 0.0, distance, 1.0));
      Vector v(0.0, -1.0, 0.0);
      df.up(v);
    }
    animate_to_view(df, 2.0);
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
    Vector lookdir(cv.eyep()-cv.lookat());
    lookdir*=amount;
    cv.eyep(cv.lookat()+lookdir);
    animate_to_view(cv, 1.0);
  } else if(args[1] == "dolly2"){
    if(args.count() != 3){
      args.error("dolly2 needs an amount");
      return;
    }
    double amount;
    if(!args[2].get_double(amount)){
      args.error("Can't figure out amount");
      return;
    }
    View cv(view.get());
    Vector lookdir(cv.eyep()-cv.lookat());
    amount = amount-1;
    lookdir*=amount;
    cv.eyep(cv.eyep()+lookdir);
    cv.lookat(cv.lookat()+lookdir);
    animate_to_view(cv, 1.0);
  } else if(args[1] == "saveobj") {
    if(args.count() != 4){
      args.error("Roe::dump_roe needs an output file name and format!");
      return;
    }
    // We need to dispatch this one to the
    // remote thread We use an ID string
    // instead of a pointer in case this
    // roe gets killed by the time the
    // redraw message gets dispatched.
    manager->mailbox.send(scinew SalmonMessage(MessageTypes::RoeDumpObjects,
					       id, args[2], args[3]));
  } else if(args[1] == "listvisuals"){
    current_renderer->listvisuals(args);
  } else if(args[1] == "switchvisual"){
    if(args.count() != 6){
      args.error("switchvisual needs a window name, a visual index, a width and a height");
      return;
    }
    int idx;
    if(!args[3].get_int(idx)){
      args.error("bad index for switchvisual");
      return;
    }
    int width;
    if(!args[4].get_int(width)){
      args.error("Bad width");
      return;
    }
    int height;
    if(!args[5].get_int(height)){
      args.error("Bad height");
      return;
    }
    current_renderer->setvisual(args[2], idx, width, height);
    // --  BAWGL -- 
  } else if(args[1] == "startbawgl") {
    if( bawgl->start(this, "bench.config")  == 0 )
    {
      bawgl_error = 0;
    }
    else
    {
      do_bawgl.set(0);
      bawgl_error = 1;
      args.error("Bummer!\n Check if the device daemons are alive!");
    }
  } else if(args[1] == "stopbawgl"){
    if( !bawgl_error ) bawgl->stop();
    // --  BAWGL -- 
  } else if(args[1] == "centerGenAxes") { 
    if(caxes.get() == 1) {  // checked
      roe_objs_draw[0] = 1;
    } else {    // unchecked
      roe_objs_draw[0] = 0;
    }
  } else if(args[1] == "iconGenAxes") {    
    if(iaxes.get() == 1) {  // checked
    } 
    else {   // unchecked 
    
    }

  }else
    args.error("Unknown minor command '" + args[1] + "' for Roe");
}


void Roe::do_mouse(MouseHandler handler, TCLArgs& args)
{
  if(args.count() != 5 && args.count() != 7 && args.count() != 8 && args.count() != 6){
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
  int time;
  if(args.count() == 8){
    if(!args[7].get_int(time)){
      args.error("err parsing time");
      return;
    }
  }
  if(args.count() == 6){
    if(!args[5].get_int(time)){
      args.error("err parsing time");
      return;
    }
  }

  // We have to send this to the salmon thread...
  if(!manager->mailbox.trySend(scinew RoeMouseMessage(id, handler, action, x, y, state, btn, time)))
    cerr << "Mouse event dropped, mailbox full!\n";
}

void Roe::autoview(const BBox& bbox)
{
  if(bbox.valid()){
    View cv(view.get());
    // Animate lookat point to center of BBox...
    cv.lookat(bbox.center());
    animate_to_view(cv, 2.0);
        
    // Move forward/backwards until entire view is in scene...

    // change this a little, make it so that the FOV must
    // be 60 deg...

    // I'm changing this to be 20 degrees - Dave

    double myfov=20.0;

    Vector diag(bbox.diagonal());
    double w=diag.length();
    Vector lookdir(cv.lookat()-cv.eyep()); 
    lookdir.normalize();
    const double scale = 1.0/(2*Tan(DtoR(myfov/2.0)));
    double length = w*scale;
    cv.fov(myfov);
    cv.eyep(cv.lookat() - lookdir*length);
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

void Roe::update_mode_string(GeomObj* pick_obj)
{
  clString ms="pick: ";
  GeomSalmonItem* si=dynamic_cast<GeomSalmonItem*>(pick_obj);
  if(!si){
    ms+="not a GeomSalmonItem?";
  } else {
    ms+=si->name;
  }
  if(pick_n != 0x12345678)
    ms+=", index="+to_string(pick_n);
  update_mode_string(ms);
}

void Roe::update_mode_string(const clString& msg)
{
  ostringstream str;
  str << id << " updateMode \"" << msg << "\"";
  TCL::execute(str.str().c_str());
}

RoeMouseMessage::RoeMouseMessage(const clString& rid, MouseHandler handler,
				 int action, int x, int y, int state, int btn,
				 int time)
  : MessageBase(MessageTypes::RoeMouse), rid(rid), handler(handler),
    action(action), x(x), y(y), state(state), btn(btn), time(time)
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
  MapClStringRenderer::iterator riter;

  riter = renderers.find(name);
  if (riter == renderers.end()) { // if not found
    // Create it...
    r = Renderer::create(name);
    if (r) {
      renderers[name] = r;
    }
  }
  else {
    r = (*riter).second;
  }
  return r;
}

void Roe::force_redraw()
{
  need_redraw=1;
}

void Roe::do_for_visible(Renderer* r, RoeVisPMF pmf)
{
				// Do internal objects first...
  int i;
  for (i = 0; i < roe_objs.size(); i++){
    if(roe_objs_draw[i] == 1) {
      (r->*pmf)(manager, this, roe_objs[i]);
    }
  }

  Array1<GeomSalmonItem*> transp_objs; // transparent objects - drawn last

  GeomIndexedGroup::IterIntGeomObj iter = manager->ports.getIter();
  
  for ( ; iter.first != iter.second; iter.first++) {
      
    GeomIndexedGroup::IterIntGeomObj serIter = 
      ((GeomSalmonPort*)((*iter.first).second))->getIter();
    
    for ( ; serIter.first != serIter.second; serIter.first++) {
	    
      GeomSalmonItem *si =
	(GeomSalmonItem*)((*serIter.first).second);
      
      // Look up the name to see if it should be drawn...
      ObjTag* vis;
      
      viter = visible.find(si->name);
      if (viter != visible.end()) { // if found
	vis = (*viter).second;
	if (vis->visible->get()) {
	  if (strstr(si->name(),"TransParent")) { // delay drawing
	    transp_objs.add(si);
	  }
	  else {
	    if(si->lock)
	      si->lock->readLock();
	    (r->*pmf)(manager, this, si);
	    if(si->lock)
	      si->lock->readUnlock();
	  }
	}
      }
      else {
	cerr << "Warning: object " << si->name << " not in visibility database...\n";
      }
    }
  }

  // now run through the transparent objects...

  for(i=0;i<transp_objs.size();i++) {
    GeomSalmonItem *si = transp_objs[i];    

    if(si->lock)
      si->lock->readLock();
    (r->*pmf)(manager, this, si);
    if(si->lock)
      si->lock->readUnlock();
  }

  // now you are done...

}

void Roe::set_current_time(double time)
{
  set_tclvar(id, "current_time", to_string(time));
}

void Roe::dump_objects(const clString& filename, const clString& format)
{
  if(format == "scirun_binary" || format == "scirun_ascii"){
    Piostream* stream;
    if(format == "scirun_binary")
      stream=new BinaryPiostream(filename, Piostream::Write);
    else
      stream=new TextPiostream(filename, Piostream::Write);
    if(stream->error()){
      delete stream;
      return;
    }
    manager->geomlock.readLock();
    GeomScene scene(bgcolor.get(), view.get(), &manager->lighting,
		    &manager->ports);
    SCICore::PersistentSpace::Pio(*stream, scene);
    if(stream->error()){
      cerr << "Error writing geom file: " << filename << endl;
    } else {
      cerr << "Done writing geom file: " << filename << endl;
    }
    delete stream;
    manager->geomlock.readUnlock();
  } else {
    cerr << "WARNING: format " << format << " not supported!\n";
  }
}

void Roe::getData(int datamask, FutureValue<GeometryData*>* result)
{
  if(current_renderer){
    cerr << "calling current_renderer->getData\n";
    current_renderer->getData(datamask, result);
    cerr << "current_renderer...\n";
  } else {
    result->send(0);
  }
}

void Roe::setView(View newView) {
  view.set(newView);
  manager->mailbox.send(scinew SalmonMessage(id)); // Redraw
}

GeomGroup* Roe::createGenAxes() {     
  
  MaterialHandle dk_red = scinew Material(Color(0,0,0), Color(.2,0,0),
					  Color(.5,.5,.5), 20);
  MaterialHandle dk_green = scinew Material(Color(0,0,0), Color(0,.2,0),
					    Color(.5,.5,.5), 20);
  MaterialHandle dk_blue = scinew Material(Color(0,0,0), Color(0,0,.2),
					   Color(.5,.5,.5), 20);
  MaterialHandle lt_red = scinew Material(Color(0,0,0), Color(.8,0,0),
					  Color(.5,.5,.5), 20);
  MaterialHandle lt_green = scinew Material(Color(0,0,0), Color(0,.8,0),
					    Color(.5,.5,.5), 20);
  MaterialHandle lt_blue = scinew Material(Color(0,0,0), Color(0,0,.8),
					   Color(.5,.5,.5), 20);

  GeomGroup* xp = scinew GeomGroup; 
  GeomGroup* yp = scinew GeomGroup;
  GeomGroup* zp = scinew GeomGroup;
  GeomGroup* xn = scinew GeomGroup;
  GeomGroup* yn = scinew GeomGroup;
  GeomGroup* zn = scinew GeomGroup;

  double sz = 1.0;
  xp->add(scinew GeomCylinder(Point(0,0,0), Point(sz, 0, 0), sz/20));
  xp->add(scinew GeomCone(Point(sz, 0, 0), Point(sz+sz/5, 0, 0), sz/10, 0));
  yp->add(scinew GeomCylinder(Point(0,0,0), Point(0, sz, 0), sz/20));
  yp->add(scinew GeomCone(Point(0, sz, 0), Point(0, sz+sz/5, 0), sz/10, 0));
  zp->add(scinew GeomCylinder(Point(0,0,0), Point(0, 0, sz), sz/20));
  zp->add(scinew GeomCone(Point(0, 0, sz), Point(0, 0, sz+sz/5), sz/10, 0));
  xn->add(scinew GeomCylinder(Point(0,0,0), Point(-sz, 0, 0), sz/20));
  xn->add(scinew GeomCone(Point(-sz, 0, 0), Point(-sz-sz/5, 0, 0), sz/10, 0));
  yn->add(scinew GeomCylinder(Point(0,0,0), Point(0, -sz, 0), sz/20));
  yn->add(scinew GeomCone(Point(0, -sz, 0), Point(0, -sz-sz/5, 0), sz/10, 0));
  zn->add(scinew GeomCylinder(Point(0,0,0), Point(0, 0, -sz), sz/20));
  zn->add(scinew GeomCone(Point(0, 0, -sz), Point(0, 0, -sz-sz/5), sz/10, 0));
  GeomGroup* all=scinew GeomGroup;
  all->add(scinew GeomMaterial(xp, lt_red));
  all->add(scinew GeomMaterial(yp, lt_green));
  all->add(scinew GeomMaterial(zp, lt_blue));
  all->add(scinew GeomMaterial(xn, dk_red));
  all->add(scinew GeomMaterial(yn, dk_green));
  all->add(scinew GeomMaterial(zn, dk_blue));
  
  return all;
}

} // End namespace Modules
} // End namespace PSECommon
