#ifndef UINTAH_HOMEBREW_SimpleSCHEDULER_H
#define UINTAH_HOMEBREW_SimpleSCHEDULER_H

#include <Packages/Uintah/CCA/Components/Schedulers/SchedulerCommon.h>
#include <Packages/Uintah/CCA/Components/Schedulers/Relocate.h>
#include <Packages/Uintah/CCA/Ports/DataWarehouseP.h>
#include <Packages/Uintah/Core/Grid/TaskProduct.h>
#include <Packages/Uintah/Core/Grid/Task.h>
#include <vector>
#include <map>

namespace Uintah {
  using std::vector;

  class OnDemandDataWarehouse;
  class Task;

/**************************************

CLASS
   SimpleScheduler
   
   Short description...

GENERAL INFORMATION

   SimpleScheduler.h

   Steven G. Parker
   Department of Computer Science
   University of Utah

   Center for the Simulation of Accidental Fires and Explosions (C-SAFE)
  
   Copyright (C) 2000 SCI Group

KEYWORDS
   Scheduler_Brain_Damaged

DESCRIPTION
   Long description...
  
WARNING
  
****************************************/

  class SimpleScheduler : public SchedulerCommon {
  public:
    SimpleScheduler(const ProcessorGroup* myworld, Output* oport);
    virtual ~SimpleScheduler();
      
    //////////
    // Insert Documentation Here:
    virtual void execute( const ProcessorGroup * pg );
    
    virtual SchedulerP createSubScheduler();
      
    //////////
    // Insert Documentation Here:
    virtual void scheduleParticleRelocation(const LevelP& level,
					    const VarLabel* old_posLabel,
					    const VarLabel* keepDeleteLabel,
					    const vector<vector<const VarLabel*> >& old_labels,
					    const VarLabel* new_posLabel,
					    const vector<vector<const VarLabel*> >& new_labels,
					    const VarLabel* particleIDLabel,
					    const MaterialSet* matls);

  protected:
    virtual void actuallyCompile( const ProcessorGroup * pg );
    
  private:
    SPRelocate reloc;
    SimpleScheduler(const SimpleScheduler&);
    SimpleScheduler& operator=(const SimpleScheduler&);

    vector<Task*> tasks;

    virtual void verifyChecksum();

  };
} // End namespace Uintah
   
#endif
