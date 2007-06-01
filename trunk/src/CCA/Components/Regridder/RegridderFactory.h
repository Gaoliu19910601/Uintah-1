#ifndef Packages_Uintah_CCA_Components_Regridders_RegridderFactory_h
#define Packages_Uintah_CCA_Components_Regridders_RegridderFactory_h

#include <Core/ProblemSpec/ProblemSpecP.h>
#include <CCA/Components/Regridder/RegridderCommon.h>

#include <CCA/Components/Regridder/uintahshare.h>
namespace Uintah {

  class ProcessorGroup;

  class UINTAHSHARE RegridderFactory
  {
  public:
    // this function has a switch for all known regridders
    
    static RegridderCommon* create(ProblemSpecP& ps,
                                   const ProcessorGroup* world);


  };
} // End namespace Uintah


#endif
