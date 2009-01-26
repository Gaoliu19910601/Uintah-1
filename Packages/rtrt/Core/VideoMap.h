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



/**********************************************************************\
 *                                                                    *
 * filename: VideoMap.h                                               *
 * author  : R. Keith Morley                                          *
 * last mod: 07/07/02                                                 *
 *                                                                    *
 * VideoMap inherits from class Material.  It reads in an array of    *
 * ppm files and indexes into them based on the program running       *
 * time in order to get a time varying video tex.                     *
 *                                                                    *
\**********************************************************************/

#ifndef _VIDEOMAP_H_
#define _VIDEOMAP_H_ 1

#include <Packages/rtrt/Core/Material.h>
#include <Packages/rtrt/Core/Array1.h>
#include <Packages/rtrt/Core/Array2.h>
#include <Packages/rtrt/Core/Color.h>
#include <stdio.h>

namespace rtrt 
{

class VideoMap : public Material
{
private:
  int nx;                             // pixel width of frames
  int ny;                             // pixel height of frames
  int numFrames;                      // number of image files to loop
  int curFrame;                       // frame index at last intersection
  double loopTime;                    // time for one complete video loop
  double framesPerSec;                // variable frame speed
  double curTime;                     // time at last intersect
  Color specular;                     // highlight color
  int specPower;                      // area of highlight
  double refl;                        // 
  Array1< Array2 <Color> * > frames;  // array of images to loop
   
public:   
  /*******************************************************************/
  // do not use!
  VideoMap ();
  /*******************************************************************/
  // fileName must be in format "name%d.ppm" 
  // files must be named in format --
  //      named name0.ppm , name1.ppm , ... ,  name[numFrames-1].ppm
  VideoMap (char* _fileName, int _numFrames, double _framesPerSec,
	    const Color& _specular, int _specPower, double _refl);
  ~VideoMap ();
  /*******************************************************************/
  // looks up diffuse color in current frame,  then calls default 
  // Material::PhongShade()
  virtual void shade (Color& result, const Ray& ray, const HitInfo& hit, 
		      int depth, double atten, const Color& accumcolor,
		      Context* cx); 	    
  virtual void io(SCIRun::Piostream &/*stream*/)
  { ASSERTFAIL("not implemented"); }
};   

} // namespace rtrt
  
#endif // _VIDEOMAP_H_
