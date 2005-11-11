/*
  For more information, please see: http://software.sci.utah.edu

  The MIT License

  Copyright (c) 2004 Scientific Computing and Imaging Institute,
  University of Utah.

  License for the specific language governing rights and limitations under
  Permission is hereby granted, free of charge, to any person obtaining a
  copy of this software and associated documentation files (the "Software"),
  to deal in the Software without restriction, including without limitation
  the rights to use, copy, modify, merge, publish, distribute, sublicense,
  and/or sell copies of the Software, and to permit persons to whom the
  Software is furnished to do so, subject to the following conditions:

  The above copyright notice and this permission notice shall be included
  in all copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
  OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
  THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
  FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
  DEALINGS IN THE SOFTWARE.
*/


/*
 *  DrawInfoOpenGL.cc: OpenGL State Machine
 *
 *  Written by:
 *   McKay Davis
 *   Scientific Computing and Imaging Institute
 *   University of Utah
 *   November, 2005
 *
 *  Copyright (C) 2005 Scientific Computing and Imaging Institute
 */

#include <Core/Geom/DrawInfoOpenGL.h>
#include <Core/Geom/GeomDL.h>
#include <Core/Geom/Material.h>
#include <Core/Math/MiscMath.h>
#include <iostream>

#ifndef _WIN32
#include <X11/X.h>
#include <X11/Xlib.h>
#else
#include <windows.h>
#define GL_TEXTURE_3D 0x806F
#define GL_TEXTURE0 0x84C0
#define GL_TEXTURE1 0x84C1
#define GL_TEXTURE2 0x84C2
typedef void (GLAPIENTRY * PFNGLACTIVETEXTUREPROC) (GLenum texture);
typedef void (GLAPIENTRY * PFNGLMULTITEXCOORD2FVPROC) (GLenum target, const GLfloat *v);
typedef void (GLAPIENTRY * PFNGLMULTITEXCOORD3FPROC) (GLenum target, GLfloat s, GLfloat t, GLfloat r);

static PFNGLACTIVETEXTUREPROC glActiveTexture = 0;
static PFNGLMULTITEXCOORD2FVPROC glMultiTexCoord2fv = 0;
static PFNGLMULTITEXCOORD3FPROC glMultiTexCoord3f = 0;

#endif


using std::map;
using std::cerr;
using std::endl;
using std::pair;

namespace SCIRun {

static void
quad_error(GLenum code)
{
  cerr << "WARNING: Quadric Error (" << (char*)gluErrorString(code) << ")" << endl;
}


// May need to do this for really old GCC compilers?
//typedef void (*gluQuadricCallbackType)(...);
typedef void (*gluQuadricCallbackType)();

DrawInfoOpenGL::DrawInfoOpenGL() :
  polycount(0),
  lighting(1),
  show_bbox(0),
  currently_lit(1),
  pickmode(1),
  pickchild(0),
  npicks(0),
  fog(0),
  cull(0),
  dl(0),
  mouse_action(0),
  check_clip(1),
  clip_planes(0),
  current_matl(0),
  ignore_matl(0),
  qobj(0),
  axis(0),
  dir(0),
  multiple_transp(0),
  ambient_scale_(1.0),
  diffuse_scale_(1.0),
  specular_scale_(1.0),
  emission_scale_(1.0),
  shininess_scale_(1.0),
  point_size_(1.0),
  line_width_(1.0),
  polygon_offset_factor_(0.0),
  polygon_offset_units_(0.0),
  using_cmtexture_(0),
  cmtexture_(0)
{
  for (int i=0; i < GEOM_FONT_COUNT; i++)
  {
    fontstatus[i] = 0;
    fontbase[i] = 0;
  }

  qobj=gluNewQuadric();

  if ( !qobj )
  {
    printf( "Error in GeomOpenGL.cc: DrawInfoOpenGL(): gluNewQuadric()\n" );
  }

#ifdef _WIN32
  gluQuadricCallback(qobj, /* FIX (GLenum)GLU_ERROR*/ 0, (void (__stdcall*)())quad_error);
#else
  gluQuadricCallback(qobj, (GLenum)GLU_ERROR, (gluQuadricCallbackType)quad_error);
#endif
}


void
DrawInfoOpenGL::reset()
{
  polycount=0;
  current_matl=0;
  ignore_matl=0;
  fog=0;
  cull=0;
  check_clip = 0;
  pickmode =0;
  pickchild =0;
  npicks =0;
}


DrawInfoOpenGL::~DrawInfoOpenGL()
{
  map<GeomDL *, pair<unsigned int, unsigned int> >::iterator loc;
  loc = dl_map_.begin();
  while (loc != dl_map_.end())
  {
    (*loc).first->dl_unregister(this);
    ++loc;
  }
}


void
DrawInfoOpenGL::set_drawtype(DrawType dt)
{
  drawtype = dt;
  switch(drawtype)
  {
  case DrawInfoOpenGL::WireFrame:
    gluQuadricDrawStyle(qobj, (GLenum)GLU_LINE);
    glPolygonMode(GL_FRONT_AND_BACK,GL_LINE);
    break;
  case DrawInfoOpenGL::Flat:
    gluQuadricDrawStyle(qobj, (GLenum)GLU_FILL);
    glShadeModel(GL_FLAT);
    glPolygonMode(GL_FRONT_AND_BACK,(GLenum)GL_FILL);
    break;
  case DrawInfoOpenGL::Gouraud:
    gluQuadricDrawStyle(qobj, (GLenum)GLU_FILL);
    glShadeModel(GL_SMOOTH);
    glPolygonMode(GL_FRONT_AND_BACK,(GLenum)GL_FILL);
    break;
  }
}


void
DrawInfoOpenGL::init_lighting(int use_light)
{
  if (use_light)
  {
    glEnable(GL_LIGHTING);
    switch(drawtype)
    {
    case DrawInfoOpenGL::WireFrame:
      gluQuadricNormals(qobj, (GLenum)GLU_SMOOTH);
      break;
    case DrawInfoOpenGL::Flat:
      gluQuadricNormals(qobj, (GLenum)GLU_FLAT);
      break;
    case DrawInfoOpenGL::Gouraud:
      gluQuadricNormals(qobj, (GLenum)GLU_SMOOTH);
      break;
    }
  }
  else
  {
    glDisable(GL_LIGHTING);
    gluQuadricNormals(qobj,(GLenum)GLU_NONE);
  }
  if (fog)
    glEnable(GL_FOG);
  else
    glDisable(GL_FOG);
  if (cull)
    glEnable(GL_CULL_FACE);
  else
    glDisable(GL_CULL_FACE);
}



void
DrawInfoOpenGL::init_clip(void)
{
  for (int num = 0; num < 6; num++) {
    GLdouble plane[4];
    planes[num].get(plane); 
    glClipPlane((GLenum)(GL_CLIP_PLANE0+num),plane); 
    if (check_clip && clip_planes & (1 << num))
      glEnable((GLenum)(GL_CLIP_PLANE0+num));
    else 
      glDisable((GLenum)(GL_CLIP_PLANE0+num));
  }
}


void
DrawInfoOpenGL::set_material(Material* matl)
{
  if (matl==current_matl || ignore_matl)
  {
    return;     
  }
  float color[4];
  (matl->ambient*ambient_scale_).get_color(color);
  glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT, color);
  (matl->diffuse*diffuse_scale_).get_color(color);
  if (matl->transparency < 1.0)
  {
    color[3] = matl->transparency * matl->transparency;
    color[3] *= color[3];
  }
  glColor4fv(color);
  (matl->specular*specular_scale_).get_color(color);
  glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, color);
  (matl->emission*emission_scale_).get_color(color);
  glMaterialfv(GL_FRONT_AND_BACK, GL_EMISSION, color);
  if (!current_matl || matl->shininess != current_matl->shininess)
  {
    glMaterialf(GL_FRONT_AND_BACK, GL_SHININESS, matl->shininess*shininess_scale_);
  }     
  current_matl=matl;
}


bool
DrawInfoOpenGL::dl_lookup(GeomDL *obj, unsigned int &state, unsigned int &dl)
{
  map<GeomDL *, pair<unsigned int, unsigned int> >::iterator loc;
  loc = dl_map_.find(obj);
  if (loc != dl_map_.end())
  {
    dl = (*loc).second.first;
    state = (*loc).second.second;
    return true;
  }
  return false;
}


bool
DrawInfoOpenGL::dl_addnew(GeomDL *obj, unsigned int state, unsigned int &dl)
{
  if (!dl_freelist_.empty())
  {
    dl = dl_freelist_.front();
    dl_freelist_.pop_front();
  }
  else
  {
    dl = glGenLists(1);
  }
  if (dl)
  {
    dl_map_[obj] = pair<unsigned int, unsigned int>(dl, state);
    obj->dl_register(this);
    return true;
  }
  return false;
}


bool
DrawInfoOpenGL::dl_update(GeomDL *obj, unsigned int state)
{
  map<GeomDL *, pair<unsigned int, unsigned int> >::iterator loc;
  loc = dl_map_.find(obj);
  if (loc != dl_map_.end())
  {
    (*loc).second.second = state;
    return true;
  }
  return false;
}


bool
DrawInfoOpenGL::dl_remove(GeomDL *obj)
{
  map<GeomDL *, pair<unsigned int, unsigned int> >::iterator loc;
  loc = dl_map_.find(obj);
  if (loc != dl_map_.end())
  {
    dl_freelist_.push_front((*loc).second.first);
    dl_map_.erase(loc);
    return true;
  }
  return false;
}


// this is for transparent rendering stuff.

void
DrawInfoOpenGL::init_view( double /*znear*/, double /*zfar*/,
                           Point& /*eyep*/, Point& /*lookat*/ )
{
  double model_mat[16]; // this is the modelview matrix

  glGetDoublev(GL_MODELVIEW_MATRIX,model_mat);

  // this is what you rip the view vector from
  // just use the "Z" axis, normalized
  view = Vector(model_mat[0*4+2],model_mat[1*4+2],model_mat[2*4+2]);

  view.normalize();

  // 0 is X, 1 is Y, 2 is Z
  dir = 1;

  if (Abs(view.x()) > Abs(view.y()))
  {
    if (Abs(view.x()) > Abs(view.z()))
    { // use x dir
      axis=0;
      if (view.x() < 0)
      {
        dir=-1;
      }
    }
    else
    { // use z dir
      axis=2;
      if (view.z() < 0)
      {
        dir=-1;
      }
    }
  }
  else if (Abs(view.y()) > Abs(view.z()))
  { // y greates
    axis=1;
    if (view.y() < 0)
    {
      dir=-1;
    }
  }
  else
  { // z is the one
    axis=2;
    if (view.z() < 0)
    {
      dir=-1;
    }
  }

  multiple_transp = 0;
}


bool
DrawInfoOpenGL::init_font(int a)
{
#ifndef _WIN32
  if (a > GEOM_FONT_COUNT || a < 0) { return false; }

  if ( fontstatus[a] == 0 )
  {
    dpy = XOpenDisplay( NULL );

    static const char *fontname[GEOM_FONT_COUNT] = {
      "-schumacher-clean-medium-r-normal-*-*-60-*-*-*-*-*",
      "-schumacher-clean-bold-r-normal-*-*-100-*-*-*-*-*",
      "-schumacher-clean-bold-r-normal-*-*-140-*-*-*-*-*",
      "-*-courier-bold-r-normal-*-*-180-*-*-*-*-*",
      "-*-courier-bold-r-normal-*-*-240-*-*-*-*-*"
    };

    XFontStruct* fontInfo = XLoadQueryFont(dpy, fontname[a]);
    if (fontInfo == NULL)
    {
      cerr << "DrawInfoOpenGL::init_font: font '" << fontname[a]
           << "' not found.\n";
      fontstatus[a] = 2;
      return false;
    }
    Font id = fontInfo->fid;
    unsigned int first = fontInfo->min_char_or_byte2;
    unsigned int last = fontInfo->max_char_or_byte2;

    fontbase[a] = glGenLists((GLuint) last+1);

    if (fontbase[a] == 0)
    {
      cerr << "DrawInfoOpenGL::init_font: Out of display lists.\n";
      fontstatus[a] = 2;
      return false;
    }
    glXUseXFont(id, first, last-first+1, fontbase[a]+first);
    fontstatus[a] = 1;
  }
  if (fontstatus[a] == 1)
  {
    return true;
  }
  return false;

#else // WIN32
  if (a > GEOM_FONT_COUNT || a < 0) { return false; }

  if ( fontstatus[a] == 0 )
  {
    HDC hDC = wglGetCurrentDC();
    if (!hDC)
      return false;

    DWORD first, count;

    // for now, just use the system font
    SelectObject(hDC,GetStockObject(SYSTEM_FONT));

    // rasterize the standard character set.
    first = 0;
    count = 256;
    fontbase[a] =  glGenLists(count);

    if (fontbase[a] == 0)
    {
      cerr << "DrawInfoOpenGL::init_font: Out of display lists.\n";
      fontstatus[a] = 2;
      return false;
    }
    wglUseFontBitmaps( hDC, first, count, fontbase[a]+first );
    fontstatus[a] = 1;
  }
  if (fontstatus[a] == 1)
  {
    return true;
  }
  return false;
#endif
}

}
