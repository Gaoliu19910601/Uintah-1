#ifndef __SIMPLE_HEBURN_H__
#define __SIMPLE_HEBURN_H__

#include <Uintah/Components/MPM/Burn/HEBurn.h>
#include <Uintah/Interface/DataWarehouseP.h>
#include <Uintah/Parallel/UintahParallelComponent.h>
#include <Uintah/Interface/MPMInterface.h>
#include <Uintah/Grid/GridP.h>
#include <Uintah/Grid/LevelP.h>
#include <Uintah/Interface/ProblemSpecP.h>
#include <Uintah/Interface/ProblemSpec.h>
#include <Uintah/Grid/SimulationState.h>
#include <Uintah/Grid/SimulationStateP.h>


namespace Uintah {
  namespace MPM {

/**************************************

CLASS
   SimpleHEBurn
   
   Short description...

GENERAL INFORMATION

   SimpleHEBurn.h

   Steven G. Parker
   Department of Computer Science
   University of Utah

   Center for the Simulation of Accidental Fires and Explosions (C-SAFE)
  
   Copyright (C) 2000 SCI Group

KEYWORDS
   HEBurn_Model_Simple

DESCRIPTION
   Long description...
  
WARNING
  
****************************************/

    class SimpleHEBurn : public HEBurn { 
    private:
      double a,b;
      
      // Prevent copying of this class
      // copy constructor
      SimpleHEBurn(const SimpleHEBurn &burn);
      SimpleHEBurn & operator=(const SimpleHEBurn &burn);
      
    public:
      // Constructor
      SimpleHEBurn(ProblemSpecP& ps);
      
      // Destructor
      virtual ~SimpleHEBurn();

      virtual void checkIfIgnited();
      virtual void computeMassRate();
      virtual void updatedParticleMassAndVolume(); 

    };
    
  } // end namespace MPM
} // end namespace Uintah

// $Log$
// Revision 1.1  2000/06/02 22:48:26  jas
// Added infrastructure for Burn models.
//

#endif /* __SIMPLE_HEBURN_H__*/

