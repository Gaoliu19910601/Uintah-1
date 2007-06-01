#ifndef UINTAH_GRID_DensityBoundCond_H
#define UINTAH_GRID_DensityBoundCond_H

#include <Core/Grid/BoundaryConditions/BoundCond.h>
#include <Core/ProblemSpec/ProblemSpecP.h>

#include <Core/Grid/uintahshare.h>
namespace Uintah {
   
/**************************************

CLASS
   DensityBoundCond
   
   
GENERAL INFORMATION

   DensityBoundCond.h

   John A. Schmidt
   Department of Mechanical Engineering
   University of Utah

   Center for the Simulation of Accidental Fires and Explosions (C-SAFE)
  
   Copyright (C) 2000 SCI Group

KEYWORDS
   DensityBoundCond

DESCRIPTION
   Long description...
  
WARNING
  
****************************************/

   class UINTAHSHARE DensityBoundCond : public BoundCond<double>  {
   public:
     DensityBoundCond(ProblemSpecP& ps,std::string& kind);
     virtual ~DensityBoundCond();
     virtual DensityBoundCond* clone();
     double getConstant() const;

   private:
     double d_constant;
   };
} // End namespace Uintah

#endif
