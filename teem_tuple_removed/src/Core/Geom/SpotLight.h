/*
  The contents of this file are subject to the University of Utah Public
  License (the "License"); you may not use this file except in compliance
  with the License.
  
  Software distributed under the License is distributed on an "AS IS"
  basis, WITHOUT WARRANTY OF ANY KIND, either express or implied. See the
  License for the specific language governing rights and limitations under
  the License.
  
  The Original Source Code is SCIRun, released March 12, 2001.
  
  The Original Source Code was developed by the University of Utah.
  Portions created by UNIVERSITY are Copyright (C) 2001, 1994 
  University of Utah. All Rights Reserved.
*/


/*
 *  SpotLight.h:  A Spot light source
 *
 *  Written by:
 *   Steven G. Parker
 *   Department of Computer Science
 *   University of Utah
 *   September 1994
 *
 *  Copyright (C) 1994 SCI Group
 */

#ifndef SCI_Geom_SpotLight_h
#define SCI_Geom_SpotLight_h 1

#include <Core/Geom/Light.h>
#include <Core/Datatypes/Color.h>
#include <Core/Geometry/Vector.h>

namespace SCIRun {

class SCICORESHARE SpotLight : public Light {
    Point p;
    Vector v;
    float cutoff; // must be [0-90], or 180
    Color c;
public:
    SpotLight(const string& name, const Point&, const Vector&, 
	      float, const Color&, bool on = true, bool transformed = true);
    virtual ~SpotLight();
    virtual void io(Piostream&);
    static PersistentTypeID type_id;
#ifdef SCI_OPENGL
    virtual void opengl_setup(const View& view, DrawInfoOpenGL*, int& idx);
#endif
  void move( const Point& newP) { p = newP; }
  void setDirection( const Vector& newV) {v = newV; }
  void setCutoff( float co ) { 
    if ( co >=0 && co <=90 ) 
      cutoff = co;
    else
      cutoff = 180;
  }
  void setColor( const Color& newC) { c = newC; }
};

} // End namespace SCIRun


#endif /* SCI_Geom_SpotLight_h */
