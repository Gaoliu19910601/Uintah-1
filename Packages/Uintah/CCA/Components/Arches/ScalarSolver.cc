//----- ScalarSolver.cc ----------------------------------------------

#include <Packages/Uintah/CCA/Components/Arches/ScalarSolver.h>
#include <Packages/Uintah/CCA/Components/Arches/Arches.h>
#include <Packages/Uintah/CCA/Components/Arches/ArchesLabel.h>
#include <Packages/Uintah/CCA/Components/Arches/ArchesMaterial.h>
#include <Packages/Uintah/CCA/Components/Arches/BoundaryCondition.h>
#include <Packages/Uintah/CCA/Components/Arches/CellInformationP.h>
#include <Packages/Uintah/CCA/Components/Arches/Discretization.h>
#include <Packages/Uintah/CCA/Components/Arches/PetscSolver.h>
#include <Packages/Uintah/CCA/Components/Arches/PhysicalConstants.h>
#include <Packages/Uintah/CCA/Components/Arches/RHSSolver.h>
#include <Packages/Uintah/CCA/Components/Arches/Source.h>
#include <Packages/Uintah/CCA/Components/Arches/TurbulenceModel.h>
#include <Packages/Uintah/CCA/Components/Arches/ScaleSimilarityModel.h>
#include <Packages/Uintah/CCA/Components/Arches/TimeIntegratorLabel.h>
#include <Packages/Uintah/CCA/Ports/DataWarehouse.h>
#include <Packages/Uintah/CCA/Ports/Scheduler.h>
#include <Packages/Uintah/Core/Exceptions/InvalidValue.h>
#include <Packages/Uintah/Core/Exceptions/VariableNotFoundInGrid.h>
#include <Packages/Uintah/Core/Grid/Variables/CCVariable.h>
#include <Packages/Uintah/Core/Grid/Level.h>
#include <Packages/Uintah/Core/Grid/Patch.h>
#include <Packages/Uintah/Core/Grid/Variables/PerPatch.h>
#include <Packages/Uintah/Core/Grid/Variables/SFCXVariable.h>
#include <Packages/Uintah/Core/Grid/Variables/SFCYVariable.h>
#include <Packages/Uintah/Core/Grid/Variables/SFCZVariable.h>
#include <Packages/Uintah/Core/Grid/SimulationState.h>
#include <Packages/Uintah/Core/Grid/Task.h>
#include <Packages/Uintah/Core/Grid/Variables/VarTypes.h>
#include <Packages/Uintah/Core/ProblemSpec/ProblemSpec.h>


using namespace Uintah;
using namespace std;

//****************************************************************************
// Default constructor for ScalarSolver
//****************************************************************************
ScalarSolver::ScalarSolver(const ArchesLabel* label,
                           const MPMArchesLabel* MAlb,
                           TurbulenceModel* turb_model,
                           BoundaryCondition* bndry_cond,
                           PhysicalConstants* physConst) :
                                 d_lab(label), d_MAlab(MAlb),
                                 d_turbModel(turb_model), 
                                 d_boundaryCondition(bndry_cond),
                                 d_physicalConsts(physConst)
{
  d_discretize = 0;
  d_source = 0;
  d_rhsSolver = 0;
}

//****************************************************************************
// Destructor
//****************************************************************************
ScalarSolver::~ScalarSolver()
{
  delete d_discretize;
  delete d_source;
  delete d_rhsSolver;
}

//****************************************************************************
// Problem Setup
//****************************************************************************
void 
ScalarSolver::problemSetup(const ProblemSpecP& params)
{
  ProblemSpecP db = params->findBlock("MixtureFractionSolver");

  d_discretize = scinew Discretization();

  string conv_scheme;
  db->getWithDefault("convection_scheme",conv_scheme,"central-upwind");
  
  if (conv_scheme == "central-upwind"){
    d_conv_scheme = 0;
  }else if (conv_scheme == "flux_limited"){
    d_conv_scheme = 1;
  }else {
    throw InvalidValue("Convection scheme not supported: " + conv_scheme, __FILE__, __LINE__);
  }
  
  string limiter_type;
  if (d_conv_scheme == 1) {
    db->getWithDefault("limiter_type",limiter_type,"superbee");
    if (limiter_type == "superbee"){
      d_limiter_type = 0;
    }else if (limiter_type == "vanLeer"){
      d_limiter_type = 1;
    }else if (limiter_type == "none") {
      d_limiter_type = 2;
      cout << "WARNING! Running central scheme for scalar," << endl;
      cout << "which can be unstable." << endl;
    }else if (limiter_type == "central-upwind"){
      d_limiter_type = 3;
    }else if (limiter_type == "upwind"){
      d_limiter_type = 4;
    }else{ 
      throw InvalidValue("Flux limiter type "
                                           "not supported: " + limiter_type, __FILE__, __LINE__);
    }
    
    string boundary_limiter_type;
    d_boundary_limiter_type = 3;
    if (d_limiter_type < 3) {
      db->getWithDefault("boundary_limiter_type",boundary_limiter_type,"central-upwind");
      if (boundary_limiter_type == "none") {
            d_boundary_limiter_type = 2;
            cout << "WARNING! Running central scheme for scalar on the boundaries," << endl;
            cout << "which can be unstable." << endl;
      }else if (boundary_limiter_type == "central-upwind"){
        d_boundary_limiter_type = 3;
      }else if (boundary_limiter_type == "upwind"){
        d_boundary_limiter_type = 4;
      }else{
       throw InvalidValue("Flux limiter type on the boundary"
                                    "not supported: " + boundary_limiter_type, __FILE__, __LINE__);
      }
      d_central_limiter = false;
      if (d_limiter_type < 2){
        db->getWithDefault("central_limiter",d_central_limiter,false);
      }
    }
  }

  // make source and boundary_condition objects
  d_source = scinew Source(d_physicalConsts);
  
  if (d_doMMS){
    d_source->problemSetup(db);
  }
  d_rhsSolver = scinew RHSSolver();

  d_dynScalarModel = d_turbModel->getDynScalarModel();
  double model_turbPrNo;
  model_turbPrNo = d_turbModel->getTurbulentPrandtlNumber();

  // see if Prandtl number gets overridden here
  d_turbPrNo = 0.0;
  if (!(d_dynScalarModel)) {
    if (db->findBlock("turbulentPrandtlNumber")){
      db->getWithDefault("turbulentPrandtlNumber",d_turbPrNo,0.4);
    }
    // if it is not set in both places
    if ((d_turbPrNo == 0.0)&&(model_turbPrNo == 0.0)){
          throw InvalidValue("Turbulent Prandtl number is not specified for"
                             "mixture fraction ", __FILE__, __LINE__);
    // if it is set in turbulence model
    }else if (d_turbPrNo == 0.0){
      d_turbPrNo = model_turbPrNo;
    }

    // if it is set here or set in both places, 
    // we only need to set mixture fraction Pr number in turbulence model
    if (!(model_turbPrNo == d_turbPrNo)) {
      cout << "Turbulent Prandtl number for mixture fraction is set to "
      << d_turbPrNo << endl;
      d_turbModel->setTurbulentPrandtlNumber(d_turbPrNo);
    }
  }

// ++ jeremy ++ 
  d_source->setBoundary(d_boundaryCondition);
// -- jeremy --        

  d_discretize->setTurbulentPrandtlNumber(d_turbPrNo);
}

//****************************************************************************
// Schedule solve of linearized scalar equation
//****************************************************************************
void 
ScalarSolver::solve(SchedulerP& sched,
                    const PatchSet* patches,
                    const MaterialSet* matls,
                    const TimeIntegratorLabel* timelabels,
                    bool d_EKTCorrection,
                    bool doing_EKT_now)
{
  //computes stencil coefficients and source terms
  // requires : scalarIN, [u,v,w]VelocitySPBC, densityIN, viscosityIN
  // computes : scalCoefSBLM, scalLinSrcSBLM, scalNonLinSrcSBLM
  sched_buildLinearMatrix(sched, patches, matls, timelabels, d_EKTCorrection,
                          doing_EKT_now);
  
  // Schedule the scalar solve
  // require : scalarIN, scalCoefSBLM, scalNonLinSrcSBLM
  sched_scalarLinearSolve(sched, patches, matls, timelabels, d_EKTCorrection,
                          doing_EKT_now);
}

//****************************************************************************
// Schedule build of linear matrix
//****************************************************************************
void 
ScalarSolver::sched_buildLinearMatrix(SchedulerP& sched,
                                      const PatchSet* patches,
                                      const MaterialSet* matls,
                                      const TimeIntegratorLabel* timelabels,
                                      bool d_EKTCorrection,
                                      bool doing_EKT_now)
{
  string taskname =  "ScalarSolver::BuildCoeff" +
                     timelabels->integrator_step_name;
  if (doing_EKT_now) taskname += "EKTnow";
  Task* tsk = scinew Task(taskname, this,
                          &ScalarSolver::buildLinearMatrix,
                          timelabels, d_EKTCorrection, doing_EKT_now);


  Task::WhichDW parent_old_dw;
  if (timelabels->recursion){
    parent_old_dw = Task::ParentOldDW;
  }else{
    parent_old_dw = Task::OldDW;
  }
  
  tsk->requires(parent_old_dw, d_lab->d_sharedState->get_delt_label());
  
  // This task requires scalar and density from old time step for transient
  // calculation
  //DataWarehouseP old_dw = new_dw->getTop();
  Ghost::GhostType  gac = Ghost::AroundCells;
  Ghost::GhostType  gaf = Ghost::AroundFaces;
  Ghost::GhostType  gn = Ghost::None;  
  Task::DomainSpec oams = Task::OutOfDomain;  //outside of arches matlSet.
  
  tsk->requires(Task::NewDW, d_lab->d_cellTypeLabel,  gac, 1);
  tsk->requires(Task::NewDW, d_lab->d_scalarSPLabel,  gac, 2);
  tsk->requires(Task::NewDW, d_lab->d_densityCPLabel, gac, 2);

  Task::WhichDW old_values_dw;
  if (timelabels->use_old_values){
    old_values_dw = parent_old_dw;
  }else{ 
    old_values_dw = Task::NewDW;
  }
  tsk->requires(old_values_dw, d_lab->d_scalarSPLabel,  gn, 0);
  tsk->requires(old_values_dw, d_lab->d_densityCPLabel, gn, 0);

  if (d_dynScalarModel){
    tsk->requires(Task::NewDW, d_lab->d_scalarDiffusivityLabel,gac, 2);
  }else{
    tsk->requires(Task::NewDW, d_lab->d_viscosityCTSLabel,     gac, 2);
  }
  tsk->requires(Task::NewDW, d_lab->d_uVelocitySPBCLabel, gaf, 1);
  tsk->requires(Task::NewDW, d_lab->d_vVelocitySPBCLabel, gaf, 1);
  tsk->requires(Task::NewDW, d_lab->d_wVelocitySPBCLabel, gaf, 1);

  if (dynamic_cast<const ScaleSimilarityModel*>(d_turbModel)) {
    if (timelabels->integrator_step_number == TimeIntegratorStepNumber::First){
      tsk->requires(Task::OldDW, d_lab->d_scalarFluxCompLabel,
          d_lab->d_vectorMatl, oams, gac, 1);
    }else{
      tsk->requires(Task::NewDW, d_lab->d_scalarFluxCompLabel,
          d_lab->d_vectorMatl, oams,gac, 1);
    }
  }

      // added one more argument of index to specify scalar component
  if ((timelabels->integrator_step_number == TimeIntegratorStepNumber::First)
      &&((!(d_EKTCorrection))||((d_EKTCorrection)&&(doing_EKT_now)))) {
    tsk->computes(d_lab->d_scalCoefSBLMLabel, d_lab->d_stencilMatl, oams);
    tsk->computes(d_lab->d_scalDiffCoefLabel, d_lab->d_stencilMatl, oams);
    tsk->computes(d_lab->d_scalNonLinSrcSBLMLabel);
//#ifdef divergenceconstraint
    tsk->computes(d_lab->d_scalDiffCoefSrcLabel);
//#endif
    tsk->modifies(d_lab->d_scalarBoundarySrcLabel);
  }else {
    tsk->modifies(d_lab->d_scalCoefSBLMLabel, d_lab->d_stencilMatl, oams);
    tsk->modifies(d_lab->d_scalDiffCoefLabel, d_lab->d_stencilMatl, oams);
    tsk->modifies(d_lab->d_scalNonLinSrcSBLMLabel);
//#ifdef divergenceconstraint
    tsk->modifies(d_lab->d_scalDiffCoefSrcLabel);
//#endif
    tsk->modifies(d_lab->d_scalarBoundarySrcLabel);
  }
  if (doing_EKT_now){
    if (timelabels->integrator_step_number == TimeIntegratorStepNumber::First)
      tsk->computes(d_lab->d_scalarEKTLabel);
    else{
      tsk->modifies(d_lab->d_scalarEKTLabel);
    }
  }       
  sched->addTask(tsk, patches, matls);
}

      
//****************************************************************************
// Actually build linear matrix
//****************************************************************************
void ScalarSolver::buildLinearMatrix(const ProcessorGroup* pc,
                                     const PatchSubset* patches,
                                     const MaterialSubset*,
                                     DataWarehouse* old_dw,
                                     DataWarehouse* new_dw,
                                     const TimeIntegratorLabel* timelabels,
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
    ArchesVariables scalarVars;
    ArchesConstVariables constScalarVars;
    
    // Get the PerPatch CellInformation data
    PerPatch<CellInformationP> cellInfoP;
    if (new_dw->exists(d_lab->d_cellInfoLabel, indx, patch)){ 
      new_dw->get(cellInfoP, d_lab->d_cellInfoLabel, indx, patch);
    }else{ 
      throw VariableNotFoundInGrid("cellInformation"," ", __FILE__, __LINE__);
    }


    DataWarehouse* old_values_dw;
    if (timelabels->use_old_values){
      old_values_dw = parent_old_dw;
    }else{ 
      old_values_dw = new_dw;
    }
    
    //__________________________________
    Ghost::GhostType  gac = Ghost::AroundCells;
    Ghost::GhostType  gaf = Ghost::AroundFaces;
    Ghost::GhostType  gn = Ghost::None;

    CellInformation* cellinfo = cellInfoP.get().get_rep();
    new_dw->get(constScalarVars.cellType,           d_lab->d_cellTypeLabel, indx, patch, gac, 1);
    
    old_values_dw->get(constScalarVars.old_scalar,  d_lab->d_scalarSPLabel,  indx, patch, gn, 0);
    old_values_dw->get(constScalarVars.old_density, d_lab->d_densityCPLabel, indx, patch, gn, 0);
  
    // from new_dw get DEN, VIS, F, U, V, W
    new_dw->get(constScalarVars.density, d_lab->d_densityCPLabel,            indx, patch, gac, 2);

    if (d_dynScalarModel){
      new_dw->get(constScalarVars.viscosity, d_lab->d_scalarDiffusivityLabel,indx, patch, gac, 2);
    }else{
      new_dw->get(constScalarVars.viscosity, d_lab->d_viscosityCTSLabel,     indx, patch, gac, 2);
    }
    new_dw->get(constScalarVars.scalar,    d_lab->d_scalarSPLabel,      indx, patch, gac, 2);
    // for explicit get old values
    new_dw->get(constScalarVars.uVelocity, d_lab->d_uVelocitySPBCLabel, indx, patch, gaf, 1);
    new_dw->get(constScalarVars.vVelocity, d_lab->d_vVelocitySPBCLabel, indx, patch, gaf, 1);
    new_dw->get(constScalarVars.wVelocity, d_lab->d_wVelocitySPBCLabel, indx, patch, gaf, 1);

   //__________________________________
   // allocate matrix coeffs
  if ((timelabels->integrator_step_number == TimeIntegratorStepNumber::First)
      &&((!(d_EKTCorrection))||((d_EKTCorrection)&&(doing_EKT_now)))) {
      
    for (int ii = 0; ii < d_lab->d_stencilMatl->size(); ii++) {
      new_dw->allocateAndPut(scalarVars.scalarCoeff[ii],
                             d_lab->d_scalCoefSBLMLabel, ii, patch);
      scalarVars.scalarCoeff[ii].initialize(0.0);
      
      new_dw->allocateAndPut(scalarVars.scalarDiffusionCoeff[ii],
                             d_lab->d_scalDiffCoefLabel, ii, patch);
      scalarVars.scalarDiffusionCoeff[ii].initialize(0.0);
    }
    new_dw->allocateAndPut(scalarVars.scalarNonlinearSrc,
                           d_lab->d_scalNonLinSrcSBLMLabel, indx, patch);
    scalarVars.scalarNonlinearSrc.initialize(0.0);
//#ifdef divergenceconstraint
    new_dw->allocateAndPut(scalarVars.scalarDiffNonlinearSrc,
                           d_lab->d_scalDiffCoefSrcLabel, indx, patch);
    scalarVars.scalarDiffNonlinearSrc.initialize(0.0);
//#endif
        new_dw->getModifiable(scalarVars.scalarBoundarySrc,
                                d_lab->d_scalarBoundarySrcLabel, indx, patch);
  }else {
    for (int ii = 0; ii < d_lab->d_stencilMatl->size(); ii++) {
      new_dw->getModifiable(scalarVars.scalarCoeff[ii],
                            d_lab->d_scalCoefSBLMLabel, ii, patch);
      scalarVars.scalarCoeff[ii].initialize(0.0);
      
      new_dw->getModifiable(scalarVars.scalarDiffusionCoeff[ii],
                            d_lab->d_scalDiffCoefLabel, ii, patch);
      scalarVars.scalarDiffusionCoeff[ii].initialize(0.0);
    }
    new_dw->getModifiable(scalarVars.scalarNonlinearSrc,
                          d_lab->d_scalNonLinSrcSBLMLabel, indx, patch);
    scalarVars.scalarNonlinearSrc.initialize(0.0);
//#ifdef divergenceconstraint
    new_dw->getModifiable(scalarVars.scalarDiffNonlinearSrc,
                          d_lab->d_scalDiffCoefSrcLabel, indx, patch);
    scalarVars.scalarDiffNonlinearSrc.initialize(0.0);
//#endif
        new_dw->getModifiable(scalarVars.scalarBoundarySrc,
                        d_lab->d_scalarBoundarySrcLabel, indx, patch);
  }

  for (int ii = 0; ii < d_lab->d_stencilMatl->size(); ii++) {
    new_dw->allocateTemporary(scalarVars.scalarConvectCoeff[ii],  patch);
    scalarVars.scalarConvectCoeff[ii].initialize(0.0);
  }
  new_dw->allocateTemporary(scalarVars.scalarLinearSrc,  patch);
  scalarVars.scalarLinearSrc.initialize(0.0);
  // compute ith component of scalar stencil coefficients
  // inputs : scalarSP, [u,v,w]VelocityMS, densityCP, viscosityCTS
  // outputs: scalCoefSBLM
  d_discretize->calculateScalarCoeff(patch,
                                     cellinfo, 
                                     &scalarVars, &constScalarVars,
                                     d_conv_scheme);

  // Calculate scalar source terms
  // inputs : [u,v,w]VelocityMS, scalarSP, densityCP, viscosityCTS
  // outputs: scalLinSrcSBLM, scalNonLinSrcSBLM
  d_source->calculateScalarSource(pc, patch,
                                  delta_t, cellinfo, 
                                  &scalarVars, &constScalarVars);
   if (d_doMMS){
    d_source->calculateScalarMMSSource(pc, patch,
                                    delta_t, cellinfo, 
                                    &scalarVars, &constScalarVars);
    }
    if (d_conv_scheme > 0) {
      int wall_celltypeval = d_boundaryCondition->wallCellType();
      d_discretize->calculateScalarFluxLimitedConvection
                                                  (patch,  cellinfo,
                                                    &scalarVars, &constScalarVars,
                                                  wall_celltypeval, 
                                                  d_limiter_type,
                                                  d_boundary_limiter_type,
                                                  d_central_limiter); 
    } 

    // for scalesimilarity model add scalarflux to the source of scalar eqn.
    if (dynamic_cast<const ScaleSimilarityModel*>(d_turbModel)) {
      StencilMatrix<constCCVariable<double> > scalarFlux; //3 point stencil
      
      if (timelabels->integrator_step_number == TimeIntegratorStepNumber::First){
        for (int ii = 0; ii < d_lab->d_vectorMatl->size(); ii++) {
          old_dw->get(scalarFlux[ii], d_lab->d_scalarFluxCompLabel, ii, patch,gac, 1);
        }
      }else{
        for (int ii = 0; ii < d_lab->d_vectorMatl->size(); ii++) {
          new_dw->get(scalarFlux[ii], d_lab->d_scalarFluxCompLabel, ii, patch, gac, 1);
        }
      }
      IntVector indexLow = patch->getFortranCellLowIndex__New();
      IntVector indexHigh = patch->getFortranCellHighIndex__New();
      
      // set density for the whole domain
      
      
      // Store current cell
      double sue, suw, sun, sus, sut, sub;
      for (int colZ = indexLow.z(); colZ <= indexHigh.z(); colZ ++) {
        for (int colY = indexLow.y(); colY <= indexHigh.y(); colY ++) {
          for (int colX = indexLow.x(); colX <= indexHigh.x(); colX ++) {
            IntVector currCell(colX, colY, colZ);
            IntVector prevXCell(colX-1, colY, colZ);
            IntVector prevYCell(colX, colY-1, colZ);
            IntVector prevZCell(colX, colY, colZ-1);
            IntVector nextXCell(colX+1, colY, colZ);
            IntVector nextYCell(colX, colY+1, colZ);
            IntVector nextZCell(colX, colY, colZ+1);
            
            sue = 0.5*cellinfo->sns[colY]*cellinfo->stb[colZ]*
              ((scalarFlux[0])[currCell]+(scalarFlux[0])[nextXCell]);
            suw = 0.5*cellinfo->sns[colY]*cellinfo->stb[colZ]*
              ((scalarFlux[0])[prevXCell]+(scalarFlux[0])[currCell]);
            sun = 0.5*cellinfo->sew[colX]*cellinfo->stb[colZ]*
              ((scalarFlux[1])[currCell]+ (scalarFlux[1])[nextYCell]);
            sus = 0.5*cellinfo->sew[colX]*cellinfo->stb[colZ]*
              ((scalarFlux[1])[currCell]+(scalarFlux[1])[prevYCell]);
            sut = 0.5*cellinfo->sns[colY]*cellinfo->sew[colX]*
              ((scalarFlux[2])[currCell]+ (scalarFlux[2])[nextZCell]);
            sub = 0.5*cellinfo->sns[colY]*cellinfo->sew[colX]*
              ((scalarFlux[2])[currCell]+ (scalarFlux[2])[prevZCell]);
#if 1
            scalarVars.scalarNonlinearSrc[currCell] += suw-sue+sus-sun+sub-sut;
#ifdef divergenceconstraint
            scalarVars.scalarDiffNonlinearSrc[currCell] = suw-sue+sus-sun+sub-sut;
#endif
#endif
          }
        }
      }
    }
    // Calculate the scalar boundary conditions
    // inputs : scalarSP, scalCoefSBLM
    // outputs: scalCoefSBLM
    
    
    if (d_boundaryCondition->anyArchesPhysicalBC()) {
      d_boundaryCondition->scalarBC(pc, patch,
                                    &scalarVars, &constScalarVars);
      /*if (d_boundaryCondition->getIntrusionBC())
        d_boundaryCondition->intrusionScalarBC(pc, patch, cellinfo,
                                               &scalarVars, &constScalarVars);*/
    }
    // apply multimaterial intrusion wallbc
    if (d_MAlab)
      d_boundaryCondition->mmscalarWallBC(pc, patch, cellinfo,
                                          &scalarVars, &constScalarVars);
    
    // similar to mascal
    // inputs :
    // outputs:
    d_source->modifyScalarMassSource(pc, patch, delta_t,
                                     &scalarVars, &constScalarVars,
                                     d_conv_scheme);
    
    // Calculate the scalar diagonal terms
    // inputs : scalCoefSBLM, scalLinSrcSBLM
    // outputs: scalCoefSBLM
    d_discretize->calculateScalarDiagonal(patch, &scalarVars);

    CCVariable<double> scalar;
    if (doing_EKT_now) {
      if (timelabels->integrator_step_number == TimeIntegratorStepNumber::First){
        new_dw->allocateAndPut(scalar, d_lab->d_scalarEKTLabel, indx, patch);
      }else{
        new_dw->getModifiable(scalar, d_lab->d_scalarEKTLabel,  indx, patch);
        new_dw->copyOut(scalar, d_lab->d_scalarSPLabel,indx, patch);
                  
      }
    }
  }
}


//****************************************************************************
// Schedule linear solve of scalar
//****************************************************************************
void
ScalarSolver::sched_scalarLinearSolve(SchedulerP& sched,
                                      const PatchSet* patches,
                                      const MaterialSet* matls,
                                      const TimeIntegratorLabel* timelabels,
                                      bool d_EKTCorrection,
                                      bool doing_EKT_now)
{
  string taskname =  "ScalarSolver::ScalarLinearSolve" + 
                     timelabels->integrator_step_name;
  if (doing_EKT_now) taskname += "EKTnow";
  Task* tsk = scinew Task(taskname, this,
                          &ScalarSolver::scalarLinearSolve,
                          timelabels, d_EKTCorrection, doing_EKT_now);
  
  Task::WhichDW parent_old_dw;
  if (timelabels->recursion){
    parent_old_dw = Task::ParentOldDW;
  }else{ 
    parent_old_dw = Task::OldDW;
  }
  
  Ghost::GhostType  gac = Ghost::AroundCells;
  Ghost::GhostType  gn  = Ghost::None;
  Task::DomainSpec oams = Task::OutOfDomain;  //outside of arches matlSet.
  
  tsk->requires(parent_old_dw, d_lab->d_sharedState->get_delt_label());
  tsk->requires(Task::NewDW, d_lab->d_cellTypeLabel,     gac, 1);
  tsk->requires(Task::NewDW, d_lab->d_densityGuessLabel, gn,  0);
  
  if (timelabels->multiple_steps){
    tsk->requires(Task::NewDW, d_lab->d_scalarTempLabel, gac, 1);
  }else{
    tsk->requires(Task::OldDW, d_lab->d_scalarSPLabel,   gac, 1);
  }
  tsk->requires(Task::NewDW, d_lab->d_scalCoefSBLMLabel,  
                             d_lab->d_stencilMatl, oams, gn, 0);
                             
  tsk->requires(Task::NewDW, d_lab->d_scalNonLinSrcSBLMLabel, gn, 0);

  if (d_MAlab) {
    tsk->requires(Task::NewDW, d_lab->d_mmgasVolFracLabel, gn, 0);
  }    

  if (doing_EKT_now){
    tsk->modifies(d_lab->d_scalarEKTLabel);
  }else{ 
    tsk->modifies(d_lab->d_scalarSPLabel);
  }
  if (timelabels->recursion){
    tsk->computes(d_lab->d_ScalarClippedLabel);
  }
  
  sched->addTask(tsk, patches, matls);
}
//****************************************************************************
// Actual scalar solve .. may be changed after recursive tasks are added
//****************************************************************************
void 
ScalarSolver::scalarLinearSolve(const ProcessorGroup* pc,
                                const PatchSubset* patches,
                                const MaterialSubset*,
                                DataWarehouse* old_dw,
                                DataWarehouse* new_dw,
                                const TimeIntegratorLabel* timelabels,
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
    ArchesVariables scalarVars;
    ArchesConstVariables constScalarVars;

    // Get the PerPatch CellInformation data
    PerPatch<CellInformationP> cellInfoP;
    if (new_dw->exists(d_lab->d_cellInfoLabel, indx, patch)){ 
      new_dw->get(cellInfoP, d_lab->d_cellInfoLabel, indx, patch);
    }else{ 
      throw VariableNotFoundInGrid("cellInformation"," ", __FILE__, __LINE__);
    }
    
    //__________________________________
    Ghost::GhostType  gac = Ghost::AroundCells;
    Ghost::GhostType  gn = Ghost::None;
    CellInformation* cellinfo = cellInfoP.get().get_rep();

    new_dw->get(constScalarVars.density_guess, d_lab->d_densityGuessLabel, indx, patch, gn, 0);

    if (timelabels->multiple_steps){
      new_dw->get(constScalarVars.old_scalar, d_lab->d_scalarTempLabel, indx, patch, gac, 1);
    }else{
      old_dw->get(constScalarVars.old_scalar, d_lab->d_scalarSPLabel,   indx, patch, gac, 1);
    }
    
    // for explicit calculation
    if (doing_EKT_now){
      new_dw->getModifiable(scalarVars.scalar, d_lab->d_scalarEKTLabel, indx, patch);
    }else{
      new_dw->getModifiable(scalarVars.scalar, d_lab->d_scalarSPLabel,  indx, patch);
    }
    
    for (int ii = 0; ii < d_lab->d_stencilMatl->size(); ii++){
      new_dw->get(constScalarVars.scalarCoeff[ii], 
                                                 d_lab->d_scalCoefSBLMLabel, ii, patch, gn, 0);
    }
    new_dw->get(constScalarVars.scalarNonlinearSrc,
                                                d_lab->d_scalNonLinSrcSBLMLabel, indx, patch, gn, 0);

    new_dw->get(constScalarVars.cellType,       d_lab->d_cellTypeLabel,     indx, patch, gac, 1);
    if (d_MAlab) {
      new_dw->get(constScalarVars.voidFraction, d_lab->d_mmgasVolFracLabel, indx, patch, gn, 0);
    }

    // make it a separate task later

    if (d_MAlab){
      d_boundaryCondition->scalarLisolve_mm(pc, patch, delta_t, 
                                            &scalarVars, &constScalarVars,
                                            cellinfo);
    }else{
      d_rhsSolver->scalarLisolve(pc, patch, delta_t, 
                                 &scalarVars, &constScalarVars,
                                 cellinfo);
    }
    
    
    double scalar_clipped = 0.0;
    double epsilon = 1.0e-15;
    // Get the patch bounds and the variable bounds
    IntVector idxLo = patch->getFortranCellLowIndex__New();
    IntVector idxHi = patch->getFortranCellHighIndex__New();
    for (int ii = idxLo.x(); ii <= idxHi.x(); ii++) {
      for (int jj = idxLo.y(); jj <= idxHi.y(); jj++) {
        for (int kk = idxLo.z(); kk <= idxHi.z(); kk++) {
          IntVector currCell(ii,jj,kk);

          if (scalarVars.scalar[currCell] > 1.0) {
            if (scalarVars.scalar[currCell] > 1.0 + epsilon) {
              scalar_clipped = 1.0;
              cout << "scalar got clipped to 1 at " << currCell
              << " , scalar value was " << scalarVars.scalar[currCell] 
              << " , density guess was " 
              << constScalarVars.density_guess[currCell] << endl;
            }
            scalarVars.scalar[currCell] = 1.0;
          }  
          else if (scalarVars.scalar[currCell] < 0.0) {
            if (scalarVars.scalar[currCell] < - epsilon) {
              scalar_clipped = 1.0;
              cout << "scalar got clipped to 0 at " << currCell
              << " , scalar value was " << scalarVars.scalar[currCell]
              << " , density guess was " 
              << constScalarVars.density_guess[currCell] << endl;
              cout << "Try setting <scalarUnderflowCheck>true</scalarUnderflowCheck> "
              << "in the <ARCHES> section of the input file, "
              <<"but it would only help for first time substep if RKSSP is used" << endl;
            }
            scalarVars.scalar[currCell] = 0.0;
          }
        }
      }
    }
    
    if (timelabels->recursion){
      new_dw->put(max_vartype(scalar_clipped), d_lab->d_ScalarClippedLabel);
    }
    
    // Outlet bc is done here not to change old scalar
    if ((d_boundaryCondition->getOutletBC())||
        (d_boundaryCondition->getPressureBC())){
      d_boundaryCondition->scalarOutletPressureBC(pc, patch, &scalarVars, &constScalarVars);
    }
  }  // patches
}

