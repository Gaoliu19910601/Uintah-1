
#ifndef LAMBERTIAN_H
#define LAMBERTIAN_H 1

#include "Material.h"
#include "Color.h"

namespace rtrt {

class LambertianMaterial : public Material {
    Color R;
public:
    LambertianMaterial(const Color& R);
    virtual ~LambertianMaterial();
    virtual void shade(Color& result, const Ray& ray,
		       const HitInfo& hit, int depth,
		       double atten, const Color& accumcolor,
		       Context* cx);
};

} // end namespace rtrt

#endif
