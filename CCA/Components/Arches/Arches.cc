//----- Arches.cc ----------------------------------------------

#include <Packages/Uintah/CCA/Components/Arches/Arches.h>
#include <Packages/Uintah/CCA/Components/Arches/ArchesFort.h>
#include <Packages/Uintah/CCA/Components/Arches/PicardNonlinearSolver.h>
#include <Packages/Uintah/CCA/Components/Arches/ExplicitSolver.h>
#include <Packages/Uintah/CCA/Components/Arches/PhysicalConstants.h>
#include <Packages/Uintah/CCA/Components/Arches/SmagorinskyModel.h>
#include <Packages/Uintah/CCA/Components/Arches/BoundaryCondition.h>
#include <Packages/Uintah/CCA/Components/Arches/Properties.h>
#include <Packages/Uintah/CCA/Components/Arches/ArchesMaterial.h>
#include <Packages/Uintah/Core/Grid/Array3Index.h>
#include <Packages/Uintah/Core/Grid/Grid.h>
#include <Packages/Uintah/Core/Grid/Level.h>
#include <Packages/Uintah/Core/Grid/CCVariable.h>
#include <Packages/Uintah/Core/Grid/SFCXVariable.h>
#include <Packages/Uintah/Core/Grid/SFCYVariable.h>
#include <Packages/Uintah/Core/Grid/SFCZVariable.h>
#include <Packages/Uintah/Core/Grid/Patch.h>
#include <Packages/Uintah/Core/Grid/ReductionVariable.h>
#include <Packages/Uintah/Core/Grid/SimulationState.h>
#include <Packages/Uintah/Core/Grid/SoleVariable.h>
#include <Packages/Uintah/Core/Grid/Task.h>
#include <Packages/Uintah/Core/Grid/VarLabel.h>
#include <Packages/Uintah/Core/Grid/VarTypes.h>
#include <Packages/Uintah/Core/ProblemSpec/ProblemSpec.h>
#include <Packages/Uintah/CCA/Ports/DataWarehouse.h>
#include <Packages/Uintah/CCA/Ports/Scheduler.h>
#include <Packages/Uintah/Core/Exceptions/ParameterNotFound.h>
#include <Packages/Uintah/Core/Exceptions/InvalidValue.h>

#include <Core/Containers/StaticArray.h>
#include <Core/Geometry/IntVector.h>
#include <Core/Geometry/Vector.h>
#include <Core/Geometry/Point.h>
#include <Core/Math/MinMax.h>
#include <Core/Math/MiscMath.h>
#include <Core/Util/NotFinished.h>

#include <iostream>
using std::cerr;
using std::endl;

using namespace Uintah;
using namespace SCIRun;

// ****************************************************************************
// Actual constructor for Arches
// ****************************************************************************
Arches::Arches(const ProcessorGroup* myworld) :
  UintahParallelComponent(myworld)
{
  d_lab = scinew ArchesLabel();
  d_MAlab = 0; // will be set by setMPMArchesLabel
}

// ****************************************************************************
// Destructor
// ****************************************************************************
Arches::~Arches()
{

}

// ****************************************************************************
// problem set up
// ****************************************************************************
void 
Arches::problemSetup(const ProblemSpecP& params, 
		     GridP&,
		     SimulationStateP& sharedState)
{

  d_sharedState= sharedState;
  d_lab->setSharedState(sharedState);
  ArchesMaterial* mat= scinew ArchesMaterial();
  sharedState->registerArchesMaterial(mat);
  ProblemSpecP db = params->findBlock("CFD")->findBlock("ARCHES");
  // not sure, do we need to reduce and put in datawarehouse
  db->require("grow_dt", d_deltaT);
  db->require("reacting_flow", d_reactingFlow);
  db->require("solve_enthalpy", d_calcEnthalpy);

  // physical constants
  d_physicalConsts = scinew PhysicalConstants();

  // ** BB 5/19/2000 ** For now read the Physical constants from the 
  // CFD-ARCHES block
  // for gravity, read it from shared state 
  //d_physicalConsts->problemSetup(params);
  d_physicalConsts->problemSetup(db);
  // read properties
  // d_MAlab = multimaterial arches common labels
  d_props = scinew Properties(d_lab, d_MAlab, d_reactingFlow, d_calcEnthalpy);
  d_props->problemSetup(db);
  d_nofScalars = d_props->getNumMixVars();

  // read turbulence model
  string turbModel;
  db->require("turbulence_model", turbModel);
  if (turbModel == "smagorinsky") 
    d_turbModel = scinew SmagorinskyModel(d_lab, d_MAlab, d_physicalConsts);
  else 
    throw InvalidValue("Turbulence Model not supported" + turbModel);
  d_turbModel->problemSetup(db);

  // read boundary
  d_boundaryCondition = scinew BoundaryCondition(d_lab, d_MAlab, d_turbModel,
						 d_props, d_calcEnthalpy);
  // send params, boundary type defined at the level of Grid
  d_boundaryCondition->problemSetup(db);
  d_props->setBC(d_boundaryCondition);

  string nlSolver;
  db->require("nonlinear_solver", nlSolver);
  if(nlSolver == "picard") {
    d_nlSolver = scinew PicardNonlinearSolver(d_lab, d_MAlab, d_props, 
					      d_boundaryCondition,
					      d_turbModel, d_physicalConsts,
					      d_calcEnthalpy,
					      d_myworld);
  }
  else if (nlSolver == "explicit") {
        d_nlSolver = scinew ExplicitSolver(d_lab, d_MAlab, d_props,
					   d_boundaryCondition,
					   d_turbModel, d_physicalConsts,
					   d_calcEnthalpy,
					   d_myworld);
  }
  else
    throw InvalidValue("Nonlinear solver not supported: "+nlSolver);

  d_nlSolver->problemSetup(db);
}

// ****************************************************************************
// Schedule initialization
// ****************************************************************************
void 
Arches::scheduleInitialize(const LevelP& level,
			   SchedulerP& sched)
{
  
  const PatchSet* patches= level->eachPatch();
  const MaterialSet* matls = d_sharedState->allArchesMaterials();

  // schedule the initialization of parameters
  // require : None
  // compute : [u,v,w]VelocityIN, pressureIN, scalarIN, densityIN,
  //           viscosityIN
  sched_paramInit(level, sched);
  // schedule init of cell type
  // require : NONE
  // compute : cellType
  d_boundaryCondition->sched_cellTypeInit(sched, patches, matls);

  // computing flow inlet areas
  d_boundaryCondition->sched_calculateArea(sched, patches, matls);

  // Set the profile (output Varlabel have SP appended to them)
  // require : densityIN,[u,v,w]VelocityIN
  // compute : densitySP, [u,v,w]VelocitySP, scalarSP
  d_boundaryCondition->sched_setProfile(sched, patches, matls);

  // Compute props (output Varlabel have CP appended to them)
  // require : densitySP
  // require scalarSP
  // compute : densityCP
  d_props->sched_computeProps(sched, patches, matls);

  // Compute Turb subscale model (output Varlabel have CTS appended to them)
  // require : densityCP, viscosityIN, [u,v,w]VelocitySP
  // compute : viscosityCTS
  d_turbModel->sched_computeTurbSubmodel(sched, patches, matls);

  // Computes velocities at apecified pressure b.c's
  // require : densityCP, pressureIN, [u,v,w]VelocitySP
  // compute : pressureSPBC, [u,v,w]VelocitySPBC
  if (d_boundaryCondition->getPressureBC()) 
    d_boundaryCondition->sched_computePressureBC(sched, patches, matls);
}

// ****************************************************************************
// schedule the initialization of parameters
// ****************************************************************************
void 
Arches::sched_paramInit(const LevelP& level,
			SchedulerP& sched)
{
    // primitive variable initialization
    Task* tsk = scinew Task( "Arches::paramInit",
			    this, &Arches::paramInit);
    tsk->computes(d_lab->d_uVelocityINLabel);
    tsk->computes(d_lab->d_vVelocityINLabel);
    tsk->computes(d_lab->d_wVelocityINLabel);
    tsk->computes(d_lab->d_newCCUVelocityLabel);
    tsk->computes(d_lab->d_newCCVVelocityLabel);
    tsk->computes(d_lab->d_newCCWVelocityLabel);
    tsk->computes(d_lab->d_pressureINLabel);
    for (int ii = 0; ii < d_nofScalars; ii++) 
      tsk->computes(d_lab->d_scalarINLabel); // only work for 1 scalar
    if (d_calcEnthalpy)
      tsk->computes(d_lab->d_enthalpyINLabel); 
    tsk->computes(d_lab->d_densityINLabel);
    tsk->computes(d_lab->d_viscosityINLabel);
    // for reacting flows save temperature and co2 
    if (d_MAlab)
      tsk->computes(d_lab->d_pressPlusHydroLabel);
    sched->addTask(tsk, level->eachPatch(), d_sharedState->allArchesMaterials());

}

// ****************************************************************************
// schedule computation of stable time step
// ****************************************************************************
void 
Arches::scheduleComputeStableTimestep(const LevelP& level,
				      SchedulerP& sched)
{
  // primitive variable initialization
  int zeroGhostCells = 0;
  Task* tsk = scinew Task( "Arches::computeStableTimeStep",
			   this, &Arches::computeStableTimeStep);
#if 1
  tsk->requires(Task::NewDW, d_lab->d_uVelocitySPBCLabel,
		Ghost::None, zeroGhostCells);
  tsk->requires(Task::NewDW, d_lab->d_vVelocitySPBCLabel,
		Ghost::None, zeroGhostCells);
  tsk->requires(Task::NewDW, d_lab->d_wVelocitySPBCLabel,
		Ghost::None, zeroGhostCells);
#endif
  tsk->computes(d_sharedState->get_delt_label());
  sched->addTask(tsk, level->eachPatch(), d_sharedState->allArchesMaterials());


}

void 
Arches::computeStableTimeStep(const ProcessorGroup* ,
			      const PatchSubset* patches,
			      const MaterialSubset*,
			      DataWarehouse* ,
			      DataWarehouse* new_dw)
{
  for (int p = 0; p < patches->size(); p++) {
    const Patch* patch = patches->get(p);
    int archIndex = 0; // only one arches material
    int matlIndex = d_lab->d_sharedState->getArchesMaterial(archIndex)->getDWIndex(); 

    int zeroGhostCells = 0;
#if 1
    SFCXVariable<double> uVelocity;
    SFCYVariable<double> vVelocity;
    SFCZVariable<double> wVelocity;
    PerPatch<CellInformationP> cellInfoP;

    if (new_dw->exists(d_lab->d_cellInfoLabel, matlIndex, patch)) 
      
      new_dw->get(cellInfoP, d_lab->d_cellInfoLabel, matlIndex, patch);

    else {

      cellInfoP.setData(scinew CellInformation(patch));
      new_dw->put(cellInfoP, d_lab->d_cellInfoLabel, matlIndex, patch);

    }

    CellInformation* cellinfo = cellInfoP.get().get_rep();

    new_dw->get(uVelocity, d_lab->d_uVelocitySPBCLabel, 
		matlIndex, patch, Ghost::None, zeroGhostCells);
    new_dw->get(vVelocity, d_lab->d_vVelocitySPBCLabel, 
		matlIndex, patch, Ghost::None, zeroGhostCells);
    new_dw->get(wVelocity, d_lab->d_wVelocitySPBCLabel, 
		matlIndex, patch, Ghost::None, zeroGhostCells);
    IntVector indexLow = patch->getCellFORTLowIndex();
    IntVector indexHigh = patch->getCellFORTHighIndex() + IntVector(1,1,1);
    int numGC = 1;
    IntVector geomLow = patch->getGhostCellLowIndex(numGC);
    //    voidFraction.print(cerr);
  // set density for the whole domain
    double delta_t = d_deltaT; // max value allowed
    double temp_t = 0;
    double small_num = 1e-30;
    for (int colZ = indexLow.z(); colZ < indexHigh.z(); colZ ++) {
      for (int colY = indexLow.y(); colY < indexHigh.y(); colY ++) {
	for (int colX = indexLow.x(); colX < indexHigh.x(); colX ++) {
	  IntVector currCell(colX, colY, colZ);
	  double tmp_time=Abs(uVelocity[currCell])/(cellinfo->sew[colX-geomLow.x()])+
	                  Abs(vVelocity[currCell])/(cellinfo->sns[colY-geomLow.y()])+
			  Abs(wVelocity[currCell])/(cellinfo->stb[colZ-geomLow.z()])+
                          small_num;
	  delta_t=Min(1.0/tmp_time, delta_t);
#if 0								  
	  delta_t=Min(Abs(cellinfo->sew[colX-geomLow.x()]/
			  (uVelocity[currCell]+small_num)),delta_t);
	  delta_t=Min(Abs(cellinfo->sns[colY-geomLow.y()]/
			  (vVelocity[currCell]+small_num)), delta_t);
	  delta_t=Min(Abs(cellinfo->stb[colZ-geomLow.z()]/
			  (wVelocity[currCell]+small_num)), delta_t);
#endif
	}
      }
    }
    cerr << " min time step: " << delta_t << endl;
    new_dw->put(delt_vartype(delta_t),  d_sharedState->get_delt_label()); 
#else
    new_dw->put(delt_vartype(d_deltaT),  d_sharedState->get_delt_label()); 
#endif
  }
}

// ****************************************************************************
// Schedule time advance
// ****************************************************************************
void 
Arches::scheduleTimeAdvance(double time, double dt,
			    const LevelP& level, 
			    SchedulerP& sched)
{
  d_nlSolver->nonlinearSolve(level, sched, time, dt);
}


// ****************************************************************************
// Actual initialization
// ****************************************************************************
void
Arches::paramInit(const ProcessorGroup* ,
		  const PatchSubset* patches,
		  const MaterialSubset*,
		  DataWarehouse* ,
		  DataWarehouse* new_dw)
{
  // ....but will only compute for computational domain
  for (int p = 0; p < patches->size(); p++) {
    const Patch* patch = patches->get(p);
    int archIndex = 0; // only one arches material
    int matlIndex = d_sharedState->getArchesMaterial(archIndex)->getDWIndex(); 
    SFCXVariable<double> uVelocity;
    SFCYVariable<double> vVelocity;
    SFCZVariable<double> wVelocity;
    CCVariable<double> uVelocityCC;
    CCVariable<double> vVelocityCC;
    CCVariable<double> wVelocityCC;
    CCVariable<double> pressure;
    StaticArray< CCVariable<double> > scalar(d_nofScalars);
    CCVariable<double> enthalpy;
    CCVariable<double> density;
    CCVariable<double> viscosity;
    CCVariable<double> pPlusHydro;
    std::cerr << "Material Index: " << matlIndex << endl;
    new_dw->allocate(uVelocityCC, d_lab->d_newCCUVelocityLabel, matlIndex, patch);
    new_dw->allocate(vVelocityCC, d_lab->d_newCCVVelocityLabel, matlIndex, patch);
    new_dw->allocate(wVelocityCC, d_lab->d_newCCWVelocityLabel, matlIndex, patch);
    uVelocityCC.initialize(0.0);
    vVelocityCC.initialize(0.0);
    wVelocityCC.initialize(0.0);
    new_dw->allocate(uVelocity, d_lab->d_uVelocityINLabel, matlIndex, patch);
    new_dw->allocate(vVelocity, d_lab->d_vVelocityINLabel, matlIndex, patch);
    new_dw->allocate(wVelocity, d_lab->d_wVelocityINLabel, matlIndex, patch);
    new_dw->allocate(pressure, d_lab->d_pressureINLabel, matlIndex, patch);
    if (d_MAlab) {
      new_dw->allocate(pPlusHydro, d_lab->d_pressPlusHydroLabel, matlIndex, patch);
      pPlusHydro.initialize(0.0);
    }
    // will only work for one scalar
    for (int ii = 0; ii < d_nofScalars; ii++) {
      new_dw->allocate(scalar[ii], d_lab->d_scalarINLabel, matlIndex, patch);
    }
    if (d_calcEnthalpy) {
      new_dw->allocate(enthalpy, d_lab->d_enthalpyINLabel, matlIndex, patch);
      enthalpy.initialize(0.0);
    }
    new_dw->allocate(density, d_lab->d_densityINLabel, matlIndex, patch);
    new_dw->allocate(viscosity, d_lab->d_viscosityINLabel, matlIndex, patch);

  // ** WARNING **  this needs to be changed soon (6/9/2000)
    IntVector domLoU = uVelocity.getFortLowIndex();
    IntVector domHiU = uVelocity.getFortHighIndex();
    IntVector idxLoU = domLoU;
    IntVector idxHiU = domHiU;
    IntVector domLoV = vVelocity.getFortLowIndex();
    IntVector domHiV = vVelocity.getFortHighIndex();
    IntVector idxLoV = domLoV;
    IntVector idxHiV = domHiV;
    IntVector domLoW = wVelocity.getFortLowIndex();
    IntVector domHiW = wVelocity.getFortHighIndex();
    IntVector idxLoW = domLoW;
    IntVector idxHiW = domHiW;
    IntVector domLo = pressure.getFortLowIndex();
    IntVector domHi = pressure.getFortHighIndex();
    IntVector idxLo = domLo;
    IntVector idxHi = domHi;

  //can read these values from input file 
    double uVal = 0.0, vVal = 0.0, wVal = 0.0;
    double pVal = 0.0, denVal = 0.0;
    double visVal = d_physicalConsts->getMolecularViscosity();
    FORT_INIT(domLoU.get_pointer(), domHiU.get_pointer(), 
	      idxLoU.get_pointer(), idxHiU.get_pointer(),
	      uVelocity.getPointer(), &uVal, 
	      domLoV.get_pointer(), domHiV.get_pointer(), 
	      idxLoV.get_pointer(), idxHiV.get_pointer(),
	      vVelocity.getPointer(), &vVal, 
	      domLoW.get_pointer(), domHiW.get_pointer(), 
	      idxLoW.get_pointer(), idxHiW.get_pointer(),
	      wVelocity.getPointer(), &wVal,
	      domLo.get_pointer(), domHi.get_pointer(), 
	      idxLo.get_pointer(), idxHi.get_pointer(),
	      pressure.getPointer(), &pVal, 
	      density.getPointer(), &denVal, 
	      viscosity.getPointer(), &visVal);
    for (int ii = 0; ii < d_nofScalars; ii++) {
      double scalVal = 0.0;
      FORT_INIT_SCALAR(domLo.get_pointer(), domHi.get_pointer(),
		       idxLo.get_pointer(), idxHi.get_pointer(), 
		       scalar[ii].getPointer(), &scalVal);
    }
    new_dw->put(uVelocityCC, d_lab->d_newCCUVelocityLabel, matlIndex, patch);
    new_dw->put(vVelocityCC, d_lab->d_newCCVVelocityLabel, matlIndex, patch);
    new_dw->put(wVelocityCC, d_lab->d_newCCWVelocityLabel, matlIndex, patch);
    
    new_dw->put(uVelocity, d_lab->d_uVelocityINLabel, matlIndex, patch);
    new_dw->put(vVelocity, d_lab->d_vVelocityINLabel, matlIndex, patch);
    new_dw->put(wVelocity, d_lab->d_wVelocityINLabel, matlIndex, patch);
    new_dw->put(pressure, d_lab->d_pressureINLabel, matlIndex, patch);
    for (int ii = 0; ii < d_nofScalars; ii++) {
      new_dw->put(scalar[ii], d_lab->d_scalarINLabel, matlIndex, patch);
    }
    if (d_calcEnthalpy)
      new_dw->put(enthalpy, d_lab->d_enthalpyINLabel, matlIndex, patch);
    new_dw->put(density, d_lab->d_densityINLabel, matlIndex, patch);
    new_dw->put(viscosity, d_lab->d_viscosityINLabel, matlIndex, patch);
    if (d_MAlab)
      new_dw->put(pPlusHydro, d_lab->d_pressPlusHydroLabel, matlIndex, patch);

  }
}
