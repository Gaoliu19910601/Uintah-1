// TODO
// track down tiny eloss - why????
// Use cp directly instead of cv/gamma

#include <Packages/Uintah/CCA/Components/ICE/ICEMaterial.h>
#include <Packages/Uintah/CCA/Components/ICE/BoundaryCond.h>
#include <Packages/Uintah/CCA/Components/ICE/Diffusion.h>
#include <Packages/Uintah/CCA/Components/Models/test/AdiabaticTable.h>
#include <Packages/Uintah/CCA/Components/Models/test/TableFactory.h>
#include <Packages/Uintah/CCA/Components/Models/test/TableInterface.h>
#include <Packages/Uintah/CCA/Ports/Scheduler.h>
#include <Packages/Uintah/Core/Exceptions/ProblemSetupException.h>
#include <Packages/Uintah/Core/ProblemSpec/ProblemSpec.h>
#include <Packages/Uintah/Core/Grid/Box.h>
#include <Packages/Uintah/Core/Grid/CellIterator.h>
#include <Packages/Uintah/Core/Grid/Level.h>
#include <Packages/Uintah/Core/Grid/Material.h>
#include <Packages/Uintah/Core/Grid/SimulationState.h>
#include <Packages/Uintah/Core/Grid/VarTypes.h>
#include <Packages/Uintah/Core/Grid/GeomPiece/GeometryPieceFactory.h>
#include <Packages/Uintah/Core/Grid/GeomPiece/UnionGeometryPiece.h>
#include <Packages/Uintah/Core/Labels/ICELabel.h>
#include <Packages/Uintah/Core/Exceptions/ParameterNotFound.h>
#include <Packages/Uintah/Core/Parallel/ProcessorGroup.h>
#include <Packages/Uintah/Core/Exceptions/InvalidValue.h>
#include <Core/Math/MiscMath.h>
#include <iostream>
#include <Core/Util/DebugStream.h>
#include <stdio.h>

// TODO:
// 1. Call modifyThermo from intialize instead of duping code
// Drive density...

using namespace Uintah;
using namespace std;
//__________________________________
//  To turn on the output
//  setenv SCI_DEBUG "MODELS_DOING_COUT:+,ADIABATIC_TABLE_DBG_COUT:+"
//  ADIABATIC_TABLE_DBG:  dumps out during problemSetup
static DebugStream cout_doing("MODELS_DOING_COUT", false);
static DebugStream cout_dbg("ADIABATIC_TABLE_DBG_COUT", false);
/*`==========TESTING==========*/
static DebugStream oldStyleAdvect("oldStyleAdvect",false); 
/*==========TESTING==========`*/
//______________________________________________________________________              
AdiabaticTable::AdiabaticTable(const ProcessorGroup* myworld, 
                     ProblemSpecP& params)
  : ModelInterface(myworld), params(params)
{
  d_matl_set = 0;
  lb  = scinew ICELabel();
  cumulativeEnergyReleased_CCLabel = VarLabel::create("cumulativeEnergyReleased", CCVariable<double>::getTypeDescription());
  cumulativeEnergyReleased_src_CCLabel = VarLabel::create("cumulativeEnergyReleased_src", CCVariable<double>::getTypeDescription());
}


//__________________________________
AdiabaticTable::~AdiabaticTable()
{
  if(d_matl_set && d_matl_set->removeReference()) {
    delete d_matl_set;
  }
  
  VarLabel::destroy(d_scalar->scalar_CCLabel);
  VarLabel::destroy(d_scalar->diffusionCoefLabel);
  VarLabel::destroy(cumulativeEnergyReleased_CCLabel);
  VarLabel::destroy(cumulativeEnergyReleased_src_CCLabel);
  delete lb;
  delete table;
  for(int i=0;i<(int)tablevalues.size();i++)
    delete tablevalues[i];
  
  for(vector<Region*>::iterator iter = d_scalar->regions.begin();
                                iter != d_scalar->regions.end(); iter++){
    Region* region = *iter;
    delete region->piece;
    delete region;

  }
}

//__________________________________
AdiabaticTable::Region::Region(GeometryPiece* piece, ProblemSpecP& ps)
  : piece(piece)
{
  ps->require("scalar", initialScalar);
}
//______________________________________________________________________
//     P R O B L E M   S E T U P
void AdiabaticTable::problemSetup(GridP&, SimulationStateP& in_state,
                        ModelSetup* setup)
{
/*`==========TESTING==========*/
if (!oldStyleAdvect.active()){
  ostringstream desc;
  desc<< "\n----------------------------\n"
      <<" ICE need the following environmental variable \n"
       << " \t setenv SCI_DEBUG oldStyleAdvect:+ \n"
       << "for this model to work.  This is gross--Todd"
       << "\n----------------------------\n";
  throw ProblemSetupException(desc.str());  
} 
/*==========TESTING==========`*/


  cout_doing << "Doing problemSetup \t\t\t\tADIABATIC_TABLE" << endl;
  sharedState = in_state;
  d_matl = sharedState->parseAndLookupMaterial(params, "material");

  vector<int> m(1);
  m[0] = d_matl->getDWIndex();
  d_matl_set = new MaterialSet();
  d_matl_set->addAll(m);
  d_matl_set->addReference();

  //__________________________________
  //setup the table
  string tablename = "adiabatic";
  table = TableFactory::readTable(params, tablename);
  table->addIndependentVariable("mix. frac.");
  
  for (ProblemSpecP child = params->findBlock("tableValue"); child != 0;
       child = child->findNextBlock("tableValue")) {
    TableValue* tv = new TableValue;
    child->get(tv->name);
    tv->index = table->addDependentVariable(tv->name);
    string labelname = tablename+"-"+tv->name;
    tv->label = VarLabel::create(labelname, CCVariable<double>::getTypeDescription());
    tablevalues.push_back(tv);
  }
  
  d_temp_index          = table->addDependentVariable("Temp(K)");
  d_density_index       = table->addDependentVariable("density (kg/m3)");
  d_gamma_index         = table->addDependentVariable("gamma");
  d_cv_index            = table->addDependentVariable("heat capacity Cv(j/kg-K)");
  d_viscosity_index     = table->addDependentVariable("viscosity");
  d_initial_cv_index    = table->addDependentVariable("initial heat capacity Cv(j/kg-K)");
  d_initial_gamma_index = table->addDependentVariable("initial gamma");
  d_initial_temp_index  = table->addDependentVariable("initial temp(K)");
  table->setup();
  
  //__________________________________
  // - create Label names
  // - Let ICE know that this model computes the 
  //   thermoTransportProperties.
  // - register the scalar to be transported
  d_scalar = new Scalar();
  d_scalar->index = 0;
  d_scalar->name  = "f";
  
  const TypeDescription* td_CCdouble = CCVariable<double>::getTypeDescription();
  d_scalar->scalar_CCLabel =     VarLabel::create("scalar-f",       td_CCdouble);
  d_scalar->diffusionCoefLabel = VarLabel::create("scalar-diffCoef",td_CCdouble);
  d_modelComputesThermoTransportProps = true;
  
  setup->registerTransportedVariable(d_matl_set->getSubset(0),
                                     d_scalar->scalar_CCLabel, 0);
  setup->registerTransportedVariable(d_matl_set->getSubset(0),
                                     cumulativeEnergyReleased_CCLabel,
                                     cumulativeEnergyReleased_src_CCLabel);
  //__________________________________
  // Read in the constants for the scalar
   ProblemSpecP child = params->findBlock("scalar");
   if (!child){
     throw ProblemSetupException("AdiabaticTable: Couldn't find scalar tag");    
   }
   ProblemSpecP const_ps = child->findBlock("constants");
   if(!const_ps) {
     throw ProblemSetupException("AdiabaticTable: Couldn't find constants tag");
   }
    
   const_ps->get("diffusivity",  d_scalar->diff_coeff);

  //__________________________________
  //  Read in the geometry objects for the scalar
  for (ProblemSpecP geom_obj_ps = child->findBlock("geom_object");
    geom_obj_ps != 0;
    geom_obj_ps = geom_obj_ps->findNextBlock("geom_object") ) {
    vector<GeometryPiece*> pieces;
    GeometryPieceFactory::create(geom_obj_ps, pieces);

    GeometryPiece* mainpiece;
    if(pieces.size() == 0){
     throw ParameterNotFound("No piece specified in geom_object");
    } else if(pieces.size() > 1){
     mainpiece = scinew UnionGeometryPiece(pieces);
    } else {
     mainpiece = pieces[0];
    }

    d_scalar->regions.push_back(scinew Region(mainpiece, geom_obj_ps));
  }
  if(d_scalar->regions.size() == 0) {
    throw ProblemSetupException("Variable: scalar-f does not have any initial value regions");
  }

  //__________________________________
  //  Read in probe locations for the scalar field
  ProblemSpecP probe_ps = child->findBlock("probePoints");
  if (probe_ps) {
    probe_ps->require("probeSamplingFreq", d_probeFreq);
     
    Vector location = Vector(0,0,0);
    map<string,string> attr;                    
    for (ProblemSpecP prob_spec = probe_ps->findBlock("location"); prob_spec != 0; 
                      prob_spec = prob_spec->findNextBlock("location")) {
                      
      prob_spec->get(location);
      prob_spec->getAttributes(attr);
      string name = attr["name"];
      
      d_probePts.push_back(location);
      d_probePtsNames.push_back(name);
      d_usingProbePts = true;
    }
  } 
}
//______________________________________________________________________
//      S C H E D U L E   I N I T I A L I Z E
void AdiabaticTable::scheduleInitialize(SchedulerP& sched,
                                   const LevelP& level,
                                   const ModelInfo*)
{
  cout_doing << "ADIABATIC_TABLE::scheduleInitialize " << endl;
  Task* t = scinew Task("AdiabaticTable::initialize", this, &AdiabaticTable::initialize);

  t->modifies(lb->sp_vol_CCLabel);
  t->modifies(lb->rho_micro_CCLabel);
  t->modifies(lb->rho_CCLabel);
  t->modifies(lb->specific_heatLabel);
  t->modifies(lb->gammaLabel);
  t->modifies(lb->thermalCondLabel);
  t->modifies(lb->temp_CCLabel);
  t->modifies(lb->viscosityLabel);
  
  t->computes(d_scalar->scalar_CCLabel);
  t->computes(cumulativeEnergyReleased_CCLabel);
  t->computes(cumulativeEnergyReleased_src_CCLabel);
  
  sched->addTask(t, level->eachPatch(), d_matl_set);
}
//______________________________________________________________________
//       I N I T I A L I Z E
void AdiabaticTable::initialize(const ProcessorGroup*, 
                           const PatchSubset* patches,
                           const MaterialSubset*,
                           DataWarehouse*,
                           DataWarehouse* new_dw)
{
  cout_doing << "Doing Initialize \t\t\t\t\tADIABATIC_TABLE" << endl;
  for(int p=0;p<patches->size();p++){
    const Patch* patch = patches->get(p);
    int indx = d_matl->getDWIndex();
    
    CCVariable<double>  f, eReleased;
    CCVariable<double> cv, gamma, thermalCond, viscosity, rho_CC, sp_vol;
    CCVariable<double> rho_micro, temp;
    new_dw->allocateAndPut(f, d_scalar->scalar_CCLabel, indx, patch);
    new_dw->allocateAndPut(eReleased, cumulativeEnergyReleased_CCLabel, indx, patch);
    new_dw->getModifiable(cv,          lb->specific_heatLabel,indx,patch);
    new_dw->getModifiable(gamma,       lb->gammaLabel,        indx,patch);
    new_dw->getModifiable(thermalCond, lb->thermalCondLabel,  indx,patch);
    new_dw->getModifiable(viscosity,   lb->viscosityLabel,    indx,patch);
    new_dw->getModifiable(rho_CC,      lb->rho_CCLabel,       indx,patch);
    new_dw->getModifiable(sp_vol,      lb->sp_vol_CCLabel,    indx,patch);
    new_dw->getModifiable(rho_micro,   lb->rho_micro_CCLabel, indx,patch);
    new_dw->getModifiable(temp, lb->temp_CCLabel, indx, patch);
    //__________________________________
    //  initialize the scalar field in a region
    f.initialize(0);

    for(vector<Region*>::iterator iter = d_scalar->regions.begin();
                                  iter != d_scalar->regions.end(); iter++){
      Region* region = *iter;
      for(CellIterator iter = patch->getExtraCellIterator();
          !iter.done(); iter++){
        IntVector c = *iter;
        Point p = patch->cellPosition(c);            
        if(region->piece->inside(p)) {
          f[c] = region->initialScalar;
        }
      } // Over cells
    } // regions
    
    setBC(f,"scalar-f", patch, sharedState,indx, new_dw); 

    //__________________________________
    // initialize other properties
    vector<constCCVariable<double> > ind_vars;
    ind_vars.push_back(f);
    
    CellIterator extraCellIterator = patch->getExtraCellIterator();
    table->interpolate(d_density_index,   rho_CC,   extraCellIterator,ind_vars);
    table->interpolate(d_gamma_index,     gamma,    extraCellIterator,ind_vars);
    table->interpolate(d_cv_index,        cv,       extraCellIterator,ind_vars);
    table->interpolate(d_viscosity_index, viscosity,extraCellIterator,ind_vars);
    table->interpolate(d_temp_index, temp, extraCellIterator, ind_vars);
    CCVariable<double> initialTemp, initial_cv, initial_gamma;
    new_dw->allocateTemporary(initial_cv, patch);
    new_dw->allocateTemporary(initial_gamma, patch);
    new_dw->allocateTemporary(initialTemp, patch);
    table->interpolate(d_initial_cv_index, initial_cv, extraCellIterator, ind_vars);
    table->interpolate(d_initial_gamma_index, initial_gamma, extraCellIterator, ind_vars);
    table->interpolate(d_initial_temp_index, initialTemp, extraCellIterator, ind_vars);
    
    Vector dx = patch->dCell();
    double volume = dx.x()*dx.y()*dx.z();
    for(CellIterator iter = patch->getExtraCellIterator();!iter.done();iter++){
      const IntVector& c = *iter;
      rho_micro[c] = rho_CC[c];
      sp_vol[c] = 1./rho_CC[c];
      thermalCond[c] = 0;
      double mass      = rho_CC[c]*volume;
      double cp        = gamma[c] * cv[c];
      double icp = initial_gamma[c] * initial_cv[c];
      eReleased[c] = temp[c] * cp * mass - initialTemp[c] * icp * mass;
    }
    setBC(f,"cumulativeEnergyReleased", patch, sharedState,indx, new_dw); 

    //__________________________________
    //  Dump out a header for the probe point files
    oldProbeDumpTime = 0;
    if (d_usingProbePts){
      FILE *fp;
      IntVector cell;
      string udaDir = d_dataArchiver->getOutputLocation();
      
        for (unsigned int i =0 ; i < d_probePts.size(); i++) {
          if(patch->findCell(Point(d_probePts[i]),cell) ) {
            string filename=udaDir + "/" + d_probePtsNames[i].c_str() + ".dat";
            fp = fopen(filename.c_str(), "a");
            fprintf(fp, "%%Time Scalar Field at [%e, %e, %e], at cell [%i, %i, %i]\n", 
                    d_probePts[i].x(),d_probePts[i].y(), d_probePts[i].x(),
                    cell.x(), cell.y(), cell.z() );
            fclose(fp);
        }
      }  // loop over probes
    }  // if using probe points
  }  // patches
}

//______________________________________________________________________     
void AdiabaticTable::scheduleModifyThermoTransportProperties(SchedulerP& sched,
                                                   const LevelP& level,
                                                   const MaterialSet* /*ice_matls*/)
{
  cout_doing << "ADIABATIC_TABLE::scheduleModifyThermoTransportProperties" << endl;

  Task* t = scinew Task("AdiabaticTable::modifyThermoTransportProperties", 
                   this,&AdiabaticTable::modifyThermoTransportProperties);
                   
  t->requires(Task::OldDW, d_scalar->scalar_CCLabel, Ghost::None,0);  
  t->modifies(lb->specific_heatLabel);
  t->modifies(lb->gammaLabel);
  t->modifies(lb->thermalCondLabel);
  t->modifies(lb->viscosityLabel);
  //t->computes(d_scalar->diffusionCoefLabel);
  sched->addTask(t, level->eachPatch(), d_matl_set);
}
//______________________________________________________________________
// Purpose:  Compute the thermo and transport properties.  This gets
//           called at the top of the timestep.
// TO DO:   FIGURE OUT A WAY TO ONLY COMPUTE CV ONCE
void AdiabaticTable::modifyThermoTransportProperties(const ProcessorGroup*, 
                                                const PatchSubset* patches,
                                                const MaterialSubset*,
                                                DataWarehouse* old_dw,
                                                DataWarehouse* new_dw)
{ 
  for(int p=0;p<patches->size();p++){
    const Patch* patch = patches->get(p);
    cout_doing << "Doing modifyThermoTransportProperties on patch "<<patch->getID()
               << "\t ADIABATIC_TABLE" << endl;
   
    int indx = d_matl->getDWIndex();
    CCVariable<double> diffusionCoeff, gamma, cv, thermalCond, viscosity;
    constCCVariable<double> f_old;
    
    //new_dw->allocateAndPut(diffusionCoeff, 
    //d_scalar->diffusionCoefLabel,indx, patch);  
    
    new_dw->getModifiable(gamma,       lb->gammaLabel,        indx,patch);
    new_dw->getModifiable(cv,          lb->specific_heatLabel,indx,patch);
    new_dw->getModifiable(thermalCond, lb->thermalCondLabel,  indx,patch);
    new_dw->getModifiable(viscosity,   lb->viscosityLabel,    indx,patch);
    
    old_dw->get(f_old,  d_scalar->scalar_CCLabel,  indx, patch, Ghost::None,0);
    
    vector<constCCVariable<double> > ind_vars;
    ind_vars.push_back(f_old);
    
    CellIterator extraCellIterator = patch->getExtraCellIterator();
    table->interpolate(d_gamma_index,     gamma,    extraCellIterator,ind_vars);
    table->interpolate(d_cv_index,        cv,       extraCellIterator,ind_vars);
    table->interpolate(d_viscosity_index, viscosity,extraCellIterator,ind_vars);
    
    for(CellIterator iter = patch->getExtraCellIterator();!iter.done();iter++){
      const IntVector& c = *iter;
      thermalCond[c] = 0;
    }
    //diffusionCoeff.initialize(d_scalar->diff_coeff);
  }
} 

//______________________________________________________________________
// Purpose:  Compute the specific heat at time.  This gets called immediately
//           after (f) is advected
//  TO DO:  FIGURE OUT A WAY TO ONLY COMPUTE CV ONCE
void AdiabaticTable::computeSpecificHeat(CCVariable<double>& cv_new,
                                    const Patch* patch,
                                    DataWarehouse* new_dw,
                                    const int indx)
{ 
  cout_doing << "Doing computeSpecificHeat on patch "<<patch->getID()
             << "\t ADIABATIC_TABLE" << endl;

  int test_indx = d_matl->getDWIndex();
  //__________________________________
  //  Compute cv for only one matl.
  if (test_indx != indx)
    return;

  constCCVariable<double> f;
  new_dw->get(f,  d_scalar->scalar_CCLabel,  indx, patch, Ghost::None,0);
  
  // interpolate cv
  vector<constCCVariable<double> > ind_vars;
  ind_vars.push_back(f);
  table->interpolate(d_cv_index, cv_new, patch->getExtraCellIterator(),
                     ind_vars);
} 

//______________________________________________________________________
void AdiabaticTable::scheduleComputeModelSources(SchedulerP& sched,
                                                 const LevelP& level,
                                                 const ModelInfo* mi)
{
  cout_doing << "ADIABATIC_TABLE::scheduleComputeModelSources " << endl;
  Task* t = scinew Task("AdiabaticTable::computeModelSources", 
                   this,&AdiabaticTable::computeModelSources, mi);
                    
  Ghost::GhostType  gn = Ghost::None;  
  Ghost::GhostType  gac = Ghost::AroundCells;
  //t->requires(Task::OldDW, mi->delT_Label); turn off for AMR 
 
  //t->requires(Task::NewDW, d_scalar->diffusionCoefLabel, gac,1);
  t->requires(Task::OldDW, d_scalar->scalar_CCLabel,     gac,1); 
  t->requires(Task::OldDW, mi->density_CCLabel,          gn);
  t->requires(Task::OldDW, mi->temperature_CCLabel,      gn);
  t->requires(Task::OldDW, cumulativeEnergyReleased_CCLabel, gn);

  t->requires(Task::NewDW, lb->specific_heatLabel,       gn);
  t->requires(Task::NewDW, lb->gammaLabel,               gn);

  t->modifies(mi->energy_source_CCLabel);
  t->modifies(cumulativeEnergyReleased_src_CCLabel);

  // Interpolated table values
  for(int i=0;i<(int)tablevalues.size();i++){
    TableValue* tv = tablevalues[i];
    t->computes(tv->label);
  }
  sched->addTask(t, level->eachPatch(), d_matl_set);
}

//______________________________________________________________________
void AdiabaticTable::computeModelSources(const ProcessorGroup*, 
                                         const PatchSubset* patches,
                                         const MaterialSubset* matls,
                                         DataWarehouse* old_dw,
                                         DataWarehouse* new_dw,
                                         const ModelInfo* mi)
{
  delt_vartype delT;
  const Level* level = getLevel(patches);
  old_dw->get(delT, mi->delT_Label, level);
  Ghost::GhostType gn = Ghost::None;
    
  for(int p=0;p<patches->size();p++){
    const Patch* patch = patches->get(p);
    cout_doing << "Doing momentumAndEnergyExch... on patch "<<patch->getID()
               << "\t\tADIABATIC_TABLE" << endl;

    for(int m=0;m<matls->size();m++){
      int matl = matls->get(m);

      // Get mixture fraction, and initialize source to zero
      constCCVariable<double> f_old, rho_CC, oldTemp, cv, gamma;
      constCCVariable<double> eReleased;
      CCVariable<double> energySource, sp_vol_ref, flameTemp;
      CCVariable<double> eReleased_src;

      old_dw->get(f_old,    d_scalar->scalar_CCLabel,   matl, patch, gn, 0);
      old_dw->get(rho_CC,   mi->density_CCLabel,        matl, patch, gn, 0);
      old_dw->get(oldTemp,  mi->temperature_CCLabel,    matl, patch, gn, 0);
      old_dw->get(eReleased, cumulativeEnergyReleased_CCLabel,
                                                        matl, patch, gn, 0);
      new_dw->get(cv,       lb->specific_heatLabel,     matl, patch, gn, 0);
      new_dw->get(gamma,    lb->gammaLabel,             matl, patch, gn, 0);
      new_dw->getModifiable(energySource,   mi->energy_source_CCLabel,  
                                                        matl, patch);
      new_dw->getModifiable(eReleased_src,  cumulativeEnergyReleased_src_CCLabel,
                            matl, patch);
      new_dw->allocateTemporary(flameTemp, patch); 
      new_dw->allocateTemporary(sp_vol_ref, patch);
      
      //__________________________________
      //  grab values from the tables
      vector<constCCVariable<double> > ind_vars;
      ind_vars.push_back(f_old);
      CellIterator iter = patch->getCellIterator();
      table->interpolate(d_temp_index,  flameTemp,  iter, ind_vars);
      CCVariable<double> initialTemp;
      new_dw->allocateTemporary(initialTemp, patch); 
      table->interpolate(d_initial_temp_index, initialTemp, iter, ind_vars);
      CCVariable<double> initial_cv;
      new_dw->allocateTemporary(initial_cv, patch); 
      table->interpolate(d_initial_cv_index, initial_cv, iter, ind_vars);
      CCVariable<double> initial_gamma;
      new_dw->allocateTemporary(initial_gamma, patch); 
      table->interpolate(d_initial_gamma_index, initial_gamma, iter, ind_vars);

      Vector dx = patch->dCell();
      double volume = dx.x()*dx.y()*dx.z();
      
      double maxTemp = 0;
      double maxIncrease = 0;    //debugging
      double maxDecrease = 0;
      double totalEnergy = 0;
      double maxFlameTemp=0;
      double esum = 0;
      double fsum = 0;
      int ncells = 0;
      double cpsum = 0;
      double masssum=0;
      bool flag=true;
      
      for(CellIterator iter = patch->getCellIterator(); !iter.done(); iter++){
        IntVector c = *iter;

        double mass      = rho_CC[c]*volume;
        // cp might need to change to be interpolated from the table
        // when we make the cp in the dw correct
        double cp        = gamma[c] * cv[c];
        double energyNew = flameTemp[c] * cp * mass;
        // double icp = initial_cp[c];
        double icp = initial_gamma[c] * initial_cv[c];
        double energyOrig = initialTemp[c] * icp * mass;
        double erelease = (energyNew-energyOrig) - eReleased[c];

        // Add energy released to the cumulative total and hand it to ICE
        // as a source
        eReleased_src[c] += erelease;
        energySource[c] += erelease;
#if 0
        // Diagnostics
        if(erelease != 0){
          cerr.precision(20);
          cerr << "\nerelease=" << erelease << '\n';
          cerr << "c=" << c << '\n';
          cerr << "flameTemp=" << flameTemp[c] << '\n';
          cerr << "cp=" << cp << '\n';
          cerr << "initialTemp = " << initialTemp[c] << '\n';
          cerr << "initial_cp=" << icp << '\n';
          cerr << "dtemp=" << flameTemp[c]-initialTemp[c] << '\n';
          cerr << "dcp=" << cp-icp << '\n';
          cerr << "mass=" << mass << '\n';
          cerr << "ediff=" << energyNew-energyOrig << '\n';
          cerr << "heatLoss=" << heatLoss[c] << '\n';
          flag=false;
        }
#endif


        //__________________________________
        // debugging
        totalEnergy += erelease;
        fsum += f_old[c] * mass;
        esum += oldTemp[c]*cp*mass;
        cpsum += cp;
        masssum += mass;
        ncells++;
        double newTemp = oldTemp[c] + erelease/(cp*mass);
        if(newTemp > maxTemp)
          maxTemp = newTemp;
        if(flameTemp[c] > maxFlameTemp)
          maxFlameTemp = flameTemp[c];
        double dtemp = newTemp-oldTemp[c];
        if(dtemp > maxIncrease)
          maxIncrease = dtemp;
        if(dtemp < maxDecrease)
          maxDecrease = dtemp;
      }
#if 0
      if(flag)
        cerr << "PASSED!!!!!!!!!!!!!!!!!!!!!!!!\n";
#endif
      cerr << "MaxTemp = " << maxTemp << ", maxFlameTemp=" << maxFlameTemp << ", maxIncrease=" << maxIncrease << ", maxDecrease=" << maxDecrease << ", totalEnergy=" << totalEnergy << '\n';
      double cp = cpsum/ncells;
      double mass = masssum/ncells;
      double e = esum/ncells;
      double atemp = e/(mass*cp);
      vector<double> tmp(1);
      tmp[0]=fsum/masssum;
      cerr << "AverageTemp=" << atemp << ", AverageF=" << fsum/masssum << ", targetTemp=" << table->interpolate(d_temp_index, tmp) << '\n';
#if 0
      //__________________________________
      //  Tack on diffusion
      double diff_coeff_test = d_scalar->diff_coeff;
      if(diff_coeff_test != 0.0){ 
        
        bool use_vol_frac = false; // don't include vol_frac in diffusion calc.
        constCCVariable<double> placeHolder;
        
        scalarDiffusionOperator(new_dw, patch, use_vol_frac,
                                placeHolder, placeHolder,  f_old,
                                f_src, diff_coeff, delT);
      }
#endif

      //__________________________________
      //  dump out the probe points
      if (d_usingProbePts ) {
        double time = d_dataArchiver->getCurrentTime();
        double nextDumpTime = oldProbeDumpTime + 1.0/d_probeFreq;
        
        if (time >= nextDumpTime){        // is it time to dump the points
          FILE *fp;
          string udaDir = d_dataArchiver->getOutputLocation();
          IntVector cell_indx;
          
          // loop through all the points and dump if that patch contains them
          for (unsigned int i =0 ; i < d_probePts.size(); i++) {
            if(patch->findCell(Point(d_probePts[i]),cell_indx) ) {
              string filename=udaDir + "/" + d_probePtsNames[i].c_str() + ".dat";
              fp = fopen(filename.c_str(), "a");
              fprintf(fp, "%16.15E  %16.15E\n",time, f_old[cell_indx]);
              fclose(fp);
            }
          }
          oldProbeDumpTime = time;
        }  // time to dump
      } // if(probePts)  
      
      // Compute miscellaneous table quantities.  Users can request
      // arbitrary quanitities to be interplated by adding an entry
      // of the form: <tableValue>N2</tableValue> to the model section.
      // This is useful for computing derived quantities such as species
      // concentrations.
      for(int i=0;i<(int)tablevalues.size();i++){
        TableValue* tv = tablevalues[i];
        cerr << "interpolating " << tv->name << '\n';
        CCVariable<double> value;
        new_dw->allocateAndPut(value, tv->label, matl, patch);
        CellIterator iter = patch->getCellIterator();
        table->interpolate(tv->index, value, iter, ind_vars);
      }
    }
  }
}

//__________________________________      
void AdiabaticTable::scheduleComputeStableTimestep(SchedulerP&,
                                      const LevelP&,
                                      const ModelInfo*)
{
  // None necessary...
}
