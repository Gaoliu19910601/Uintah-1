
/*******************************************************************************\
 *                                                                             *
 * filename: TimeVaryingInstance.h                                             *
 * author  : R. Keith Morley                                                   *
 * last mod: 07/12/02                                                          *
 *                                                                             *
\*******************************************************************************/ 

#ifndef _TIME_VARYING_INSTANCE_
#define _TIME_VARYING_INSTANCE_ 1


#include <Packages/rtrt/Core/Instance.h>
#include <Core/Geometry/Point.h>
#include <Core/Geometry/Vector.h>
#include <Packages/rtrt/Core/InstanceWrapperObject.h>
#include <Packages/rtrt/Core/BBox.h>

namespace rtrt 
{

/*******************************************************************************/
class TimeVaryingInstance: public Instance
{
public:
   Point center;
   Point ocenter;
   Vector axis;
   double rate;
   BBox bbox_orig;
   Vector originToCenter; 
   TimeVaryingInstance (InstanceWrapperObject* obj);
   virtual void computeTransform(double t); 
   virtual void compute_bounds(BBox& b, double /*offset*/);
   virtual void animate(double t, bool& changed);
   void intersect(Ray& ray, HitInfo& hit, DepthStats* st,
                  PerProcessorContext* ppc);
}; // TimeVaryingInstance

/*******************************************************************************/
class FishInstance1 : public TimeVaryingInstance
{
public:   
   double vertHeightScale;
   double vertPerScale;
   double horizHeightScale;
   double horizPerScale;
   double rotPerSec;
   double startTime;
   double vertShift;
   
   FishInstance1(InstanceWrapperObject* obj, double _vertHeightScale, double _horizHeightScale, 
	                                     double _vertPerScale, double _horizPerScale,   
					     double _rotPerSec, double _startTime, 
					     double _vertShift);
   virtual void computeTransform(double t);
}; // FishInstance1

/*******************************************************************************/
class FishInstance2 : public TimeVaryingInstance
{
public:   
   double vertHeightScale;
   double vertPerScale;
   double horizHeightScale;
   double horizPerScale;
   double rotPerSec;
   double startTime;
   double vertShift;

   FishInstance2(InstanceWrapperObject* obj, double _vertHeightScale, double _horizHeightScale, 
                                             double _vertPerScale, double _horizPerScale,   
                                             double _rotPerSec, double _startTime, 
                                             double _vertShift);
   virtual void computeTransform(double t);
}; // FishInstance2

}  // rtrt

#endif // _TIME_VARYING_INSTANCE_

