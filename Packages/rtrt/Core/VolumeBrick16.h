
#ifndef VOLUMEBRICK16_H
#define VOLUMEBRICK16_H 1

#include "VolumeBase.h"
#include "Point.h"
#include <stdlib.h>

namespace rtrt {

class VolumeBrick16 : public VolumeBase {
protected:
    Point min;
    Vector diag;
    Vector sdiag;
    int nx,ny,nz;
    short* indata;
    short* blockdata;
    short datamin, datamax;
    int* xidx;
    int* yidx;
    int* zidx;
public:
    VolumeBrick16(Material* matl, VolumeDpy* dpy, char* filebase);
    virtual ~VolumeBrick16();
    virtual void intersect(const Ray& ray, HitInfo& hit, DepthStats* st,
			   PerProcessorContext*);
    virtual void light_intersect(Light* light, const Ray& ray,
				 HitInfo& hit, double dist, Color& atten,
				 DepthStats* st, PerProcessorContext*);
    virtual Vector normal(const Point&, const HitInfo& hit);
    virtual void compute_bounds(BBox&, double offset);
    virtual void preprocess(double maxradius, int& pp_offset, int& scratchsize);
    virtual void compute_hist(int nhist, int* hist,
			      float datamin, float datamax);
    virtual void get_minmax(float& min, float& max);
};

} // end namespace rtrt

#endif
