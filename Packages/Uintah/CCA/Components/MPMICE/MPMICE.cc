#include <Packages/Uintah/CCA/Components/MPMICE/MPMICE.h>
#include <Packages/Uintah/CCA/Components/MPM/SerialMPM.h>
#include <Packages/Uintah/CCA/Components/MPM/ConstitutiveModel/MPMMaterial.h>
#include <Packages/Uintah/CCA/Components/ICE/ICE.h>
#include <Packages/Uintah/CCA/Components/ICE/ICEMaterial.h>
#include <Packages/Uintah/Core/Grid/Task.h>
#include <Packages/Uintah/CCA/Ports/Scheduler.h>

#include <Packages/Uintah/CCA/Components/MPM/Burn/HEBurn.h>
#include <Packages/Uintah/CCA/Components/MPM/ConstitutiveModel/ConstitutiveModel.h>
#include <Packages/Uintah/CCA/Components/MPM/MPMPhysicalModules.h>

using namespace Uintah;
using namespace SCIRun;
using namespace std;

MPMICE::MPMICE(const ProcessorGroup* myworld)
  : UintahParallelComponent(myworld)
{
  Mlb = scinew MPMLabel();
  Ilb = scinew ICELabel();
  d_fracture = false;
  d_mpm      = scinew SerialMPM(myworld);
  d_ice      = scinew ICE(myworld);
}

MPMICE::~MPMICE()
{
  delete Mlb;
  delete Ilb;
  delete d_mpm;
  delete d_ice;
}

void MPMICE::problemSetup(const ProblemSpecP& prob_spec, GridP& grid,
			  SimulationStateP& sharedState)
{
   d_sharedState = sharedState;

   d_mpm->setMPMLabel(Mlb);
   d_mpm->problemSetup(prob_spec, grid, d_sharedState);

   d_ice->setICELabel(Ilb);
   d_ice->problemSetup(prob_spec, grid, d_sharedState);

   cerr << "MPMICE::problemSetup passed.\n";
}

void MPMICE::scheduleInitialize(const LevelP& level,
				SchedulerP& sched,
				DataWarehouseP& dw)
{
  d_mpm->scheduleInitialize(level, sched, dw);
  d_ice->scheduleInitialize(level, sched, dw);
}

void MPMICE::scheduleComputeStableTimestep(const LevelP&,
					   SchedulerP&,
					   DataWarehouseP&)
{
   // Nothing to do here - delt is computed as a by-product of the
   // consitutive model
}

void MPMICE::scheduleTimeAdvance(double t, double dt,
				 const LevelP&         level,
				 SchedulerP&     sched,
				 DataWarehouseP& old_dw, 
				 DataWarehouseP& new_dw)
{
   int numMPMMatls = d_sharedState->getNumMPMMatls();
   int numICEMatls = d_sharedState->getNumICEMatls();

   for(Level::const_patchIterator iter=level->patchesBegin();
       iter != level->patchesEnd(); iter++){

    const Patch* patch=*iter;
    if(d_fracture) {
       d_mpm->scheduleComputeNodeVisibility(patch,sched,old_dw,new_dw);
    }
    d_mpm->scheduleInterpolateParticlesToGrid(patch,sched,old_dw,new_dw);

    if (MPMPhysicalModules::thermalContactModel) {
       d_mpm->scheduleComputeHeatExchange(patch,sched,old_dw,new_dw);
    }

    d_mpm->scheduleInterpolateParticlesToGrid(patch,sched,old_dw,new_dw);
    d_mpm->scheduleExMomInterpolated(patch,sched,old_dw,new_dw);
    d_mpm->scheduleComputeStressTensor(patch,sched,old_dw,new_dw);
    d_mpm->scheduleComputeInternalForce(patch,sched,old_dw,new_dw);
    d_mpm->scheduleComputeInternalHeatRate(patch,sched,old_dw,new_dw);
    d_mpm->scheduleSolveEquationsMotion(patch,sched,old_dw,new_dw);
    d_mpm->scheduleSolveHeatEquations(patch,sched,old_dw,new_dw);
    d_mpm->scheduleIntegrateAcceleration(patch,sched,old_dw,new_dw);
    d_mpm->scheduleIntegrateTemperatureRate(patch,sched,old_dw,new_dw);
    d_mpm->scheduleExMomIntegrated(patch,sched,old_dw,new_dw);
    d_mpm->scheduleInterpolateToParticlesAndUpdate(patch,sched,old_dw,new_dw);
    d_mpm->scheduleComputeMassRate(patch,sched,old_dw,new_dw);
    if(d_fracture) {
      d_mpm->scheduleCrackGrow(patch,sched,old_dw,new_dw);
      d_mpm->scheduleStressRelease(patch,sched,old_dw,new_dw);
      d_mpm->scheduleComputeCrackSurfaceContactForce(patch,sched,old_dw,new_dw);
    }
    d_mpm->scheduleCarryForwardVariables(patch,sched,old_dw,new_dw);

    // Step 1a  computeSoundSpeed
    d_ice->scheduleStep1a(patch,sched,old_dw,new_dw);
    // Step 1b calculate equlibration pressure
    d_ice->scheduleStep1b(patch,sched,old_dw,new_dw);
    // Step 1c compute face centered velocities
    d_ice->scheduleStep1c(patch,sched,old_dw,new_dw);
    // Step 1d computes momentum exchange on FC velocities
    d_ice->scheduleStep1d(patch,sched,old_dw,new_dw);
    // Step 2 computes delPress and the new pressure
    d_ice->scheduleStep2(patch,sched,old_dw,new_dw);
    // Step 3 compute face centered pressure
    d_ice->scheduleStep3(patch,sched,old_dw,new_dw);
    // Step 4a compute sources of momentum
    d_ice->scheduleStep4a(patch,sched,old_dw,new_dw);
    // Step 4b compute sources of energy
    d_ice->scheduleStep4b(patch,sched,old_dw,new_dw);
    // Step 5a compute lagrangian quantities
    d_ice->scheduleStep5a(patch,sched,old_dw,new_dw);
    // Step 5b cell centered momentum exchange
    d_ice->scheduleStep5b(patch,sched,old_dw,new_dw);
    // Step 6and7 advect and advance in time
    d_ice->scheduleStep6and7(patch,sched,old_dw,new_dw);

#if 0
      {
	/* interpolateNCToCC */

	 Task* t=scinew Task("MPMICE::interpolateNCToCC",
		    patch, old_dw, new_dw,
		    this, &MPMICE::interpolateNCToCC);

	 for(int m = 0; m < numMPMMatls; m++){
	    MPMMaterial* mpm_matl = d_sharedState->getMPMMaterial(m);
	    int idx = mpm_matl->getDWIndex();
	    t->requires(new_dw, Mlb->gMomExedVelocityStarLabel, idx, patch,
			Ghost::AroundCells, 1);
	    t->requires(new_dw, Mlb->gMassLabel,                idx, patch,
			Ghost::AroundCells, 1);
	    t->computes(new_dw, Mlb->cVelocityLabel, idx, patch);
	 }

	sched->addTask(t);
      }

      {
	/* interpolateCCToNC */

	 Task* t=scinew Task("MPMICE::interpolateCCToNC",
		    patch, old_dw, new_dw,
		    this, &MPMICE::interpolateCCToNC);

	 for(int m = 0; m < numMPMMatls; m++){
	    MPMMaterial* mpm_matl = d_sharedState->getMPMMaterial(m);
	    int idx = mpm_matl->getDWIndex();
	    t->requires(new_dw, Mlb->cVelocityLabel,   idx, patch,
			Ghost::None, 0);
	    t->requires(new_dw, Mlb->cVelocityMELabel, idx, patch,
			Ghost::None, 0);
	    t->computes(new_dw, Mlb->gVelAfterIceLabel, idx, patch);
	    t->computes(new_dw, Mlb->gAccAfterIceLabel, idx, patch);
	 }
	 t->requires(old_dw, d_sharedState->get_delt_label() );

	sched->addTask(t);
      }
#endif
  }

    
   sched->scheduleParticleRelocation(level, old_dw, new_dw,
				     Mlb->pXLabel_preReloc, 
				     Mlb->d_particleState_preReloc,
				     Mlb->pXLabel, Mlb->d_particleState,
				     numMPMMatls);

   /* Do 'save's in the DataArchiver section of the problem specification now
   new_dw->pleaseSave(Mlb->pXLabel, numMPMMatls);
   new_dw->pleaseSave(Mlb->pVolumeLabel, numMPMMatls);
   new_dw->pleaseSave(Mlb->pStressLabel, numMPMMatls);

   new_dw->pleaseSave(Mlb->gMassLabel, numMPMMatls);

   // Add pleaseSaves here for each of the grid variables
   // created by interpolateParticlesForSaving
   new_dw->pleaseSave(Mlb->gStressForSavingLabel, numMPMMatls);

   if(d_fracture) {
     new_dw->pleaseSave(Mlb->pCrackSurfaceNormalLabel, numMPMMatls);
     new_dw->pleaseSave(Mlb->pIsBrokenLabel, numMPMMatls);
   }

   new_dw->pleaseSaveIntegrated(Mlb->StrainEnergyLabel);
   new_dw->pleaseSaveIntegrated(Mlb->KineticEnergyLabel);
   new_dw->pleaseSaveIntegrated(Mlb->TotalMassLabel);
   new_dw->pleaseSaveIntegrated(Mlb->CenterOfMassPositionLabel);
   new_dw->pleaseSaveIntegrated(Mlb->CenterOfMassVelocityLabel);
   */
}

void MPMICE::interpolateNCToCC(const ProcessorGroup*,
                                     const Patch* patch,
                                     DataWarehouseP&,
                                     DataWarehouseP& new_dw)
{
  int numMatls = d_sharedState->getNumMPMMatls();
  Vector zero(0.,0.,0.);

  for(int m = 0; m < numMatls; m++){
    MPMMaterial* mpm_matl = d_sharedState->getMPMMaterial( m );
    int matlindex = mpm_matl->getDWIndex();

     // Create arrays for the grid data
     NCVariable<double> gmass;
     NCVariable<Vector> gvelocity;
     CCVariable<double> cmass;
     CCVariable<Vector> cvelocity;

     new_dw->get(gmass,     Mlb->gMassLabel,                matlindex, patch,
					   Ghost::AroundCells, 1);
     new_dw->get(gvelocity, Mlb->gMomExedVelocityStarLabel, matlindex, patch,
					   Ghost::AroundCells, 1);
     new_dw->allocate(cmass,     Mlb->cMassLabel,     matlindex, patch);
     new_dw->allocate(cvelocity, Mlb->cVelocityLabel, matlindex, patch);
 
     IntVector nodeIdx[8];

     for(CellIterator iter = patch->getCellIterator(); !iter.done(); iter++){
      patch->findNodesFromCell(*iter,nodeIdx);
      cvelocity[*iter] = zero;
      cmass[*iter]     = 0.;
      for (int in=0;in<8;in++){
	cvelocity[*iter] += gvelocity[nodeIdx[in]]*gmass[nodeIdx[in]];
	cmass[*iter]     += gmass[nodeIdx[in]];
      }
     }

     for(CellIterator iter = patch->getCellIterator(); !iter.done(); iter++){
	cvelocity[*iter] = cvelocity[*iter]/cmass[*iter];
     }
     new_dw->put(cvelocity, Mlb->cVelocityLabel, matlindex, patch);
  }
}

