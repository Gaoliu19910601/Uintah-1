//----- BoundaryCondition.cc ----------------------------------------------

/* REFERENCED */
static char *id="@(#) $Id$";

#include <Uintah/Components/Arches/BoundaryCondition.h>
#include <Uintah/Components/Arches/CellInformation.h>
#include <Uintah/Components/Arches/StencilMatrix.h>
#include <Uintah/Components/Arches/ArchesFort.h>
#include <Uintah/Components/Arches/Discretization.h>
#include <Uintah/Grid/Stencil.h>
#include <Uintah/Components/Arches/TurbulenceModel.h>
#include <Uintah/Components/Arches/Properties.h>
#include <SCICore/Util/NotFinished.h>
#include <Uintah/Grid/Task.h>
#include <Uintah/Grid/SFCXVariable.h>
#include <Uintah/Grid/SFCYVariable.h>
#include <Uintah/Grid/SFCZVariable.h>
#include <Uintah/Grid/CCVariable.h>
#include <Uintah/Grid/PerPatch.h>
#include <Uintah/Grid/CellIterator.h>
#include <Uintah/Grid/SoleVariable.h>
#include <Uintah/Grid/ReductionVariable.h>
#include <Uintah/Grid/Reductions.h>
#include <Uintah/Interface/ProblemSpec.h>
#include <Uintah/Grid/GeometryPieceFactory.h>
#include <Uintah/Grid/UnionGeometryPiece.h>
#include <Uintah/Interface/DataWarehouse.h>
#include <SCICore/Geometry/Vector.h>
#include <SCICore/Geometry/IntVector.h>
#include <Uintah/Exceptions/InvalidValue.h>
#include <Uintah/Exceptions/ParameterNotFound.h>
#include <Uintah/Interface/Scheduler.h>
#include <Uintah/Grid/Level.h>
#include <Uintah/Grid/VarLabel.h>
#include <Uintah/Grid/VarTypes.h>
#include <Uintah/Grid/TypeUtils.h>
#include <iostream>
using namespace std;
using namespace Uintah::MPM;
using namespace Uintah::ArchesSpace;
using SCICore::Geometry::Vector;

//****************************************************************************
// Constructor for BoundaryCondition
//****************************************************************************
BoundaryCondition::BoundaryCondition(const ArchesLabel* label,
				     TurbulenceModel* turb_model,
				     Properties* props):
                                     d_lab(label), 
				     d_turbModel(turb_model), 
				     d_props(props)
{
  d_nofScalars = d_props->getNumMixVars();
}

//****************************************************************************
// Destructor
//****************************************************************************
BoundaryCondition::~BoundaryCondition()
{
}

//****************************************************************************
// Problem Setup
//****************************************************************************
void 
BoundaryCondition::problemSetup(const ProblemSpecP& params)
{

  ProblemSpecP db = params->findBlock("BoundaryConditions");
  d_numInlets = 0;
  int total_cellTypes = 0;
  int numMixingScalars = d_props->getNumMixVars();
  for (ProblemSpecP inlet_db = db->findBlock("FlowInlet");
       inlet_db != 0; inlet_db = inlet_db->findNextBlock("FlowInlet")) {
    d_flowInlets.push_back(FlowInlet(numMixingScalars, total_cellTypes));
    d_flowInlets[d_numInlets].problemSetup(inlet_db);
    d_cellTypes.push_back(total_cellTypes);
    d_flowInlets[d_numInlets].density = 
      d_props->computeInletProperties(d_flowInlets[d_numInlets].streamMixturefraction);
    ++total_cellTypes;
    ++d_numInlets;
  }
  if (ProblemSpecP wall_db = db->findBlock("WallBC")) {
    d_wallBdry = scinew WallBdry(total_cellTypes);
    d_wallBdry->problemSetup(wall_db);
    d_cellTypes.push_back(total_cellTypes);
    ++total_cellTypes;
  }
  else {
    cerr << "Wall boundary not specified" << endl;
  }
  
  if (ProblemSpecP press_db = db->findBlock("PressureBC")) {
    d_pressBoundary = true;
    d_pressureBdry = scinew PressureInlet(numMixingScalars, total_cellTypes);
    d_pressureBdry->problemSetup(press_db);
    d_pressureBdry->density = 
      d_props->computeInletProperties(d_pressureBdry->streamMixturefraction);
    d_cellTypes.push_back(total_cellTypes);
    ++total_cellTypes;
  }
  else {
    d_pressBoundary = false;
  }
  
  if (ProblemSpecP outlet_db = db->findBlock("outletBC")) {
    d_outletBoundary = true;
    d_outletBC = scinew FlowOutlet(numMixingScalars, total_cellTypes);
    d_outletBC->problemSetup(outlet_db);
    d_outletBC->density = 
      d_props->computeInletProperties(d_outletBC->streamMixturefraction);
    d_cellTypes.push_back(total_cellTypes);
    ++total_cellTypes;
  }
  else {
    d_outletBoundary = false;
  }

}

//****************************************************************************
// schedule the initialization of cell types
//****************************************************************************
void 
BoundaryCondition::sched_cellTypeInit(const LevelP& level,
				      SchedulerP& sched,
				      DataWarehouseP& old_dw,
				      DataWarehouseP& new_dw)
{
  for(Level::const_patchIterator iter=level->patchesBegin();
      iter != level->patchesEnd(); iter++){
    const Patch* patch=*iter;

    // cell type initialization
    Task* tsk = new Task("BoundaryCondition::cellTypeInit",
			 patch, old_dw, new_dw, this,
			 &BoundaryCondition::cellTypeInit);
    int matlIndex = 0;
    tsk->computes(new_dw, d_lab->d_cellTypeLabel, matlIndex, patch);
    sched->addTask(tsk);
  }
}

//****************************************************************************
// Actual initialization of celltype
//****************************************************************************
void 
BoundaryCondition::cellTypeInit(const ProcessorGroup*,
				const Patch* patch,
				DataWarehouseP& old_dw,
				DataWarehouseP&)
{
  CCVariable<int> cellType;
  int matlIndex = 0;

  old_dw->allocate(cellType, d_lab->d_cellTypeLabel, matlIndex, patch);

  IntVector domLo = cellType.getFortLowIndex();
  IntVector domHi = cellType.getFortHighIndex();
  IntVector idxLo = domLo;
  IntVector idxHi = domHi;
 
  cerr << "Just before geom init" << endl;
  // initialize CCVariable to -1 which corresponds to flowfield
  int celltypeval;
  d_flowfieldCellTypeVal = -1;
  FORT_CELLTYPEINIT(domLo.get_pointer(), domHi.get_pointer(), 
		    idxLo.get_pointer(), idxHi.get_pointer(),
		    cellType.getPointer(), &d_flowfieldCellTypeVal);

  // Find the geometry of the patch
  Box patchBox = patch->getBox();
  cout << "Patch box = " << patchBox << endl;

  // wall boundary type
  {
    int nofGeomPieces = d_wallBdry->d_geomPiece.size();
    for (int ii = 0; ii < nofGeomPieces; ii++) {
      GeometryPiece*  piece = d_wallBdry->d_geomPiece[ii];
      Box geomBox = piece->getBoundingBox();
      cout << "Wall Geometry box = " << geomBox << endl;
      Box b = geomBox.intersect(patchBox);
      cout << "Wall Intersection box = " << b << endl;
      cerr << "Just before geom wall "<< endl;
      // check for another geometry
      if (!(b.degenerate())) {
	CellIterator iter = patch->getCellIterator(b);
	IntVector idxLo = iter.begin();
	IntVector idxHi = iter.end() - IntVector(1,1,1);
	celltypeval = d_wallBdry->d_cellTypeID;
	cout << "Wall cell type val = " << celltypeval << endl;
	FORT_CELLTYPEINIT(domLo.get_pointer(), domHi.get_pointer(), 
			  idxLo.get_pointer(), idxHi.get_pointer(),
			  cellType.getPointer(), &celltypeval);
      }
    }
  }
  // initialization for pressure boundary
  {
    if (d_pressBoundary) {
      int nofGeomPieces = d_pressureBdry->d_geomPiece.size();
      for (int ii = 0; ii < nofGeomPieces; ii++) {
	GeometryPiece*  piece = d_pressureBdry->d_geomPiece[ii];
	Box geomBox = piece->getBoundingBox();
	cout << "Pressure Geometry box = " << geomBox << endl;
	Box b = geomBox.intersect(patchBox);
	cout << "Pressure Intersection box = " << b << endl;
	// check for another geometry
	if (!(b.degenerate())) {
	  CellIterator iter = patch->getCellIterator(b);
	  IntVector idxLo = iter.begin();
	  IntVector idxHi = iter.end() - IntVector(1,1,1);
	  celltypeval = d_pressureBdry->d_cellTypeID;
	  cout << "Pressure Bdry  cell type val = " << celltypeval << endl;
	  FORT_CELLTYPEINIT(domLo.get_pointer(), domHi.get_pointer(),
			    idxLo.get_pointer(), idxHi.get_pointer(),
			    cellType.getPointer(), &celltypeval);
	}
      }
    }
  }
  // initialization for outlet boundary
  {
    if (d_outletBoundary) {
      int nofGeomPieces = d_outletBC->d_geomPiece.size();
      for (int ii = 0; ii < nofGeomPieces; ii++) {
	GeometryPiece*  piece = d_outletBC->d_geomPiece[ii];
	Box geomBox = piece->getBoundingBox();
	cout << "Outlet Geometry box = " << geomBox << endl;
	Box b = geomBox.intersect(patchBox);
	cout << "Outlet Intersection box = " << b << endl;
	// check for another geometry
	if (!(b.degenerate())) {
	  CellIterator iter = patch->getCellIterator(b);
	  IntVector idxLo = iter.begin();
	  IntVector idxHi = iter.end() - IntVector(1,1,1);
	  celltypeval = d_outletBC->d_cellTypeID;
	  cout << "Flow Outlet cell type val = " << celltypeval << endl;
	  FORT_CELLTYPEINIT(domLo.get_pointer(), domHi.get_pointer(),
			    idxLo.get_pointer(), idxHi.get_pointer(),
			    cellType.getPointer(), &celltypeval);
	}
      }
    }
  }
  // set boundary type for inlet flow field
  for (int ii = 0; ii < d_numInlets; ii++) {
    int nofGeomPieces = d_flowInlets[ii].d_geomPiece.size();
    for (int jj = 0; jj < nofGeomPieces; jj++) {
      GeometryPiece*  piece = d_flowInlets[ii].d_geomPiece[jj];
      Box geomBox = piece->getBoundingBox();
      cout << "Inlet " << ii << " Geometry box = " << geomBox << endl;
      Box b = geomBox.intersect(patchBox);
      cout << "Inlet " << ii << " Intersection box = " << b << endl;
      // check for another geometry
      if (b.degenerate())
	continue; // continue the loop for other inlets
      // iterates thru box b, converts from geometry space to index space
      // make sure this works
      CellIterator iter = patch->getCellIterator(b);
      IntVector idxLo = iter.begin();
      IntVector idxHi = iter.end() - IntVector(1,1,1);
      celltypeval = d_flowInlets[ii].d_cellTypeID;
      cout << "Flow inlet " << ii << " cell type val = " << celltypeval << endl;
      FORT_CELLTYPEINIT(domLo.get_pointer(), domHi.get_pointer(),
			idxLo.get_pointer(), idxHi.get_pointer(),
			cellType.getPointer(), &celltypeval);
    }
  }
  // Testing if correct values have been put
  cout << " In C++ (BoundaryCondition.cc) after flow inlet init " << endl;
  for (int kk = idxLo.z(); kk <= idxHi.z(); kk++) {
    cout << "Celltypes for kk = " << kk << endl;
    for (int ii = idxLo.x(); ii <= idxHi.x(); ii++) {
      for (int jj = idxLo.y(); jj <= idxHi.y(); jj++) {
	cout.width(2);
	cout << cellType[IntVector(ii,jj,kk)] << " " ; 
      }
      cout << endl;
    }
  }

  old_dw->put(cellType, d_lab->d_cellTypeLabel, matlIndex, patch);
}  
    
//****************************************************************************
// Actual initialization of celltype
//****************************************************************************
void 
BoundaryCondition::computeInletFlowArea(const ProcessorGroup*,
					const Patch* patch,
					DataWarehouseP& old_dw,
					DataWarehouseP&)
{
  // Create the cellType variable
  CCVariable<int> cellType;

  // Get the cell type data from the old_dw
  // **WARNING** numGhostcells, Ghost::NONE may change in the future
  int matlIndex = 0;
  int numGhostCells = 0;
  old_dw->get(cellType, d_lab->d_cellTypeLabel, matlIndex, patch,
	      Ghost::None, numGhostCells);

  // Get the PerPatch CellInformation data
  PerPatch<CellInformation*> cellInfoP;
  if (old_dw->exists(d_lab->d_cellInfoLabel, patch)) 
    old_dw->get(cellInfoP, d_lab->d_cellInfoLabel, matlIndex, patch);
  else {
    cellInfoP.setData(scinew CellInformation(patch));
    old_dw->put(cellInfoP, d_lab->d_cellInfoLabel, matlIndex, patch);
  }
  CellInformation* cellInfo = cellInfoP;
  
  // Get the low and high index for the variable and the patch
  IntVector domLo = cellType.getFortLowIndex();
  IntVector domHi = cellType.getFortHighIndex();

  // Get the geometry of the patch
  Box patchBox = patch->getBox();

  // Go thru the number of inlets
  for (int ii = 0; ii < d_numInlets; ii++) {

    // Loop thru the number of geometry pieces in each inlet
    int nofGeomPieces = d_flowInlets[ii].d_geomPiece.size();
    for (int jj = 0; jj < nofGeomPieces; jj++) {

      // Intersect the geometry piece with the patch box
      GeometryPiece*  piece = d_flowInlets[ii].d_geomPiece[jj];
      Box geomBox = piece->getBoundingBox();
      Box b = geomBox.intersect(patchBox);
      // check for another geometry
      if (b.degenerate())
	continue; // continue the loop for other inlets

      // iterates thru box b, converts from geometry space to index space
      // make sure this works
      CellIterator iter = patch->getCellIterator(b);
      IntVector idxLo = iter.begin();
      IntVector idxHi = iter.end() - IntVector(1,1,1);

      // Calculate the inlet area
      double inlet_area;
      int cellid = d_flowInlets[ii].d_cellTypeID;
      cout << "Domain Lo = [" << domLo.x() << "," <<domLo.y()<< "," <<domLo.z()
	   << "] Domain hi = [" << domHi.x() << "," <<domHi.y()<< "," <<domHi.z() 
	   << "]" << endl;
      cout << "Index Lo = [" << idxLo.x() << "," <<idxLo.y()<< "," <<idxLo.z()
	   << "] Index hi = [" << idxHi.x() << "," <<idxHi.y()<< "," <<idxHi.z()
	   << "]" << endl;
      cout << "Cell ID = " << cellid << endl;
      FORT_AREAIN(domLo.get_pointer(), domHi.get_pointer(),
		  idxLo.get_pointer(), idxHi.get_pointer(),
		  cellInfo->sew.get_objs(),
		  cellInfo->sns.get_objs(), cellInfo->stb.get_objs(),
		  &inlet_area, cellType.getPointer(), &cellid);

      cout << "Inlet area = " << inlet_area << endl;
      // Write the inlet area to the old_dw
      old_dw->put(sum_vartype(inlet_area),d_flowInlets[ii].d_area_label);
    }
  }
}
    
//****************************************************************************
// Schedule the computation of the presures bcs
//****************************************************************************
void 
BoundaryCondition::sched_computePressureBC(const LevelP& level,
				    SchedulerP& sched,
				    DataWarehouseP& old_dw,
				    DataWarehouseP& new_dw)
{
  for(Level::const_patchIterator iter=level->patchesBegin();
      iter != level->patchesEnd(); iter++){
    const Patch* patch=*iter;
    {
      //copies old db to new_db and then uses non-linear
      //solver to compute new values
      Task* tsk = scinew Task("BoundaryCondition::calcPressureBC",patch,
			      old_dw, new_dw, this,
			      &BoundaryCondition::calcPressureBC);

      int numGhostCells = 0;
      int matlIndex = 0;
      
      // This task requires the pressure
      tsk->requires(old_dw, d_lab->d_cellTypeLabel, matlIndex, patch, Ghost::None,
		    numGhostCells);
      tsk->requires(old_dw, d_lab->d_pressureINLabel, matlIndex, patch, Ghost::None,
		    numGhostCells);
      tsk->requires(old_dw, d_lab->d_densityCPLabel, matlIndex, patch, Ghost::None,
		    numGhostCells);
      tsk->requires(old_dw, d_lab->d_uVelocitySPLabel, matlIndex, patch, Ghost::None,
		    numGhostCells);
      tsk->requires(old_dw, d_lab->d_vVelocitySPLabel, matlIndex, patch, Ghost::None,
		    numGhostCells);
      tsk->requires(old_dw, d_lab->d_wVelocitySPLabel, matlIndex, patch, Ghost::None,
		    numGhostCells);
      // This task computes new uVelocity, vVelocity and wVelocity
      tsk->computes(new_dw, d_lab->d_pressureSPBCLabel, matlIndex, patch);
      tsk->computes(new_dw, d_lab->d_uVelocitySPBCLabel, matlIndex, patch);
      tsk->computes(new_dw, d_lab->d_vVelocitySPBCLabel, matlIndex, patch);
      tsk->computes(new_dw, d_lab->d_wVelocitySPBCLabel, matlIndex, patch);

      sched->addTask(tsk);
    }
  }
}

//****************************************************************************
// Actually calculate the pressure BCs
//****************************************************************************
void 
BoundaryCondition::calcPressureBC(const ProcessorGroup* ,
				  const Patch* patch,
				  DataWarehouseP& old_dw,
				  DataWarehouseP& new_dw) 
{
  CCVariable<int> cellType;
  CCVariable<double> density;
  CCVariable<double> pressure;
  SFCXVariable<double> uVelocity;
  SFCYVariable<double> vVelocity;
  SFCZVariable<double> wVelocity;
  int matlIndex = 0;
  int nofGhostCells = 0;

  // get cellType, pressure and velocity
  old_dw->get(cellType, d_lab->d_cellTypeLabel, matlIndex, patch, Ghost::None,
	      nofGhostCells);
  old_dw->get(density, d_lab->d_densityCPLabel, matlIndex, patch, Ghost::None,
	      nofGhostCells);
  old_dw->get(pressure, d_lab->d_pressureINLabel, matlIndex, patch, Ghost::None,
	      nofGhostCells);
  old_dw->get(uVelocity, d_lab->d_uVelocitySPLabel, matlIndex, patch, Ghost::None,
	      nofGhostCells);
  old_dw->get(vVelocity, d_lab->d_vVelocitySPLabel, matlIndex, patch, Ghost::None,
	      nofGhostCells);
  old_dw->get(wVelocity, d_lab->d_wVelocitySPLabel, matlIndex, patch, Ghost::None,
	      nofGhostCells);

  // Get the low and high index for the patch and the variables
  IntVector domLoScalar = density.getFortLowIndex();
  IntVector domHiScalar = density.getFortHighIndex();
  IntVector idxLo = patch->getCellFORTLowIndex();
  IntVector idxHi = patch->getCellFORTHighIndex();
  IntVector domLoU = uVelocity.getFortLowIndex();
  IntVector domHiU = uVelocity.getFortHighIndex();
  IntVector domLoV = vVelocity.getFortLowIndex();
  IntVector domHiV = vVelocity.getFortHighIndex();
  IntVector domLoW = wVelocity.getFortLowIndex();
  IntVector domHiW = wVelocity.getFortHighIndex();

  FORT_CALPBC(domLoU.get_pointer(), domHiU.get_pointer(), 
	      uVelocity.getPointer(),
	      domLoV.get_pointer(), domHiV.get_pointer(), 
	      vVelocity.getPointer(),
	      domLoW.get_pointer(), domHiW.get_pointer(), 
	      wVelocity.getPointer(),
	      domLoScalar.get_pointer(), domHiScalar.get_pointer(), 
	      idxLo.get_pointer(), idxHi.get_pointer(), 
	      pressure.getPointer(),
	      density.getPointer(), 
	      cellType.getPointer(),
	      &(d_pressureBdry->d_cellTypeID),
	      &(d_pressureBdry->refPressure));

  // Put the calculated data into the new DW
  new_dw->put(uVelocity, d_lab->d_uVelocitySPBCLabel, matlIndex, patch);
  new_dw->put(vVelocity, d_lab->d_vVelocitySPBCLabel, matlIndex, patch);
  new_dw->put(wVelocity, d_lab->d_wVelocitySPBCLabel, matlIndex, patch);
  new_dw->put(pressure, d_lab->d_pressureSPBCLabel, matlIndex, patch);
} 

//****************************************************************************
// Schedule the setting of inlet velocity BC
//****************************************************************************
void 
BoundaryCondition::sched_setInletVelocityBC(const LevelP& level,
					    SchedulerP& sched,
					    DataWarehouseP& old_dw,
					    DataWarehouseP& new_dw)
{
  for(Level::const_patchIterator iter=level->patchesBegin();
      iter != level->patchesEnd(); iter++){
    const Patch* patch=*iter;
    {
      Task* tsk = scinew Task("BoundaryCondition::setInletVelocityBC",
			      patch, old_dw, new_dw, this,
			      &BoundaryCondition::setInletVelocityBC);

      int numGhostCells = 0;
      int matlIndex = 0;

      // This task requires densityCP, [u,v,w]VelocitySP from new_dw
      tsk->requires(old_dw, d_lab->d_cellTypeLabel, matlIndex, patch, Ghost::None,
		    numGhostCells);
      tsk->requires(new_dw, d_lab->d_densityINLabel, matlIndex, patch, Ghost::None,
		    numGhostCells);
      tsk->requires(new_dw, d_lab->d_uVelocityINLabel, matlIndex, patch, Ghost::None,
		    numGhostCells);
      tsk->requires(new_dw, d_lab->d_vVelocityINLabel, matlIndex, patch, Ghost::None,
		    numGhostCells);
      tsk->requires(new_dw, d_lab->d_wVelocityINLabel, matlIndex, patch, Ghost::None,
		    numGhostCells);

      // This task computes new density, uVelocity, vVelocity and wVelocity
      tsk->computes(new_dw, d_lab->d_uVelocitySIVBCLabel, matlIndex, patch);
      tsk->computes(new_dw, d_lab->d_vVelocitySIVBCLabel, matlIndex, patch);
      tsk->computes(new_dw, d_lab->d_wVelocitySIVBCLabel, matlIndex, patch);

      sched->addTask(tsk);
    }
  }
}

//****************************************************************************
// Schedule the compute of Pressure BC
//****************************************************************************
void 
BoundaryCondition::sched_recomputePressureBC(const LevelP& level,
					   SchedulerP& sched,
					   DataWarehouseP& old_dw,
					   DataWarehouseP& new_dw)
{
  for(Level::const_patchIterator iter=level->patchesBegin();
      iter != level->patchesEnd(); iter++){
    const Patch* patch=*iter;
    {
      Task* tsk = scinew Task("BoundaryCondition::recomputePressureBC",
			      patch, old_dw, new_dw, this,
			      &BoundaryCondition::recomputePressureBC);

      int numGhostCells = 0;
      int matlIndex = 0;

      // This task requires new density, pressure and velocity
      tsk->requires(new_dw, d_lab->d_densityINLabel, matlIndex, patch, Ghost::None,
		    numGhostCells);
      tsk->requires(new_dw, d_lab->d_pressurePSLabel, matlIndex, patch, Ghost::None,
		    numGhostCells);
      tsk->requires(new_dw, d_lab->d_uVelocitySIVBCLabel, matlIndex, patch, 
		    Ghost::None, numGhostCells);
      tsk->requires(new_dw, d_lab->d_vVelocitySIVBCLabel, matlIndex, patch, 
		    Ghost::None, numGhostCells);
      tsk->requires(new_dw, d_lab->d_wVelocitySIVBCLabel, matlIndex, patch, 
		    Ghost::None, numGhostCells);

      // This task computes new uVelocity, vVelocity and wVelocity
      tsk->computes(new_dw, d_lab->d_uVelocityCPBCLabel, matlIndex, patch);
      tsk->computes(new_dw, d_lab->d_vVelocityCPBCLabel, matlIndex, patch);
      tsk->computes(new_dw, d_lab->d_wVelocityCPBCLabel, matlIndex, patch);
      tsk->computes(new_dw, d_lab->d_pressureSPBCLabel, matlIndex, patch);

      sched->addTask(tsk);
    }
  }
}

//****************************************************************************
// Schedule computes inlet areas
// computes inlet area for inlet bc
//****************************************************************************
void 
BoundaryCondition::sched_calculateArea(const LevelP& level,
				       SchedulerP& sched,
				       DataWarehouseP& old_dw,
				       DataWarehouseP& new_dw)
{
  for(Level::const_patchIterator iter=level->patchesBegin();
      iter != level->patchesEnd(); iter++){
    const Patch* patch=*iter;
    {
      Task* tsk = new Task("BoundaryCondition::calculateArea",
			   patch, old_dw, new_dw, this,
			   &BoundaryCondition::computeInletFlowArea);
      cerr << "New task created successfully\n";
      int matlIndex = 0;
      int numGhostCells = 0;
      tsk->requires(old_dw, d_lab->d_cellTypeLabel, matlIndex, patch, Ghost::None,
		    numGhostCells);
      for (int ii = 0; ii < d_numInlets; ii++) {
	// make it simple by adding matlindex for reduction vars
	tsk->computes(old_dw, d_flowInlets[ii].d_area_label);
      }
      sched->addTask(tsk);
      cerr << "New task added successfully to scheduler\n";
    }
  }
}

//****************************************************************************
// Schedule set profile
// assigns flat velocity profiles for primary and secondary inlets
// Also sets flat profiles for density
//****************************************************************************
void 
BoundaryCondition::sched_setProfile(const LevelP& level,
				    SchedulerP& sched,
				    DataWarehouseP& old_dw,
				    DataWarehouseP& new_dw)
{
  for(Level::const_patchIterator iter=level->patchesBegin();
      iter != level->patchesEnd(); iter++){
    const Patch* patch=*iter;
    {
      Task* tsk = scinew Task("BoundaryCondition::setProfile",
			      patch, old_dw, new_dw, this,
			      &BoundaryCondition::setFlatProfile);
      int numGhostCells = 0;
      int matlIndex = 0;

      // This task requires cellTypeVariable and areaLabel for inlet boundary
      // Also densityIN, [u,v,w] velocityIN, scalarIN
      tsk->requires(old_dw, d_lab->d_cellTypeLabel, matlIndex, patch, Ghost::None,
		    numGhostCells);
      for (int ii = 0; ii < d_numInlets; ii++) {
	tsk->requires(old_dw, d_flowInlets[ii].d_area_label);
      }
      // This task requires old density, uVelocity, vVelocity and wVelocity
      tsk->requires(old_dw, d_lab->d_densityINLabel, matlIndex, patch, Ghost::None,
		    numGhostCells);
      tsk->requires(old_dw, d_lab->d_uVelocityINLabel, matlIndex, patch, Ghost::None,
		    numGhostCells);
      tsk->requires(old_dw, d_lab->d_vVelocityINLabel, matlIndex, patch, Ghost::None,
		    numGhostCells);
      tsk->requires(old_dw, d_lab->d_wVelocityINLabel, matlIndex, patch, Ghost::None,
		    numGhostCells);
      for (int ii = 0; ii < d_props->getNumMixVars(); ii++) 
	tsk->requires(old_dw, d_lab->d_scalarINLabel, ii, patch, Ghost::None,
		      numGhostCells);
      // This task computes new density, uVelocity, vVelocity and wVelocity, scalars
      tsk->computes(new_dw, d_lab->d_densitySPLabel, matlIndex, patch);
      tsk->computes(new_dw, d_lab->d_uVelocitySPLabel, matlIndex, patch);
      tsk->computes(new_dw, d_lab->d_vVelocitySPLabel, matlIndex, patch);
      tsk->computes(new_dw, d_lab->d_wVelocitySPLabel, matlIndex, patch);
      for (int ii = 0; ii < d_props->getNumMixVars(); ii++) 
	tsk->computes(new_dw, d_lab->d_scalarSPLabel, ii, patch);
      sched->addTask(tsk);
    }
  }
}

//****************************************************************************
// Actually calculate the velocity BC
//****************************************************************************
void 
BoundaryCondition::velocityBC(const ProcessorGroup* pc,
			      const Patch* patch,
			      DataWarehouseP& old_dw,
			      DataWarehouseP& new_dw,
			      int index, int eqnType,
			      CellInformation* cellinfo,
			      ArchesVariables* vars) 
{
  //get Molecular Viscosity of the fluid
  double molViscosity = d_turbModel->getMolecularViscosity();

  // Call the fortran routines
  switch(index) {
  case 1:
    uVelocityBC(new_dw, patch,
		&molViscosity, cellinfo, vars);
    break;
  case 2:
    vVelocityBC(new_dw, patch,
		&molViscosity, cellinfo, vars);
    break;
  case 3:
    wVelocityBC(new_dw, patch,
		&molViscosity, cellinfo, vars);
    break;
  default:
    cerr << "Invalid Index value" << endl;
    break;
  }
  // Calculate the velocity wall BC
  // For Arches::PRESSURE
  //  inputs : densityCP, [u,v,w]VelocitySIVBC, [u,v,w]VelCoefPBLM
  //           [u,v,w]VelLinSrcPBLM, [u,v,w]VelNonLinSrcPBLM
  //  outputs: [u,v,w]VelCoefPBLM, [u,v,w]VelLinSrcPBLM, 
  //           [u,v,w]VelNonLinSrcPBLM
  // For Arches::MOMENTUM
  //  inputs : densityCP, [u,v,w]VelocitySIVBC, [u,v,w]VelCoefMBLM
  //           [u,v,w]VelLinSrcMBLM, [u,v,w]VelNonLinSrcMBLM
  //  outputs: [u,v,w]VelCoefMBLM, [u,v,w]VelLinSrcMBLM, 
  //           [u,v,w]VelNonLinSrcMBLM
  //  d_turbModel->calcVelocityWallBC(pc, patch, old_dw, new_dw, index, eqnType);
}

//****************************************************************************
// call fortran routine to calculate the U Velocity BC
//****************************************************************************
void 
BoundaryCondition::uVelocityBC(DataWarehouseP& new_dw,
			       const Patch* patch,
			       const double* VISCOS,
			       CellInformation* cellinfo,
			       ArchesVariables* vars)
{
  int wall_celltypeval = d_wallBdry->d_cellTypeID;
  int flow_celltypeval = d_flowfieldCellTypeVal;
  // Get the low and high index for the patch and the variables
  IntVector domLoU = vars->uVelLinearSrc.getFortLowIndex();
  IntVector domHiU = vars->uVelLinearSrc.getFortHighIndex();
  IntVector domLo = vars->cellType.getFortLowIndex();
  IntVector domHi = vars->cellType.getFortHighIndex();
  // for a single patch should be equal to 1 and nx
  IntVector idxLoU = vars->cellType.getFortLowIndex();
  IntVector idxHiU = vars->cellType.getFortHighIndex();
  // computes momentum source term due to wall
  FORT_BCUVEL(domLoU.get_pointer(), domHiU.get_pointer(), 
	      idxLoU.get_pointer(), idxHiU.get_pointer(),
	      vars->uVelocity.getPointer(),
	      vars->uVelocityCoeff[Arches::AP].getPointer(), 
	      vars->uVelocityCoeff[Arches::AE].getPointer(), 
	      vars->uVelocityCoeff[Arches::AW].getPointer(), 
	      vars->uVelocityCoeff[Arches::AN].getPointer(), 
	      vars->uVelocityCoeff[Arches::AS].getPointer(), 
	      vars->uVelocityCoeff[Arches::AT].getPointer(), 
	      vars->uVelocityCoeff[Arches::AB].getPointer(), 
	      vars->uVelNonlinearSrc.getPointer(), vars->uVelLinearSrc.getPointer(),
	      domLo.get_pointer(), domHi.get_pointer(),
	      vars->cellType.getPointer(),
	      &wall_celltypeval, &flow_celltypeval, VISCOS, 
	      cellinfo->sewu.get_objs(), cellinfo->sns.get_objs(), 
	      cellinfo->stb.get_objs(),
	      cellinfo->yy.get_objs(), cellinfo->yv.get_objs(), 
	      cellinfo->zz.get_objs(),
	      cellinfo->zw.get_objs());
  cerr << "after uvelbc_fort" << endl;

}

//****************************************************************************
// call fortran routine to calculate the V Velocity BC
//****************************************************************************
void 
BoundaryCondition::vVelocityBC(DataWarehouseP& new_dw,
			       const Patch* patch,
			       const double* VISCOS,
			       CellInformation* cellinfo,
			       ArchesVariables* vars)
{
  int wall_celltypeval = d_wallBdry->d_cellTypeID;
  int flow_celltypeval = d_flowfieldCellTypeVal;
  // Get the low and high index for the patch and the variables
  IntVector domLoV = vars->vVelLinearSrc.getFortLowIndex();
  IntVector domHiV = vars->vVelLinearSrc.getFortHighIndex();
  IntVector domLo = vars->cellType.getFortLowIndex();
  IntVector domHi = vars->cellType.getFortHighIndex();
  // for a single patch should be equal to 1 and nx
  IntVector idxLoV = vars->cellType.getFortLowIndex();
  IntVector idxHiV = vars->cellType.getFortHighIndex();
  // computes momentum source term due to wall


  // computes remianing diffusion term and also computes source due to gravity
  FORT_BCVVEL(domLoV.get_pointer(), domHiV.get_pointer(), 
	      idxLoV.get_pointer(), idxHiV.get_pointer(),
	      vars->vVelocity.getPointer(),   
	      vars->vVelocityCoeff[Arches::AP].getPointer(), 
	      vars->vVelocityCoeff[Arches::AE].getPointer(), 
	      vars->vVelocityCoeff[Arches::AW].getPointer(), 
	      vars->vVelocityCoeff[Arches::AN].getPointer(), 
	      vars->vVelocityCoeff[Arches::AS].getPointer(), 
	      vars->vVelocityCoeff[Arches::AT].getPointer(), 
	      vars->vVelocityCoeff[Arches::AB].getPointer(), 
	      vars->vVelNonlinearSrc.getPointer(), vars->vVelLinearSrc.getPointer(),
	      domLo.get_pointer(), domHi.get_pointer(),
	      vars->cellType.getPointer(),
	      &wall_celltypeval, &flow_celltypeval, VISCOS, 
	      cellinfo->sew.get_objs(), cellinfo->snsv.get_objs(), 
	      cellinfo->stb.get_objs(),
	      cellinfo->xx.get_objs(), cellinfo->xu.get_objs(), 
	      cellinfo->zz.get_objs(),
	      cellinfo->zw.get_objs());

}

//****************************************************************************
// call fortran routine to calculate the W Velocity BC
//****************************************************************************
void 
BoundaryCondition::wVelocityBC(DataWarehouseP& new_dw,
			       const Patch* patch,
			       const double* VISCOS,
			       CellInformation* cellinfo,
			       ArchesVariables* vars)
{
  int wall_celltypeval = d_wallBdry->d_cellTypeID;
  int flow_celltypeval = d_flowfieldCellTypeVal;
  // Get the low and high index for the patch and the variables
  IntVector domLoW = vars->wVelLinearSrc.getFortLowIndex();
  IntVector domHiW = vars->wVelLinearSrc.getFortHighIndex();
  IntVector domLo = vars->cellType.getFortLowIndex();
  IntVector domHi = vars->cellType.getFortHighIndex();
  // for a single patch should be equal to 1 and nx
  IntVector idxLoW = vars->cellType.getFortLowIndex();
  IntVector idxHiW = vars->cellType.getFortHighIndex();
  // computes momentum source term due to wall
  FORT_BCWVEL(domLoW.get_pointer(), domHiW.get_pointer(), 
	      idxLoW.get_pointer(), idxHiW.get_pointer(),
	      vars->wVelocity.getPointer(),   
	      vars->wVelocityCoeff[Arches::AP].getPointer(), 
	      vars->wVelocityCoeff[Arches::AE].getPointer(), 
	      vars->wVelocityCoeff[Arches::AW].getPointer(), 
	      vars->wVelocityCoeff[Arches::AN].getPointer(), 
	      vars->wVelocityCoeff[Arches::AS].getPointer(), 
	      vars->wVelocityCoeff[Arches::AT].getPointer(), 
	      vars->wVelocityCoeff[Arches::AB].getPointer(), 
	      vars->wVelNonlinearSrc.getPointer(), vars->wVelLinearSrc.getPointer(),
	      domLo.get_pointer(), domHi.get_pointer(),
	      vars->cellType.getPointer(),
	      &wall_celltypeval, &flow_celltypeval, VISCOS, 
	      cellinfo->sew.get_objs(), cellinfo->sns.get_objs(), 
	      cellinfo->stbw.get_objs(),
	      cellinfo->xx.get_objs(), cellinfo->xu.get_objs(), 
	      cellinfo->yy.get_objs(),
	      cellinfo->yv.get_objs());

}

//****************************************************************************
// Actually compute the pressure bcs
//****************************************************************************
void 
BoundaryCondition::pressureBC(const ProcessorGroup*,
			      const Patch* patch,
			      DataWarehouseP& old_dw,
			      DataWarehouseP& new_dw,
			      CellInformation* cellinfo,
			      ArchesVariables* vars)
{
  // Get the low and high index for the patch
  IntVector domLo = vars->pressLinearSrc.getFortLowIndex();
  IntVector domHi = vars->pressLinearSrc.getFortHighIndex();
  IntVector idxLo = patch->getCellFORTLowIndex();
  IntVector idxHi = patch->getCellFORTHighIndex();

  // Get the wall boundary and flow field codes
  int wall_celltypeval = d_wallBdry->d_cellTypeID;
  int flow_celltypeval = d_flowfieldCellTypeVal;
  // ** WARNING ** Symmetry is hardcoded to -3
  int symmetry_celltypeval = -3;

  //fortran call
  FORT_PRESSBC(domLo.get_pointer(), domHi.get_pointer(),
	       idxLo.get_pointer(), idxHi.get_pointer(),
	       vars->pressure.getPointer(), 
	       vars->pressCoeff[Arches::AE].getPointer(),
	       vars->pressCoeff[Arches::AW].getPointer(),
	       vars->pressCoeff[Arches::AN].getPointer(),
	       vars->pressCoeff[Arches::AS].getPointer(),
	       vars->pressCoeff[Arches::AT].getPointer(),
	       vars->pressCoeff[Arches::AB].getPointer(),
	       vars->pressNonlinearSrc.getPointer(),
	       vars->pressLinearSrc.getPointer(),
	       vars->cellType.getPointer(),
	       &wall_celltypeval, &symmetry_celltypeval,
	       &flow_celltypeval);
}

//****************************************************************************
// Actually compute the scalar bcs
//****************************************************************************
void 
BoundaryCondition::scalarBC(const ProcessorGroup*,
			    const Patch* patch,
			    DataWarehouseP& old_dw,
			    DataWarehouseP& new_dw,
			    int index,
			    CellInformation* cellinfo,
			    ArchesVariables* vars)
{
  // Get the low and high index for the patch
  IntVector domLo = vars->scalar.getFortLowIndex();
  IntVector domHi = vars->scalar.getFortHighIndex();
  IntVector idxLo = patch->getCellFORTLowIndex();
  IntVector idxHi = patch->getCellFORTHighIndex();
  IntVector domLoU = vars->uVelocity.getFortLowIndex();
  IntVector domHiU = vars->uVelocity.getFortHighIndex();
  IntVector domLoV = vars->vVelocity.getFortLowIndex();
  IntVector domHiV = vars->vVelocity.getFortHighIndex();
  IntVector domLoW = vars->wVelocity.getFortLowIndex();
  IntVector domHiW = vars->wVelocity.getFortHighIndex();

  // Get the wall boundary and flow field codes
  int wall_celltypeval = d_wallBdry->d_cellTypeID;
  int flow_celltypeval = d_flowfieldCellTypeVal;
  // ** WARNING ** Symmetry/sfield/outletfield/ffield hardcoded to -3,-4,-5, -6
  //               Fmixin hardcoded to 0
  int symmetry_celltypeval = -3;
  int sfield = -4;
  int outletfield = -5;
  int ffield = -6;
  double fmixin = 0.0;

  //fortran call
  FORT_SCALARBC(domLo.get_pointer(), domHi.get_pointer(),
		idxLo.get_pointer(), idxHi.get_pointer(),
		vars->scalar.getPointer(), 
		vars->scalarCoeff[Arches::AE].getPointer(),
		vars->scalarCoeff[Arches::AW].getPointer(),
		vars->scalarCoeff[Arches::AN].getPointer(),
		vars->scalarCoeff[Arches::AS].getPointer(),
		vars->scalarCoeff[Arches::AT].getPointer(),
		vars->scalarCoeff[Arches::AB].getPointer(),
		vars->scalarNonlinearSrc.getPointer(),
		vars->scalarLinearSrc.getPointer(),
		vars->density.getPointer(),
		&fmixin,
		domLoU.get_pointer(), domHiU.get_pointer(),
		vars->uVelocity.getPointer(), 
		domLoV.get_pointer(), domHiV.get_pointer(),
		vars->vVelocity.getPointer(), 
		domLoW.get_pointer(), domHiW.get_pointer(),
		vars->wVelocity.getPointer(), 
		cellinfo->sew.get_objs(), cellinfo->sns.get_objs(), 
		cellinfo->stb.get_objs(),
		vars->cellType.getPointer(),
		&wall_celltypeval, &symmetry_celltypeval,
		&flow_celltypeval, &ffield, &sfield, &outletfield);
}


//****************************************************************************
// Actually set the inlet velocity BC
//****************************************************************************
void 
BoundaryCondition::setInletVelocityBC(const ProcessorGroup* ,
				      const Patch* patch,
				      DataWarehouseP& old_dw,
				      DataWarehouseP& new_dw) 
{
  CCVariable<int> cellType;
  CCVariable<double> density;
  SFCXVariable<double> uVelocity;
  SFCYVariable<double> vVelocity;
  SFCZVariable<double> wVelocity;

  int matlIndex = 0;
  int nofGhostCells = 0;

  // get cellType, velocity and density
  old_dw->get(cellType, d_lab->d_cellTypeLabel, matlIndex, patch, Ghost::None,
	      nofGhostCells);
  new_dw->get(uVelocity, d_lab->d_uVelocityINLabel, matlIndex, patch, Ghost::None,
	      nofGhostCells);
  new_dw->get(vVelocity, d_lab->d_vVelocityINLabel, matlIndex, patch, Ghost::None,
	      nofGhostCells);
  new_dw->get(wVelocity, d_lab->d_wVelocityINLabel, matlIndex, patch, Ghost::None,
	      nofGhostCells);
  new_dw->get(density, d_lab->d_densityINLabel, matlIndex, patch, Ghost::None,
	      nofGhostCells);

  // Get the low and high index for the patch and the variables
  IntVector domLo = density.getFortLowIndex();
  IntVector domHi = density.getFortHighIndex();
  IntVector idxLo = patch->getCellFORTLowIndex();
  IntVector idxHi = patch->getCellFORTHighIndex();
  IntVector domLoU = uVelocity.getFortLowIndex();
  IntVector domHiU = uVelocity.getFortHighIndex();
  IntVector domLoV = vVelocity.getFortLowIndex();
  IntVector domHiV = vVelocity.getFortHighIndex();
  IntVector domLoW = wVelocity.getFortLowIndex();
  IntVector domHiW = wVelocity.getFortHighIndex();
  // stores cell type info for the patch with the ghost cell type
  for (int indx = 0; indx < d_numInlets; indx++) {
    // Get a copy of the current flowinlet
    FlowInlet fi = d_flowInlets[indx];

    // assign flowType the value that corresponds to flow
    //CellTypeInfo flowType = FLOW;

    FORT_INLBCS(domLoU.get_pointer(), domHiU.get_pointer(), 
		uVelocity.getPointer(),
		domLoV.get_pointer(), domHiV.get_pointer(), 
		vVelocity.getPointer(),
		domLoW.get_pointer(), domHiW.get_pointer(), 
		wVelocity.getPointer(),
		domLo.get_pointer(), domHi.get_pointer(), 
		idxLo.get_pointer(), idxHi.get_pointer(), 
		density.getPointer(),
		cellType.getPointer(),
		&fi.d_cellTypeID); 
                //      &fi.flowRate, area, &fi.density, 
	        //	&fi.inletType,
	        //	flowType);

  }
  // Put the calculated data into the new DW
  new_dw->put(uVelocity, d_lab->d_uVelocitySIVBCLabel, matlIndex, patch);
  new_dw->put(vVelocity, d_lab->d_vVelocitySIVBCLabel, matlIndex, patch);
  new_dw->put(wVelocity, d_lab->d_wVelocitySIVBCLabel, matlIndex, patch);
}

//****************************************************************************
// Actually calculate the pressure BCs
//****************************************************************************
void 
BoundaryCondition::recomputePressureBC(const ProcessorGroup* ,
				    const Patch* patch,
				    DataWarehouseP& old_dw,
				    DataWarehouseP& new_dw) 
{
  CCVariable<int> cellType;
  CCVariable<double> density;
  CCVariable<double> pressure;
  SFCXVariable<double> uVelocity;
  SFCYVariable<double> vVelocity;
  SFCZVariable<double> wVelocity;
  int matlIndex = 0;
  int nofGhostCells = 0;

  // get cellType, pressure and velocity
  old_dw->get(cellType, d_lab->d_cellTypeLabel, matlIndex, patch, Ghost::None,
	      nofGhostCells);
  new_dw->get(density, d_lab->d_densityINLabel, matlIndex, patch, Ghost::None,
	      nofGhostCells);
  new_dw->get(pressure, d_lab->d_pressurePSLabel, matlIndex, patch, Ghost::None,
	      nofGhostCells);
  new_dw->get(uVelocity, d_lab->d_uVelocitySIVBCLabel, matlIndex, patch, Ghost::None,
	      nofGhostCells);
  new_dw->get(vVelocity, d_lab->d_vVelocitySIVBCLabel, matlIndex, patch, Ghost::None,
	      nofGhostCells);
  new_dw->get(wVelocity, d_lab->d_wVelocitySIVBCLabel, matlIndex, patch, Ghost::None,
	      nofGhostCells);

  // Get the low and high index for the patch and the variables
  IntVector domLoScalar = density.getFortLowIndex();
  IntVector domHiScalar = density.getFortHighIndex();
  IntVector idxLo = patch->getCellFORTLowIndex();
  IntVector idxHi = patch->getCellFORTHighIndex();
  IntVector domLoU = uVelocity.getFortLowIndex();
  IntVector domHiU = uVelocity.getFortHighIndex();
  IntVector domLoV = vVelocity.getFortLowIndex();
  IntVector domHiV = vVelocity.getFortHighIndex();
  IntVector domLoW = wVelocity.getFortLowIndex();
  IntVector domHiW = wVelocity.getFortHighIndex();

  FORT_CALPBC(domLoU.get_pointer(), domHiU.get_pointer(), 
	      uVelocity.getPointer(),
	      domLoV.get_pointer(), domHiV.get_pointer(), 
	      vVelocity.getPointer(),
	      domLoW.get_pointer(), domHiW.get_pointer(), 
	      wVelocity.getPointer(),
	      domLoScalar.get_pointer(), domHiScalar.get_pointer(), 
	      idxLo.get_pointer(), idxHi.get_pointer(), 
	      pressure.getPointer(),
	      density.getPointer(), 
	      cellType.getPointer(),
	      &(d_pressureBdry->d_cellTypeID),
	      &(d_pressureBdry->refPressure));

  // Put the calculated data into the new DW
  new_dw->put(uVelocity, d_lab->d_uVelocityCPBCLabel, matlIndex, patch);
  new_dw->put(vVelocity, d_lab->d_vVelocityCPBCLabel, matlIndex, patch);
  new_dw->put(wVelocity, d_lab->d_wVelocityCPBCLabel, matlIndex, patch);
  new_dw->put(pressure, d_lab->d_pressureSPBCLabel, matlIndex, patch);
} 

//****************************************************************************
// Actually set flat profile at flow inlet boundary
//****************************************************************************
void 
BoundaryCondition::setFlatProfile(const ProcessorGroup* pc,
				  const Patch* patch,
				  DataWarehouseP& old_dw,
				  DataWarehouseP& new_dw)
{
  CCVariable<int> cellType;
  CCVariable<double> density;
  SFCXVariable<double> uVelocity;
  SFCYVariable<double> vVelocity;
  SFCZVariable<double> wVelocity;
  vector<CCVariable<double> > scalar(d_nofScalars);
  int matlIndex = 0;
  int nofGhostCells = 0;

  // get cellType, density and velocity
  old_dw->get(cellType, d_lab->d_cellTypeLabel, matlIndex, patch, Ghost::None,
	      nofGhostCells);
  old_dw->get(density, d_lab->d_densityINLabel, matlIndex, patch, Ghost::None,
	      nofGhostCells);
  old_dw->get(uVelocity, d_lab->d_uVelocityINLabel, matlIndex, patch, Ghost::None,
	      nofGhostCells);
  old_dw->get(vVelocity, d_lab->d_vVelocityINLabel, matlIndex, patch, Ghost::None,
	      nofGhostCells);
  old_dw->get(wVelocity, d_lab->d_wVelocityINLabel, matlIndex, patch, Ghost::None,
	      nofGhostCells);
  for (int ii = 0; ii < d_nofScalars; ii++) {
    old_dw->get(scalar[ii], d_lab->d_scalarINLabel, ii, patch, Ghost::None,
	      nofGhostCells);
  }

  // Get the low and high index for the patch and the variables
  IntVector domLo = density.getFortLowIndex();
  IntVector domHi = density.getFortHighIndex();
  IntVector idxLo = patch->getCellFORTLowIndex();
  IntVector idxHi = patch->getCellFORTHighIndex();
  IntVector domLoU = uVelocity.getFortLowIndex();
  IntVector domHiU = uVelocity.getFortHighIndex();
  IntVector idxLoU = patch->getSFCXFORTLowIndex();
  IntVector idxHiU = patch->getSFCXFORTHighIndex();
  IntVector domLoV = vVelocity.getFortLowIndex();
  IntVector domHiV = vVelocity.getFortHighIndex();
  IntVector idxLoV = patch->getSFCYFORTLowIndex();
  IntVector idxHiV = patch->getSFCYFORTHighIndex();
  IntVector domLoW = wVelocity.getFortLowIndex();
  IntVector domHiW = wVelocity.getFortHighIndex();
  IntVector idxLoW = patch->getSFCZFORTLowIndex();
  IntVector idxHiW = patch->getSFCZFORTHighIndex();

  // loop thru the flow inlets to set all the components of velocity and density
  for (int indx = 0; indx < d_numInlets; indx++) {
    sum_vartype area_var;
    old_dw->get(area_var, d_flowInlets[indx].d_area_label);
    double area = area_var;
    // Get a copy of the current flowinlet
    // check if given patch intersects with the inlet boundary of type index
    FlowInlet fi = d_flowInlets[indx];

    FORT_PROFV(domLoU.get_pointer(), domHiU.get_pointer(),
	       idxLoU.get_pointer(), idxHiU.get_pointer(),
	       uVelocity.getPointer(), 
	       domLoV.get_pointer(), domHiV.get_pointer(),
	       idxLoV.get_pointer(), idxHiV.get_pointer(),
	       vVelocity.getPointer(),
	       domLoW.get_pointer(), domHiW.get_pointer(),
	       idxLoW.get_pointer(), idxHiW.get_pointer(),
	       wVelocity.getPointer(),
	       domLo.get_pointer(), domHi.get_pointer(),
	       idxLo.get_pointer(), idxHi.get_pointer(),
	       cellType.getPointer(), 
	       &area, &fi.d_cellTypeID, &fi.flowRate, 
	       &fi.density);
    FORT_PROFSCALAR(domLo.get_pointer(), domHi.get_pointer(),
		    idxLo.get_pointer(), idxHi.get_pointer(),
		    density.getPointer(), 
		    cellType.getPointer(),
		    &fi.density, &fi.d_cellTypeID);
  }   
  if (d_pressureBdry) {
    // set density
    FORT_PROFSCALAR(domLo.get_pointer(), domHi.get_pointer(), 
		    idxLo.get_pointer(), idxHi.get_pointer(),
		    density.getPointer(), cellType.getPointer(),
		    &d_pressureBdry->density, &d_pressureBdry->d_cellTypeID);
    // set scalar values at the boundary
    cerr << "nofscalar " << d_nofScalars << endl;
    for (int indx = 0; indx < d_nofScalars; indx++) {
      double scalarValue = d_pressureBdry->streamMixturefraction[indx];
      FORT_PROFSCALAR(domLo.get_pointer(), domHi.get_pointer(), 
		      idxLo.get_pointer(), idxHi.get_pointer(),
		      scalar[indx].getPointer(), cellType.getPointer(),
		      &scalarValue, &d_pressureBdry->d_cellTypeID);
    }
  }    
  for (int indx = 0; indx < d_nofScalars; indx++) {
    for (int ii = 0; ii < d_numInlets; ii++) {
      double scalarValue = d_flowInlets[ii].streamMixturefraction[indx];
      FORT_PROFSCALAR(domLo.get_pointer(), domHi.get_pointer(), 
		      idxLo.get_pointer(), idxHi.get_pointer(),
		      scalar[indx].getPointer(), cellType.getPointer(),
		      &scalarValue, &d_flowInlets[ii].d_cellTypeID);
    }
    if (d_pressBoundary) {
      double scalarValue = d_pressureBdry->streamMixturefraction[indx];
      cout << "Scalar Value = " << scalarValue << endl;
      FORT_PROFSCALAR(domLo.get_pointer(), domHi.get_pointer(),
		      idxLo.get_pointer(), idxHi.get_pointer(),
		      scalar[indx].getPointer(), cellType.getPointer(),
		      &scalarValue, &d_pressureBdry->d_cellTypeID);
    }
  }
      
  // Put the calculated data into the new DW
  new_dw->put(density, d_lab->d_densitySPLabel, matlIndex, patch);
  new_dw->put(uVelocity, d_lab->d_uVelocitySPLabel, matlIndex, patch);
  new_dw->put(vVelocity, d_lab->d_vVelocitySPLabel, matlIndex, patch);
  new_dw->put(wVelocity, d_lab->d_wVelocitySPLabel, matlIndex, patch);
  for (int ii =0; ii < d_nofScalars; ii++) {
    new_dw->put(scalar[ii], d_lab->d_scalarSPLabel, ii, patch);
  }

  // Testing if correct values have been put
  cout << "In set flat profile : " << endl;
  cout << "DomLo = (" << domLo.x() << "," << domLo.y() << "," << domLo.z() << ")\n";
  cout << "DomHi = (" << domHi.x() << "," << domHi.y() << "," << domHi.z() << ")\n";
  cout << "DomLoU = (" << domLoU.x()<<","<<domLoU.y()<< "," << domLoU.z() << ")\n";
  cout << "DomHiU = (" << domHiU.x()<<","<<domHiU.y()<< "," << domHiU.z() << ")\n";
  cout << "DomLoV = (" << domLoV.x()<<","<<domLoV.y()<< "," << domLoV.z() << ")\n";
  cout << "DomHiV = (" << domHiV.x()<<","<<domHiV.y()<< "," << domHiV.z() << ")\n";
  cout << "DomLoW = (" << domLoW.x()<<","<<domLoW.y()<< "," << domLoW.z() << ")\n";
  cout << "DomHiW = (" << domHiW.x()<<","<<domHiW.y()<< "," << domHiW.z() << ")\n";
  cout << "IdxLo = (" << idxLo.x() << "," << idxLo.y() << "," << idxLo.z() << ")\n";
  cout << "IdxHi = (" << idxHi.x() << "," << idxHi.y() << "," << idxHi.z() << ")\n";
  cout << "IdxLoU = (" << idxLoU.x()<<","<<idxLoU.y()<< "," << idxLoU.z() << ")\n";
  cout << "IdxHiU = (" << idxHiU.x()<<","<<idxHiU.y()<< "," << idxHiU.z() << ")\n";
  cout << "IdxLoV = (" << idxLoV.x()<<","<<idxLoV.y()<< "," << idxLoV.z() << ")\n";
  cout << "IdxHiV = (" << idxHiV.x()<<","<<idxHiV.y()<< "," << idxHiV.z() << ")\n";
  cout << "IdxLoW = (" << idxLoW.x()<<","<<idxLoW.y()<< "," << idxLoW.z() << ")\n";
  cout << "IdxHiW = (" << idxHiW.x()<<","<<idxHiW.y()<< "," << idxHiW.z() << ")\n";

  cout << " After setting flat profile (BoundaryCondition)" << endl;
  for (int kk = idxLoU.z(); kk <= idxHiU.z(); kk++) {
    cout << "U Velocity for kk = " << kk << endl;
    for (int ii = idxLoU.x(); ii <= idxHiU.x(); ii++) {
      for (int jj = idxLoU.y(); jj <= idxHiU.y(); jj++) {
	cout.width(10);
	cout << uVelocity[IntVector(ii,jj,kk)] << " " ; 
      }
      cout << endl;
    }
  }
  for (int kk = idxLoV.z(); kk <= idxHiV.z(); kk++) {
    cout << "U Velocity for kk = " << kk << endl;
    for (int ii = idxLoV.x(); ii <= idxHiV.x(); ii++) {
      for (int jj = idxLoV.y(); jj <= idxHiV.y(); jj++) {
	cout.width(10);
	cout << vVelocity[IntVector(ii,jj,kk)] << " " ; 
      }
      cout << endl;
    }
  }
  for (int kk = idxLoW.z(); kk <= idxHiW.z(); kk++) {
    cout << "U Velocity for kk = " << kk << endl;
    for (int ii = idxLoW.x(); ii <= idxHiW.x(); ii++) {
      for (int jj = idxLoW.y(); jj <= idxHiW.y(); jj++) {
	cout.width(10);
	cout << wVelocity[IntVector(ii,jj,kk)] << " " ; 
      }
      cout << endl;
    }
  }
  for (int kk = idxLo.z(); kk <= idxHi.z(); kk++) {
    cout << "Density for kk = " << kk << endl;
    for (int ii = idxLo.x(); ii <= idxHi.x(); ii++) {
      for (int jj = idxLo.y(); jj <= idxHi.y(); jj++) {
	cout.width(10);
	cout << density[IntVector(ii,jj,kk)] << " " ; 
      }
      cout << endl;
    }
  }
  for (int indx = 0; indx < d_nofScalars; indx++) {
    for (int kk = idxLo.z(); kk <= idxHi.z(); kk++) {
      cout << "Scalar " << indx <<" for kk = " << kk << endl;
      for (int ii = idxLo.x(); ii <= idxHi.x(); ii++) {
	for (int jj = idxLo.y(); jj <= idxHi.y(); jj++) {
	  cout.width(10);
	  cout << (scalar[indx])[IntVector(ii,jj,kk)] << " " ; 
	}
	cout << endl;
      }
    }
  }

}

//****************************************************************************
// constructor for BoundaryCondition::WallBdry
//****************************************************************************
BoundaryCondition::WallBdry::WallBdry(int cellID):
  d_cellTypeID(cellID)
{
}

//****************************************************************************
// Problem Setup for BoundaryCondition::WallBdry
//****************************************************************************
void 
BoundaryCondition::WallBdry::problemSetup(ProblemSpecP& params)
{
  ProblemSpecP geomObjPS = params->findBlock("geom_object");
  GeometryPieceFactory::create(geomObjPS, d_geomPiece);
  // loop thru all the wall bdry geometry objects
  //for (ProblemSpecP geom_obj_ps = params->findBlock("geom_object");
    // geom_obj_ps != 0; 
    // geom_obj_ps = geom_obj_ps->findNextBlock("geom_object") ) {
    //vector<GeometryPiece> pieces;
    //GeometryPieceFactory::create(geom_obj_ps, pieces);
    //if(pieces.size() == 0){
    //  throw ParameterNotFound("No piece specified in geom_object");
    //} else if(pieces.size() > 1){
    //  d_geomPiece = scinew UnionGeometryPiece(pieces);
    //} else {
    //  d_geomPiece = pieces[0];
    //}
  //}
}




//****************************************************************************
// constructor for BoundaryCondition::FlowInlet
//****************************************************************************
BoundaryCondition::FlowInlet::FlowInlet(int numMix, int cellID):
  d_cellTypeID(cellID)
{
  density = 0.0;
  turb_lengthScale = 0.0;
  flowRate = 0.0;
  // add cellId to distinguish different inlets
  d_area_label = scinew VarLabel("flowarea"+cellID,
   ReductionVariable<double, Reductions::Sum<double> >::getTypeDescription()); 
}

//****************************************************************************
// Problem Setup for BoundaryCondition::FlowInlet
//****************************************************************************
void 
BoundaryCondition::FlowInlet::problemSetup(ProblemSpecP& params)
{
  params->require("Flow_rate", flowRate);
  params->require("TurblengthScale", turb_lengthScale);
  // check to see if this will work
  ProblemSpecP geomObjPS = params->findBlock("geom_object");
  GeometryPieceFactory::create(geomObjPS, d_geomPiece);
  // loop thru all the inlet geometry objects
  //for (ProblemSpecP geom_obj_ps = params->findBlock("geom_object");
  //     geom_obj_ps != 0; 
  //     geom_obj_ps = geom_obj_ps->findNextBlock("geom_object") ) {
  //  vector<GeometryPiece*> pieces;
  //  GeometryPieceFactory::create(geom_obj_ps, pieces);
  //  if(pieces.size() == 0){
  //    throw ParameterNotFound("No piece specified in geom_object");
  //  } else if(pieces.size() > 1){
  //    d_geomPiece = scinew UnionGeometryPiece(pieces);
  //  } else {
  //    d_geomPiece = pieces[0];
  //  }
  //}

  double mixfrac;
  for (ProblemSpecP mixfrac_db = params->findBlock("MixtureFraction");
       mixfrac_db != 0; 
       mixfrac_db = mixfrac_db->findNextBlock("MixtureFraction")) {
    mixfrac_db->require("Mixfrac", mixfrac);
    streamMixturefraction.push_back(mixfrac);
  }
 
}


//****************************************************************************
// constructor for BoundaryCondition::PressureInlet
//****************************************************************************
BoundaryCondition::PressureInlet::PressureInlet(int numMix, int cellID):
  d_cellTypeID(cellID)
{
  //  streamMixturefraction.setsize(numMix-1);
  density = 0.0;
  turb_lengthScale = 0.0;
  refPressure = 0.0;
}

//****************************************************************************
// Problem Setup for BoundaryCondition::PressureInlet
//****************************************************************************
void 
BoundaryCondition::PressureInlet::problemSetup(ProblemSpecP& params)
{
  params->require("RefPressure", refPressure);
  params->require("TurblengthScale", turb_lengthScale);
  ProblemSpecP geomObjPS = params->findBlock("geom_object");
  GeometryPieceFactory::create(geomObjPS, d_geomPiece);
  // loop thru all the pressure inlet geometry objects
  //for (ProblemSpecP geom_obj_ps = params->findBlock("geom_object");
  //     geom_obj_ps != 0; 
  //     geom_obj_ps = geom_obj_ps->findNextBlock("geom_object") ) {
  //  vector<GeometryPiece*> pieces;
  //  GeometryPieceFactory::create(geom_obj_ps, pieces);
  //  if(pieces.size() == 0){
  //    throw ParameterNotFound("No piece specified in geom_object");
  //  } else if(pieces.size() > 1){
  //    d_geomPiece = scinew UnionGeometryPiece(pieces);
  //  } else {
  //    d_geomPiece = pieces[0];
  //  }
  //}
  double mixfrac;
  for (ProblemSpecP mixfrac_db = params->findBlock("MixtureFraction");
       mixfrac_db != 0; 
       mixfrac_db = mixfrac_db->findNextBlock("MixtureFraction")) {
    mixfrac_db->require("Mixfrac", mixfrac);
    streamMixturefraction.push_back(mixfrac);
  }
}

//****************************************************************************
// constructor for BoundaryCondition::FlowOutlet
//****************************************************************************
BoundaryCondition::FlowOutlet::FlowOutlet(int numMix, int cellID):
  d_cellTypeID(cellID)
{
  //  streamMixturefraction.setsize(numMix-1);
  density = 0.0;
  turb_lengthScale = 0.0;
}

//****************************************************************************
// Problem Setup for BoundaryCondition::FlowOutlet
//****************************************************************************
void 
BoundaryCondition::FlowOutlet::problemSetup(ProblemSpecP& params)
{
  params->require("TurblengthScale", turb_lengthScale);
  ProblemSpecP geomObjPS = params->findBlock("geom_object");
  GeometryPieceFactory::create(geomObjPS, d_geomPiece);
  // loop thru all the inlet geometry objects
  //for (ProblemSpecP geom_obj_ps = params->findBlock("geom_object");
  //     geom_obj_ps != 0; 
  //     geom_obj_ps = geom_obj_ps->findNextBlock("geom_object") ) {
  //  vector<GeometryPiece*> pieces;
  //  GeometryPieceFactory::create(geom_obj_ps, pieces);
  //  if(pieces.size() == 0){
  //    throw ParameterNotFound("No piece specified in geom_object");
  //  } else if(pieces.size() > 1){
  //    d_geomPiece = scinew UnionGeometryPiece(pieces);
  //  } else {
  //    d_geomPiece = pieces[0];
  //  }
  //}
  double mixfrac;
  for (ProblemSpecP mixfrac_db = params->findBlock("MixtureFraction");
       mixfrac_db != 0; 
       mixfrac_db = mixfrac_db->findNextBlock("MixtureFraction")) {
    mixfrac_db->require("Mixfrac", mixfrac);
    streamMixturefraction.push_back(mixfrac);
  }
}

//
// $Log$
// Revision 1.48  2000/07/30 22:21:21  bbanerje
// Added bcscalar.F (originally bcf.f in Kumar's code) needs more work
// in C++ side.
//
// Revision 1.47  2000/07/28 02:30:59  rawat
// moved all the labels in ArchesLabel. fixed some bugs and added matrix_dw to store matrix
// coeffecients
//
// Revision 1.44  2000/07/17 22:06:58  rawat
// modified momentum source
//
// Revision 1.43  2000/07/14 03:45:45  rawat
// completed velocity bc and fixed some bugs
//
// Revision 1.42  2000/07/13 06:32:09  bbanerje
// Labels are once more consistent for one iteration.
//
// Revision 1.41  2000/07/13 04:51:32  bbanerje
// Added pressureBC (bcp) .. now called bcpress.F (bcp.F removed)
//
// Revision 1.40  2000/07/12 23:59:21  rawat
// added wall bc for u-velocity
//
// Revision 1.39  2000/07/11 15:46:27  rawat
// added setInitialGuess in PicardNonlinearSolver and also added uVelSrc
//
// Revision 1.38  2000/07/08 23:42:53  bbanerje
// Moved all enums to Arches.h and made corresponding changes.
//
// Revision 1.37  2000/07/08 08:03:33  bbanerje
// Readjusted the labels upto uvelcoef, removed bugs in CellInformation,
// made needed changes to uvelcoef.  Changed from StencilMatrix::AE etc
// to Arches::AE .. doesn't like enums in templates apparently.
//
// Revision 1.36  2000/07/07 23:07:44  rawat
// added inlet bc's
//
// Revision 1.35  2000/07/03 05:30:13  bbanerje
// Minor changes for inlbcs dummy code to compile and work. densitySIVBC is no more.
//
// Revision 1.34  2000/07/02 05:47:29  bbanerje
// Uncommented all PerPatch and CellInformation stuff.
// Updated array sizes in inlbcs.F
//
// Revision 1.33  2000/06/30 22:41:18  bbanerje
// Corrected behavior of profv and profscalar
//
// Revision 1.32  2000/06/30 06:29:42  bbanerje
// Got Inlet Area to be calculated correctly .. but now two CellInformation
// variables are being created (Rawat ... check that).
//
// Revision 1.31  2000/06/30 04:19:16  rawat
// added turbulence model and compute properties
//
// Revision 1.30  2000/06/29 06:22:47  bbanerje
// Updated FCVariable to SFCX, SFCY, SFCZVariables and made corresponding
// changes to profv.  Code is broken until the changes are reflected
// thru all the files.
//
// Revision 1.29  2000/06/28 08:14:52  bbanerje
// Changed the init routines a bit.
//
// Revision 1.28  2000/06/22 23:06:33  bbanerje
// Changed velocity related variables to FCVariable type.
// ** NOTE ** We may need 3 types of FCVariables (one for each direction)
//
// Revision 1.27  2000/06/21 07:50:59  bbanerje
// Corrected new_dw, old_dw problems, commented out intermediate dw (for now)
// and made the stuff go through schedule_time_advance.
//
// Revision 1.26  2000/06/21 06:49:21  bbanerje
// Straightened out some of the problems in data location .. still lots to go.
//
// Revision 1.25  2000/06/20 20:42:36  rawat
// added some more boundary stuff and modified interface to IntVector. Before
// compiling the code you need to update /SCICore/Geometry/IntVector.h
//
// Revision 1.24  2000/06/19 18:00:29  rawat
// added function to compute velocity and density profiles and inlet bc.
// Fixed bugs in CellInformation.cc
//
// Revision 1.23  2000/06/18 01:20:14  bbanerje
// Changed names of varlabels in source to reflect the sequence of tasks.
// Result : Seg Violation in addTask in MomentumSolver
//
// Revision 1.22  2000/06/17 07:06:22  sparker
// Changed ProcessorContext to ProcessorGroup
//
// Revision 1.21  2000/06/16 21:50:47  bbanerje
// Changed the Varlabels so that sequence in understood in init stage.
// First cycle detected in task graph.
//
// Revision 1.20  2000/06/16 07:06:16  bbanerje
// Added init of props, pressure bcs and turbulence model in Arches.cc
// Changed duplicate task names (setProfile) in BoundaryCondition.cc
// Commented out nolinear_dw creation in PicardNonlinearSolver.cc
//
// Revision 1.19  2000/06/15 23:47:56  rawat
// modified Archesfort to fix function call
//
// Revision 1.18  2000/06/15 22:13:22  rawat
// modified boundary stuff
//
// Revision 1.17  2000/06/15 08:48:12  bbanerje
// Removed most commented stuff , added StencilMatrix, tasks etc.  May need some
// more work
//
//
