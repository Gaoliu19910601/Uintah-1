/*

The MIT License

Copyright (c) 1997-2009 Center for the Simulation of Accidental Fires and 
Explosions (CSAFE), and  Scientific Computing and Imaging Institute (SCI), 
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



#ifndef NORMALMAP_MAT_H
#define NORMALMAP_MAT_H 1

#include <Packages/rtrt/Core/Material.h>
#include <Packages/rtrt/Core/Color.h>
#include <Packages/rtrt/Core/LambertianMaterial.h> 
#include <Packages/rtrt/Core/BumpObject.h> 

namespace rtrt {
class NormalMapMaterial;
}

namespace SCIRun {
void Pio(Piostream&, rtrt::NormalMapMaterial*&);
}

namespace rtrt {

class NormalMapMaterial : public Material {
  Material *material;
  //added for bumpmaps from file
  int dimension_x;             /* width and height*/
  int dimension_y;
  Vector *normalmapimage;      /*holds the bump structure*/
  double evaru, evarv; /* 1/width, 1/height */
  double persistence;
public:
  FILE* readcomments(FILE *fin);
  int readfromppm6(char *filename);
  int readfromppm(char *filename);
  NormalMapMaterial(Material *, char *, double);
  virtual ~NormalMapMaterial();

  NormalMapMaterial() : Material() {} // for Pio.

  //! Persistent I/O.
  static  SCIRun::PersistentTypeID type_id;
  virtual void io(SCIRun::Piostream &stream);
  friend void SCIRun::Pio(SCIRun::Piostream&, NormalMapMaterial*&);

  virtual void shade(Color& result, const Ray& ray,
		     const HitInfo& hit, int depth,
		     double atten, const Color& accumcolor,
		     Context* cx);
  void perturbnormal(Vector &, const Ray &, const HitInfo &);

  // added for file bumpmaps
  int read_file(char *filename);
  Vector fval(double u, double v);
  double get_persistence() {return persistence;}

};

} // end namespace rtrt

#endif
