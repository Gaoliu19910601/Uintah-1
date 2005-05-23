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
 *  Axes.h: Displayable 2D object
 *
 *  Written by:
 *   Yarden Livnat
 *   Department of Computer Science
 *   University of Utah
 *   Aug 2001
 *
 *  Copyright (C) 2001 SCI Group
 */

#ifndef SCI_Axes_h
#define SCI_Axes_h 

#include <Core/GuiInterface/TclObj.h>
#include <Core/2d/Polyline.h>
#include <Core/2d/AxesObj.h>

namespace SCIRun {

class Diagram;

class XAxis :  public TclObj, public XAxisObj {
protected:
  Array1< Polyline *> poly_;
  Diagram *parent_;
  int activepoly_;
  bool initialized_;

public:
  
  XAxis(GuiInterface* gui) : TclObj(gui, "xaxis"), XAxisObj("XAxis") {}
  XAxis(GuiInterface* gui, Diagram *, const string &name="XAxis" );
  virtual ~XAxis();

  void update();

  virtual void select( double x, double y, int b );
  virtual void move( double x, double y, int b );
  virtual void release( double x, double y, int b );

  // For OpenGL
#ifdef SCI_OPENGL
  virtual void draw( bool = false );
#endif
  static PersistentTypeID type_id;
  
  virtual void io(Piostream&);    
  
};  

class YAxis :  public TclObj, public YAxisObj {
protected:
  Array1< Polyline *> poly_;
  Diagram *parent_;
  int activepoly_;
  bool initialized_;

public:
  
  YAxis(GuiInterface* gui) : TclObj(gui, "yaxis"), YAxisObj("YAxis") {}
  YAxis(GuiInterface* gui, Diagram *, const string &name="YAxis" );
  virtual ~YAxis();

  void update();

  virtual void select( double x, double y, int b );
  virtual void move( double x, double y, int b );
  virtual void release( double x, double y, int b );

  // For OpenGL
#ifdef SCI_OPENGL
  virtual void draw( bool = false );
#endif
  static PersistentTypeID type_id;
  
  virtual void io(Piostream&);    
  
};

void Pio(Piostream&, XAxis*&);
void Pio(Piostream&, YAxis*&);

} // namespace SCIRun

#endif // SCI_Axes_h
