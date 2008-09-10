//----- PressureSolver.cc ----------------------------------------------

#include <sci_defs/hypre_defs.h>

#include <Packages/Uintah/CCA/Components/Arches/PressureSolver.h>
#include <Packages/Uintah/CCA/Components/Arches/Arches.h>
#include <Packages/Uintah/CCA/Components/Arches/ArchesLabel.h>
#include <Packages/Uintah/CCA/Components/Arches/ArchesMaterial.h>
#include <Packages/Uintah/CCA/Components/Arches/ArchesVariables.h>
#include <Packages/Uintah/CCA/Components/Arches/ArchesConstVariables.h>
#include <Packages/Uintah/CCA/Components/Arches/BoundaryCondition.h>
#include <Packages/Uintah/CCA/Components/Arches/CellInformationP.h>
#include <Packages/Uintah/CCA/Components/Arches/Discretization.h>
#include <Packages/Uintah/CCA/Components/Arches/LinearSolver.h>
#include <Packages/Uintah/CCA/Components/Arches/PetscSolver.h>
#ifdef HAVE_HYPRE
#include <Packages/Uintah/CCA/Components/Arches/HypreSolver.h>
#endif
#include <Packages/Uintah/CCA/Components/Arches/PhysicalConstants.h>
#include <Packages/Uintah/CCA/Components/Arches/Source.h>
#include <Packages/Uintah/CCA/Components/Arches/ScaleSimilarityModel.h>
#include <Packages/Uintah/CCA/Components/Arches/TimeIntegratorLabel.h>
#include <Packages/Uintah/CCA/Components/MPMArches/MPMArchesLabel.h>
#include <Packages/Uintah/CCA/Ports/DataWarehouse.h>
#include <Packages/Uintah/CCA/Ports/LoadBalancer.h>
#include <Packages/Uintah/CCA/Ports/Scheduler.h>
#include <Packages/Uintah/Core/Exceptions/InvalidValue.h>
#include <Packages/Uintah/Core/Exceptions/VariableNotFoundInGrid.h>
#include <Packages/Uintah/Core/Grid/Variables/CCVariable.h>
#include <Packages/Uintah/Core/Grid/Variables/CellIterator.h>
#include <Packages/Uintah/Core/Grid/Level.h>
#include <Packages/Uintah/Core/Grid/Variables/PerPatch.h>
#include <Packages/Uintah/Core/Grid/Variables/SFCXVariable.h>
#include <Packages/Uintah/Core/Grid/Variables/SFCYVariable.h>
#include <Packages/Uintah/Core/Grid/Variables/SFCZVariable.h>
#include <Packages/Uintah/Core/Grid/SimulationState.h>
#include <Packages/Uintah/Core/Grid/Task.h>
#include <Packages/Uintah/Core/Grid/Variables/VarTypes.h>
#include <Packages/Uintah/Core/Parallel/ProcessorGroup.h>
#include <Packages/Uintah/Core/ProblemSpec/ProblemSpec.h>

using namespace Uintah;
using namespace std;

#include <Packages/Uintah/CCA/Components/Arches/fortran/add_hydrostatic_term_topressure_fort.h>

// ****************************************************************************
// Default constructor for PressureSolver
// ****************************************************************************
PressureSolver::PressureSolver(const ArchesLabel* label,
                               const MPMArchesLabel* MAlb,
                               BoundaryCondition* bndry_cond,
                               PhysicalConstants* phys_const,
                               const ProcessorGroup* myworld):
                                     d_lab(label), d_MAlab(MAlb),
                                     d_boundaryCondition(bndry_cond),
                                     d_physicalConsts(phys_const),
                                     d_myworld(myworld)
{
  d_perproc_patches=0;
  d_discretize = 0;
  d_source = 0;
  d_linearSolver = 0; 
}

// ****************************************************************************
// Destructor
// ****************************************************************************
PressureSolver::~PressureSolver()
{
  if(d_perproc_patches && d_perproc_patches->removeReference())
    delete d_perproc_patches;
  delete d_discretize;
  delete d_source;
  delete d_linearSolver;
}

// ****************************************************************************
// Problem Setup
// ****************************************************************************
void 
PressureSolver::problemSetup(const ProblemSpecP& params)
{
  ProblemSpecP db = params->findBlock("PressureSolver");
  d_pressRef = d_physicalConsts->getRefPoint();
  db->getWithDefault("normalize_pressure",      d_norm_pres, false);
  db->getWithDefault("do_only_last_projection", d_do_only_last_projection, false);

  d_discretize = scinew Discretization();

  // make source and boundary_condition objects
  d_source = scinew Source(d_physicalConsts);
  if (d_doMMS){
    d_source->problemSetup(db);
  }
  string linear_sol;
  db->require("linear_solver", linear_sol);
  if (linear_sol == "petsc"){
    d_linearSolver = scinew PetscSolver(d_myworld);
  }
#ifdef HAVE_HYPRE
  else if (linear_sol == "hypre"){
    d_linearSolver = scinew HypreSolver(d_myworld);
  }
#endif
  else {
    throw InvalidValue("Linear solver option"
                       " not supported" + linear_sol, __FILE__, __LINE__);
  }
  d_linearSolver->problemSetup(db);
}
// ****************************************************************************
// Schedule solve of linearized pressure equation
// ****************************************************************************
void PressureSolver::sched_solve(const LevelP& level,
                           SchedulerP& sched,
                           const TimeIntegratorLabel* timelabels,
                           bool extraProjection,
                           bool d_EKTCorrection,
                           bool doing_EKT_now)
{
  const PatchSet* patches = level->eachPatch();
  const MaterialSet* matls = d_lab->d_sharedState->allArchesMaterials();

  sched_buildLinearMatrix(sched, patches, matls, timelabels, extraProjection,
                         d_EKTCorrection, doing_EKT_now);

  sched_pressureLinearSolve(level, sched, timelabels, extraProjection,
                            d_EKTCorrection, doing_EKT_now);

  if ((d_MAlab)&&(!(extraProjection))) {
      sched_addHydrostaticTermtoPressure(sched, patches, matls, timelabels);
  }
}

// ****************************************************************************
// Schedule build of linear matrix
// ****************************************************************************
void 
PressureSolver::sched_buildLinearMatrix(SchedulerP& sched,
                                        const PatchSet* patches,
                                        const MaterialSet* matls,
                                        const TimeIntegratorLabel* timelabels,
                                        bool extraProjection,
                                        bool d_EKTCorrection,
                                        bool doing_EKT_now)
{
  //  build pressure equation coefficients and source
  string taskname =  "PressureSolver::buildLinearMatrix" +
                     timelabels->integrator_step_name;
  if (extraProjection){
    taskname += "extraProjection";
  }
  if (doing_EKT_now){
    taskname += "EKTnow";
  }
  Task* tsk = scinew Task(taskname, this,
                          &PressureSolver::buildLinearMatrix,
                          timelabels, extraProjection,
                          d_EKTCorrection, doing_EKT_now);
    

  Task::WhichDW parent_old_dw;
  if (timelabels->recursion){
    parent_old_dw = Task::ParentOldDW;
  }else{
    parent_old_dw = Task::OldDW;
  }
  
  Ghost::GhostType  gac = Ghost::AroundCells;
  Ghost::GhostType  gn  = Ghost::None;
  Ghost::GhostType  gaf = Ghost::AroundFaces;
  Task::DomainSpec oams = Task::OutOfDomain;  //outside of arches matlSet.
  
  tsk->requires(parent_old_dw, d_lab->d_sharedState->get_delt_label());
  tsk->requires(Task::NewDW, d_lab->d_cellTypeLabel,       gac, 1);

  if (timelabels->integrator_step_number == TimeIntegratorStepNumber::First){
    tsk->requires(Task::OldDW, timelabels->pressure_guess, gn, 0);
  }else{
    tsk->requires(Task::NewDW, timelabels->pressure_guess, gn, 0);
  }
  
  tsk->requires(Task::NewDW, d_lab->d_densityCPLabel,      gac, 1);
  tsk->requires(Task::NewDW, d_lab->d_uVelRhoHatLabel,     gaf, 1);
  tsk->requires(Task::NewDW, d_lab->d_vVelRhoHatLabel,     gaf, 1);
  tsk->requires(Task::NewDW, d_lab->d_wVelRhoHatLabel,     gaf, 1);
  // get drhodt that goes in the rhs of the pressure equation
  tsk->requires(Task::NewDW, d_lab->d_filterdrhodtLabel,   gn, 0);
#ifdef divergenceconstraint
  tsk->requires(Task::NewDW, d_lab->d_divConstraintLabel,  gn, 0);
#endif
  if (d_MAlab) {
    tsk->requires(Task::NewDW, d_lab->d_mmgasVolFracLabel, gac, 1);
  }

  if ((timelabels->integrator_step_number == TimeIntegratorStepNumber::First)
      &&(((!(extraProjection))&&(!(d_EKTCorrection)))
         ||((d_EKTCorrection)&&(doing_EKT_now)))) {
    tsk->computes(d_lab->d_presCoefPBLMLabel, d_lab->d_stencilMatl, oams);
    tsk->computes(d_lab->d_presNonLinSrcPBLMLabel);
  } else {
    tsk->modifies(d_lab->d_presCoefPBLMLabel, d_lab->d_stencilMatl, oams);
    tsk->modifies(d_lab->d_presNonLinSrcPBLMLabel);
  }
  

  sched->addTask(tsk, patches, matls);
}

// ****************************************************************************
// Actually build of linear matrix for pressure equation
// ****************************************************************************
void 
PressureSolver::buildLinearMatrix(const ProcessorGroup* pc,
                                  const PatchSubset* patches,
                                  const MaterialSubset* /* matls */,
                                  DataWarehouse* old_dw,
                                  DataWarehouse* new_dw,
                                  const TimeIntegratorLabel* timelabels,
                                  bool extraProjection,
                                  bool d_EKTCorrection,
                                  bool doing_EKT_now)
{
  DataWarehouse* parent_old_dw;
  if (timelabels->recursion){
    parent_old_dw = new_dw->getOtherDataWarehouse(Task::ParentOldDW);
  }else{
    parent_old_dw = old_dw;
  }
  
  delt_vartype delT;
  parent_old_dw->get(delT, d_lab->d_sharedState->get_delt_label() );
  double delta_t = delT;
  delta_t *= timelabels->time_multiplier;

  
  for (int p = 0; p < patches->size(); p++) {
    const Patch* patch = patches->get(p);
    int archIndex = 0; // only one arches material
    int indx = d_lab->d_sharedState->
                    getArchesMaterial(archIndex)->getDWIndex(); 
    ArchesVariables pressureVars;
    ArchesConstVariables constPressureVars;
    int nofStencils = 7;


    Ghost::GhostType  gn = Ghost::None;
    Ghost::GhostType  gac = Ghost::AroundCells;
    Ghost::GhostType  gaf = Ghost::AroundFaces;
    new_dw->get(constPressureVars.cellType, d_lab->d_cellTypeLabel, indx, patch, gac, 1);

    if (timelabels->integrator_step_number == TimeIntegratorStepNumber::First){
      old_dw->get(constPressureVars.pressure, timelabels->pressure_guess, indx, patch, gn, 0);
    }else{
      new_dw->get(constPressureVars.pressure, timelabels->pressure_guess, indx, patch, gn, 0);
    }

    new_dw->get(constPressureVars.density,      d_lab->d_densityCPLabel,   indx, patch, gac, 1);
    new_dw->get(constPressureVars.uVelRhoHat,   d_lab->d_uVelRhoHatLabel,  indx, patch, gaf, 1);
    new_dw->get(constPressureVars.vVelRhoHat,   d_lab->d_vVelRhoHatLabel,  indx, patch, gaf, 1);
    new_dw->get(constPressureVars.wVelRhoHat,   d_lab->d_wVelRhoHatLabel,  indx, patch, gaf, 1);
    new_dw->get(constPressureVars.filterdrhodt, d_lab->d_filterdrhodtLabel,indx, patch, gn, 0);
#ifdef divergenceconstraint
    new_dw->get(constPressureVars.divergence,   d_lab->d_divConstraintLabel,indx, patch, gn, 0);
#endif


    PerPatch<CellInformationP> cellInfoP;
    if (new_dw->exists(d_lab->d_cellInfoLabel, indx, patch)){ 
      new_dw->get(cellInfoP, d_lab->d_cellInfoLabel, indx, patch);
    }else{ 
      throw VariableNotFoundInGrid("cellInformation"," ", __FILE__, __LINE__);
    }
    CellInformation* cellinfo = cellInfoP.get().get_rep();

  
    // Calculate Pressure Coeffs
    for (int ii = 0; ii < nofStencils; ii++) {
      if ((timelabels->integrator_step_number == TimeIntegratorStepNumber::First)
          &&(((!(extraProjection))&&(!(d_EKTCorrection)))
             ||((d_EKTCorrection)&&(doing_EKT_now)))){
        new_dw->allocateAndPut(pressureVars.pressCoeff[ii],
                               d_lab->d_presCoefPBLMLabel, ii, patch);
      }else{
        new_dw->getModifiable(pressureVars.pressCoeff[ii],
                              d_lab->d_presCoefPBLMLabel, ii, patch);
      }
      pressureVars.pressCoeff[ii].initialize(0.0);
    }

    d_discretize->calculatePressureCoeff(patch, cellinfo, &pressureVars, &constPressureVars);

    // Modify pressure coefficients for multimaterial formulation

    if (d_MAlab) {
      new_dw->get(constPressureVars.voidFraction,
                  d_lab->d_mmgasVolFracLabel, indx, patch,
                  gac, 1);

      d_discretize->mmModifyPressureCoeffs(patch, &pressureVars,
                                           &constPressureVars);

    }

    // Calculate Pressure Source
    //  inputs : pressureSPBC, [u,v,w]VelocitySIVBC, densityCP,
    //           [u,v,w]VelCoefPBLM, [u,v,w]VelNonLinSrcPBLM
    //  outputs: presLinSrcPBLM, presNonLinSrcPBLM
    // Allocate space

    new_dw->allocateTemporary(pressureVars.pressLinearSrc,  patch);
    pressureVars.pressLinearSrc.initialize(0.0);
    
    if ((timelabels->integrator_step_number == TimeIntegratorStepNumber::First)
        &&(((!(extraProjection))&&(!(d_EKTCorrection)))
           ||((d_EKTCorrection)&&(doing_EKT_now))))
      new_dw->allocateAndPut(pressureVars.pressNonlinearSrc,
                             d_lab->d_presNonLinSrcPBLMLabel, indx, patch);
    else
      new_dw->getModifiable(pressureVars.pressNonlinearSrc,
                            d_lab->d_presNonLinSrcPBLMLabel, indx, patch);
    pressureVars.pressNonlinearSrc.initialize(0.0);

    d_source->calculatePressureSourcePred(pc, patch, delta_t,
                                          cellinfo, &pressureVars,
                                          &constPressureVars,
                                          doing_EKT_now);

    if (d_doMMS){
      d_source->calculatePressMMSSourcePred(pc, patch, delta_t,
                                            cellinfo, &pressureVars,
                                            &constPressureVars);
    }

    // Calculate Pressure BC
    //  inputs : pressureIN, presCoefPBLM
    //  outputs: presCoefPBLM


    // do multimaterial bc; this is done before 
    // calculatePressDiagonal because unlike the outlet
    // boundaries in the explicit projection, we want to 
    // show the effect of AE, etc. in AP for the 
    // intrusion boundaries
    if (d_boundaryCondition->anyArchesPhysicalBC())
      /*if (d_boundaryCondition->getIntrusionBC())
        d_boundaryCondition->intrusionPressureBC(pc, patch, cellinfo,
                                                 &pressureVars,&constPressureVars);*/
    
    if (d_MAlab){
      d_boundaryCondition->mmpressureBC(pc, patch, cellinfo,
                                        &pressureVars, &constPressureVars);
    }
    // Calculate Pressure Diagonal
    d_discretize->calculatePressDiagonal(patch,&pressureVars);

    if (d_boundaryCondition->anyArchesPhysicalBC()){
      d_boundaryCondition->pressureBC(pc, patch, old_dw, new_dw, 
                                      cellinfo, &pressureVars,&constPressureVars);
    }
  }

}

// ****************************************************************************
// Schedule solver for linear matrix
// ****************************************************************************
void 
PressureSolver::sched_pressureLinearSolve(const LevelP& level,
                                          SchedulerP& sched,
                                          const TimeIntegratorLabel* timelabels,
                                          bool extraProjection,
                                          bool d_EKTCorrection,
                                          bool doing_EKT_now)
{
  if(d_perproc_patches && d_perproc_patches->removeReference())
    delete d_perproc_patches;

  LoadBalancer* lb = sched->getLoadBalancer();
  d_perproc_patches = lb->getPerProcessorPatchSet(level);
  d_perproc_patches->addReference();
  const MaterialSet* matls = d_lab->d_sharedState->allArchesMaterials();

  string taskname =  "PressureSolver::PressLinearSolve_all" + 
                     timelabels->integrator_step_name;
  if (extraProjection) taskname += "extraProjection";
  if (doing_EKT_now) taskname += "EKTnow";
  Task* tsk = scinew Task(taskname, this,
                          &PressureSolver::pressureLinearSolve_all,
                          timelabels, extraProjection,
                          d_EKTCorrection, doing_EKT_now);

  // Requires
  // coefficient for the variable for which solve is invoked

  Ghost::GhostType  gn = Ghost::None;
  Task::DomainSpec oams = Task::OutOfDomain;  //outside of arches matlSet.
  if (!((d_pressure_correction)||(extraProjection)
        ||((d_EKTCorrection)&&(doing_EKT_now)))){
    if (timelabels->integrator_step_number == TimeIntegratorStepNumber::First){
      tsk->requires(Task::OldDW, timelabels->pressure_guess, gn, 0);
    }else{
      tsk->requires(Task::NewDW, timelabels->pressure_guess, gn, 0);
    }
  }
  tsk->requires(Task::NewDW, d_lab->d_presCoefPBLMLabel, d_lab->d_stencilMatl,oams, gn, 0);
  tsk->requires(Task::NewDW, d_lab->d_presNonLinSrcPBLMLabel,gn, 0);


  if ((extraProjection)||(doing_EKT_now)){
    if (timelabels->integrator_step_number == TimeIntegratorStepNumber::First){
      tsk->computes(d_lab->d_pressureExtraProjectionLabel);
    }else{
      tsk->modifies(d_lab->d_pressureExtraProjectionLabel);
    }
  }else {
    tsk->computes(timelabels->pressure_out);
    if (timelabels->recursion){
      tsk->computes(d_lab->d_InitNormLabel);
    }
  }


  sched->addTask(tsk, d_perproc_patches, matls);

  const Patch* d_pressRefPatch = 0;
  for(Level::const_patchIterator iter=level->patchesBegin();
      iter != level->patchesEnd(); iter++){

    const Patch* patch=*iter;
    if(patch->containsCell(d_pressRef)){
      d_pressRefPatch = patch;
    }
  }
  if(!d_pressRefPatch)
    throw InternalError("Patch containing pressure (and density) reference point was not found",
                        __FILE__, __LINE__);

  d_pressRefProc = lb->getPatchwiseProcessorAssignment(d_pressRefPatch);
}

//______________________________________________________________________
//
void 
PressureSolver::pressureLinearSolve_all(const ProcessorGroup* pg,
                                        const PatchSubset* patches,
                                        const MaterialSubset*,
                                        DataWarehouse* old_dw,
                                        DataWarehouse* new_dw,
                                        const TimeIntegratorLabel* timelabels,
                                        bool extraProjection,
                                        bool d_EKTCorrection,
                                        bool doing_EKT_now)
{
  int archIndex = 0; // only one arches material
  int indx = d_lab->d_sharedState->getArchesMaterial(archIndex)->getDWIndex(); 
  ArchesVariables pressureVars;
  int me = pg->myrank();
  // initializeMatrix...
  d_linearSolver->matrixCreate(d_perproc_patches, patches);
  for (int p = 0; p < patches->size(); p++) {
    const Patch *patch = patches->get(p);
    // This calls fillRows on linear(petsc) solver
    pressureLinearSolve(pg, patch, indx, old_dw, new_dw, pressureVars,
                        timelabels, extraProjection,
                        d_EKTCorrection, doing_EKT_now);
  }
  
  bool converged =  d_linearSolver->pressLinearSolve();
  if (converged) {
    for (int p = 0; p < patches->size(); p++) {
      const Patch *patch = patches->get(p);
      d_linearSolver->copyPressSoln(patch, &pressureVars);
    }
  } else {
    if (pg->myrank() == 0){
      cerr << "pressure solver not converged, using old values" << endl;
    }
    throw InternalError("pressure solver is diverging", __FILE__, __LINE__);
  }
  
  
  double init_norm = d_linearSolver->getInitNorm();
  if (timelabels->recursion){
    new_dw->put(max_vartype(init_norm), d_lab->d_InitNormLabel);
  }
  
  if(d_pressRefProc == me){
    CCVariable<double> pressure;
    pressure.copyPointer(pressureVars.pressure);
    pressureVars.press_ref = pressure[d_pressRef];
    cerr << "press_ref for norm: " << pressureVars.press_ref << " " <<
      d_pressRefProc << endl;
  }
  MPI_Bcast(&pressureVars.press_ref, 1, MPI_DOUBLE, d_pressRefProc, pg->getComm());
  
  if (d_norm_pres){ 
    for (int p = 0; p < patches->size(); p++) {
      const Patch *patch = patches->get(p);
      normPressure(patch, &pressureVars);
      //    updatePressure(pg, patch, &pressureVars);
      // put back the results
    }
  }

  if (d_do_only_last_projection){
    if ((timelabels->integrator_step_name == "Predictor")||
        (timelabels->integrator_step_name == "Intermediate")) {
      pressureVars.pressure.initialize(0.0);
      if (pg->myrank() == 0){
        cout << "Projection skipped" << endl;
      }
    }else{ 
      if (!((timelabels->integrator_step_name == "Corrector")||
            (timelabels->integrator_step_name == "CorrectorRK3"))){
        throw InvalidValue("Projection can only be skipped for RK SSP methods",__FILE__, __LINE__); 
      }
    }
  }
  // destroy matrix
  d_linearSolver->destroyMatrix();
}


//______________________________________________________________________
// Actual linear solve
void 
PressureSolver::pressureLinearSolve(const ProcessorGroup* pc,
                                    const Patch* patch,
                                    const int indx,
                                    DataWarehouse* old_dw,
                                    DataWarehouse* new_dw,
                                    ArchesVariables& pressureVars,
                                    const TimeIntegratorLabel* timelabels,
                                    bool extraProjection,
                                    bool d_EKTCorrection,
                                    bool doing_EKT_now)
{
  ArchesConstVariables constPressureVars;
  // Get the required data
  Ghost::GhostType  gn = Ghost::None;
  if ((extraProjection)||(doing_EKT_now)){
    if (timelabels->integrator_step_number == TimeIntegratorStepNumber::First){
      new_dw->allocateAndPut(pressureVars.pressure, d_lab->d_pressureExtraProjectionLabel,indx, patch);
    }else{
      new_dw->getModifiable(pressureVars.pressure, d_lab->d_pressureExtraProjectionLabel, indx, patch);
    }
  }else{
    new_dw->allocateAndPut(pressureVars.pressure, timelabels->pressure_out, indx, patch);
  }
  
  if (!((d_pressure_correction)||(extraProjection)||((d_EKTCorrection)&&(doing_EKT_now)))){
    if (timelabels->integrator_step_number == TimeIntegratorStepNumber::First){
      old_dw->copyOut(pressureVars.pressure, timelabels->pressure_guess, indx, patch);
    }else{
      new_dw->copyOut(pressureVars.pressure, timelabels->pressure_guess, indx, patch);
    }
  }else{
    pressureVars.pressure.initialize(0.0);
  }
  
  for (int ii = 0; ii < d_lab->d_stencilMatl->size(); ii++){
    new_dw->get(constPressureVars.pressCoeff[ii], d_lab->d_presCoefPBLMLabel, 
                   ii, patch, gn, 0);
  }

  new_dw->get(constPressureVars.pressNonlinearSrc,d_lab->d_presNonLinSrcPBLMLabel, indx, patch, gn, 0);

  // for parallel code lisolve will become a recursive task and 
  // will make the following subroutine separate
  // get patch numer ***warning****
  // sets matrix
  d_linearSolver->setPressMatrix(pc, patch,
                                 &pressureVars, &constPressureVars, d_lab);
}

// ************************************************************************
// Schedule addition of hydrostatic term to relative pressure calculated
// in pressure solve
// ************************************************************************
void
PressureSolver::sched_addHydrostaticTermtoPressure(SchedulerP& sched, 
                                                   const PatchSet* patches,
                                                   const MaterialSet* matls,
                                                   const TimeIntegratorLabel* timelabels)

{
  Task* tsk = scinew Task("Psolve:addhydrostaticterm",
                          this, &PressureSolver::addHydrostaticTermtoPressure,
                          timelabels);

  Ghost::GhostType  gn = Ghost::None;
  tsk->requires(Task::OldDW, d_lab->d_pressurePSLabel,    gn, 0);
  tsk->requires(Task::OldDW, d_lab->d_densityMicroLabel,  gn, 0);
  tsk->requires(Task::NewDW, d_lab->d_cellTypeLabel,      gn, 0);

  if (timelabels->integrator_step_number == TimeIntegratorStepNumber::First){
    tsk->computes(d_lab->d_pressPlusHydroLabel);
  }else {
    tsk->modifies(d_lab->d_pressPlusHydroLabel);
  }

  sched->addTask(tsk, patches, matls);
}

// ****************************************************************************
// Actual addition of hydrostatic term to relative pressure
// ****************************************************************************
void 
PressureSolver::addHydrostaticTermtoPressure(const ProcessorGroup*,
                                             const PatchSubset* patches,
                                             const MaterialSubset*,
                                             DataWarehouse* old_dw,
                                             DataWarehouse* new_dw,
                                             const TimeIntegratorLabel* timelabels)
{
  for (int p = 0; p < patches->size(); p++) {

    const Patch* patch = patches->get(p);

    int archIndex = 0; // only one arches material
    int indx = d_lab->d_sharedState->getArchesMaterial(archIndex)->getDWIndex(); 

    constCCVariable<double> prel;
    CCVariable<double> pPlusHydro;
    constCCVariable<double> denMicro;
    constCCVariable<int> cellType;

    double gx = d_physicalConsts->getGravity(1);
    double gy = d_physicalConsts->getGravity(2);
    double gz = d_physicalConsts->getGravity(3);

    // Get the PerPatch CellInformation data
    PerPatch<CellInformationP> cellInfoP;
    if (new_dw->exists(d_lab->d_cellInfoLabel, indx, patch)){ 
      new_dw->get(cellInfoP, d_lab->d_cellInfoLabel, indx, patch);
    }else{ 
      throw VariableNotFoundInGrid("cellInformation"," ", __FILE__, __LINE__);
    }
    CellInformation* cellinfo = cellInfoP.get().get_rep();
    
    Ghost::GhostType  gn = Ghost::None;
    old_dw->get(prel,     d_lab->d_pressurePSLabel,     indx, patch, gn, 0);
    old_dw->get(denMicro, d_lab->d_densityMicroLabel,   indx, patch, gn, 0);
    new_dw->get(cellType, d_lab->d_cellTypeLabel,       indx, patch, gn, 0);

    if (timelabels->integrator_step_number == TimeIntegratorStepNumber::First){
      new_dw->allocateAndPut(pPlusHydro, d_lab->d_pressPlusHydroLabel,indx, patch);
    }else{
      new_dw->getModifiable(pPlusHydro,  d_lab->d_pressPlusHydroLabel,indx, patch);
    }

    IntVector valid_lo = patch->getFortranCellLowIndex__New();
    IntVector valid_hi = patch->getFortranCellHighIndex__New();

    int mmwallid = d_boundaryCondition->getMMWallId();

    pPlusHydro.initialize(0.0);

    fort_add_hydrostatic_term_topressure(pPlusHydro, prel, denMicro,
                                         gx, gy, gz, cellinfo->xx,
                                         cellinfo->yy, cellinfo->zz,
                                         valid_lo, valid_hi,
                                         cellType, mmwallid);
  }
}

//______________________________________________________________________
//  
void 
PressureSolver::normPressure(const Patch* patch,
                             ArchesVariables* vars)
{
  double pressref = vars->press_ref;
 
  for(CellIterator iter=patch->getExtraCellIterator__New(); !iter.done(); iter++){
    IntVector c = *iter;
    vars->pressure[c] = vars->pressure[c] - pressref;
  } 
 
  #if 0
  fort_normpress(idxLo, idxHi, vars->pressure, pressref);
  #endif
}  
//______________________________________________________________________

void 
PressureSolver::updatePressure(const Patch* patch,
                               ArchesVariables* vars)
{
  for(CellIterator iter=patch->getExtraCellIterator__New(); !iter.done(); iter++){
    IntVector c = *iter;
    vars->pressureNew[c] = vars->pressure[c];
  }
}  
