  
/*
 *  GeomOpenGL.h: Displayable Geometry
 *
 *  Written by:
 *   Steven G. Parker & David Weinstein
 *   Department of Computer Science
 *   University of Utah
 *   April 1994
 *
 *  Copyright (C) 1994 SCI Group
 */

#ifndef SCI_Geom_GeomOpenGL_h
#define SCI_Geom_GeomOpenGL_h 1

#ifdef _WIN32
#define WINGDIAPI __declspec(dllimport)
#define APIENTRY __stdcall
#define CALLBACK APIENTRY
#endif

#include <stddef.h>
#include <stdlib.h>
#include <GL/glu.h>
#include <GL/glx.h>

#include <sci_config.h>
#include <SCICore/Geometry/Vector.h>
#include <SCICore/Geometry/Point.h>

namespace PSECommon {
  namespace Modules {
    class Roe;
  }
}

namespace SCICore {
namespace GeomSpace {

using PSECommon::Modules::Roe;
using SCICore::Geometry::Vector;
using SCICore::Geometry::Point;

class Material;

const int CLIP_P0 = 1;
const int CLIP_P1 = 2;
const int CLIP_P2 = 4;
const int CLIP_P3 = 8;
const int CLIP_P4 = 16;
const int CLIP_P5 = 32;

const int MULTI_TRANSP_FIRST_PASS=2; // 1 is just if you are doing mpasses...

struct DrawInfoOpenGL {
    DrawInfoOpenGL();
    ~DrawInfoOpenGL();

    int polycount;
    enum DrawType {
	WireFrame,
	Flat,
	Gouraud
    };
private:
    DrawType drawtype;
public:
    void set_drawtype(DrawType dt);
    inline DrawType get_drawtype() {return drawtype;}

    void init_lighting(int use_light);
    void init_clip(void);
    int lighting;
    int currently_lit;
    int pickmode;
    int pickchild;
    int npicks;
    int fog;
    int cull;
    int dl;

    int check_clip; // see if you should ignore clipping planes
    
    int clip_planes; // clipping planes that are on
    double point_size; // so points and lines can be thicker than 1 pixel

    Material* current_matl;
    void set_matl(Material*);

    int ignore_matl;

    GLUquadricObj* qobj;

    Vector view;  // view vector...
    int axis;     // which axis you are working with...
    int dir;      // direction +/- -> depends on the view...

    double abs_val; // value wi/respect view
    double axis_val; // value wi/respect axis -> pt for comparison...

    double axis_delt; // delta wi/respect axis

    int multiple_transp; // if you have multiple transparent objects...

    void init_view(double znear, double zfar, Point& eyep, Point& lookat);

    Roe* roe;
#ifndef _WIN32
    Display *dpy;
#endif
    int debug;
    void reset();
};

} // End namespace GeomSpace
} // End namespace SCICore


#endif /* SCI_Geom_GeomOpenGL_h */

