#ifndef UINTAH_HOMEBREW_HIERARCHICALREGRIDDER_H
#define UINTAH_HOMEBREW_HIERARCHICALREGRIDDER_H

#include <Packages/Uintah/CCA/Components/Regridder/RegridderCommon.h>

namespace Uintah {

/**************************************

CLASS
   HierarchicalRegridder
   
   Short description...

GENERAL INFORMATION

   HierarchicalRegridder.h

   Bryan Worthen
   Department of Computer Science
   University of Utah

   Center for the Simulation of Accidental Fires and Explosions (C-SAFE)
  
   Copyright (C) 2000 SCI Group

KEYWORDS
   HierarchicalRegridder

DESCRIPTION
   Long description...
  
WARNING
  
****************************************/

  //! Takes care of AMR Regridding, using a hierarchical algorithm
  class HierarchicalRegridder : public RegridderCommon {
  public:
    HierarchicalRegridder(const ProcessorGroup* pg);
    virtual ~HierarchicalRegridder();

    //! Create a new Grid
    virtual Grid* regrid(Grid* oldGrid, SchedulerP sched, const ProblemSpecP& ups);

  private:
    void MarkPatches( const GridP& origGrid, int levelIdx  ); 
    void ExtendPatches( const GridP& origGrid, int levelIdx  ); 
    inline IntVector StartCellToLattice ( SCIRun::IntVector startCell, int levelIdx );
  };

} // End namespace Uintah

#endif
