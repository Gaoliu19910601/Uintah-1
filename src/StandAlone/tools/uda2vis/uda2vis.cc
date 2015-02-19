/*
 * The MIT License
 *
 * Copyright (c) 1997-2015 The University of Utah
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

/*
 *  uda2nrrd.cc: Provides an interface between VisIt and Uintah.
 *
 *  Written by:
 *   Department of Computer Science
 *   University of Utah
 *   April 2003-2007
 *
 */

#include <StandAlone/tools/uda2vis/uda2Vis.h>

#include <Core/DataArchive/DataArchive.h>

#include <iostream>
#include <string>
#include <vector>
#include <sstream>
#include <iomanip>
#include <cstdio>
#include <algorithm>

using namespace std;
using namespace Uintah;


/////////////////////////////////////////////////////////////////////
// Utility functions for copying data from Uintah structures into
// simple arrays.
void copyIntVector(int to[3], const IntVector &from) {
  to[0]=from[0];  to[1]=from[1];  to[2]=from[2];
}

void copyVector(double to[3], const Vector &from) {
  to[0]=from[0];  to[1]=from[1];  to[2]=from[2];
}

void copyVector(double to[3], const Point &from) {
  to[0]=from.x();  to[1]=from.y();  to[2]=from.z();
}

/////////////////////////////////////////////////////////////////////
// Utility functions for serializing Uintah data structures into
// a simple array for visit.
template <typename T>
int numComponents() {
  return 1;
}

template <>
int numComponents<Vector>() {
  return 3;
}

template <>
int numComponents<Stencil7>() {
  return 7;
}

template <>
int numComponents<Stencil4>() {
  return 4;
}

template <>
int numComponents<Point>() {
  return 3;
}

template <>
int numComponents<Matrix3>() {
  return 9;
}

template <typename T>
void copyComponents(double *dest, const T &src) {
  (*dest) = (double)src;
}

template <>
void copyComponents<Vector>(double *dest, const Vector &src) {
  dest[0] = (double)src[0];
  dest[1] = (double)src[1];
  dest[2] = (double)src[2];
}

template <>
void copyComponents<Stencil7>(double *dest, const Stencil7 &src) {
  dest[0] = (double)src[0];
  dest[1] = (double)src[1];
  dest[2] = (double)src[2];
  dest[3] = (double)src[3];
  dest[4] = (double)src[4];
  dest[5] = (double)src[5];
  dest[6] = (double)src[6];
}

template <>
void copyComponents<Stencil4>(double *dest, const Stencil4 &src) {
  dest[0] = (double)src[0];
  dest[1] = (double)src[1];
  dest[2] = (double)src[2];
  dest[3] = (double)src[3];
}

template <>
void copyComponents<Point>(double *dest, const Point &src) {
  dest[0] = (double)src.x();
  dest[1] = (double)src.y();
  dest[2] = (double)src.z();
}

template <>
void copyComponents<Matrix3>(double *dest, const Matrix3 &src) {
  for (int i=0; i<3; i++) {
    for (int j=0; j<3; j++) {
      dest[i*3+j] = (double)src(i,j);
    }
  }
}


/////////////////////////////////////////////////////////////////////
// Open a data archive.
extern "C"
DataArchive*
openDataArchive(const string& input_uda_name) {

  DataArchive *archive = scinew DataArchive(input_uda_name);

  return archive;
}


/////////////////////////////////////////////////////////////////////
// Close a data archive - the visit plugin itself doesn't know about
// DataArchive::~DataArchive().
extern "C"
void
closeDataArchive(DataArchive *archive) {
  delete archive;
}


/////////////////////////////////////////////////////////////////////
// Get the grid for the current timestep, so we don't have to query
// it over and over.  We return a pointer to the GridP since the 
// visit plugin doesn't actually know about Grid's (or GridP's), and
// so the handle doesn't get destructed.
extern "C"
GridP*
getGrid(DataArchive *archive, int timeStepNo) {
  GridP *grid = new GridP(archive->queryGrid(timeStepNo));
  return grid;
}


/////////////////////////////////////////////////////////////////////
// Destruct the GridP, which will decrement the reference count.
extern "C"
void
releaseGrid(GridP *grid) {
  delete grid;
}


/////////////////////////////////////////////////////////////////////
// Get the time for each cycle.
extern "C"
vector<double>
getCycleTimes(DataArchive *archive) {

  // Get the times and indices.
  vector<int> index;
  vector<double> times;

  // query time info from dataarchive
  archive->queryTimesteps(index, times);

  return times;
} 

  
/////////////////////////////////////////////////////////////////////
// Get all the information that may be needed for the current timestep,
// including variable/material info, and level/patch info
extern "C"
TimeStepInfo*
getTimeStepInfo(DataArchive *archive, GridP *grid, int timestep, bool useExtraCells) {
  int numLevels = (*grid)->numLevels();
  TimeStepInfo *stepInfo = new TimeStepInfo();
  stepInfo->levelInfo.resize(numLevels);

  // get variable information
  vector<string> vars;
  vector<const Uintah::TypeDescription*> types;
  archive->queryVariables(vars, types);
  stepInfo->varInfo.resize(vars.size());

  for (unsigned int i=0; i<vars.size(); i++) {
    VariableInfo &varInfo = stepInfo->varInfo[i];

    varInfo.name = vars[i];
    varInfo.type = types[i]->getName();

    // query each level for material info until we find something
    for (int l=0; l<numLevels; l++) {
      LevelP level = (*grid)->getLevel(l);
      const Patch* patch = *(level->patchesBegin());
      ConsecutiveRangeSet matls = archive->queryMaterials(vars[i], patch, timestep);
      if (matls.size() > 0) {

        // copy the list of materials
        for (ConsecutiveRangeSet::iterator matlIter = matls.begin();
             matlIter != matls.end(); matlIter++)
          varInfo.materials.push_back(*matlIter);

        // don't query on any more levels
        break;
      }
    }
  }


  // get level information
  for (int l=0; l<numLevels; l++) {
    LevelInfo &levelInfo = stepInfo->levelInfo[l];
    LevelP level = (*grid)->getLevel(l);

    copyIntVector(levelInfo.refinementRatio, level->getRefinementRatio());
    copyVector(levelInfo.spacing, level->dCell());
    copyVector(levelInfo.anchor, level->getAnchor());
    copyIntVector(levelInfo.periodic, level->getPeriodicBoundaries());

    // patch info
    int numPatches = level->numPatches();
    levelInfo.patchInfo.resize(numPatches);

    for (int p=0; p<numPatches; p++) {
      const Patch* patch = level->getPatch(p);
      PatchInfo &patchInfo = levelInfo.patchInfo[p];

      // If the user wants to see extra cells, just include them and let VisIt believe they are
      // part of the original data. This is accomplished by setting <meshtype>_low and
      // <meshtype>_high to the extra cell boundaries so that VisIt is none the wiser.
      if (useExtraCells)
      {
        patchInfo.setBounds(&patch->getExtraCellLowIndex()[0],&patch->getExtraCellHighIndex()[0],"CC_Mesh");
        patchInfo.setBounds(&patch->getExtraNodeLowIndex()[0],&patch->getExtraNodeHighIndex()[0],"NC_Mesh");
        patchInfo.setBounds(&patch->getExtraSFCXLowIndex()[0],&patch->getExtraSFCXHighIndex()[0],"SFCX_Mesh");
        patchInfo.setBounds(&patch->getExtraSFCYLowIndex()[0],&patch->getExtraSFCYHighIndex()[0],"SFCY_Mesh");
        patchInfo.setBounds(&patch->getExtraSFCZLowIndex()[0],&patch->getExtraSFCZHighIndex()[0],"SFCZ_Mesh");
      }
      else
      {
        patchInfo.setBounds(&patch->getCellLowIndex()[0],&patch->getCellHighIndex()[0],"CC_Mesh");
        patchInfo.setBounds(&patch->getNodeLowIndex()[0],&patch->getNodeHighIndex()[0],"NC_Mesh");
        patchInfo.setBounds(&patch->getSFCXLowIndex()[0],&patch->getSFCXHighIndex()[0],"SFCX_Mesh");
        patchInfo.setBounds(&patch->getSFCYLowIndex()[0],&patch->getSFCYHighIndex()[0],"SFCY_Mesh");
        patchInfo.setBounds(&patch->getSFCZLowIndex()[0],&patch->getSFCZHighIndex()[0],"SFCZ_Mesh");
      }

      //set processor id
      patchInfo.setProcId(archive->queryPatchwiseProcessor(patch, timestep));
    }
  }


  return stepInfo;
}


/////////////////////////////////////////////////////////////////////
// Read the grid data for the given index range
template<template <typename> class VAR, typename T>
static GridDataRaw* readGridData(DataArchive *archive,
                                 const Patch *patch,
                                 const LevelP level,
                                 string variable_name,
                                 int material,
                                 int timestep,
                                 int low[3],
                                 int high[3]) {

  IntVector ilow(low[0], low[1], low[2]);
  IntVector ihigh(high[0], high[1], high[2]);

  // this queries the entire patch, including extra cells and boundary cells
  VAR<T> var;
  archive->queryRegion(var, variable_name, material, level.get_rep(), timestep, ilow, ihigh);

  //  IntVector low = var.getLowIndex();
  //  IntVector high = var.getHighIndex();

  GridDataRaw *gd = new GridDataRaw;
  gd->components = numComponents<T>();
  for (int i=0; i<3; i++) {
    gd->low[i] = low[i];
    gd->high[i] = high[i];
  }

  int n = (high[0]-low[0])*(high[1]-low[1])*(high[2]-low[2]);
  gd->data = new double[n*gd->components];

  T *p=var.getPointer();
  for (int i=0; i<n; i++)
    copyComponents<T>(&gd->data[i*gd->components], p[i]);
  
  return gd;
}


template<template<typename> class VAR>
GridDataRaw* getGridDataMainType(DataArchive *archive,
                                 const Patch *patch,
                                 const LevelP level,
                                 string variable_name,
                                 int material,
                                 int timestep,
                                 int low[3],
                                 int high[3],
                                 const Uintah::TypeDescription *subtype) {

  switch (subtype->getType()) {
  case Uintah::TypeDescription::double_type:
    return readGridData<VAR, double>(archive, patch, level, variable_name, material, timestep, low, high);
  case Uintah::TypeDescription::float_type:
    return readGridData<VAR, float>(archive, patch, level, variable_name, material, timestep, low, high);
  case Uintah::TypeDescription::int_type:
    return readGridData<VAR, int>(archive, patch, level, variable_name, material, timestep, low, high);
  case Uintah::TypeDescription::Vector:
    return readGridData<VAR, Vector>(archive, patch, level, variable_name, material, timestep, low, high);
  case Uintah::TypeDescription::Stencil7:
    return readGridData<VAR, Stencil7>(archive, patch, level, variable_name, material, timestep, low, high);
  case Uintah::TypeDescription::Stencil4:
    return readGridData<VAR, Stencil4>(archive, patch, level, variable_name, material, timestep, low, high);
  case Uintah::TypeDescription::Matrix3:
    return readGridData<VAR, Matrix3>(archive, patch, level, variable_name, material, timestep, low, high);
  case Uintah::TypeDescription::bool_type:
  case Uintah::TypeDescription::short_int_type:
  case Uintah::TypeDescription::long_type:
  case Uintah::TypeDescription::long64_type:
    cerr << "Subtype " << subtype->getName() << " is not implemented...\n";
    return NULL;
  default:
    cerr << "Unknown subtype: "
	 <<subtype->getType()<<"  "
	 <<subtype->getName()<<"\n";
    return NULL;
  }
}


extern "C"
GridDataRaw*
getGridData(DataArchive *archive,
            GridP *grid,
            int level_i,
            int patch_i,
            string variable_name,
            int material,
            int timestep,
            int low[3],
            int high[3]) {

  LevelP level = (*grid)->getLevel(level_i);
  const Patch *patch = level->getPatch(patch_i);

  // figure out what the type of the variable we're querying is
  vector<string> vars;
  vector<const Uintah::TypeDescription*> types;
  archive->queryVariables(vars, types);

  const Uintah::TypeDescription* maintype = NULL;
  const Uintah::TypeDescription* subtype = NULL;

  for (unsigned int i=0; i<vars.size(); i++) {
    if (vars[i] == variable_name) {
      maintype = types[i];
      subtype = maintype->getSubType();
    }
  }

  if (!maintype || !subtype) {
    cerr<<"couldn't find variable " << variable_name<<endl;
    return NULL;
  }


  switch(maintype->getType()) {
  case Uintah::TypeDescription::CCVariable:
    return getGridDataMainType<CCVariable>(archive, patch, level, variable_name, material, timestep, low, high, subtype);
  case Uintah::TypeDescription::NCVariable:
    return getGridDataMainType<NCVariable>(archive, patch, level, variable_name, material, timestep, low, high, subtype);
  case Uintah::TypeDescription::SFCXVariable:
    return getGridDataMainType<SFCXVariable>(archive, patch, level, variable_name, material, timestep, low, high, subtype);
  case Uintah::TypeDescription::SFCYVariable:
    return getGridDataMainType<SFCYVariable>(archive, patch, level, variable_name, material, timestep, low, high, subtype);
  case Uintah::TypeDescription::SFCZVariable:
    return getGridDataMainType<SFCZVariable>(archive, patch, level, variable_name, material, timestep, low, high, subtype);
  default:
    cerr << "Type is unknown.\n";
    return NULL;
  }
}




/////////////////////////////////////////////////////////////////////
// Read all the particle data for a given patch.
template<typename T>
ParticleDataRaw* readParticleData(DataArchive *archive,
                                  const Patch *patch,
                                  string variable_name,
                                  int material,
                                  int timestep) {

  ParticleDataRaw *pd = new ParticleDataRaw;
  pd->components = numComponents<T>();
  pd->num = 0;

  // figure out which material we're interested in
  ConsecutiveRangeSet allMatls = archive->queryMaterials(variable_name, patch, timestep);

  ConsecutiveRangeSet matlsForVar;
  if (material<0) {
    matlsForVar = allMatls;
  }
  else {
    // make sure the patch has the variable - use empty material set if it doesn't
    if (allMatls.size()>0 && allMatls.find(material) != allMatls.end())
      matlsForVar.addInOrder(material);
  }

  // first get all the particle subsets so that we know how many total particles we'll have
  vector<ParticleVariable<T>*> particle_vars;
  for( ConsecutiveRangeSet::iterator matlIter = matlsForVar.begin(); matlIter != matlsForVar.end(); matlIter++ ) {
    int matl = *matlIter;

    ParticleVariable<T> *var = new ParticleVariable<T>;
    archive->query(*var, variable_name, matl, patch, timestep);

    particle_vars.push_back(var);
    pd->num += var->getParticleSubset()->numParticles();
  }

  // copy all the data
  int pi=0;
  pd->data = new double[pd->components * pd->num];
  for (unsigned int i=0; i<particle_vars.size(); i++) {
    for (ParticleSubset::iterator p = particle_vars[i]->getParticleSubset()->begin();
         p != particle_vars[i]->getParticleSubset()->end(); ++p) {

      //TODO: need to be able to read data as array of longs for particle id, but copyComponents always reads double
      copyComponents<T>(&pd->data[pi*pd->components],
                        (*particle_vars[i])[*p]);
      pi++;
    }
  }

  // cleanup
  for (unsigned int i=0; i<particle_vars.size(); i++)
    delete particle_vars[i];

  return pd;
}



extern "C"
ParticleDataRaw*
getParticleData(DataArchive *archive,
                GridP *grid,
                int level_i,
                int patch_i,
                string variable_name,
                int material,
                int timestep) {

  LevelP level = (*grid)->getLevel(level_i);
  const Patch *patch = level->getPatch(patch_i);

  // figure out what the type of the variable we're querying is
  vector<string> vars;
  vector<const Uintah::TypeDescription*> types;
  archive->queryVariables(vars, types);

  const Uintah::TypeDescription* maintype = NULL;
  const Uintah::TypeDescription* subtype = NULL;

  for (unsigned int i=0; i<vars.size(); i++) {
    if (vars[i] == variable_name) {
      maintype = types[i];
      subtype = maintype->getSubType();
    }
  }

  if (!maintype || !subtype) {
    cerr<<"couldn't find variable " << variable_name<<endl;
    return NULL;
  }


  switch (subtype->getType()) {
  case Uintah::TypeDescription::double_type:
    return readParticleData<double>(archive, patch, variable_name, material, timestep);
  case Uintah::TypeDescription::float_type:
    return readParticleData<float>(archive, patch, variable_name, material, timestep);
  case Uintah::TypeDescription::int_type:
    return readParticleData<int>(archive, patch, variable_name, material, timestep);
  case Uintah::TypeDescription::long64_type:
    return readParticleData<long64>(archive, patch, variable_name, material, timestep);
  case Uintah::TypeDescription::Point:
    return readParticleData<Point>(archive, patch, variable_name, material, timestep);
  case Uintah::TypeDescription::Vector:
    return readParticleData<Vector>(archive, patch, variable_name, material, timestep);
  case Uintah::TypeDescription::Stencil7:
    return readParticleData<Stencil7>(archive, patch, variable_name, material, timestep);
  case Uintah::TypeDescription::Stencil4:
    return readParticleData<Stencil4>(archive, patch, variable_name, material, timestep);
  case Uintah::TypeDescription::Matrix3:
    return readParticleData<Matrix3>(archive, patch, variable_name, material, timestep);
  default:
    cerr << "Unknown subtype for particle data: " << subtype->getName() << "\n";
    return NULL;
  }
}

extern "C"
std::string
getParticlePositionName(DataArchive *archive) {
    return archive->getParticlePositionName();
}








namespace Uintah {

/////////////////////////////////////////////////////////////////////
// Get all the information that may be needed for the current timestep,
// including variable/material info, and level/patch info
TimeStepInfo* getTimeStepInfo2(SchedulerP schedulerP,
			       GridP gridP,
			       int timestep,
			       bool useExtraCells)
{
  int numLevels = gridP->numLevels();
  TimeStepInfo *stepInfo = new TimeStepInfo();
  stepInfo->levelInfo.resize(numLevels);

  // get the variable information
  const std::set<const VarLabel*, VarLabel::Compare> varLabels =
    schedulerP->getInitialRequiredVars();

  // get the material information
  Scheduler::VarLabelMaterialMap* pLabelMatlMap =
    schedulerP->makeVarLabelMaterialMap();

  unsigned int i = 0;
  std::set<const VarLabel*, VarLabel::Compare>::iterator varIter;

  stepInfo->varInfo.resize(varLabels.size());

  // loop through all of the variables
  for (varIter = varLabels.begin(); varIter != varLabels.end(); varIter++, i++)
  {
    VariableInfo &varInfo = stepInfo->varInfo[i];

    varInfo.name = (*varIter)->getName();
    varInfo.type = (*varIter)->typeDescription()->getName();

    // loop through all of the materials for this variable
    Scheduler::VarLabelMaterialMap::iterator matMapIter =
      pLabelMatlMap->find( varInfo.name );

    std::list< int > &materials = matMapIter->second;
    std::list< int >::iterator matIter;

    for (matIter = materials.begin(); matIter != materials.end(); matIter++)
    {
      varInfo.materials.push_back( *matIter );
    }
  }

  // get level information
  for (int l=0; l<numLevels; l++) {
    LevelInfo &levelInfo = stepInfo->levelInfo[l];
    LevelP level = gridP->getLevel(l);

    copyIntVector(levelInfo.refinementRatio, level->getRefinementRatio());
    copyVector(levelInfo.spacing, level->dCell());
    copyVector(levelInfo.anchor, level->getAnchor());
    copyIntVector(levelInfo.periodic, level->getPeriodicBoundaries());

    // patch info
    int numPatches = level->numPatches();
    levelInfo.patchInfo.resize(numPatches);

    for (int p=0; p<numPatches; p++) {
      const Patch* patch = level->getPatch(p);
      PatchInfo &patchInfo = levelInfo.patchInfo[p];

      // If the user wants to see extra cells, just include them and
      // let VisIt believe they are part of the original data. This is
      // accomplished by setting <meshtype>_low and <meshtype>_high to
      // the extra cell boundaries so that VisIt is none the wiser.
      if (useExtraCells)
      {
        patchInfo.setBounds(&patch->getExtraCellLowIndex()[0],&patch->getExtraCellHighIndex()[0],"CC_Mesh");
        patchInfo.setBounds(&patch->getExtraNodeLowIndex()[0],&patch->getExtraNodeHighIndex()[0],"NC_Mesh");
        patchInfo.setBounds(&patch->getExtraSFCXLowIndex()[0],&patch->getExtraSFCXHighIndex()[0],"SFCX_Mesh");
        patchInfo.setBounds(&patch->getExtraSFCYLowIndex()[0],&patch->getExtraSFCYHighIndex()[0],"SFCY_Mesh");
        patchInfo.setBounds(&patch->getExtraSFCZLowIndex()[0],&patch->getExtraSFCZHighIndex()[0],"SFCZ_Mesh");
      }
      else
      {
        patchInfo.setBounds(&patch->getCellLowIndex()[0],&patch->getCellHighIndex()[0],"CC_Mesh");
        patchInfo.setBounds(&patch->getNodeLowIndex()[0],&patch->getNodeHighIndex()[0],"NC_Mesh");
        patchInfo.setBounds(&patch->getSFCXLowIndex()[0],&patch->getSFCXHighIndex()[0],"SFCX_Mesh");
        patchInfo.setBounds(&patch->getSFCYLowIndex()[0],&patch->getSFCYHighIndex()[0],"SFCY_Mesh");
        patchInfo.setBounds(&patch->getSFCZLowIndex()[0],&patch->getSFCZHighIndex()[0],"SFCZ_Mesh");
      }

      //set processor id
//      patchInfo.setProcId(archive->queryPatchwiseProcessor(patch, timestep));
    }
  }

  return stepInfo;
}

// ****************************************************************************
//  Method: getBounds
//
//  Purpose:
//   Returns the bounds for the given patch of the specified mesh 
//   based on periodicity and type.
//
//  Node centered data uses the same mesh as cell centered, 
//  but face centered meshes need an extra value for one axis,
//  unless they are periodic on that axis.
//
//  use patch_id=-1 to query all patches.
//
// ****************************************************************************
void getBounds(int low[3], int high[3],
	       const string meshName, const LevelInfo &levelInfo,int patch_id)
{
  levelInfo.getBounds(low,high,meshName,patch_id);
  
  // debug5 << "getBounds(" << meshName << ",id=" << patch_id << ")=["
  // 	 << low[0] << "," << low[1] << "," << low[2] << "] to ["
  // 	 << high[0] << "," << high[1] << "," << high[2] << "]" << std::endl;
}

// ****************************************************************************
//  Method: avtUintahFileFormat::GetLevelAndLocalPatchNumber
//
//  Purpose:
//      Translates the global patch identifier to a refinement level and patch
//      number local to that refinement level.
//  
//  Programmer: sshankar, taken from implementation of the plugin, CHOMBO
//  Creation:   May 20, 2008
//
// ****************************************************************************
void GetLevelAndLocalPatchNumber(TimeStepInfo* stepInfo,
				 int global_patch, 
				 int &level, int &local_patch)
{
  int num_levels = stepInfo->levelInfo.size();
  int num_patches = 0;
  int tmp = global_patch;
  level = 0;

  while (level < num_levels) {
    num_patches = stepInfo->levelInfo[level].patchInfo.size();
    if (tmp < num_patches)
      break;
    tmp -= num_patches;
    level++;
  }
  local_patch = tmp;
}

// ****************************************************************************
//  Method: avtUintahFileFormat::GetGlobalDomainNumber
//
//  Purpose:
//      Translates the level and local patch number into a global patch id.
//  
// ****************************************************************************
int GetGlobalDomainNumber(TimeStepInfo* stepInfo,
			  int level, int local_patch)
{
  int g = 0;

  for (int l=0; l<level; l++)
    g += stepInfo->levelInfo[l].patchInfo.size();
  g += local_patch;

  return g;
}

// ****************************************************************************
//  Method: avtUintahFileFormat::CheckNaNs
//
//  Purpose:
//      Check for and warn about NaN values in the file.
//
//  Arguments:
//      num        data size
//      data       data
//      level      level that contains this patch
//      patch      patch that contains these cells
//
//  Returns:    none
//
//  Programmer: cchriste
//  Creation:   06.02.2012
//
//  Modifications:
const double NAN_REPLACE_VAL = 1.0E9;

void CheckNaNs(int num, double *data, int level, int patch)
{
  // replace nan's with a large negative number
  std::vector<int> nanCells;
  for (int i=0; i<num; i++) 
  {
    if (std::isnan(data[i]))
    {
      data[i] = NAN_REPLACE_VAL;
      nanCells.push_back(i);
    }
  }

  if (!nanCells.empty())
  {
    std::stringstream sstr;
    sstr << "NaNs exist in this file (patch "<<patch<<" of level "<<level
         <<"). They have been replaced by the value "<< NAN_REPLACE_VAL<<".";
    if ((int)nanCells.size()>40)
    {
      sstr<<"\nFirst 20: ";
      for (int i=0;i<(int)nanCells.size() && i<20;i++)
        sstr<<nanCells[i]<<",";
      sstr<<"\nLast 20: ";
      for (int i=(int)nanCells.size()-21;i<(int)nanCells.size();i++)
        sstr<<nanCells[i]<<",";
    }
    else
    {
      for (int i=0;i<(int)nanCells.size();i++)
        sstr<<nanCells[i]<<((int)nanCells.size()!=(i+1)?",":".");
    }
    // ARS - FIX ME
    // avtCallback::IssueWarning(sstr.str().c_str());
  }
}

// ****************************************************************************
//  Method: avtUintahFileFormat::CalculateDomainNesting
//
//  Purpose:
//      Calculates two important data structures.  One is the structure domain
//      nesting, which tells VisIt how the AMR patches are nested, which allows
//      VisIt to ghost out coarse zones that are refined by smaller zones.
//      The other structure is the rectilinear domain boundaries, which tells
//      VisIt which patches are next to each other, allowing VisIt to create
//      a layer of ghost zones around each patch.  Note that this only works
//      within a refinement level, not across refinement levels.
//  
//
// NOTE: The cache variable for the mesh MUST be called "any_mesh",
// which is a problem when there are multiple meshes or one of them is
// actually named "any_mesh" (see
// https://visitbugs.ornl.gov/issues/52). Thus, for each mesh we keep
// around our own cache variable and if this function finds it then it
// just uses it again instead of recomputing it.
//
// ****************************************************************************
void CalculateDomainNesting(TimeStepInfo* stepInfo,
			    std::map<std::string, void *> mesh_domains,
			    std::map<std::string, void *> mesh_boundaries,
			    bool &forceMeshReload,
			    int timestate, const std::string &meshname)
{
  //lookup mesh in our cache and if it's not there, compute it
  if (mesh_domains[meshname] == NULL || forceMeshReload == true)
  {
    //
    // Calculate some info we will need in the rest of the routine.
    //
    int num_levels = stepInfo->levelInfo.size();
    int totalPatches = 0;
    for (int level = 0 ; level < num_levels ; level++)
      totalPatches += stepInfo->levelInfo[level].patchInfo.size();

    //
    // Now set up the data structure for patch boundaries.  The data 
    // does all the work ... it just needs to know the extents of each patch.
    //
    avtRectilinearDomainBoundaries *rdb =
      new avtRectilinearDomainBoundaries(true);

    // debug5 << "Calculating avtRectilinearDomainBoundaries for "
    // 	   << meshname << " mesh (" << rdb << ")." << std::endl;

    rdb->SetNumDomains(totalPatches);

    for (int patch = 0 ; patch < totalPatches ; patch++) {
      int my_level, local_patch;
      GetLevelAndLocalPatchNumber(stepInfo, patch, my_level, local_patch);

      PatchInfo &patchInfo = stepInfo->levelInfo[my_level].patchInfo[local_patch];

      int low[3],high[3];
      patchInfo.getBounds(low,high,meshname);

      int e[6] = { low[0], high[0],
                   low[1], high[1],
                   low[2], high[2] };
      // debug5 << "\trdb->SetIndicesForAMRPatch(" << patch << ","
      // 	     << my_level << ", <" << e[0] << "," << e[2] << "," << e[4]
      // 	     << "> to <" << e[1] << "," << e[3] << "," << e[5] << ">)"
      //             << std::endl;
      rdb->SetIndicesForAMRPatch(patch, my_level, e);
    }

    rdb->CalculateBoundaries();

    mesh_boundaries[meshname] =
       void_ref_ptr(rdb, avtStructuredDomainBoundaries::Destruct);
    
    //
    // Domain Nesting
    //
    avtStructuredDomainNesting *dn =
      new avtStructuredDomainNesting(totalPatches, num_levels);

    //debug5 << "Calculating avtStructuredDomainNesting for "
    //       << meshname << " mesh (" << dn << ")." << std::endl;
    dn->SetNumDimensions(3);

    //
    // Calculate what the refinement ratio is from one level to the next.
    //
    for (int level = 0 ; level < num_levels ; level++) {
      // SetLevelRefinementRatios requires data as a vector<int>
      vector<int> rr(3);
      for (int i=0; i<3; i++)
        rr[i] = stepInfo->levelInfo[level].refinementRatio[i];

      // debug5 << "\tdn->SetLevelRefinementRatios(" << level << ", <"
      //        << rr[0] << "," << rr[1] << "," << rr[2] << ">)\n";
      dn->SetLevelRefinementRatios(level, rr);
    }

    //
    // Calculating the child patches really needs some better sorting than
    // what I am doing here.  This is likely to become a bottleneck in extreme
    // cases.  Although this routine has performed well for a previous 55K
    // patch run.
    //
    vector< vector<int> > childPatches(totalPatches);

    for (int level = num_levels-1 ; level > 0 ; level--)
    {
      int prev_level = level-1;
      LevelInfo &levelInfoParent = stepInfo->levelInfo[prev_level];
      LevelInfo &levelInfoChild = stepInfo->levelInfo[level];

      for (int child=0; child<(int)levelInfoChild.patchInfo.size(); child++)
      {
        PatchInfo &childPatchInfo = levelInfoChild.patchInfo[child];
        int child_low[3],child_high[3];
        childPatchInfo.getBounds(child_low,child_high,meshname);

        for (int parent = 0;
	     parent<(int)levelInfoParent.patchInfo.size(); parent++)
	{
          PatchInfo &parentPatchInfo = levelInfoParent.patchInfo[parent];
          int parent_low[3],parent_high[3];
          parentPatchInfo.getBounds(parent_low,parent_high,meshname);

          int mins[3], maxs[3];
          for (int i=0; i<3; i++)
	  {
            mins[i] = max(child_low[i],
			  parent_low[i] *levelInfoChild.refinementRatio[i]);
            maxs[i] = min(child_high[i],
			  parent_high[i]*levelInfoChild.refinementRatio[i]);
          }

          bool overlap = (mins[0]<maxs[0] &&
                          mins[1]<maxs[1] &&
                          mins[2]<maxs[2]);

          if (overlap)
	  {
            int child_gpatch = GetGlobalDomainNumber(stepInfo, level, child);
            int parent_gpatch = GetGlobalDomainNumber(stepInfo, prev_level, parent);
            childPatches[parent_gpatch].push_back(child_gpatch);
          }
        }
      }
    }

    //
    // Now that we know the extents for each patch and what its children are,
    // tell the structured domain boundary that information.
    //
    for (int p=0; p<totalPatches ; p++)
    {
      int my_level, local_patch;
      GetLevelAndLocalPatchNumber(stepInfo, p, my_level, local_patch);

      PatchInfo &patchInfo =
	stepInfo->levelInfo[my_level].patchInfo[local_patch];
      int low[3],high[3];
      patchInfo.getBounds(low,high,meshname);

      vector<int> e(6);
      for (int i=0; i<3; i++) {
        e[i+0] = low[i];
        e[i+3] = high[i]-1;
      }
      //debug5<<"\tdn->SetNestingForDomain("<<p<<","<<my_level<<", <>, <"<<e[0]<<","<<e[1]<<","<<e[2]<<"> to <"<<e[3]<<","<<e[4]<<","<<e[5]<<">)\n";
      dn->SetNestingForDomain(p, my_level, childPatches[p], e);
    }

    mesh_domains[meshname] =
       void_ref_ptr(dn, avtStructuredDomainNesting::Destruct);

    forceMeshReload = false;
  }

#ifdef COMMENT_OUT_FOR_NOW
  //
  // Register these structures with the generic database so that it knows
  // to ghost out the right cells.
  //
  cache->CacheVoidRef("any_mesh", // key MUST be called any_mesh
                      AUXILIARY_DATA_DOMAIN_BOUNDARY_INFORMATION,
                      timestate, -1, mesh_boundaries[meshname]);
  cache->CacheVoidRef("any_mesh", // key MUST be called any_mesh
                      AUXILIARY_DATA_DOMAIN_NESTING_INFORMATION,
                      timestate, -1, mesh_domains[meshname]);

  //VERIFY we got the mesh boundary and domain in there
  void_ref_ptr vrTmp = cache->GetVoidRef("any_mesh", // MUST be called any_mesh
                            AUXILIARY_DATA_DOMAIN_BOUNDARY_INFORMATION,
                            timestate, -1);
  if (*vrTmp == NULL || *vrTmp != mesh_boundaries[meshname])
    throw InvalidFilesException("uda boundary mesh not registered");

  vrTmp = cache->GetVoidRef("any_mesh", // MUST be called any_mesh
                            AUXILIARY_DATA_DOMAIN_NESTING_INFORMATION,
                            timestate, -1);
  if (*vrTmp == NULL || *vrTmp != mesh_domains[meshname])
    throw InvalidFilesException("uda domain mesh not registered");
#endif
}




/////////////////////////////////////////////////////////////////////
// Read the grid data for the given index range
template<template <typename> class VAR, typename T>
static GridDataRaw* readGridData(SchedulerP schedulerP,
                                 const Patch *patch,
                                 const LevelP level,
                                 string variable_name,
                                 int material,
                                 int timestep,
                                 int low[3],
                                 int high[3]) {

  IntVector ilow(low[0], low[1], low[2]);
  IntVector ihigh(high[0], high[1], high[2]);

  // this queries the entire patch, including extra cells and boundary cells
  VAR<T> var;

  // ARS - FIX ME
  // schedulerP->queryRegion(var, variable_name, material,
  // 			  level.get_rep(), timestep, ilow, ihigh);

  //  IntVector low = var.getLowIndex();
  //  IntVector high = var.getHighIndex();

  GridDataRaw *gd = new GridDataRaw;
  gd->components = numComponents<T>();
  for (int i=0; i<3; i++) {
    gd->low[i] = low[i];
    gd->high[i] = high[i];
  }

  int n = (high[0]-low[0])*(high[1]-low[1])*(high[2]-low[2]);
  gd->data = new double[n*gd->components];

  T *p=var.getPointer();
  for (int i=0; i<n; i++)
    copyComponents<T>(&gd->data[i*gd->components], p[i]);
  
  return gd;
}


template<template<typename> class VAR>
GridDataRaw* getGridDataMainType(SchedulerP schedulerP,
                                 const Patch *patch,
                                 const LevelP level,
                                 string variable_name,
                                 int material,
                                 int timestep,
                                 int low[3],
                                 int high[3],
                                 const Uintah::TypeDescription *subtype) {

  switch (subtype->getType()) {
  case Uintah::TypeDescription::double_type:
    return readGridData<VAR, double>(schedulerP, patch, level, variable_name, material, timestep, low, high);
  case Uintah::TypeDescription::float_type:
    return readGridData<VAR, float>(schedulerP, patch, level, variable_name, material, timestep, low, high);
  case Uintah::TypeDescription::int_type:
    return readGridData<VAR, int>(schedulerP, patch, level, variable_name, material, timestep, low, high);
  case Uintah::TypeDescription::Vector:
    return readGridData<VAR, Vector>(schedulerP, patch, level, variable_name, material, timestep, low, high);
  case Uintah::TypeDescription::Stencil7:
    return readGridData<VAR, Stencil7>(schedulerP, patch, level, variable_name, material, timestep, low, high);
  case Uintah::TypeDescription::Stencil4:
    return readGridData<VAR, Stencil4>(schedulerP, patch, level, variable_name, material, timestep, low, high);
  case Uintah::TypeDescription::Matrix3:
    return readGridData<VAR, Matrix3>(schedulerP, patch, level, variable_name, material, timestep, low, high);
  case Uintah::TypeDescription::bool_type:
  case Uintah::TypeDescription::short_int_type:
  case Uintah::TypeDescription::long_type:
  case Uintah::TypeDescription::long64_type:
    cerr << "Subtype " << subtype->getName() << " is not implemented...\n";
    return NULL;
  default:
    cerr << "Unknown subtype: "
	 <<subtype->getType()<<"  "
	 <<subtype->getName()<<"\n";
    return NULL;
  }
}


GridDataRaw* getGridData2(SchedulerP schedulerP,
			  GridP gridP,
			  int level_i,
			  int patch_i,
			  string variable_name,
			  int material,
			  int timestep,
			  int low[3],
			  int high[3])
{
  LevelP level = gridP->getLevel(level_i);
  const Patch *patch = level->getPatch(patch_i);

  // get the variable information
  const std::set<const VarLabel*, VarLabel::Compare> varLabels =
    schedulerP->getInitialRequiredVars();

  std::set<const VarLabel*, VarLabel::Compare>::iterator varIter;

  const Uintah::TypeDescription* maintype = NULL;
  const Uintah::TypeDescription* subtype = NULL;

  for (varIter = varLabels.begin(); varIter != varLabels.end(); varIter++)
  {
    if ((*varIter)->getName() == variable_name) {
    
      maintype = (*varIter)->typeDescription();
      subtype = (*varIter)->typeDescription()->getSubType();
    }
  }

  // // figure out what the type of the variable we're querying is
  // vector<string> vars;
  // vector<const Uintah::TypeDescription*> types;
  // schedulerP->queryVariables(vars, types);

  // const Uintah::TypeDescription* maintype = NULL;
  // const Uintah::TypeDescription* subtype = NULL;

  // for (unsigned int i=0; i<vars.size(); i++) {
  //   if (vars[i] == variable_name) {
  //     maintype = types[i];
  //     subtype = maintype->getSubType();
  //   }
  // }

  if (!maintype || !subtype) {
    cerr<<"couldn't find variable " << variable_name<<endl;
    return NULL;
  }


  switch(maintype->getType()) {
  case Uintah::TypeDescription::CCVariable:
    return getGridDataMainType<CCVariable>(schedulerP, patch, level, variable_name, material, timestep, low, high, subtype);
  case Uintah::TypeDescription::NCVariable:
    return getGridDataMainType<NCVariable>(schedulerP, patch, level, variable_name, material, timestep, low, high, subtype);
  case Uintah::TypeDescription::SFCXVariable:
    return getGridDataMainType<SFCXVariable>(schedulerP, patch, level, variable_name, material, timestep, low, high, subtype);
  case Uintah::TypeDescription::SFCYVariable:
    return getGridDataMainType<SFCYVariable>(schedulerP, patch, level, variable_name, material, timestep, low, high, subtype);
  case Uintah::TypeDescription::SFCZVariable:
    return getGridDataMainType<SFCZVariable>(schedulerP, patch, level, variable_name, material, timestep, low, high, subtype);
  default:
    cerr << "Type is unknown.\n";
    return NULL;
  }
}



/////////////////////////////////////////////////////////////////////
// Read all the particle data for a given patch.
template<typename T>
ParticleDataRaw* readParticleData(SchedulerP schedulerP,
				  const Patch *patch,
				  string variable_name,
				  int material,
				  int timestep)
{
  ParticleDataRaw *pd = new ParticleDataRaw;
  pd->components = numComponents<T>();
  pd->num = 0;

  // get the material information for all variables
  Scheduler::VarLabelMaterialMap* pLabelMatlMap =
    schedulerP->makeVarLabelMaterialMap();

  // get the materials for this variable
  Scheduler::VarLabelMaterialMap::iterator matMapIter =
    pLabelMatlMap->find( variable_name );

  // figure out which material we're interested in
  std::list< int > &allMatls = matMapIter->second;
  std::list< int > matlsForVar;

  if (material < 0)
  {
    matlsForVar = allMatls;
  }
  else
  {
    // make sure the patch has the variable - use empty material set
    // if it doesn't
    for (std::list< int >::iterator matIter = allMatls.begin();
	 matIter != allMatls.end(); matIter++)
    {
      if( *matIter == material )
      {
	matlsForVar.push_back(material);
	break;
      }
    }
  }

  // first get all the particle subsets so that we know how many total
  // particles we'll have
  vector<ParticleVariable<T>*> particle_vars;

  for( std::list< int >::iterator matIter = matlsForVar.begin();
       matIter != matlsForVar.end(); matIter++ )
  {
    int matl = *matIter;

    ParticleVariable<T> *var = new ParticleVariable<T>;
    // ARS - FIX ME  
    //schedulerP->query(*var, variable_name, matl, patch, timestep);

    particle_vars.push_back(var);
    pd->num += var->getParticleSubset()->numParticles();
  }

  // figure out which material we're interested in
  // ConsecutiveRangeSet allMatls =
  //   archive->queryMaterials(variable_name, patch, timestep);

  // ConsecutiveRangeSet matlsForVar;

  // if (material < 0)
  // {
  //   matlsForVar = allMatls;
  // }
  // else
  // {
       // make sure the patch has the variable - use empty material set
       // if it doesn't
  //   if (0 < allMatls.size() && allMatls.find(material) != allMatls.end())
  //     matlsForVar.addInOrder(material);
  // }

  // first get all the particle subsets so that we know how many total
  // particles we'll have
  // vector<ParticleVariable<T>*> particle_vars;

  // for( ConsecutiveRangeSet::iterator matlIter = matlsForVar.begin();
  //      matlIter != matlsForVar.end(); matlIter++ )
  // {
  //   int matl = *matlIter;

  //   ParticleVariable<T> *var = new ParticleVariable<T>;
  //   archive->query(*var, variable_name, matl, patch, timestep);

  //   particle_vars.push_back(var);
  //   pd->num += var->getParticleSubset()->numParticles();
  // }

  // copy all the data
  int pi=0;
  pd->data = new double[pd->components * pd->num];

  for (unsigned int i=0; i<particle_vars.size(); i++)
  {
    ParticleSubset *pSubset = particle_vars[i]->getParticleSubset();

    for (ParticleSubset::iterator p = pSubset->begin();
         p != pSubset->end(); ++p)
    {

      //TODO: need to be able to read data as array of longs for
      //particle id, but copyComponents always reads double
      copyComponents<T>(&pd->data[pi*pd->components],
                        (*particle_vars[i])[*p]);
      pi++;
    }
  }

  // cleanup
  for (unsigned int i=0; i<particle_vars.size(); i++)
    delete particle_vars[i];

  return pd;
}

ParticleDataRaw* getParticleData2(SchedulerP schedulerP,
				  GridP gridP,
				  int level_i,
				  int patch_i,
				  string variable_name,
				  int material,
				  int timestep)
{
  LevelP level = gridP->getLevel(level_i);
  const Patch *patch = level->getPatch(patch_i);

  // get the variable information
  const std::set<const VarLabel*, VarLabel::Compare> varLabels =
    schedulerP->getInitialRequiredVars();

  std::set<const VarLabel*, VarLabel::Compare>::iterator varIter;

  const Uintah::TypeDescription* maintype = NULL;
  const Uintah::TypeDescription* subtype = NULL;

  for (varIter = varLabels.begin(); varIter != varLabels.end(); varIter++)
  {
    if ((*varIter)->getName() == variable_name) {
    
      maintype = (*varIter)->typeDescription();
      subtype = (*varIter)->typeDescription()->getSubType();
    }
  }

  // figure out what the type of the variable we're querying is
  // vector<string> vars;
  // vector<const Uintah::TypeDescription*> types;
  // archive->queryVariables(vars, types);

  // const Uintah::TypeDescription* maintype = NULL;
  // const Uintah::TypeDescription* subtype = NULL;

  // for (unsigned int i=0; i<vars.size(); i++) {
  //   if (vars[i] == variable_name) {
  //     maintype = types[i];
  //     subtype = maintype->getSubType();
  //   }
  // }

  if (!maintype || !subtype) {
    cerr<<"couldn't find variable " << variable_name<<endl;
    return NULL;
  }


  switch (subtype->getType()) {
  case Uintah::TypeDescription::double_type:
    return readParticleData<double>(schedulerP, patch, variable_name, material, timestep);
  case Uintah::TypeDescription::float_type:
    return readParticleData<float>(schedulerP, patch, variable_name, material, timestep);
  case Uintah::TypeDescription::int_type:
    return readParticleData<int>(schedulerP, patch, variable_name, material, timestep);
  case Uintah::TypeDescription::long64_type:
    return readParticleData<long64>(schedulerP, patch, variable_name, material, timestep);
  case Uintah::TypeDescription::Point:
    return readParticleData<Point>(schedulerP, patch, variable_name, material, timestep);
  case Uintah::TypeDescription::Vector:
    return readParticleData<Vector>(schedulerP, patch, variable_name, material, timestep);
  case Uintah::TypeDescription::Stencil7:
    return readParticleData<Stencil7>(schedulerP, patch, variable_name, material, timestep);
  case Uintah::TypeDescription::Stencil4:
    return readParticleData<Stencil4>(schedulerP, patch, variable_name, material, timestep);
  case Uintah::TypeDescription::Matrix3:
    return readParticleData<Matrix3>(schedulerP, patch, variable_name, material, timestep);
  default:
    cerr << "Unknown subtype for particle data: " << subtype->getName() << "\n";
    return NULL;
  }
}

}
