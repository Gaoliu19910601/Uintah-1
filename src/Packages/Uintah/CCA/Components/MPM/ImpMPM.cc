
#ifdef __APPLE__
// This is a hack.  gcc 3.3 #undefs isnan in the cmath header, which
// make the isnan function not work.  This define makes the cmath header
// not get included since we do not need it anyway.
#  define _CPP_CMATH
#endif

#include <Packages/Uintah/CCA/Components/MPM/ImpMPM.h> 
#include <Packages/Uintah/CCA/Components/MPM/ImpMPMFlags.h> 
// put here to avoid template problems
#include <Packages/Uintah/Core/Math/Matrix3.h>
#include <Packages/Uintah/CCA/Components/MPM/ConstitutiveModel/MPMMaterial.h>
#include <Packages/Uintah/CCA/Components/MPM/ConstitutiveModel/ConstitutiveModel.h>
#include <Packages/Uintah/CCA/Components/MPM/ConstitutiveModel/ImplicitCM.h>
#include <Packages/Uintah/CCA/Components/MPM/PhysicalBC/MPMPhysicalBCFactory.h>
#include <Packages/Uintah/CCA/Components/MPM/PhysicalBC/NormalForceBC.h>
#include <Packages/Uintah/CCA/Components/MPM/PhysicalBC/HeatFluxBC.h>
#include <Packages/Uintah/CCA/Components/MPM/PhysicalBC/ArchesHeatFluxBC.h>
#include <Packages/Uintah/Core/Grid/LinearInterpolator.h>
#include <Packages/Uintah/CCA/Components/MPM/HeatConduction/ImplicitHeatConduction.h>
#include <Packages/Uintah/CCA/Components/MPM/ThermalContact/ThermalContact.h>
#include <Packages/Uintah/CCA/Components/MPM/ThermalContact/ThermalContactFactory.h>
#include <Packages/Uintah/CCA/Components/MPM/MPMBoundCond.h>
#include <Packages/Uintah/CCA/Ports/DataWarehouse.h>
#include <Packages/Uintah/CCA/Ports/Scheduler.h>
#include <Packages/Uintah/Core/Grid/Grid.h>
#include <Packages/Uintah/Core/Grid/Level.h>
#include <Packages/Uintah/Core/Grid/Variables/CCVariable.h>
#include <Packages/Uintah/Core/Grid/Variables/NCVariable.h>
#include <Packages/Uintah/Core/Grid/Variables/ParticleSet.h>
#include <Packages/Uintah/Core/Grid/Variables/ParticleVariable.h>
#include <Packages/Uintah/Core/ProblemSpec/ProblemSpec.h>
#include <Packages/Uintah/Core/Grid/Variables/NodeIterator.h>
#include <Packages/Uintah/Core/Grid/Variables/CellIterator.h>
#include <Packages/Uintah/Core/Grid/Patch.h>
#include <Packages/Uintah/Core/Grid/SimulationState.h>
#include <Packages/Uintah/Core/Grid/Variables/SoleVariable.h>
#include <Packages/Uintah/Core/Grid/Task.h>
#include <Packages/Uintah/Core/Grid/BoundaryConditions/BoundCond.h>
#include <Packages/Uintah/Core/Grid/BoundaryConditions/VelocityBoundCond.h>
#include <Packages/Uintah/Core/Grid/BoundaryConditions/TemperatureBoundCond.h>
#include <Packages/Uintah/Core/Grid/BoundaryConditions/SymmetryBoundCond.h>
#include <Packages/Uintah/Core/Grid/Variables/VarTypes.h>
#include <Packages/Uintah/Core/Exceptions/ParameterNotFound.h>
#include <Packages/Uintah/Core/Exceptions/ProblemSetupException.h>
#include <Packages/Uintah/Core/Parallel/ProcessorGroup.h>
#include <Core/Geometry/Vector.h>
#include <Core/Geometry/Point.h>
#include <Core/Math/MinMax.h>
#include <Packages/Uintah/CCA/Ports/LoadBalancer.h>
#include <Core/Util/DebugStream.h>
#include <Packages/Uintah/Core/Grid/BoundaryConditions/fillFace.h>
#include <Packages/Uintah/CCA/Components/MPM/PetscSolver.h>
#include <Packages/Uintah/CCA/Components/MPM/SimpleSolver.h>
#include <Packages/Uintah/Core/Grid/BoundaryConditions/BCDataArray.h>
#include <Packages/Uintah/Core/Math/FastMatrix.h>
#include <sgi_stl_warnings_off.h>
#include <set>
#include <numeric>
#include <algorithm>
#include <limits>
#include <iostream>
#include <fstream>
#include <sgi_stl_warnings_on.h>
#include <math.h>

#ifdef _WIN32
#  include <process.h>
#  include <float.h>
#  define isnan _isnan
#endif

using namespace Uintah;
using namespace SCIRun;
using namespace std;

static DebugStream cout_doing("IMPM", false);


ImpMPM::ImpMPM(const ProcessorGroup* myworld) :
  MPMCommon(myworld), UintahParallelComponent(myworld)
{
  lb = scinew MPMLabel();
  flags = scinew ImpMPMFlags();
  d_nextOutputTime=0.;
  d_SMALL_NUM_MPM=1e-200;
  d_rigid_body = false;
  d_numIterations=0;

  heatConductionModel = 0;
  thermalContactModel = 0;
  d_perproc_patches = 0;
  d_switchCriteria = 0;

  one_matl = scinew MaterialSubset();
  one_matl->add(0);
  one_matl->addReference();
}

bool ImpMPM::restartableTimesteps()
{
  return true;
}

ImpMPM::~ImpMPM()
{
  delete lb;
  delete flags;

  if(d_perproc_patches && d_perproc_patches->removeReference()) { 
    delete d_perproc_patches;
    cout << "Freeing patches!!\n";
  }

  if(one_matl->removeReference())
    delete one_matl;

  delete d_solver;
  delete heatConductionModel;
  delete thermalContactModel;
  MPMPhysicalBCFactory::clean();
}

void ImpMPM::problemSetup(const ProblemSpecP& prob_spec, 
                          const ProblemSpecP& materials_ps,GridP& grid,
                          SimulationStateP& sharedState)
{
   d_sharedState = sharedState;

   ProblemSpecP p = prob_spec->findBlock("DataArchiver");
   if(!p->get("outputInterval", d_outputInterval))
      d_outputInterval = 1.0;

   
   ProblemSpecP mpm_ps = 0;
   ProblemSpecP restart_mat_ps = 0;

   if (materials_ps)
     restart_mat_ps = materials_ps;
   else
     restart_mat_ps = prob_spec;

   ProblemSpecP mpm_soln_ps = restart_mat_ps->findBlock("MPM");

   string integrator_type;
   if (mpm_soln_ps) {

     // Read all MPM flags (look in MPMFlags.cc)
     flags->readMPMFlags(restart_mat_ps);
     if (flags->d_integrator_type != "implicit")
       throw ProblemSetupException("Can't use explicit integration with -impm", __FILE__, __LINE__);

     // convert text representation of face into FaceType
     std::vector<std::string>::const_iterator ftit;
     for(ftit=flags->d_bndy_face_txt_list.begin();ftit!=flags->d_bndy_face_txt_list.end();ftit++){
        Patch::FaceType face = Patch::invalidFace;
        for(Patch::FaceType ft=Patch::startFace;ft<=Patch::endFace;
                                                ft=Patch::nextFace(ft)) {
          if(Patch::getFaceName(ft)==*ftit) face =  ft;
        }
        if(face!=Patch::invalidFace) {
          d_bndy_traction_faces.push_back(face);
        } else {
          std::cerr << "warning: ignoring unknown face '" 
                    << *ftit<< "'" << std::endl;
        }
     }
   }

   //Search for the MaterialProperties block and then get the MPM section

   ProblemSpecP mat_ps =  restart_mat_ps->findBlock("MaterialProperties");

   ProblemSpecP mpm_mat_ps = mat_ps->findBlock("MPM");

   ProblemSpecP child = mpm_mat_ps->findBlock("contact");

   d_con_type = "null";
   child->get("type",d_con_type);
   d_rigid_body = false;

   if (d_con_type == "rigid"){
      d_rigid_body = true;
      Vector defaultDir(1,1,1);
      child->getWithDefault("direction",d_contact_dirs, defaultDir);
      child->getWithDefault("stop_time",d_stop_time, 
                                        std::numeric_limits<double>::max());
      child->getWithDefault("velocity_after_stop",d_vel_after_stop, 
                                                  Vector(0,0,0));
   }

   MPMPhysicalBCFactory::create(restart_mat_ps);
   if( (int)MPMPhysicalBCFactory::mpmPhysicalBCs.size()==0) {
     if(flags->d_useLoadCurves){
       throw ProblemSetupException("No load curve in ups, d_useLoadCurve==true?", __FILE__, __LINE__);
    }
   }

   materialProblemSetup(restart_mat_ps, d_sharedState,flags);
   
   if (flags->d_solver_type == "petsc") {
     d_solver = scinew MPMPetscSolver();
   } else {
     d_solver = scinew SimpleSolver();
   }


   d_solver->initialize();

   // setup sub scheduler
   Scheduler* sched = dynamic_cast<Scheduler*>(getPort("scheduler"));
   sched->setRestartable(true);

   d_subsched = sched->createSubScheduler();
   d_subsched->initialize(3,1);
   d_subsched->clearMappings();
   d_subsched->mapDataWarehouse(Task::ParentOldDW, 0);
   d_subsched->mapDataWarehouse(Task::ParentNewDW, 1);
   d_subsched->mapDataWarehouse(Task::OldDW, 2);
   d_subsched->mapDataWarehouse(Task::NewDW, 3);
   
   d_recompileSubsched = true;


   heatConductionModel = scinew ImplicitHeatConduction(sharedState,lb,flags);

   heatConductionModel->problemSetup(flags->d_solver_type);

   thermalContactModel =
     ThermalContactFactory::create(restart_mat_ps, sharedState, lb,flags);

   d_switchCriteria = dynamic_cast<SwitchingCriteria*>
     (getPort("switch_criteria"));
   
   if (d_switchCriteria) {
     d_switchCriteria->problemSetup(restart_mat_ps,materials_ps,d_sharedState);
   }
    
   
   // Pull out from Time section
   d_initialDt = 10000.0;
   ProblemSpecP time_ps = restart_mat_ps->findBlock("Time");
   time_ps->get("delt_init",d_initialDt);
}


void ImpMPM::outputProblemSpec(ProblemSpecP& root_ps)
{
  ProblemSpecP root = root_ps->getRootNode();

  ProblemSpecP flags_ps = root->appendChild("MPM");
  flags->outputProblemSpec(flags_ps);

  ProblemSpecP mat_ps = 0;
  mat_ps = root->findBlock("MaterialProperties");

  if (mat_ps == 0)
    mat_ps = root->appendChild("MaterialProperties");
    
  ProblemSpecP mpm_ps = mat_ps->appendChild("MPM");
  for (int i = 0; i < d_sharedState->getNumMPMMatls();i++) {
    MPMMaterial* mat = d_sharedState->getMPMMaterial(i);
    ProblemSpecP cm_ps = mat->outputProblemSpec(mpm_ps);
  }

#if 0
  contactModel->outputProblemSpec(mpm_ps);
#endif

  ProblemSpecP contact_ps = mpm_ps->appendChild("contact");
  contact_ps->appendElement("type",d_con_type);
  contact_ps->appendElement("direction",d_contact_dirs);
  contact_ps->appendElement("stop_time",d_stop_time);
  contact_ps->appendElement("velocity_after_stop",d_vel_after_stop);
}


void ImpMPM::scheduleInitialize(const LevelP& level, SchedulerP& sched)
{

  sched->setPositionVar(lb->pXLabel);
  Task* t = scinew Task("ImpMPM::actuallyInitialize",
                        this, &ImpMPM::actuallyInitialize);
  t->computes(lb->partCountLabel);
  t->computes(lb->pXLabel);
  t->computes(lb->pMassLabel);
  t->computes(lb->pVolumeLabel);
  t->computes(lb->pVelocityLabel);
  t->computes(lb->pAccelerationLabel);
  t->computes(lb->pExternalForceLabel);
  t->computes(lb->pTemperatureLabel);
  t->computes(lb->pTempPreviousLabel);
  t->computes(lb->pSizeLabel);
  t->computes(lb->pParticleIDLabel);
  t->computes(lb->pDeformationMeasureLabel);
  t->computes(lb->pStressLabel);
  t->computes(lb->pCellNAPIDLabel);
  t->computes(lb->pErosionLabel);  //  only used for imp -> exp transition
  t->computes(d_sharedState->get_delt_label());

  t->computes(lb->pExternalHeatFluxLabel);

  t->computes(lb->heatRate_CCLabel);
  if(!flags->d_doGridReset){
    t->computes(lb->gDisplacementLabel);
  }

  if (d_switchCriteria) {
    d_switchCriteria->scheduleInitialize(level,sched);
  }

  if (flags->d_useLoadCurves) {
    // Computes the load curve ID associated with each particle
    t->computes(lb->pLoadCurveIDLabel);
  }

  t->computes(lb->NC_CCweightLabel, one_matl);

  LoadBalancer* loadbal = sched->getLoadBalancer();
  d_perproc_patches = loadbal->getPerProcessorPatchSet(level);
  d_perproc_patches->addReference();

  sched->addTask(t, d_perproc_patches, d_sharedState->allMPMMaterials());

  t = scinew Task("ImpMPM::printParticleCount",
                  this, &ImpMPM::printParticleCount);
  t->requires(Task::NewDW, lb->partCountLabel);
  sched->addTask(t, d_perproc_patches, d_sharedState->allMPMMaterials());

#if 1
  if (flags->d_useLoadCurves) {
    // Schedule the initialization of HeatFlux BCs per particle
    scheduleInitializeHeatFluxBCs(level, sched);
  }
#endif
  
}


void ImpMPM::countMaterialPointsPerLoadCurve(const ProcessorGroup*,
                                             const PatchSubset* patches,
                                             const MaterialSubset*,
                                             DataWarehouse* ,
                                             DataWarehouse* new_dw)
{
  // Find the number of pressure BCs in the problem
  int nofHeatFluxBCs = 0;
  for (int ii = 0; ii<(int)MPMPhysicalBCFactory::mpmPhysicalBCs.size(); ii++){
    string bcs_type = MPMPhysicalBCFactory::mpmPhysicalBCs[ii]->getType();
    if (bcs_type == "HeatFlux" || "ArchesHeatFlux" ) {
      nofHeatFluxBCs++;
      cout << "nofHeatFluxBCs = " << nofHeatFluxBCs << endl;

      // Loop through the patches and count
      for(int p=0;p<patches->size();p++){
        const Patch* patch = patches->get(p);
        int numMPMMatls=d_sharedState->getNumMPMMatls();
        int numPts = 0;
        for(int m = 0; m < numMPMMatls; m++){
          MPMMaterial* mpm_matl = d_sharedState->getMPMMaterial( m );
          int dwi = mpm_matl->getDWIndex();

          ParticleSubset* pset = new_dw->getParticleSubset(dwi, patch);
          constParticleVariable<int> pLoadCurveID;
          new_dw->get(pLoadCurveID, lb->pLoadCurveIDLabel, pset);

          ParticleSubset::iterator iter = pset->begin();
          for(;iter != pset->end(); iter++){
            particleIndex idx = *iter;
            if (pLoadCurveID[idx] == (nofHeatFluxBCs)) 
              ++numPts;
          }
        } // matl loop
        cout << "numPts found = " << numPts << endl;
        new_dw->put(sumlong_vartype(numPts), 
                    lb->materialPointsPerLoadCurveLabel, 0, nofHeatFluxBCs-1);
      }  // patch loop
    }
  }
}

// Calculate the number of material points per load curve
void ImpMPM::initializeHeatFluxBC(const ProcessorGroup*,
                                  const PatchSubset* patches,
                                  const MaterialSubset*,
                                  DataWarehouse* ,
                                  DataWarehouse* new_dw)
{
  // Get the current time
  double time = 0.0;

  // Calculate the heat flux at each particle
  int nofHeatFluxBCs = 0;
  for (int ii = 0; ii<(int)MPMPhysicalBCFactory::mpmPhysicalBCs.size(); ii++) {
    string bcs_type = MPMPhysicalBCFactory::mpmPhysicalBCs[ii]->getType();
    if (bcs_type == "HeatFlux" || "ArchesHeatFlux" ) {

      // Get the material points per load curve
      sumlong_vartype numPart = 0;
      new_dw->get(numPart, lb->materialPointsPerLoadCurveLabel,
                  0, nofHeatFluxBCs++);

      double fluxPerPart = 0.;
      // Save the material points per load curve in the HeatFluxBC object
      HeatFluxBC* phf;
      if (bcs_type == "HeatFlux") {
        phf 
          = dynamic_cast<HeatFluxBC*>(MPMPhysicalBCFactory::mpmPhysicalBCs[ii]);
        cout << "numPart = " << numPart << endl;
        phf->numMaterialPoints(numPart);
      // Calculate the force per particle at t = 0.0
        fluxPerPart = phf->fluxPerParticle(time);
        cout << "fluxPerPart = " << fluxPerPart << endl;
      }
      
      ArchesHeatFluxBC* pahf;
      if (bcs_type == "ArchesHeatFlux") {
        pahf = 
          dynamic_cast<ArchesHeatFluxBC*>(MPMPhysicalBCFactory::mpmPhysicalBCs[ii]);
        cout << "numPart = " << numPart << endl;
        pahf->numMaterialPoints(numPart);
      // Calculate the force per particle at t = 0.0
        fluxPerPart = pahf->fluxPerParticle(time);
        cout << "fluxPerPart = " << fluxPerPart << endl;
      }


#if 0
      // Calculate the force per particle at t = 0.0
      double fluxPerPart = pbc->fluxPerParticle(time);
      cout << "fluxPerPart = " << fluxPerPart << endl;
#endif

      // Loop through the patches and calculate the force vector
      // at each particle
      for(int p=0;p<patches->size();p++){
        const Patch* patch = patches->get(p);
        int numMPMMatls=d_sharedState->getNumMPMMatls();
        for(int m = 0; m < numMPMMatls; m++){
          MPMMaterial* mpm_matl = d_sharedState->getMPMMaterial( m );
          int dwi = mpm_matl->getDWIndex();

          ParticleSubset* pset = new_dw->getParticleSubset(dwi, patch);
          constParticleVariable<Point>  px;
          new_dw->get(px, lb->pXLabel,             pset);
          constParticleVariable<int> pLoadCurveID;
          new_dw->get(pLoadCurveID, lb->pLoadCurveIDLabel, pset);
          ParticleVariable<double> pExternalHeatFlux;
          new_dw->getModifiable(pExternalHeatFlux, lb->pExternalHeatFluxLabel, 
                                pset);

          ParticleSubset::iterator iter = pset->begin();
          for(;iter != pset->end(); iter++){
            particleIndex idx = *iter;
            if (pLoadCurveID[idx] == nofHeatFluxBCs) {
              if (bcs_type == "HeatFlux")
                pExternalHeatFlux[idx] = phf->getFlux(px[idx], fluxPerPart);
              if (bcs_type == "ArchesHeatFlux") {
                pExternalHeatFlux[idx] = pahf->getFlux(px[idx], fluxPerPart);
                cout << "pExternalHeatFlux[idx] = " << pExternalHeatFlux[idx] 
                     << endl;
              }
            }
          }
        } // matl loop
      }  // patch loop
    }
  }
}

void ImpMPM::actuallyInitialize(const ProcessorGroup*,
                                const PatchSubset* patches,
                                const MaterialSubset* matls,
                                DataWarehouse*,
                                DataWarehouse* new_dw)
{
  particleIndex totalParticles=0;
  for(int p=0;p<patches->size();p++){
    const Patch* patch = patches->get(p);
    printTask(patches, patch,cout_doing,"Doing actuallyInitialize\t\t\t");

    CCVariable<short int> cellNAPID;
    new_dw->allocateAndPut(cellNAPID, lb->pCellNAPIDLabel, 0, patch);
    cellNAPID.initialize(0);

    CCVariable<double> heatFlux;
    new_dw->allocateAndPut(heatFlux,lb->heatRate_CCLabel,0,patch);
    heatFlux.initialize(1.0);

    for(int m=0;m<matls->size();m++){
      int matl = matls->get(m);
      MPMMaterial* mpm_matl = d_sharedState->getMPMMaterial( matl );
      particleIndex numParticles = mpm_matl->countParticles(patch);
      totalParticles+=numParticles;
      mpm_matl->createParticles(numParticles, cellNAPID, patch, new_dw);

      mpm_matl->getConstitutiveModel()->initializeCMData(patch,
                                                         mpm_matl, new_dw);
      if(!flags->d_doGridReset){
        int indx = mpm_matl->getDWIndex();
        NCVariable<Vector> gDisplacement;
        new_dw->allocateAndPut(gDisplacement,lb->gDisplacementLabel,indx,patch);        gDisplacement.initialize(Vector(0.));
      }
    }

   //__________________________________
   // - Initialize NC_CCweight = 0.125
   // - Find the walls with symmetry BC and double NC_CCweight
   NCVariable<double> NC_CCweight;
   new_dw->allocateAndPut(NC_CCweight, lb->NC_CCweightLabel,    0, patch);
   NC_CCweight.initialize(0.125);
   for(Patch::FaceType face = Patch::startFace; face <= Patch::endFace;
        face=Patch::nextFace(face)){
      int mat_id = 0;
      if (patch->haveBC(face,mat_id,"symmetry","Symmetric")) {
        for(CellIterator iter = patch->getFaceCellIterator(face,"NC_vars");
                                                  !iter.done(); iter++) {
          NC_CCweight[*iter] = 2.0*NC_CCweight[*iter];
        }
      }
    }
  }
  new_dw->put(sumlong_vartype(totalParticles), lb->partCountLabel);
}

void ImpMPM::scheduleComputeStableTimestep(const LevelP& lev,SchedulerP& sched)
{
  if (cout_doing.active())
    cout_doing << "ImpMPM::scheduleComputeStableTimestep " << endl;

  Task* t = scinew Task("ImpMPM::actuallyComputeStableTimestep",
                     this, &ImpMPM::actuallyComputeStableTimestep);

  const MaterialSet* matls = d_sharedState->allMPMMaterials();
  t->requires(Task::OldDW,d_sharedState->get_delt_label());
  t->requires(Task::NewDW, lb->pVelocityLabel,   Ghost::None);
  t->computes(            d_sharedState->get_delt_label());

  sched->addTask(t,lev->eachPatch(), matls);
}

void
ImpMPM::scheduleTimeAdvance( const LevelP& level, SchedulerP& sched)
{
  const MaterialSet* matls = d_sharedState->allMPMMaterials();
  if (!d_perproc_patches) {
    LoadBalancer* loadbal = sched->getLoadBalancer();
    d_perproc_patches = loadbal->getPerProcessorPatchSet(level);
    d_perproc_patches->addReference();
  }

  scheduleApplyExternalLoads(             sched, d_perproc_patches,matls);
  scheduleInterpolateParticlesToGrid(     sched, d_perproc_patches,matls);
  if (flags->d_projectHeatSource) {
    scheduleComputeCCVolume(           sched, d_perproc_patches,one_matl,matls);
    scheduleProjectCCHeatSourceToNodes(sched, d_perproc_patches,one_matl,matls);
  }

  scheduleDestroyMatrix(                  sched, d_perproc_patches,matls,false);
  scheduleCreateMatrix(                   sched, d_perproc_patches,matls);
  scheduleDestroyHCMatrix(                sched, d_perproc_patches,matls);
  scheduleCreateHCMatrix(                 sched, d_perproc_patches,matls);
  scheduleApplyBoundaryConditions(        sched, d_perproc_patches,matls);
  scheduleApplyHCBoundaryConditions(      sched, d_perproc_patches,matls);
  scheduleComputeContact(                 sched, d_perproc_patches,matls);
  scheduleFindFixedDOF(                   sched, d_perproc_patches,matls);
  scheduleFindFixedHCDOF(                 sched, d_perproc_patches,matls);
  scheduleFormHCStiffnessMatrix(          sched, d_perproc_patches,matls);
  scheduleFormHCQ(                        sched, d_perproc_patches,matls);
  scheduleAdjustHCQAndHCKForBCs(          sched, d_perproc_patches,matls);
  scheduleSolveForTemp(                   sched, d_perproc_patches,matls);
  scheduleGetTemperatureIncrement(        sched, d_perproc_patches,matls);

  scheduleIterate(                   sched,level,d_perproc_patches,matls);

  if(!flags->d_doGridReset){
    scheduleUpdateTotalDisplacement(      sched, d_perproc_patches,matls);
  }
  scheduleComputeStressTensor(            sched, d_perproc_patches,matls);
  scheduleComputeAcceleration(            sched, d_perproc_patches,matls);
  scheduleInterpolateToParticlesAndUpdate(sched, d_perproc_patches,matls);
  scheduleInterpolateStressToGrid(        sched, d_perproc_patches,matls);

  sched->scheduleParticleRelocation(level, lb->pXLabel_preReloc, 
                                    d_sharedState->d_particleState_preReloc,
                                    lb->pXLabel, 
                                    d_sharedState->d_particleState,
                                    lb->pParticleIDLabel, matls);

}

void ImpMPM::scheduleApplyExternalLoads(SchedulerP& sched,
                                        const PatchSet* patches,
                                        const MaterialSet* matls)
                                                                                
{
  Task* t=scinew Task("MPM::applyExternalLoads",
                    this, &ImpMPM::applyExternalLoads);
                                                                                
  t->requires(Task::OldDW, lb->pExternalForceLabel,    Ghost::None);
  t->requires(Task::OldDW, lb->pExternalHeatFluxLabel,    Ghost::None);
  t->requires(Task::OldDW, lb->pXLabel,                Ghost::None);
  t->computes(             lb->pExtForceLabel_preReloc);
  t->computes(             lb->pExternalHeatRateLabel);
  t->computes(             lb->pExternalHeatFluxLabel_preReloc);
  if (flags->d_useLoadCurves) {
    t->requires(Task::OldDW, lb->pLoadCurveIDLabel,    Ghost::None);
    t->computes(             lb->pLoadCurveIDLabel_preReloc);
  }

  sched->addTask(t, patches, matls);
                                                                                
}

void ImpMPM::scheduleInterpolateParticlesToGrid(SchedulerP& sched,
                                                const PatchSet* patches,
                                                const MaterialSet* matls)
{
  Task* t = scinew Task("ImpMPM::interpolateParticlesToGrid",
                        this,&ImpMPM::interpolateParticlesToGrid);
  t->requires(Task::OldDW, lb->pMassLabel,             Ghost::AroundNodes,1);
  t->requires(Task::OldDW, lb->pVolumeLabel,           Ghost::AroundNodes,1);
  t->requires(Task::OldDW, lb->pAccelerationLabel,     Ghost::AroundNodes,1);
  t->requires(Task::OldDW, lb->pVelocityLabel,         Ghost::AroundNodes,1);
  t->requires(Task::OldDW, lb->pTemperatureLabel,      Ghost::AroundNodes,1);
  t->requires(Task::OldDW, lb->pXLabel,                Ghost::AroundNodes,1);
  t->requires(Task::NewDW, lb->pExtForceLabel_preReloc,Ghost::AroundNodes,1);
  t->requires(Task::NewDW, lb->pExternalHeatRateLabel, Ghost::AroundNodes,1);
  t->requires(Task::NewDW, lb->pExternalHeatFluxLabel_preReloc, 
              Ghost::AroundNodes,1);
  if(!flags->d_doGridReset){
    t->requires(Task::OldDW,lb->gDisplacementLabel,    Ghost::None);
    t->computes(lb->gDisplacementLabel);
  }

  t->computes(lb->gMassLabel,        d_sharedState->getAllInOneMatl(),
              Task::OutOfDomain);
  t->computes(lb->gVolumeLabel,      d_sharedState->getAllInOneMatl(),
              Task::OutOfDomain);

  t->computes(lb->gMassLabel);
  t->computes(lb->gMassAllLabel);
  t->computes(lb->gVolumeLabel);
  t->computes(lb->gVelocityOldLabel);
  t->computes(lb->gVelocityLabel);
  t->computes(lb->dispNewLabel);
  t->computes(lb->gAccelerationLabel);
  t->computes(lb->gExternalForceLabel);
  t->computes(lb->gInternalForceLabel);
  t->computes(lb->TotalMassLabel);
  t->computes(lb->gTemperatureLabel,one_matl);
  t->computes(lb->gTemperatureNoBCLabel);
  t->computes(lb->gExternalHeatRateLabel);
  t->computes(lb->gExternalHeatFluxLabel);

  sched->addTask(t, patches, matls);
}

void ImpMPM::scheduleProjectCCHeatSourceToNodes(SchedulerP& sched,
                                                const PatchSet* patches,
                                                const MaterialSubset* one_matl,
                                                const MaterialSet* matls)
{
  Task* t = scinew Task("ImpMPM::projectCCHeatSourceToNodes",
                        this,&ImpMPM::projectCCHeatSourceToNodes);

  t->requires(Task::OldDW,lb->NC_CCweightLabel,one_matl,Ghost::AroundCells,1);
  t->requires(Task::NewDW,lb->gVolumeLabel,             Ghost::AroundCells,1);
  t->requires(Task::NewDW,lb->cVolumeLabel,             Ghost::AroundCells,1);
  t->requires(Task::OldDW,lb->heatRate_CCLabel,         Ghost::AroundCells,1);

  t->computes(lb->heatRate_CCLabel);
  t->modifies(lb->gExternalHeatRateLabel);
  t->computes(lb->NC_CCweightLabel, one_matl);

  sched->addTask(t, patches, matls);
}

void ImpMPM::scheduleComputeCCVolume(SchedulerP& sched,
                                     const PatchSet* patches,
                                     const MaterialSubset* one_matl,
                                     const MaterialSet* matls)
{
  Task* t = scinew Task("ImpMPM::computeCCVolume",
                        this,&ImpMPM::computeCCVolume);
                                                                                
  t->requires(Task::OldDW,lb->NC_CCweightLabel,one_matl,Ghost::AroundCells,1);
  t->requires(Task::NewDW,lb->gVolumeLabel,             Ghost::AroundCells,1);

  t->computes(lb->cVolumeLabel);

  sched->addTask(t, patches, matls);
}

void ImpMPM::scheduleComputeHeatExchange(SchedulerP& sched,
                                         const PatchSet* patches,
                                         const MaterialSet* matls)
{
  /* computeHeatExchange
   *   in(G.MASS, G.TEMPERATURE, G.EXTERNAL_HEAT_RATE)
   *   operation(peform heat exchange which will cause each of
   *   velocity fields to exchange heat according to 
   *   the temperature differences)
   *   out(G.EXTERNAL_HEAT_RATE) */

  if (cout_doing.active())
    cout_doing << getpid() << " Doing MPM::ThermalContact::computeHeatExchange " << endl;

  Task* t = scinew Task("ThermalContact::computeHeatExchange",
                        thermalContactModel,
                        &ThermalContact::computeHeatExchange);

  thermalContactModel->addComputesAndRequires(t, patches, matls);
  sched->addTask(t, patches, matls);
}


void ImpMPM::scheduleDestroyMatrix(SchedulerP& sched,
                                   const PatchSet* patches,
                                   const MaterialSet* matls,
                                   bool recursion)
{
  Task* t = scinew Task("ImpMPM::destroyMatrix",this,&ImpMPM::destroyMatrix,
                         recursion);

  sched->addTask(t, patches, matls);
}

void ImpMPM::scheduleDestroyHCMatrix(SchedulerP& sched,
                                     const PatchSet* patches,
                                     const MaterialSet* matls)
{
  heatConductionModel->scheduleDestroyHCMatrix(sched,patches,matls);
}

void ImpMPM::scheduleCreateMatrix(SchedulerP& sched,
                                  const PatchSet* patches,
                                  const MaterialSet* matls)
{
  Task* t = scinew Task("ImpMPM::createMatrix",this,&ImpMPM::createMatrix);

  t->requires(Task::OldDW, lb->pXLabel,Ghost::AroundNodes,1);

  sched->addTask(t, patches, matls);
}

void ImpMPM::scheduleCreateHCMatrix(SchedulerP& sched,
                                    const PatchSet* patches,
                                    const MaterialSet* matls)
{
  heatConductionModel->scheduleCreateHCMatrix(sched,patches,matls);
}

void ImpMPM::scheduleApplyBoundaryConditions(SchedulerP& sched,
                                             const PatchSet* patches,
                                             const MaterialSet* matls)
{
  Task* t = scinew Task("ImpMPM::applyBoundaryCondition",
                        this, &ImpMPM::applyBoundaryConditions);

  t->modifies(lb->gVelocityOldLabel);
  t->modifies(lb->gAccelerationLabel);

  sched->addTask(t, patches, matls);
}

void ImpMPM::scheduleApplyHCBoundaryConditions(SchedulerP& sched,
                                               const PatchSet* patches,
                                               const MaterialSet* matls)
{
  heatConductionModel->scheduleApplyHCBoundaryConditions(sched,patches,matls);
}

void ImpMPM::scheduleComputeContact(SchedulerP& sched,
                                    const PatchSet* patches,
                                    const MaterialSet* matls)
{
  Task* t = scinew Task("ImpMPM::computeContact",
                         this, &ImpMPM::computeContact);

  t->requires(Task::OldDW,d_sharedState->get_delt_label());
  if(d_rigid_body){
    t->modifies(lb->dispNewLabel);
    t->requires(Task::NewDW,lb->gMassLabel,        Ghost::None);
    t->requires(Task::NewDW,lb->gVelocityOldLabel, Ghost::None);
    if(!flags->d_doGridReset){
      t->requires(Task::OldDW,lb->gDisplacementLabel, Ghost::None);
      t->modifies(lb->gDisplacementLabel);
    }
  }
  t->computes(lb->gContactLabel);  

  sched->addTask(t, patches, matls);
}

void ImpMPM::scheduleFindFixedDOF(SchedulerP& sched,
                                  const PatchSet* patches,
                                  const MaterialSet* matls)
{
  Task* t = scinew Task("ImpMPM::findFixedDOF", this, 
                        &ImpMPM::findFixedDOF);

  t->requires(Task::NewDW, lb->gMassAllLabel, Ghost::None, 0);
  t->requires(Task::NewDW, lb->gContactLabel, Ghost::None, 0);

  sched->addTask(t, patches, matls);
}

void ImpMPM::scheduleFindFixedHCDOF(SchedulerP& sched,
                                    const PatchSet* patches,
                                    const MaterialSet* matls)
{
  heatConductionModel->scheduleFindFixedHCDOF(sched,patches,matls);
}

void ImpMPM::scheduleComputeStressTensor(SchedulerP& sched,
                                         const PatchSet* patches,
                                         const MaterialSet* matls,
                                         bool recursion)
{
  int numMatls = d_sharedState->getNumMPMMatls();
  Task* t = scinew Task("ImpMPM::computeStressTensor",
                    this, &ImpMPM::computeStressTensor,recursion);

  for(int m = 0; m < numMatls; m++){
    MPMMaterial* mpm_matl = d_sharedState->getMPMMaterial(m);
    ConstitutiveModel* cm = mpm_matl->getConstitutiveModel();
    cm->addComputesAndRequires(t, mpm_matl, patches,recursion);
  }
  sched->addTask(t, patches, matls);
}

void ImpMPM::scheduleFormStiffnessMatrix(SchedulerP& sched,
                                         const PatchSet* patches,
                                         const MaterialSet* matls)
{
  Task* t = scinew Task("ImpMPM::formStiffnessMatrix",
                    this, &ImpMPM::formStiffnessMatrix);

  t->requires(Task::ParentNewDW,lb->gMassAllLabel, Ghost::None);
  t->requires(Task::ParentOldDW,d_sharedState->get_delt_label());

  sched->addTask(t, patches, matls);
}

void ImpMPM::scheduleFormHCStiffnessMatrix(SchedulerP& sched,
                                           const PatchSet* patches,
                                           const MaterialSet* matls)
{
  heatConductionModel->scheduleFormHCStiffnessMatrix(sched,patches,matls);
}

void ImpMPM::scheduleComputeInternalForce(SchedulerP& sched,
                                          const PatchSet* patches,
                                          const MaterialSet* matls)
{
  Task* t = scinew Task("ImpMPM::computeInternalForce",
                         this, &ImpMPM::computeInternalForce);

  t->requires(Task::ParentOldDW,lb->pXLabel,              Ghost::AroundNodes,1);
  t->requires(Task::NewDW,      lb->pStressLabel_preReloc,Ghost::AroundNodes,1);
  t->requires(Task::NewDW,      lb->pVolumeDeformedLabel, Ghost::AroundNodes,1);

  t->computes(lb->gInternalForceLabel);

  sched->addTask(t, patches, matls);
}

void ImpMPM::scheduleFormQ(SchedulerP& sched,const PatchSet* patches,
                           const MaterialSet* matls)
{
  Task* t = scinew Task("ImpMPM::formQ", this, 
                        &ImpMPM::formQ);

  Ghost::GhostType  gnone = Ghost::None;

  t->requires(Task::ParentOldDW,d_sharedState->get_delt_label());
  t->requires(Task::NewDW,      lb->gInternalForceLabel,gnone,0);
  t->requires(Task::ParentNewDW,lb->gExternalForceLabel,gnone,0);
  t->requires(Task::OldDW,      lb->dispNewLabel,       gnone,0);
  t->requires(Task::ParentNewDW,lb->gVelocityOldLabel,  gnone,0);
  t->requires(Task::ParentNewDW,lb->gAccelerationLabel, gnone,0);
  t->requires(Task::ParentNewDW,lb->gMassAllLabel,      gnone,0);
  
  sched->addTask(t, patches, matls);
}

void ImpMPM::scheduleFormHCQ(SchedulerP& sched,const PatchSet* patches,
                             const MaterialSet* matls)
{
  heatConductionModel->scheduleFormHCQ(sched,patches,matls);
}

void ImpMPM::scheduleAdjustHCQAndHCKForBCs(SchedulerP& sched,
                                           const PatchSet* patches,
                                           const MaterialSet* matls)
{
  heatConductionModel->scheduleAdjustHCQAndHCKForBCs(sched,patches,matls);
}

void ImpMPM::scheduleSolveForDuCG(SchedulerP& sched,
                                  const PatchSet* patches,
                                  const MaterialSet* matls)
{
  Task* t = scinew Task("ImpMPM::solveForDuCG", this, 
                        &ImpMPM::solveForDuCG);

  sched->addTask(t, patches, matls);
}

void ImpMPM::scheduleSolveForTemp(SchedulerP& sched,
                                  const PatchSet* patches,
                                  const MaterialSet* matls)
{
  heatConductionModel->scheduleSolveForTemp(sched,patches,matls);
}

void ImpMPM::scheduleGetDisplacementIncrement(SchedulerP& sched,
                                              const PatchSet* patches,
                                              const MaterialSet* matls)
{
  Task* t = scinew Task("ImpMPM::getDisplacementIncrement", this, 
                        &ImpMPM::getDisplacementIncrement);

  t->computes(lb->dispIncLabel);

  sched->addTask(t, patches, matls);
}

void ImpMPM::scheduleGetTemperatureIncrement(SchedulerP& sched,
                                             const PatchSet* patches,
                                             const MaterialSet* matls)
{
  heatConductionModel->scheduleGetTemperatureIncrement(sched,patches,matls);
}

void ImpMPM::scheduleUpdateGridKinematics(SchedulerP& sched,
                                          const PatchSet* patches,
                                          const MaterialSet* matls)
{
  Task* t = scinew Task("ImpMPM::updateGridKinematics", this, 
                        &ImpMPM::updateGridKinematics);

  t->requires(Task::OldDW,lb->dispNewLabel,Ghost::None,0);
  t->computes(lb->dispNewLabel);
  t->computes(lb->gVelocityLabel);
  t->requires(Task::ParentOldDW, d_sharedState->get_delt_label() );
  t->requires(Task::NewDW,      lb->dispIncLabel,         Ghost::None,0);
  t->requires(Task::ParentNewDW,lb->gVelocityOldLabel,    Ghost::None,0);
  t->requires(Task::ParentNewDW,lb->gContactLabel,        Ghost::None,0);
  if(!flags->d_doGridReset){
    t->requires(Task::ParentOldDW,lb->gDisplacementLabel, Ghost::None,0);
    t->computes(lb->gDisplacementLabel);
  }

  sched->addTask(t, patches, matls);
}

void ImpMPM::scheduleCheckConvergence(SchedulerP& sched, 
                                      const LevelP& /* level */,
                                      const PatchSet* patches,
                                      const MaterialSet* matls)
{
  Task* t = scinew Task("ImpMPM::checkConvergence", this,
                        &ImpMPM::checkConvergence);

  t->requires(Task::NewDW,lb->dispIncLabel,Ghost::None,0);
  t->requires(Task::OldDW,lb->dispIncQNorm0);
  t->requires(Task::OldDW,lb->dispIncNormMax);

  t->computes(lb->dispIncNormMax);
  t->computes(lb->dispIncQNorm0);
  t->computes(lb->dispIncNorm);
  t->computes(lb->dispIncQNorm);

  sched->addTask(t,patches,matls);
}

void ImpMPM::scheduleIterate(SchedulerP& sched,const LevelP& level,
                             const PatchSet*, const MaterialSet*)
{
  d_recompileSubsched = true;
  Task* task = scinew Task("scheduleIterate", this, &ImpMPM::iterate,level,
                           sched.get_rep());

  task->hasSubScheduler();

  task->requires(Task::OldDW,lb->pXLabel,                 Ghost::None,0);
  task->requires(Task::OldDW,lb->pVolumeLabel,            Ghost::None,0);
  task->requires(Task::OldDW,lb->pDeformationMeasureLabel,Ghost::None,0);

  task->modifies(lb->dispNewLabel);
  task->modifies(lb->gVelocityLabel);
  task->modifies(lb->gInternalForceLabel);
  if(!flags->d_doGridReset){
    task->requires(Task::OldDW,lb->gDisplacementLabel, Ghost::None,0);
    task->modifies(lb->gDisplacementLabel);
  }

  task->requires(Task::NewDW,lb->gVelocityOldLabel,    Ghost::None,0);
  task->requires(Task::NewDW,lb->gMassAllLabel,        Ghost::None,0);
  task->requires(Task::NewDW,lb->gExternalForceLabel,  Ghost::None,0);
  task->requires(Task::NewDW,lb->gAccelerationLabel,   Ghost::None,0);
  task->requires(Task::NewDW,lb->gContactLabel,        Ghost::None,0);

  task->requires(Task::OldDW,d_sharedState->get_delt_label());

  sched->addTask(task,d_perproc_patches,d_sharedState->allMaterials());
}

void ImpMPM::scheduleComputeStressTensor(SchedulerP& sched,
                                         const PatchSet* patches,
                                         const MaterialSet* matls)
{
  int numMatls = d_sharedState->getNumMPMMatls();
  Task* t = scinew Task("ImpMPM::computeStressTensor",
                    this, &ImpMPM::computeStressTensor);

  for(int m = 0; m < numMatls; m++){
    MPMMaterial* mpm_matl = d_sharedState->getMPMMaterial(m);
    ConstitutiveModel* cm = mpm_matl->getConstitutiveModel();
    cm->addComputesAndRequires(t, mpm_matl, patches);
  }
  sched->addTask(t, patches, matls);
}

void ImpMPM::scheduleUpdateTotalDisplacement(SchedulerP& sched,
                                             const PatchSet* patches,
                                             const MaterialSet* matls)
{
  if(!flags->d_doGridReset){
    Task* t = scinew Task("ImpMPM::updateTotalDisplacement",
                              this, &ImpMPM::updateTotalDisplacement);

    t->requires(Task::OldDW, lb->gDisplacementLabel, Ghost::None);
    t->modifies(lb->gDisplacementLabel);

    sched->addTask(t, patches, matls);
  }
}

void ImpMPM::scheduleComputeAcceleration(SchedulerP& sched,
                                         const PatchSet* patches,
                                         const MaterialSet* matls)
{
  Task* t = scinew Task("ImpMPM::computeAcceleration",
                            this, &ImpMPM::computeAcceleration);

  t->requires(Task::OldDW, d_sharedState->get_delt_label() );

  t->modifies(lb->gAccelerationLabel);
  t->requires(Task::NewDW, lb->gVelocityOldLabel,Ghost::None);
  t->requires(Task::NewDW, lb->dispNewLabel,     Ghost::None);
  if(!flags->d_doGridReset){
    t->requires(Task::OldDW, lb->gDisplacementLabel, Ghost::None);
    t->modifies(lb->gDisplacementLabel);
  }

  sched->addTask(t, patches, matls);
}

void ImpMPM::scheduleInterpolateToParticlesAndUpdate(SchedulerP& sched,
                                                     const PatchSet* patches,
                                                     const MaterialSet* matls)
{
  Task* t=scinew Task("ImpMPM::interpolateToParticlesAndUpdate",
                    this, &ImpMPM::interpolateToParticlesAndUpdate);

  t->requires(Task::OldDW, d_sharedState->get_delt_label() );

  t->requires(Task::NewDW, lb->gAccelerationLabel,     Ghost::AroundCells,1);
  t->requires(Task::NewDW, lb->dispNewLabel,           Ghost::AroundCells,1);
  t->requires(Task::OldDW, lb->pXLabel,                Ghost::None);
  t->requires(Task::OldDW, lb->pExternalForceLabel,    Ghost::None);
  t->requires(Task::OldDW, lb->pMassLabel,             Ghost::None);
  t->requires(Task::OldDW, lb->pParticleIDLabel,       Ghost::None);
  t->requires(Task::OldDW, lb->pVelocityLabel,         Ghost::None);
  t->requires(Task::OldDW, lb->pAccelerationLabel,     Ghost::None);
  t->requires(Task::OldDW, lb->pMassLabel,             Ghost::None);
  t->requires(Task::NewDW, lb->pVolumeDeformedLabel,   Ghost::None);
  t->requires(Task::OldDW, lb->pTemperatureLabel,      Ghost::None);
  t->requires(Task::OldDW, lb->pTempPreviousLabel,     Ghost::None);
  t->requires(Task::OldDW, lb->pDispLabel,             Ghost::None);
  t->requires(Task::OldDW, lb->pSizeLabel,             Ghost::None);
  t->requires(Task::OldDW, lb->pErosionLabel,          Ghost::None);
  t->requires(Task::NewDW, lb->gTemperatureRateLabel,one_matl,
              Ghost::AroundCells,1);
  t->requires(Task::NewDW, lb->gTemperatureLabel,one_matl, 
              Ghost::AroundCells,1);
  t->requires(Task::NewDW, lb->gTemperatureNoBCLabel,  Ghost::AroundCells,1);

  t->computes(lb->pVelocityLabel_preReloc);
  t->computes(lb->pAccelerationLabel_preReloc);
  t->computes(lb->pXLabel_preReloc);
  t->computes(lb->pXXLabel);
  t->computes(lb->pParticleIDLabel_preReloc);
  t->computes(lb->pMassLabel_preReloc);
  t->computes(lb->pVolumeLabel_preReloc);
  t->computes(lb->pTemperatureLabel_preReloc);
  t->computes(lb->pDispLabel_preReloc);
  t->computes(lb->pSizeLabel_preReloc);
  t->computes(lb->pErosionLabel_preReloc);
  t->computes(lb->pTempPreviousLabel_preReloc);

  t->computes(lb->KineticEnergyLabel);
  t->computes(lb->CenterOfMassPositionLabel);
  t->computes(lb->CenterOfMassVelocityLabel);
  t->computes(lb->ThermalEnergyLabel);
  sched->addTask(t, patches, matls);
}
void ImpMPM::scheduleInterpolateStressToGrid(SchedulerP& sched,
                                            const PatchSet* patches,
                                            const MaterialSet* matls)
{
  Task* t=scinew Task("ImpMPM::interpolateStressToGrid",
                    this, &ImpMPM::interpolateStressToGrid);

  // This task is done for visualization only

  t->requires(Task::OldDW,lb->pXLabel,              Ghost::AroundNodes,1);
  t->requires(Task::NewDW,lb->pVolumeDeformedLabel, Ghost::AroundNodes,1);
  t->requires(Task::NewDW,lb->pStressLabel_preReloc,Ghost::AroundNodes,1);
  t->requires(Task::NewDW,lb->gVolumeLabel,         Ghost::None);
  t->requires(Task::NewDW,lb->gVolumeLabel,d_sharedState->getAllInOneMatl(),
                                 Task::OutOfDomain, Ghost::None);
  t->modifies(lb->gInternalForceLabel);
  t->computes(lb->gStressForSavingLabel);

  if (!d_bndy_traction_faces.empty()) {
    for(std::list<Patch::FaceType>::const_iterator ftit(d_bndy_traction_faces.begin()); ftit!=d_bndy_traction_faces.end();ftit++) {
      t->computes(lb->BndyForceLabel[*ftit]);       // node based
      t->computes(lb->BndyContactAreaLabel[*ftit]); // node based
    }
                                                                                
  }
  sched->addTask(t, patches, matls);
}

void ImpMPM::scheduleSwitchTest(const LevelP& level, SchedulerP& sched)
{
  if (d_switchCriteria) {
    d_switchCriteria->scheduleSwitchTest(level,sched);
  }
}


void ImpMPM::iterate(const ProcessorGroup*,
                     const PatchSubset* patches,
                     const MaterialSubset*,
                     DataWarehouse* old_dw, DataWarehouse* new_dw,
                     LevelP level, Scheduler* sched)
{
  DataWarehouse::ScrubMode old_dw_scrubmode =
                           old_dw->setScrubbing(DataWarehouse::ScrubNone);
  DataWarehouse::ScrubMode new_dw_scrubmode =
                           new_dw->setScrubbing(DataWarehouse::ScrubNone);

  GridP grid = level->getGrid();
  d_subsched->setParentDWs(old_dw, new_dw);
  d_subsched->advanceDataWarehouse(grid);
  const MaterialSet* matls = d_sharedState->allMPMMaterials();

  if (d_recompileSubsched) {
    d_subsched->initialize(3, 1);
    // Create the tasks
    
    // This task only zeros out the stiffness matrix it doesn't free any memory.
    scheduleDestroyMatrix(           d_subsched,level->eachPatch(),matls,true);
    
    if (flags->d_doMechanics) {
      scheduleComputeStressTensor(   d_subsched,level->eachPatch(),matls,true);
      scheduleFormStiffnessMatrix(   d_subsched,level->eachPatch(),matls);
      scheduleComputeInternalForce(  d_subsched,level->eachPatch(),matls);
      scheduleFormQ(                 d_subsched,level->eachPatch(),matls);
      scheduleSolveForDuCG(          d_subsched,d_perproc_patches, matls);
    }
    
    scheduleGetDisplacementIncrement(d_subsched,      level->eachPatch(),matls);
    scheduleUpdateGridKinematics(    d_subsched,      level->eachPatch(),matls);
    scheduleCheckConvergence(        d_subsched,level,level->eachPatch(),matls);
    
    d_subsched->compile();
    d_recompileSubsched = false;  
  }
  int count = 0;
  bool dispInc = false;
  bool dispIncQ = false;
  sum_vartype dispIncQNorm,dispIncNorm,dispIncQNorm0,dispIncNormMax;

  // Get all of the required particle data that is in the old_dw and put it 
  // in the subscheduler's  new_dw.  Then once dw is advanced, subscheduler
  // will be pulling data out of the old_dw.

  for (int p=0;p<patches->size();p++) {
    const Patch* patch = patches->get(p);
    if (cout_doing.active()) {
      cout_doing <<"Doing iterate on patch " << patch->getID()
		 <<"\t\t\t\t IMPM"<< "\n" << "\n";
    }

    for(int m = 0; m < d_sharedState->getNumMPMMatls(); m++){
      MPMMaterial* mpm_matl = d_sharedState->getMPMMaterial( m );
      int matl = mpm_matl->getDWIndex();
      ParticleSubset* pset = 
        d_subsched->get_dw(0)->getParticleSubset(matl, patch);

      delt_vartype dt;
      old_dw->get(dt,d_sharedState->get_delt_label(), patch->getLevel());
      d_subsched->get_dw(3)->put(sum_vartype(0.0),lb->dispIncQNorm0);
      d_subsched->get_dw(3)->put(sum_vartype(0.0),lb->dispIncNormMax);

      // New data to be stored in the subscheduler
      NCVariable<Vector> dispNew,newdisp;
      new_dw->getModifiable(dispNew, lb->dispNewLabel,             matl,patch);
      d_subsched->get_dw(3)->allocateAndPut(newdisp,lb->dispNewLabel,matl,patch);
      newdisp.copyData(dispNew);

      if(!flags->d_doGridReset){
        NCVariable<Vector> TDisp, TDispNew;
        new_dw->getModifiable(TDisp,   lb->gDisplacementLabel, matl,patch);
        d_subsched->get_dw(3)->allocateAndPut(TDispNew,lb->gDisplacementLabel,
                                                               matl,patch);
        TDispNew.copyData(TDisp);
      }
      d_subsched->get_dw(3)->saveParticleSubset(pset, matl, patch);

      // These variables are ultimately retrieved from the subschedulers
      // old datawarehouse after the advancement of the data warehouse.
      double new_dt;
      new_dt = dt;
      d_subsched->get_dw(3)->put(delt_vartype(new_dt),
                                  d_sharedState->get_delt_label());
    }
  }

  d_subsched->get_dw(3)->finalize();
  d_subsched->advanceDataWarehouse(grid);

  d_numIterations = 0;
  while(!(dispInc && dispIncQ)) {
    if(UintahParallelComponent::d_myworld->myrank() == 0){
     cerr << "Beginning Iteration = " << count << "\n";
    }
    count++;
    d_subsched->get_dw(2)->setScrubbing(DataWarehouse::ScrubComplete);
    d_subsched->get_dw(3)->setScrubbing(DataWarehouse::ScrubNone);
    d_subsched->execute();  // THIS ACTUALLY GETS THE WORK DONE
    d_subsched->get_dw(3)->get(dispIncNorm,   lb->dispIncNorm);
    d_subsched->get_dw(3)->get(dispIncQNorm,  lb->dispIncQNorm); 
    d_subsched->get_dw(3)->get(dispIncNormMax,lb->dispIncNormMax);
    d_subsched->get_dw(3)->get(dispIncQNorm0, lb->dispIncQNorm0);

    if(UintahParallelComponent::d_myworld->myrank() == 0){
      cerr << "dispIncNorm/dispIncNormMax = "
           << dispIncNorm/(dispIncNormMax + 1.e-100) << "\n";
      cerr << "dispIncQNorm/dispIncQNorm0 = "
           << dispIncQNorm/(dispIncQNorm0 + 1.e-100) << "\n";
    }
    if (dispIncNorm/(dispIncNormMax + 1e-100) <= flags->d_conv_crit_disp)
      dispInc = true;
    if (dispIncQNorm/(dispIncQNorm0 + 1e-100) <= flags->d_conv_crit_energy)
      dispIncQ = true;
    // Check to see if the residual is likely a nan, if so, we'll restart.
    bool restart_nan=false;
    bool restart_neg_residual=false;
    bool restart_num_iters=false;

    if ((isnan(dispIncQNorm/dispIncQNorm0)||isnan(dispIncNorm/dispIncNormMax))
        && dispIncQNorm0!=0.){
      restart_nan=true;
      cerr << "Restarting due to a nan residual" << endl;
    }
    if (dispIncQNorm/dispIncQNorm0 < 0. ||dispIncNorm/dispIncNormMax < 0.){
      restart_neg_residual=true;
      cerr << "Restarting due to a negative residual" << endl;
    }
    if (count > flags->d_max_num_iterations){
      restart_num_iters=true;
      cerr << "Restarting due to exceeding max number of iterations" << endl;
    }
    if (restart_nan || restart_neg_residual || restart_num_iters) {
      new_dw->abortTimestep();
      new_dw->restartTimestep();
      return;
    }

    d_subsched->advanceDataWarehouse(grid);
  }
  d_numIterations = count;

  // Move the particle data from subscheduler to scheduler.
  for (int p = 0; p < patches->size();p++) {
    const Patch* patch = patches->get(p);
    if (cout_doing.active()) {
      cout_doing <<"Getting the recursive data on patch " << patch->getID()
		 <<"\t\t\t IMPM"<< "\n" << "\n";
    }

    Ghost::GhostType  gnone = Ghost::None;
    for(int m = 0; m < d_sharedState->getNumMPMMatls(); m++){
      MPMMaterial* mpm_matl = d_sharedState->getMPMMaterial( m );
      int matl = mpm_matl->getDWIndex();

      // Needed in computeAcceleration 
      constNCVariable<Vector> velocity, dispNew, internalForce;
      d_subsched->get_dw(2)->get(velocity, lb->gVelocityLabel,matl,patch,gnone,0);
      d_subsched->get_dw(2)->get(dispNew,  lb->dispNewLabel,  matl,patch,gnone,0);
      if (flags->d_doMechanics) {
        d_subsched->get_dw(2)->get(internalForce,
                                    lb->gInternalForceLabel,matl,patch,gnone,0);
      }

      NCVariable<Vector> velocity_new, dispNew_new, internalForce_new;
      new_dw->getModifiable(velocity_new,lb->gVelocityLabel,      matl,patch);
      new_dw->getModifiable(dispNew_new, lb->dispNewLabel,        matl,patch);
      new_dw->getModifiable(internalForce_new,
                            lb->gInternalForceLabel,              matl,patch);
      velocity_new.copyData(velocity);
      dispNew_new.copyData(dispNew);
      if (flags->d_doMechanics) {
        internalForce_new.copyData(internalForce);
      }
    }
  }
  old_dw->setScrubbing(old_dw_scrubmode);
  new_dw->setScrubbing(new_dw_scrubmode);
}

void ImpMPM::applyExternalLoads(const ProcessorGroup* ,
                                const PatchSubset* patches,
                                const MaterialSubset*,
                                DataWarehouse* old_dw,
                                DataWarehouse* new_dw)
{
  // Get the current time
  double time = d_sharedState->getElapsedTime();

  if (cout_doing.active())
    cout_doing << "Current Time (applyExternalLoads) = " << time << endl;
                                                                                
  // Calculate the force vector at each particle for each bc
  std::vector<double> forceMagPerPart;
  std::vector<NormalForceBC*> nfbcP;
  std::vector<double> heatFluxMagPerPart;
  std::vector<HeatFluxBC*> hfbcP;
  std::vector<ArchesHeatFluxBC*> ahfbcP;
  if (flags->d_useLoadCurves) {
    // Currently, only one load curve at a time is supported, but
    // I've left the infrastructure in place to go to multiple
    for (int ii = 0;
             ii < (int)MPMPhysicalBCFactory::mpmPhysicalBCs.size(); ii++) {

      string bcs_type = MPMPhysicalBCFactory::mpmPhysicalBCs[ii]->getType();
      cout << "bcs_type = " << bcs_type << endl;
      if (bcs_type == "Pressure") {
        cerr << "Pressure BCs not supported in ImpMPM" << endl;
      }
      if (bcs_type == "NormalForce") {
        NormalForceBC* nfbc =
         dynamic_cast<NormalForceBC*>(MPMPhysicalBCFactory::mpmPhysicalBCs[ii]);
        nfbcP.push_back(nfbc);

        // Calculate the force per particle at current time
        forceMagPerPart.push_back(nfbc->getLoad(time));
      }
      if (bcs_type == "HeatFlux") {
        HeatFluxBC* hfbc =
         dynamic_cast<HeatFluxBC*>(MPMPhysicalBCFactory::mpmPhysicalBCs[ii]);
#if 0
        cout << *hfbc << endl;
        cout << "hfbc type = " << hfbc->getType() << endl;
        cout << "surface area = " << hfbc->getSurfaceArea() << endl;
        cout << "heat flux = " << hfbc->heatflux(time) << endl;
        cout << "flux per particle = " << hfbc->fluxPerParticle(time) << endl;
#endif
        hfbcP.push_back(hfbc);

        // Calculate the force per particle at current time

        heatFluxMagPerPart.push_back(hfbc->fluxPerParticle(time));
      }
      if (bcs_type == "ArchesHeatFlux"){
        ArchesHeatFluxBC* ahfbc =
         dynamic_cast<ArchesHeatFluxBC*>(MPMPhysicalBCFactory::mpmPhysicalBCs[ii]);
#if 1
        cout << *ahfbc << endl;
        cout << "ahfbc type = " << ahfbc->getType() << endl;
        cout << "surface area = " << ahfbc->getSurfaceArea() << endl;
        cout << "heat flux = " << ahfbc->heatflux(time) << endl;
        cout << "flux per particle = " << ahfbc->fluxPerParticle(time) << endl;
#endif
        ahfbcP.push_back(ahfbc);

        // Calculate the force per particle at current time

        heatFluxMagPerPart.push_back(ahfbc->fluxPerParticle(time));
      }
    }
  }
                                                                                
  // Loop thru patches to update external force vector
  for(int p=0;p<patches->size();p++){
    const Patch* patch = patches->get(p);
    printTask(patches, patch,cout_doing,"Doing applyExternalLoads\t\t\t\t");
    
    // Place for user defined loading scenarios to be defined,
    // otherwise pExternalForce is just carried forward.
    
    int numMPMMatls=d_sharedState->getNumMPMMatls();
    
    for(int m = 0; m < numMPMMatls; m++){
      MPMMaterial* mpm_matl = d_sharedState->getMPMMaterial( m );
      int dwi = mpm_matl->getDWIndex();
      ParticleSubset* pset = old_dw->getParticleSubset(dwi, patch);
                                                                                
      constParticleVariable<Vector> pExternalForce;
      ParticleVariable<Vector> pExternalForce_new;
      old_dw->get(pExternalForce, lb->pExternalForceLabel, pset);
      new_dw->allocateAndPut(pExternalForce_new,
                             lb->pExtForceLabel_preReloc,  pset);
      
      constParticleVariable<double> pExternalHeatFlux;
      ParticleVariable<double> pExternalHeatFlux_new;
      old_dw->get(pExternalHeatFlux, lb->pExternalHeatFluxLabel, pset);
      new_dw->allocateAndPut(pExternalHeatFlux_new,
                             lb->pExternalHeatFluxLabel_preReloc,pset);

      for (ParticleSubset::iterator iter = pset->begin();iter != pset->end(); 
           iter++){
        particleIndex idx = *iter;
        pExternalHeatFlux_new[idx] = 0.;
      }

      if (flags->d_useLoadCurves) {
        // Get the load curve data
        constParticleVariable<int> pLoadCurveID;
        old_dw->get(pLoadCurveID, lb->pLoadCurveIDLabel, pset);
      
        if (!forceMagPerPart.empty()) {
          double mag = forceMagPerPart[0];
          //cout << "force mag = " << mag << endl;
          // Iterate over the particles
          ParticleSubset::iterator iter = pset->begin();
          for(;iter != pset->end(); iter++){
            particleIndex idx = *iter;
            // For particles with an existing external force, apply the
            // new magnitude to the same direction.
            if(pExternalForce[idx].length() > 1.e-7){
              pExternalForce_new[idx] = mag*
                (pExternalForce[idx]/pExternalForce[idx].length());
            } else{
              pExternalForce_new[idx] = Vector(0.,0.,0.);
            }
          }
        } else {
          ParticleSubset::iterator iter = pset->begin();
          for(;iter != pset->end(); iter++){
            particleIndex idx = *iter;
            pExternalForce_new[idx] = pExternalForce[idx]
              *flags->d_forceIncrementFactor;
          }
        }
        if (!heatFluxMagPerPart.empty()) {
          double mag = heatFluxMagPerPart[0];
	  cout << "heat flux mag = " << mag << endl;
          ParticleSubset::iterator iter = pset->begin();
          for(;iter != pset->end(); iter++){
            //input the theta calculation here.
            particleIndex idx = *iter;
            int loadCurveID = pLoadCurveID[idx]-1;
            if (loadCurveID < 0) {
              pExternalHeatFlux_new[idx] = 0.;
            } else {
              //              pExternalHeatFlux_new[idx] = mag;
              pExternalHeatFlux_new[idx] = pExternalHeatFlux[idx];
            }
          }
        }
        // Recycle the loadCurveIDs
        ParticleVariable<int> pLoadCurveID_new;
        new_dw->allocateAndPut(pLoadCurveID_new, 
                               lb->pLoadCurveIDLabel_preReloc, pset);
        pLoadCurveID_new.copyData(pLoadCurveID);
      } else {
                                                                               
        // Iterate over the particles
        ParticleSubset::iterator iter = pset->begin();
        for(;iter != pset->end(); iter++){
          particleIndex idx = *iter;
          pExternalForce_new[idx] = pExternalForce[idx]
            *flags->d_forceIncrementFactor;
          pExternalHeatFlux_new[idx] = pExternalHeatFlux[idx];
        }
      }

      // Prescribe an external heat rate to some particles
      ParticleVariable<double> pExtHeatRate;
      new_dw->allocateAndPut(pExtHeatRate, lb->pExternalHeatRateLabel,  pset);
      constParticleVariable<Point> px;
      old_dw->get(px,lb->pXLabel,pset);
      
      ParticleSubset::iterator iter = pset->begin();
      for(;iter != pset->end(); iter++){
        particleIndex idx = *iter;
        pExtHeatRate[idx]=0.0;
#if 0
      if(px[idx].x()*px[idx].x() + px[idx].y()*px[idx].y() > 0.0562*0.0562 ||
         px[idx].z()>.0562 || px[idx].z()<-.0562){
          pExtHeatRate[idx]=0.001;
        }
#endif

      }

    } // matl loop
  }  // patch loop
}

void ImpMPM::projectCCHeatSourceToNodes(const ProcessorGroup*,
                                        const PatchSubset* patches,
                                        const MaterialSubset* ,
                                        DataWarehouse* old_dw,
                                        DataWarehouse* new_dw)
{
  for(int p=0;p<patches->size();p++){
    const Patch* patch = patches->get(p);
    printTask(patches,patch,cout_doing,
              "Doing projectCCHeatSourceToNodes on patch\t\t");
      
    Ghost::GhostType  gac = Ghost::AroundCells;

    constNCVariable<double> NC_CCweight;
    NCVariable<double> NC_CCweight_copy;
    old_dw->get(NC_CCweight,     lb->NC_CCweightLabel,       0, patch,gac,1);
    new_dw->allocateAndPut(NC_CCweight_copy, lb->NC_CCweightLabel, 0,patch);
    // carry forward interpolation weight
    IntVector low = patch->getNodeLowIndex();
    IntVector hi  = patch->getNodeHighIndex();
    NC_CCweight_copy.copyPatch(NC_CCweight, low,hi);

    int numMPMMatls=d_sharedState->getNumMPMMatls();

    for(int m = 0; m < numMPMMatls; m++){
      MPMMaterial* mpm_matl = d_sharedState->getMPMMaterial( m );
      int dwi = mpm_matl->getDWIndex();

      constNCVariable<double> gvolume;
      NCVariable<double> gextHR;
      constCCVariable<double> CCheatrate,cvolume;
      CCVariable<double> CCheatrate_copy;

      new_dw->get(gvolume,         lb->gVolumeLabel,          dwi, patch,gac,1);
      old_dw->get(CCheatrate,      lb->heatRate_CCLabel,      dwi, patch,gac,1);
      new_dw->get(cvolume,         lb->cVolumeLabel,          dwi, patch,gac,1);
      new_dw->getModifiable(gextHR,lb->gExternalHeatRateLabel,dwi, patch);
      new_dw->allocateAndPut(CCheatrate_copy,  lb->heatRate_CCLabel, dwi,patch);

      // carry forward heat rate.
      CCheatrate_copy.copyData(CCheatrate);

      // Project  CC heat rate to nodes
      for(NodeIterator iter = patch->getNodeIterator();!iter.done();iter++){
        IntVector n = *iter;
        IntVector cIdx[8];
        patch->findCellsFromNode(n, cIdx);
        for (int ic=0;ic<8;ic++){
          double solid_volume = cvolume[cIdx[ic]];
          gextHR[n] += CCheatrate[cIdx[ic]]*(NC_CCweight[n]*gvolume[n])
                       /solid_volume;
        }
      }

    }
  }
}

void ImpMPM::computeCCVolume(const ProcessorGroup*,
                             const PatchSubset* patches,
                             const MaterialSubset* ,
                             DataWarehouse* old_dw,
                             DataWarehouse* new_dw)
{
  for(int p=0;p<patches->size();p++){
    const Patch* patch = patches->get(p);
    if (cout_doing.active()) {
      cout_doing <<"Doing computeCCVolume on patch "
                 << patch->getID() <<"\t\t IMPM"<< "\n" << "\n";
    }
      
    Ghost::GhostType  gac = Ghost::AroundCells;

    constNCVariable<double> NC_CCweight;
    old_dw->get(NC_CCweight,     lb->NC_CCweightLabel,       0, patch,gac,1);

    int numMPMMatls=d_sharedState->getNumMPMMatls();

    for(int m = 0; m < numMPMMatls; m++){
      MPMMaterial* mpm_matl = d_sharedState->getMPMMaterial( m );
      int dwi = mpm_matl->getDWIndex();

      constNCVariable<double> gvolume;
      CCVariable<double> cvolume;

      new_dw->get(gvolume,         lb->gVolumeLabel,          dwi, patch,gac,1);
      new_dw->allocateAndPut(cvolume,      lb->cVolumeLabel, dwi,patch);
      cvolume.initialize(1.e-20);

      for(CellIterator iter =patch->getExtraCellIterator();!iter.done();iter++){
        IntVector c = *iter;
        IntVector nodeIdx[8];
        patch->findNodesFromCell(c, nodeIdx);
        for (int in=0;in<8;in++){
          cvolume[c] += NC_CCweight[nodeIdx[in]] * gvolume[nodeIdx[in]];
        }
      }
    }
  }
}

void ImpMPM::interpolateParticlesToGrid(const ProcessorGroup*,
                                        const PatchSubset* patches,
                                        const MaterialSubset* ,
                                        DataWarehouse* old_dw,
                                        DataWarehouse* new_dw)
{
  for(int p=0;p<patches->size();p++){
    const Patch* patch = patches->get(p);

    if (cout_doing.active()) {
      cout_doing <<"Doing interpolateParticlesToGrid on patch " 
		 << patch->getID() <<"\t\t IMPM"<< "\n" << "\n";
    }

    LinearInterpolator* interpolator = new LinearInterpolator(patch);
    vector<IntVector> ni(interpolator->size());
    vector<double> S(interpolator->size());

    NCVariable<double> gmassglobal,gvolumeglobal;
    new_dw->allocateAndPut(gmassglobal, lb->gMassLabel,
                           d_sharedState->getAllInOneMatl()->get(0), patch);
    new_dw->allocateAndPut(gvolumeglobal, lb->gVolumeLabel,
                           d_sharedState->getAllInOneMatl()->get(0), patch);
    gmassglobal.initialize(d_SMALL_NUM_MPM);
    gvolumeglobal.initialize(d_SMALL_NUM_MPM);

    int numMatls = d_sharedState->getNumMPMMatls();
    // Create arrays for the grid data
    NCVariable<double> gTemperature;
    StaticArray<NCVariable<double> > gmass(numMatls),gvolume(numMatls),
      gExternalHeatRate(numMatls),gExternalHeatFlux(numMatls),
      gTemperatureNoBC(numMatls),gmassall(numMatls);
    StaticArray<NCVariable<Vector> > gvel_old(numMatls),gacc(numMatls);
    StaticArray<NCVariable<Vector> > dispNew(numMatls),gvelocity(numMatls);
    StaticArray<NCVariable<Vector> > gextforce(numMatls),gintforce(numMatls);

    NCVariable<double> GMASS,GVOLUME;
    NCVariable<Vector> GVEL_OLD, GACC, GEXTFORCE;
    new_dw->allocateTemporary(GMASS,     patch,Ghost::None,0);
    new_dw->allocateTemporary(GVOLUME,   patch,Ghost::None,0);
    new_dw->allocateTemporary(GVEL_OLD,  patch,Ghost::None,0);
    new_dw->allocateTemporary(GACC,      patch,Ghost::None,0);
    new_dw->allocateTemporary(GEXTFORCE, patch,Ghost::None,0);
    GMASS.initialize(0.);
    GVOLUME.initialize(0.);
    GVEL_OLD.initialize(Vector(0.,0.,0.));
    GACC.initialize(Vector(0.,0.,0.));
    GEXTFORCE.initialize(Vector(0.,0.,0.));

    NCVariable<double> gSpecificHeat;
    new_dw->allocateTemporary(gSpecificHeat,patch,Ghost::None,0);
    gSpecificHeat.initialize(0.);

    new_dw->allocateAndPut(gTemperature,lb->gTemperatureLabel, 0,patch);
    gTemperature.initialize(0.0);

    for(int m = 0; m < numMatls; m++){
      MPMMaterial* mpm_matl = d_sharedState->getMPMMaterial( m );
      int matl = mpm_matl->getDWIndex();
      // Create arrays for the particle data
      constParticleVariable<Point>  px;
      constParticleVariable<double> pmass, pvolume, pTemperature;
      constParticleVariable<double> pextheatrate,pextheatflux;
      constParticleVariable<Vector> pvelocity, pacceleration, pexternalforce;

      ParticleSubset* pset = old_dw->getParticleSubset(matl, patch,
                                               Ghost::AroundNodes, 1,
                                               lb->pXLabel);

      old_dw->get(px,             lb->pXLabel,                 pset);
      old_dw->get(pmass,          lb->pMassLabel,              pset);
      old_dw->get(pvolume,        lb->pVolumeLabel,            pset);
      old_dw->get(pvelocity,      lb->pVelocityLabel,          pset);
      old_dw->get(pTemperature,   lb->pTemperatureLabel,       pset);
      old_dw->get(pacceleration,  lb->pAccelerationLabel,      pset);
      new_dw->get(pexternalforce, lb->pExtForceLabel_preReloc, pset);
      new_dw->get(pextheatrate,   lb->pExternalHeatRateLabel,  pset);
      new_dw->get(pextheatflux,   lb->pExternalHeatFluxLabel_preReloc,  pset);

      new_dw->allocateAndPut(gmass[m],      lb->gMassLabel,         matl,patch);
      new_dw->allocateAndPut(gmassall[m],   lb->gMassAllLabel,      matl,patch);
      new_dw->allocateAndPut(gvolume[m],    lb->gVolumeLabel,       matl,patch);
      new_dw->allocateAndPut(gvel_old[m],   lb->gVelocityOldLabel,  matl,patch);
      new_dw->allocateAndPut(gvelocity[m],  lb->gVelocityLabel,     matl,patch);
     
      new_dw->allocateAndPut(gTemperatureNoBC[m],lb->gTemperatureNoBCLabel,
                                                                    matl,patch);
      new_dw->allocateAndPut(dispNew[m],    lb->dispNewLabel,       matl,patch);
      new_dw->allocateAndPut(gacc[m],       lb->gAccelerationLabel, matl,patch);
      new_dw->allocateAndPut(gextforce[m],  lb->gExternalForceLabel,matl,patch);
      new_dw->allocateAndPut(gintforce[m],  lb->gInternalForceLabel,matl,patch);
      new_dw->allocateAndPut(gExternalHeatRate[m],lb->gExternalHeatRateLabel,
                             matl,patch);
      new_dw->allocateAndPut(gExternalHeatFlux[m],lb->gExternalHeatFluxLabel,
                             matl,patch);

      if(!flags->d_doGridReset){
        constNCVariable<Vector> TDisp;
        NCVariable<Vector> TDispNew;
        old_dw->get(TDisp,lb->gDisplacementLabel, matl, patch,Ghost::None,0);
        new_dw->allocateAndPut(TDispNew, lb->gDisplacementLabel, matl,patch);
        TDispNew.copyData(TDisp);
      }

      gmass[m].initialize(d_SMALL_NUM_MPM);
      gvolume[m].initialize(0);
      gvel_old[m].initialize(Vector(0,0,0));
      gacc[m].initialize(Vector(0,0,0));
      gextforce[m].initialize(Vector(0,0,0));

      dispNew[m].initialize(Vector(0,0,0));
      gvelocity[m].initialize(Vector(0,0,0));
      gintforce[m].initialize(Vector(0,0,0));

      gTemperatureNoBC[m].initialize(0.0);
      gExternalHeatRate[m].initialize(0.0);
      gExternalHeatFlux[m].initialize(0.0);

      double totalmass = 0;
      Vector pmom, pmassacc;

      double Cp=mpm_matl->getSpecificHeat();

      for(ParticleSubset::iterator iter = pset->begin();
          iter != pset->end(); iter++){
        particleIndex idx = *iter;

        // Get the node indices that surround the cell
        interpolator->findCellAndWeights(px[idx], ni, S);

        pmassacc    = pacceleration[idx]*pmass[idx];
        pmom        = pvelocity[idx]*pmass[idx];
        totalmass  += pmass[idx];

        // Add each particles contribution to the local mass & velocity 
        // Must use the node indices
        for(int k = 0; k < 8; k++) {
          if(patch->containsNode(ni[k])) {
            gmass[m][ni[k]]          += pmass[idx]          * S[k];
            gmassglobal[ni[k]]       += pmass[idx]          * S[k];
            gvolume[m][ni[k]]        += pvolume[idx]        * S[k];
            gvolumeglobal[ni[k]]     += pvolume[idx]        * S[k];
            gextforce[m][ni[k]]      += pexternalforce[idx] * S[k];
            gSpecificHeat[ni[k]]     += Cp                  * S[k];
            if (flags->d_temp_solve == false){
              gTemperature[ni[k]]   +=  pTemperature[idx]  * pmass[idx]* S[k];
            }
            gvel_old[m][ni[k]]       += pmom                * S[k];
            gacc[m][ni[k]]           += pmassacc            * S[k];
            gExternalHeatRate[m][ni[k]] += pextheatrate[idx]* S[k];
            gExternalHeatFlux[m][ni[k]] += pextheatflux[idx]* S[k];
          }
        }
      }

      if(mpm_matl->getIsRigid()){
        for(NodeIterator iter = patch->getNodeIterator(); !iter.done();iter++){
          IntVector c = *iter;
          gvel_old[m][c] /= gmass[m][c];
          gacc[m][c]     /= gmass[m][c];
          gTemperatureNoBC[m][c] = gTemperature[c];
        }
      }

      if(!mpm_matl->getIsRigid()){
        for(NodeIterator iter = patch->getNodeIterator(); !iter.done();iter++){
          IntVector c = *iter;
          GMASS[c]+=gmass[m][c];
          GVOLUME[c]+=gvolume[m][c];
          GEXTFORCE[c]+=gextforce[m][c];
          GVEL_OLD[c]+=gvel_old[m][c];
          GACC[c]+=gacc[m][c];
        }
      }

      new_dw->put(sum_vartype(totalmass), lb->TotalMassLabel);
    }  // End loop over materials


    // Single material temperature field
    if (flags->d_temp_solve == false) {
      for(NodeIterator iter = patch->getNodeIterator(); !iter.done();iter++){
        IntVector c = *iter;
        gTemperature[c] /= gmassglobal[c];
      }
    }


    if (flags->d_temp_solve == true) {
    // This actually solves for the grid temperatures assuming a linear
    // form of the temperature field.  Uses the particle temperatures
    // as known points within the cell.  For number of particles less
    // than the number of grid cells, take the transpose of the matrix
    // and multiply the original matrix by the transpose.  Also multiply
    // the right hand side by the transpose.  This will yield a matrix
    // that is 8 x 8 and will yield the nodal temperatures even when 
    // the number of particles is less than the 8 (number of grid nodes).
      for (int m = 0; m < numMatls; m++) {
        MPMMaterial* mpm_matl = d_sharedState->getMPMMaterial(m);
        int matl = mpm_matl->getDWIndex();
        
        // Create arrays for the particle data
        constParticleVariable<Point>  px;
        constParticleVariable<double> pTemperature;
        
        ParticleSubset* pset = old_dw->getParticleSubset(matl, patch,
                                                         Ghost::AroundNodes, 1,
                                                         lb->pXLabel);
        
        old_dw->get(px,             lb->pXLabel,                 pset);
        old_dw->get(pTemperature,   lb->pTemperatureLabel,       pset);
        
        
        FastMatrix A(8,8);
        double B[8];
        
        ParticleSubset::iterator iter = pset->begin();
        
        
        vector<IntVector> ni_cell(interpolator->size());
        if (iter != pset->end())
          interpolator->findCellAndWeights(px[*iter], ni_cell, S);
        
        int numParticles = 0;
        
        while (iter != pset->end() ) {
          interpolator->findCellAndWeights(px[*iter], ni, S);
          A.zero();
          for (int i = 0; i<8;i++) B[i] = 0.;
          
          while(ni_cell[0] == ni[0] && iter != pset->end()) {
#if 0
            cout << "numParticles = " << numParticles << endl;
            cout << "px[" << *iter << "]= " << px[*iter] << endl;
#endif
            for (int j = 0; j < 8; j++) {
#if 0
              cout << "Filling A[" << numParticles << "][" << j << "]" << endl;
#endif
              A(numParticles,j) = S[j];
            }
#if 0
            cout << "pTemperature[" << *iter << "] = " << pTemperature[*iter] 
                 << endl;
#endif
            B[numParticles] = pTemperature[*iter];
            iter++;
            numParticles++;
#if 0
            cout << "*iter = " << *iter << " numParticles = " << numParticles 
                 << endl;
#endif
            if (iter != pset->end())
              interpolator->findCellAndWeights(px[*iter], ni, S);
          }
          
          if (numParticles < 8) {
            FastMatrix A_t(8,8);
            A_t.transpose(A);
            A.print(cout);
            A_t.print(cout);
            double A_tB_t[8];
            A_t.multiply(B,A_tB_t);
            FastMatrix A_tA(8,8);
            A_tA.multiply(A_t,A);
            A_tA.print(cout);
            cout << "condition number = " << A_tA.conditionNumber() << endl;
            A_tA.destructiveSolve(A_tB_t);
            
          } else {
            A.destructiveSolve(B);
          }
          for (int i = 0; i < 8; i++) {
            if(patch->containsNode(ni_cell[i])) {
              gTemperature[ni_cell[i]] = B[i];
#if 0
              cout << "gTemperature[" << ni_cell[i] << "]= " 
                   << gTemperature[ni_cell[i]]  << endl;
#endif
            }
          }
          
          numParticles = 0;
          copy(ni.begin(),ni.end(),ni_cell.begin());
          
        }
      }
    }

    for(int m = 0; m < numMatls; m++){
      MPMMaterial* mpm_matl = d_sharedState->getMPMMaterial( m );
      if(!mpm_matl->getIsRigid()){
        for(NodeIterator iter = patch->getNodeIterator(); !iter.done();iter++){
          IntVector c = *iter;
          gTemperatureNoBC[m][c] = gTemperature[c];
          gmassall[m][c]=GMASS[c];
          gvolume[m][c]=GVOLUME[c];
          gextforce[m][c]=GEXTFORCE[c];
          gvel_old[m][c]=GVEL_OLD[c]/(GMASS[c] + 1.e-200);
          gacc[m][c]=GACC[c]/(GMASS[c] + 1.e-200);
        }
      }
    }  // End loop over materials

    delete interpolator;
  }  // End loop over patches
}

void ImpMPM::destroyMatrix(const ProcessorGroup*,
                           const PatchSubset* /*patches*/,
                           const MaterialSubset* ,
                           DataWarehouse* /* old_dw */,
                           DataWarehouse* /* new_dw */,
                           bool recursion)
{
  if (cout_doing.active())
    cout_doing <<"Doing destroyMatrix " <<"\t\t\t\t\t IMPM" << "\n" << "\n";

  d_solver->destroyMatrix(recursion);
}

void ImpMPM::createMatrix(const ProcessorGroup*,
                          const PatchSubset* patches,
                          const MaterialSubset* ,
                          DataWarehouse* old_dw,
                          DataWarehouse* new_dw)
{
  map<int,int> dof_diag;
  for(int pp=0;pp<patches->size();pp++){
    const Patch* patch = patches->get(pp);
    if (cout_doing.active()) {
      cout_doing <<"Doing createMatrix on patch " << patch->getID() 
		 << "\t\t\t\t IMPM"    << "\n" << "\n";
    }

    d_solver->createLocalToGlobalMapping(UintahParallelComponent::d_myworld,d_perproc_patches,patches,3);
    
    IntVector lowIndex = patch->getInteriorNodeLowIndex();
    IntVector highIndex = patch->getInteriorNodeHighIndex()+IntVector(1,1,1);

    Array3<int> l2g(lowIndex,highIndex);
    d_solver->copyL2G(l2g,patch);

    CCVariable<int> visited;
    new_dw->allocateTemporary(visited,patch,Ghost::AroundCells,1);
    visited.initialize(0);

    int numMatls = d_sharedState->getNumMPMMatls();
    for (int m = 0; m < numMatls; m++){
      MPMMaterial* mpm_matl = d_sharedState->getMPMMaterial( m );
      int dwi = mpm_matl->getDWIndex();    
      constParticleVariable<Point> px;
      ParticleSubset* pset;

      pset = old_dw->getParticleSubset(dwi,patch, Ghost::AroundNodes,1,
                                                          lb->pXLabel);
      old_dw->get(px,lb->pXLabel,pset);

      for(ParticleSubset::iterator iter = pset->begin();
          iter != pset->end(); iter++){
        particleIndex idx = *iter;
        IntVector cell,ni[8];
        patch->findCell(px[idx],cell);
        if (visited[cell] == 0 ) {
          visited[cell] = 1;
          patch->findNodesFromCell(cell,ni);
          vector<int> dof(0);
          int l2g_node_num;
          for (int k = 0; k < 8; k++) {
            if (patch->containsNode(ni[k]) ) {
              l2g_node_num = l2g[ni[k]] - l2g[lowIndex];
              dof.push_back(l2g_node_num);
              dof.push_back(l2g_node_num+1);
              dof.push_back(l2g_node_num+2);
            }
          }
          for (int I = 0; I < (int) dof.size(); I++) {
            int dofi = dof[I];
            for (int J = 0; J < (int) dof.size(); J++) {
              dof_diag[dofi] += 1;
            }
          }
         }
       }
     } 
  }
  d_solver->createMatrix(UintahParallelComponent::d_myworld,dof_diag);
}

void ImpMPM::applyBoundaryConditions(const ProcessorGroup*,
                                     const PatchSubset* patches,
                                     const MaterialSubset* ,
                                     DataWarehouse* /*old_dw*/,
                                     DataWarehouse* new_dw)
{
  for (int p = 0; p < patches->size(); p++) {
    const Patch* patch = patches->get(p);

    printTask(patches,patch,cout_doing,
              "Doing applyBoundaryConditions\t\t\t\t");

    IntVector lowIndex = patch->getInteriorNodeLowIndex();
    IntVector highIndex = patch->getInteriorNodeHighIndex()+IntVector(1,1,1);
    Array3<int> l2g(lowIndex,highIndex);
    d_solver->copyL2G(l2g,patch);

    // Apply grid boundary conditions to the velocity before storing the data
    IntVector offset =  IntVector(0,0,0);
    for (int m = 0; m < d_sharedState->getNumMPMMatls(); m++ ) {
      MPMMaterial* mpm_matl = d_sharedState->getMPMMaterial( m );
      int matl = mpm_matl->getDWIndex();
      
      NCVariable<Vector> gacceleration,gvelocity_old;

      new_dw->getModifiable(gvelocity_old,lb->gVelocityOldLabel, matl, patch);
      new_dw->getModifiable(gacceleration,lb->gAccelerationLabel,matl, patch);

      for(Patch::FaceType face = Patch::startFace;
          face <= Patch::endFace; face=Patch::nextFace(face)){
        const BoundCondBase *vel_bcs,*sym_bcs;
        if (patch->getBCType(face) == Patch::None) {
          int numChildren = 
            patch->getBCDataArray(face)->getNumberChildren(matl);
          for (int child = 0; child < numChildren; child++) {
            vector<IntVector> *nbound_ptr;
            vector<IntVector> *nu;     // not used;
            vector<IntVector>::const_iterator boundary;
            
            vel_bcs = patch->getArrayBCValues(face,matl,"Velocity",nu,
                                              nbound_ptr,nu,nu,nu,child);
            sym_bcs  = patch->getArrayBCValues(face,matl,"Symmetric",nu,
                                               nbound_ptr,nu,nu,nu,child);
            if (vel_bcs != 0) {
              const VelocityBoundCond* bc =
                dynamic_cast<const VelocityBoundCond*>(vel_bcs);
              if (bc->getKind() == "Dirichlet") {
                for (boundary=nbound_ptr->begin(); boundary != nbound_ptr->end();
                     boundary++) {
                  gvelocity_old[*boundary] = bc->getValue();
                  gacceleration[*boundary] = bc->getValue();
                  
                }
                IntVector l,h;
                patch->getFaceNodes(face,0,l,h);
                for(NodeIterator it(l,h); !it.done(); it++) {
                  IntVector n = *it;
                  int dof[3];
                  int l2g_node_num = l2g[n];
                  dof[0] = l2g_node_num;
                  dof[1] = l2g_node_num+1;
                  dof[2] = l2g_node_num+2;
                  d_solver->d_DOF.insert(dof[0]);
                  d_solver->d_DOF.insert(dof[1]);
                  d_solver->d_DOF.insert(dof[2]);
                }
              }
              delete vel_bcs;
            }
            if (sym_bcs != 0) {
              if (face == Patch::xplus || face == Patch::xminus)
                for (boundary=nbound_ptr->begin(); boundary != nbound_ptr->end(); 
                     boundary++) {
                  gvelocity_old[*boundary] = 
                    Vector(0.,gvelocity_old[*boundary].y(),
                           gvelocity_old[*boundary].z());
                  gacceleration[*boundary] = 
                    Vector(0.,gacceleration[*boundary].y(),
                           gacceleration[*boundary].z());
                }
              if (face == Patch::yplus || face == Patch::yminus)
                for (boundary=nbound_ptr->begin(); boundary != nbound_ptr->end(); 
                     boundary++) {
                  gvelocity_old[*boundary] = 
                    Vector(gvelocity_old[*boundary].x(),0.,
                           gvelocity_old[*boundary].z());
                  gacceleration[*boundary] = 
                    Vector(gacceleration[*boundary].x(),0.,
                           gacceleration[*boundary].z());
                }
              if (face == Patch::zplus || face == Patch::zminus)
                for (boundary=nbound_ptr->begin(); boundary != nbound_ptr->end(); 
                     boundary++) {
                  gvelocity_old[*boundary] = 
                    Vector(gvelocity_old[*boundary].x(),
                           gvelocity_old[*boundary].y(),0.);
                  gacceleration[*boundary] = 
                    Vector(gacceleration[*boundary].x(),
                           gacceleration[*boundary].y(),0.);
                }
              IntVector l,h;
              patch->getFaceNodes(face,0,l,h);
              for(NodeIterator it(l,h); !it.done(); it++) {
                IntVector n = *it;
                // The DOF is an IntVector which is initially (0,0,0).
                // Inserting a 1 into any of the components indicates that 
                // the component should be inserted into the DOF array.
                IntVector DOF(0,0,0);
                if (face == Patch::xminus || face == Patch::xplus)
                  DOF=IntVector(max(DOF.x(),1),max(DOF.y(),0),max(DOF.z(),0));
                if (face == Patch::yminus || face == Patch::yplus)
                  DOF=IntVector(max(DOF.x(),0),max(DOF.y(),1),max(DOF.z(),0));
                if (face == Patch::zminus || face == Patch::zplus)
                  DOF=IntVector(max(DOF.x(),0),max(DOF.y(),0),max(DOF.z(),1));
                
                int dof[3];
                int l2g_node_num = l2g[n];
                dof[0] = l2g_node_num;
                dof[1] = l2g_node_num+1;
                dof[2] = l2g_node_num+2;
                if (DOF.x())
                  d_solver->d_DOF.insert(dof[0]);
                if (DOF.y())
                  d_solver->d_DOF.insert(dof[1]);
                if (DOF.z())
                  d_solver->d_DOF.insert(dof[2]);
              }
              delete sym_bcs;
            }
          }
        } else
          continue;
      }
    }
  }
}

void ImpMPM::computeContact(const ProcessorGroup*,
                            const PatchSubset* patches,
                            const MaterialSubset* ,
                            DataWarehouse* old_dw,
                            DataWarehouse* new_dw)
{
  for(int p=0;p<patches->size();p++){
    const Patch* patch = patches->get(p);
    if (cout_doing.active()) {
      cout_doing <<"Doing computeContact on patch " << patch->getID()
		 <<"\t\t\t\t IMPM"<< "\n" << "\n";
    }

    delt_vartype dt;

    int numMatls = d_sharedState->getNumMPMMatls();
    StaticArray<NCVariable<int> >  contact(numMatls);
    for(int n = 0; n < numMatls; n++){
      MPMMaterial* mpm_matl = d_sharedState->getMPMMaterial( n );
      int dwi = mpm_matl->getDWIndex();
      new_dw->allocateAndPut(contact[n], lb->gContactLabel,       dwi,patch);
      contact[n].initialize(0);
    }

    if(d_rigid_body){
     constNCVariable<Vector> vel_rigid;
     constNCVariable<double> mass_rigid;
     int numMatls = d_sharedState->getNumMPMMatls();
     for(int n = 0; n < numMatls; n++){
      MPMMaterial* mpm_matl = d_sharedState->getMPMMaterial( n );
      if(mpm_matl->getIsRigid()){
        int RM = mpm_matl->getDWIndex();
        new_dw->get(vel_rigid, lb->gVelocityOldLabel,RM,patch,Ghost::None,0);
        new_dw->get(mass_rigid,lb->gMassLabel,       RM,patch,Ghost::None,0);
      }
     }

     // Get and modify non-rigid data
     for(int n = 0; n < numMatls; n++){
      MPMMaterial* mpm_matl = d_sharedState->getMPMMaterial( n );
      int matl = mpm_matl->getDWIndex();
        NCVariable<Vector> dispNew;                     
        new_dw->getModifiable(dispNew,lb->dispNewLabel,matl, patch);

        delt_vartype dt;
        old_dw->get(dt, d_sharedState->get_delt_label(), patch->getLevel() );

        for (NodeIterator iter = patch->getNodeIterator(); !iter.done();iter++){
          IntVector c = *iter;
          if(!compare(mass_rigid[c],0.0)){
            dispNew[c] = Vector(vel_rigid[c].x()*dt*d_contact_dirs.x(),
                                vel_rigid[c].y()*dt*d_contact_dirs.y(),
                                vel_rigid[c].z()*dt*d_contact_dirs.z());
            contact[n][c] = 2;
          }
        }  // for nodes
        if(!flags->d_doGridReset){
          NCVariable<Vector> TDispNew;
          constNCVariable<Vector> TDispOld;
          new_dw->getModifiable(TDispNew,lb->gDisplacementLabel,matl, patch);
          new_dw->get(TDispOld,lb->gDisplacementLabel,matl,patch,Ghost::None,0);
          for (NodeIterator iter=patch->getNodeIterator(); !iter.done();iter++){
            IntVector c = *iter;
            TDispNew[c] = TDispOld[c] + dispNew[c];
          }
        }
     }     // for numMatls
    }      // if rigid_body
  }        // patches
}

void ImpMPM::findFixedDOF(const ProcessorGroup*, 
                          const PatchSubset* patches,
                          const MaterialSubset*, 
                          DataWarehouse* ,
                          DataWarehouse* new_dw)
{
  for(int p=0;p<patches->size();p++){
    const Patch* patch = patches->get(p);
    if (cout_doing.active()) {
      cout_doing <<"Doing findFixedDOF on patch " << patch->getID()
		 <<"\t\t\t\t IMPM"<< "\n" << "\n";
    }

    IntVector lowIndex = patch->getInteriorNodeLowIndex();
    IntVector highIndex = patch->getInteriorNodeHighIndex()+IntVector(1,1,1);
    Array3<int> l2g(lowIndex,highIndex);
    d_solver->copyL2G(l2g,patch);

    bool firstTimeThrough=true;
    int numMatls = d_sharedState->getNumMPMMatls();
    for(int m = 0; m < numMatls; m++){
     MPMMaterial* mpm_matl = d_sharedState->getMPMMaterial( m );
     if(!mpm_matl->getIsRigid() && firstTimeThrough){ 
      firstTimeThrough=false;
      int matlindex = mpm_matl->getDWIndex();
      constNCVariable<double> mass;
      constNCVariable<int> contact;
      new_dw->get(mass,   lb->gMassAllLabel,matlindex,patch,Ghost::None,0);
      new_dw->get(contact,lb->gContactLabel,matlindex,patch,Ghost::None,0);

      for (NodeIterator iter = patch->getNodeIterator(); !iter.done();iter++){
        IntVector n = *iter;
        int dof[3];
        int l2g_node_num = l2g[n];
        dof[0] = l2g_node_num;
        dof[1] = l2g_node_num+1;
        dof[2] = l2g_node_num+2;

        // Just look on the grid to see if the gmass is 0 and then remove that
        if (compare(mass[n],0.)) {
          d_solver->d_DOF.insert(dof[0]);
          d_solver->d_DOF.insert(dof[1]);
          d_solver->d_DOF.insert(dof[2]);
        }
        if (contact[n] == 2) {  // Rigid Contact imposed on these nodes
          for(int i=0;i<3;i++){
            if(d_contact_dirs[i]==1){
             d_solver->d_DOF.insert(dof[i]);  // specifically, these DOFs
            }
          }
        }// contact ==2
      }  // node iterator
     }   // if not rigid
    }    // loop over matls
  }      // patches
}

void ImpMPM::computeStressTensor(const ProcessorGroup*,
                                 const PatchSubset* patches,
                                 const MaterialSubset* ,
                                 DataWarehouse* old_dw,
                                 DataWarehouse* new_dw,
                                 bool recursion)
{
  if (cout_doing.active())
    cout_doing <<"Doing computeStressTensor " <<"\t\t\t\t IMPM"<< "\n" << "\n";

  for(int m = 0; m < d_sharedState->getNumMPMMatls(); m++) {
    MPMMaterial* mpm_matl = d_sharedState->getMPMMaterial(m);
    ConstitutiveModel* cm = mpm_matl->getConstitutiveModel();
    ImplicitCM* cmi = dynamic_cast<ImplicitCM*>(cm);
    if (cmi)
      cmi->computeStressTensor(patches, mpm_matl, old_dw, new_dw,
                               d_solver, recursion);

  }
  
}

void ImpMPM::formStiffnessMatrix(const ProcessorGroup*,
                                 const PatchSubset* patches,
                                 const MaterialSubset*,
                                 DataWarehouse* /*old_dw*/,
                                 DataWarehouse* new_dw)
{
  if (!flags->d_dynamic)
    return;
  for(int p=0;p<patches->size();p++){
    const Patch* patch = patches->get(p);
    if (cout_doing.active()) {
      cout_doing <<"Doing formStiffnessMatrix " << patch->getID()
		 <<"\t\t\t\t IMPM"<< "\n" << "\n";
    }

    IntVector lowIndex = patch->getInteriorNodeLowIndex();
    IntVector highIndex = patch->getInteriorNodeHighIndex()+IntVector(1,1,1);
    Array3<int> l2g(lowIndex,highIndex);

    bool firstTimeThrough=true;
    int numMatls = d_sharedState->getNumMPMMatls();
    for(int m = 0; m < numMatls; m++){
     MPMMaterial* mpm_matl = d_sharedState->getMPMMaterial( m );
     if(!mpm_matl->getIsRigid() && firstTimeThrough){ 
      firstTimeThrough=false;
      int matlindex = mpm_matl->getDWIndex();
      d_solver->copyL2G(l2g,patch);
   
      constNCVariable<double> gmass;
      delt_vartype dt;
      DataWarehouse* parent_new_dw = 
        new_dw->getOtherDataWarehouse(Task::ParentNewDW);
      parent_new_dw->get(gmass,lb->gMassAllLabel,matlindex,patch,Ghost::None,0);
      DataWarehouse* parent_old_dw =
        new_dw->getOtherDataWarehouse(Task::ParentOldDW);
      parent_old_dw->get(dt,d_sharedState->get_delt_label(), patch->getLevel());

      double v[1];

      for (NodeIterator iter = patch->getNodeIterator(); !iter.done(); iter++) {
        IntVector n = *iter;
        int dof[1];
        int l2g_node_num = l2g[n];
        v[0] = gmass[*iter]*(4./(dt*dt));
        for(int ii=0;ii<3;ii++){
          dof[0] = l2g_node_num+ii;
          d_solver->fillMatrix(1,dof,1,dof,v);
        }
      }  // node iterator
     }   // if
    }    // matls
  }
  d_solver->finalizeMatrix();
}

void ImpMPM::computeInternalForce(const ProcessorGroup*,
                                  const PatchSubset* patches,
                                  const MaterialSubset* ,
                                  DataWarehouse* /*old_dw*/,
                                  DataWarehouse* new_dw)
{
  for(int p=0;p<patches->size();p++){
    const Patch* patch = patches->get(p);
    if (cout_doing.active()) {
      cout_doing <<"Doing computeInternalForce on patch " << patch->getID()
		 <<"\t\t\t IMPM"<< "\n" << "\n";
    }
    
    LinearInterpolator* interpolator = new LinearInterpolator(patch);
    vector<IntVector> ni(interpolator->size());
    vector<Vector> d_S(interpolator->size());


    Vector dx = patch->dCell();
    double oodx[3];
    oodx[0] = 1.0/dx.x();
    oodx[1] = 1.0/dx.y();
    oodx[2] = 1.0/dx.z();
    
    int numMPMMatls = d_sharedState->getNumMPMMatls();

    StaticArray<NCVariable<Vector> > int_force(numMPMMatls);
    NCVariable<Vector> INT_FORCE;
    new_dw->allocateTemporary(INT_FORCE,     patch,Ghost::None,0);
    INT_FORCE.initialize(Vector(0,0,0));

    for(int m = 0; m < numMPMMatls; m++){
      MPMMaterial* mpm_matl = d_sharedState->getMPMMaterial( m );
      int dwi = mpm_matl->getDWIndex();

      new_dw->allocateAndPut(int_force[m],lb->gInternalForceLabel,  dwi, patch);
      int_force[m].initialize(Vector(0,0,0));

      if(!mpm_matl->getIsRigid()){

        DataWarehouse* parent_old_dw = 
          new_dw->getOtherDataWarehouse(Task::ParentOldDW);

        ParticleSubset* pset = parent_old_dw->getParticleSubset(dwi, patch,
                                              Ghost::AroundNodes,1,lb->pXLabel);

        constParticleVariable<Point>   px;
        constParticleVariable<double>  pvol;
        constParticleVariable<Matrix3> pstress;

        parent_old_dw->get(px,   lb->pXLabel,               pset);
        new_dw->get(pvol,        lb->pVolumeDeformedLabel,  pset);
        new_dw->get(pstress,     lb->pStressLabel_preReloc, pset);

        Matrix3 stressvol;

        for(ParticleSubset::iterator iter = pset->begin();
            iter != pset->end(); iter++){
          particleIndex idx = *iter;

          // Get the node indices that surround the cell
          interpolator->findCellAndShapeDerivatives(px[idx], ni, d_S);

          stressvol  = pstress[idx]*pvol[idx];

          for (int k = 0; k < 8; k++){
            if(patch->containsNode(ni[k])){
              Vector div(d_S[k].x()*oodx[0],d_S[k].y()*oodx[1],
                                            d_S[k].z()*oodx[2]);
              int_force[m][ni[k]] -= (div * pstress[idx])  * pvol[idx];
            }
          }
        }
        for (NodeIterator iter = patch->getNodeIterator();!iter.done();iter++){
          IntVector n = *iter;
          INT_FORCE[n]+=int_force[m][n];
        }
      }  // if matl isn't rigid
    }  // matls

    for(int m = 0; m < numMPMMatls; m++){
     MPMMaterial* mpm_matl = d_sharedState->getMPMMaterial( m );
     if(!mpm_matl->getIsRigid()){
      for (NodeIterator iter = patch->getNodeIterator(); !iter.done(); iter++) {
        IntVector n = *iter;
        int_force[m][n]=INT_FORCE[n];
      }
     }
    }  // matls
    delete interpolator;
  }    // patches
}

void ImpMPM::formQ(const ProcessorGroup*, const PatchSubset* patches,
                   const MaterialSubset*, DataWarehouse* old_dw,
                   DataWarehouse* new_dw)
{
  for(int p=0;p<patches->size();p++){
    const Patch* patch = patches->get(p);
    if (cout_doing.active()) {
      cout_doing <<"Doing formQ on patch " << patch->getID()
		 <<"\t\t\t\t\t IMPM"<< "\n" << "\n";
    }

    IntVector lowIndex = patch->getInteriorNodeLowIndex();
    IntVector highIndex = patch->getInteriorNodeHighIndex()+IntVector(1,1,1);
    Array3<int> l2g(lowIndex,highIndex);
    d_solver->copyL2G(l2g,patch);

    bool firstTimeThrough=true;
    int numMatls = d_sharedState->getNumMPMMatls();
    for(int m = 0; m < numMatls; m++){
     MPMMaterial* mpm_matl = d_sharedState->getMPMMaterial( m );
     if(!mpm_matl->getIsRigid() && firstTimeThrough){
      firstTimeThrough=false;
      int dwi = mpm_matl->getDWIndex();

      delt_vartype dt;
      Ghost::GhostType  gnone = Ghost::None;

      constNCVariable<Vector> extForce, intForce;
      constNCVariable<Vector> dispNew,velocity,accel;
      constNCVariable<double> mass;
      DataWarehouse* parent_new_dw = 
        new_dw->getOtherDataWarehouse(Task::ParentNewDW);
      DataWarehouse* parent_old_dw = 
        new_dw->getOtherDataWarehouse(Task::ParentOldDW);

      parent_old_dw->get(dt,d_sharedState->get_delt_label(), patch->getLevel());
      new_dw->get(       intForce, lb->gInternalForceLabel,dwi,patch,gnone,0);
      old_dw->get(       dispNew,  lb->dispNewLabel,       dwi,patch,gnone,0);
      parent_new_dw->get(extForce, lb->gExternalForceLabel,dwi,patch,gnone,0);
      parent_new_dw->get(velocity, lb->gVelocityOldLabel,  dwi,patch,gnone,0);
      parent_new_dw->get(accel,    lb->gAccelerationLabel, dwi,patch,gnone,0);
      parent_new_dw->get(mass,     lb->gMassAllLabel,      dwi,patch,gnone,0);

      double fodts = 4./(dt*dt);
      double fodt = 4./dt;

      double Q=0.;

      for (NodeIterator iter = patch->getNodeIterator(); !iter.done(); iter++){
        IntVector n = *iter;
        int dof[3];
        int l2g_node_num = l2g[n];
        dof[0] = l2g_node_num;
        dof[1] = l2g_node_num+1;
        dof[2] = l2g_node_num+2;

        double v[3];
        v[0] = extForce[n].x() + intForce[n].x();
        v[1] = extForce[n].y() + intForce[n].y();
        v[2] = extForce[n].z() + intForce[n].z();

        // temp2 = M*a^(k-1)(t+dt)
        if (flags->d_dynamic) {
          v[0] -= (dispNew[n].x()*fodts - velocity[n].x()*fodt -
                   accel[n].x())*mass[n];
          v[1] -= (dispNew[n].y()*fodts - velocity[n].y()*fodt -
                   accel[n].y())*mass[n];
          v[2] -= (dispNew[n].z()*fodts - velocity[n].z()*fodt -
                   accel[n].z())*mass[n];
        }
        d_solver->fillVector(dof[0],double(v[0]));
        d_solver->fillVector(dof[1],double(v[1]));
        d_solver->fillVector(dof[2],double(v[2]));
        Q += v[0]*v[0] + v[1]*v[1] + v[2]*v[2];
      }
      d_solver->assembleVector();
      if(isnan(Q)){
        cout << "RHS contains a nan, restarting timestep" << endl;
        new_dw->abortTimestep();
        new_dw->restartTimestep();
        return;
      }
     } // first time through non-rigid
    }  // matls
  }    // patches
}

void ImpMPM::solveForDuCG(const ProcessorGroup* /*pg*/,
                          const PatchSubset* patches,
                          const MaterialSubset* ,
                          DataWarehouse*,
                          DataWarehouse* new_dw)

{
  int num_nodes = 0;
  for(int p = 0; p<patches->size();p++) {
    const Patch* patch = patches->get(p);
    if (cout_doing.active()) {
      cout_doing <<"Doing solveForDuCG on patch " << patch->getID()
		 <<"\t\t\t\t IMPM"<< "\n" << "\n";
    }

    IntVector nodes = patch->getNInteriorNodes();
    num_nodes += (nodes.x()-2)*(nodes.y()-2)*(nodes.z()-2)*3;
  }

  DataWarehouse* parent_new_dw=new_dw->getOtherDataWarehouse(Task::ParentNewDW);
  bool tsr = parent_new_dw->timestepRestarted();

  if(!tsr){  // if a tsr has already been called for don't do the solve
    d_solver->removeFixedDOF(num_nodes);
    vector<double> guess;
    d_solver->solve(guess);   
  }
  else{
    cout << "skipping solve, timestep has already called for a restart" << endl;
  }
}

void ImpMPM::getDisplacementIncrement(const ProcessorGroup* /*pg*/,
                                      const PatchSubset* patches,
                                      const MaterialSubset* ,
                                      DataWarehouse*,
                                      DataWarehouse* new_dw)

{
  for(int p = 0; p<patches->size();p++) {
    const Patch* patch = patches->get(p);
    if (cout_doing.active()) {
      cout_doing <<"Doing getDisplacementIncrement on patch " << patch->getID()
		 <<"\t\t\t\t IMPM"<< "\n" << "\n";
    }

    IntVector lowIndex = patch->getInteriorNodeLowIndex();
    IntVector highIndex = patch->getInteriorNodeHighIndex()+IntVector(1,1,1);
    Array3<int> l2g(lowIndex,highIndex);
    d_solver->copyL2G(l2g,patch);
 
    vector<double> x;
    int begin = d_solver->getSolution(x);
  
    int numMatls = d_sharedState->getNumMPMMatls();
    for(int m = 0; m < numMatls; m++){
      MPMMaterial* mpm_matl = d_sharedState->getMPMMaterial( m );
      int matlindex = mpm_matl->getDWIndex();

      NCVariable<Vector> dispInc;
      new_dw->allocateAndPut(dispInc,lb->dispIncLabel,matlindex,patch);
      dispInc.initialize(Vector(0.));

      if (flags->d_doMechanics) {
        for (NodeIterator iter = patch->getNodeIterator();!iter.done();iter++){
          IntVector n = *iter;
          int dof[3];
          int l2g_node_num = l2g[n] - begin;
          dof[0] = l2g_node_num;
          dof[1] = l2g_node_num+1;
          dof[2] = l2g_node_num+2;
          dispInc[n] = Vector(x[dof[0]],x[dof[1]],x[dof[2]]);
        }
      }
    }
  }
}

void ImpMPM::updateGridKinematics(const ProcessorGroup*,
                                  const PatchSubset* patches,
                                  const MaterialSubset* ,
                                  DataWarehouse* old_dw,
                                  DataWarehouse* new_dw)
{
  for (int p = 0; p<patches->size();p++) {
    const Patch* patch = patches->get(p);
    if (cout_doing.active()) {
      cout_doing <<"Doing updateGridKinematics on patch " << patch->getID()
		 <<"\t\t\t IMPM"<< "\n" << "\n";
    }

    Ghost::GhostType  gnone = Ghost::None;

    int rig_index=-99;
    int numMatls = d_sharedState->getNumMPMMatls();
    for(int m = 0; m < numMatls; m++){
       MPMMaterial* mpm_matl = d_sharedState->getMPMMaterial( m );
       if(mpm_matl->getIsRigid()){
         rig_index = mpm_matl->getDWIndex();
       }
    }

    constNCVariable<Vector> velocity_rig;
    if(d_rigid_body){
      DataWarehouse* parent_new_dw = 
                              new_dw->getOtherDataWarehouse(Task::ParentNewDW);
      parent_new_dw->get(velocity_rig,
                                 lb->gVelocityOldLabel,rig_index,patch,gnone,0);
    }

    for(int m = 0; m < numMatls; m++){
      MPMMaterial* mpm_matl = d_sharedState->getMPMMaterial( m );
      int dwi = mpm_matl->getDWIndex();

      NCVariable<Vector> dispNew,velocity;
      constNCVariable<Vector> dispInc,dispNew_old,velocity_old;
      constNCVariable<int> contact;

      delt_vartype dt;

      DataWarehouse* parent_new_dw = 
        new_dw->getOtherDataWarehouse(Task::ParentNewDW);
      DataWarehouse* parent_old_dw = 
        new_dw->getOtherDataWarehouse(Task::ParentOldDW);
      parent_old_dw->get(dt, d_sharedState->get_delt_label(),patch->getLevel());
      old_dw->get(dispNew_old,         lb->dispNewLabel,   dwi,patch,gnone,0);
      new_dw->get(dispInc,             lb->dispIncLabel,   dwi,patch,gnone,0);
      new_dw->allocateAndPut(dispNew,  lb->dispNewLabel,   dwi,patch);
      new_dw->allocateAndPut(velocity, lb->gVelocityLabel, dwi,patch);
      parent_new_dw->get(velocity_old, lb->gVelocityOldLabel,
                                                           dwi,patch,gnone,0);
      parent_new_dw->get(contact,      lb->gContactLabel,  dwi,patch,gnone,0);

      double oneifdyn = 0.;
      if(flags->d_dynamic){
        oneifdyn = 1.;
      }

      dispNew.copyData(dispNew_old);

      if(!mpm_matl->getIsRigid()){
       for (NodeIterator iter = patch->getNodeIterator();!iter.done();iter++){
        IntVector n = *iter;
        dispNew[n] += dispInc[n];
        velocity[n] = dispNew[n]*(2./dt) - oneifdyn*velocity_old[n];
       }
      }

      if(d_rigid_body){  // overwrite some of the values computed above
        for (NodeIterator iter = patch->getNodeIterator();!iter.done();iter++){
          IntVector n = *iter;
          if(contact[n]==2){
            dispNew[n] = Vector((1.-d_contact_dirs.x())*dispNew[n].x() +
                                d_contact_dirs.x()*velocity_rig[n].x()*dt,
                                (1.-d_contact_dirs.y())*dispNew[n].y() +
                                d_contact_dirs.y()*velocity_rig[n].y()*dt,
                                (1.-d_contact_dirs.z())*dispNew[n].z() +
                                d_contact_dirs.z()*velocity_rig[n].z()*dt);

            velocity[n] = dispNew[n]*(2./dt) - oneifdyn*velocity_old[n];
          } // if contact == 2
        } // for
      } // if d_rigid_body
      if(!flags->d_doGridReset){
        constNCVariable<Vector> TDispOld;
        NCVariable<Vector> TDisp;
        parent_old_dw->get(TDispOld, lb->gDisplacementLabel,dwi,patch,gnone,0);
        new_dw->allocateAndPut(TDisp,lb->gDisplacementLabel,dwi,patch);
        for (NodeIterator iter = patch->getNodeIterator();!iter.done();iter++){
          IntVector n = *iter;
          TDisp[n] = TDispOld[n] + dispNew[n];
        }
      }
    }   // matls
  }
}

void ImpMPM::checkConvergence(const ProcessorGroup*,
                              const PatchSubset* patches,
                              const MaterialSubset* ,
                              DataWarehouse* old_dw,
                              DataWarehouse* new_dw)
{
  for(int p=0;p<patches->size();p++){
   const Patch* patch = patches->get(p);
   if (cout_doing.active()) {
     cout_doing <<"Doing checkConvergence on patch " << patch->getID()
		<<"\t\t\t IMPM"<< "\n" << "\n";
   }

   IntVector lowIndex = patch->getInteriorNodeLowIndex();
   IntVector highIndex = patch->getInteriorNodeHighIndex()+IntVector(1,1,1);
   Array3<int> l2g(lowIndex,highIndex);
   d_solver->copyL2G(l2g,patch);

   int matlindex = 0;

    constNCVariable<Vector> dispInc;
    new_dw->get(dispInc,lb->dispIncLabel,matlindex,patch,Ghost::None,0);
    
    double dispIncNorm  = 0.;
    double dispIncQNorm = 0.;
    vector<double> getQ;
    int begin = d_solver->getRHS(getQ);
    for (NodeIterator iter = patch->getNodeIterator(); !iter.done();iter++){
      IntVector n = *iter;
      int dof[3];
      int l2g_node_num = l2g[n] - begin;
      dof[0] = l2g_node_num;
      dof[1] = l2g_node_num+1;
      dof[2] = l2g_node_num+2;
      dispIncNorm += Dot(dispInc[n],dispInc[n]);
      dispIncQNorm += dispInc[n].x()*getQ[dof[0]] +
      dispInc[n].y()*getQ[dof[1]] +  dispInc[n].z()*getQ[dof[2]];
    }
    // We are computing both dispIncQNorm0 and dispIncNormMax (max residuals)
    // We are computing both dispIncQNorm and dispIncNorm (current residuals)

    double dispIncQNorm0,dispIncNormMax;
    sum_vartype dispIncQNorm0_var,dispIncNormMax_var;
    old_dw->get(dispIncQNorm0_var, lb->dispIncQNorm0);
    old_dw->get(dispIncNormMax_var,lb->dispIncNormMax);

    dispIncQNorm0 = dispIncQNorm0_var;
    dispIncNormMax = dispIncNormMax_var;

    bool first_iteration=false;
    if (compare(dispIncQNorm0,0.)){
      first_iteration = true;
      dispIncQNorm0 = dispIncQNorm;
    }

    if (dispIncNorm > dispIncNormMax){
      dispIncNormMax = dispIncNorm;
    }

    // The following is being done because the denominator in the
    // convergence criteria is carried forward each iteration.  Since 
    // every patch puts this into the sum_vartype, the value is multiplied
    // by the number of patches.  Predividing by numPatches fixes this.
    int numPatches = patch->getLevel()->numPatches();
    if(!first_iteration){
      dispIncQNorm0/=((double) numPatches);
      if(dispIncNormMax!=dispIncNorm){
        dispIncNormMax/=((double) numPatches);
      }
    }

    new_dw->put(sum_vartype(dispIncNorm),   lb->dispIncNorm);
    new_dw->put(sum_vartype(dispIncQNorm),  lb->dispIncQNorm);
    new_dw->put(sum_vartype(dispIncNormMax),lb->dispIncNormMax);
    new_dw->put(sum_vartype(dispIncQNorm0), lb->dispIncQNorm0);
  }  // End of loop over patches

}

void ImpMPM::computeStressTensor(const ProcessorGroup*,
                                 const PatchSubset* patches,
                                 const MaterialSubset* ,
                                 DataWarehouse* old_dw,
                                 DataWarehouse* new_dw)
{
  if (cout_doing.active()) 
    cout_doing <<"Doing computeStressTensor" <<"\t\t\t\t IMPM"<< "\n" << "\n";

  for(int m = 0; m < d_sharedState->getNumMPMMatls(); m++) {
    MPMMaterial* mpm_matl = d_sharedState->getMPMMaterial(m);
    ConstitutiveModel* cm = mpm_matl->getConstitutiveModel();
    cm->computeStressTensor(patches, mpm_matl, old_dw, new_dw);
  }
}

void ImpMPM::updateTotalDisplacement(const ProcessorGroup*,
                                     const PatchSubset* patches,
                                     const MaterialSubset*,
                                     DataWarehouse* old_dw,
                                     DataWarehouse* new_dw)
{
  for(int p=0;p<patches->size();p++){
    const Patch* patch = patches->get(p);
    if (cout_doing.active()) {
      cout_doing <<"Doing updateTotalDisplacement on patch " << patch->getID()
		 <<"\t\t\t IMPM"<< "\n" << "\n";
    }

    Ghost::GhostType  gnone = Ghost::None;
    for(int m = 0; m < d_sharedState->getNumMPMMatls(); m++){
      MPMMaterial* mpm_matl = d_sharedState->getMPMMaterial( m );
      int dwi = mpm_matl->getDWIndex();

      constNCVariable<Vector> dispNew,TDispOld;
      NCVariable<Vector> TDisp;
      old_dw->get(TDispOld,       lb->gDisplacementLabel,dwi,patch,  gnone,0);
      new_dw->getModifiable(TDisp,lb->gDisplacementLabel,dwi,patch);
      new_dw->get(dispNew,        lb->dispNewLabel,      dwi, patch, gnone,0);

      for (NodeIterator iter = patch->getNodeIterator();!iter.done();iter++){
        IntVector n = *iter;
        TDisp[n] = TDispOld[n] + dispNew[n];
      }
    }
  }
}

void ImpMPM::computeAcceleration(const ProcessorGroup*,
                                 const PatchSubset* patches,
                                 const MaterialSubset*,
                                 DataWarehouse* old_dw,
                                 DataWarehouse* new_dw)
{
  if (!flags->d_dynamic)
    return;
  for(int p=0;p<patches->size();p++){
    const Patch* patch = patches->get(p);
    if (cout_doing.active()) {
      cout_doing <<"Doing computeAcceleration on patch " << patch->getID()
		 <<"\t\t\t IMPM"<< "\n" << "\n";
    }

    Ghost::GhostType  gnone = Ghost::None;
    for(int m = 0; m < d_sharedState->getNumMPMMatls(); m++){
      MPMMaterial* mpm_matl = d_sharedState->getMPMMaterial( m );
      int dwi = mpm_matl->getDWIndex();

      NCVariable<Vector> acceleration;
      constNCVariable<Vector> velocity,dispNew;
      delt_vartype delT;

      new_dw->getModifiable(acceleration,lb->gAccelerationLabel,dwi,patch);
      new_dw->get(velocity,lb->gVelocityOldLabel,dwi, patch, gnone, 0);
      new_dw->get(dispNew, lb->dispNewLabel,     dwi, patch, gnone, 0);

      old_dw->get(delT, d_sharedState->get_delt_label(), getLevel(patches) );

      double fodts = 4./(delT*delT);
      double fodt = 4./(delT);

      for(NodeIterator iter = patch->getNodeIterator(); !iter.done(); iter++){
        IntVector c = *iter;
        acceleration[c] = dispNew[c]*fodts - velocity[c]*fodt - acceleration[c];
      }
    }
  }
}

void ImpMPM::interpolateToParticlesAndUpdate(const ProcessorGroup*,
                                             const PatchSubset* patches,
                                             const MaterialSubset* ,
                                             DataWarehouse* old_dw,
                                             DataWarehouse* new_dw)
{
  for(int p=0;p<patches->size();p++){
    const Patch* patch = patches->get(p);
    if (cout_doing.active()) {
      cout_doing <<"Doing interpolateToParticlesAndUpdate on patch " 
		 << patch->getID() <<"\t IMPM"<< "\n" << "\n";
    }

    Ghost::GhostType  gac = Ghost::AroundCells;

    LinearInterpolator* interpolator = new LinearInterpolator(patch);
    vector<IntVector> ni(interpolator->size());
    vector<double> S(interpolator->size());
    vector<Vector> d_S(interpolator->size());


    // Performs the interpolation from the cell vertices of the grid
    // acceleration and displacement to the particles to update their
    // velocity and position respectively
    Vector disp(0.0,0.0,0.0);
    Vector acc(0.0,0.0,0.0);
  
    // DON'T MOVE THESE!!!
    Vector CMX(0.0,0.0,0.0);
    Vector CMV(0.0,0.0,0.0);
    double ke=0;
    double thermal_energy = 0.0;
    double thermal_energy2 = 0.0;
    int numMPMMatls=d_sharedState->getNumMPMMatls();

    double move_particles=1.;
    if(!flags->d_doGridReset){
      move_particles=0.;
    }

    constNCVariable<double> gTemperature,gTemperatureRate;
    
    new_dw->get(gTemperatureRate,lb->gTemperatureRateLabel,0,patch,gac, 1);

    new_dw->get(gTemperature,lb->gTemperatureLabel, 0,patch,gac,1);

    for(int m = 0; m < numMPMMatls; m++){
      MPMMaterial* mpm_matl = d_sharedState->getMPMMaterial( m );
      int dwindex = mpm_matl->getDWIndex();
      // Get the arrays of particle values to be changed
      constParticleVariable<Point> px;
      ParticleVariable<Point> pxnew,pxx;
      constParticleVariable<Vector> pvelocity, pacceleration,pexternalForce;
      constParticleVariable<Vector> pDispOld,psize;
      ParticleVariable<Vector> pvelnew,pexternalForceNew,paccNew,pDisp,psizeNew;
      constParticleVariable<double> pmass, pvolume,pTempOld,pEro;
      ParticleVariable<double> pmassNew,pvolumeNew,pTemp,pEroNew;
      ParticleVariable<double> pTempPreNew;
  
      // Get the arrays of grid data on which the new part. values depend
      constNCVariable<Vector> dispNew, gacceleration;
      constNCVariable<double> dTdt;

      delt_vartype delT;

      ParticleSubset* pset = old_dw->getParticleSubset(dwindex, patch);

      ParticleSubset* delete_particles = scinew ParticleSubset
        (pset->getParticleSet(),false,dwindex,patch, 0);
    
      old_dw->get(px,                    lb->pXLabel,                    pset);
      old_dw->get(pmass,                 lb->pMassLabel,                 pset);
      new_dw->get(pvolume,               lb->pVolumeDeformedLabel,       pset);
      old_dw->get(pexternalForce,        lb->pExternalForceLabel,        pset);
      old_dw->get(pvelocity,             lb->pVelocityLabel,             pset);
      old_dw->get(pacceleration,         lb->pAccelerationLabel,         pset);
      old_dw->get(pTempOld,              lb->pTemperatureLabel,          pset);
      old_dw->get(pDispOld,              lb->pDispLabel,                 pset);
      new_dw->allocateAndPut(pvelnew,    lb->pVelocityLabel_preReloc,    pset);
      new_dw->allocateAndPut(paccNew,    lb->pAccelerationLabel_preReloc,pset);
      new_dw->allocateAndPut(pxnew,      lb->pXLabel_preReloc,           pset);
      new_dw->allocateAndPut(pxx,        lb->pXXLabel,                   pset);
      new_dw->allocateAndPut(pmassNew,   lb->pMassLabel_preReloc,        pset);
      new_dw->allocateAndPut(pvolumeNew, lb->pVolumeLabel_preReloc,      pset);
      new_dw->allocateAndPut(pTemp,      lb->pTemperatureLabel_preReloc, pset);
      new_dw->allocateAndPut(pDisp,      lb->pDispLabel_preReloc,        pset);

      new_dw->get(dispNew,        lb->dispNewLabel,      dwindex,patch,gac, 1);
      new_dw->get(gacceleration,  lb->gAccelerationLabel,dwindex,patch,gac, 1);


      old_dw->get(psize,                 lb->pSizeLabel,                 pset);
      old_dw->get(pEro,                  lb->pErosionLabel,              pset);
      new_dw->allocateAndPut(psizeNew,   lb->pSizeLabel_preReloc,        pset);
      new_dw->allocateAndPut(pEroNew,    lb->pErosionLabel_preReloc,     pset);
      new_dw->allocateAndPut(pTempPreNew,lb->pTempPreviousLabel_preReloc,pset);
      psizeNew.copyData(psize);
      pEroNew.copyData(pEro);
      pTempPreNew.copyData(pTempOld);
     
      NCVariable<double> dTdt_create, massBurnFraction_create;  
      new_dw->allocateTemporary(dTdt_create, patch,Ghost::AroundCells,1);
      dTdt_create.initialize(0.);
      dTdt = dTdt_create; // reference created data

      old_dw->get(delT, d_sharedState->get_delt_label(), getLevel(patches) );
      double Cp=mpm_matl->getSpecificHeat();
      double K  = mpm_matl->getThermalConductivity();

      for(ParticleSubset::iterator iter = pset->begin();
          iter != pset->end(); iter++){
        particleIndex idx = *iter;

        // Get the node indices that surround the cell
        interpolator->findCellAndWeightsAndShapeDerivatives(px[idx],ni,S,d_S);

        disp = Vector(0.0,0.0,0.0);
        acc = Vector(0.0,0.0,0.0);
        double tempRate = 0.;
        double potential_energy = 0.;
        //double ptemp = 0.;
        
        // Accumulate the contribution from each surrounding vertex
        for (int k = 0; k < 8; k++) {
          disp      += dispNew[ni[k]]       * S[k];
          acc       += gacceleration[ni[k]] * S[k];
          tempRate += (gTemperatureRate[ni[k]] + dTdt[ni[k]])* S[k];
          //tempRate +=  dTdt[ni[k]]* S[k];
          //ptemp += gTemperature[ni[k]] * S[k];
          for (int dir=0;dir<3;dir++)
            potential_energy += d_S[k][dir]*d_S[k][dir]*K;
        }

        // Update the particle's position and velocity
        pxnew[idx]        = px[idx] + disp*move_particles;
        pDisp[idx]        = pDispOld[idx] + disp;
        pvelnew[idx]      = pvelocity[idx] 
                          + (pacceleration[idx]+acc)*(.5* delT);

        // pxx is only useful if we're not in normal grid resetting mode.
        pxx[idx]             = px[idx]    + pDisp[idx];

        paccNew[idx]         = acc;
        pmassNew[idx]        = pmass[idx];
        pvolumeNew[idx]      = pvolume[idx];
        pTemp[idx]           = pTempOld[idx] + tempRate*delT;
        //pTemp[idx]           = ptemp + tempRate*delT;
        //        cout << "pTemp[" << idx << "]= " << pTemp[idx] << endl;

        if(pmassNew[idx] <= 0.0){
          delete_particles->addParticle(idx);
          pvelnew[idx] = Vector(0.,0.,0);
          pxnew[idx] = px[idx];
        }

        // thermal_energy += pTemp[idx] * pmass[idx] * Cp;
        thermal_energy += tempRate * pmass[idx] * Cp;
        // Thermal energy due to temperature flux (spatially varying part).
        thermal_energy2 += potential_energy* pvolume[idx];
        ke += .5*pmass[idx]*pvelnew[idx].length2();
        CMX = CMX + (pxnew[idx]*pmass[idx]).asVector();
        CMV += pvelnew[idx]*pmass[idx];
      }
 
      if(mpm_matl->getIsRigid()) {
        const double tcurr = d_sharedState->getElapsedTime(); 
        if(tcurr >= d_stop_time){
          for(ParticleSubset::iterator iter = pset->begin();
              iter != pset->end(); iter++){
            particleIndex idx = *iter;
            pvelnew[idx] = d_vel_after_stop;
          }
        }
      }

      new_dw->deleteParticles(delete_particles);
      
      constParticleVariable<long64> pids;
      ParticleVariable<long64> pids_new;
      old_dw->get(pids, lb->pParticleIDLabel, pset);
      new_dw->allocateAndPut(pids_new, lb->pParticleIDLabel_preReloc, pset);
      pids_new.copyData(pids);

    }

    // DON'T MOVE THESE!!!
    new_dw->put(sum_vartype(ke),     lb->KineticEnergyLabel);
    new_dw->put(sumvec_vartype(CMX), lb->CenterOfMassPositionLabel);
    new_dw->put(sumvec_vartype(CMV), lb->CenterOfMassVelocityLabel);
    new_dw->put(sum_vartype(thermal_energy+thermal_energy2), lb->ThermalEnergyLabel);

    delete interpolator;
  }
}

void ImpMPM::interpolateStressToGrid(const ProcessorGroup*,
                                     const PatchSubset* patches,
                                     const MaterialSubset* ,
                                     DataWarehouse* old_dw,
                                     DataWarehouse* new_dw)
{
  // node based forces
  Vector bndyForce[6];
  double bndyArea[6];
  for(int iface=0;iface<6;iface++) {
      bndyForce[iface]  = Vector(0.);
      bndyArea [iface]  = 0.;
  }
  for(int p=0;p<patches->size();p++){
    const Patch* patch = patches->get(p);
    if (cout_doing.active()) {
      cout_doing <<"Doing interpolateStressToGrid on patch " << patch->getID()
		 <<"\t\t IMPM"<< "\n" << "\n";
    }

    LinearInterpolator* interpolator = new LinearInterpolator(patch);
    vector<IntVector> ni(interpolator->size());
    vector<double> S(interpolator->size());
    vector<Vector> d_S(interpolator->size());

    // This task is done for visualization only
    int numMatls = d_sharedState->getNumMPMMatls();

    constNCVariable<double>   GVOLUME;
    new_dw->get(GVOLUME, lb->gVolumeLabel,
                d_sharedState->getAllInOneMatl()->get(0), patch, Ghost::None,0);

    NCVariable<Matrix3>       GSTRESS;
    new_dw->allocateTemporary(GSTRESS, patch, Ghost::None,0);
    NCVariable<Vector> INT_FORCE;
    new_dw->allocateTemporary(INT_FORCE,     patch,Ghost::None,0);
    INT_FORCE.initialize(Vector(0,0,0));

    GSTRESS.initialize(Matrix3(0.));
    StaticArray<NCVariable<Matrix3> >         gstress(numMatls);
    StaticArray<constNCVariable<double> >     gvolume(numMatls);
    StaticArray<NCVariable<Vector> >          int_force(numMatls);

    Vector dx = patch->dCell();
    double oodx[3];
    oodx[0] = 1.0/dx.x();
    oodx[1] = 1.0/dx.y();
    oodx[2] = 1.0/dx.z();

    for(int m = 0; m < numMatls; m++){
      MPMMaterial* mpm_matl = d_sharedState->getMPMMaterial( m );
      int dwi = mpm_matl->getDWIndex();

      new_dw->getModifiable(int_force[m],lb->gInternalForceLabel,  dwi, patch);
      new_dw->allocateAndPut(gstress[m], lb->gStressForSavingLabel,dwi, patch);
      new_dw->get(gvolume[m], lb->gVolumeLabel, dwi, patch,Ghost::None,0);
      gstress[m].initialize(Matrix3(0.));
      int_force[m].initialize(Vector(0.));

      if(!mpm_matl->getIsRigid()){

       constParticleVariable<Point>   px;
       constParticleVariable<double>  pvol;
       constParticleVariable<Matrix3> pstress;

       ParticleSubset* pset = old_dw->getParticleSubset(dwi, patch,
                                              Ghost::AroundNodes,1,lb->pXLabel);
       old_dw->get(px,          lb->pXLabel,               pset);
       new_dw->get(pvol,        lb->pVolumeDeformedLabel,  pset);
       new_dw->get(pstress,     lb->pStressLabel_preReloc, pset);

       Matrix3 stressvol;

       for(ParticleSubset::iterator iter = pset->begin();
          iter != pset->end(); iter++){
        particleIndex idx = *iter;

        // Get the node indices that surround the cell
        interpolator->findCellAndWeightsAndShapeDerivatives(px[idx],ni,S,d_S);

        stressvol  = pstress[idx]*pvol[idx];

        for (int k = 0; k < 8; k++){
          if(patch->containsNode(ni[k])){
           gstress[m][ni[k]]       += stressvol * S[k];
           Vector div(d_S[k].x()*oodx[0],d_S[k].y()*oodx[1],d_S[k].z()*oodx[2]);           int_force[m][ni[k]] -= div * stressvol;
          }
        }
       }
       for(NodeIterator iter = patch->getNodeIterator(); !iter.done(); iter++) {
        IntVector c = *iter;
        GSTRESS[c] += (gstress[m][c]);
        INT_FORCE[c]+=int_force[m][c];
       }
     }  // if matl isn't rigid
    }  // Loop over matls

    // gstress will be normalized by gvolume (same for all matls)
    for(int m = 0; m < numMatls; m++){
      for(NodeIterator iter = patch->getNodeIterator(); !iter.done(); iter++) {
        IntVector c = *iter;
        gstress[m][c] = GSTRESS[c]/(gvolume[m][c]+1.e-200);
      }
      MPMMaterial* mpm_matl = d_sharedState->getMPMMaterial( m );
      if(!mpm_matl->getIsRigid()){
       for (NodeIterator iter = patch->getNodeIterator(); !iter.done(); iter++){
         IntVector n = *iter;
         int_force[m][n]=INT_FORCE[n];
       }
      }
    }  // Loop over matls

    // Fill in the value for the all in one material
    // GSTRESS will be normalized by gvolumeglobal
    for(NodeIterator iter = patch->getNodeIterator(); !iter.done(); iter++) {
      IntVector c = *iter;
      GSTRESS[c] /= (GVOLUME[c]+1.e-200);
    }

    // save boundary forces before apply symmetry boundary condition.
    bool did_it_already=false;
    for(int m = 0; m < numMatls; m++){
     MPMMaterial* mpm_matl = d_sharedState->getMPMMaterial( m );
     if(!did_it_already && !mpm_matl->getIsRigid()){
      did_it_already=true;
      for(list<Patch::FaceType>::const_iterator fit(d_bndy_traction_faces.begin());
          fit!=d_bndy_traction_faces.end();fit++) {
        Patch::FaceType face = *fit;
                                                                                
        // Check if the face is on an external boundary
        if(patch->getBCType(face)==Patch::Neighbor)
           continue;
                                                                                
        // We are on the boundary, i.e. not on an interior patch
        // boundary, and also on the correct side,
        // so do the traction accumulation . . .
        // loop nodes to find forces
        IntVector projlow, projhigh;
        patch->getFaceNodes(face, 0, projlow, projhigh);
                                                                                
        for (int i = projlow.x(); i<projhigh.x(); i++) {
          for (int j = projlow.y(); j<projhigh.y(); j++) {
            for (int k = projlow.z(); k<projhigh.z(); k++) {
              IntVector ijk(i,j,k);
              // flip sign so that pushing on boundary gives positive force
              bndyForce[face] -= int_force[m][ijk];
                                                                                
              double celldepth  = dx[face/2];
              bndyArea [face] += gvolume[m][ijk]/celldepth;
            }
          }
        }
                                                                                
      } // faces
     } // if
   }  // matls
    delete interpolator;
  }

  // be careful only to put the fields that we have built
  // that way if the user asks to output a field that has not been built
  // it will fail early rather than just giving zeros.
  for(std::list<Patch::FaceType>::const_iterator ftit(d_bndy_traction_faces.begin()); ftit!=d_bndy_traction_faces.end();ftit++) {
    new_dw->put(sumvec_vartype(bndyForce[*ftit]),lb->BndyForceLabel[*ftit]);
    new_dw->put(sum_vartype(bndyArea[*ftit]),lb->BndyContactAreaLabel[*ftit]);
  }

}

void ImpMPM::setSharedState(SimulationStateP& ssp)
{
  d_sharedState = ssp;
}

void ImpMPM::printParticleCount(const ProcessorGroup* pg,
                                const PatchSubset*,
                                const MaterialSubset*,
                                DataWarehouse*,
                                DataWarehouse* new_dw)
{
  if(pg->myrank() == 0){
    static bool printed=false;
    if(!printed){
      sumlong_vartype pcount;
      new_dw->get(pcount, lb->partCountLabel);
      cerr << "Created " << (long) pcount << " total particles\n";
      printed=true;
    }
  }
}

void ImpMPM::scheduleInitializeHeatFluxBCs(const LevelP& level,
                                              SchedulerP& sched)
{
  MaterialSubset* loadCurveIndex = scinew MaterialSubset();
  int nofHeatFluxBCs = 0;
  for (int ii = 0; ii<(int)MPMPhysicalBCFactory::mpmPhysicalBCs.size(); ii++){
    string bcs_type = MPMPhysicalBCFactory::mpmPhysicalBCs[ii]->getType();
    if (bcs_type == "HeatFlux" || "ArchesHeatFlux")
      loadCurveIndex->add(nofHeatFluxBCs++);
  }
  if (nofHeatFluxBCs > 0) {

    // Create a task that calculates the total number of particles
    // associated with each load curve.  
    Task* t = scinew Task("ImpMPM::countMaterialPointsPerLoadCurve",
                          this, &ImpMPM::countMaterialPointsPerLoadCurve);
    t->requires(Task::NewDW, lb->pLoadCurveIDLabel, Ghost::None);
    t->computes(lb->materialPointsPerLoadCurveLabel, loadCurveIndex,
                Task::OutOfDomain);
    sched->addTask(t, level->eachPatch(), d_sharedState->allMPMMaterials());

    // Create a task that calculates the heatflux to be associated with
    // each particle based on the HeatFluxBCs
    t = scinew Task("ImpMPM::initializeHeatFluxBC",
                    this, &ImpMPM::initializeHeatFluxBC);
    t->requires(Task::NewDW, lb->pXLabel, Ghost::None);
    t->requires(Task::NewDW, lb->pLoadCurveIDLabel, Ghost::None);
    t->requires(Task::NewDW, lb->materialPointsPerLoadCurveLabel, loadCurveIndex, Task::OutOfDomain, Ghost::None);
    t->modifies(lb->pExternalHeatFluxLabel);
    sched->addTask(t, level->eachPatch(), d_sharedState->allMPMMaterials());
  }
}


void ImpMPM::actuallyComputeStableTimestep(const ProcessorGroup*,
                                           const PatchSubset* patches,
                                           const MaterialSubset*,
                                           DataWarehouse* old_dw,
                                           DataWarehouse* new_dw)
{
  for(int p=0;p<patches->size();p++){
   const Patch* patch = patches->get(p);
   if (cout_doing.active()) {
     cout_doing <<"Doing actuallyComputeStableTimestep on patch "
		<< patch->getID() <<"\t IMPM"<< "\n" << "\n";
   }

   if(d_numIterations==0){
     new_dw->put(delt_vartype(d_initialDt), lb->delTLabel, patch->getLevel());
   }
   else{
    Vector dx = patch->dCell();
    delt_vartype old_delT;
    old_dw->get(old_delT, d_sharedState->get_delt_label(), patch->getLevel());

    int numMPMMatls=d_sharedState->getNumMPMMatls();
    for(int m = 0; m < numMPMMatls; m++){
      MPMMaterial* mpm_matl = d_sharedState->getMPMMaterial( m );
      int dwindex = mpm_matl->getDWIndex();

      ParticleSubset* pset = new_dw->getParticleSubset(dwindex, patch);

      constParticleVariable<Vector> pvelocity;
      new_dw->get(pvelocity, lb->pVelocityLabel, pset);

      Vector ParticleSpeed(1.e-12,1.e-12,1.e-12);

      for(ParticleSubset::iterator iter=pset->begin();iter!=pset->end();iter++){
        particleIndex idx = *iter;
        ParticleSpeed=Vector(Max(fabs(pvelocity[idx].x()),ParticleSpeed.x()),
                             Max(fabs(pvelocity[idx].y()),ParticleSpeed.y()),
                             Max(fabs(pvelocity[idx].z()),ParticleSpeed.z()));
      }
      ParticleSpeed = dx/ParticleSpeed;
      double delT_new = .8*ParticleSpeed.minComponent();

      double old_dt=old_delT;
      if(d_numIterations <= flags->d_num_iters_to_increase_delT){
        old_dt = flags->d_delT_increase_factor*old_delT;
      }
      if(d_numIterations >= flags->d_num_iters_to_decrease_delT){
        old_dt = flags->d_delT_decrease_factor*old_delT;
      }
      delT_new = min(delT_new, old_dt);

      new_dw->put(delt_vartype(delT_new), lb->delTLabel, patch->getLevel());
    }
   }
  }
}

double ImpMPM::recomputeTimestep(double current_dt)
{
  return current_dt*flags->d_delT_decrease_factor;
}
