#include <Packages/Uintah/CCA/Components/ICE/ICE.h>
#include <Packages/Uintah/CCA/Components/ICE/ICEMaterial.h>
#include <Packages/Uintah/CCA/Components/MPM/ConstitutiveModel/MPMMaterial.h>
#include <Packages/Uintah/CCA/Ports/DataWarehouse.h>
#include <Packages/Uintah/CCA/Ports/Scheduler.h>

#include <Packages/Uintah/Core/Grid/Grid.h>
#include <Packages/Uintah/Core/Grid/Level.h>
#include <Packages/Uintah/Core/Grid/Patch.h>
#include <Packages/Uintah/Core/Grid/PerPatch.h>
#include <Packages/Uintah/Core/Grid/SimulationState.h>
#include <Packages/Uintah/Core/Grid/Task.h>
#include <Packages/Uintah/Core/Grid/PressureBoundCond.h>
#include <Packages/Uintah/Core/Grid/VelocityBoundCond.h>
#include <Packages/Uintah/Core/Grid/TemperatureBoundCond.h>
#include <Packages/Uintah/Core/Grid/DensityBoundCond.h>
#include <Packages/Uintah/Core/Grid/VarTypes.h>
#include <Packages/Uintah/Core/Grid/CellIterator.h>
#include <Core/Util/NotFinished.h>

#include <Packages/Uintah/Core/Exceptions/ParameterNotFound.h>
#include <Packages/Uintah/Core/Exceptions/ProblemSetupException.h>
#include <Packages/Uintah/Core/Parallel/ProcessorGroup.h>
#include <Core/Datatypes/DenseMatrix.h>
#include <vector>
#include <Core/Geometry/Vector.h>
#include <Core/Containers/StaticArray.h>
#include <sstream>

using std::vector;
using std::max;
using std::istringstream;
 
using namespace SCIRun;
using namespace Uintah;

static int iterNum = 0;
 
//#define DOING
#undef DOING
/*`==========TESTING==========*/ 
// KEEP THIS AROUND UNTIL
// I'M SURE OF THE NEW STYLE OF SETBC -Todd
#define oldStyle_setBC 1
#define newStyle_setBC 0
 /*==========TESTING==========`*/
//#define ANNULUSICE
#undef ANNULUSICE

ICE::ICE(const ProcessorGroup* myworld) 
  : UintahParallelComponent(myworld)
{
  lb   = scinew ICELabel();

  IFS_CCLabel = scinew VarLabel("IFS_CC",
                                CCVariable<fflux>::getTypeDescription());
  OFS_CCLabel = scinew VarLabel("OFS_CC",
                                CCVariable<fflux>::getTypeDescription());
  IFE_CCLabel = scinew VarLabel("IFE_CC",
                                CCVariable<eflux>::getTypeDescription());
  OFE_CCLabel = scinew VarLabel("OFE_CC",
                                CCVariable<eflux>::getTypeDescription());
  IFC_CCLabel = scinew VarLabel("IFC_CC",
                                CCVariable<cflux>::getTypeDescription());
  OFC_CCLabel = scinew VarLabel("OFC_CC",
                                CCVariable<cflux>::getTypeDescription());
  q_outLabel = scinew VarLabel("q_out",
                                CCVariable<fflux>::getTypeDescription());
  q_out_EFLabel = scinew VarLabel("q_out_EF",
                                CCVariable<eflux>::getTypeDescription());
  q_out_CFLabel = scinew VarLabel("q_out_CF",
                                CCVariable<cflux>::getTypeDescription());
  q_inLabel = scinew VarLabel("q_in",
                                CCVariable<fflux>::getTypeDescription());
  q_in_EFLabel = scinew VarLabel("q_in_EF",
                                CCVariable<eflux>::getTypeDescription());
  q_in_CFLabel = scinew VarLabel("q_in_CF",
                                CCVariable<cflux>::getTypeDescription());

  // Turn off all the debuging switches
  switchDebugInitialize           = false;
  switchDebug_equilibration_press = false;
  switchDebug_vel_FC              = false;
  switchDebug_Exchange_FC         = false;
  switchDebug_explicit_press      = false;
  switchDebug_PressFC             = false;
  switchDebugLagrangianValues     = false;
  switchDebugMomentumExchange_CC  = false;
  switchDebugSource_Sink          = false;
  switchDebug_advance_advect      = false;
  switchDebug_advectQFirst        = false;
  switchTestConservation          = false;

  d_massExchange = false;
  
}

ICE::~ICE()
{
  delete lb;
  delete IFS_CCLabel;
  delete OFS_CCLabel;
  delete IFE_CCLabel;
  delete OFE_CCLabel;
  delete IFC_CCLabel;
  delete OFC_CCLabel;
  delete q_outLabel;
  delete q_out_EFLabel;
  delete q_out_CFLabel;
  delete q_inLabel;
  delete q_in_EFLabel;
  delete q_in_CFLabel;

}
/* ---------------------------------------------------------------------
 Function~  ICE::problemSetup--
_____________________________________________________________________*/
void  ICE::problemSetup(const ProblemSpecP& prob_spec,GridP& ,
		       SimulationStateP&   sharedState)
{
  d_sharedState = sharedState;
  d_SMALL_NUM   = 1.e-100;
  cerr << "In the preprocessor . . ." << endl;
  
  // Find the switches
  ProblemSpecP debug_ps = prob_spec->findBlock("Debug");
  if (debug_ps) {
    for (ProblemSpecP child = debug_ps->findBlock("debug"); child != 0;
	 child = child->findNextBlock("debug")) {
      map<string,string> debug_attr;
      child->getAttributes(debug_attr);
      if (debug_attr["label"]      == "switchDebugInitialize")
	switchDebugInitialize            = true;
      else if (debug_attr["label"] == "switchDebug_equilibration_press")
	switchDebug_equilibration_press  = true;
      else if (debug_attr["label"] == "switchDebug_vel_FC")
	switchDebug_vel_FC               = true;
      else if (debug_attr["label"] == "switchDebug_Exchange_FC")
	switchDebug_Exchange_FC          = true;
      else if (debug_attr["label"] == "switchDebug_explicit_press")
	switchDebug_explicit_press       = true;
      else if (debug_attr["label"] == "switchDebug_PressFC")
	switchDebug_PressFC              = true;
      else if (debug_attr["label"] == "switchDebugLagrangianValues")
	switchDebugLagrangianValues      = true;
      else if (debug_attr["label"] == "switchDebugMomentumExchange_CC")
	switchDebugMomentumExchange_CC   = true;
      else if (debug_attr["label"] == "switchDebugSource_Sink")
	switchDebugSource_Sink           = true;
      else if (debug_attr["label"] == "switchDebug_advance_advect")
	switchDebug_advance_advect       = true;
      else if (debug_attr["label"] == "switchDebug_advectQFirst")
	switchDebug_advectQFirst         = true;
      else if (debug_attr["label"] == "switchTestConservation")
        switchTestConservation           = true;
    }
  }
  cerr << "Pulled out the debugging switches from input file" << endl;

  // Pull out from CFD-ICE section
  ProblemSpecP cfd_ps = prob_spec->findBlock("CFD");
  cfd_ps->require("cfl",d_CFL);
  ProblemSpecP cfd_ice_ps = cfd_ps->findBlock("ICE");
  cfd_ice_ps->require("max_iteration_equilibration",d_max_iter_equilibration);
  cout << "cfl = " << d_CFL << endl;
  cout << "max_iteration_equilibration " << d_max_iter_equilibration << endl;
  cerr << "Pulled out CFD-ICE block of the input file" << endl;
    
  // Pull out from Time section
  ProblemSpecP time_ps = prob_spec->findBlock("Time");
  time_ps->require("delt_init",d_initialDt);
  cout << "Initial dt = " << d_initialDt << endl;
  cerr << "Pulled out Time block of the input file" << endl;

  // Pull out Initial Conditions
  ProblemSpecP mat_ps       =  prob_spec->findBlock("MaterialProperties");
  ProblemSpecP ice_mat_ps   = mat_ps->findBlock("ICE");  

  for (ProblemSpecP ps = ice_mat_ps->findBlock("material"); ps != 0;
       ps = ps->findNextBlock("material") ) {
    // Extract out the type of EOS and the 
    // associated parameters
     ICEMaterial *mat = scinew ICEMaterial(ps);
     sharedState->registerICEMaterial(mat);
  }     
  cerr << "Pulled out InitialConditions block of the input file" << endl;

  // Pull out the exchange coefficients
  ProblemSpecP exch_ps = mat_ps->findBlock("exchange_properties");
  if (!exch_ps)
    throw ProblemSetupException("Cannot find exchange_properties tag");
  
  ProblemSpecP exch_co_ps = exch_ps->findBlock("exchange_coefficients");
  exch_co_ps->require("momentum",d_K_mom);
  exch_co_ps->require("heat",d_K_heat);

  for (int i = 0; i<(int)d_K_mom.size(); i++)
    cout << "K_mom = " << d_K_mom[i] << endl;
  for (int i = 0; i<(int)d_K_heat.size(); i++)
    cout << "K_heat = " << d_K_heat[i] << endl;
  cerr << "Pulled out exchange coefficients of the input file" << endl;

  string mass_exch_in;
  ProblemSpecP mass_exch_ps = exch_ps->get("mass_exchange",mass_exch_in);

  if (mass_exch_ps) {
    istringstream in(mass_exch_in);
    string mass_exch_out;
    in >> mass_exch_out;
    if (mass_exch_out == "true" || mass_exch_out == "TRUE" || 
	mass_exch_out == "1") {
      d_massExchange = true;
    } else 
      d_massExchange = false;
  } else
    d_massExchange = false;

  cerr << "Mass exchange = " << d_massExchange << endl;

  //__________________________________
  //  Print out what I've found
  cout << "Number of ICE materials: " 
       << d_sharedState->getNumICEMatls()<< endl;

  if (switchDebugInitialize == true) 
    cout << "switchDebugInitialize is ON" << endl;
  if (switchDebug_equilibration_press == true) 
    cout << "switchDebug_equilibration_press is ON" << endl;
  if (switchDebug_vel_FC == true) 
    cout << "switchDebug_vel_FC is ON" << endl;
  if (switchDebug_Exchange_FC == true) 
    cout << "switchDebug_Exchange_FC is ON" << endl;
  if (switchDebug_explicit_press == true) 
    cout << "switchDebug_explicit_press is ON" << endl;
  if (switchDebug_PressFC == true) 
    cout << "switchDebug_PressFC is ON" << endl;
  if (switchDebugLagrangianValues == true) 
    cout << "switchDebugLagrangianValues is ON" << endl;
  if (switchDebugSource_Sink == true) 
    cout << "switchDebugSource_Sink is ON" << endl;
  if (switchDebug_advance_advect == true) 
    cout << "switchDebug_advance_advect is ON" << endl;
  if (switchDebug_advectQFirst == true) 
    cout << "switchDebug_advectQFirst is ON" << endl;
  if (switchTestConservation == true)
    cout << "switchTestConservation is ON" << endl;

}
/* ---------------------------------------------------------------------
 Function~  ICE::scheduleInitialize--
_____________________________________________________________________*/
void ICE::scheduleInitialize(const LevelP& level, 
                             SchedulerP& sched)
{
#ifdef DOING
  cout << "Doing ICE::scheduleInitialize " << endl;
#endif  
  Task* t = scinew Task("ICE::actuallyInitialize",
                  this, &ICE::actuallyInitialize);
  t->computes(d_sharedState->get_delt_label());

  t->computes(lb->temp_CCLabel);
  t->computes(lb->rho_micro_CCLabel);
  t->computes(lb->sp_vol_CCLabel);
  t->computes(lb->mass_CCLabel);
  t->computes(lb->rho_CC_top_cycleLabel);
  t->computes(lb->vol_frac_CCLabel);
  t->computes(lb->vel_CCLabel);
  t->computes(lb->press_CCLabel);
  t->computes(lb->speedSound_CCLabel);

  sched->addTask(t, level->eachPatch(), d_sharedState->allICEMaterials());
}
/* ---------------------------------------------------------------------
 Function~  ICE::scheduleComputeStableTimestep--
_____________________________________________________________________*/
void ICE::scheduleComputeStableTimestep(const LevelP& level,
                                      SchedulerP& sched)
{
#ifdef DOING
  cout << "ICE::scheduleComputeStableTimestep " << endl;
#endif
    Task* task = scinew Task("ICE::actuallyComputeStableTimestep",
			  this, &ICE::actuallyComputeStableTimestep);

    task->requires(Task::NewDW,lb->vel_CCLabel,        Ghost::None);
    task->requires(Task::NewDW,lb->speedSound_CCLabel, Ghost::None);
    task->computes(d_sharedState->get_delt_label());
    sched->addTask(task,level->eachPatch(), d_sharedState->allICEMaterials());
}
/* ---------------------------------------------------------------------
 Function~  ICE::scheduleTimeAdvance--
_____________________________________________________________________*/
void ICE::scheduleTimeAdvance(double t, double dt,const LevelP& level,
			      SchedulerP& sched)
{
#ifdef DOING
  cout << "ICE::scheduleTimeAdvance" << endl;
#endif
  const PatchSet* patches = level->eachPatch();
  const MaterialSet* ice_matls = d_sharedState->allICEMaterials();
  const MaterialSet* mpm_matls = d_sharedState->allMPMMaterials();
  const MaterialSet* all_matls = d_sharedState->allMaterials();  
  MaterialSubset* press_matl    = scinew MaterialSubset();
  press_matl->add(0);
  press_matl ->addReference();
  const MaterialSubset* ice_matls_sub = ice_matls->getUnion();
  const MaterialSubset* mpm_matls_sub = mpm_matls->getUnion();

  
  scheduleComputeEquilibrationPressure(sched, patches, press_matl,
                                                       all_matls);

  scheduleComputeFaceCenteredVelocities(sched, patches, ice_matls_sub,
                                                        mpm_matls_sub,
                                                        press_matl, 
                                                        all_matls);

  scheduleAddExchangeContributionToFCVel(sched, patches, all_matls);    

  scheduleMassExchange(sched, patches, all_matls);

  scheduleComputeDelPressAndUpdatePressCC(sched, patches, press_matl,
                                                          ice_matls_sub,
                                                          all_matls);

  scheduleComputePressFC(sched, patches, press_matl,
                                        all_matls);

  scheduleAccumulateMomentumSourceSinks(sched, patches, press_matl,
                                        ice_matls_sub, all_matls);

  scheduleAccumulateEnergySourceSinks(sched, patches, press_matl,
                                                      all_matls);

  scheduleComputeLagrangianValues(sched, patches,   mpm_matls_sub,
                                                    all_matls);

  scheduleAddExchangeToMomentumAndEnergy(sched, patches, all_matls);

  scheduleAdvectAndAdvanceInTime(sched, patches, all_matls);
  
  if(switchTestConservation) {
    schedulePrintConservedQuantities(sched, patches, press_matl,all_matls); 
  }
  
}

/* ---------------------------------------------------------------------
 Function~  ICE::scheduleComputeEquilibrationPressure--
_____________________________________________________________________*/
void ICE::scheduleComputeEquilibrationPressure(SchedulerP& sched,
					       const PatchSet* patches,
                                          const MaterialSubset* press_matl,
					       const MaterialSet* ice_matls)
{
#ifdef DOING
  cout << "ICE::scheduleComputeEquilibrationPressure" << endl;
#endif
  Task* task = scinew Task("ICE::computeEquilibrationPressure",
                     this, &ICE::computeEquilibrationPressure);
  
  task->requires(Task::OldDW,lb->press_CCLabel, press_matl, Ghost::None);
  task->requires(Task::OldDW,lb->rho_CC_top_cycleLabel,     Ghost::None);
  task->requires(Task::OldDW,lb->temp_CCLabel,              Ghost::None);

  task->computes(lb->speedSound_CCLabel);
  task->computes(lb->vol_frac_CCLabel);
  task->computes(lb->rho_micro_CCLabel);
  task->computes(lb->rho_CCLabel);
  task->computes(lb->press_equil_CCLabel, press_matl);
  sched->addTask(task, patches, ice_matls);
}

/* ---------------------------------------------------------------------
 Function~  ICE::scheduleComputeFaceCenteredVelocities--
_____________________________________________________________________*/
void ICE::scheduleComputeFaceCenteredVelocities(SchedulerP& sched,
						const PatchSet* patches,
						const MaterialSubset* ice_matls,
                                          const MaterialSubset* mpm_matls,
                                          const MaterialSubset* press_matl,
                                          const MaterialSet* all_matls)
{
#ifdef DOING
  cout << "ICE::scheduleComputeFaceCenteredVelocities" << endl;
#endif
  Task* task = scinew Task("ICE::computeFaceCenteredVelocities",
                     this, &ICE::computeFaceCenteredVelocities);

  task->requires(Task::NewDW,lb->press_equil_CCLabel, press_matl,
                                                      Ghost::AroundCells,1);
  task->requires(Task::NewDW,lb->rho_micro_CCLabel, /*all_matls*/
                                                      Ghost::AroundCells,1);
  task->requires(Task::NewDW,lb->rho_CCLabel,       /*all_matls*/
                                                      Ghost::AroundCells,1);
  task->requires(Task::OldDW,lb->vel_CCLabel,         ice_matls, 
                                                      Ghost::AroundCells,1);
  task->requires(Task::NewDW,lb->vel_CCLabel,         mpm_matls, 
                                                      Ghost::AroundCells,1);

  task->computes(lb->uvel_FCLabel);
  task->computes(lb->vvel_FCLabel);
  task->computes(lb->wvel_FCLabel);
  sched->addTask(task, patches, all_matls);
}
/* ---------------------------------------------------------------------
 Function~  ICE::scheduleAddExchangeContributionToFCVel--
_____________________________________________________________________*/
void ICE::scheduleAddExchangeContributionToFCVel(SchedulerP& sched,
						 const PatchSet* patches,
						 const MaterialSet* matls)
{
#ifdef DOING
  cout << "ICE::scheduleAddExchangeContributionToFCVel" << endl;
#endif
  Task* task = scinew Task("ICE::addExchangeContributionToFCVel",
                     this, &ICE::addExchangeContributionToFCVel);

  task->requires(Task::NewDW,lb->rho_micro_CCLabel, Ghost::AroundCells,1);
  task->requires(Task::NewDW,lb->vol_frac_CCLabel,  Ghost::AroundCells,1);
  task->requires(Task::NewDW,lb->uvel_FCLabel,      Ghost::AroundCells,2);
  task->requires(Task::NewDW,lb->vvel_FCLabel,      Ghost::AroundCells,2);
  task->requires(Task::NewDW,lb->wvel_FCLabel,      Ghost::AroundCells,2);
 
  task->computes(lb->uvel_FCMELabel);
  task->computes(lb->vvel_FCMELabel);
  task->computes(lb->wvel_FCMELabel);
  
  sched->addTask(task, patches, matls);
}

/* ---------------------------------------------------------------------
 Function~  ICE::scheduleMassExchange--
_____________________________________________________________________*/
void  ICE::scheduleMassExchange(SchedulerP& sched,
				const PatchSet* patches,
				const MaterialSet* matls)
{
#ifdef DOING
  cout << "ICE::scheduleMassExchange" << endl;
#endif 
  Task* task = scinew Task("ICE::massExchange",
			this, &ICE::massExchange);
  task->requires(Task::NewDW, lb->rho_CCLabel, Ghost::None);
  task->computes(lb->burnedMass_CCLabel);
  task->computes(lb->releasedHeat_CCLabel);
  
  sched->addTask(task, patches, matls);
}
/* ---------------------------------------------------------------------
 Function~  ICE::scheduleComputeDelPressAndUpdatePressCC--
_____________________________________________________________________*/
void ICE::scheduleComputeDelPressAndUpdatePressCC(SchedulerP& sched,
						  const PatchSet* patches,
                                            const MaterialSubset* press_matl,
                                            const MaterialSubset* ice_matls,
						  const MaterialSet* matls)
{
#ifdef DOING
  cout << "ICE::scheduleComputeDelPressAndUpdatePressCC" << endl;
#endif
  Task* task = scinew Task("ICE::computeDelPressAndUpdatePressCC",
                     this, &ICE::computeDelPressAndUpdatePressCC);
  
  task->requires( Task::NewDW,lb->press_equil_CCLabel,
                                          press_matl, Ghost::None);
  task->requires( Task::NewDW, lb->vol_frac_CCLabel,  Ghost::AroundCells,1);
  task->requires( Task::NewDW, lb->uvel_FCMELabel,    Ghost::AroundCells,2);
  task->requires( Task::NewDW, lb->vvel_FCMELabel,    Ghost::AroundCells,2);
  task->requires( Task::NewDW, lb->wvel_FCMELabel,    Ghost::AroundCells,2);

  task->requires( Task::NewDW, lb->speedSound_CCLabel,Ghost::None);
  task->requires( Task::NewDW, lb->rho_micro_CCLabel, Ghost::None);
  task->requires( Task::NewDW, lb->rho_CCLabel,       Ghost::None);
  task->requires( Task::OldDW, lb->vel_CCLabel,       ice_matls, 
                                                      Ghost::None);
 task->requires(Task::NewDW,lb->burnedMass_CCLabel,   Ghost::None);
    
  task->computes(lb->press_CCLabel,    press_matl);
  task->computes(lb->delPress_CCLabel, press_matl);
  
  sched->addTask(task, patches, matls);
}

/* ---------------------------------------------------------------------
 Function~  ICE::scheduleComputePressFC--
_____________________________________________________________________*/
void ICE::scheduleComputePressFC(SchedulerP& sched,
				 const PatchSet* patches,
                             const MaterialSubset* press_matl,
				 const MaterialSet* matls)
{ 
#ifdef DOING
  cout << "ICE::scheduleComputePressFC" << endl;
#endif                    
  Task* task = scinew Task("ICE::computePressFC",
                     this, &ICE::computePressFC);

  task->requires(Task::NewDW,lb->press_CCLabel,
                                     press_matl,Ghost::AroundCells,1);
  task->requires(Task::NewDW,lb->rho_CCLabel,   Ghost::AroundCells,1);
  
  task->computes(lb->pressX_FCLabel, press_matl);
  task->computes(lb->pressY_FCLabel, press_matl);
  task->computes(lb->pressZ_FCLabel, press_matl);

  sched->addTask(task, patches, matls);
}

/* ---------------------------------------------------------------------
 Function~  ICE::scheduleAccumulateMomentumSourceSinks--
_____________________________________________________________________*/
void ICE::scheduleAccumulateMomentumSourceSinks(SchedulerP& sched,
						const PatchSet* patches,
                                          const MaterialSubset* press_matl,
                                          const MaterialSubset* ice_matls_sub,
						const MaterialSet* matls)
{
#ifdef DOING
  cout << "ICE::scheduleAccumulateMomentumSourceSinks" << endl;
#endif 
  Task* task = scinew Task("ICE::accumulateMomentumSourceSinks", 
                     this, &ICE::accumulateMomentumSourceSinks);
                     
  task->requires(Task::NewDW,lb->pressX_FCLabel,   press_matl,    
                                                   Ghost::AroundCells,1);
  task->requires(Task::NewDW,lb->pressY_FCLabel,   press_matl,
                                                   Ghost::AroundCells,1);
  task->requires(Task::NewDW,lb->pressZ_FCLabel,   press_matl,
                                                   Ghost::AroundCells,1);
  task->requires(Task::NewDW,lb->rho_CCLabel,      Ghost::None);
  task->requires(Task::NewDW,lb->vol_frac_CCLabel, Ghost::None);
  task->requires(Task::OldDW,lb->vel_CCLabel,      ice_matls_sub,
                                                   Ghost::None);
  task->computes(lb->mom_source_CCLabel);
  sched->addTask(task, patches, matls);
}

/* ---------------------------------------------------------------------
 Function~  ICE::scheduleAccumulateEnergySourceSinks--
_____________________________________________________________________*/
void ICE::scheduleAccumulateEnergySourceSinks(SchedulerP& sched,
					      const PatchSet* patches,
                                         const MaterialSubset* press_matl,
					      const MaterialSet* matls)

{
#ifdef DOING
  cout << "ICE::scheduleAccumulateEnergySourceSinks" << endl;
#endif 
  Task* task = scinew Task("ICE::accumulateEnergySourceSinks",
                     this, &ICE::accumulateEnergySourceSinks);
  
  task->requires(Task::NewDW, lb->press_CCLabel,      press_matl,Ghost::None);
  task->requires(Task::NewDW, lb->delPress_CCLabel,   press_matl,Ghost::None);
  task->requires(Task::NewDW, lb->rho_micro_CCLabel,             Ghost::None);
  task->requires(Task::NewDW, lb->speedSound_CCLabel,            Ghost::None);
  task->requires(Task::NewDW, lb->vol_frac_CCLabel,              Ghost::None);

#ifdef ANNULUSICE
  task->requires(Task::NewDW, lb->rho_CCLabel,                   Ghost::None);
#endif
  
  task->computes(lb->int_eng_source_CCLabel);
  
  sched->addTask(task, patches, matls);
}

/* ---------------------------------------------------------------------
 Function~  ICE:: scheduleComputeLagrangianValues--
 Note:      Only loop over ICE materials, mom_L for MPM is computed
            prior to this function.  
_____________________________________________________________________*/
void ICE::scheduleComputeLagrangianValues(SchedulerP& sched,
					  const PatchSet* patches,
                                     const MaterialSubset* mpm_matls,
                                     const MaterialSet* ice_matls)
{
#ifdef DOING
  cout << "ICE::scheduleComputeLagrangianValues" << endl;
#endif 
  Task* task = scinew Task("ICE::computeLagrangianValues",
                      this,&ICE::computeLagrangianValues);

  task->requires(Task::NewDW,lb->rho_CCLabel,             Ghost::None);
  task->requires(Task::OldDW,lb->vel_CCLabel,             Ghost::None);
  task->requires(Task::OldDW,lb->temp_CCLabel,            Ghost::None);
  task->requires(Task::NewDW,lb->mom_source_CCLabel,      Ghost::None);
  task->requires(Task::NewDW,lb->burnedMass_CCLabel,      Ghost::None);
  task->requires(Task::NewDW,lb->releasedHeat_CCLabel,    Ghost::None);
  task->requires(Task::NewDW,lb->int_eng_source_CCLabel,  Ghost::None);
  if (switchDebugLagrangianValues ) {
  task->requires(Task::NewDW,lb->mom_L_CCLabel,     mpm_matls,
                                                          Ghost::None);
  task->requires(Task::NewDW,lb->int_eng_L_CCLabel, mpm_matls,
                                                          Ghost::None);
  }

  task->computes(lb->mom_L_CCLabel);
  task->computes(lb->int_eng_L_CCLabel);
  task->computes(lb->mass_L_CCLabel);
 
  sched->addTask(task, patches, ice_matls);
}

/* ---------------------------------------------------------------------
 Function~  ICE::scheduleAddExchangeToMomentumAndEnergy--
_____________________________________________________________________*/
void ICE::scheduleAddExchangeToMomentumAndEnergy(SchedulerP& sched,
						 const PatchSet* patches, 
						 const MaterialSet* matls)
{
#ifdef DOING
  cout << "ICE::scheduleAddExchangeToMomentumAndEnergy" << endl;
#endif
  Task* task = scinew Task("ICE::addExchangeToMomentumAndEnergy",
                     this, &ICE::addExchangeToMomentumAndEnergy);;
  task->requires(Task::NewDW, lb->mass_L_CCLabel,   Ghost::None);
  task->requires(Task::NewDW, lb->mom_L_CCLabel,    Ghost::None);
  task->requires(Task::NewDW, lb->int_eng_L_CCLabel,Ghost::None);
  task->requires(Task::NewDW, lb->vol_frac_CCLabel, Ghost::None);
  task->requires(Task::NewDW, lb->rho_micro_CCLabel,Ghost::None);
 
  task->computes(lb->mom_L_ME_CCLabel);
  task->computes(lb->int_eng_L_ME_CCLabel);
  
  sched->addTask(task, patches, matls);
}

/* ---------------------------------------------------------------------
 Function~  ICE::scheduleAdvectAndAdvanceInTime--
_____________________________________________________________________*/
void ICE::scheduleAdvectAndAdvanceInTime(SchedulerP& sched,
					 const PatchSet* patches,
					 const MaterialSet* matls)
{
#ifdef DOING
  cout << "ICE::scheduleAdvectAndAdvanceInTime" << endl;
#endif
  Task* task = scinew Task("ICE::advectAndAdvanceInTime",
                     this, &ICE::advectAndAdvanceInTime);
  task->requires(Task::NewDW, lb->uvel_FCMELabel,      Ghost::AroundCells,2);
  task->requires(Task::NewDW, lb->vvel_FCMELabel,      Ghost::AroundCells,2);
  task->requires(Task::NewDW, lb->wvel_FCMELabel,      Ghost::AroundCells,2);
  task->requires(Task::NewDW, lb->mom_L_ME_CCLabel,    Ghost::AroundCells,1);
  task->requires(Task::NewDW, lb->mass_L_CCLabel,      Ghost::AroundCells,1);
  task->requires(Task::NewDW, lb->int_eng_L_ME_CCLabel,Ghost::AroundCells,1);
  task->requires(Task::NewDW, lb->rho_micro_CCLabel,   Ghost::AroundCells,1);
 
  task->computes(lb->rho_CC_top_cycleLabel);
  task->computes(lb->mass_CCLabel);
  task->computes(lb->sp_vol_CCLabel);
  task->computes(lb->temp_CCLabel);
  task->computes(lb->vel_CCLabel);
  sched->addTask(task, patches, matls);
}
/* ---------------------------------------------------------------------
 Function~  ICE::schedulePrintConservedQuantities--
_____________________________________________________________________*/
void ICE::schedulePrintConservedQuantities(SchedulerP& sched,
					   const PatchSet* patches,
					   const MaterialSubset* press_matl,
					   const MaterialSet* matls)
{
#ifdef DOING
  cout << "ICE::schedulePrintConservedQuantities" << endl;
#endif
  Task* task = scinew Task("ICE::printConservedQuantities",
                     this, &ICE::printConservedQuantities);

  task->requires(Task::NewDW, lb->press_CCLabel,press_matl,Ghost::None);
  task->requires(Task::NewDW, lb->delPress_CCLabel, press_matl,Ghost::None);
  task->requires(Task::NewDW,lb->rho_CCLabel, Ghost::None);
  task->requires(Task::NewDW,lb->vel_CCLabel, Ghost::None);
  task->requires(Task::NewDW,lb->temp_CCLabel,Ghost::None);
  task->computes(lb->TotalMassLabel);
  task->computes(lb->KineticEnergyLabel);
  task->computes(lb->TotalIntEngLabel);
  task->computes(lb->CenterOfMassVelocityLabel); //momentum
  sched->addTask(task, patches, matls);
}

/* ---------------------------------------------------------------------
 Function~  ICE::actuallyComputeStableTimestep--
 Purpose~   Compute next time step based on speed of sound and 
            maximum velocity in the domain
_____________________________________________________________________*/
void ICE::actuallyComputeStableTimestep(const ProcessorGroup*,  
					 const PatchSubset* patches,
                                    const MaterialSubset* /*matls*/,
					 DataWarehouse*, 
					 DataWarehouse* new_dw)
{
  for(int p=0;p<patches->size();p++){
    const Patch* patch = patches->get(p);
  #ifdef DOING
    cout << "Doing Compute Stable Timestep on patch " << patch->getID() 
         << "\t\t ICE" << endl;
  #endif
      
      Vector dx = patch->dCell();
      double delt_CFL = 100000, fudge_factor = 1.;
      CCVariable<double> speedSound;
      CCVariable<Vector> vel;
      double CFL,N_ITERATIONS_TO_STABILIZE = 1;

      ::iterNum++;
      if (iterNum < N_ITERATIONS_TO_STABILIZE) {
        CFL = d_CFL * (double)(::iterNum)  *
                        (1./(double)N_ITERATIONS_TO_STABILIZE);
      } else {
        CFL = d_CFL;
      }

      for (int m = 0; m < d_sharedState->getNumICEMatls(); m++) {
        ICEMaterial* ice_matl = d_sharedState->getICEMaterial(m);
        int indx= ice_matl->getDWIndex();

        new_dw->get(speedSound, lb->speedSound_CCLabel,
 		                            indx,patch,Ghost::None, 0);
        new_dw->get(vel, lb->vel_CCLabel, indx,patch,Ghost::None, 0);
	
	for(CellIterator iter=patch->getCellIterator(); !iter.done(); iter++){
	  double A = fudge_factor*CFL*dx.x()/(speedSound[*iter] + 
					     fabs(vel[*iter].x())+d_SMALL_NUM);
	  double B = fudge_factor*CFL*dx.y()/(speedSound[*iter] + 
					     fabs(vel[*iter].y())+d_SMALL_NUM);
	  double C = fudge_factor*CFL*dx.z()/(speedSound[*iter] + 
					     fabs(vel[*iter].z())+d_SMALL_NUM);

	  delt_CFL = std::min(A, delt_CFL);
	  delt_CFL = std::min(B, delt_CFL);
	  delt_CFL = std::min(C, delt_CFL);

        }
      }
      //__________________________________
      // First time through grab delT from input file
      if (iterNum == 1) {
        delt_CFL = d_initialDt;
      }
      
    new_dw->put(delt_vartype(delt_CFL), lb->delTLabel);
  }  // patch loop
}

/* --------------------------------------------------------------------- 
 Function~  ICE::actuallyInitialize--
 Purpose~  Initialize the CC and FC variables and the pressure  
 Note that rho_micro, sp_vol, temp and velocity must be defined 
 everywhere in the domain
_____________________________________________________________________*/ 
void ICE::actuallyInitialize(const ProcessorGroup*, 
                          const PatchSubset* patches,
                          const MaterialSubset* /*matls*/,
			     DataWarehouse*, 
                          DataWarehouse* new_dw)
{
  for(int p=0;p<patches->size();p++){
    const Patch* patch = patches->get(p);

  #ifdef DOING
    cout << "Doing Initialize on patch " << patch->getID() 
         << "\t\t\t ICE" << endl;
  #endif
    int numMatls    = d_sharedState->getNumICEMatls();
    int numALLMatls = d_sharedState->getNumMatls();
    Vector dx       = patch->dCell();
    Vector grav     = d_sharedState->getGravity();
    double cell_vol = dx.x()*dx.y()*dx.z();
    StaticArray<CCVariable<double>   > rho_micro(numMatls);
    StaticArray<CCVariable<double>   > sp_vol_CC(numMatls);
    StaticArray<CCVariable<double>   > mass_CC(numMatls);
    StaticArray<CCVariable<double>   > rho_top_cycle(numMatls);
    StaticArray<CCVariable<double>   > Temp_CC(numMatls);
    StaticArray<CCVariable<double>   > speedSound(numMatls);
    StaticArray<CCVariable<double>   > vol_frac_CC(numMatls);
    StaticArray<CCVariable<Vector>   > vel_CC(numMatls);
    CCVariable<double>    press_CC;  
    StaticArray<double>        cv(numMatls);
    new_dw->allocate(press_CC,lb->press_CCLabel, 0,patch);

  //__________________________________
  // Note:
  // The press_CC isn't material dependent even though
  // we loop over numMatls below. This is done so we don't need additional
  // machinery to grab the pressure inside a geom_object
    for (int m = 0; m < numMatls; m++ ) {
      ICEMaterial* ice_matl = d_sharedState->getICEMaterial(m);
      int indx= ice_matl->getDWIndex();
      new_dw->allocate(rho_micro[m],   lb->rho_micro_CCLabel,   indx,patch);
      new_dw->allocate(sp_vol_CC[m],   lb->sp_vol_CCLabel,      indx,patch);
      new_dw->allocate(mass_CC[m],     lb->mass_CCLabel,        indx,patch);
      new_dw->allocate(rho_top_cycle[m],lb->rho_CC_top_cycleLabel,
                                                                indx,patch);
      new_dw->allocate(Temp_CC[m],     lb->temp_CCLabel,        indx,patch);
      new_dw->allocate(speedSound[m],  lb->speedSound_CCLabel,  indx,patch);
      new_dw->allocate(vol_frac_CC[m], lb->vol_frac_CCLabel,    indx,patch);
      new_dw->allocate(vel_CC[m],      lb->vel_CCLabel,         indx,patch);
    }
    for (int m = 0; m < numMatls; m++ ) {
      ICEMaterial* ice_matl = d_sharedState->getICEMaterial(m);
      int indx = ice_matl->getDWIndex();
      ice_matl->initializeCells(rho_micro[m], sp_vol_CC[m],   rho_top_cycle[m],
                                Temp_CC[m],   speedSound[m], 
                                vol_frac_CC[m], vel_CC[m], 
                                press_CC,  numALLMatls,    patch, new_dw);

      cv[m] = ice_matl->getSpecificHeat();
#if 0
      cerr << "Before BC application" << endl;
      char description[50];
      sprintf(description, "Initialization_Mat_%d ",m);
      printData(   patch, 1, description, "rho_CC",      rho_top_cycle[m]);
#endif

      setBC(rho_top_cycle[m], "Density",      patch, indx);
      setBC(Temp_CC[m],       "Temperature",  patch, indx);
      setBC(vel_CC[m],        "Velocity",     patch, indx); 
#if 0
      cerr << "After BC application" << endl;
      printData(   patch, 1, description, "rho_CC",      rho_top_cycle[m]);
#endif

      //__________________________________
      //  Adjust pressure and Temp field if g != 0
      //  so fields are thermodynamically consistent.
      if ((grav.x() !=0 || grav.y() != 0.0 || grav.z() != 0.0))  {
        hydrostaticPressureAdjustment(patch, 
                                      rho_micro[SURROUND_MAT], press_CC);

        setBC(press_CC, rho_micro[SURROUND_MAT], "Pressure",patch,0);

        ICEMaterial* ice_matl = d_sharedState->getICEMaterial(m);
        double gamma = ice_matl->getGamma();
        ice_matl->getEOS()->computeTempCC(patch, "WholeDomain",
                                     press_CC,   gamma,   cv[m],
					  rho_micro[m],    Temp_CC[m]);
      }

  //______________________________________________________
  // H A R D W I R E   F O R   M P M I C E   P R O B L E M
  //  Either read in the data or compute it.
  //  readData( patch, 1,"hardwire_initialize","mass_CC", mass_CC[m]);

      for(CellIterator iter=patch->getExtraCellIterator();!iter.done();iter++){
        mass_CC[m][*iter] = rho_top_cycle[m][*iter] * cell_vol;
      }
    }  
    for (int m = 0; m < numMatls; m++ ) { 
      ICEMaterial* ice_matl = d_sharedState->getICEMaterial(m);
      int indx = ice_matl->getDWIndex(); 
      new_dw->put(rho_micro[m],     lb->rho_micro_CCLabel,      indx,patch);
      new_dw->put(sp_vol_CC[m],     lb->sp_vol_CCLabel,         indx,patch);
      new_dw->put(mass_CC[m],       lb->mass_CCLabel,           indx,patch);
      new_dw->put(rho_top_cycle[m], lb->rho_CC_top_cycleLabel,  indx,patch);
      new_dw->put(vol_frac_CC[m],   lb->vol_frac_CCLabel,       indx,patch);
      new_dw->put(Temp_CC[m],       lb->temp_CCLabel,           indx,patch);
      new_dw->put(speedSound[m],    lb->speedSound_CCLabel,     indx,patch);
      new_dw->put(vel_CC[m],        lb->vel_CCLabel,            indx,patch);

      if (switchDebugInitialize){
        cout << " Initial Conditions" << endl;       
        char description[50];
        sprintf(description, "Initialization_Mat_%d ",indx);
        printData(   patch, 1, description, "rho_CC",      rho_top_cycle[m]);
        printData(   patch, 1, description, "rho_micro_CC",rho_micro[m]);
      // printData(   patch, 1, description, "sp_vol_CC",  sp_vol_CC[m]);
        printData(   patch, 1, description, "Temp_CC",     Temp_CC[m]);
        printData(   patch, 1, description, "vol_frac_CC", vol_frac_CC[m]);
        printVector( patch, 1, description, "uvel_CC", 0,  vel_CC[m]);
        printVector( patch, 1, description, "vvel_CC", 1,  vel_CC[m]);
        printVector( patch, 1, description, "wvel_CC", 2,  vel_CC[m]);
      }   
    }
    setBC(press_CC, rho_micro[SURROUND_MAT], "Pressure",patch,0);
    if (switchDebugInitialize){
       printData(   patch, 1, "Initialization", "press_CC", press_CC);
    }
    new_dw->put(press_CC,    lb->press_CCLabel,  0,patch);
  }  // patch loop 
}

/* --------------------------------------------------------------------- 
 Function~  ICE::computeEquilibrationPressure--
 Purpose~   Find the equilibration pressure  
 Reference: Flow of Interpenetrating Material Phases, J. Comp, Phys
               18, 440-464, 1975, see the equilibration section
                   
 Steps
 ----------------
    - Compute rho_micro_CC, SpeedSound, vol_frac

    For each cell
    _ WHILE LOOP(convergence, max_iterations)
        - compute the pressure and dp_drho from the EOS of each material.
        - Compute delta Pressure
        - Compute delta volume fraction and update the 
          volume fraction and the celldensity.
        - Test for convergence of delta pressure and delta volume fraction
    - END WHILE LOOP
    - bulletproofing
    end
 
Note:  The nomenclature follows the reference.   
_____________________________________________________________________*/
void ICE::computeEquilibrationPressure(const ProcessorGroup*,  
					 const PatchSubset* patches,
                                    const MaterialSubset* /*matls*/,
					 DataWarehouse* old_dw, 
					 DataWarehouse* new_dw)
{
  for(int p=0;p<patches->size();p++){
    const Patch* patch = patches->get(p);
  #ifdef DOING
    cout << "Doing calc_equilibration_pressure on patch " << patch->getID() 
         << "\t\t ICE" << endl;
  #endif
    double    converg_coeff = 15;              
    double    convergence_crit = converg_coeff * DBL_EPSILON;
    double    sum, tmp;

    int       numMatls = d_sharedState->getNumICEMatls();
    char      warning[100];
    static int n_passes;                  
    n_passes ++; 

    StaticArray<double> delVol_frac(numMatls),press_eos(numMatls);
    StaticArray<double> dp_drho(numMatls),dp_de(numMatls);
    StaticArray<CCVariable<double> > vol_frac(numMatls);
    StaticArray<CCVariable<double> > rho_micro(numMatls);
    StaticArray<CCVariable<double> > rho_CC(numMatls);
    StaticArray<CCVariable<double> > rho_CC_new(numMatls);
    StaticArray<CCVariable<double> > Temp(numMatls);
    StaticArray<CCVariable<double> > speedSound(numMatls),speedSound_new(numMatls);
    CCVariable<int> n_iters_equil_press;
    CCVariable<double> press,press_new;
    StaticArray<double> cv(numMatls);

    old_dw->get(press,         lb->press_CCLabel, 0,patch,Ghost::None, 0); 
    new_dw->allocate(press_new,lb->press_equil_CCLabel, 0,patch);

    for (int m = 0; m < numMatls; m++) {
      ICEMaterial* matl = d_sharedState->getICEMaterial(m);
      int indx = matl->getDWIndex();
      old_dw->get(Temp[m],  lb->temp_CCLabel,         indx,patch,
                                                    Ghost::None, 0);
      old_dw->get(rho_CC[m],lb->rho_CC_top_cycleLabel,indx,patch,
		                                      Ghost::None,0);

      new_dw->allocate(speedSound_new[m],lb->speedSound_CCLabel,indx, patch);
      new_dw->allocate(rho_micro[m],     lb->rho_micro_CCLabel, indx, patch);
      new_dw->allocate(vol_frac[m],      lb->vol_frac_CCLabel,  indx, patch);
      new_dw->allocate(rho_CC_new[m],    lb->rho_CCLabel, indx, patch);
      cv[m] = matl->getSpecificHeat();
    }

    press_new.copyPatch(press);
    //__________________________________
    // Compute rho_micro, speedSound, and volfrac
    for (CellIterator iter=patch->getExtraCellIterator();!iter.done();iter++) {
      for (int m = 0; m < numMatls; m++) {
        ICEMaterial* ice_matl = d_sharedState->getICEMaterial(m);
        double gamma = ice_matl->getGamma();
        rho_micro[m][*iter] = 
	  ice_matl->getEOS()->computeRhoMicro(press_new[*iter],gamma,cv[m],
					      Temp[m][*iter]); 

          ice_matl->getEOS()->computePressEOS(rho_micro[m][*iter],gamma,
                                          cv[m], Temp[m][*iter],
                                          press_eos[m], dp_drho[m], dp_de[m]);

        tmp = dp_drho[m] + dp_de[m] * 
	  (press_eos[m]/(rho_micro[m][*iter]*rho_micro[m][*iter]));
        speedSound_new[m][*iter] = sqrt(tmp);
        vol_frac[m][*iter] = rho_CC[m][*iter]/rho_micro[m][*iter];
      }
    }

   //---- P R I N T   D A T A ------  
    if (switchDebug_equilibration_press) {
    
      new_dw->allocate(n_iters_equil_press, lb->scratchLabel, 0, patch);
      
    #if 0
      printData( patch, 1, "TOP_equilibration", "Press_CC_top", press);

     for (int m = 0; m < numMatls; m++)  {
       ICEMaterial* matl = d_sharedState->getICEMaterial( m );
       int indx = matl->getDWIndex(); 
       char description[50];
       sprintf(description, "TOP_equilibration_Mat_%d ",   indx);
       printData( patch, 1, description, "rho_CC",          rho_CC[m]);
       printData( patch, 1, description, "rho_micro_CC",    rho_micro[m]);
       printData( patch, 0, description, "speedSound",      speedSound_new[m]);
       printData( patch, 1, description, "Temp_CC",         Temp[m]);
       printData( patch, 1, description, "vol_frac_CC",     vol_frac[m]);
      }
     #endif
    }

  //______________________________________________________________________
  // Done with preliminary calcs, now loop over every cell
    int count, test_max_iter = 0;
    for (CellIterator iter=patch->getExtraCellIterator();!iter.done();iter++) {

      IntVector curcell = *iter;    //So I have a chance at finding bugs -Todd
      int i,j,k;
      i   = curcell.x();
      j   = curcell.y();
      k   = curcell.z();

      double delPress = 0.;
      bool converged  = false;
      count           = 0;
      while ( count < d_max_iter_equilibration && converged == false) {
        count++;
        double A = 0.;
        double B = 0.;
        double C = 0.;

        for (int m = 0; m < numMatls; m++) 
          delVol_frac[m] = 0.;
        //__________________________________
       // evaluate press_eos at cell i,j,k
       for (int m = 0; m < numMatls; m++)  {
         ICEMaterial* ice_matl = d_sharedState->getICEMaterial(m);
         double gamma = ice_matl->getGamma();

         ice_matl->getEOS()->computePressEOS(rho_micro[m][*iter],gamma,
                                           cv[m], Temp[m][*iter],
                                           press_eos[m], dp_drho[m], dp_de[m]);
       }
       //__________________________________
       // - compute delPress
       // - update press_CC     
       StaticArray<double> Q(numMatls),y(numMatls);     
       for (int m = 0; m < numMatls; m++)   {
         Q[m] =  press_new[*iter] - press_eos[m];
         y[m] =  dp_drho[m] * ( rho_CC[m][*iter]/
                 (vol_frac[m][*iter] * vol_frac[m][*iter]) ); 
         A   +=  vol_frac[m][*iter];
         B   +=  Q[m]/y[m];
         C   +=  1.0/y[m];
       }
       double vol_frac_not_close_packed = 1.;
       delPress = (A - vol_frac_not_close_packed - B)/C;

       press_new[*iter] += delPress;

       //__________________________________
       // backout rho_micro_CC at this new pressure
       for (int m = 0; m < numMatls; m++) {
         ICEMaterial* ice_matl = d_sharedState->getICEMaterial(m);
         double gamma = ice_matl->getGamma();

         rho_micro[m][*iter] = 
           ice_matl->getEOS()->computeRhoMicro(press_new[*iter],gamma,
                                               cv[m],Temp[m][*iter]);
       }
       //__________________________________
       // - compute the updated volume fractions
       for (int m = 0; m < numMatls; m++)  {
         delVol_frac[m]       = -(Q[m] + delPress)/y[m];
         vol_frac[m][*iter]   = rho_CC[m][*iter]/rho_micro[m][*iter];
       }
       //__________________________________
       // Find the speed of sound 
       // needed by eos and the explicit
       // del pressure function
       for (int m = 0; m < numMatls; m++)  {
          ICEMaterial* ice_matl = d_sharedState->getICEMaterial(m);
          double gamma = ice_matl->getGamma();
          ice_matl->getEOS()->computePressEOS(rho_micro[m][*iter],gamma,
                                            cv[m],Temp[m][*iter],
                                            press_eos[m],dp_drho[m], dp_de[m]);

          tmp = dp_drho[m] + dp_de[m] * 
                      (press_eos[m]/(rho_micro[m][*iter]*rho_micro[m][*iter]));
          speedSound_new[m][*iter] = sqrt(tmp);
       }
       //__________________________________
       // - Test for convergence 
       //  If sum of vol_frac_CC ~= 1.0 then converged 
       sum = 0.0;
       for (int m = 0; m < numMatls; m++)  {
         sum += vol_frac[m][*iter];
       }
       if (fabs(sum-1.0) < convergence_crit)
         converged = true;

      }   // end of converged

      test_max_iter = std::max(test_max_iter, count);

      //__________________________________
      //      BULLET PROOFING
      if(test_max_iter == d_max_iter_equilibration)  {
          sprintf(warning, 
          " cell[%d][%d][%d], iter %d, n_passes %d,Now exiting ",
          i,j,k,count,n_passes);
           Message(1,"calc_equilibration_press:",
              " Maximum number of iterations was reached ", warning);
      }

       for (int m = 0; m < numMatls; m++) {
           ASSERT(( vol_frac[m][*iter] > 0.0 ) ||
                  ( vol_frac[m][*iter] < 1.0));
       }
      if ( fabs(sum - 1.0) > convergence_crit)   {
          sprintf(warning, 
          " cell[%d][%d][%d], iter %d, n_passes %d,Now exiting ",
          i,j,k,count,n_passes);
          Message(1,"calc_equilibration_press:",
              " sum(vol_frac_CC) != 1.0", warning);
      }

      if ( press_new[*iter] < 0.0 )   {
          sprintf(warning, 
          " cell[%d][%d][%d], iter %d, n_passes %d, Now exiting",
           i,j,k, count, n_passes);
          Message(1,"calc_equilibration_press:", 
              " press_new[iter*] < 0", warning);
      }

      for (int m = 0; m < numMatls; m++)
      if ( rho_micro[m][*iter] < 0.0 || vol_frac[m][*iter] < 0.0) {
          sprintf(warning, 
          " cell[%d][%d][%d], mat %d, iter %d, n_passes %d,Now exiting ",
          i,j,k,m,count,n_passes);
          Message(1," calc_equilibration_press:", 
              " rho_micro < 0 || vol_frac < 0", warning);
      }
      if (switchDebug_equilibration_press) {
        n_iters_equil_press[*iter] = count;
      }
    }     // end of cell interator

    fprintf(stderr, "\n max. iterations in any cell %i\n", test_max_iter); 
    //__________________________________
    //  press_CC boundary conditions are updated in
    //  ICE::ComputeDelPressAndUpdateCC()
    
    //__________________________________
    // carry rho_cc forward 
    // In MPMICE was compute rho_CC_new and 
    // therefore need the machinery here
    for (int m = 0; m < numMatls; m++)   {
      ICEMaterial* ice_matl = d_sharedState->getICEMaterial(m);
      int indx = ice_matl->getDWIndex();
      rho_CC_new[m].copyPatch(rho_CC[m]);
      new_dw->put( vol_frac[m],      lb->vol_frac_CCLabel,   indx, patch);
      new_dw->put( speedSound_new[m],lb->speedSound_CCLabel, indx, patch);
      new_dw->put( rho_micro[m],     lb->rho_micro_CCLabel,  indx, patch);
      new_dw->put( rho_CC_new[m],    lb->rho_CCLabel,        indx, patch);
    }
    new_dw->put(press_new,lb->press_equil_CCLabel,0,patch);

   //---- P R I N T   D A T A ------   
    if (switchDebug_equilibration_press) {
     printData( patch, 1, "BOTTOM", "Press_CC_equil", press_new);

     for (int m = 0; m < numMatls; m++)  {
       ICEMaterial* matl = d_sharedState->getICEMaterial( m );
       int indx = matl->getDWIndex(); 
       char description[50];
       sprintf(description, "BOT_equilibration_Mat_%d ", indx);
       printData( patch, 1, description, "rho_CC",       rho_CC[m]);
     //printData( patch, 1, description, "speedSound",   speedSound_new[m]);
       printData( patch, 1, description, "rho_micro_CC", rho_micro[m]);
       printData( patch, 1, description, "vol_frac_CC",  vol_frac[m]);
     //printData( patch, 1, description, "iterations",   n_iters_equil_press);

      }
    }
  }  // patch loop
}

/* ---------------------------------------------------------------------
 Function~  ICE::computeFaceCenteredVelocities--
 Purpose~   compute the face centered velocities minus the exchange
            contribution.
_____________________________________________________________________*/
void ICE::computeFaceCenteredVelocities(const ProcessorGroup*,  
					 const PatchSubset* patches,
                                    const MaterialSubset* /*matls*/,
					 DataWarehouse* old_dw, 
					 DataWarehouse* new_dw)
{
  for(int p = 0; p<patches->size(); p++){
    const Patch* patch = patches->get(p);
    

  #ifdef DOING
    cout << "Doing compute_face_centered_velocities on patch " 
         << patch->getID() << "\t ICE" << endl;
  #endif

    int numMatls = d_sharedState->getNumMatls();

    delt_vartype delT;
    old_dw->get(delT, d_sharedState->get_delt_label());
    Vector dx      = patch->dCell();
    Vector gravity = d_sharedState->getGravity();

    CCVariable<double> press_CC;
    new_dw->get(press_CC,lb->press_equil_CCLabel, 0, patch, 
	        Ghost::AroundCells, 1);


    // Compute the face centered velocities
    for(int m = 0; m < numMatls; m++) {
      Material* matl = d_sharedState->getMaterial( m );
      int indx = matl->getDWIndex();
      ICEMaterial* ice_matl = dynamic_cast<ICEMaterial*>(matl);
      CCVariable<double> rho_CC, rho_micro_CC;
      CCVariable<Vector> vel_CC;
      if(ice_matl){
        new_dw->get(rho_CC, lb->rho_CCLabel, indx, patch, 
		    Ghost::AroundCells, 1);
        old_dw->get(vel_CC, lb->vel_CCLabel, indx, patch, 
		    Ghost::AroundCells, 1);
      } else {
        new_dw->get(rho_CC, lb->rho_CCLabel, indx, patch, 
		    Ghost::AroundCells, 1);
        new_dw->get(vel_CC, lb->vel_CCLabel, indx, patch, 
		    Ghost::AroundCells, 1);
      }
      new_dw->get(rho_micro_CC, lb->rho_micro_CCLabel,indx,patch,
		  Ghost::AroundCells, 1);


   //---- P R I N T   D A T A ------ 
      if (switchDebug_vel_FC ) {
      #if 0
        char description[50];
        sprintf(description, "TOP_vel_FC_Mat_%d ",indx); 
        printData(  patch, 1, description, "rho_CC",      rho_CC);
        printData(  patch, 1, description, "rho_micro_CC",rho_micro_CC);
        printVector( patch,1, description, "uvel_CC", 0, vel_CC);
        printVector( patch,1, description, "vvel_CC", 1, vel_CC);
        printVector( patch,1, description, "wvel_CC", 2, vel_CC);
      #endif
      }

      SFCXVariable<double> uvel_FC;
      SFCYVariable<double> vvel_FC;
      SFCZVariable<double> wvel_FC;
      new_dw->allocate(uvel_FC, lb->uvel_FCLabel, indx, patch);
      new_dw->allocate(vvel_FC, lb->vvel_FCLabel, indx, patch);
      new_dw->allocate(wvel_FC, lb->wvel_FCLabel, indx, patch);
      IntVector lowIndex(patch->getSFCXLowIndex());
      uvel_FC.initialize(0.0, lowIndex,patch->getSFCXHighIndex());
      vvel_FC.initialize(0.0, lowIndex,patch->getSFCYHighIndex());
      wvel_FC.initialize(0.0, lowIndex,patch->getSFCZHighIndex());

      double term1, term2, term3, press_coeff, rho_micro_FC, rho_FC;

      for(CellIterator iter=patch->getExtraCellIterator();!iter.done();iter++){
        IntVector curcell = *iter;
        //__________________________________
        //   B O T T O M   F A C E S 
        //   Extend the computations into the left
        //   and right ghost cells 
        if (curcell.y() >= (patch->getInteriorCellLowIndex()).y()) {
	  IntVector adjcell(curcell.x(),curcell.y()-1,curcell.z()); 

	  rho_micro_FC = rho_micro_CC[adjcell] + rho_micro_CC[curcell];
	  rho_FC       = rho_CC[adjcell]       + rho_CC[curcell];
	  ASSERT(rho_FC > 0.0);
	  //__________________________________
	  // interpolation to the face
	  term1 = (rho_CC[adjcell] * vel_CC[adjcell].y() +
		   rho_CC[curcell] * vel_CC[curcell].y())/(rho_FC);            
	  //__________________________________
	  // pressure term
	  press_coeff = 2.0/(rho_micro_FC);
	  term2 =   delT * press_coeff *
	    (press_CC[curcell] - press_CC[adjcell])/dx.y();                
	  //__________________________________
	  // gravity term
	  term3 =  delT * gravity.y();
	  vvel_FC[curcell] = term1- term2 + term3;
        }

        //__________________________________
        //  L E F T   F A C E 
        // Extend the computations to the 
        // top and bottom ghostcells 

        if (curcell.x() >= (patch->getInteriorCellLowIndex()).x()) {
	  // Now setting the left face because of ownership of faces by
	  // multi-patches restriction of UCF.
	  IntVector adjcell(curcell.x()-1,curcell.y(),curcell.z()); 

	  rho_micro_FC = rho_micro_CC[adjcell] + rho_micro_CC[curcell];
	  rho_FC       = rho_CC[adjcell]       + rho_CC[curcell];
	  ASSERT(rho_FC > 0.0);
	  //__________________________________
	  // interpolation to the face
	  term1 = (rho_CC[adjcell] * vel_CC[adjcell].x() +
		    rho_CC[curcell] * vel_CC[curcell].x())/(rho_FC);
	  //__________________________________
	  // pressure term
	  press_coeff = 2.0/(rho_micro_FC);

	  term2 =   delT * press_coeff *
	    (press_CC[curcell] - press_CC[adjcell])/dx.x();
	  //__________________________________
	  // gravity term
	  term3 =  delT * gravity.x();
	  uvel_FC[curcell] = term1- term2 + term3;
        }

        //__________________________________
        //  B A C K    F A C E
        // Extend the computations to the front
        // and back ghostcells
        if (curcell.z() >= (patch->getInteriorCellLowIndex()).z())  {
	  IntVector adjcell(curcell.x(),curcell.y(),curcell.z()-1); 

	  rho_micro_FC = rho_micro_CC[adjcell] + rho_micro_CC[curcell];
	  rho_FC       = rho_CC[adjcell]       + rho_CC[curcell];
	  ASSERT(rho_FC > 0.0);
	  //__________________________________
	  // interpolation to the face
	  term1 = (rho_CC[adjcell] * vel_CC[adjcell].z() +
		   rho_CC[curcell] * vel_CC[curcell].z())/(rho_FC);
	  //__________________________________
	  // pressure term
	  press_coeff = 2.0/(rho_micro_FC);

	  term2 =   delT * press_coeff *
	    (press_CC[curcell] - press_CC[adjcell])/dx.z();
	  //__________________________________
	  // gravity term
	  term3 =  delT * gravity.z();
	  wvel_FC[curcell] = term1- term2 + term3;
        }
      }

      //__________________________________
      // (*)vel_FC BC are updated in 
      // ICE::addExchangeContributionToFCVel()

      new_dw->put(uvel_FC, lb->uvel_FCLabel, indx, patch);
      new_dw->put(vvel_FC, lb->vvel_FCLabel, indx, patch);
      new_dw->put(wvel_FC, lb->wvel_FCLabel, indx, patch);

   //---- P R I N T   D A T A ------ 
      if (switchDebug_vel_FC ) {
        char description[50];
        sprintf(description, "bottom_of_vel_FC_Mat_%d ",indx);
        printData_FC( patch,1, description, "uvel_FC", uvel_FC);
        printData_FC( patch,1, description, "vvel_FC", vvel_FC);
        printData_FC( patch,1, description, "wvel_FC", wvel_FC);
      }
    }
  }  // patch loop
}

/*---------------------------------------------------------------------
 Function~  addExchangeContributionToFCVel--
 Purpose~
   This function adds the momentum exchange contribution to the 
   existing face-centered velocities

 Prerequisites:
            The face centered velocity for each material without
            the exchange must be solved prior to this routine.
            
                   (A)                              (X)
| (1+b12 + b13)     -b12          -b23          |   |del_FC[1]  |    
|                                               |   |           |    
| -b21              (1+b21 + b23) -b32          |   |del_FC[2]  |    
|                                               |   |           | 
| -b31              -b32          (1+b31 + b32) |   |del_FC[2]  |

                        =
                        
                        (B)
| b12( uvel_FC[2] - uvel_FC[1] ) + b13 ( uvel_FC[3] -uvel_FC[1])    | 
|                                                                   |
| b21( uvel_FC[1] - uvel_FC[2] ) + b23 ( uvel_FC[3] -uvel_FC[2])    | 
|                                                                   |
| b31( uvel_FC[1] - uvel_FC[3] ) + b32 ( uvel_FC[2] -uvel_FC[3])    | 
 
 Steps for each face:
    1) Comute the beta coefficients
    2) Form and A matrix and B vector
    3) Solve for del_FC[*]
    4) Add del_FC[*] to the appropriate velocity
 
 References: see "A Cell-Centered ICE method for multiphase flow simulations"
 by Kashiwa, above equation 4.13.
 ---------------------------------------------------------------------  */
void ICE::addExchangeContributionToFCVel(const ProcessorGroup*,  
					 const PatchSubset* patches,
                                    const MaterialSubset* /*matls*/,
					 DataWarehouse* old_dw, 
					 DataWarehouse* new_dw)
{
  for(int p=0;p<patches->size();p++){
    const Patch* patch = patches->get(p);
  #ifdef DOING
    cout << "Doing Add_exchange_contribution_to_FC_vel on patch " <<
      patch->getID() << "\t ICE" << endl;
  #endif

    int numMatls = d_sharedState->getNumMatls();
    delt_vartype delT;
    old_dw->get(delT, d_sharedState->get_delt_label());
    double tmp;

    StaticArray<CCVariable<double> > rho_micro_CC(numMatls);
    StaticArray<CCVariable<double> > vol_frac_CC(numMatls);
    StaticArray<SFCXVariable<double> > uvel_FC(numMatls);
    StaticArray<SFCYVariable<double> > vvel_FC(numMatls);
    StaticArray<SFCZVariable<double> > wvel_FC(numMatls);

    StaticArray<SFCXVariable<double> > uvel_FCME(numMatls);
    StaticArray<SFCYVariable<double> > vvel_FCME(numMatls);
    StaticArray<SFCZVariable<double> > wvel_FCME(numMatls);
    // lowIndex is the same for all vel_FC
    IntVector lowIndex(patch->getSFCXLowIndex()); 
    
    // Extract the momentum exchange coefficients
    vector<double> b(numMatls);
    DenseMatrix beta(numMatls,numMatls),a(numMatls,numMatls);

    DenseMatrix K(numMatls,numMatls), junk(numMatls,numMatls);

    beta.zero();
    a.zero();
    K.zero();
    getExchangeCoefficients( K, junk);
    
    for(int m = 0; m < numMatls; m++) {
      Material* matl = d_sharedState->getMaterial( m );
      int indx = matl->getDWIndex();
      new_dw->get(rho_micro_CC[m], lb->rho_micro_CCLabel, indx, patch, 
		  Ghost::AroundCells, 1);         
      new_dw->get(vol_frac_CC[m],  lb->vol_frac_CCLabel,indx, patch, 
		  Ghost::AroundCells, 1);
      new_dw->get(uvel_FC[m], lb->uvel_FCLabel, indx, patch, 
		  Ghost::AroundCells, 2);
      new_dw->get(vvel_FC[m], lb->vvel_FCLabel, indx, patch, 
		  Ghost::AroundCells, 2);
      new_dw->get(wvel_FC[m], lb->wvel_FCLabel, indx, patch, 
		  Ghost::AroundCells, 2);

      new_dw->allocate(uvel_FCME[m], lb->uvel_FCMELabel, indx, patch);
      new_dw->allocate(vvel_FCME[m], lb->vvel_FCMELabel, indx, patch);
      new_dw->allocate(wvel_FCME[m], lb->wvel_FCMELabel, indx, patch);

      uvel_FCME[m].initialize(0.0, lowIndex,patch->getSFCXHighIndex());
      vvel_FCME[m].initialize(0.0, lowIndex,patch->getSFCYHighIndex());
      wvel_FCME[m].initialize(0.0, lowIndex,patch->getSFCZHighIndex());
    }
    
    for(CellIterator iter=patch->getExtraCellIterator(); !iter.done(); iter++){
      IntVector curcell = *iter;
      //__________________________________
      //    B O T T O M  F A C E -- B  E  T  A      
      //  Note this includes b[m][m]
      //  You need to make sure that mom_exch_coeff[m][m] = 0
      //   - form off diagonal terms of (a) 
      if (curcell.y() >= (patch->getInteriorCellLowIndex()).y()) {
        IntVector adjcell(curcell.x(),curcell.y()-1,curcell.z()); 
        for(int m = 0; m < numMatls; m++) {
	  for(int n = 0; n < numMatls; n++) {
	    tmp = (vol_frac_CC[n][adjcell] + vol_frac_CC[n][curcell]) * K[n][m];

	    beta[m][n] = delT * tmp/
	      (rho_micro_CC[m][curcell] + rho_micro_CC[m][adjcell]);

	    a[m][n] = -beta[m][n];
	  }
        }
        //__________________________________
        //  F  O  R  M     M  A  T  R  I  X   (a)
        //  - Diagonal terms      
        for(int m = 0; m < numMatls; m++) {
	  a[m][m] = 1.;
	  for(int n = 0; n < numMatls; n++) {
	    a[m][m] +=  beta[m][n];
	  }
        }
        //__________________________________
        //    F  O  R  M     R  H  S  (b)     
        for(int m = 0; m < numMatls; m++) {
	  b[m] = 0.0;
	  for(int n = 0; n < numMatls; n++)  {
	    b[m] += beta[m][n] * (vvel_FC[n][curcell] - vvel_FC[m][curcell]);
	  }
        }
        //__________________________________
        //      S  O  L  V  E  
        //   - backout velocities           
        a.solve(b);
        for(int m = 0; m < numMatls; m++)  {
	  vvel_FCME[m][curcell] = vvel_FC[m][curcell] + b[m];
        }
      }

      //__________________________________
      //   L E F T  F A C E-- B  E  T  A      
      //  Note this includes b[m][m]
      //  You need to make sure that mom_exch_coeff[m][m] = 0
      //   - form off diagonal terms of (a)
      if (curcell.x() >= (patch->getInteriorCellLowIndex()).x())  {
        IntVector adjcell(curcell.x()-1,curcell.y(),curcell.z()); 

        for(int m = 0; m < numMatls; m++)  {
	  for(int n = 0; n < numMatls; n++)  {
	    tmp = (vol_frac_CC[n][adjcell] + vol_frac_CC[n][curcell]) * K[n][m];
	    beta[m][n] = delT * tmp/
	      (rho_micro_CC[m][curcell] + rho_micro_CC[m][adjcell]);

	    a[m][n] = -beta[m][n];
	  }
        }
        /*__________________________________
         *  F  O  R  M     M  A  T  R  I  X   (a)
         * - Diagonal terms
         *___________________________________*/
        for(int m = 0; m < numMatls; m++) {
	  a[m][m] = 1.;
	  for(int n = 0; n < numMatls; n++) {
	    a[m][m] +=  beta[m][n];
	  }
        }

        //__________________________________
        //    F  O  R  M     R  H  S  (b) 
        for(int m = 0; m < numMatls; m++)  {
	  b[m] = 0.0;
	  for(int n = 0; n < numMatls; n++)  {
	    b[m] += beta[m][n] * (uvel_FC[n][curcell] - uvel_FC[m][curcell]);
	  }
        }
        //__________________________________
        //      S  O  L  V  E
        //   - backout velocities
        a.solve(b);

        for(int m = 0; m < numMatls; m++) {
	  uvel_FCME[m][curcell] = uvel_FC[m][curcell] + b[m];
        }
      }
      //__________________________________
      //  B A C K  F A C E -- B  E  T  A      
      //  Note this includes b[m][m]
      //  You need to make sure that mom_exch_coeff[m][m] = 0
      //   - form off diagonal terms of (a)
      if (curcell.z() >= (patch->getInteriorCellLowIndex()).z())  {
        IntVector adjcell(curcell.x(),curcell.y(),curcell.z()-1); 
        for(int m = 0; m < numMatls; m++)  {
	  for(int n = 0; n < numMatls; n++) {
	    tmp = (vol_frac_CC[n][adjcell] + vol_frac_CC[n][curcell]) * K[n][m];
	    beta[m][n] = delT * tmp/
	      (rho_micro_CC[m][curcell] + rho_micro_CC[m][adjcell]);

	    a[m][n] = -beta[m][n];
	  }
        }
        //__________________________________
        //  F  O  R  M     M  A  T  R  I  X   (a)
        // - Diagonal terms
        for(int m = 0; m < numMatls; m++) {
	  a[m][m] = 1.;
	  for(int n = 0; n < numMatls; n++) {
	    a[m][m] +=  beta[m][n];
	  }
        }
        //__________________________________
        //    F  O  R  M     R  H  S  (b)
        for(int m = 0; m < numMatls; m++) {
	  b[m] = 0.0;
	  for(int n = 0; n < numMatls; n++) {
	    b[m] += beta[m][n] * (wvel_FC[n][curcell] - wvel_FC[m][curcell]);
	  }
        }
        //__________________________________
        //      S  O  L  V  E
        //   - backout velocities 
        a.solve(b);
        for(int m = 0; m < numMatls; m++) {
	  wvel_FCME[m][curcell] = wvel_FC[m][curcell] + b[m];
        }
      }
    }

    for (int m = 0; m < numMatls; m++)  {
      Material* matl = d_sharedState->getMaterial( m );
      int indx = matl->getDWIndex();
      setBC(uvel_FCME[m],"Velocity","x",patch,indx);
      setBC(vvel_FCME[m],"Velocity","y",patch,indx);
      setBC(wvel_FCME[m],"Velocity","z",patch,indx);
    }

   //---- P R I N T   D A T A ------ 
    if (switchDebug_Exchange_FC ) {
      for (int m = 0; m < numMatls; m++)  {
      Material* matl = d_sharedState->getMaterial( m );
      int indx = matl->getDWIndex();
      char description[50];
      sprintf(description, "Exchange_FC_after_BC_Mat_%d ",indx);
      printData_FC( patch,1, description, "uvel_FCME", uvel_FCME[m]);
      printData_FC( patch,1, description, "vvel_FCME", vvel_FCME[m]);
      printData_FC( patch,1, description, "wvel_FCME", wvel_FCME[m]);
      }
    }

    for(int m = 0; m < numMatls; m++) {
      Material* matl = d_sharedState->getMaterial( m );
      int indx = matl->getDWIndex();
      new_dw->put(uvel_FCME[m], lb->uvel_FCMELabel, indx, patch);
      new_dw->put(vvel_FCME[m], lb->vvel_FCMELabel, indx, patch);
      new_dw->put(wvel_FCME[m], lb->wvel_FCMELabel, indx, patch);
    }
  }  // patch loop  
}

/*---------------------------------------------------------------------
 Function~  ICE::computeDelPressAndUpdatePressCC--
 Purpose~
   This function calculates the change in pressure explicitly. 
 Note:  Units of delpress are [Pa]
 Reference:  Multimaterial Formalism eq. 1.5
 ---------------------------------------------------------------------  */
void ICE::computeDelPressAndUpdatePressCC(const ProcessorGroup*,  
					  const PatchSubset* patches,
                                     const MaterialSubset* /*matls*/,
					  DataWarehouse* old_dw, 
					  DataWarehouse* new_dw)
{
  for(int p=0;p<patches->size();p++){
    const Patch* patch = patches->get(p);
  #ifdef DOING
    cout << "Doing explicit delPress on patch " << patch->getID() 
         <<  "\t\t\t ICE" << endl;
  #endif
    int numMatls  = d_sharedState->getNumMatls();
    delt_vartype delT;
    old_dw->get(delT, d_sharedState->get_delt_label());
    Vector dx     = patch->dCell();

    double vol    = dx.x()*dx.y()*dx.z();
    double invvol = 1./vol;

    CCVariable<double> q_CC,      q_advected;
    CCVariable<fflux> OFS;
    CCVariable<eflux> OFE;
    CCVariable<cflux> OFC;                    
    CCVariable<double> pressure;
    CCVariable<double> delPress;
    CCVariable<double> press_CC;
    CCVariable<double> burnedMass;
   
    StaticArray<CCVariable<double>   > rho_micro_CC(numMatls);
    StaticArray<CCVariable<double>   > vol_frac(numMatls);
    StaticArray<CCVariable<Vector>   > vel_CC(numMatls);
    StaticArray<CCVariable<double>   > rho_CC(numMatls);

    const IntVector gc(1,1,1);

    new_dw->get(pressure,       lb->press_equil_CCLabel,0,patch,Ghost::None,0);
    new_dw->allocate(delPress,  lb->delPress_CCLabel,  0, patch);
    new_dw->allocate(press_CC,  lb->press_CCLabel,     0, patch);
    new_dw->allocate(q_CC,      lb->q_CCLabel,         0, patch,gc);
    new_dw->allocate(q_advected,lb->q_advectedLabel,   0, patch);
    new_dw->allocate(OFS,       OFS_CCLabel,           0, patch,gc);
    new_dw->allocate(OFE,       OFE_CCLabel,           0, patch,gc);
    new_dw->allocate(OFC,       OFC_CCLabel,           0, patch,gc);

    CCVariable<double> term1, term2, term3;
    new_dw->allocate(term1, lb->term1Label, 0, patch);
    new_dw->allocate(term2, lb->term2Label, 0, patch);
    new_dw->allocate(term3, lb->term3Label, 0, patch);

    term1.initialize(0.);
    term2.initialize(0.);
    term3.initialize(0.);
    delPress.initialize(0.0);

    for(int m = 0; m < numMatls; m++) {
      Material* matl = d_sharedState->getMaterial( m );
      int indx = matl->getDWIndex();
      ICEMaterial* ice_matl = dynamic_cast<ICEMaterial*>(matl);
      CCVariable<double>   speedSound;
      SFCXVariable<double> uvel_FC;
      SFCYVariable<double> vvel_FC;
      SFCZVariable<double> wvel_FC;
      new_dw->get(uvel_FC, lb->uvel_FCMELabel, indx,  patch,
		  Ghost::AroundCells, 2);
      new_dw->get(vvel_FC, lb->vvel_FCMELabel, indx,  patch,
		  Ghost::AroundCells, 2);
      new_dw->get(wvel_FC, lb->wvel_FCMELabel, indx,  patch,
		  Ghost::AroundCells, 2);
      new_dw->get(rho_CC[m],lb->rho_CCLabel,   indx,  patch,
		  Ghost::None,0);
 /*`==========TESTING==========*/ 
  if(ice_matl) {
     old_dw->get(vel_CC[m],lb->vel_CCLabel,   indx,  patch,
  		  Ghost::None,0);
  }
 /*==========TESTING==========`*/
      new_dw->get(vol_frac[m],    lb->vol_frac_CCLabel,  indx,patch,
		  Ghost::AroundCells,1);
      new_dw->get(rho_micro_CC[m],lb->rho_micro_CCLabel, indx,patch,
		  Ghost::None,0);
      new_dw->get(speedSound,     lb->speedSound_CCLabel,indx,patch,
		  Ghost::None,0);
      new_dw->get(burnedMass,     lb->burnedMass_CCLabel,indx,patch,
		  Ghost::None,0);

      //__________________________________
      // Advection preprocessing
      // - divide vol_frac_cc/vol
      influxOutfluxVolume(uvel_FC, vvel_FC, wvel_FC,
                        delT, patch, OFS, OFE, OFC);

      for(CellIterator iter = patch->getCellIterator(gc); !iter.done();
	  iter++) {
        q_CC[*iter] = vol_frac[m][*iter] * invvol;
      }
      //__________________________________
      //   First order advection of q_CC
      advectQFirst(q_CC, patch,OFS,OFE,OFC, q_advected);

      //---- P R I N T   D A T A ------  
      if (switchDebug_explicit_press ) {
        char description[50];
        sprintf(description, "middle_of_explicit_Pressure_Mat_%d ",indx);
        printData_FC( patch,1, description, "uvel_FC", uvel_FC);
        printData_FC( patch,1, description, "vvel_FC", vvel_FC);
        printData_FC( patch,1, description, "wvel_FC", wvel_FC);
      }

      for(CellIterator iter=patch->getCellIterator(); !iter.done();iter++) {
        //__________________________________
        //   Contributions from reactions
        //   to be filled in Be very careful with units
        term1[*iter] += burnedMass[*iter]/(rho_micro_CC[m][*iter] * vol);

        //__________________________________
        //   Divergence of velocity * face area
        //   Be very careful with the units
        //   do the volume integral to check them
        //   See journal pg 171
        //   You need to divide by the cell volume
        //
        //  Note that sum(div (theta_k U^f_k) 
        //          =
        //  Advection(theta_k, U^f_k)
        //  This subtle point is discussed on pg
        //  190 of my Journal
        term2[*iter] -= q_advected[*iter];

        term3[*iter] += vol_frac[m][*iter] /(rho_micro_CC[m][*iter] *
				speedSound[*iter]*speedSound[*iter]);
      }  //iter loop
    }  //matl loop
    press_CC.copyPatch(pressure);
    for(CellIterator iter = patch->getCellIterator(); !iter.done(); iter++) { 

      delPress[*iter] = (delT * term1[*iter] - term2[*iter])/(term3[*iter]);
      press_CC[*iter]  = pressure[*iter] + delPress[*iter];    
    }
/*`==========TESTING==========*/ 
  #if oldStyle_setBC
    setBC(press_CC, rho_micro_CC[SURROUND_MAT], "Pressure",patch,0);
  #endif
  #if newStyle_setBC
    setBC(press_CC,     rho_micro_CC,   rho_CC,
          vol_frac,     vel_CC,         old_dw,
          "Pressure",   patch,0);
    #endif
 /*==========TESTING==========`*/

    new_dw->put(delPress, lb->delPress_CCLabel, 0, patch);
    new_dw->put(press_CC, lb->press_CCLabel,    0, patch);

   //---- P R I N T   D A T A ------  
    if (switchDebug_explicit_press) {
      printData( patch, 1, "Bottom_of_explicit_Pressure ", "delPress_CC",  
                                                                    delPress);
      printData( patch, 1, "Bottom_of_explicit_Pressure",  "Press_CC",    
                                                                    press_CC);
    }
  }  // patch loop
}

/* ---------------------------------------------------------------------  
 Function~  ICE::computePressFC--
 Purpose~
    This function calculates the face centered pressure on each of the 
    cell faces for every cell in the computational domain and a single 
    layer of ghost cells. 
  ---------------------------------------------------------------------  */
void ICE::computePressFC(const ProcessorGroup*,   
                    const PatchSubset* patches,
                    const MaterialSubset* /*matls*/,
		      DataWarehouse*,
                    DataWarehouse* new_dw)
{
  for(int p=0;p<patches->size();p++){
    const Patch* patch = patches->get(p);
  #ifdef DOING
    cout << "Doing press_face_MM on patch " << patch->getID() 
         << "\t\t\t\t ICE" << endl;
  #endif
    int numMatls = d_sharedState->getNumMatls();
    double sum_rho, sum_rho_adj;
    double A;                                 

    StaticArray<CCVariable<double> > rho_CC(numMatls);
    CCVariable<double> press_CC;
    new_dw->get(press_CC,lb->press_CCLabel, 0, patch, 
	        Ghost::AroundCells, 1);

    SFCXVariable<double> pressX_FC;
    SFCYVariable<double> pressY_FC;
    SFCZVariable<double> pressZ_FC;
    new_dw->allocate(pressX_FC,lb->pressX_FCLabel, 0, patch);
    new_dw->allocate(pressY_FC,lb->pressY_FCLabel, 0, patch);
    new_dw->allocate(pressZ_FC,lb->pressZ_FCLabel, 0, patch);

    // Compute the face centered velocities
    for(int m = 0; m < numMatls; m++)  {
      Material* matl = d_sharedState->getMaterial( m );
      int indx = matl->getDWIndex();
      new_dw->get(rho_CC[m],lb->rho_CCLabel,indx,patch,Ghost::AroundCells,1);
    }


    for(CellIterator iter = patch->getExtraCellIterator();!iter.done();iter++){
      IntVector curcell = *iter;
      //__________________________________
      //  B O T T O M   F A C E
      if (curcell.y() >= (patch->getInteriorCellLowIndex()).y()) {
        IntVector adjcell(curcell.x(),curcell.y()-1,curcell.z());
        sum_rho = 0.0;
        sum_rho_adj = 0.0;
        for(int m = 0; m < numMatls; m++) {
	  sum_rho      += rho_CC[m][curcell];
	  sum_rho_adj  += rho_CC[m][adjcell];
        }

        A =  (press_CC[curcell]/sum_rho) + (press_CC[adjcell]/sum_rho_adj);
        pressY_FC[curcell] = A/((1/sum_rho)+(1.0/sum_rho_adj));
      }

      //__________________________________
      //  L E F T   F A C E
      if (curcell.x() >= (patch->getInteriorCellLowIndex()).x())  {
        IntVector adjcell(curcell.x()-1,curcell.y(),curcell.z());
        sum_rho=0.0;
        sum_rho_adj  = 0.0;

        for(int m = 0; m < numMatls; m++) {
	  sum_rho      += rho_CC[m][curcell];
	  sum_rho_adj  += rho_CC[m][adjcell];
        }

        A =  (press_CC[curcell]/sum_rho) + (press_CC[adjcell]/sum_rho_adj);
        pressX_FC[curcell] = A/((1/sum_rho)+(1.0/sum_rho_adj));
      }
      //__________________________________
      //     B A C K   F A C E 
      if (curcell.z() >= (patch->getInteriorCellLowIndex()).z()) {
        IntVector adjcell(curcell.x(),curcell.y(),curcell.z()-1);

        sum_rho=0.0;
        sum_rho_adj  = 0.0;
        for(int m = 0; m < numMatls; m++) {
	  sum_rho      += rho_CC[m][curcell];
	  sum_rho_adj  += rho_CC[m][adjcell];
        }

        A =  (press_CC[curcell]/sum_rho) + (press_CC[adjcell]/sum_rho_adj);
        pressZ_FC[curcell]=A/((1/sum_rho)+(1.0/sum_rho_adj));
      }
    }

    new_dw->put(pressX_FC,lb->pressX_FCLabel, 0, patch);
    new_dw->put(pressY_FC,lb->pressY_FCLabel, 0, patch);
    new_dw->put(pressZ_FC,lb->pressZ_FCLabel, 0, patch);

   //---- P R I N T   D A T A ------ 
    if (switchDebug_PressFC) {
      printData_FC( patch,0,"press_FC",   "press_FC_RIGHT", pressX_FC);
      printData_FC( patch,0,"press_FC",   "press_FC_TOP",   pressY_FC);
      printData_FC( patch,0,"press_FC",   "press_FC_FRONT", pressZ_FC);
    }
  }  // patch loop
}


/* ---------------------------------------------------------------------
 Function~  ICE::massExchange--
 ---------------------------------------------------------------------  */
void ICE::massExchange(const ProcessorGroup*,  
			  const PatchSubset* patches,
                       const MaterialSubset* /*matls*/,
			  DataWarehouse* /*old_dw*/,
			  DataWarehouse* new_dw)
{
  for(int p=0;p<patches->size();p++){
    const Patch* patch = patches->get(p);
  #ifdef DOING
    cout << "Doing massExchange on patch " <<
      patch->getID() << "\t\t\t\t ICE" << endl;
  #endif

   Vector dx        = patch->dCell();
   double vol       = dx.x()*dx.y()*dx.z();

   int numMatls   =d_sharedState->getNumMatls();
   int numICEMatls=d_sharedState->getNumICEMatls();
   StaticArray<CCVariable<double> > burnedMass(numMatls);
   StaticArray<CCVariable<double> > releasedHeat(numMatls);
   StaticArray<CCVariable<double> > rho_CC(numMatls);
   
   int reactant_indx = -1;

    for(int m = 0; m < numMatls; m++) {
      Material* matl = d_sharedState->getMaterial( m );

      // Look for the reactant material
      if (matl->getRxProduct() == Material::reactant)
	reactant_indx = matl->getDWIndex();

      int indx = matl->getDWIndex();
      new_dw->get(rho_CC[m], lb->rho_CCLabel, indx,patch,Ghost::None, 0);
      new_dw->allocate(burnedMass[m],  lb->burnedMass_CCLabel,  indx,patch);
      new_dw->allocate(releasedHeat[m],lb->releasedHeat_CCLabel,indx,patch);
      burnedMass[m].initialize(0.0);
      releasedHeat[m].initialize(0.0); 
    }
    //__________________________________
    // Do the exchange if there is a reactant (reactant_indx >= 0)
    // and the switch is on.
    if(d_massExchange && (reactant_indx >= 0)){       
      for(CellIterator iter = patch->getCellIterator(); !iter.done(); iter++){

        double mass_hmx = rho_CC[reactant_indx][*iter] * vol;
        if (mass_hmx > d_SMALL_NUM)  {
           double burnedMass_tmp = (rho_CC[reactant_indx][*iter] * vol);  
           // hardwired wipes out all the mass in one 
          // timestep
           burnedMass[reactant_indx][*iter] =  -burnedMass_tmp;
        }
      }
      //__________________________________
      // Find the ICE matl which is the products of reaction
      // dump all the mass into that matl.
      for(int prods = 0; prods < numICEMatls; prods++) {
        ICEMaterial* ice_matl = d_sharedState->getICEMaterial(prods);
	if (ice_matl->getRxProduct() == Material::product) {
	  for(int m = 0; m < numICEMatls; m++) {
	    for(CellIterator iter=patch->getCellIterator();
		!iter.done();iter++){
	      burnedMass[prods][*iter]  -= burnedMass[m][*iter];
	    }
	  }
	}
      }    
    }
    //__________________________________
    // if there is no mass exchange carry forward
    for(int m = 0; m < numMatls; m++) {
      Material* matl = d_sharedState->getMaterial( m );
      int indx = matl->getDWIndex();
      new_dw->put(burnedMass[m], lb->burnedMass_CCLabel, indx,patch);
      new_dw->put(releasedHeat[m], lb->releasedHeat_CCLabel, indx,patch);
    }
    //---- P R I N T   D A T A ------ 
    for(int m = 0; m < numMatls; m++) {
    #if 0    // turn off for quality control tests
      if (switchDebugSource_Sink) {
        Material* matl = d_sharedState->getMaterial( m );
        int indx = matl->getDWIndex();
        char description[50];
        sprintf(description, "sources/sinks_Mat_%d",indx);
        printData( patch, 0, description, "burnedMass", burnedMass[m]);
        printData( patch, 0, description, "releasedHeat", releasedHeat[m]);
      }
    #endif
    }
  }   // patch loop
}
 

/* ---------------------------------------------------------------------
 Function~  ICE::accumulateMomentumSourceSinks--
 Purpose~   This function accumulates all of the sources/sinks of momentum
 ---------------------------------------------------------------------  */
void ICE::accumulateMomentumSourceSinks(const ProcessorGroup*,  
					const PatchSubset* patches,
                                        const MaterialSubset* /*matls*/,
					DataWarehouse* old_dw, 
					DataWarehouse* new_dw)
{
  for(int p=0;p<patches->size();p++){
    const Patch* patch = patches->get(p);
  #ifdef DOING
    cout << "Doing accumulate_momentum_source_sinks_MM on patch " <<
      patch->getID() << "\t ICE" << endl;
  #endif
    int indx;
    int numMatls  = d_sharedState->getNumMatls();

    IntVector right, left, top, bottom, front, back;
    int rightFace = 1, topFace = 1, frontFace = 1;
    int leftFace = 0, bottomFace = 0, backFace = 0;
    delt_vartype delT; 
    Vector dx, gravity;
    double pressure_source, mass, vol;
    double viscous_source;
    double viscosity;

    old_dw->get(delT, d_sharedState->get_delt_label());
    dx      = patch->dCell();
    double delX = dx.x();
    double delY = dx.y();
    double delZ = dx.z();
    gravity = d_sharedState->getGravity();
    vol     = delX * delY * delZ;

    StaticArray<double>  tau_XX(2), tau_XY(2), tau_XZ(2);
    StaticArray<double>  tau_YX(2), tau_YY(2), tau_YZ(2);
    StaticArray<double>  tau_ZX(2), tau_ZY(2), tau_ZZ(2);
    CCVariable<double>   rho_CC;
    CCVariable<Vector>   vel_CC;
    CCVariable<double>   vol_frac;
    SFCXVariable<double> pressX_FC;
    SFCYVariable<double> pressY_FC;
    SFCZVariable<double> pressZ_FC;

    new_dw->get(pressX_FC,lb->pressX_FCLabel, 0, patch,Ghost::AroundCells, 1);
    new_dw->get(pressY_FC,lb->pressY_FCLabel, 0, patch,Ghost::AroundCells, 1);
    new_dw->get(pressZ_FC,lb->pressZ_FCLabel, 0, patch,Ghost::AroundCells, 1);

    for(int m = 0; m < numMatls; m++) {
      Material* matl        = d_sharedState->getMaterial( m );
      ICEMaterial* ice_matl = dynamic_cast<ICEMaterial*>(matl);
      indx = matl->getDWIndex();
      viscosity = 0.0;
      if(ice_matl){
       old_dw->get(vel_CC,  lb->vel_CCLabel,     indx,patch,Ghost::None, 0);
       viscosity = ice_matl->getViscosity();
      }
      new_dw->get(rho_CC,  lb->rho_CCLabel,      indx,patch,Ghost::None, 0);
      new_dw->get(vol_frac,lb->vol_frac_CCLabel, indx,patch,Ghost::None, 0);

      CCVariable<Vector>   mom_source;
      new_dw->allocate(mom_source,  lb->mom_source_CCLabel,  indx, patch);
      mom_source.initialize(Vector(0.,0.,0.));

      for(CellIterator iter = patch->getCellIterator(); !iter.done(); iter++) {
        mass = rho_CC[*iter] * vol;
        right    = *iter + IntVector(1,0,0);
        left     = *iter + IntVector(0,0,0);
        top      = *iter + IntVector(0,1,0);
        bottom   = *iter + IntVector(0,0,0);
        front    = *iter + IntVector(0,0,1);
        back     = *iter + IntVector(0,0,0);

        //__________________________________
        // Compute the shear stresses on cell face
        // I HATE THIS BUT I DON'T KNOW HOW ELSE TO DO
        // IT WITHOUT BIG ARRAYS. I'M OPEN TO SUGGESTIONS -todd
        tau_XX[0] = 0.0; tau_XX[1] = 0.0;   tau_YX[0] = 0.0; tau_YX[1] = 0.0;
        tau_XY[0] = 0.0; tau_XY[1] = 0.0;   tau_YY[0] = 0.0; tau_YY[1] = 0.0;
        tau_XZ[0] = 0.0; tau_XZ[1] = 0.0;   tau_YZ[0] = 0.0; tau_YZ[1] = 0.0;
        
        tau_ZX[0] = 0.0; tau_ZX[1] = 0.0;
        tau_ZY[0] = 0.0; tau_ZY[1] = 0.0;
        tau_ZZ[0] = 0.0; tau_ZZ[1] = 0.0;
        if(ice_matl && viscosity >0.0){
            computeTauX_LeftandRightCellFace(vel_CC, *iter, viscosity, dx,
                                             tau_XX, tau_XY, tau_XZ);

            computeTauY_BottomandTopCellFace(vel_CC, *iter, viscosity, dx,
                                             tau_YX, tau_YY, tau_YZ);

            computeTauZ_BackandFrontCellFace(vel_CC, *iter, viscosity, dx,
                                             tau_ZX, tau_ZY, tau_ZZ);
        }
        //__________________________________
        //    X - M O M E N T U M 
        pressure_source = (pressX_FC[right]-pressX_FC[left]) * vol_frac[*iter];

        viscous_source = tau_XX[rightFace] - tau_XX[leftFace]   * delY * delZ + 
                         tau_YX[topFace]   - tau_YX[bottomFace] * delX * delZ +
                         tau_ZX[frontFace] - tau_ZX[backFace]   * delX * delY;     
                                 
        mom_source[*iter].x( (-pressure_source * delY * delZ +
                               viscous_source +
			          mass * gravity.x()) * delT );
        //__________________________________
        //    Y - M O M E N T U M
         pressure_source = (pressY_FC[top]-pressY_FC[bottom])* vol_frac[*iter];

        viscous_source = tau_XY[rightFace] - tau_XY[leftFace]   * delY * delZ + 
                         tau_YY[topFace]   - tau_YY[bottomFace] * delX * delZ +
                         tau_ZY[frontFace] - tau_ZY[backFace]   * delX * delY;

        mom_source[*iter].y( (-pressure_source * delX * delZ +
                               viscous_source +
			          mass * gravity.y()) * delT );
        //__________________________________
        //    Z - M O M E N T U M
        pressure_source = (pressZ_FC[front]-pressZ_FC[back]) * vol_frac[*iter];
        
        viscous_source = tau_XZ[rightFace] - tau_XZ[leftFace]   * delY * delZ + 
                         tau_YZ[topFace]   - tau_YZ[bottomFace] * delX * delZ +
                         tau_ZZ[frontFace] - tau_ZZ[backFace]   * delX * delY;

        mom_source[*iter].z( (-pressure_source * delX * delY +
			          viscous_source + 
                               mass * gravity.z()) * delT );
                               
      }
      new_dw->put(mom_source, lb->mom_source_CCLabel, indx, patch);

      //---- P R I N T   D A T A ------ 
      if (switchDebugSource_Sink) {
        char description[50];
        sprintf(description, "sources/sinks_Mat_%d",indx);
        printVector( patch, 1, description,    "xmom_source", 0, mom_source);
        printVector( patch, 1, description,    "ymom_source", 1, mom_source);
        printVector( patch, 1, description,    "zmom_source", 2, mom_source);
      }
    }
  }
}

/* --------------------------------------------------------------------- 
 Function~  ICE::accumulateEnergySourceSinks--
 Purpose~   This function accumulates all of the sources/sinks of energy 
 Currently the kinetic energy isn't included.
 ---------------------------------------------------------------------  */
void ICE::accumulateEnergySourceSinks(const ProcessorGroup*,  
				      const PatchSubset* patches,
                                  const MaterialSubset* /*matls*/,
				      DataWarehouse* old_dw, 
				      DataWarehouse* new_dw)
{
#ifdef ANNULUSICE
  static int n_iter;
  n_iter ++;
#endif

  for(int p=0;p<patches->size();p++){
    const Patch* patch = patches->get(p);
  #ifdef DOING
    cout << "Doing accumulate_energy_source_sinks on patch " 
         << patch->getID() << "\t\t ICE" << endl;
  #endif
    int numMatls = d_sharedState->getNumMatls();

    delt_vartype delT;
    old_dw->get(delT, d_sharedState->get_delt_label());
    Vector dx = patch->dCell();
    double A, B, vol=dx.x()*dx.y()*dx.z();

    CCVariable<double> rho_micro_CC;
    CCVariable<double> speedSound;
    CCVariable<double> vol_frac;
    CCVariable<double> press_CC;
    CCVariable<double> delPress;

    new_dw->get(press_CC,lb->press_CCLabel,    0, patch,Ghost::None, 0);
    new_dw->get(delPress,lb->delPress_CCLabel, 0, patch,Ghost::None, 0);

    for(int m = 0; m < numMatls; m++) {
      Material* matl = d_sharedState->getMaterial( m );
      int indx    = matl->getDWIndex();   
      CCVariable<double> int_eng_source;              
      new_dw->get(rho_micro_CC,lb->rho_micro_CCLabel, indx,patch,
		  Ghost::None, 0);                                            
      new_dw->get(speedSound,  lb->speedSound_CCLabel,indx,patch,
		  Ghost::None, 0); 
      new_dw->get(vol_frac,    lb->vol_frac_CCLabel,  indx,patch,
		  Ghost::None, 0);

#ifdef ANNULUSICE
      CCVariable<double> rho_CC;
      new_dw->get(rho_CC,      lb->rho_CCLabel,       indx,patch,
		  Ghost::None, 0);                                            
#endif
      new_dw->allocate(int_eng_source, lb->int_eng_source_CCLabel,indx,patch);

      //__________________________________
      //   Compute int_eng_source 
      int_eng_source.initialize(0.);
      for(CellIterator iter = patch->getCellIterator(); !iter.done(); iter++){
        A = vol * vol_frac[*iter] * press_CC[*iter];
        B = rho_micro_CC[*iter]   * speedSound[*iter] * speedSound[*iter];
        int_eng_source[*iter] = (A/B) * delPress[*iter];
      }

#ifdef ANNULUSICE
      if(n_iter <= 1.e10){
        if(m==2){
          for(CellIterator iter = patch->getCellIterator();!iter.done();iter++){
            int_eng_source[*iter] += 1.e10 * delT * rho_CC[*iter] * vol;
          }
        }
      }
#endif

      //---- P R I N T   D A T A ------ 
      if (switchDebugSource_Sink) {
        char description[50];
        sprintf(description, "sources/sinks_Mat_%d ",indx);
        printData( patch, 1, description, "int_eng_source", int_eng_source);
      }

      new_dw->put(int_eng_source,lb->int_eng_source_CCLabel,indx,patch);
    }
  }  // patch loop
}

/* ---------------------------------------------------------------------
 Function~  ICE::computeLagrangianValues--
 Computes lagrangian mass momentum and energy
 Note:      Only loop over ICE materials, mom_L for MPM is computed
            prior to this function
 ---------------------------------------------------------------------  */
void ICE::computeLagrangianValues(const ProcessorGroup*,  
				      const PatchSubset* patches,
                                  const MaterialSubset* /*matls*/,
				      DataWarehouse* old_dw, 
				      DataWarehouse* new_dw)
{
  for(int p=0;p<patches->size();p++){
    const Patch* patch = patches->get(p);
  #ifdef DOING
    cout << "Doing Lagrangian mass, momentum and energy on patch " <<
      patch->getID() << "\t ICE" << endl;
  #endif

    int numALLMatls = d_sharedState->getNumMatls();
    Vector  dx = patch->dCell();
    double vol = dx.x()*dx.y()*dx.z();    
    
    //__________________________________ 
    //  Compute the Lagrangian quantities
    for(int m = 0; m < numALLMatls; m++) {
     Material* matl = d_sharedState->getMaterial( m );
     int indx = matl->getDWIndex();
     ICEMaterial* ice_matl = dynamic_cast<ICEMaterial*>(matl);
     MPMMaterial* mpm_matl = dynamic_cast<MPMMaterial*>(matl);
     CCVariable<Vector> mom_L; 
     CCVariable<double> int_eng_L; 
     CCVariable<double> mass_L;
     if(ice_matl)  {               //  I C E
      CCVariable<double> rho_CC, temp_CC;
      CCVariable<Vector> vel_CC;
      CCVariable<double> int_eng_source, burnedMass;
      CCVariable<double> releasedHeat;
      CCVariable<Vector> mom_source;

      new_dw->get(rho_CC,  lb->rho_CCLabel,     indx,patch,Ghost::None, 0);
      old_dw->get(vel_CC,  lb->vel_CCLabel,     indx,patch,Ghost::None, 0);
      old_dw->get(temp_CC, lb->temp_CCLabel,    indx,patch,Ghost::None, 0);
      new_dw->get(burnedMass,     lb->burnedMass_CCLabel,indx,patch,
		                                             Ghost::None, 0);
      new_dw->get(releasedHeat,   lb->releasedHeat_CCLabel,indx,patch,
		                                             Ghost::None, 0);
      new_dw->get(mom_source,     lb->mom_source_CCLabel,indx,patch,
		                                             Ghost::None, 0);
      new_dw->get(int_eng_source, lb->int_eng_source_CCLabel,indx,patch,
		                                             Ghost::None, 0);
      new_dw->allocate(mom_L,     lb->mom_L_CCLabel,     indx,patch);
      new_dw->allocate(int_eng_L, lb->int_eng_L_CCLabel, indx,patch);
      new_dw->allocate(mass_L,    lb->mass_L_CCLabel,    indx,patch);
      double cv = ice_matl->getSpecificHeat();
      //__________________________________
      //  NO mass exchange
      if(d_massExchange == false) {
        for(CellIterator iter = patch->getExtraCellIterator(); !iter.done(); 
	    iter++) {

          double mass = rho_CC[*iter] * vol;
          mass_L[*iter] = mass;

          mom_L[*iter] = vel_CC[*iter] * mass + mom_source[*iter];

          int_eng_L[*iter] = mass*cv * temp_CC[*iter] + int_eng_source[*iter];
        }
      }
      //__________________________________
      //  WITH mass exchange
      // Note that the mass exchange can't completely
      // eliminate all the mass, momentum and internal E
      // If it does then we'll get erroneous vel, and temps
      // after advection.  Thus there is always a mininum amount
      if(d_massExchange)  {
        double massGain = 0.;
        for(CellIterator iter = patch->getExtraCellIterator(); !iter.done(); 
	    iter++) {
           //  must have a minimum mass
          double mass = rho_CC[*iter] * vol;
          double min_mass = d_SMALL_NUM * vol;

          mass_L[*iter] = std::max( (mass + burnedMass[*iter] ), min_mass);

          massGain += burnedMass[*iter];

          //  must have a minimum momentum                            
          Vector min_mom_L = vel_CC[*iter] * min_mass;
          Vector mom_L_tmp = vel_CC[*iter] * mass + 
                               vel_CC[*iter] * burnedMass[*iter];
                               
           // find the max between mom_L_tmp and min_mom_L
           // but keep the sign of the mom_L_tmp     
           // You need the d_SMALL_NUMs to avoid nans when mom_L_temp = 0.0           
          double plus_minus_one_x = (mom_L_tmp.x()+d_SMALL_NUM)/
                                    (fabs(mom_L_tmp.x()+d_SMALL_NUM));
          double plus_minus_one_y = (mom_L_tmp.y()+d_SMALL_NUM)/
                                    (fabs(mom_L_tmp.y()+d_SMALL_NUM));
          double plus_minus_one_z = (mom_L_tmp.z()+d_SMALL_NUM)/
                                    (fabs(mom_L_tmp.z()+d_SMALL_NUM));
          
          double mom_L_x = std::max( fabs(mom_L_tmp.x()), min_mom_L.x() );
          double mom_L_y = std::max( fabs(mom_L_tmp.y()), min_mom_L.y() );
          double mom_L_z = std::max( fabs(mom_L_tmp.z()), min_mom_L.z() );
          
          mom_L_x = plus_minus_one_x * mom_L_x;
          mom_L_y = plus_minus_one_y * mom_L_y;
          mom_L_z = plus_minus_one_z * mom_L_z;
 
          mom_L[*iter] = Vector(mom_L_x,mom_L_y,mom_L_z) + mom_source[*iter];

          // must have a minimum int_eng   
          double min_int_eng = min_mass * cv * temp_CC[*iter];
          double int_eng_tmp = mass * cv * temp_CC[*iter] + 
                               cv * temp_CC[*iter] * burnedMass[*iter];

          int_eng_L[*iter] = std::max(int_eng_tmp, min_int_eng) + 
                             int_eng_source[*iter] + releasedHeat[*iter];     
         }
	cout << "Mass gained by the gas this timestep = " << massGain << endl;
       }  // 
     }  // if (ice_matl)
      //---- P R I N T   D A T A ------ 
      // Dump out all the matls data
      if (switchDebugLagrangianValues ) {
        if(mpm_matl) {
          new_dw->get(int_eng_L,lb->int_eng_L_CCLabel,indx,patch,Ghost::None,0);
          new_dw->get(mom_L,    lb->mom_L_CCLabel,    indx,patch,Ghost::None,0);
        }
        char description[50];
        sprintf(description, "Bot_Lagrangian_Values_Mat_%d ",indx);
        printVector( patch,1, description, "xmom_L_CC", 0, mom_L);
        printVector( patch,1, description, "ymom_L_CC", 1, mom_L);
        printVector( patch,1, description, "zmom_L_CC", 2, mom_L);
        printData(   patch,1, description, "int_eng_L_CC",int_eng_L); 
           
      }
      
      if(ice_matl)  {
        new_dw->put(mom_L,     lb->mom_L_CCLabel,     indx,patch);
        new_dw->put(int_eng_L, lb->int_eng_L_CCLabel, indx,patch);
        new_dw->put(mass_L,    lb->mass_L_CCLabel,    indx,patch);
      }
    }  // end numALLMatl loop
  }  // patch loop
}

/*---------------------------------------------------------------------
 Function~  ICE::addExchangeToMomentumAndEnergy--
 Purpose~
   This function adds the energy exchange contribution to the 
   existing cell-centered lagrangian temperature

 Prerequisites:
            The face centered velocity for each material without
            the exchange must be solved prior to this routine.
            
                   (A)                              (X)
| (1+b12 + b13)     -b12          -b23          |   |del_data_CC[1]  |    
|                                               |   |                |    
| -b21              (1+b21 + b23) -b32          |   |del_data_CC[2]  |    
|                                               |   |                | 
| -b31              -b32          (1+b31 + b32) |   |del_data_CC[2]  |

                        =
                        
                        (B)
| b12( data_CC[2] - data_CC[1] ) + b13 ( data_CC[3] -data_CC[1])    | 
|                                                                   |
| b21( data_CC[1] - data_CC[2] ) + b23 ( data_CC[3] -data_CC[2])    | 
|                                                                   |
| b31( data_CC[1] - data_CC[3] ) + b32 ( data_CC[2] -data_CC[3])    | 
 
 - set *_L_ME arrays = *_L arrays
 - convert flux variables
 Steps for each cell;
    1) Comute the beta coefficients
    2) Form and A matrix and B vector
    3) Solve for del_data_CC[*]
    4) Add del_data_CC[*] to the appropriate Lagrangian data
 - apply Boundary conditions to vel_CC and Temp_CC
 - Stuff fluxes mom_L_ME and int_eng_L_ME back into dw
 
 References: see "A Cell-Centered ICE method for multiphase flow simulations"
 by Kashiwa, above equation 4.13.
 ---------------------------------------------------------------------  */
void ICE::addExchangeToMomentumAndEnergy(const ProcessorGroup*,  
                                         const PatchSubset* patches,
                                         const MaterialSubset* /*matls*/,
                                         DataWarehouse* old_dw, 
                                         DataWarehouse* new_dw)
{
  for(int p=0;p<patches->size();p++){
    const Patch* patch = patches->get(p);
  #ifdef DOING
    cout << "Doing Heat and momentum exchange on patch " << 
      patch->getID() << "\t\t ICE" << endl;
  #endif
    int     numMatls  = d_sharedState->getNumICEMatls();
    double  tmp;
    delt_vartype delT;
    old_dw->get(delT, d_sharedState->get_delt_label());

    StaticArray<CCVariable<double> > mass_L(numMatls);
    StaticArray<CCVariable<double> > Temp_CC(numMatls);
    StaticArray<CCVariable<double> > int_eng_L(numMatls);
    StaticArray<CCVariable<double> > vol_frac_CC(numMatls);
    StaticArray<CCVariable<double> > rho_micro_CC(numMatls);
    StaticArray<CCVariable<double> > int_eng_L_ME(numMatls);
    StaticArray<CCVariable<Vector> > mom_L(numMatls);
    StaticArray<CCVariable<Vector> > vel_CC(numMatls);
    StaticArray<CCVariable<Vector> > mom_L_ME(numMatls);

    vector<double> b(numMatls);
    vector<double> cv(numMatls);
    DenseMatrix beta(numMatls,numMatls),acopy(numMatls,numMatls);
    DenseMatrix K(numMatls,numMatls),H(numMatls,numMatls),a(numMatls,numMatls);
    beta.zero();
    acopy.zero();
    K.zero();
    H.zero();
    a.zero();
    getExchangeCoefficients(K, H);

    for(int m = 0; m < numMatls; m++)  {
      ICEMaterial* matl = d_sharedState->getICEMaterial( m );
      int indx = matl->getDWIndex();
      new_dw->get(mass_L[m],   lb->mass_L_CCLabel, indx,patch,Ghost::None,0);
      new_dw->get(mom_L[m],    lb->mom_L_CCLabel,  indx,patch,Ghost::None,0);
      new_dw->get(int_eng_L[m],lb->int_eng_L_CCLabel, indx,patch,
		  Ghost::None,0);
      new_dw->get(vol_frac_CC[m], lb->vol_frac_CCLabel,indx,patch, 
		  Ghost::None,0);
      new_dw->get(rho_micro_CC[m],lb->rho_micro_CCLabel,indx,patch,
		  Ghost::None,0);           

      new_dw->allocate( mom_L_ME[m],   lb->mom_L_ME_CCLabel,    indx, patch);
      new_dw->allocate(int_eng_L_ME[m],lb->int_eng_L_ME_CCLabel,indx, patch);
      new_dw->allocate( vel_CC[m],     lb->vel_CCLabel,         indx, patch);
      new_dw->allocate( Temp_CC[m],    lb->temp_CCLabel,        indx, patch);
      cv[m] = matl->getSpecificHeat();
    }  

    //__________________________________
    // Convert vars. flux -> primitive 
    for(CellIterator iter = patch->getCellIterator(); !iter.done(); iter++){
      for (int m = 0; m < numMatls; m++) {
        Temp_CC[m][*iter] = int_eng_L[m][*iter]/(mass_L[m][*iter]*cv[m]);
        vel_CC[m][*iter]  =  mom_L[m][*iter]/mass_L[m][*iter];
      }  
    }

    for(CellIterator iter = patch->getCellIterator(); !iter.done(); iter++){
      //   Form BETA matrix (a), off diagonal terms
      //  The beta and (a) matrix is common to all momentum exchanges
      for(int m = 0; m < numMatls; m++)  {
        tmp    = rho_micro_CC[m][*iter];
        for(int n = 0; n < numMatls; n++) {
	  beta[m][n] = delT * vol_frac_CC[n][*iter] * K[n][m]/tmp;
	  a[m][n] = -beta[m][n];
        }
      }

      //   Form matrix (a) diagonal terms
      for(int m = 0; m < numMatls; m++) {
        a[m][m] = 1.0;
        for(int n = 0; n < numMatls; n++) {
	  a[m][m] +=  beta[m][n];
        }
      }
      //---------- X - M O M E N T U M
      // -  F O R M   R H S   (b)
      // -  push a copy of (a) into the solver
      // -  Add exchange contribution to orig value
      for(int m = 0; m < numMatls; m++) {
        b[m] = 0.0;

        for(int n = 0; n < numMatls; n++) {
	  b[m] += beta[m][n] *
	    (vel_CC[n][*iter].x() - vel_CC[m][*iter].x());
        }
      }
      acopy = a;
      acopy.solve(b);

      for(int m = 0; m < numMatls; m++) {
          vel_CC[m][*iter].x( vel_CC[m][*iter].x() + b[m] );
      }

      //---------- Y - M O M E N T U M
      // -  F O R M   R H S   (b)
      // -  push a copy of (a) into the solver
      // -  Add exchange contribution to orig value
      for(int m = 0; m < numMatls; m++) {
        b[m] = 0.0;

        for(int n = 0; n < numMatls; n++) {
	  b[m] += beta[m][n] *
	    (vel_CC[n][*iter].y() - vel_CC[m][*iter].y());
        }
      }
      acopy    = a;
      acopy.solve(b);

      for(int m = 0; m < numMatls; m++)   {
          vel_CC[m][*iter].y( vel_CC[m][*iter].y() + b[m] );
      }

      //---------- Z - M O M E N T U M
      // -  F O R M   R H S   (b)
      // -  push a copy of (a) into the solver
      // -  Adde exchange contribution to orig value
      for(int m = 0; m < numMatls; m++)  {
        b[m] = 0.0;

        for(int n = 0; n < numMatls; n++) {
	  b[m] += beta[m][n] *
	    (vel_CC[n][*iter].z() - vel_CC[m][*iter].z());
        }
      }    
      acopy    = a;
      acopy.solve(b);

      for(int m = 0; m < numMatls; m++)  {
        vel_CC[m][*iter].z( vel_CC[m][*iter].z() + b[m] );
      }  
      //---------- E N E R G Y   E X C H A N G E
      //   Form BETA matrix (a) off diagonal terms
      for(int m = 0; m < numMatls; m++) {
        tmp = cv[m]*rho_micro_CC[m][*iter];
        for(int n = 0; n < numMatls; n++)  {
	  beta[m][n] = delT * vol_frac_CC[n][*iter] * H[n][m]/tmp;
	  a[m][n] = -beta[m][n];
        }
      }
      //   Form matrix (a) diagonal terms
      for(int m = 0; m < numMatls; m++) {
        a[m][m] = 1.;
        for(int n = 0; n < numMatls; n++)   {
	  a[m][m] +=  beta[m][n];
        }
      }
      // -  F O R M   R H S   (b)
      for(int m = 0; m < numMatls; m++)  {
        b[m] = 0.0;

       for(int n = 0; n < numMatls; n++) {
	  b[m] += beta[m][n] *
	    (Temp_CC[n][*iter] - Temp_CC[m][*iter]);
        }
      }
      //     S O L V E, Add exchange contribution to orig value
      a.solve(b);

      for(int m = 0; m < numMatls; m++) {
        Temp_CC[m][*iter] = Temp_CC[m][*iter] + b[m];
      }
    }
    //__________________________________
    //  Set the Boundary condiitions
    for (int m = 0; m < numMatls; m++)  {
      ICEMaterial* matl = d_sharedState->getICEMaterial( m );
      int indx = matl->getDWIndex();
      setBC(vel_CC[m],"Velocity",patch,indx);
      setBC(Temp_CC[m],"Temperature",patch,indx);
    }
    //__________________________________
    // Convert vars. primitive-> flux 
    for(CellIterator iter=patch->getExtraCellIterator(); !iter.done(); iter++){
      for (int m = 0; m < numMatls; m++) {
        int_eng_L_ME[m][*iter] = Temp_CC[m][*iter] * cv[m] * mass_L[m][*iter];
        mom_L_ME[m][*iter]     = vel_CC[m][*iter] * mass_L[m][*iter];
      }  
    } 

    for(int m = 0; m < numMatls; m++) {
      ICEMaterial* matl = d_sharedState->getICEMaterial( m );
      int indx = matl->getDWIndex();
      new_dw->put(mom_L_ME[m],    lb->mom_L_ME_CCLabel,    indx, patch);
      new_dw->put(int_eng_L_ME[m],lb->int_eng_L_ME_CCLabel,indx, patch);
    }
  }  // patch loop
}

/* --------------------------------------------------------------------- 
 Function~  ICE::advectAndAdvanceInTime--
 Purpose~
   This function calculates the The cell-centered, time n+1, mass, momentum
   and internal energy

   Need to include kinetic energy 
   Note, ICE doesn't need either sp_vol_CC or mass_CC, but MPMICE does.
 ---------------------------------------------------------------------  */
void ICE::advectAndAdvanceInTime(const ProcessorGroup*,  
                                 const PatchSubset* patches,
                                 const MaterialSubset* /*matls*/,
                                 DataWarehouse* old_dw,
                                 DataWarehouse* new_dw)
{
  for(int p=0;p<patches->size();p++){
    const Patch* patch = patches->get(p);
  #ifdef DOING
    cout << "Doing Advect and Advance in Time on patch " << 
      patch->getID() << "\t\t ICE" << endl;
  #endif
    delt_vartype delT;
    old_dw->get(delT, d_sharedState->get_delt_label());

    Vector dx = patch->dCell();
    double vol = dx.x()*dx.y()*dx.z(),mass;
    double invvol = 1.0/vol;

    int numALLmatls = d_sharedState->getNumMatls();
    int numICEmatls = d_sharedState->getNumICEMatls();

    // These arrays get re-used for each material, and for each
    // advected quantity
    CCVariable<double> q_CC, q_advected;
    const IntVector gc(1,1,1);
    CCVariable<Vector> qV_CC, qV_advected;
    CCVariable<fflux> OFS;
    CCVariable<eflux> OFE;
    CCVariable<cflux> OFC;

    new_dw->allocate(q_CC,       lb->q_CCLabel,       0, patch,gc);
    new_dw->allocate(q_advected, lb->q_advectedLabel, 0, patch);
    new_dw->allocate(qV_CC,      lb->qV_CCLabel,      0, patch,gc);
    new_dw->allocate(qV_advected,lb->qV_advectedLabel,0, patch);
    new_dw->allocate(OFS,        OFS_CCLabel,         0, patch,gc);
    new_dw->allocate(OFE,        OFE_CCLabel,         0, patch,gc);
    new_dw->allocate(OFC,        OFE_CCLabel,         0, patch,gc);

    for (int m = 0; m < d_sharedState->getNumICEMatls(); m++ ) {
      ICEMaterial* ice_matl = d_sharedState->getICEMaterial(m);
      int indx = ice_matl->getDWIndex();

      CCVariable<double> rho_CC, mass_CC, temp, sp_vol_CC, rho_micro;
      CCVariable<Vector> vel_CC, mom_L_ME;
      CCVariable<double > int_eng_L_ME, mass_L,speedSound;

      SFCXVariable<double > uvel_FC;
      SFCYVariable<double > vvel_FC;
      SFCZVariable<double > wvel_FC;

      new_dw->get(uvel_FC,lb->uvel_FCMELabel,indx,patch,Ghost::AroundCells,2);
      new_dw->get(vvel_FC,lb->vvel_FCMELabel,indx,patch,Ghost::AroundCells,2);
      new_dw->get(wvel_FC,lb->wvel_FCMELabel,indx,patch,Ghost::AroundCells,2);
      new_dw->get(mom_L_ME,  lb->mom_L_ME_CCLabel, indx,patch,
		  Ghost::AroundCells,1);
      new_dw->get(mass_L,lb->mass_L_CCLabel,indx,patch,Ghost::AroundCells,1);
      new_dw->get(rho_micro,
		  lb->rho_micro_CCLabel,indx,patch,Ghost::AroundCells,1);
      new_dw->get(int_eng_L_ME,lb->int_eng_L_ME_CCLabel,indx,patch,
		  Ghost::AroundCells,1);

      new_dw->allocate(rho_CC,    lb->rho_CC_top_cycleLabel,  indx,patch);
      new_dw->allocate(mass_CC,   lb->mass_CCLabel,           indx,patch);
      new_dw->allocate(sp_vol_CC, lb->sp_vol_CCLabel,         indx,patch);
      new_dw->allocate(temp,      lb->temp_CCLabel,           indx,patch);
      new_dw->allocate(vel_CC,    lb->vel_CCLabel,            indx,patch);

      double cv = ice_matl->getSpecificHeat();
      //__________________________________
      //   Advection preprocessing
      influxOutfluxVolume(uvel_FC,vvel_FC,wvel_FC,delT,patch,OFS,OFE,OFC);

      //__________________________________
      // Advect mass and backout mass_CC and rho_CC
      for(CellIterator iter=patch->getCellIterator(gc); !iter.done();iter++){
        q_CC[*iter] = mass_L[*iter] * invvol;
      }

      advectQFirst(q_CC, patch, OFS,OFE, OFC, q_advected);

      for(CellIterator iter = patch->getCellIterator(); !iter.done(); iter++) {
        rho_CC[*iter]  = (mass_L[*iter] + q_advected[*iter]) * invvol;
      }
      setBC(rho_CC,   "Density",              patch,indx);
      // mass_CC is needed for MPMICE
      for(CellIterator iter=patch->getExtraCellIterator();!iter.done();iter++){
        mass_CC[*iter] = rho_CC[*iter] * vol;
      }

      //__________________________________
      // Advect  momentum and backout vel_CC
      for(CellIterator iter=patch->getCellIterator(gc); !iter.done(); iter++){
        qV_CC[*iter] = mom_L_ME[*iter] * invvol;
      }

      advectQFirst(qV_CC, patch, OFS,OFE, OFC, qV_advected);

      for(CellIterator iter = patch->getCellIterator(); !iter.done();  iter++){
        mass = rho_CC[*iter] * vol;
        vel_CC[*iter] = (mom_L_ME[*iter] + qV_advected[*iter])/mass ;
      }
      setBC(vel_CC,   "Velocity",             patch,indx);

      //__________________________________
      // Advect internal energy and backout Temp_CC
      for(CellIterator iter=patch->getCellIterator(gc); !iter.done(); iter++){
        q_CC[*iter] = int_eng_L_ME[*iter] * invvol;
      }

      advectQFirst(q_CC, patch, OFS,OFE, OFC, q_advected);
      for(CellIterator iter = patch->getCellIterator(); !iter.done(); iter++){
        mass = rho_CC[*iter] * vol;
        temp[*iter] = (int_eng_L_ME[*iter] + q_advected[*iter])/(mass*cv);
      }
      setBC(temp,     "Temperature",          patch,indx);

      //__________________________________
      // Advection of specific volume.  Advected quantity is a volume fraction
      if (numICEmatls != numALLmatls)  {
        // I am doing this so that we get a reasonable answer for sp_vol
        // in the extra cells.  This calculation will get overwritten in
        // the interior cells.
        for(CellIterator iter=patch->getExtraCellIterator();!iter.done();
          iter++){
          sp_vol_CC[*iter] = 1.0/rho_micro[*iter];
        }
        for(CellIterator iter=patch->getCellIterator(gc); !iter.done();iter++){
          q_CC[*iter] = (mass_L[*iter]/rho_micro[*iter])*invvol;
        }

        advectQFirst(q_CC, patch, OFS,OFE, OFC, q_advected);

        for(CellIterator iter = patch->getCellIterator();!iter.done(); iter++){
          sp_vol_CC[*iter] = (q_CC[*iter] + q_advected[*iter]/vol);
        }
       // Divide by the new rho_CC.
       for(CellIterator iter=patch->getCellIterator();!iter.done();iter++){
	  sp_vol_CC[*iter] /= rho_CC[*iter];
       }
      }

      //---- P R I N T   D A T A ------   
      if (switchDebug_advance_advect ) {
      char description[50];
      sprintf(description, "AFTER_Advection_after_BC_Mat_%d ",indx);
      printVector( patch,1, description, "xmom_L_CC", 0, mom_L_ME);
      printVector( patch,1, description, "ymom_L_CC", 1, mom_L_ME);
      printVector( patch,1, description, "zmom_L_CC", 2, mom_L_ME);
      printData( patch,1,   description, "int_eng_L_CC",int_eng_L_ME);
      printData( patch,1,   description, "rho_CC",      rho_CC);
      printData( patch,1,   description, "Temp_CC",temp);
      printVector( patch,1, description, "uvel_CC", 0, vel_CC);
      printVector( patch,1, description, "vvel_CC", 1, vel_CC);
      printVector( patch,1, description, "wvel_CC", 2, vel_CC);
      }
      new_dw->put(rho_CC,   lb->rho_CC_top_cycleLabel, indx,patch);
      new_dw->put(mass_CC,  lb->mass_CCLabel,          indx,patch);
      new_dw->put(sp_vol_CC,lb->sp_vol_CCLabel,        indx,patch);
      new_dw->put(vel_CC,   lb->vel_CCLabel,           indx,patch);
      new_dw->put(temp,     lb->temp_CCLabel,          indx,patch);
    }
  }  // patch loop
}


/* --------------------------------------------------------------------- 
 Function~  ICE::setBC--
 Purpose~   Takes care Pressure_CC
 ---------------------------------------------------------------------  */
void ICE::setBC(CCVariable<double>& press_CC, 
                StaticArray<CCVariable<double> >& rho_micro_CC,
                StaticArray<CCVariable<double> >& rho_CC,
                StaticArray<CCVariable<double> >& vol_frac_CC,
                StaticArray<CCVariable<Vector> >& vel_CC,
                DataWarehouse* old_dw,
                const string& kind, 
                const Patch* patch, const int mat_id)
{
  
  Vector dx = patch->dCell();
  Vector gravity = d_sharedState->getGravity();
  
  for(Patch::FaceType face = Patch::startFace;
      face <= Patch::endFace && patch->getBCType(face)==Patch::None;
      face=Patch::nextFace(face)){
    BoundCondBase* bcs;
    BoundCond<double>* new_bcs;
    if (patch->getBCType(face) == Patch::None) {
      bcs = patch->getBCValues(mat_id,kind,face);
      new_bcs = dynamic_cast<BoundCond<double> *>(bcs);
    }
    else
      continue;

    if (new_bcs != 0) {
      if (new_bcs->getKind() == "Dirichlet") 
	press_CC.fillFace(face,new_bcs->getValue());
      //__________________________________
      //  This automatically takes care of the
      //  hydrostatic pressure gradient.
      if (new_bcs->getKind() == "Neumann") 
        backoutGCPressFromVelFC(patch, face, old_dw,
                              press_CC, rho_micro_CC, rho_CC,
                              vol_frac_CC,  vel_CC);
   
    }
  }
}
/*`==========TESTING==========*/ 
// KEEP THIS AROUND UNTIL
// I'M SURE OF THE NEW STYLE OF SETBC
// FOR PRESSURE -Todd
/* --------------------------------------------------------------------- 
 Function~  ICE::setBC--
 Purpose~   Takes care Pressure_CC
 ---------------------------------------------------------------------  */
void ICE::setBC(CCVariable<double>& press_CC, CCVariable<double>& rho_micro,
              const string& kind, const Patch* patch, const int mat_id)
{
  
  Vector dx = patch->dCell();
  Vector gravity = d_sharedState->getGravity();
  
  for(Patch::FaceType face = Patch::startFace;
      face <= Patch::endFace; face=Patch::nextFace(face)){
    BoundCondBase* bcs;
    BoundCond<double>* new_bcs;
    if (patch->getBCType(face) == Patch::None) {
      bcs = patch->getBCValues(mat_id,kind,face);
      new_bcs = dynamic_cast<BoundCond<double> *>(bcs);
    }
    else
      continue;

    if (new_bcs != 0) {
      if (new_bcs->getKind() == "Dirichlet") 
	press_CC.fillFace(face,new_bcs->getValue());

      if (new_bcs->getKind() == "Neumann") 
	press_CC.fillFaceFlux(face,new_bcs->getValue(),dx);
       
      if ( fabs(gravity.x()) > 0.0  || 
           fabs(gravity.y()) > 0.0  || 
	   fabs(gravity.z()) > 0.0) {
        press_CC.setHydrostaticPressureBC(face, gravity, rho_micro, dx);
      }
    }
  }
}
 /*==========TESTING==========`*/
/* --------------------------------------------------------------------- 
 Function~  ICE::setBC--
 Purpose~   Takes care of Density_CC and Temperature_CC
 ---------------------------------------------------------------------  */
void ICE::setBC(CCVariable<double>& variable, const string& kind, 
		const Patch* patch, const int mat_id)
{
#if 0
  cerr << "Setting BC " << kind << " for material " << mat_id << endl;
#endif
  Vector dx = patch->dCell();
  Vector grav = d_sharedState->getGravity();
  for(Patch::FaceType face = Patch::startFace;
      face <= Patch::endFace; face=Patch::nextFace(face)){
    BoundCondBase* bcs;
    BoundCond<double>* new_bcs;
    if (patch->getBCType(face) == Patch::None) {
      bcs = patch->getBCValues(mat_id,kind,face);
      new_bcs = dynamic_cast<BoundCond<double> *>(bcs);
    } else
      continue;
 
    if (new_bcs != 0) {
      //__________________________________
      //  Density_CC
      if (kind == "Density") {
        if (new_bcs->getKind() == "Dirichlet") 
	  variable.fillFace(face,new_bcs->getValue());

        if (new_bcs->getKind() == "Neumann") 
	  variable.fillFaceFlux(face,new_bcs->getValue(),dx);
      }
 
      //__________________________________
      // Temperature_CC
      if (kind == "Temperature" ){ 
        if (new_bcs->getKind() == "Dirichlet") 
	    variable.fillFace(face,new_bcs->getValue());
           
         // Neumann && gravity                 
        if (new_bcs->getKind() == "Neumann") {  
          variable.fillFaceFlux(face,new_bcs->getValue(),dx);
            
          if(fabs(grav.x()) >0.0 ||fabs(grav.y()) >0.0 ||fabs(grav.z()) >0.0) {
            Material *matl = d_sharedState->getMaterial(mat_id);
            ICEMaterial* ice_matl = dynamic_cast<ICEMaterial*>(matl);
            if(ice_matl) {
              double cv     = ice_matl->getSpecificHeat();
              double gamma  = ice_matl->getGamma();
                    
              ice_matl->getEOS()->hydrostaticTempAdjustment(face, 
                                  patch,  grav, gamma,
                                  cv,     dx,   variable);
            }  // if(ice_matl) 
          }  // if(gravity)
        }  // if(Neumann)
      }  //  if(Temperature)
    }  // if(new_bc)
  }  // Patch loop
}


/* --------------------------------------------------------------------- 
 Function~  ICE::setBC--        
 Purpose~   Takes care of Velocity_CC Boundary conditions
 ---------------------------------------------------------------------  */
void ICE::setBC(CCVariable<Vector>& variable, const string& kind, 
		const Patch* patch, const int mat_id) 
{
  IntVector  low, hi;
  Vector dx = patch->dCell();
  for(Patch::FaceType face = Patch::startFace; face <= Patch::endFace;
      face=Patch::nextFace(face)){
    BoundCondBase* bcs;
    BoundCond<Vector>* new_bcs;
    if (patch->getBCType(face) == Patch::None) {
      bcs = patch->getBCValues(mat_id,kind,face);
      new_bcs = dynamic_cast<BoundCond<Vector> *>(bcs);
    } else
      continue;

    if (new_bcs != 0) {
      if (new_bcs->getKind() == "Dirichlet") 
	variable.fillFace(face,new_bcs->getValue());
      
      if (new_bcs->getKind() == "Neumann") 
	variable.fillFaceFlux(face,new_bcs->getValue(),dx);
       
      if (new_bcs->getKind() == "NegInterior") {
         variable.fillFaceFlux(face,Vector(0.0,0.0,0.0),dx, -1.0);
      }  
    }  // end velocity loop
  }  // end face loop
}

/* --------------------------------------------------------------------- 
 Function~  ICE::setBC--      
 Purpose~   Takes care of vel_FC.x()
 ---------------------------------------------------------------------  */
void ICE::setBC(SFCXVariable<double>& variable, const  string& kind, 
		const string& comp, const Patch* patch, const int mat_id) 
{
  Vector dx = patch->dCell();
  for(Patch::FaceType face = Patch::startFace; face <= Patch::endFace;
      face=Patch::nextFace(face)){
    BoundCondBase* bcs;
    BoundCond<Vector>* new_bcs;
    if (patch->getBCType(face) == Patch::None) {
      bcs = patch->getBCValues(mat_id,kind,face);
    new_bcs = dynamic_cast<BoundCond<Vector> *>(bcs);
    } else
      continue;

    if (new_bcs != 0) {
      if (new_bcs->getKind() == "Dirichlet") {
	if (comp == "x")
	  variable.fillFace(face,new_bcs->getValue().x());
	if (comp == "y")
	  variable.fillFace(face,new_bcs->getValue().y());
	if (comp == "z")
	  variable.fillFace(face,new_bcs->getValue().z());
      }
      
      if (new_bcs->getKind() == "Neumann") {
	if (comp == "x")
	  variable.fillFaceFlux(face,new_bcs->getValue().x(),dx);
	if (comp == "y")
	  variable.fillFaceFlux(face,new_bcs->getValue().y(),dx);
	if (comp == "z")
	  variable.fillFaceFlux(face,new_bcs->getValue().z(),dx);
      }
    }
  }
}
/* --------------------------------------------------------------------- 
 Function~  ICE::setBC--      
 Purpose~   Takes care of vel_FC.y()
 ---------------------------------------------------------------------  */
void ICE::setBC(SFCYVariable<double>& variable, const  string& kind, 
		const string& comp, const Patch* patch, const int mat_id) 
{
  Vector dx = patch->dCell();
  for(Patch::FaceType face = Patch::startFace;
      face <= Patch::endFace; face=Patch::nextFace(face)){
    BoundCondBase* bcs;
    BoundCond<Vector>* new_bcs;
    if (patch->getBCType(face) == Patch::None) {
      bcs = patch->getBCValues(mat_id,kind,face);
      new_bcs = dynamic_cast<BoundCond<Vector> *>(bcs);
    } else
      continue;

    if (new_bcs != 0) {
      if (new_bcs->getKind() == "Dirichlet") {
	if (comp == "x")
	  variable.fillFace(face,new_bcs->getValue().x());
	if (comp == "y")
	  variable.fillFace(face,new_bcs->getValue().y());
	if (comp == "z")
	  variable.fillFace(face,new_bcs->getValue().z());
      }
      
      if (new_bcs->getKind() == "Neumann") {
	if (comp == "x")
	  variable.fillFaceFlux(face,new_bcs->getValue().x(),dx);
	if (comp == "y")
	  variable.fillFaceFlux(face,new_bcs->getValue().y(),dx);
	if (comp == "z")
	  variable.fillFaceFlux(face,new_bcs->getValue().z(),dx);
      }
    }
  }
}
/* --------------------------------------------------------------------- 
 Function~  ICE::setBC--      
 Purpose~   Takes care of vel_FC.z()
 ---------------------------------------------------------------------  */
void ICE::setBC(SFCZVariable<double>& variable, const  string& kind, 
		const string& comp, const Patch* patch, const int mat_id) 
{
  Vector dx = patch->dCell();
  for(Patch::FaceType face = Patch::startFace;
      face <= Patch::endFace; face=Patch::nextFace(face)){
    BoundCondBase* bcs;
    BoundCond<Vector>* new_bcs;
    if (patch->getBCType(face) == Patch::None) {
      bcs = patch->getBCValues(mat_id,kind,face);
      new_bcs = dynamic_cast<BoundCond<Vector> *>(bcs);
    } else
      continue;
    
    if (new_bcs != 0) {
      if (new_bcs->getKind() == "Dirichlet") {
	if (comp == "x")
	  variable.fillFace(face,new_bcs->getValue().x());
	if (comp == "y")
	  variable.fillFace(face,new_bcs->getValue().y());
	if (comp == "z")
	  variable.fillFace(face,new_bcs->getValue().z());
      }
      
      if (new_bcs->getKind() == "Neumann") {
	if (comp == "x")
	  variable.fillFaceFlux(face,new_bcs->getValue().x(),dx);
	if (comp == "y")
	  variable.fillFaceFlux(face,new_bcs->getValue().y(),dx);
	if (comp == "z")
	  variable.fillFaceFlux(face,new_bcs->getValue().z(),dx);
      }
    }
  }
}

/* ---------------------------------------------------------------------
 Function~  influxOutfluxVolume--
 Purpose~   calculate the individual outfluxes for each cell.
            This includes the slabs and edge fluxes
 References:
    "Compatible Fluxes for van Leer Advection" W.B VanderHeyden and 
    B.A. Kashiwa, Journal of Computational Physics, 146, 1-28, (1998) 
            
 Steps for each cell:  
 1) calculate the volume for each outflux
 3) set the influx_volume for the appropriate cell = to the q_outflux of the 
    adjacent cell. 

Implementation notes:
The outflux of volume is calculated in each cell in the computational domain
+ one layer of extra cells  surrounding the domain.The face-centered velocity 
needs to be defined on all faces for these cells 

See schematic diagram at bottom of ice.cc for del* definitions
 ---------------------------------------------------------------------  */
void ICE::influxOutfluxVolume(const SFCXVariable<double>&     uvel_FC,
			      const SFCYVariable<double>&     vvel_FC,
			      const SFCZVariable<double>&     wvel_FC,
			      const double&                   delT, 
			      const Patch*                    patch,
			      CCVariable<fflux>&              OFS, 
			      CCVariable<eflux>&              OFE,
			      CCVariable<cflux>&              OFC)

{
  Vector dx = patch->dCell();
  double vol = dx.x()*dx.y()*dx.z();
  double delY_top, delY_bottom,delX_right, delX_left, delZ_front, delZ_back;
  double delX_tmp, delY_tmp,   delZ_tmp;

  // Compute outfluxes 
  const IntVector gc(1,1,1);
  for(CellIterator iter = patch->getCellIterator(gc); !iter.done(); iter++){

    delY_top    = std::max(0.0, (vvel_FC[*iter+IntVector(0,1,0)] * delT));
    delY_bottom = std::max(0.0,-(vvel_FC[*iter+IntVector(0,0,0)] * delT));
    delX_right  = std::max(0.0, (uvel_FC[*iter+IntVector(1,0,0)] * delT));
    delX_left   = std::max(0.0,-(uvel_FC[*iter+IntVector(0,0,0)] * delT));
    delZ_front  = std::max(0.0, (wvel_FC[*iter+IntVector(0,0,1)] * delT));
    delZ_back   = std::max(0.0,-(wvel_FC[*iter+IntVector(0,0,0)] * delT));
    
    delX_tmp    = dx.x() - delX_right - delX_left;
    delY_tmp    = dx.y() - delY_top   - delY_bottom;
    delZ_tmp    = dx.z() - delZ_front - delZ_back;
    
    //__________________________________
    //   SLAB outfluxes
    OFS[*iter].d_fflux[TOP]    = delY_top     * delX_tmp * delZ_tmp;
    OFS[*iter].d_fflux[BOTTOM] = delY_bottom  * delX_tmp * delZ_tmp;
    OFS[*iter].d_fflux[RIGHT]  = delX_right   * delY_tmp * delZ_tmp;
    OFS[*iter].d_fflux[LEFT]   = delX_left    * delY_tmp * delZ_tmp;
    OFS[*iter].d_fflux[FRONT]  = delZ_front   * delX_tmp * delY_tmp;
    OFS[*iter].d_fflux[BACK]   = delZ_back    * delX_tmp * delY_tmp;
    //__________________________________
    // Edge flux terms
    OFE[*iter].d_eflux[TOP_R]     = delY_top      * delX_right * delZ_tmp;
    OFE[*iter].d_eflux[TOP_FR]    = delY_top      * delX_tmp   * delZ_front;
    OFE[*iter].d_eflux[TOP_L]     = delY_top      * delX_left  * delZ_tmp;
    OFE[*iter].d_eflux[TOP_BK]    = delY_top      * delX_tmp   * delZ_back;
    
    OFE[*iter].d_eflux[BOT_R]     = delY_bottom   * delX_right * delZ_tmp;
    OFE[*iter].d_eflux[BOT_FR]    = delY_bottom   * delX_tmp   * delZ_front;
    OFE[*iter].d_eflux[BOT_L]     = delY_bottom   * delX_left  * delZ_tmp;
    OFE[*iter].d_eflux[BOT_BK]    = delY_bottom   * delX_tmp   * delZ_back;
    
    OFE[*iter].d_eflux[RIGHT_BK]  = delY_tmp      * delX_right * delZ_back;
    OFE[*iter].d_eflux[RIGHT_FR]  = delY_tmp      * delX_right * delZ_front;
    
    OFE[*iter].d_eflux[LEFT_BK]   = delY_tmp      * delX_left  * delZ_back;
    OFE[*iter].d_eflux[LEFT_FR]   = delY_tmp      * delX_left  * delZ_front;
    
    //__________________________________
    //   Corner flux terms
    OFC[*iter].d_cflux[TOP_R_BK]  = delY_top      * delX_right * delZ_back;
    OFC[*iter].d_cflux[TOP_R_FR]  = delY_top      * delX_right * delZ_front;
    OFC[*iter].d_cflux[TOP_L_BK]  = delY_top      * delX_left  * delZ_back;
    OFC[*iter].d_cflux[TOP_L_FR]  = delY_top      * delX_left  * delZ_front;
    
    OFC[*iter].d_cflux[BOT_R_BK]  = delY_bottom   * delX_right * delZ_back;
    OFC[*iter].d_cflux[BOT_R_FR]  = delY_bottom   * delX_right * delZ_front;
    OFC[*iter].d_cflux[BOT_L_BK]  = delY_bottom   * delX_left  * delZ_back;
    OFC[*iter].d_cflux[BOT_L_FR]  = delY_bottom   * delX_left  * delZ_front;

    //__________________________________
    //  Bullet proofing
    double total_fluxout = 0.0;
    for(int face = TOP; face <= BACK; face++ )  {
      total_fluxout  += OFS[*iter].d_fflux[face];
    }
    for(int edge = TOP_R; edge <= LEFT_BK; edge++ )  {
      total_fluxout  += OFE[*iter].d_eflux[edge];
    }
    for(int corner = TOP_R_BK; corner <= BOT_L_FR; corner++ )  {
      total_fluxout  += OFC[*iter].d_cflux[corner];
    }
    
 // ASSERT(total_fluxout < vol);
    if (total_fluxout > vol) {
      IntVector curcell = *iter;
      int i,j,k;
      i   = curcell.x();
      j   = curcell.y();
      k   = curcell.z();

      char warning [100];
      sprintf(warning,
        " cell[%d][%d][%d], total_outflux (%5.4g)  > vol (%5.4g) .....Now exiting ",
        i,j,k,total_fluxout, vol);
      Message(1,"ICE::influxOutfluxVolume:",warning, "");
   }
  }
}
  

/* ---------------------------------------------------------------------
 Function~  ICE::advectQFirst--ADVECTION:
 Purpose~   Calculate the advection of q_CC 
   
 References:
    "Compatible Fluxes for van Leer Advection" W.B VanderHeyden and 
    B.A. Kashiwa, Journal of Computational Physics, 146, 1-28, (1998) 
            
 Steps for each cell:      
- Compute q outflux and q influx for each cell.
- Finally sum the influx and outflux portions
       
 advect_preprocessing MUST be done prior to this function
 ---------------------------------------------------------------------  */
void ICE::advectQFirst(const CCVariable<double>&   q_CC,const Patch* patch,
		       const CCVariable<fflux>&    OFS,
		       const CCVariable<eflux>&    OFE,
		       const CCVariable<cflux>&    OFC,
		       CCVariable<double>&         q_advected)
  
{
  double  sum_q_outflux, sum_q_outflux_EF, sum_q_outflux_CF, sum_q_influx;
  double sum_q_influx_EF, sum_q_influx_CF;
  IntVector adjcell;
  
  for(CellIterator iter = patch->getCellIterator(); !iter.done(); iter++) { 
    IntVector curcell = *iter;
    int i = curcell.x();
    int j = curcell.y();
    int k = curcell.z();
   
    sum_q_outflux       = 0.0;
    sum_q_outflux_EF    = 0.0;
    sum_q_outflux_CF    = 0.0;
    sum_q_influx        = 0.0;
    sum_q_influx_EF     = 0.0;
    sum_q_influx_CF     = 0.0;
    
    //__________________________________
    //  OUTFLUX: SLAB 
    for(int face = TOP; face <= BACK; face++ )  {
      sum_q_outflux  += q_CC[*iter] * OFS[*iter].d_fflux[face];
    }
    //__________________________________
    //  OUTFLUX: EDGE_FLUX
    for(int edge = TOP_R; edge <= LEFT_BK; edge++ )   {
      sum_q_outflux_EF += q_CC[*iter] * OFE[*iter].d_eflux[edge];
    }
    //__________________________________
    //  OUTFLUX: CORNER FLUX
    for(int corner = TOP_R_BK; corner <= BOT_L_FR; corner++ )  {
      sum_q_outflux_CF +=  q_CC[*iter] * OFC[*iter].d_cflux[corner];
    } 

    //__________________________________
    //  INFLUX: SLABS
    adjcell = IntVector(i, j+1, k);	// TOP
    sum_q_influx  += q_CC[adjcell] * OFS[adjcell].d_fflux[BOTTOM];
    adjcell = IntVector(i, j-1, k);	// BOTTOM
    sum_q_influx  += q_CC[adjcell] * OFS[adjcell].d_fflux[TOP];
    adjcell = IntVector(i+1, j, k);	// RIGHT
    sum_q_influx  += q_CC[adjcell] * OFS[adjcell].d_fflux[LEFT];
    adjcell = IntVector(i-1, j, k);	// LEFT
    sum_q_influx  += q_CC[adjcell] * OFS[adjcell].d_fflux[RIGHT];
    adjcell = IntVector(i, j, k+1);	// FRONT
    sum_q_influx  += q_CC[adjcell] * OFS[adjcell].d_fflux[BACK];
    adjcell = IntVector(i, j, k-1);	// BACK
    sum_q_influx  += q_CC[adjcell] * OFS[adjcell].d_fflux[FRONT];
    //__________________________________
    //  INFLUX: EDGES
    adjcell = IntVector(i+1, j+1, k);	// TOP_R
    sum_q_influx_EF += q_CC[adjcell] * OFE[adjcell].d_eflux[BOT_L];
    adjcell = IntVector(i, j+1, k+1);   // TOP_FR
    sum_q_influx_EF += q_CC[adjcell] * OFE[adjcell].d_eflux[BOT_BK];
    adjcell = IntVector(i-1, j+1, k);	// TOP_L
    sum_q_influx_EF += q_CC[adjcell] * OFE[adjcell].d_eflux[BOT_R];
    adjcell = IntVector(i, j+1, k-1);	// TOP_BK
    sum_q_influx_EF += q_CC[adjcell] * OFE[adjcell].d_eflux[BOT_FR];
    adjcell = IntVector(i+1, j-1, k);	// BOT_R
    sum_q_influx_EF += q_CC[adjcell] * OFE[adjcell].d_eflux[TOP_L];
    adjcell = IntVector(i, j-1, k+1);	// BOT_FR
    sum_q_influx_EF += q_CC[adjcell] * OFE[adjcell].d_eflux[TOP_BK];
    adjcell = IntVector(i-1, j-1, k);	// BOT_L
    sum_q_influx_EF += q_CC[adjcell] * OFE[adjcell].d_eflux[TOP_R];
    adjcell = IntVector(i, j-1, k-1);	// BOT_BK
    sum_q_influx_EF += q_CC[adjcell] * OFE[adjcell].d_eflux[TOP_FR];
    adjcell = IntVector(i+1, j, k-1);	// RIGHT_BK
    sum_q_influx_EF += q_CC[adjcell] * OFE[adjcell].d_eflux[LEFT_FR];
    adjcell = IntVector(i+1, j, k+1);	// RIGHT_FR
    sum_q_influx_EF += q_CC[adjcell] * OFE[adjcell].d_eflux[LEFT_BK];
    adjcell = IntVector(i-1, j, k-1);	// LEFT_BK
    sum_q_influx_EF += q_CC[adjcell] * OFE[adjcell].d_eflux[RIGHT_FR];
    adjcell = IntVector(i-1, j, k+1);	// LEFT_FR
    sum_q_influx_EF += q_CC[adjcell] * OFE[adjcell].d_eflux[RIGHT_BK];

    //__________________________________
    //   INFLUX: CORNER FLUX
    adjcell = IntVector(i+1, j+1, k-1);	// TOP_R_BK
    sum_q_influx_CF += q_CC[adjcell] * OFC[adjcell].d_cflux[BOT_L_FR];
    adjcell = IntVector(i+1, j+1, k+1);	// TOP_R_FR
    sum_q_influx_CF += q_CC[adjcell] * OFC[adjcell].d_cflux[BOT_L_BK];
    adjcell = IntVector(i-1, j+1, k-1);	// TOP_L_BK
    sum_q_influx_CF += q_CC[adjcell] * OFC[adjcell].d_cflux[BOT_R_FR];
    adjcell = IntVector(i-1, j+1, k+1);	// TOP_L_FR
    sum_q_influx_CF += q_CC[adjcell] * OFC[adjcell].d_cflux[BOT_R_BK];
    adjcell = IntVector(i+1, j-1, k-1);	// BOT_R_BK
    sum_q_influx_CF += q_CC[adjcell] * OFC[adjcell].d_cflux[TOP_L_FR];
    adjcell = IntVector(i+1, j-1, k+1);	// BOT_R_FR
    sum_q_influx_CF += q_CC[adjcell] * OFC[adjcell].d_cflux[TOP_L_BK];
    adjcell = IntVector(i-1, j-1, k-1); // BOT_L_BK
    sum_q_influx_CF += q_CC[adjcell] * OFC[adjcell].d_cflux[TOP_R_FR];
    adjcell = IntVector(i-1, j-1, k+1);	// BOT_L_FR
    sum_q_influx_CF += q_CC[adjcell] * OFC[adjcell].d_cflux[TOP_R_BK];

    //__________________________________
    //  Calculate the advected q at t + delta t
    q_advected[*iter] = - sum_q_outflux - sum_q_outflux_EF - sum_q_outflux_CF
                        + sum_q_influx  + sum_q_influx_EF  + sum_q_influx_CF;

  }

}

/* ---------------------------------------------------------------------
 Function~  ICE::advectQFirst-- Vector version*/
void ICE::advectQFirst(const CCVariable<Vector>&   q_CC,const Patch* patch,
		       const CCVariable<fflux>&    OFS,
		       const CCVariable<eflux>&    OFE,
		       const CCVariable<cflux>&    OFC,
		       CCVariable<Vector>&         q_advected)
  
{
  Vector  sum_q_outflux, sum_q_outflux_EF, sum_q_outflux_CF;
  Vector  sum_q_influx,  sum_q_influx_EF,  sum_q_influx_CF;

  IntVector adjcell;
  Vector zero(0.,0.,0.);
  
  for(CellIterator iter = patch->getCellIterator(); !iter.done(); iter++) { 
    IntVector curcell = *iter;
    int i = curcell.x();
    int j = curcell.y();
    int k = curcell.z();

    sum_q_outflux       = zero;
    sum_q_outflux_EF    = zero;
    sum_q_outflux_CF    = zero;
    sum_q_influx        = zero;
    sum_q_influx_EF     = zero;
    sum_q_influx_CF     = zero;
    
    //__________________________________
    //  OUTFLUX: SLAB 
    for(int face = TOP; face <= BACK; face++ )  {
      sum_q_outflux  += q_CC[*iter] * OFS[*iter].d_fflux[face];
    }
    //__________________________________
    //  OUTFLUX: EDGE_FLUX
    for(int edge = TOP_R; edge <= LEFT_BK; edge++ )   {
      sum_q_outflux_EF += q_CC[*iter] * OFE[*iter].d_eflux[edge];
    }
    //__________________________________
    //  OUTFLUX: CORNER FLUX
    for(int corner = TOP_R_BK; corner <= BOT_L_FR; corner++ )  {
      sum_q_outflux_CF +=  q_CC[*iter] * OFC[*iter].d_cflux[corner];
    } 


    //__________________________________
    //  INFLUX: SLABS
    adjcell = IntVector(i, j+1, k);	// TOP
    sum_q_influx  += q_CC[adjcell] * OFS[adjcell].d_fflux[BOTTOM];
    adjcell = IntVector(i, j-1, k);	// BOTTOM
    sum_q_influx  += q_CC[adjcell] * OFS[adjcell].d_fflux[TOP];
    adjcell = IntVector(i+1, j, k);	// RIGHT
    sum_q_influx  += q_CC[adjcell] * OFS[adjcell].d_fflux[LEFT];
    adjcell = IntVector(i-1, j, k);	// LEFT
    sum_q_influx  += q_CC[adjcell] * OFS[adjcell].d_fflux[RIGHT];
    adjcell = IntVector(i, j, k+1);	// FRONT
    sum_q_influx  += q_CC[adjcell] * OFS[adjcell].d_fflux[BACK];
    adjcell = IntVector(i, j, k-1);	// BACK
    sum_q_influx  += q_CC[adjcell] * OFS[adjcell].d_fflux[FRONT];
    //__________________________________
    //  INFLUX: EDGES
    adjcell = IntVector(i+1, j+1, k);	// TOP_R
    sum_q_influx_EF += q_CC[adjcell] * OFE[adjcell].d_eflux[BOT_L];
    adjcell = IntVector(i, j+1, k+1);   // TOP_FR
    sum_q_influx_EF += q_CC[adjcell] * OFE[adjcell].d_eflux[BOT_BK];
    adjcell = IntVector(i-1, j+1, k);	// TOP_L
    sum_q_influx_EF += q_CC[adjcell] * OFE[adjcell].d_eflux[BOT_R];
    adjcell = IntVector(i, j+1, k-1);	// TOP_BK
    sum_q_influx_EF += q_CC[adjcell] * OFE[adjcell].d_eflux[BOT_FR];
    adjcell = IntVector(i+1, j-1, k);	// BOT_R
    sum_q_influx_EF += q_CC[adjcell] * OFE[adjcell].d_eflux[TOP_L];
    adjcell = IntVector(i, j-1, k+1);	// BOT_FR
    sum_q_influx_EF += q_CC[adjcell] * OFE[adjcell].d_eflux[TOP_BK];
    adjcell = IntVector(i-1, j-1, k);	// BOT_L
    sum_q_influx_EF += q_CC[adjcell] * OFE[adjcell].d_eflux[TOP_R];
    adjcell = IntVector(i, j-1, k-1);	// BOT_BK
    sum_q_influx_EF += q_CC[adjcell] * OFE[adjcell].d_eflux[TOP_FR];
    adjcell = IntVector(i+1, j, k-1);	// RIGHT_BK
    sum_q_influx_EF += q_CC[adjcell] * OFE[adjcell].d_eflux[LEFT_FR];
    adjcell = IntVector(i+1, j, k+1);	// RIGHT_FR
    sum_q_influx_EF += q_CC[adjcell] * OFE[adjcell].d_eflux[LEFT_BK];
    adjcell = IntVector(i-1, j, k-1);	// LEFT_BK
    sum_q_influx_EF += q_CC[adjcell] * OFE[adjcell].d_eflux[RIGHT_FR];
    adjcell = IntVector(i-1, j, k+1);	// LEFT_FR
    sum_q_influx_EF += q_CC[adjcell] * OFE[adjcell].d_eflux[RIGHT_BK];

    //__________________________________
    //   INFLUX: CORNER FLUX
    adjcell = IntVector(i+1, j+1, k-1);	// TOP_R_BK
    sum_q_influx_CF += q_CC[adjcell] * OFC[adjcell].d_cflux[BOT_L_FR];
    adjcell = IntVector(i+1, j+1, k+1);	// TOP_R_FR
    sum_q_influx_CF += q_CC[adjcell] * OFC[adjcell].d_cflux[BOT_L_BK];
    adjcell = IntVector(i-1, j+1, k-1);	// TOP_L_BK
    sum_q_influx_CF += q_CC[adjcell] * OFC[adjcell].d_cflux[BOT_R_FR];
    adjcell = IntVector(i-1, j+1, k+1);	// TOP_L_FR
    sum_q_influx_CF += q_CC[adjcell] * OFC[adjcell].d_cflux[BOT_R_BK];
    adjcell = IntVector(i+1, j-1, k-1);	// BOT_R_BK
    sum_q_influx_CF += q_CC[adjcell] * OFC[adjcell].d_cflux[TOP_L_FR];
    adjcell = IntVector(i+1, j-1, k+1);	// BOT_R_FR
    sum_q_influx_CF += q_CC[adjcell] * OFC[adjcell].d_cflux[TOP_L_BK];
    adjcell = IntVector(i-1, j-1, k-1); // BOT_L_BK
    sum_q_influx_CF += q_CC[adjcell] * OFC[adjcell].d_cflux[TOP_R_FR];
    adjcell = IntVector(i-1, j-1, k+1);	// BOT_L_FR
    sum_q_influx_CF += q_CC[adjcell] * OFC[adjcell].d_cflux[TOP_R_BK];

    //__________________________________
    //  Calculate the advected q at t + delta t
    q_advected[*iter] = - sum_q_outflux - sum_q_outflux_EF - sum_q_outflux_CF
                        + sum_q_influx  + sum_q_influx_EF  + sum_q_influx_CF;

  }
}

/*---------------------------------------------------------------------
 Function~  ICE::qOutfluxFirst-- 
 Purpose~  Calculate the quantity \langle q \rangle for each outflux, including
    the corner flux terms

 References:
    "Compatible Fluxes for van Leer Advection" W.B VanderHeyden and 
    B.A. Kashiwa, Journal of Computational Physics, 
    146, 1-28, (1998) 
            
 Steps for each cell:  
 --------------------        
    Calculate the quantity outflux of q for each of the outflowing volumes 
       
01/02/01   Added corner fluxes
 
 See schematic diagram at bottom of ice.cc
 FIRST ORDER ONLY AT THIS TIME 10/21/00
---------------------------------------------------------------------  */ 
void  ICE::qOutfluxFirst(const CCVariable<double>&   q_CC,const Patch* patch,
			CCVariable<fflux>& q_out, CCVariable<eflux>& q_out_EF,
			CCVariable<cflux>& q_out_CF)
{
  const IntVector gc(1,1,1);
// for(CellIterator iter = patch->getExtraCellIterator(); !iter.done(); iter++){
  for(CellIterator iter = patch->getCellIterator(gc); !iter.done(); iter++){
    //__________________________________
    //  SLABS
    for(int face = TOP; face <= BACK; face++ ) {
      q_out[*iter].d_fflux[face] = q_CC[*iter];
    }
    //__________________________________
    //  EDGE fluxes
    for(int edge = TOP_R; edge <= LEFT_BK; edge++ )  {
      q_out_EF[*iter].d_eflux[edge] = q_CC[*iter];
    }
    
    //__________________________________
    //  CORNER fluxes
    for(int corner = TOP_R_BK; corner <= BOT_L_FR; corner++ )  {
      q_out_CF[*iter].d_cflux[corner] = q_CC[*iter];
    }
  }
}

/*---------------------------------------------------------------------
 Function~  ICE::qInflux
 Purpose~
    Calculate the influx contribution \langle q \rangle for each slab and 
    corner flux.   
 
 References:
    "Compatible Fluxes for van Leer Advection" W.B VanderHeyden 
    and B.A. Kashiwa, Journal of Computational Physics, 146, 1-28, (1998) 
              
Implementation Notes:
    The quantity q_outflux is needed from one layer of extra cells surrounding
    the computational domain.
    
    Todd            01/02/01    Added corner cells and extend edged to 3d
See schematic diagram at bottom of file ice.cc
---------------------------------------------------------------------  */
void ICE::qInfluxFirst(const CCVariable<fflux>& q_out, 
		  const CCVariable<eflux>& q_out_EF, 
		  const CCVariable<cflux>& q_out_CF, const Patch* patch,
		  CCVariable<fflux>& q_in, CCVariable<eflux>& q_in_EF, 
		  CCVariable<cflux>& q_in_CF)
{
  for(CellIterator iter = patch->getCellIterator(); !iter.done(); iter++) {
    IntVector curcell = *iter,adjcell;
    int i = curcell.x();
    int j = curcell.y();
    int k = curcell.z();
    
    //   INFLUX SLABS
    adjcell = IntVector(i, j+1, k);
    q_in[*iter].d_fflux[TOP]    = q_out[adjcell].d_fflux[BOTTOM];
    
    adjcell = IntVector(i, j-1, k);
    q_in[*iter].d_fflux[BOTTOM] = q_out[adjcell].d_fflux[TOP];
    
    adjcell = IntVector(i+1, j, k);
    q_in[*iter].d_fflux[RIGHT]  = q_out[adjcell].d_fflux[LEFT];
    
    adjcell = IntVector(i-1, j, k);
    q_in[*iter].d_fflux[LEFT]   = q_out[adjcell].d_fflux[RIGHT];
    
    adjcell = IntVector(i, j, k+1);
    q_in[*iter].d_fflux[FRONT]  = q_out[adjcell].d_fflux[BACK];
    
    adjcell = IntVector(i, j, k-1);
    q_in[*iter].d_fflux[BACK]   = q_out[adjcell].d_fflux[FRONT];
    
    //    INFLUX EDGES
    adjcell = IntVector(i+1, j+1, k);
    q_in_EF[*iter].d_eflux[TOP_R]    = q_out_EF[adjcell].d_eflux[BOT_L];
    
    adjcell = IntVector(i, j+1, k+1);
    q_in_EF[*iter].d_eflux[TOP_FR]   = q_out_EF[adjcell].d_eflux[BOT_BK];
    
    adjcell = IntVector(i-1, j+1, k);
    q_in_EF[*iter].d_eflux[TOP_L]    = q_out_EF[adjcell].d_eflux[BOT_R];
    
    adjcell = IntVector(i, j+1, k-1);
    q_in_EF[*iter].d_eflux[TOP_BK]   = q_out_EF[adjcell].d_eflux[BOT_FR];
    
    adjcell = IntVector(i+1, j-1, k);
    q_in_EF[*iter].d_eflux[BOT_R]    = q_out_EF[adjcell].d_eflux[TOP_L];
    
    adjcell = IntVector(i, j-1, k+1);
    q_in_EF[*iter].d_eflux[BOT_FR]    = q_out_EF[adjcell].d_eflux[TOP_BK];
    
    adjcell = IntVector(i-1, j-1, k);
    q_in_EF[*iter].d_eflux[BOT_L]    = q_out_EF[adjcell].d_eflux[TOP_R];
    
    adjcell = IntVector(i, j-1, k-1);
    q_in_EF[*iter].d_eflux[BOT_BK]    = q_out_EF[adjcell].d_eflux[TOP_FR];
    
    adjcell = IntVector(i+1, j, k-1);
    q_in_EF[*iter].d_eflux[RIGHT_BK]  = q_out_EF[adjcell].d_eflux[LEFT_FR];
    
    adjcell = IntVector(i+1, j, k+1);
    q_in_EF[*iter].d_eflux[RIGHT_FR]  = q_out_EF[adjcell].d_eflux[LEFT_BK];
    
    adjcell = IntVector(i-1, j, k-1);
    q_in_EF[*iter].d_eflux[LEFT_BK]  = q_out_EF[adjcell].d_eflux[RIGHT_FR];
    
    adjcell = IntVector(i-1, j, k+1);
    q_in_EF[*iter].d_eflux[LEFT_FR]  = q_out_EF[adjcell].d_eflux[RIGHT_BK];
    
    /*__________________________________
     *   INFLUX CORNER FLUXES
     *___________________________________*/
    adjcell = IntVector(i+1, j+1, k-1);
    q_in_CF[*iter].d_cflux[TOP_R_BK]= q_out_CF[adjcell].d_cflux[BOT_L_FR];
    
    adjcell = IntVector(i+1, j+1, k+1);
    q_in_CF[*iter].d_cflux[TOP_R_FR]= q_out_CF[adjcell].d_cflux[BOT_L_BK];
    
    adjcell = IntVector(i-1, j+1, k-1);
    q_in_CF[*iter].d_cflux[TOP_L_BK]= q_out_CF[adjcell].d_cflux[BOT_R_FR];
    
    adjcell = IntVector(i-1, j+1, k+1);
    q_in_CF[*iter].d_cflux[TOP_L_FR]= q_out_CF[adjcell].d_cflux[BOT_R_BK];
    
    adjcell = IntVector(i+1, j-1, k-1);
    q_in_CF[*iter].d_cflux[BOT_R_BK]= q_out_CF[adjcell].d_cflux[TOP_L_FR];
    
    adjcell = IntVector(i+1, j-1, k+1);
    q_in_CF[*iter].d_cflux[BOT_R_FR]= q_out_CF[adjcell].d_cflux[TOP_L_BK];
    
    adjcell = IntVector(i-1, j-1, k-1);
    q_in_CF[*iter].d_cflux[BOT_L_BK]= q_out_CF[adjcell].d_cflux[TOP_R_FR];
    
    adjcell = IntVector(i-1, j-1, k+1);
    q_in_CF[*iter].d_cflux[BOT_L_FR]= q_out_CF[adjcell].d_cflux[TOP_R_BK];
  }
}
/* 
 ======================================================================*
 Function:  hydrostaticPressureAdjustment--
 Notes:
            This doesn't take the ghostcells into account, therefore you 
            must adjust boundary conditions after this function.  Material 0 is
            assumed to be the surrounding fluid and therfore we compute the 
            hydrostatic pressure using
            
            press_hydro_ = rho_micro_CC[SURROUNDING_MAT] * grav * some_distance
            
            Currently it is assumed that the top, right and front ghostcells is
            where the reference pressure is defined.
            
CAVEAT:     Only works -gravity.x(), -gravity.y() and -gravity.z()           
_______________________________________________________________________ */
void   ICE::hydrostaticPressureAdjustment(const Patch* patch, 
                          const CCVariable<double>& rho_micro_CC,
                                CCVariable<double>& press_CC)
{
  Vector dx             = patch->dCell();
  Vector gravity        = d_sharedState->getGravity();
  IntVector highIndex   = patch->getInteriorCellHighIndex();
  
                  //ONLY WORKS ON ONE PATCH  Press_ref_* will have to change
  double press_hydro;
  double dist_from_p_ref;
  int press_ref_x  = highIndex.x();      // PRESS REFERENCE LOCATION HARDWIRED
  int press_ref_y  = highIndex.y();
  int press_ref_z  = highIndex.z();
  //__________________________________
  //  X direction
  if (gravity.x() != 0.)  {
    for (CellIterator iter = patch->getCellIterator(); !iter.done(); iter++) {
      IntVector curcell = *iter;
      dist_from_p_ref   =  fabs((double) (curcell.x() - press_ref_x)) * dx.x();
      press_hydro       = rho_micro_CC[*iter] * 
                          fabs(gravity.x() ) * dist_from_p_ref;
      
      press_CC[*iter] += press_hydro;
    }
  }
  //__________________________________
  //  Y direction
  if (gravity.y() != 0.)  {
    for (CellIterator iter = patch->getCellIterator(); !iter.done(); iter++) {
      IntVector curcell = *iter;
      dist_from_p_ref   = fabs((double) (curcell.y() - press_ref_y)) * dx.y();
      press_hydro       = rho_micro_CC[*iter] * 
                          fabs(gravity.y() ) * dist_from_p_ref;
      
      press_CC[*iter] += press_hydro;
    }
  }
  //__________________________________
  //  Z direction
  if (gravity.z() != 0.)  {
    for (CellIterator iter = patch->getCellIterator(); !iter.done(); iter++) {
      IntVector curcell = *iter;
      dist_from_p_ref   = fabs((double) (curcell.z() - press_ref_z)) * dx.z();
      press_hydro       = rho_micro_CC[*iter] * 
                          fabs(gravity.z() ) * dist_from_p_ref;
      
      press_CC[*iter] += press_hydro;
    }
  }   
}


/* 
 ======================================================================*
 Function:  backoutGCPressFromVelFC--           
 You need to set the pressure in the ghostcells so that it is
 consistent with the face centered velocies. If you don't then you can easily
 set the ghostcell pressure so there is flow out of a solid boundary.
 Currently this computes the pressure based on ice_matl[0] which
 is the surrounding matl.
 Note: this function doesn't set the pressure in either the edge or 
       corners cell of the domain.  There isn't a normal component of 
       velocity that we can use to compute it.   
       
  W A R N I N G:
  This is only setup for ice_matls.  I need to modify this 
  when we have mpm_matls on the edge of the domain.
_______________________________________________________________________ */
void   ICE::backoutGCPressFromVelFC(const Patch* patch,
                                Patch::FaceType face,
                                DataWarehouse* old_dw,
                                CCVariable<double>& press_CC, 
                          const StaticArray<CCVariable<double> >& rho_micro_CC,
                          const StaticArray<CCVariable<double> >& rho_CC,
                          const StaticArray<CCVariable<double> >& vol_frac_CC,
                          const StaticArray<CCVariable<Vector> >& vel_CC)
{
  int numICEMatls = d_sharedState->getNumICEMatls();
  int numMatls = d_sharedState->getNumMatls();
  IntVector offset;
  
  double term1, term2, term3, term4, term5;
  double grav, del_Q;
  double rho_FC, rho_micro_FC;
  double plus_minus_one;
 
  double tmp, beta;
  delt_vartype delT;
  old_dw->get(delT, d_sharedState->get_delt_label());
  DenseMatrix K(numICEMatls,numICEMatls), junk(numICEMatls,numICEMatls);
  K.zero();
  
  StaticArray<CCVariable<double> > vel_FC(numICEMatls);
  for(int m = 0; m < numICEMatls; m++) {
    ICEMaterial* matl = d_sharedState->getICEMaterial(m);
    int indx = matl->getDWIndex();
    old_dw->allocate(vel_FC[m],  lb->scratchLabel,  indx,patch);
    vel_FC[m].initialize(0.0);
  }

  getExchangeCoefficients(K, junk);
   
//__________________________________
//  For each wall
//  - convert the vectors into a doubles, vel_CC into vel_FC
//  - define the offsets to the interior cells and 
//    which component of gravity

  Vector gravity = d_sharedState->getGravity();
  Vector dx = patch->dCell();
  int m = SURROUND_MAT;          /*   H A R D W I R E   */
  
  if (face == Patch::xplus) {           //  X P L U S
    offset = IntVector(-1,0,0);
    grav   = gravity.x();
    del_Q  = dx.x();
    plus_minus_one  = -1.0;
    for(int m = 0; m < numICEMatls; m++) {
      for(CellIterator iter=patch->getFaceCellIterator(face);!iter.done();iter++){
        vel_FC[m][*iter] = vel_CC[m][*iter].x();
      }
    }
    //cout << "XPLUS:" <<endl;
  }
  if(face == Patch::xminus){            //  X M I N U S
    offset = IntVector(1,0,0);
    grav   = gravity.x();
    del_Q  = dx.x();
    plus_minus_one  = 1.0;
    for(int m = 0; m < numICEMatls; m++) {
      for(CellIterator iter=patch->getFaceCellIterator(face);!iter.done();iter++){
         vel_FC[m][*iter] = vel_CC[m][*iter].x();
      }
    }
    //cout << "XMINUS:" <<endl;
  }
  if(face == Patch::yplus) {            //  Y P L U S
    offset = IntVector(0,-1,0);
    grav   = gravity.y();
    del_Q  = dx.y();
    plus_minus_one  = -1.0;
    for(int m = 0; m < numICEMatls; m++) {
      for(CellIterator iter=patch->getFaceCellIterator(face);!iter.done();iter++){
         vel_FC[m][*iter] = vel_CC[m][*iter].y();
      }
    }
    //cout << "YPLUS:" <<endl;
  }
  if(face == Patch::yminus) {           //  Y M I N U S
    offset = IntVector(0,1,0);
    grav   = gravity.y();
    del_Q  = dx.y();
    plus_minus_one  = 1.0;
    for(int m = 0; m < numICEMatls; m++) {
      for(CellIterator iter=patch->getFaceCellIterator(face);!iter.done();iter++){
        vel_FC[m][*iter] = vel_CC[m][*iter].y();
      }
    }
    //cout << "YMINUS:" <<endl;
  }
  if (face == Patch::zplus) {           //  Z P L U S
    offset = IntVector(0,0,-1);
    grav   = gravity.z();
    del_Q  = dx.z();
    plus_minus_one  = -1.0;
    for(int m = 0; m < numICEMatls; m++) {
      for(CellIterator iter=patch->getFaceCellIterator(face);!iter.done();iter++){
        vel_FC[m][*iter] = vel_CC[m][*iter].z();
      }
    }
    //cout << "ZPLUS:" <<endl;
  } 
  if (face == Patch::zminus) {          //  Z M I N U S
    offset = IntVector(0,0,1);
    grav   = gravity.z();
    del_Q  = dx.z();
    plus_minus_one  = 1.0;
    for(int m = 0; m < numICEMatls; m++) {
      for(CellIterator iter=patch->getFaceCellIterator(face);!iter.done();iter++){
        vel_FC[m][*iter] = vel_CC[m][*iter].z();
      }
    }
    //cout << "ZMINUS:" <<endl;
  }
   
  //__________________________________
  // Now loop over the ghostcells in that face
  // and backout the pressure this doesn't include 
  // the corner or edge ghostcells 
  for(CellIterator iter=patch->getFaceCellIterator(face);!iter.done();iter++){
    IntVector gcell = *iter;                  // current ghost cell
    IntVector adjcell(gcell.x()+offset.x(),   // interior adjacent cell
                      gcell.y()+offset.y(),
                      gcell.z()+offset.z());

    rho_micro_FC = rho_micro_CC[m][adjcell] + rho_micro_CC[m][gcell];
    rho_FC       = rho_CC[m][adjcell]       + rho_CC[m][gcell];
    ASSERT(rho_FC > 0.0);

    term1 =  vel_FC[m][gcell];
    //__________________________________
    // interpolation to the face
    term2 = (rho_CC[m][adjcell] * vel_FC[m][adjcell] +
             rho_CC[m][gcell]   * vel_FC[m][gcell])/(rho_FC);

    //__________________________________
    // Exchange term
    term3 = 0.0;
    for(int n = 0; n < numICEMatls; n++) {
      tmp    = (vol_frac_CC[n][adjcell] + vol_frac_CC[n][gcell]) * K[n][m];
      beta   = delT * tmp/ (rho_micro_CC[m][gcell] + rho_micro_CC[m][adjcell]);
      term3 += beta*(vel_FC[n][gcell] - vel_FC[m][gcell] );
    }

    //__________________________________
    //gravity & denominator
    term4 =  delT * grav;
    term5 =  2.0 * delT/(rho_micro_FC);
    ASSERT(term5 > 0.0);
   // cout<< *iter<<adjcell<<" "<<term1<<" "<<term2<<" " <<term3<<" "<<term4<<" "<<term5<<endl;
    press_CC[gcell]  = press_CC[adjcell] + plus_minus_one * del_Q * 
                      (term1- term2 - term3 - term4)/term5;
                      
    //__________________________________
    //  Bulletproofing
    if (press_CC[gcell] < 0.0) {
     int i,j,k;
     i   = gcell.x();
     j   = gcell.y();
     k   = gcell.z();
     char warning[100];
     double stupid = delT;
     sprintf(warning, " cell[%d][%d][%d]  delT = %e",i,j,k, stupid);
     Message(1,"ICE::backoutGCPressFromVelFC(): negative pressure detected",
             " This is usually caused by delT being too small. Now exiting..."
             , warning);
    }
  }
   //---- P R I N T   D A T A ------ 
 #if 0
  if( face == Patch::endFace) {
   char description[50];
   sprintf(description, "backoutGCPress");
   printData(   patch, 1, description, "press_CC",    press_CC);
   printData(   patch, 1, description, "vel_FC",      vel_FC[m]);
   getchar();
  }
 #endif
}

/*---------------------------------------------------------------------
 Function~  ICE::computeTauX_LeftandRightCellFace
 Purpose:   This function computes shear stress tau_xx, ta_xy, tau_xz 
            for both the left and right cell faces.
  Note:   - The edge velocities are defined as the average velocity 
            of the 4 cells surrounding that edge, however we only use 2 cells
            to compute it.  When you take the difference of the edge velocities 
            there are two common cells that automatically cancel themselves out.
          - The viscosity we're using isn't right if it varies spatially.
            
  STUPIDNESS:  since the iterator marches in the x dir. we only need 
            to compute the right face for each cell since the left 
            face was computed in the previous cell.  
 ---------------------------------------------------------------------  */
void ICE::computeTauX_LeftandRightCellFace( const CCVariable<Vector> vel_CC,
                          const IntVector curcell,
                          const double viscosity,
                          const Vector dx,
                          StaticArray<double>& tau_XX,
                          StaticArray<double>& tau_XY,
                          StaticArray<double>& tau_XZ)
{
  double term1, term2, grad_1, grad_2;
  double grad_uvel, grad_vvel, grad_wvel;
  //__________________________________
  // loop over the left then right cell face
  for(int face = 0; face <=1; face++) {   
      
    int i = curcell.x() + face;
    int j = curcell.y();
    int k = curcell.z();
    IntVector cell(i,j,k);
    double delX = dx.x();
    double delY = dx.y();
    double delZ = dx.z();
    IntVector left(i-1, j, k);
    //__________________________________
    // - find indices of surrounding cells
    // - compute velocities at cell face edges see note above.
    double uvel_EC_top    = (vel_CC[IntVector(i-1,j+1,k  )].x()   + 
                             vel_CC[IntVector(i  ,j+1,k  )].x() )/4.0;    
    double uvel_EC_bottom = (vel_CC[IntVector(i-1,j-1,k  )].x()   + 
                             vel_CC[IntVector(i  ,j-1,k  )].x() )/4.0;
    double uvel_EC_front  = (vel_CC[IntVector(i-1,j  ,k+1)].x()   + 
                             vel_CC[IntVector(i  ,j  ,k+1)].x() )/4.0;
    double uvel_EC_back   = (vel_CC[IntVector(i-1,j  ,k-1)].x()   + 
                             vel_CC[IntVector(i  ,j  ,k-1)].x() )/4.0;
    double vvel_EC_top    = (vel_CC[IntVector(i-1,j+1,k  )].y()   + 
                             vel_CC[IntVector(i  ,j+1,k  )].y() )/4.0;
    double vvel_EC_bottom = (vel_CC[IntVector(i-1,j-1,k  )].y()   + 
                             vel_CC[IntVector(i  ,j-1,k  )].y() )/4.0;
    double wvel_EC_front  = (vel_CC[IntVector(i-1,j-1,k+1)].z()   + 
                             vel_CC[IntVector(i  ,j  ,k+1)].z() )/4.0;
    double wvel_EC_back   = (vel_CC[IntVector(i-1,j-1,k-1)].z()   + 
                             vel_CC[IntVector(i  ,j  ,k-1)].z() )/4.0;
    //__________________________________
    //  tau_XX
    grad_uvel = (vel_CC[cell].x() - vel_CC[left].x())/delX;
    grad_vvel = (vvel_EC_top      - vvel_EC_bottom)  /delY;
    grad_wvel = (wvel_EC_front    - wvel_EC_back )   /delZ;

    term1 = 2.0 * viscosity * grad_uvel;
    term2 = (2.0/3.0) * viscosity * (grad_uvel + grad_vvel + grad_wvel);
    tau_XX[face] =  term1 - term2;
    //__________________________________
    //  tau_XY
    grad_1 = (uvel_EC_top      - uvel_EC_bottom)  /delY;
    grad_2 = (vel_CC[cell].y() - vel_CC[left].y())/delX;
    tau_XY[face] =  viscosity * (grad_1 + grad_2);

    //__________________________________
    //  tau_XZ
    grad_1 = (uvel_EC_front    - uvel_EC_back)    /delZ;
    grad_2 = (vel_CC[cell].z() - vel_CC[left].z())/delX;
    tau_XZ[face] =  viscosity * (grad_1 + grad_2);
  }
}


/*---------------------------------------------------------------------
 Function~  ICE::computeTauY_BottomandTopCellFace
 Purpose:   This function computes shear stress tau_YY, ta_yx, tau_yz 
            for both the bottom and top cell faces.
  Note:   - The edge velocities are defined as the average velocity 
            of the 4 cells surrounding that edge, however we only use2 cells
            to compute it.  When you take the difference of the edge velocities 
            there are two common cells that automatically cancel themselves out.
          - The viscosity we're using isn't right if it varies spatially.
 ---------------------------------------------------------------------  */
void ICE::computeTauY_BottomandTopCellFace( const CCVariable<Vector> vel_CC,
                          const IntVector curcell,
                          const double viscosity,
                          const Vector dx,
                          StaticArray<double>& tau_YX,
                          StaticArray<double>& tau_YY,
                          StaticArray<double>& tau_YZ)
{
  double term1, term2, grad_1, grad_2;
  double grad_uvel, grad_vvel, grad_wvel;
  //__________________________________
  // loop over the bottom then top cell face
  for(int face = 0; face <=1; face++) {   
      
    int i = curcell.x();
    int j = curcell.y() + face;
    int k = curcell.z();
    IntVector cell(i,j,k);
    double delX = dx.x();
    double delY = dx.y();
    double delZ = dx.z();
    IntVector bottom(i,j-1,k);
    //__________________________________
    // - find indices of surrounding cells
    // - compute velocities at cell face edges see note above.
    double uvel_EC_right = (vel_CC[IntVector(i+1,j,  k  )].x()  + 
                            vel_CC[IntVector(i+1,j-1,k  )].x() )/4.0;
    double uvel_EC_left  = (vel_CC[IntVector(i-1,j,  k  )].x()  +
                            vel_CC[IntVector(i-1,j-1,k  )].x() )/4.0;
    double vvel_EC_right = (vel_CC[IntVector(i+1,j  ,k  )].y()  + 
                            vel_CC[IntVector(i+1,j-1,k  )].y() )/4.0;
    double vvel_EC_left  = (vel_CC[IntVector(i-1,j  ,k  )].y()  + 
                            vel_CC[IntVector(i-1,j-1,k  )].y() )/4.0; 
    double vvel_EC_front = (vel_CC[IntVector(i  ,j  ,k+1)].y()  +
                            vel_CC[IntVector(i  ,j-1,k+1)].y() )/4.0;
    double vvel_EC_back  = (vel_CC[IntVector(i  ,j  ,k-1)].y()  +
                            vel_CC[IntVector(i  ,j-1,k-1)].y() )/4.0;
    double wvel_EC_front = (vel_CC[IntVector(i  ,j  ,k+1)].z()  +
                            vel_CC[IntVector(i  ,j-1,k+1)].z() )/4.0;
    double wvel_EC_back  = (vel_CC[IntVector(i  ,j  ,k-1)].z()  +
                            vel_CC[IntVector(i  ,j-1,k-1)].z() )/4.0;
    //__________________________________
    //  tau_YY
    grad_uvel = (uvel_EC_right    - uvel_EC_left)      /delX;
    grad_vvel = (vel_CC[cell].y() - vel_CC[bottom].y())/delY;
    grad_wvel = (wvel_EC_front    - wvel_EC_back )     /delZ;
    
    term1 = 2.0 * viscosity * grad_vvel;
    term2 = (2.0/3.0) * viscosity * (grad_uvel + grad_vvel + grad_wvel);
    tau_YY[face] =  term1 - term2;
    //__________________________________
    //  tau_YX
    grad_1 = (vel_CC[cell].x() - vel_CC[bottom].x())/delY;
    grad_2 = (vvel_EC_right    - vvel_EC_left)    /delX;

    tau_YX[face] =  viscosity * (grad_1 + grad_2);

    //__________________________________
    //  tau_YZ
    grad_1 = (vvel_EC_front    - vvel_EC_back)    /delZ;
    grad_2 = (vel_CC[cell].z() - vel_CC[bottom].z())/delY;
    tau_YZ[face] =  viscosity * (grad_1 + grad_2);
  }
}

/*---------------------------------------------------------------------
 Function~  ICE::omputeTauZ_BackandFrontCellFace
 Purpose:   This function computes shear stress tau_zx, ta_zy, tau_zz 
            for both the back and front cell faces.
  Note:   - The edge velocities are defined as the average velocity 
            of the 4 cells surrounding that edge, however we only use 2 cells
            to compute it.  When you take the difference of the edge velocities 
            there are two common cells that automatically cancel themselves out.
          - The viscosity we're using isn't right if it varies spatially.
 ---------------------------------------------------------------------  */
void ICE::computeTauZ_BackandFrontCellFace( const CCVariable<Vector> vel_CC,
                          const IntVector curcell,
                          const double viscosity,
                          const Vector dx,
                          StaticArray<double>& tau_ZX,
                          StaticArray<double>& tau_ZY,
                          StaticArray<double>& tau_ZZ)
{
  double term1, term2, grad_1, grad_2;
  double grad_uvel, grad_vvel, grad_wvel;
  //__________________________________
  // loop over the left then right cell face
  for(int face = 0; face <=1; face++) {   
      
    int i = curcell.x();
    int j = curcell.y();
    int k = curcell.z() + face;
    IntVector cell(i,j,k);
    double delX = dx.x();
    double delY = dx.y();
    double delZ = dx.z();
    IntVector back(i, j, k-1);
    //__________________________________
    // - find indices of surrounding cells
    // - compute velocities at cell face edges see note above.
    double uvel_EC_right  = (vel_CC[IntVector(i+1,j,  k  )].x()  + 
                             vel_CC[IntVector(i+1,j,  k-1)].x() )/4.0;
    double uvel_EC_left   = (vel_CC[IntVector(i-1,j,  k  )].x()  +
                             vel_CC[IntVector(i-1,j  ,k-1)].x() )/4.0;
    double vvel_EC_top    = (vel_CC[IntVector(i  ,j+1,k  )].y()   + 
                             vel_CC[IntVector(i  ,j+1,k-1)].y() )/4.0;
    double vvel_EC_bottom = (vel_CC[IntVector(i  ,j-1,k  )].y()   + 
                             vel_CC[IntVector(i  ,j-1,k-1)].y() )/4.0;
    double wvel_EC_right  = (vel_CC[IntVector(i+1,j,  k  )].z()  + 
                             vel_CC[IntVector(i+1,j  ,k-1)].z() )/4.0;
    double wvel_EC_left   = (vel_CC[IntVector(i-1,j,  k  )].z()  +
                             vel_CC[IntVector(i-1,j  ,k-1)].z() )/4.0;
    double wvel_EC_top    = (vel_CC[IntVector(i  ,j+1,k  )].z()   + 
                             vel_CC[IntVector(i  ,j+1,k-1)].z() )/4.0;
    double wvel_EC_bottom = (vel_CC[IntVector(i  ,j-1,k  )].z()   + 
                             vel_CC[IntVector(i  ,j-1,k-1)].z() )/4.0;
    //__________________________________
    //  tau_ZX
    grad_1 = (vel_CC[cell].x() - vel_CC[back].x()) /delZ;
    grad_2 = (wvel_EC_right    - wvel_EC_left)     /delX;
    tau_ZX[face] =  viscosity * (grad_1 + grad_2);
 
    //__________________________________
    //  tau_ZY
    grad_1 = (vel_CC[cell].y() - vel_CC[back].y()) /delZ;
    grad_2 = (wvel_EC_top      - wvel_EC_bottom)   /delX;
    tau_ZY[face] =  viscosity * (grad_1 + grad_2);

    //__________________________________
    //  tau_ZZ
    grad_uvel = (uvel_EC_right    - uvel_EC_left)    /delX;
    grad_vvel = (vvel_EC_top      - vvel_EC_bottom)  /delY;
    grad_wvel = (vel_CC[cell].z() - vel_CC[back].z())/delZ;

    term1 = 2.0 * viscosity * grad_wvel;
    term2 = (2.0/3.0) * viscosity * (grad_uvel + grad_vvel + grad_wvel);
    tau_ZZ[face] =  term1 - term2;
  }
}
/*---------------------------------------------------------------------
 Function~  ICE::getExchangeCoefficients--
 ---------------------------------------------------------------------  */
void ICE::getExchangeCoefficients( DenseMatrix& K,
                                   DenseMatrix& H  )
{
  int numMatls  = d_sharedState->getNumMatls();
    // Fill in the exchange matrix with the vector of exchange coefficients.
   // The vector of exchange coefficients only contains the upper triagonal
   // matrix

   // Check if the # of coefficients = # of upper triangular terms needed
   int num_coeff = ((numMatls)*(numMatls) - numMatls)/2;

   vector<double>::iterator it=d_K_mom.begin(),it1=d_K_heat.begin();

   if (num_coeff == (int)d_K_mom.size() && num_coeff==(int)d_K_heat.size()) {
     // Fill in the upper triangular matrix
     for (int i = 0; i < numMatls; i++ )  {
      for (int j = i + 1; j < numMatls; j++) {
        K[i][j] = K[j][i] = *it++;
        H[i][j] = H[j][i] = *it1++;
      }
     }
   } else if (2*num_coeff==(int)d_K_mom.size() && 
	      2*num_coeff == (int)d_K_heat.size()){
     // Fill in the whole matrix but skip the diagonal terms
     for (int i = 0; i < numMatls; i++ )  {
      for (int j = 0; j < numMatls; j++) {
        if (i == j) continue;
        K[i][j] = *it++;
        H[i][j] = *it1++;
      }
     }
   } else {
     cerr << "Number of exchange components don't match " << endl;
     abort();
   }
}



#if 0
/*__________________________________
*   ONLY NEEDED BY SECOND ORDER ADVECTION
*___________________________________*/
void ICE::outflowVolCentroid(const SFCXVariable<double>& uvel_FC,
                             const SFCYVariable<double>& vvel_FC,
                             const SFCZVariable<double>& wvel_FC,
                             const double& delT, const Vector& dx,
                             CCVariable<fflux>& r_out_x,
                             CCVariable<fflux>& r_out_y,
                             CCVariable<fflux>& r_out_z,
                             CCVariable<eflux>& r_out_x_CF,
                             CCVariable<eflux>& r_out_y_CF,
                             CCVariable<eflux>& r_out_z_CF)

{

}

void ICE::qOutfluxSecond(CCVariable<fflux>& OFS,
                         CCVariable<fflux>& IFS,
                         CCVariable<fflux>& r_out_x,
                         CCVariable<fflux>& r_out_y,
                         CCVariable<fflux>& r_out_z,
                         CCVariable<eflux>& r_out_x_CF,
                         CCVariable<eflux>& r_out_y_CF,
                         CCVariable<eflux>& r_out_z_CF,
                         const Vector& dx)
{

}
#endif

#ifdef __sgi
#define IRIX
#pragma set woff 1209
#endif

namespace Uintah {

static MPI_Datatype makeMPI_fflux()
{
   ASSERTEQ(sizeof(ICE::fflux), sizeof(double)*6);
   MPI_Datatype mpitype;
   MPI_Type_vector(1, 6, 6, MPI_DOUBLE, &mpitype);
   MPI_Type_commit(&mpitype);
   return mpitype;
}

const TypeDescription* fun_getTypeDescription(ICE::fflux*)
{
   static TypeDescription* td = 0;
   if(!td){
      td = scinew TypeDescription(TypeDescription::Other,
                               "ICE::fflux", true, &makeMPI_fflux);
   }
   return td;
}

static MPI_Datatype makeMPI_eflux()
{
   ASSERTEQ(sizeof(ICE::eflux), sizeof(double)*12);
   MPI_Datatype mpitype;
   MPI_Type_vector(1, 12, 12, MPI_DOUBLE, &mpitype);
   MPI_Type_commit(&mpitype);
   return mpitype;
}

const TypeDescription* fun_getTypeDescription(ICE::eflux*)
{
   static TypeDescription* td = 0;
   if(!td){
      td = scinew TypeDescription(TypeDescription::Other,
                               "ICE::eflux", true, &makeMPI_eflux);
   }
   return td;
}

static MPI_Datatype makeMPI_cflux()
{
   ASSERTEQ(sizeof(ICE::cflux), sizeof(double)*8);
   MPI_Datatype mpitype;
   MPI_Type_vector(1, 8, 8, MPI_DOUBLE, &mpitype);
   MPI_Type_commit(&mpitype);
   return mpitype;
}

const TypeDescription* fun_getTypeDescription(ICE::cflux*)
{
   static TypeDescription* td = 0;
   if(!td){
      td = scinew TypeDescription(TypeDescription::Other,
                               "ICE::cflux", true, &makeMPI_cflux);
   }
   return td;
}

} // end namespace Uintah
//______________________________________________________________________
//  Snippets of debugging code
#if 0
    int i, j, k;
    for(CellIterator iter = patch->getCellIterator(); !iter.done(); iter++){
      IntVector curcell = *iter;       
     i = curcell.x();
     j = curcell.y();
     k = curcell.z();
}
#endif
/*______________________________________________________________________
          S H E M A T I C   D I A G R A M S

                                    q_outflux(TOP)

                                        |    (I/O)flux_EF(TOP_BK)
                                        |
  (I/O)flux_CF(TOP_L_BK)       _________|___________
                              /___/_____|_______/__/|   (I/O)flux_CF(TOP_R_BK)
                             /   /      |      /  | |
                            /   /       |     /  /| |
  (I/O)flux_EF(TOP_L)      /   /             /  / |/|
                          /___/_____________/__/ ------ (I/O)flux_EF(TOP_R)
                        _/__ /_____________/__/| /| | 
                        |   |             |  | |/ | |   (I/O)flux_EF(BCK_R)
                        | + |      +      | +| /  | |      
                        |---|----------------|/|  |/| 
                        |   |             |  | | /| /  (I/O)flux_CF(BOT_R_BK)
  (I/O)flux(LEFT_FR)    | + |     i,j,k   | +| |/ /          
                        |   |             |  |/| /   (I/O)flux_EF(BOT_R)
                        |---|----------------| |/
  (I/O)flux_CF(BOT_L_FR)| + |      +      | +|/    (I/O)flux_CF(BOT_R_FR)
                        ---------------------- 
                         (I/O)flux_EF(BOT_FR)       
                         
                                         
                         
                            (TOP)      
   ______________________              ______________________  _
   |   |             |  |              |   |             |  |  |  delY_top
   | + |      +      | +|              | + |      +      | +|  |
   |---|----------------|  --ytop      |---|----------------|  -
   |   |             |  |              |   |             |  |
   | + |     i,j,k   | +| (RIGHT)      | + |     i,j,k   | +|
   |   |             |  |              |   |             |  |
   |---|----------------|  --y0        |---|----------------|  -
   | + |      +      | +|              | + |      +      | +|  | delY_bottom
   ----------------------              ----------------------  -
       |             |                 |---|             |--|
       x0            xright              delX_left         delX_right
       
                            (BACK)
   ______________________              ______________________  _
   |   |             |  |              |   |             |  |  |  delZ_back
   | + |      +      | +|              | + |      +      | +|  |
   |---|----------------|  --z0        |---|----------------|  -
   |   |             |  |              |   |             |  |
   | + |     i,j,k   | +| (RIGHT)      | + |     i,j,k   | +|
   |   |             |  |              |   |             |  |
   |---|----------------|  --z_frt     |---|----------------|  -
   | + |      +      | +|              | + |      +      | +|  | delZ_front
   ----------------------              ----------------------  -
       |             |                 |---|             |--|
       x0            xright              delX_left         delX_right
                         
______________________________________________________________________*/ 

