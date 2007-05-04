#ifndef PARTICLESUBSET_H
#define PARTICLESUBSET_H

#include <Packages/Uintah/Core/Grid/Variables/ParticleSet.h>
#include <Packages/Uintah/Core/Util/RefCounted.h>
#include <Packages/Uintah/Core/Grid/Ghost.h>
#include <Core/Geometry/IntVector.h>

#include <sgi_stl_warnings_off.h>
#include <vector>
#include <iostream>
#include <sgi_stl_warnings_on.h>


using std::ostream;
using SCIRun::IntVector;

#include <Packages/Uintah/Core/Grid/share.h>
namespace Uintah {
  class Patch;
/**************************************

CLASS
   ParticleSubset
   
   Short description...

GENERAL INFORMATION

   ParticleSubset.h

   Steven G. Parker
   Department of Computer Science
   University of Utah

   Center for the Simulation of Accidental Fires and Explosions (C-SAFE)
  
   Copyright (C) 2000 SCI Group

KEYWORDS
   Particle, ParticleSet, ParticleSubset

DESCRIPTION
   Long description...
  
WARNING
  
****************************************/

  class ParticleVariableBase;

  class SCISHARE ParticleSubset : public RefCounted {
  public:
    ParticleSubset(ParticleSet* pset, bool fill,
		   int matlIndex, const Patch*,
		   particleIndex sizeHint);
    ParticleSubset(ParticleSet* pset, bool fill,
		   int matlIndex, const Patch*,
                   IntVector low, IntVector high,
		   particleIndex sizeHint);
    ParticleSubset(ParticleSet* pset, bool fill,
		   int matlIndex, const Patch*,
		   IntVector low, IntVector high,
                   const std::vector<const Patch*>& neighbors,
		   const std::vector<ParticleSubset*>& subsets);
    ParticleSubset();
    ~ParticleSubset();
    
    //////////
    // Insert Documentation Here:
    ParticleSet* getParticleSet() const {
      return d_pset;
    }

    bool operator==(const ParticleSubset& ps) const {
      return d_numParticles == ps.d_numParticles && 
        // a null patch means that there is no patch center for the pset
        // (probably on an AMR copy data timestep)
        (!d_patch || !ps.d_patch || d_patch == ps.d_patch) && 
        d_matlIndex == ps.d_matlIndex && d_low == ps.d_low && d_high == ps.d_high;
    }
      
    //////////
    // Insert Documentation Here:
    void addParticle(particleIndex idx) {
      if(d_numParticles >= d_allocatedSize)
	expand(1);
      d_particles[d_numParticles++] = idx;
    }
    particleIndex addParticles(particleIndex count);

    void resize(particleIndex idx);

    typedef particleIndex* iterator;
      
    //////////
    // Insert Documentation Here:
    iterator begin() {
      return d_particles;
    }
      
    //////////
    // Insert Documentation Here:

    iterator end() {
      return d_particles+d_numParticles;
    }
      
    //////////
    // Insert Documentation Here:
    const particleIndex* getPointer() const
    {
      return d_particles;
    }
      
    particleIndex* getPointer()
    {
      return d_particles;
    }
      
    //////////
    // Insert Documentation Here:
    particleIndex numParticles() {
      return d_numParticles;
    }
      
    //////////
    // Insert Documentation Here:
    void set(particleIndex idx, particleIndex value) {
      d_particles[idx] = value;
    }

    IntVector getLow() const {
      return d_low;
    }
    IntVector getHigh() const {
      return d_high;
    }
    const Patch* getPatch() const {
      return d_patch;
    }
    int getMatlIndex() const {
      return d_matlIndex;
    }

    // sort the set by particle IDs
    void sort(ParticleVariableBase* particleIDs);

    const std::vector<const Patch*>& getNeighbors() const {
      return neighbors;
    }
    const std::vector<ParticleSubset*>& getNeighborSubsets() const {
      return neighbor_subsets;
    }
    
    SCISHARE friend ostream& operator<<(ostream& out, Uintah::ParticleSubset& pset);

   private:
    //////////
    // Insert Documentation Here:
     
    ParticleSet*               d_pset;
    particleIndex* d_particles;
    particleIndex d_numParticles;
    particleIndex d_allocatedSize;
    int d_numExpansions;

    int d_matlIndex;
    const Patch* d_patch;
    IntVector d_low, d_high;

    std::vector<const Patch*> neighbors;
    std::vector<ParticleSubset*> neighbor_subsets;

    void fillset();

    void init();
    void expand(particleIndex minSizeIncrement);

    ParticleSubset(const ParticleSubset& copy);
    ParticleSubset& operator=(const ParticleSubset&);
  };
} // End namespace Uintah

#endif
