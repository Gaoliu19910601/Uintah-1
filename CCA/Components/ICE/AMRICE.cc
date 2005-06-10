
#include <Packages/Uintah/CCA/Components/ICE/AMRICE.h>
#include <Packages/Uintah/CCA/Components/ICE/ICEMaterial.h>
#include <Packages/Uintah/CCA/Components/Regridder/PerPatchVars.h>
#include <Packages/Uintah/CCA/Ports/SolverInterface.h>
#include <Packages/Uintah/CCA/Ports/Scheduler.h>
#include <Packages/Uintah/Core/Exceptions/ProblemSetupException.h>
#include <Packages/Uintah/Core/Exceptions/InvalidValue.h>
#include <Packages/Uintah/Core/Grid/Variables/CCVariable.h>
#include <Packages/Uintah/Core/Grid/Variables/CellIterator.h>
#include <Packages/Uintah/Core/Grid/Variables/PerPatch.h>
#include <Packages/Uintah/Core/Grid/Level.h>
#include <Packages/Uintah/Core/Grid/SimulationState.h>
#include <Packages/Uintah/Core/Grid/Task.h>
#include <Packages/Uintah/Core/Math/FastMatrix.h>
#include <Core/Math/MiscMath.h>
#include <Core/Util/DebugStream.h>
#include <iomanip>

#define SPEW
#undef SPEW

using namespace Uintah;
using namespace std;
static DebugStream cout_doing("AMRICE_DOING_COUT", false);
static DebugStream cout_dbg("AMRICE_DBG", false);
//setenv SCI_DEBUG AMR:+ you can see the new grid it creates


AMRICE::AMRICE(const ProcessorGroup* myworld)
  : ICE(myworld, true)
{
}

AMRICE::~AMRICE()
{
}
//___________________________________________________________________
void AMRICE::problemSetup(const ProblemSpecP& params, GridP& grid,
                            SimulationStateP& sharedState)
{
  cout_doing << "Doing problemSetup  \t\t\t AMRICE" << '\n';
  ICE::problemSetup(params, grid, sharedState);
  ProblemSpecP cfd_ps = params->findBlock("CFD");
  ProblemSpecP ice_ps = cfd_ps->findBlock("ICE");
  ProblemSpecP amr_ps = ice_ps->findBlock("AMR");
  if(!amr_ps){
    string warn;
    warn ="\n INPUT FILE ERROR:\n <AMR>  block not found inside of <ICE> block \n";
    throw ProblemSetupException(warn);
    
  }
  ProblemSpecP refine_ps = amr_ps->findBlock("Refinement_Criteria_Thresholds");
  if(!refine_ps ){
    string warn;
    warn ="\n INPUT FILE ERROR:\n <Refinement_Criteria_Thresholds> "
         " block not found inside of <ICE> block \n";
    throw ProblemSetupException(warn);
  }
  
  amr_ps->getWithDefault( "regridderTest", d_regridderTest,     false);
  amr_ps->getWithDefault( "do_Refluxing",  d_doRefluxing,       true);
  refine_ps->getWithDefault("Density",     d_rho_threshold,     1e100);
  refine_ps->getWithDefault("Temperature", d_temp_threshold,    1e100);
  refine_ps->getWithDefault("Pressure",    d_press_threshold,   1e100);
  refine_ps->getWithDefault("VolumeFrac",  d_vol_frac_threshold,1e100);
  refine_ps->getWithDefault("Velocity",    d_vel_threshold,     1e100);
}
//___________________________________________________________________              
void AMRICE::scheduleInitialize(const LevelP& level,
                                  SchedulerP& sched)
{
  cout_doing << "AMRICE::scheduleInitialize \t\tL-"<<level->getIndex()<< '\n';
  ICE::scheduleInitialize(level, sched);
}
//___________________________________________________________________
void AMRICE::initialize(const ProcessorGroup*,
                           const PatchSubset*, const MaterialSubset*,
                           DataWarehouse*, DataWarehouse*)
{
}
/*___________________________________________________________________
 Function~  AMRICE::addRefineDependencies--
 Purpose:  
_____________________________________________________________________*/
void AMRICE::addRefineDependencies(Task* task, 
                                   const VarLabel* var,
                                   int step, 
                                   int nsteps)
{
  cout_dbg << "\t addRefineDependencies (" << var->getName()
           << ") \t step " << step << " nsteps " << nsteps;
  ASSERTRANGE(step, 0, nsteps+1);
  Ghost::GhostType  gac = Ghost::AroundCells;
  if(step != nsteps) {
    cout_dbg << " requires from CoarseOldDW ";
    task->requires(Task::CoarseOldDW, var,
                 0, Task::CoarseLevel, 0, Task::NormalDomain, gac, 1);
  }
  if(step != 0) {
    cout_dbg << " requires from CoarseNewDW ";
    task->requires(Task::CoarseNewDW, var,
                 0, Task::CoarseLevel, 0, Task::NormalDomain, gac, 1);
  }
  cout_dbg <<""<<endl;
}
/*___________________________________________________________________
 Function~  AMRICE::scheduleRefineInterface--
 Purpose:  
_____________________________________________________________________*/
void AMRICE::scheduleRefineInterface(const LevelP& fineLevel,
                                     SchedulerP& sched,
                                     int step, 
                                     int nsteps)
{
  if(fineLevel->getIndex() > 0){
    cout_doing << "AMRICE::scheduleRefineInterface \t\tL-" 
               << fineLevel->getIndex() << " step "<< step <<'\n';
               
    double subCycleProgress = double(step)/double(nsteps);
    
    Task* task = scinew Task("AMRICE::refineCoarseFineInterface", 
                       this, &AMRICE::refineCoarseFineInterface, 
                       subCycleProgress);
    
    addRefineDependencies(task, lb->press_CCLabel,    step, nsteps);
    addRefineDependencies(task, lb->rho_CCLabel,      step, nsteps);
    addRefineDependencies(task, lb->sp_vol_CCLabel,   step, nsteps);
    addRefineDependencies(task, lb->temp_CCLabel,     step, nsteps);
    addRefineDependencies(task, lb->vel_CCLabel,      step, nsteps);
    
    //__________________________________
    // Model Variables.
    if(d_modelSetup && d_modelSetup->tvars.size() > 0){
      vector<TransportedVariable*>::iterator iter;

      for(iter = d_modelSetup->tvars.begin();
         iter != d_modelSetup->tvars.end(); iter++){
        TransportedVariable* tvar = *iter;
        addRefineDependencies(task, tvar->var, step, nsteps);
      }
    }
    
    const MaterialSet* ice_matls = d_sharedState->allICEMaterials();
    sched->addTask(task, fineLevel->eachPatch(), ice_matls);
  }
}
/*______________________________________________________________________
 Function~  AMRICE::refineCoarseFineInterface
 Purpose~   
______________________________________________________________________*/
void AMRICE::refineCoarseFineInterface(const ProcessorGroup*,
                                       const PatchSubset* patches,
                                       const MaterialSubset*,
                                       DataWarehouse* fine_old_dw,
                                       DataWarehouse* fine_new_dw,
                                       double subCycleProgress)
{
  const Level* level = getLevel(patches);
  if(level->getIndex() > 0){     
    cout_doing << "Doing refineCoarseFineInterface"<< "\t\t\t\t AMRICE L-" 
               << level->getIndex() << " step " << subCycleProgress<<endl;
    int  numMatls = d_sharedState->getNumICEMatls();
    Ghost::GhostType  gac = Ghost::AroundCells;
    Ghost::GhostType  gn = Ghost::None;
    bool dbg_onOff = cout_dbg.active();      // is cout_dbg switch on or off
      
    for(int p=0;p<patches->size();p++){
      const Patch* patch = patches->get(p);
      
      for (int m = 0; m < numMatls; m++) {
        ICEMaterial* matl = d_sharedState->getICEMaterial(m);
        int indx = matl->getDWIndex();    
        constCCVariable<double> press_CC, rho_CC, sp_vol_CC, temp_CC;
        constCCVariable<Vector> vel_CC;

        // NEVER use gac, 1 (ask Bryan)
        fine_old_dw->get(press_CC, lb->press_CCLabel,  indx,patch, gn,0);
        fine_old_dw->get(rho_CC,   lb->rho_CCLabel,    indx,patch, gn,0);
        fine_old_dw->get(sp_vol_CC,lb->sp_vol_CCLabel, indx,patch, gn,0);
        fine_old_dw->get(temp_CC,  lb->temp_CCLabel,   indx,patch, gn,0);
        fine_old_dw->get(vel_CC,   lb->vel_CCLabel,    indx,patch, gn,0);

        //__________________________________
        //  Print Data 
        if(switchDebug_AMR_refineInterface){
          ostringstream desc;     
          desc << "TOP_refineInterface_Mat_" << indx << "_patch_"
               << patch->getID()<< " step " << subCycleProgress;
          printData(indx, patch,   1, desc.str(), "press_CC",    press_CC);
          printData(indx, patch,   1, desc.str(), "rho_CC",      rho_CC);
          printData(indx, patch,   1, desc.str(), "sp_vol_CC",   sp_vol_CC);
          printData(indx, patch,   1, desc.str(), "Temp_CC",     temp_CC);
          printVector(indx, patch, 1, desc.str(), "vel_CC", 0,   vel_CC);
        }

        refineCoarseFineBoundaries(patch, press_CC.castOffConst(), fine_new_dw, 
                                   lb->press_CCLabel,  indx,subCycleProgress);

        refineCoarseFineBoundaries(patch, rho_CC.castOffConst(),   fine_new_dw, 
                                   lb->rho_CCLabel,    indx,subCycleProgress);

        refineCoarseFineBoundaries(patch, sp_vol_CC.castOffConst(),fine_new_dw,
                                   lb->sp_vol_CCLabel, indx,subCycleProgress);

        refineCoarseFineBoundaries(patch, temp_CC.castOffConst(),  fine_new_dw,
                                   lb->temp_CCLabel,   indx,subCycleProgress);

        refineCoarseFineBoundaries(patch, vel_CC.castOffConst(),   fine_new_dw,
                                   lb->vel_CCLabel,    indx,subCycleProgress);
       //__________________________________
       //    Model Variables                     
       if(d_modelSetup && d_modelSetup->tvars.size() > 0){
         vector<TransportedVariable*>::iterator t_iter;
          for( t_iter  = d_modelSetup->tvars.begin();
               t_iter != d_modelSetup->tvars.end(); t_iter++){
            TransportedVariable* tvar = *t_iter;

            if(tvar->matls->contains(indx)){
              constCCVariable<double> q_CC;
              fine_old_dw->get(q_CC, tvar->var, indx, patch, gac, 1);
              
              if(switchDebug_AMR_refineInterface){ 
                string name = tvar->var->getName();
                printData(indx, patch, 1, "TOP_refineInterface", name, q_CC);
              }              
              
              refineCoarseFineBoundaries(patch, q_CC.castOffConst(),fine_new_dw,
                                          tvar->var,    indx,subCycleProgress);
              
              if(switchDebug_AMR_refineInterface){ 
                string name = tvar->var->getName();
                printData(indx, patch, 1, "BOT_refineInterface", name, q_CC);
              }
            }
          }
        }                                     
                            

        //__________________________________
        //  Print Data 
        if(switchDebug_AMR_refineInterface){
          ostringstream desc;    
          desc << "BOT_refineInterface_Mat_" << indx << "_patch_"
               << patch->getID()<< " step " << subCycleProgress;
          printData(indx, patch,   1, desc.str(), "press_CC",  press_CC);
          printData(indx, patch,   1, desc.str(), "rho_CC",    rho_CC);
          printData(indx, patch,   1, desc.str(), "sp_vol_CC", sp_vol_CC);
          printData(indx, patch,   1, desc.str(), "Temp_CC",   temp_CC);
          printVector(indx, patch, 1, desc.str(), "vel_CC", 0, vel_CC);
        }
      }
    }
    cout_dbg.setActive(dbg_onOff);  // reset on/off switch for cout_dbg
  }             
}


/*___________________________________________________________________
 Function~  AMRICE::interpolationTest--
_____________________________________________________________________*/
template<class T>
  void interpolationTest(const Level* fineLevel,
                         const IntVector& f_cell,
                         int& ncell,
                         T& error,
                         T& q_FineLevel)
{
                      
  Vector f_dx = fineLevel->dCell();
  double X = ((f_cell.x() * f_dx.x()) + f_dx.x()/2);
  double Y = ((f_cell.y() * f_dx.y()) + f_dx.y()/2);
  double Z = ((f_cell.z() * f_dx.z()) + f_dx.z()/2);

  //T exact(5.0);
  //T exact( X );
  //T exact( Y );
  //T exact( Z );
  T exact( X * Y* Z  );
  //T exact( X * X * Y * Y * Z * Z);
  //T exact( X * X * X * Y* Y * Y  * Z * Z *Z );

  error = error + (q_FineLevel - exact) * (q_FineLevel - exact);
  ncell += 1;

#if 0  
  cout  << "f_cell \t" << f_cell 
        << " q_FineLevel[f_cell] " << q_FineLevel
        <<  " error " << error <<  endl;
#endif
}

/*___________________________________________________________________
 Function~  AMRICE::linearInterpolation--
 
 X-Y PLANE 1

           |       x   |
           |     |---| |
___________|___________|_______________
  |  |  |  |  |  |  |  |  |
__|__|__|__|__|__|__|__|__|
  |  |  |  |  |  |  |  |  |
__|__*__|__|__|__o__|x1|__|  ---o         Q_x1 = (1-x)Q  + (x)Q(i+1)
  |  |  |  |  |  |  |  |  |   |
__|__|__|__|__|__|__|__|__|   |  y
  |  |  |  |  |  |  |  |  |   |
__|__|__|__|__|__|__|__|__|___|________
  |  |  |  |  |  |  |+ |  |  ---
__|__|__|__|__|__|__|__|__|
           |           |
           |           |
     *     |     o   x2        o          Q_x2 = (1-x)Q(j-1) + (x)(Q(i+1,j-1))
           |           |
           |           |
___________|___________|_______________

* coarse cell centers                   
o cells used for interpolation          
+ fine cell cell center

 Q_fc_plane_1 = (1-y)Q_x1 + (y)*Q_x2
              = (1-y)(1-x)Q + (1-y)(x)Q(i+1) + (1-x)(y)Q(j-1) + (x)(y)Q(i+1)(j-1)
              = (w0)Q + (w1)Q(i+1) + (w2)Q(j-1) + (w3)Q(i+1)(j-1)               

Q_fc_plane_2 is identical to Q_fc_plane_1 with except for a z offset.

Q_FC =(1-z)Q_fc_plane_1 + (z) * Q_fc_plane2
_____________________________________________________________________*/
template<class T>
  void linearInterpolation(constCCVariable<T>& q_CL,// course level
                           const Level* coarseLevel,
                           const Level* fineLevel,
                           const IntVector& refineRatio,
                           const IntVector& fl,
                           const IntVector& fh,
                           CCVariable<T>& q_FineLevel)
{
  //int ncell = 0;  // needed by interpolation test
  //T error(0);
  
  Vector c_dx = coarseLevel->dCell();
  Vector inv_c_dx = Vector(1.0)/c_dx;
  
  for(CellIterator iter(fl,fh); !iter.done(); iter++){
    IntVector f_cell = *iter;
    IntVector c_cell = fineLevel->mapCellToCoarser(f_cell);
    //__________________________________
    // Offset for coarse level surrounding cells:
    //  -find the distance between the center of the patch and fine level cell centers
    //   This tells which direction the offset should be  
    Point coarse_cell_pos = coarseLevel->getCellPosition(c_cell);
    Point fine_cell_pos   = fineLevel->getCellPosition(f_cell);
    Vector dist = (fine_cell_pos.asVector() - coarse_cell_pos.asVector()) * inv_c_dx;
    dist = Abs(dist);
    Vector dir = (fine_cell_pos.asVector() - coarse_cell_pos.asVector()); 
  
    // determine the direction to the surrounding interpolation cells
    int i = Sign(dir.x());
    int j = Sign(dir.y());
    int k = Sign(dir.z());
    
    i *=RoundUp(dist.x());  // if dist.x,y,z() = 0 then set (i,j,k) = 0
    j *=RoundUp(dist.y());  // Only need surrounding coarse cell data if dist != 0
    k *=RoundUp(dist.z());  // This is especially true for 1D and 2D problems
    
    #if 0
    if(f_cell == IntVector(30,39,0) ||true){
    cout << " c_cell " << c_cell << " f_cell " << f_cell << " offset ["<<i<<","<<j<<","<<k<<"]  "
         << " dist " << dist << " dir "<< dir
         << " f_cell_pos " << fine_cell_pos.asVector()<< " c_cell_pos " << coarse_cell_pos.asVector() << endl;
    }
    #endif

    //__________________________________
    //  Find the weights      
    double w0 = (1. - dist.x()) * (1. - dist.y());
    double w1 = dist.x() * (1. - dist.y());
    double w2 = dist.y() * (1. - dist.x());
    double w3 = dist.x() * dist.y(); 
      
    T q_XY_Plane_1   // X-Y plane closest to the fine level cell 
        = w0 * q_CL[c_cell] 
        + w1 * q_CL[c_cell + IntVector( i, 0, 0)] 
        + w2 * q_CL[c_cell + IntVector( 0, j, 0)]
        + w3 * q_CL[c_cell + IntVector( i, j, 0)];
                   
    T q_XY_Plane_2   // X-Y plane furthest from the fine level cell
        = w0 * q_CL[c_cell + IntVector( 0, 0, k)] 
        + w1 * q_CL[c_cell + IntVector( i, 0, k)]  
        + w2 * q_CL[c_cell + IntVector( 0, j, k)]  
        + w3 * q_CL[c_cell + IntVector( i, j, k)]; 

    // interpolate the two X-Y planes in the k direction
    q_FineLevel[f_cell] = (1.0 - dist.z()) * q_XY_Plane_1 
                        + dist.z() * q_XY_Plane_2;
    
    //interpolationTest(fineLevel,f_cell, ncell, error, q_FineLevel[f_cell]);  
    if(f_cell == IntVector(30,39,0)){  
      cout << " plane 1 " << q_XY_Plane_1 << " plane2 " << q_XY_Plane_2 << endl;
    }
  }
  
  //interpolation test
  //error = error/(double)ncell;
  //cout << "Linear interpolation    error^2/ncell " << error << " Ncells " << ncell<< endl;
}

/*___________________________________________________________________
 Function~  AMRICE::QuadraticInterpolation--
 X-Y PLANE
                   x
           |     |----||
  |  |  |  |  |  |  |  |  |
__|__|__|__|__|__|__|__|__|
  |  |  |  |  |  |  |  |  |
__|__o__|__|__|__o__|_0|__|      o       Q_0 = (w0_x)Q(i-1)  + (w1_x)Q(i) + (w2_x)Q(i+1)
  |  |  |  |  |  |  |  |  |               (j+1)
__|__|__|__|__|__|__|__|__|
  |  |  |  |  |  |  |  |  |
__|__|__|__|__|__|__|__|__|____________
  |  |  |  |  |  |  |  |  |
__|__|__|__|__|__|__|__|__|
  |  |  |  |  |(i,j)|  |  |
__|__o__|__|__|__o__|_1|__|  ---o         Q_1 = (w0_x)Q(i-1)  + (w1_x)Q(i) + (w2_x)Q(i+1)
  |  |  |  |  |  |  |  |  |   |             (j)
__|__|__|__|__|__|__|__|__|   |  y
  |  |  |  |  |  |  | +|  |  ---
__|__|__|__|__|__|__|__|__|____________
  |  |  |  |  |  |  |  |  |
__|__|__|__|__|__|__|__|__|
           |           |
           |           |
     o     |     o    2|       o          Q_2 = (w0_x)Q(i-1)  + (w1_x)Q(i) + (w2_x)Q(i+1)
           |           |                    (j+1)
           |           |
___________|___________|_______________

o cells used for interpolation
+ fine cell center that you want to interpolate to

(z = k -1)  Q_FC_plane_0 = (w0_y) * Q_0 + (w1_y) Q_1  + (w2_y) Q_2
(z = k)     Q_FC_plane_1 = (w0_y) * Q_0 + (w1_y) Q_1  + (w2_y) Q_2
(z = k +1)  Q_FC_plane_2 = (w0_y) * Q_0 + (w1_y) Q_1  + (w2_y) Q_2

 Q_FC = (w0_z)Q_fc_plane_0 + (w1_z)Q_fc_plane_0 + (w2_z)Q_fc_plane_0


_____________________________________________________________________*/
template<class T>
  void quadraticInterpolation(constCCVariable<T>& q_CL,// course level
                             const Level* coarseLevel,
                             const Level* fineLevel,
                             const IntVector& refineRatio,
                             const IntVector& fl,
                             const IntVector& fh,
                             CCVariable<T>& q_FineLevel)
{
  //int ncell = 0;  // needed by interpolation test
  //T error(0);
  
  Vector c_dx = coarseLevel->dCell();
  Vector inv_c_dx = Vector(1.0)/c_dx;
  
  for(CellIterator iter(fl,fh); !iter.done(); iter++){
    IntVector f_cell = *iter;
    IntVector c_cell = fineLevel->mapCellToCoarser(f_cell);
    
    //__________________________________
    // Offset for coarse level surrounding cells:
    //  -find the normalized distance between the coarse and fine level cell centers  
    Point coarse_cell_pos = coarseLevel->getCellPosition(c_cell);
    Point fine_cell_pos   = fineLevel->getCellPosition(f_cell);
    Vector dist = (fine_cell_pos.asVector() - coarse_cell_pos.asVector()) * inv_c_dx;
    
    //dist = Abs(dist); 
    //__________________________________
    //  Find the weights 
    double x = dist.x();
    double y = dist.y();
    double z = dist.z();
    
    double w0_x =  0.5 * x  * (x - 1.0);
    double w1_x = -(x + 1.0)* (x - 1.0);
    double w2_x =  0.5 * x  * (x + 1.0);
    
    double w0_y =  0.5 * y  * (y - 1.0);
    double w1_y = -(y + 1.0)* (y - 1.0);
    double w2_y =  0.5 * y  * (y + 1.0);
    
    double w0_z =  0.5 * z  * (z - 1.0);
    double w1_z = -(z + 1.0)* (z - 1.0);
    double w2_z =  0.5 * z  * (z + 1.0);
    
    FastMatrix w(3, 3);
    //  Q_CL(-1,-1,k)      Q_CL(0,-1,k)          Q_CL(1,-1,k)
    w(0,0) = w0_x * w0_y; w(1,0) = w1_x * w0_y; w(2,0) = w2_x * w0_y;
    w(0,1) = w0_x * w1_y; w(1,1) = w1_x * w1_y; w(2,1) = w2_x * w1_y;
    w(0,2) = w0_x * w2_y; w(1,2) = w1_x * w2_y; w(2,2) = w2_x * w2_y;  
    //  Q_CL(-1, 1,k)      Q_CL(0, 1,k)          Q_CL(1, 1,k)      
    
    
#if 0    
    // Maybe use this when the patch is on the fine level.
  bool onBoundary = true;
    int x_m = onBoundary?Sign(dist.x()):-1;
    int y_m = onBoundary?Sign(dist.y()):-1;
    int z_m = onBoundary?Sign(dist.z()):-1;
    int x_p = onBoundary?2*Sign(dist.x()):1;
    int y_p = onBoundary?2*Sign(dist.y()):1;
    int z_p = onBoundary?2*Sign(dist.z()):1;
    
    cout << f_cell << endl;
    cout << " x_p " << x_p << " y_p " << y_p << " z_p " << z_p<< endl; 
    cout << " x_m " << x_m << " y_m " << y_m << " z_m " << z_m<< endl;    
#endif    
    
    vector<T> q_XY_Plane(3);
     
    int k = -2; 
    // loop over the three X-Y planes
    for(int p = 0; p < 3; p++){
      k += 1;

      q_XY_Plane[p]   // X-Y plane
        = w(0,0) * q_CL[c_cell + IntVector( -1, -1, k)]   
        + w(1,0) * q_CL[c_cell + IntVector(  0, -1, k)]           
        + w(2,0) * q_CL[c_cell + IntVector(  1, -1, k)]           
        + w(0,1) * q_CL[c_cell + IntVector( -1,  0, k)]            
        + w(1,1) * q_CL[c_cell + IntVector(  0,  0, k)]    
        + w(2,1) * q_CL[c_cell + IntVector(  1,  0, k)]     
        + w(0,2) * q_CL[c_cell + IntVector( -1,  1, k)]   
        + w(1,2) * q_CL[c_cell + IntVector(  0,  1, k)]     
        + w(2,2) * q_CL[c_cell + IntVector(  1,  1, k)]; 
    }
    
    // interpolate the 3 X-Y planes 
    q_FineLevel[f_cell] = w0_z * q_XY_Plane[0] 
                        + w1_z * q_XY_Plane[1] 
                        + w2_z * q_XY_Plane[2];

   //interpolationTest(fineLevel,f_cell, ncell, error, q_FineLevel[f_cell]); 
   // cout  << " plane 1 " << q_XY_Plane[0] << " plane2 " << q_XY_Plane[1] << " plane3 "<< q_XY_Plane[3]<< endl;
  } 
  
  // interpolation test
  //error = error/(double)ncell;
  //cout << "Quadratic interpolation error^2/ncell " << error << " ncells " << ncell<< endl;
}
/*___________________________________________________________________
 Function~  AMRICE::interpolationInterface--
 Purpose:    depending on which 
_____________________________________________________________________*/
#if 0
template<class T>
  void interpolationInterface(constCCVariable<T>& q_CL,// course level
                              DataWarehouse* coarse_new_dw,
                              const Level* coarseLevel,
                              const Level* fineLevel,
                              const IntVector& refineRatio,
                              const IntVector& fl,
                              const IntVector& fh,
                              CCVariable<T>& q_FineLevel)
{


  //__________________________________
  //  Initialize test data
  cout.setf(ios::scientific,ios::floatfield);
  cout.precision(16);

  CCVariable<T> tmp;
  constCCVariable<T> testData;
  
  Vector c_dx = coarseLevel->dCell();
  for(Level::const_patchIterator iter = coarseLevel->patchesBegin();
                                iter != coarseLevel->patchesEnd(); iter++){
    const Patch* patch2 = *iter;
    coarse_new_dw->allocateTemporary(tmp,patch2);
    for(CellIterator iter = patch2->getExtraCellIterator(); !iter.done();iter++){
      IntVector c = *iter; 
      double X = ((c.x() * c_dx.x()) + c_dx.x()/2);
      double Y = ((c.y() * c_dx.y()) + c_dx.y()/2);
      double Z = ((c.z() * c_dx.z()) + c_dx.z()/2);
      //tmp[c] = varType(5.0);
      //tmp[c] = varType( X );
      //tmp[c] = varType( Y );
      //tmp[c] = varType( Z );
      tmp[c] = T( X * Y* Z  );
      //tmp[c] = varType( X * X * Y * Y * Z * Z);
      //tmp[c] = varType( X * X * X * Y* Y * Y  * Z * Z *Z );
    }
    testData = tmp;
  }

  
  linearInterpolation<T>(testData, coarseLevel, fineLevel,
                          refineRatio, fl,fh, q_FineLevel); 
                                     
  quadraticInterpolation<T>(testData, coarseLevel, fineLevel,
                            refineRatio, fl,fh, q_FineLevel);
}
#endif

/*___________________________________________________________________
 Function~  AMRICE::refine_CF_interfaceOperator-- 
_____________________________________________________________________*/
template<class varType>
void AMRICE::refine_CF_interfaceOperator(const Patch* patch, 
                                 const Level* fineLevel,
                                 const Level* coarseLevel,
                                 CCVariable<varType>& Q, 
                                 const VarLabel* label,
                                 double subCycleProgress_var, 
                                 int matl, 
                                 DataWarehouse* fine_new_dw,
                                 DataWarehouse* coarse_old_dw,
                                 DataWarehouse* coarse_new_dw)
{
  cout_dbg << *patch << " ";
  patch->printPatchBCs(cout_dbg);
  
  for(Patch::FaceType face = Patch::startFace;
      face <= Patch::endFace; face=Patch::nextFace(face)){

   if(patch->getBCType(face) != Patch::Neighbor) {
      //__________________________________
      // fine level hi & lo cell iter limits
      // coarselevel hi and low index
      CellIterator iter_tmp = patch->getFaceCellIterator(face, "plusEdgeCells");
      IntVector fl = iter_tmp.begin();
      IntVector fh = iter_tmp.end(); 
      IntVector refineRatio = fineLevel->getRefinementRatio();
      IntVector coarseLow  = fineLevel->mapCellToCoarser(fl);
      IntVector coarseHigh = fineLevel->mapCellToCoarser(fh+refineRatio - IntVector(1,1,1));

      //__________________________________
      // enlarge the coarselevel foot print by oneCell
      // x-           x+        y-       y+       z-        z+
      // (-1,0,0)  (1,0,0)  (0,-1,0)  (0,1,0)  (0,0,-1)  (0,0,1)
      IntVector oneCell = patch->faceDirection(face);
      if( face == Patch::xminus || face == Patch::yminus 
                                || face == Patch::zminus) {
        coarseHigh -= oneCell;
      }
      if( face == Patch::xplus || face == Patch::yplus 
                               || face == Patch::zplus) {
        coarseLow -= oneCell;
      }
      
      //__________________________________
      // If the face is orthogonal to a neighboring
      // patch face increase the coarse level foot print
      // Linear interpolation:    1
      // Quadratic interpolation: 3     ??????
      
      IntVector expandCellsLo = IntVector(1,1,1) - patch->neighborsLow();
      IntVector expandCellsHi = IntVector(1,1,1) - patch->neighborsHigh();
      
      IntVector plusMinus_face = patch->faceDirection(face);
      IntVector axes = patch->faceAxes(face);
      int P_dir   = axes[0];  // principal direction
      
      // in the face direction ignore the cell expansion
      expandCellsLo[P_dir] = 0;
      expandCellsHi[P_dir] = 0;
      
      coarseHigh += expandCellsHi;
      coarseLow  -= expandCellsLo;
    
      cout_dbg<< " face " << face << " refineRatio "<< refineRatio
              << " BC type " << patch->getBCType(face)
              << " FineLevel iterator" << fl << " " << fh 
              << " expandCellsHi " << expandCellsHi
              << " expandCellsLo " << expandCellsLo
              << " \t coarseLevel iterator " << coarseLow << " " << coarseHigh<<endl;
               
      //__________________________________
      //   subCycleProgress_var  = 0
      //  interpolation using the coarse_old_dw data
      if(subCycleProgress_var < 1.e-10){
        constCCVariable<varType> q_OldDW;
        coarse_old_dw->getRegion(q_OldDW, label, matl, coarseLevel,
                                 coarseLow, coarseHigh);

        linearInterpolation<varType>(q_OldDW, coarseLevel, fineLevel,
                                     refineRatio, fl,fh, Q);      
                                    
                                    
     //  interpolationInterface<varType>(q_OldDW,coarse_new_dw, coarseLevel, fineLevel,
     //                                  refineRatio, fl,fh, Q);  
      } 
       
       //__________________________________
       // subCycleProgress_var near 1.0
       //  interpolation using the coarse_new_dw data
      else if(subCycleProgress_var > 1-1.e-10){ 
       constCCVariable<varType> q_NewDW;
       coarse_new_dw->getRegion(q_NewDW, label, matl, coarseLevel,
                             coarseLow, coarseHigh);

       linearInterpolation<varType>(q_NewDW, coarseLevel, fineLevel,
                                    refineRatio, fl,fh, Q); 
      } else {    
                      
      //__________________________________
      // subCycleProgress_var somewhere between 0 or 1
      //  interpolation from both coarse new and old dw 
        constCCVariable<varType> q_OldDW, q_NewDW;
        coarse_old_dw->getRegion(q_OldDW, label, matl, coarseLevel,
                             coarseLow, coarseHigh);
        coarse_new_dw->getRegion(q_NewDW, label, matl, coarseLevel,
                             coarseLow, coarseHigh);
                             
        CCVariable<varType> Q_old, Q_new;
        fine_new_dw->allocateTemporary(Q_old, patch);
        fine_new_dw->allocateTemporary(Q_new, patch);
        
        Q_old.initialize(varType(d_EVIL_NUM));
        Q_new.initialize(varType(d_EVIL_NUM));
                             
        linearInterpolation<varType>(q_OldDW, coarseLevel, fineLevel,
                                     refineRatio, fl,fh, Q_old);
                                      
        linearInterpolation<varType>(q_NewDW, coarseLevel, fineLevel,
                                     refineRatio, fl,fh, Q_new);
        // Linear interpolation in time
        for(CellIterator iter(fl,fh); !iter.done(); iter++){
          IntVector f_cell = *iter;
          Q[f_cell] = (1. - subCycleProgress_var)*Q_old[f_cell] 
                          + subCycleProgress_var *Q_new[f_cell];
        }
      }
    }
  }  // face
   
  //____ B U L L E T   P R O O F I N G_______ 
  // All values must be initialized at this point
  if(subCycleProgress_var < 1.e-10){  
    IntVector badCell;
    if( isEqual<varType>(varType(d_EVIL_NUM),patch,Q, badCell) ){
      ostringstream warn;
      warn <<"ERROR AMRICE::refine_CF_interfaceOperator "
           << "detected an uninitialized variable: "
           << label->getName() << ", cell " << badCell
           << " Q_CC " << Q[badCell] 
           << " Patch " << patch->getID() << " Level idx "
           <<fineLevel->getIndex()<<"\n ";
      throw InvalidValue(warn.str());
    }
  }
 
  cout_dbg.setActive(false);// turn off the switch for cout_dbg
}


/*___________________________________________________________________
 Function~  AMRICE::refineCoarseFineBoundaries--    D O U B L E  
_____________________________________________________________________*/
void AMRICE::refineCoarseFineBoundaries(const Patch* patch,
                                        CCVariable<double>& val,
                                        DataWarehouse* fine_new_dw,
                                        const VarLabel* label,
                                        int matl,
                                        double subCycleProgress_var)
{
  const Level* level = patch->getLevel();
  const Level* coarseLevel = level->getCoarserLevel().get_rep();

  cout_dbg << "\t refineCoarseFineBoundaries ("<<label->getName() << ") \t" 
           << " subCycleProgress_var " << subCycleProgress_var
           << " Level-" << level->getIndex()<< '\n';
  DataWarehouse* coarse_old_dw = 
                 fine_new_dw->getOtherDataWarehouse(Task::CoarseOldDW);
  DataWarehouse* coarse_new_dw = 
                 fine_new_dw->getOtherDataWarehouse(Task::CoarseNewDW);
  
  refine_CF_interfaceOperator<double>
    (patch, level, coarseLevel, val, label, subCycleProgress_var, matl,
     fine_new_dw, coarse_old_dw, coarse_new_dw);
}
/*___________________________________________________________________
 Function~  AMRICE::refineCoarseFineBoundaries--    V E C T O R 
_____________________________________________________________________*/
void AMRICE::refineCoarseFineBoundaries(const Patch* patch,
                                        CCVariable<Vector>& val,
                                        DataWarehouse* fine_new_dw,
                                        const VarLabel* label,
                                        int matl,
                                        double subCycleProgress_var)
{
  const Level* level = patch->getLevel();
  const Level* coarseLevel = level->getCoarserLevel().get_rep();

  cout_dbg << "\t refineCoarseFineBoundaries ("<<label->getName() << ") \t" 
           << " subCycleProgress_var " << subCycleProgress_var
           << " Level-" << level->getIndex()<< '\n';
  DataWarehouse* coarse_old_dw = 
                 fine_new_dw->getOtherDataWarehouse(Task::CoarseOldDW);
  DataWarehouse* coarse_new_dw = 
                 fine_new_dw->getOtherDataWarehouse(Task::CoarseNewDW);

  refine_CF_interfaceOperator<Vector>
    (patch, level, coarseLevel, val, label, subCycleProgress_var, matl,
     fine_new_dw, coarse_old_dw, coarse_new_dw);
}

/*___________________________________________________________________
 Function~  AMRICE::scheduleRefine--  
_____________________________________________________________________*/
void AMRICE::scheduleRefine(const PatchSet* patches,
                               SchedulerP& sched)
{
  cout_dbg << "AMRICE::scheduleRefine\t\t\t\tP-" << *patches << '\n';
  Task* task = scinew Task("refine",this, &AMRICE::refine);

  MaterialSubset* subset = scinew MaterialSubset;
  subset->add(0);
  Ghost::GhostType  gac = Ghost::AroundCells;
  
  task->requires(Task::NewDW, lb->press_CCLabel,
               0, Task::CoarseLevel, subset, Task::OutOfDomain, gac,1);
                 
  task->requires(Task::NewDW, lb->rho_CCLabel,
               0, Task::CoarseLevel, 0, Task::NormalDomain, gac,1);
               
  task->requires(Task::NewDW, lb->sp_vol_CCLabel,
               0, Task::CoarseLevel, 0, Task::NormalDomain, gac,1);
  
  task->requires(Task::NewDW, lb->temp_CCLabel,
               0, Task::CoarseLevel, 0, Task::NormalDomain, gac,1);
  
  task->requires(Task::NewDW, lb->vel_CCLabel,
               0, Task::CoarseLevel, 0, Task::NormalDomain, gac,1);

  //__________________________________
  // Model Variables.
  if(d_modelSetup && d_modelSetup->tvars.size() > 0){
    vector<TransportedVariable*>::iterator iter;

    for(iter = d_modelSetup->tvars.begin();
       iter != d_modelSetup->tvars.end(); iter++){
      TransportedVariable* tvar = *iter;
      task->requires(Task::NewDW, tvar->var,
                  0, Task::CoarseLevel, 0, Task::NormalDomain, gac,1);
      task->computes(tvar->var);
    }
  }
  
  task->computes(lb->press_CCLabel);
  task->computes(lb->rho_CCLabel);
  task->computes(lb->sp_vol_CCLabel);
  task->computes(lb->temp_CCLabel);
  task->computes(lb->vel_CCLabel);

  sched->addTask(task, patches, d_sharedState->allMaterials()); 
}

/*___________________________________________________________________
 Function~  AMRICE::Refine--  
_____________________________________________________________________*/
void AMRICE::refine(const ProcessorGroup*,
                    const PatchSubset* patches,
                    const MaterialSubset* matls,
                    DataWarehouse*,
                    DataWarehouse* new_dw)
{
  
  const Level* fineLevel = getLevel(patches);
  const Level* coarseLevel = fineLevel->getCoarserLevel().get_rep();
  
  cout_doing << "Doing refine \t\t\t\t\t\t AMRICE L-"<< fineLevel->getIndex();
  IntVector rr(fineLevel->getRefinementRatio());
  double invRefineRatio = 1./(rr.x()*rr.y()*rr.z());
  
  for(int p=0;p<patches->size();p++){  
    const Patch* finePatch = patches->get(p);
    cout_doing << " patch " << finePatch->getID()<< endl;
    
    Level::selectType coarsePatches;
    finePatch->getCoarseLevelPatches(coarsePatches);
    
    // bullet proofing
    iteratorTest(finePatch, fineLevel, coarseLevel, new_dw);

    for(int m = 0;m<matls->size();m++){
      int indx = matls->get(m);

      CCVariable<double> rho_CC, press_CC, temp, sp_vol_CC;
      CCVariable<Vector> vel_CC;
      new_dw->allocateAndPut(press_CC, lb->press_CCLabel,  indx, finePatch);
      new_dw->allocateAndPut(rho_CC,   lb->rho_CCLabel,    indx, finePatch);
      new_dw->allocateAndPut(sp_vol_CC,lb->sp_vol_CCLabel, indx, finePatch);
      new_dw->allocateAndPut(temp,     lb->temp_CCLabel,   indx, finePatch);
      new_dw->allocateAndPut(vel_CC,   lb->vel_CCLabel,    indx, finePatch);  
 
      press_CC.initialize(d_EVIL_NUM);
      rho_CC.initialize(d_EVIL_NUM);
      sp_vol_CC.initialize(d_EVIL_NUM);
      temp.initialize(d_EVIL_NUM);
      vel_CC.initialize(Vector(d_EVIL_NUM));
      
      // refine data
      CoarseToFineOperator<double>(press_CC,  lb->press_CCLabel,indx, new_dw, 
                         invRefineRatio, finePatch, fineLevel, coarseLevel);
                         
      CoarseToFineOperator<double>(rho_CC,    lb->rho_CCLabel,  indx, new_dw, 
                         invRefineRatio, finePatch, fineLevel, coarseLevel);      

      CoarseToFineOperator<double>(sp_vol_CC, lb->sp_vol_CCLabel,indx, new_dw, 
                         invRefineRatio, finePatch, fineLevel, coarseLevel);

      CoarseToFineOperator<double>(temp,      lb->temp_CCLabel, indx, new_dw, 
                         invRefineRatio, finePatch, fineLevel, coarseLevel);
       
      CoarseToFineOperator<Vector>( vel_CC,   lb->vel_CCLabel,  indx, new_dw, 
                         invRefineRatio, finePatch, fineLevel, coarseLevel);
      //__________________________________
      //    Model Variables                     
      if(d_modelSetup && d_modelSetup->tvars.size() > 0){
        vector<TransportedVariable*>::iterator t_iter;
        for( t_iter  = d_modelSetup->tvars.begin();
             t_iter != d_modelSetup->tvars.end(); t_iter++){
          TransportedVariable* tvar = *t_iter;

          if(tvar->matls->contains(indx)){
            CCVariable<double> q_CC;
            new_dw->allocateAndPut(q_CC, tvar->var, indx, finePatch);
            
            q_CC.initialize(d_EVIL_NUM);
            
            CoarseToFineOperator<double>(q_CC, tvar->var, indx, new_dw, 
                       invRefineRatio, finePatch, fineLevel, coarseLevel);
                       
            if(switchDebug_AMR_refine){
              ostringstream desc; 
              string name = tvar->var->getName();
              printData(indx, finePatch, 1, "Refine_task", name, q_CC);
            }                 
          } 
        }
      }    
      
      //__________________________________
      //  Print Data
      if(switchDebug_AMR_refine){ 
      ostringstream desc;     
        desc << "BOT_Refine_Mat_" << indx << "_patch_"<< finePatch->getID();
        printData(indx, finePatch,   1, desc.str(), "press_CC",  press_CC);
        printData(indx, finePatch,   1, desc.str(), "rho_CC",    rho_CC);
        printData(indx, finePatch,   1, desc.str(), "sp_vol_CC", sp_vol_CC);
        printData(indx, finePatch,   1, desc.str(), "Temp_CC",   temp);
        printVector(indx, finePatch, 1, desc.str(), "vel_CC", 0, vel_CC);
      }
    }
  }  // course patch loop 
}

/*_____________________________________________________________________
 Function~  AMRICE::iteratorTest--
 Purpose~   Verify that the all of the fine level cells will be accessed 
_____________________________________________________________________*/
void AMRICE::iteratorTest(const Patch* finePatch,
                          const Level* fineLevel,
                          const Level* coarseLevel,
                          DataWarehouse* new_dw)
{
  Level::selectType coarsePatches;
  finePatch->getCoarseLevelPatches(coarsePatches); 
  IntVector fl = finePatch->getCellLowIndex();
  IntVector fh = finePatch->getCellHighIndex();
  
  CCVariable<double> hitCells;
  new_dw->allocateTemporary(hitCells, finePatch);
  hitCells.initialize(d_EVIL_NUM);
  
  for(int i=0;i<coarsePatches.size();i++){
    const Patch* coarsePatch = coarsePatches[i];
    // iterator should hit the cells over the intersection of the fine and coarse patches

    IntVector cl = coarsePatch->getLowIndex();
    IntVector ch = coarsePatch->getHighIndex();
         
    IntVector fl_tmp = coarseLevel->mapCellToFiner(cl);
    IntVector fh_tmp = coarseLevel->mapCellToFiner(ch);
    
    IntVector lo = Max(fl, fl_tmp);
    IntVector hi = Min(fh, fh_tmp);
    
    for(CellIterator iter(lo,hi); !iter.done(); iter++){
      IntVector c = *iter;
      hitCells[c] = 1.0;
    }
#if 0
    cout << " coarsePatch.size() " << coarsePatches.size() 
         << " coarsePatch " << coarsePatch->getID()
         << " finePatch " << finePatch->getID() 
         << " fineLevel: fl " << fl << " fh " << fh
         << " coarseLevel: cl " << cl << " ch " << ch 
         << " final Iterator: " << lo << " " << hi << endl;
#endif    
    
  }
  
 //____ B U L L E T   P R O O F I N G_______ 
  // All cells must be initialized at this point
  // even in the extra cells 
  IntVector badCell;
  if( isEqual<double>(d_EVIL_NUM,finePatch,hitCells, badCell) ){
  
    IntVector c_badCell = fineLevel->mapCellToCoarser(badCell);
    const Patch* patch = coarseLevel->selectPatchForCellIndex(c_badCell);
    
    ostringstream warn;
    warn <<"ERROR AMRICE::Refine Task:iteratorTest "
         << "detected an fine level cell that won't get initialized "
         << badCell << " Patch " << finePatch->getID() 
         << " Level idx "<<fineLevel->getIndex()<<"\n "
         << "The underlying coarse cell "<< c_badCell 
         << " belongs to coarse level patch " << patch->getID() << "\n";
    throw InvalidValue(warn.str());
  }  
}

/*_____________________________________________________________________
 Function~  AMRICE::CoarseToFineOperator--
 Purpose~   push data from coarse Grid to the fine grid
_____________________________________________________________________*/
template<class T>
void AMRICE::CoarseToFineOperator(CCVariable<T>& q_CC,
                                  const VarLabel* varLabel,
                                  const int indx,
                                  DataWarehouse* new_dw,
                                  const double /*ratio*/,
                                  const Patch* finePatch,
                                  const Level* fineLevel,
                                  const Level* coarseLevel)
{
  Level::selectType coarsePatches;
  finePatch->getCoarseLevelPatches(coarsePatches); 
  IntVector refineRatio = coarseLevel->getRefinementRatio();
                       
  IntVector fl = finePatch->getCellLowIndex();
  IntVector fh = finePatch->getCellHighIndex();
  cout_dbg <<" coarseToFineOperator: " << varLabel->getName()
           <<" finePatch  "<< finePatch->getID() << " " 
           << fl<<" "<< fh<<endl;
  Ghost::GhostType  gac = Ghost::AroundCells;
  
  for(int i=0;i<coarsePatches.size();i++){
    const Patch* coarsePatch = coarsePatches[i];
    constCCVariable<T> coarse_q_CC;
    new_dw->get(coarse_q_CC, varLabel, indx, coarsePatch,gac, 1);
    
    // Only interpolate over the intersection of the fine and coarse patches
    // coarse cell 
    IntVector cl = coarsePatch->getLowIndex();
    IntVector ch = coarsePatch->getHighIndex();
         
    IntVector fl_tmp = coarseLevel->mapCellToFiner(cl);
    IntVector fh_tmp = coarseLevel->mapCellToFiner(ch);
    
    IntVector lo = Max(fl, fl_tmp);
    IntVector hi = Min(fh, fh_tmp);
    
/*`==========TESTING==========*/
      linearInterpolation<T>(coarse_q_CC, coarseLevel, fineLevel,
                            refineRatio, lo,hi,q_CC);
/*===========TESTING==========`*/
  }
  
  //____ B U L L E T   P R O O F I N G_______ 
  // All values must be initialized at this point
  // even in the extra cells 
  IntVector badCell;
  if( isEqual<T>(T(d_EVIL_NUM),finePatch,q_CC, badCell) ){
    ostringstream warn;
    warn <<"ERROR AMRICE::Refine Task:CoarseToFineOperator "
         << "detected an uninitialized variable "<< varLabel->getName()
         << " " << badCell << " Patch " << finePatch->getID() 
         << " Level idx "<<fineLevel->getIndex()<<"\n ";
    throw InvalidValue(warn.str());
  }
}

/*___________________________________________________________________
 Function~  AMRICE::scheduleCoarsen--  
_____________________________________________________________________*/
void AMRICE::scheduleCoarsen(const LevelP& coarseLevel,
                               SchedulerP& sched)
{
  Ghost::GhostType  gn = Ghost::None; 
  cout_dbg << "AMRICE::scheduleCoarsen\t\t\t\tL-" << coarseLevel->getIndex() << '\n';
  Task* task = scinew Task("coarsen",this, &AMRICE::coarsen);

  task->requires(Task::NewDW, lb->press_CCLabel,
               0, Task::FineLevel, 0, Task::NormalDomain, gn, 0);
                 
  task->requires(Task::NewDW, lb->rho_CCLabel,
               0, Task::FineLevel, 0, Task::NormalDomain, gn, 0);
               
  task->requires(Task::NewDW, lb->sp_vol_CCLabel,
               0, Task::FineLevel, 0, Task::NormalDomain, gn, 0);
  
  task->requires(Task::NewDW, lb->temp_CCLabel,
               0, Task::FineLevel, 0, Task::NormalDomain, gn, 0);
  
  task->requires(Task::NewDW, lb->vel_CCLabel,
               0, Task::FineLevel, 0, Task::NormalDomain, gn, 0);

  //__________________________________
  // Model Variables.
  if(d_modelSetup && d_modelSetup->tvars.size() > 0){
    vector<TransportedVariable*>::iterator iter;

    for(iter = d_modelSetup->tvars.begin();
       iter != d_modelSetup->tvars.end(); iter++){
      TransportedVariable* tvar = *iter;
      task->requires(Task::NewDW, tvar->var,
                  0, Task::FineLevel, 0, Task::NormalDomain, gn, 0);
      task->modifies(tvar->var);
    }
  }
  
  task->modifies(lb->press_CCLabel);
  task->modifies(lb->rho_CCLabel);
  task->modifies(lb->sp_vol_CCLabel);
  task->modifies(lb->temp_CCLabel);
  task->modifies(lb->vel_CCLabel);

  sched->addTask(task, coarseLevel->eachPatch(), d_sharedState->allMaterials()); 
  
  
  //__________________________________
  // schedule refluxing
  if(d_doRefluxing){
    scheduleReflux_computeCorrectionFluxes(coarseLevel, sched);
    scheduleReflux_applyCorrection(coarseLevel, sched);
  }
  
}

/*___________________________________________________________________
 Function~  AMRICE::Coarsen--  
_____________________________________________________________________*/
void AMRICE::coarsen(const ProcessorGroup*,
                     const PatchSubset* patches,
                     const MaterialSubset* matls,
                     DataWarehouse*,
                     DataWarehouse* new_dw)
{
  
  const Level* coarseLevel = getLevel(patches);
  const Level* fineLevel = coarseLevel->getFinerLevel().get_rep();
  cout_doing << "Doing coarsen \t\t\t\t\t\t AMRICE L-" <<fineLevel->getIndex();
  
  IntVector rr(fineLevel->getRefinementRatio());
  double invRefineRatio = 1./(rr.x()*rr.y()*rr.z());
  
  bool dbg_onOff = cout_dbg.active();      // is cout_dbg switch on or off
  
  for(int p=0;p<patches->size();p++){  
    const Patch* coarsePatch = patches->get(p);
    cout_doing << " patch " << coarsePatch->getID()<< endl;

    for(int m = 0;m<matls->size();m++){
      int indx = matls->get(m);

      CCVariable<double> rho_CC, press_CC, temp, sp_vol_CC;
      CCVariable<Vector> vel_CC;
      new_dw->getModifiable(press_CC, lb->press_CCLabel,  indx, coarsePatch);
      new_dw->getModifiable(rho_CC,   lb->rho_CCLabel,    indx, coarsePatch);
      new_dw->getModifiable(sp_vol_CC,lb->sp_vol_CCLabel, indx, coarsePatch);
      new_dw->getModifiable(temp,     lb->temp_CCLabel,   indx, coarsePatch);
      new_dw->getModifiable(vel_CC,   lb->vel_CCLabel,    indx, coarsePatch);  
      
      // coarsen
      fineToCoarseOperator<double>(press_CC,  lb->press_CCLabel,indx, new_dw, 
                         invRefineRatio, coarsePatch, coarseLevel, fineLevel);
                         
      fineToCoarseOperator<double>(rho_CC,    lb->rho_CCLabel,  indx, new_dw, 
                         invRefineRatio, coarsePatch, coarseLevel, fineLevel);      

      fineToCoarseOperator<double>(sp_vol_CC, lb->sp_vol_CCLabel,indx, new_dw, 
                         invRefineRatio, coarsePatch, coarseLevel, fineLevel);

      fineToCoarseOperator<double>(temp,      lb->temp_CCLabel, indx, new_dw, 
                         invRefineRatio, coarsePatch, coarseLevel, fineLevel);
       
      fineToCoarseOperator<Vector>( vel_CC,   lb->vel_CCLabel,  indx, new_dw, 
                         invRefineRatio, coarsePatch, coarseLevel, fineLevel);
      //__________________________________
      //    Model Variables                     
      if(d_modelSetup && d_modelSetup->tvars.size() > 0){
        vector<TransportedVariable*>::iterator t_iter;
        for( t_iter  = d_modelSetup->tvars.begin();
            t_iter != d_modelSetup->tvars.end(); t_iter++){
          TransportedVariable* tvar = *t_iter;

          if(tvar->matls->contains(indx)){
            CCVariable<double> q_CC;
            new_dw->getModifiable(q_CC, tvar->var, indx, coarsePatch);
            fineToCoarseOperator<double>(q_CC, tvar->var, indx, new_dw, 
                       invRefineRatio, coarsePatch, coarseLevel, fineLevel);
            
            if(switchDebug_AMR_refine){  
              string name = tvar->var->getName();
              printData(indx, coarsePatch, 1, "coarsen_models", name, q_CC);
            }                 
          }
        }
      }    

      //__________________________________
      //  Print Data 
      if(switchDebug_AMR_refine){
        ostringstream desc;     
        desc << "coarsen_Mat_" << indx << "_patch_"<< coarsePatch->getID();
        printData(indx, coarsePatch,   1, desc.str(), "press_CC",    press_CC);
        printData(indx, coarsePatch,   1, desc.str(), "rho_CC",      rho_CC);
        printData(indx, coarsePatch,   1, desc.str(), "sp_vol_CC",   sp_vol_CC);
        printData(indx, coarsePatch,   1, desc.str(), "Temp_CC",     temp);
        printVector(indx, coarsePatch, 1, desc.str(), "vel_CC", 0,   vel_CC);
      }
    }
  }  // course patch loop 
  cout_dbg.setActive(dbg_onOff);  // reset on/off switch for cout_dbg
}
/*_____________________________________________________________________
 Function~  AMRICE::fineToCoarseOperator--
 Purpose~   averages the interior fine patch data onto the coarse patch
_____________________________________________________________________*/
template<class T>
void AMRICE::fineToCoarseOperator(CCVariable<T>& q_CC,
                                  const VarLabel* varLabel,
                                  const int indx,
                                  DataWarehouse* new_dw,
                                  const double ratio,
                                  const Patch* coarsePatch,
                                  const Level* coarseLevel,
                                  const Level* fineLevel)
{
  Level::selectType finePatches;
  coarsePatch->getFineLevelPatches(finePatches);
                            
  for(int i=0;i<finePatches.size();i++){
    const Patch* finePatch = finePatches[i];
    
    constCCVariable<T> fine_q_CC;
    new_dw->get(fine_q_CC, varLabel, indx, finePatch,Ghost::AroundCells, 1);

    IntVector fl(finePatch->getInteriorCellLowIndex());
    IntVector fh(finePatch->getInteriorCellHighIndex());
    IntVector cl(fineLevel->mapCellToCoarser(fl));
    IntVector ch(fineLevel->mapCellToCoarser(fh));
    
    cl = Max(cl, coarsePatch->getCellLowIndex());
    ch = Min(ch, coarsePatch->getCellHighIndex());
    
    cout_dbg << " fineToCoarseOperator: finePatch "<< fl << " " << fh 
             << " coarsePatch "<< cl << " " << ch << endl;
             
    IntVector refinementRatio = fineLevel->getRefinementRatio();
    
    T zero(0.0);
    // iterate over coarse level cells
    for(CellIterator iter(cl, ch); !iter.done(); iter++){
      IntVector c = *iter;
      T q_CC_tmp(zero);
      IntVector fineStart = coarseLevel->mapCellToFiner(c);
    
      // for each coarse level cell iterate over the fine level cells   
      for(CellIterator inside(IntVector(0,0,0),refinementRatio );
                                          !inside.done(); inside++){
        IntVector fc = fineStart + *inside;
        q_CC_tmp += fine_q_CC[fc];
      }
      q_CC[c] =q_CC_tmp*ratio;
    }
  }
  cout_dbg.setActive(false);// turn off the switch for cout_dbg
}
 
/*___________________________________________________________________
 Function~  AMRICE::scheduleReflux_computeCorrectionFluxes--  
_____________________________________________________________________*/
void AMRICE::scheduleReflux_computeCorrectionFluxes(const LevelP& coarseLevel,
                                                    SchedulerP& sched)
{
 
  cout_doing << "AMRICE::scheduleReflux_computeCorrectionFluxes\tL-" 
             << coarseLevel->getIndex() << '\n';
             
  Task* task = scinew Task("reflux_computeCorrectionFluxes",
                           this, &AMRICE::reflux_computeCorrectionFluxes);
  
  Ghost::GhostType gn = Ghost::None;     
  //__________________________________
  // Fluxes from the fine level            
                                      // MASS
  task->requires(Task::NewDW, lb->mass_X_FC_fluxLabel,
               0,Task::FineLevel, 0, Task::NormalDomain, gn, 0);
  task->requires(Task::NewDW, lb->mass_Y_FC_fluxLabel,
               0,Task::FineLevel, 0, Task::NormalDomain, gn, 0);
  task->requires(Task::NewDW, lb->mass_Z_FC_fluxLabel,
               0,Task::FineLevel, 0, Task::NormalDomain, gn, 0);
                                      // MOMENTUM
  task->requires(Task::NewDW, lb->mom_X_FC_fluxLabel,
               0,Task::FineLevel, 0, Task::NormalDomain, gn, 0);
  task->requires(Task::NewDW, lb->mom_Y_FC_fluxLabel,
               0,Task::FineLevel, 0, Task::NormalDomain, gn, 0);
  task->requires(Task::NewDW, lb->mom_Z_FC_fluxLabel,
               0,Task::FineLevel, 0, Task::NormalDomain, gn, 0);
                                      // INT_ENG
  task->requires(Task::NewDW, lb->int_eng_X_FC_fluxLabel,
               0,Task::FineLevel, 0, Task::NormalDomain, gn, 0);
  task->requires(Task::NewDW, lb->int_eng_Y_FC_fluxLabel,
               0,Task::FineLevel, 0, Task::NormalDomain, gn, 0);
  task->requires(Task::NewDW, lb->int_eng_Z_FC_fluxLabel,
               0,Task::FineLevel, 0, Task::NormalDomain, gn, 0);
                                      // SPECIFIC VOLUME
  task->requires(Task::NewDW, lb->sp_vol_X_FC_fluxLabel,
               0,Task::FineLevel, 0, Task::NormalDomain, gn, 0);
  task->requires(Task::NewDW, lb->sp_vol_Y_FC_fluxLabel,
               0,Task::FineLevel, 0, Task::NormalDomain, gn, 0);
  task->requires(Task::NewDW, lb->sp_vol_Z_FC_fluxLabel,
               0,Task::FineLevel, 0, Task::NormalDomain, gn, 0);             

  //__________________________________
  // Model Variables.
  if(d_modelSetup && d_modelSetup->d_reflux_vars.size() > 0){
    vector<AMR_refluxVariable*>::iterator iter;
    for( iter  = d_modelSetup->d_reflux_vars.begin();
         iter != d_modelSetup->d_reflux_vars.end(); iter++){
      AMR_refluxVariable* rvar = *iter;
      
      task->requires(Task::NewDW, rvar->var_CC,
                  0, Task::FineLevel, 0, Task::NormalDomain, gn, 0);
    }
  }

  task->modifies(lb->mass_X_FC_fluxLabel);
  task->modifies(lb->mass_Y_FC_fluxLabel);
  task->modifies(lb->mass_Z_FC_fluxLabel);
  
  task->modifies(lb->mom_X_FC_fluxLabel);
  task->modifies(lb->mom_Y_FC_fluxLabel);
  task->modifies(lb->mom_Z_FC_fluxLabel);
  
  task->modifies(lb->int_eng_X_FC_fluxLabel);
  task->modifies(lb->int_eng_Y_FC_fluxLabel);
  task->modifies(lb->int_eng_Z_FC_fluxLabel); 
  
  task->modifies(lb->sp_vol_X_FC_fluxLabel);
  task->modifies(lb->sp_vol_Y_FC_fluxLabel);
  task->modifies(lb->sp_vol_Z_FC_fluxLabel);
  
  sched->addTask(task, coarseLevel->eachPatch(), d_sharedState->allMaterials()); 
}
/*___________________________________________________________________
 Function~  AMRICE::Reflux_computeCorrectionFluxesFluxes--  
_____________________________________________________________________*/
void AMRICE::reflux_computeCorrectionFluxes(const ProcessorGroup*,
                                            const PatchSubset* coarsePatches,
                                            const MaterialSubset* matls,
                                            DataWarehouse*,
                                            DataWarehouse* new_dw)
{
  const Level* coarseLevel = getLevel(coarsePatches);
  const Level* fineLevel = coarseLevel->getFinerLevel().get_rep();
  
  cout_doing << "Doing reflux_computeCorrectionFluxes \t\t AMRICE L-"
             <<coarseLevel->getIndex()<< endl;
  
  bool dbg_onOff = cout_dbg.active();      // is cout_dbg switch on or off
  
  for(int c_p=0;c_p<coarsePatches->size();c_p++){  
    const Patch* coarsePatch = coarsePatches->get(c_p);

    for(int m = 0;m<matls->size();m++){
      int indx = matls->get(m);     
      CCVariable<double> rho_CC, temp, sp_vol_CC;
      constCCVariable<double> cv;
      CCVariable<Vector> vel_CC;

      Ghost::GhostType  gn  = Ghost::None;
      new_dw->getModifiable(rho_CC,   lb->rho_CCLabel,    indx, coarsePatch);
      new_dw->getModifiable(sp_vol_CC,lb->sp_vol_CCLabel, indx, coarsePatch);
      new_dw->getModifiable(temp,     lb->temp_CCLabel,   indx, coarsePatch);
      new_dw->getModifiable(vel_CC,   lb->vel_CCLabel,    indx, coarsePatch);
      new_dw->get(cv,                 lb->specific_heatLabel,indx,coarsePatch, gn,0);
      
      Level::selectType finePatches;
      coarsePatch->getFineLevelPatches(finePatches);
      
      for(int i=0; i < finePatches.size();i++){  
        const Patch* finePatch = finePatches[i];        
//        cout_doing << " coarsePatch " << coarsePatch->getID() <<" finepatch " << finePatch->getID()<< endl;

        //__________________________________
        // reflux
        refluxOperator_computeCorrectionFluxes<double>(rho_CC,   rho_CC,  cv, "mass",    indx, 
                      coarsePatch, finePatch, coarseLevel, fineLevel,new_dw);
                               
        refluxOperator_computeCorrectionFluxes<double>(sp_vol_CC, rho_CC, cv, "sp_vol",  indx, 
                      coarsePatch, finePatch, coarseLevel, fineLevel,new_dw);
                               
        refluxOperator_computeCorrectionFluxes<Vector>(vel_CC,    rho_CC, cv, "mom",     indx, 
                      coarsePatch, finePatch, coarseLevel, fineLevel,new_dw);

        refluxOperator_computeCorrectionFluxes<double>(temp,      rho_CC, cv, "int_eng", indx, 
                      coarsePatch, finePatch, coarseLevel, fineLevel,new_dw);
        //__________________________________
        //    Model Variables
        if(d_modelSetup && d_modelSetup->d_reflux_vars.size() > 0){
          vector<AMR_refluxVariable*>::iterator iter;
          for( iter  = d_modelSetup->d_reflux_vars.begin();
               iter != d_modelSetup->d_reflux_vars.end(); iter++){
            AMR_refluxVariable* r_var = *iter;
             
            if(r_var->matls->contains(indx)){
              CCVariable<double> q_CC;
              string var_name = r_var->var_CC->getName();
              new_dw->getModifiable(q_CC,  r_var->var_CC, indx, coarsePatch);
              
              refluxOperator_computeCorrectionFluxes<double>(q_CC, rho_CC, cv, var_name, indx, 
                            coarsePatch, finePatch, coarseLevel, fineLevel,new_dw);
            
              if(switchDebug_AMR_refine){
                string name = r_var->var_CC->getName();
                printData(indx, coarsePatch, 1, "coarsen_models", name, q_CC);
              }
            }
          }
        }
      }  // finePatch loop 
  
      //__________________________________
      //  Print Data
      if(switchDebug_AMR_reflux){ 
        ostringstream desc;     
        desc << "Reflux_Mat_" << indx << "_patch_"<< coarsePatch->getID();
        printData(indx, coarsePatch,   1, desc.str(), "rho_CC",   rho_CC);
        printData(indx, coarsePatch,   1, desc.str(), "sp_vol_CC",sp_vol_CC);
        printData(indx, coarsePatch,   1, desc.str(), "Temp_CC",  temp);
        printVector(indx, coarsePatch, 1, desc.str(), "vel_CC", 0,vel_CC);
      }
    }  // matl loop
  }  // course patch loop 
  cout_dbg.setActive(dbg_onOff);  // reset on/off switch for cout_dbg
}

/*___________________________________________________________________
 Function~  AMRICE::refluxCoarseLevelIterator--  
 Purpose:  returns the iterator and face-centered offset that the coarse 
           level uses to do refluxing.  THIS IS COMPILCATED AND CONFUSING
_____________________________________________________________________*/
void AMRICE::refluxCoarseLevelIterator(Patch::FaceType patchFace,
                                       const Patch* coarsePatch,
                                       const Patch* finePatch,
                                       const Level* fineLevel,
                                       CellIterator& iter,
                                       IntVector& coarse_FC_offset)
{
  CellIterator f_iter=finePatch->getFaceCellIterator(patchFace, "alongInteriorFaceCells");

  // find the intersection of the fine patch face iterator and underlying coarse patch
  IntVector f_lo_face = f_iter.begin();                 // fineLevel face indices   
  IntVector f_hi_face = f_iter.end();

  f_lo_face = fineLevel->mapCellToCoarser(f_lo_face);     
  f_hi_face = fineLevel->mapCellToCoarser(f_hi_face);

  IntVector c_lo_patch = coarsePatch->getLowIndex(); 
  IntVector c_hi_patch = coarsePatch->getHighIndex();

  IntVector l = Max(f_lo_face, c_lo_patch);             // intersection
  IntVector h = Min(f_hi_face, c_hi_patch);

  //__________________________________
  // Offset for the coarse level iterator
  // shift l & h,   1 cell for x+, y+, z+ finePatchfaces
  // shift l only, -1 cell for x-, y-, z- finePatchfaces
  // 
  // Offset for the coarse_face center (c_FC_offset)
  // shift 1 cell   for x-, y-, z- finePatch faces
  // shift 0 cells  for x+, y+, z+ finePatchFaces

  string name = finePatch->getFaceName(patchFace);
  IntVector offset = finePatch->faceDirection(patchFace);
  coarse_FC_offset = IntVector(0,0,0);

  if(name == "xminus" || name == "yminus" || name == "zminus"){
    l += offset;
    coarse_FC_offset = -offset;
  }
  if(name == "xplus" || name == "yplus" || name == "zplus"){
    l += offset;
    h += offset;
  }
  iter=CellIterator(l,h);
  
  cout_dbg << "refluxCoarseLevelIterator: face "<< patchFace
           << " " << iter << " coarse_FC_offset " << coarse_FC_offset << endl; 
}


/*_____________________________________________________________________
 Function~  AMRICE::refluxOperator_computeCorrectionFluxes--
 Purpose~   
_____________________________________________________________________*/
template<class T>
void AMRICE::refluxOperator_computeCorrectionFluxes( 
                              CCVariable<T>& q_CC_coarse,
                              CCVariable<double>& rho_CC_coarse,
                              constCCVariable<double>& cv,
                              const string& fineVarLabel,
                              const int indx,
                              const Patch* coarsePatch,
                              const Patch* finePatch,
                              const Level* coarseLevel,
                              const Level* fineLevel,
                              DataWarehouse* new_dw)
{
  // form the fine patch flux label names
  string x_name = fineVarLabel + "_X_FC_flux";
  string y_name = fineVarLabel + "_Y_FC_flux";
  string z_name = fineVarLabel + "_Z_FC_flux";
  
  // grab the varLabels
  VarLabel* xlabel = VarLabel::find(x_name);
  VarLabel* ylabel = VarLabel::find(y_name);
  VarLabel* zlabel = VarLabel::find(z_name);  

  if(xlabel == NULL || ylabel == NULL || zlabel == NULL){
    throw InternalError( "refluxOperator_computeCorrectionFluxes: variable label not found: " 
                          + x_name + " or " + y_name + " or " + z_name);
  }

  constSFCXVariable<T> Q_X_fine_flux;
  constSFCYVariable<T> Q_Y_fine_flux;
  constSFCZVariable<T> Q_Z_fine_flux;
  
  SFCXVariable<T>  Q_X_coarse_flux, Q_X_coarse_flux_org;
  SFCYVariable<T>  Q_Y_coarse_flux, Q_Y_coarse_flux_org;
  SFCZVariable<T>  Q_Z_coarse_flux, Q_Z_coarse_flux_org;
  
  Ghost::GhostType  gn = Ghost::None;
  new_dw->get(Q_X_fine_flux,    xlabel,indx, finePatch,   gn,0);
  new_dw->get(Q_Y_fine_flux,    ylabel,indx, finePatch,   gn,0);
  new_dw->get(Q_Z_fine_flux,    zlabel,indx, finePatch,   gn,0);
  
  new_dw->allocateTemporary(Q_X_coarse_flux_org, coarsePatch);
  new_dw->allocateTemporary(Q_Y_coarse_flux_org, coarsePatch);
  new_dw->allocateTemporary(Q_Z_coarse_flux_org, coarsePatch);
  
  new_dw->getModifiable(Q_X_coarse_flux,  xlabel,indx, coarsePatch);
  new_dw->getModifiable(Q_Y_coarse_flux,  ylabel,indx, coarsePatch);
  new_dw->getModifiable(Q_Z_coarse_flux,  zlabel,indx, coarsePatch);
  
  Q_X_coarse_flux_org.copyData(Q_X_coarse_flux);
  Q_Y_coarse_flux_org.copyData(Q_Y_coarse_flux);
  Q_Z_coarse_flux_org.copyData(Q_Z_coarse_flux);
  
  Q_X_coarse_flux.initialize(T(d_EVIL_NUM));  
  Q_Y_coarse_flux.initialize(T(d_EVIL_NUM));
  Q_Z_coarse_flux.initialize(T(d_EVIL_NUM));
  
  Vector dx = coarsePatch->dCell();
  double coarseCellVol = dx.x()*dx.y()*dx.z();
  
  //__________________________________
  //  switches that modify the denomiator 
  //  depending on which quantity is being refluxed.
  
  double switch1 = 0.0;     // denomiator = cellVol
  double switch2 = 1.0;     //            = mass
  double switch3 = 0.0;     //            = mass * cv
  if(fineVarLabel == "mass"){
    switch1 = 1.0;
    switch2 = 0.0;
    switch3 = 0.0;         
  }
  if(fineVarLabel == "int_eng"){
    switch1 = 0.0;
    switch2 = 0.0;
    switch3 = 1.0;
  }
  
/*`==========TESTING==========*/
#if 0
cout << " ------------ refluxOperator_computeCorrectionFluxes " << endl; 
#endif
/*===========TESTING==========`*/
  //__________________________________
  // Iterate over coarsefine interface faces
  vector<Patch::FaceType>::const_iterator iter;  
  for (iter  = finePatch->getCoarseFineInterfaceFaces()->begin(); 
       iter != finePatch->getCoarseFineInterfaceFaces()->end(); ++iter){
    Patch::FaceType patchFace = *iter;   
    
    // find the coarse level iterator along the interface
    IntVector c_FC_offset;
    CellIterator c_iter(IntVector(-8,-8,-8),IntVector(-9,-9,-9));
    refluxCoarseLevelIterator( patchFace,coarsePatch, finePatch, fineLevel,c_iter ,c_FC_offset);
    
    // Offset for the fine cell loop (fineStart)
    // shift (+-refinement ratio) for x-, y-, z- finePatch faces
    // shift 0 cells              for x+, y+, z+ finePatchFaces
    
    string name = finePatch->getFaceName(patchFace);
    IntVector offset = finePatch->faceDirection(patchFace);
    IntVector r_Ratio = fineLevel->getRefinementRatio();
    IntVector f_offset(0,0,0);
    double c_FaceNormal = 0;
    double f_FaceNormal = 0;
    
    if(name == "xminus" || name == "yminus" || name == "zminus"){
      f_offset = r_Ratio * -offset;
      c_FaceNormal = +1;
      f_FaceNormal = -1;
    }
    if(name == "xplus" || name == "yplus" || name == "zplus"){
      c_FaceNormal = -1;
      f_FaceNormal = +1;
    } 

    
/*`==========TESTING==========*/
#if SPEW
    cout << name << " l " << l << " h " << h << endl;
    cout << " coarsePatch " << *coarsePatch << endl;
    cout << " finePatch " << *finePatch << endl; 
#endif 
/*===========TESTING==========`*/
    //__________________________________
    // Add fine patch face fluxes to the coarse cells
    // c_CC f_CC:    coarse/fine level cell center index
    // c_FC f_FC:    coarse/fine level face center index
    if(patchFace == Patch::xminus || patchFace == Patch::xplus){    // X+ X-
    
      //__________________________________
      // sum all of the fluxes passing from the 
      // fine level to the coarse level
      for(; !c_iter.done(); c_iter++){
         IntVector c_CC = *c_iter;
         IntVector c_FC = c_CC + c_FC_offset;
         
         T sum_fineLevelFlux(0.0);
         IntVector fineStart = coarseLevel->mapCellToFiner(c_CC) + f_offset;
         IntVector rRatio_X(1,r_Ratio.y(), r_Ratio.z()); 
         
         for(CellIterator inside(IntVector(0,0,0),rRatio_X );!inside.done(); inside++){
           IntVector f_FC = fineStart + *inside;
           sum_fineLevelFlux += Q_X_fine_flux[f_FC];
         }
         // Q_CC = mass * q_CC = cellVol * rho * q_CC
         // coeff accounts for the different cell sizes on the different levels
         double coeff = (double)r_Ratio.x() * r_Ratio.y() * r_Ratio.z();
         double mass_CC_coarse = rho_CC_coarse[c_CC] * coarseCellVol;
         double denominator = switch1 * coarseCellVol     
                            + switch2 * mass_CC_coarse
                            + switch3 * mass_CC_coarse * cv[c_CC];          
                            
         Q_X_coarse_flux[c_FC] = ( c_FaceNormal*Q_X_coarse_flux_org[c_FC] + coeff* f_FaceNormal*sum_fineLevelFlux) /denominator;
         
/*`==========TESTING==========*/
#if SPEW
        if (c_CC.y() == half.y() && c_CC.z() == half.z() ) {
          cout << "refluxOperator: "<< name
               << " \t c_CC " << c_CC  << " c_FC " << c_FC << " q_X_FC " << c_FaceNormal*Q_X_coarse_flux[c_FC]
               << " sum_fineLevelflux " << coeff* f_FaceNormal *sum_fineLevelFlux
               << " correction " << ( c_FaceNormal*Q_X_coarse_flux[c_FC] + coeff* f_FaceNormal *sum_fineLevelFlux) /denominator << endl;
          cout << "" << endl;
        }
#endif 
/*===========TESTING==========`*/
      }
    }
    if(patchFace == Patch::yminus || patchFace == Patch::yplus){    // Y+ Y-
    
      for(; !c_iter.done(); c_iter++){
         IntVector c_CC = *c_iter;
         IntVector c_FC = c_CC + c_FC_offset;
         
         T sum_fineLevelFlux(0.0);
         IntVector fineStart = coarseLevel->mapCellToFiner(c_CC) + f_offset;
         IntVector rRatio_Y(r_Ratio.x(),1, r_Ratio.z()); 
         
         for(CellIterator inside(IntVector(0,0,0),rRatio_Y );!inside.done(); inside++){
           IntVector f_FC = fineStart + *inside;
           sum_fineLevelFlux += Q_Y_fine_flux[f_FC];          
         }
         // Q_CC = mass * q_CC = cellVol * rho * q_CC
         // coeff accounts for the different cell sizes on the different levels
         double coeff = (double)r_Ratio.x() * r_Ratio.y() * r_Ratio.z();
         double mass_CC_coarse = rho_CC_coarse[c_CC] * coarseCellVol;
         double denominator = switch1 * coarseCellVol     
                            + switch2 * mass_CC_coarse
                            + switch3 * mass_CC_coarse * cv[c_CC];
                            
         Q_Y_coarse_flux[c_FC] = (c_FaceNormal*Q_Y_coarse_flux_org[c_FC] + coeff*f_FaceNormal*sum_fineLevelFlux) /denominator;
         
/*`==========TESTING==========*/
#if SPEW
        if (c_CC.x() == half.x() && c_CC.z() == half.z() ) {
          cout << "refluxOperator: " << name 
               << " \t c_CC " << c_CC  << " c_FC " << c_FC << " q_Y_FC " << c_FaceNormal*Q_Y_coarse_flux[c_FC]
               << " sum_fineLevelflux " << coeff* f_FaceNormal *sum_fineLevelFlux
               << " correction " << ( c_FaceNormal*Q_Y_coarse_flux[c_FC] + coeff* f_FaceNormal *sum_fineLevelFlux) /denominator << endl;
          cout << "" << endl;
        }
#endif 
/*===========TESTING==========`*/
      }
    }
    if(patchFace == Patch::zminus || patchFace == Patch::zplus){    // Z+ Z-
    
 
      for(; !c_iter.done(); c_iter++){
         IntVector c_CC = *c_iter;
         IntVector c_FC = c_CC + c_FC_offset;
         
         T sum_fineLevelFlux(0.0);
         IntVector fineStart = coarseLevel->mapCellToFiner(c_CC) + f_offset;
         IntVector rRatio_Z(r_Ratio.x(),r_Ratio.y(), 1); 
         
         for(CellIterator inside(IntVector(0,0,0),rRatio_Z );!inside.done(); inside++){
           IntVector f_FC = fineStart + *inside;
           sum_fineLevelFlux += Q_Z_fine_flux[f_FC];
         }
         // Q_CC = mass * q_CC = cellVol * rho * q_CC
         // coeff accounts for the different cell sizes on the different levels
         double coeff = (double)r_Ratio.x() * r_Ratio.y() * r_Ratio.z();
         double mass_CC_coarse = rho_CC_coarse[c_CC] * coarseCellVol;
         double denominator = switch1 * coarseCellVol     
                            + switch2 * mass_CC_coarse
                            + switch3 * mass_CC_coarse * cv[c_CC];
                            
         Q_Z_coarse_flux[c_FC] = (c_FaceNormal*Q_Z_coarse_flux_org[c_FC] + coeff*f_FaceNormal*sum_fineLevelFlux) /denominator;
         
/*`==========TESTING==========*/
#if SPEW
        if (c_CC.x() == half.x() && c_CC.y() == half.y() ) {
          cout << "refluxOperator: " << name 
               << " \t c_CC " << c_CC  << " c_FC " << c_FC << " q_Z_FC " << c_FaceNormal*Q_Z_coarse_flux[c_FC]
               << " sum_fineLevelflux " << coeff* f_FaceNormal *sum_fineLevelFlux
               << " correction " << ( c_FaceNormal*Q_Z_coarse_flux[c_FC] + coeff* f_FaceNormal *sum_fineLevelFlux) /denominator<< endl;
          cout << "" << endl;
        }
#endif 
/*===========TESTING==========`*/
      }
    }
  }  // coarseFineInterface faces   
}
/*___________________________________________________________________
 Function~  AMRICE::scheduleReflux_applyCorrection--  
_____________________________________________________________________*/
void AMRICE::scheduleReflux_applyCorrection(const LevelP& coarseLevel,
                                            SchedulerP& sched)
{
 
  cout_doing << "AMRICE::scheduleReflux_applyCorrectionFluxes\tL-" 
             << coarseLevel->getIndex() << '\n';
  Task* task = scinew Task("reflux_applyCorrectionFluxes",
                          this, &AMRICE::reflux_applyCorrectionFluxes);
  
  Ghost::GhostType gn = Ghost::None;
    
  //__________________________________
  // coarse grid solution variables after advection               
  task->requires(Task::NewDW, lb->rho_CCLabel,     gn, 0);
  task->requires(Task::NewDW, lb->vel_CCLabel,     gn, 0);
  task->requires(Task::NewDW, lb->sp_vol_CCLabel,  gn, 0);
  task->requires(Task::NewDW, lb->temp_CCLabel,    gn, 0);                         
  //__________________________________
  // Correction fluxes  from the coarse level            
                                      // MASS
  task->requires(Task::NewDW, lb->mass_X_FC_fluxLabel,   gn, 0);
  task->requires(Task::NewDW, lb->mass_Y_FC_fluxLabel,   gn, 0);
  task->requires(Task::NewDW, lb->mass_Z_FC_fluxLabel,   gn, 0);
                                      // MOMENTUM
  task->requires(Task::NewDW, lb->mom_X_FC_fluxLabel,    gn, 0);
  task->requires(Task::NewDW, lb->mom_Y_FC_fluxLabel,    gn, 0);
  task->requires(Task::NewDW, lb->mom_Z_FC_fluxLabel,    gn, 0);
                                      // INT_ENG
  task->requires(Task::NewDW, lb->int_eng_X_FC_fluxLabel,  gn, 0);
  task->requires(Task::NewDW, lb->int_eng_Y_FC_fluxLabel,  gn, 0);
  task->requires(Task::NewDW, lb->int_eng_Z_FC_fluxLabel,  gn, 0);
                                      // SPECIFIC VOLUME
  task->requires(Task::NewDW, lb->sp_vol_X_FC_fluxLabel,   gn, 0);
  task->requires(Task::NewDW, lb->sp_vol_Y_FC_fluxLabel,   gn, 0);
  task->requires(Task::NewDW, lb->sp_vol_Z_FC_fluxLabel,   gn, 0);             


  //__________________________________
  // Model Variables.
  if(d_modelSetup && d_modelSetup->d_reflux_vars.size() > 0){
    vector<AMR_refluxVariable*>::iterator iter;
    for( iter  = d_modelSetup->d_reflux_vars.begin();
         iter != d_modelSetup->d_reflux_vars.end(); iter++){
      AMR_refluxVariable* rvar = *iter;
      
      task->requires(Task::NewDW, rvar->var_CC,  gn, 0);
      task->modifies(rvar->var_CC);
    }
  }

  task->modifies(lb->rho_CCLabel);
  task->modifies(lb->sp_vol_CCLabel);
  task->modifies(lb->temp_CCLabel);
  task->modifies(lb->vel_CCLabel);

  sched->addTask(task, coarseLevel->eachPatch(), d_sharedState->allMaterials()); 
}
/*___________________________________________________________________
 Function~  AMRICE::Reflux_applyCorrectionFluxes--  
_____________________________________________________________________*/
void AMRICE::reflux_applyCorrectionFluxes(const ProcessorGroup*,
                                          const PatchSubset* coarsePatches,
                                          const MaterialSubset* matls,
                                          DataWarehouse*,
                                          DataWarehouse* new_dw)
{
  const Level* coarseLevel = getLevel(coarsePatches);
  const Level* fineLevel = coarseLevel->getFinerLevel().get_rep();
  
  cout_doing << "Doing reflux_applyCorrectionFluxes \t\t\t AMRICE L-"
             <<coarseLevel->getIndex()<< endl;
  
  bool dbg_onOff = cout_dbg.active();      // is cout_dbg switch on or off
  
  for(int c_p=0;c_p<coarsePatches->size();c_p++){  
    const Patch* coarsePatch = coarsePatches->get(c_p);

    for(int m = 0;m<matls->size();m++){
      int indx = matls->get(m);     
      CCVariable<double> rho_CC, temp, sp_vol_CC;
      constCCVariable<double> cv;
      CCVariable<Vector> vel_CC;

      Ghost::GhostType  gn  = Ghost::None;
      new_dw->getModifiable(rho_CC,   lb->rho_CCLabel,    indx, coarsePatch);
      new_dw->getModifiable(sp_vol_CC,lb->sp_vol_CCLabel, indx, coarsePatch);
      new_dw->getModifiable(temp,     lb->temp_CCLabel,   indx, coarsePatch);
      new_dw->getModifiable(vel_CC,   lb->vel_CCLabel,    indx, coarsePatch);
      new_dw->get(cv,                 lb->specific_heatLabel,indx,coarsePatch, gn,0);
      
      Level::selectType finePatches;
      coarsePatch->getFineLevelPatches(finePatches);
      
      for(int i=0; i < finePatches.size();i++){  
        const Patch* finePatch = finePatches[i];        
//      cout_doing << " coarsePatch " << coarsePatch->getID() <<" finepatch " << finePatch->getID()<< endl;

        //__________________________________
        // reflux
        refluxOperator_applyCorrectionFluxes<double>(rho_CC,    "mass",  indx, 
                      coarsePatch, finePatch, coarseLevel, fineLevel,new_dw);
                               
        refluxOperator_applyCorrectionFluxes<double>(sp_vol_CC, "sp_vol",  indx, 
                      coarsePatch, finePatch, coarseLevel, fineLevel,new_dw);
                               
        refluxOperator_applyCorrectionFluxes<Vector>(vel_CC, "mom",     indx, 
                      coarsePatch, finePatch, coarseLevel, fineLevel,new_dw);

        refluxOperator_applyCorrectionFluxes<double>(temp,  "int_eng", indx, 
                      coarsePatch, finePatch, coarseLevel, fineLevel,new_dw);
        //__________________________________
        //    Model Variables
        if(d_modelSetup && d_modelSetup->d_reflux_vars.size() > 0){
          vector<AMR_refluxVariable*>::iterator iter;
          for( iter  = d_modelSetup->d_reflux_vars.begin();
               iter != d_modelSetup->d_reflux_vars.end(); iter++){
            AMR_refluxVariable* r_var = *iter;
             
            if(r_var->matls->contains(indx)){
              CCVariable<double> q_CC;
              string var_name = r_var->var_CC->getName();
              new_dw->getModifiable(q_CC,  r_var->var_CC, indx, coarsePatch);
              
              refluxOperator_applyCorrectionFluxes<double>(q_CC, var_name, indx, 
                            coarsePatch, finePatch, coarseLevel, fineLevel,new_dw);
            
              if(switchDebug_AMR_refine){
                string name = r_var->var_CC->getName();
                printData(indx, coarsePatch, 1, "coarsen_models", name, q_CC);
              }
            }
          }
        }
      }  // finePatch loop 
  
      //__________________________________
      //  Print Data
      if(switchDebug_AMR_reflux){ 
        ostringstream desc;     
        desc << "Reflux_applyCorrection_Mat_" << indx << "_patch_"<< coarsePatch->getID();
        printData(indx, coarsePatch,   1, desc.str(), "rho_CC",   rho_CC);
        printData(indx, coarsePatch,   1, desc.str(), "sp_vol_CC",sp_vol_CC);
        printData(indx, coarsePatch,   1, desc.str(), "Temp_CC",  temp);
        printVector(indx, coarsePatch, 1, desc.str(), "vel_CC", 0,vel_CC);
      }
    }  // matl loop
  }  // course patch loop 
  cout_dbg.setActive(dbg_onOff);  // reset on/off switch for cout_dbg
}

/*_____________________________________________________________________
 Function~  AMRICE::refluxOperator_applyCorrectionFluxes
 Purpose~   
_____________________________________________________________________*/
template<class T>
void AMRICE::refluxOperator_applyCorrectionFluxes(                             
                              CCVariable<T>& q_CC_coarse,
                              const string& varLabel,
                              const int indx,
                              const Patch* coarsePatch,
                              const Patch* finePatch,
                              const Level* coarseLevel,
                              const Level* fineLevel,
                              DataWarehouse* new_dw)
{
  // form the fine patch flux label names
  string x_name = varLabel + "_X_FC_flux";
  string y_name = varLabel + "_Y_FC_flux";
  string z_name = varLabel + "_Z_FC_flux";
  
  // grab the varLabels
  VarLabel* xlabel = VarLabel::find(x_name);
  VarLabel* ylabel = VarLabel::find(y_name);
  VarLabel* zlabel = VarLabel::find(z_name);  

  if(xlabel == NULL || ylabel == NULL || zlabel == NULL){
    throw InternalError( "refluxOperator_applyCorrectionFluxes: variable label not found: " 
                          + x_name + " or " + y_name + " or " + z_name);
  }
  constSFCXVariable<T>  Q_X_coarse_flux;
  constSFCYVariable<T>  Q_Y_coarse_flux;
  constSFCZVariable<T>  Q_Z_coarse_flux;
  
  Ghost::GhostType  gac = Ghost::AroundCells;
  new_dw->get(Q_X_coarse_flux,  xlabel,indx, coarsePatch, gac,1);
  new_dw->get(Q_Y_coarse_flux,  ylabel,indx, coarsePatch, gac,1);
  new_dw->get(Q_Z_coarse_flux,  zlabel,indx, coarsePatch, gac,1); 
  
  //__________________________________
  // Iterate over coarsefine interface faces
  vector<Patch::FaceType>::const_iterator iter;  
  for (iter  = finePatch->getCoarseFineInterfaceFaces()->begin(); 
       iter != finePatch->getCoarseFineInterfaceFaces()->end(); ++iter){
    Patch::FaceType patchFace = *iter;
 
    // determine the iterator for the coarse level.
    IntVector c_FC_offset;
    CellIterator c_iter(IntVector(-8,-8,-8),IntVector(-9,-9,-9));
    refluxCoarseLevelIterator( patchFace,coarsePatch, finePatch, fineLevel,c_iter ,c_FC_offset);

#if 0    
    cout << name << " l " << l << " h " << h << endl;
    cout << " coarsePatch " << *coarsePatch << endl;
    cout << " finePatch " << *finePatch << endl; 
#endif    
    
    //__________________________________
    // Add fine patch face fluxes correction to the coarse cells
    // c_CC:    coarse level cell center index
    // c_FC:    coarse level face center index
    if(patchFace == Patch::xminus || patchFace == Patch::xplus){
      for(; !c_iter.done(); c_iter++){
         IntVector c_CC = *c_iter;
         IntVector c_FC = c_CC + c_FC_offset;                
         q_CC_coarse[c_CC] += Q_X_coarse_flux[c_FC];
      }
    }
    if(patchFace == Patch::yminus || patchFace == Patch::yplus){
      for(; !c_iter.done(); c_iter++){
         IntVector c_CC = *c_iter;
         IntVector c_FC = c_CC + c_FC_offset;
         q_CC_coarse[c_CC] += Q_Y_coarse_flux[c_FC];
      }
    }
    if(patchFace == Patch::zminus || patchFace == Patch::zplus){
      for(; !c_iter.done(); c_iter++){
         IntVector c_CC = *c_iter;
         IntVector c_FC = c_CC + c_FC_offset;             
         q_CC_coarse[c_CC] += Q_Z_coarse_flux[c_FC];
      }
    }
  }  // coarseFineInterface faces
} 
/*_____________________________________________________________________
 Function~  AMRICE::scheduleInitialErrorEstimate--
______________________________________________________________________*/
void AMRICE::scheduleInitialErrorEstimate(const LevelP& /*coarseLevel*/,
                                          SchedulerP& /*sched*/)
{
#if 0
  scheduleErrorEstimate(coarseLevel, sched);
  
  //__________________________________
  // Models
  for(vector<ModelInterface*>::iterator iter = d_models.begin();
     iter != d_models.end(); iter++){
    (*iter)->scheduleErrorEstimate(coarseLevel, sched);;
  }
#endif
}

/*_____________________________________________________________________
 Function~  AMRICE::scheduleErrorEstimate--
______________________________________________________________________*/
void AMRICE::scheduleErrorEstimate(const LevelP& coarseLevel,
                                   SchedulerP& sched)
{
  if(!coarseLevel->hasFinerLevel() || !doICEOnThisLevel(coarseLevel->getFinerLevel()))
    return;
    
  cout_doing << "AMRICE::scheduleErrorEstimate \t\t\t\tL-" 
             << coarseLevel->getIndex() << '\n';
  
  Task* t = scinew Task("AMRICE::errorEstimate", 
                  this, &AMRICE::errorEstimate, false);  
  
  Ghost::GhostType  gac  = Ghost::AroundCells; 
  Task::DomainSpec oims = Task::OutOfDomain;  //outside of ice matlSet.
                  
  t->requires(Task::NewDW, lb->rho_CCLabel,       gac, 1);
  t->requires(Task::NewDW, lb->temp_CCLabel,      gac, 1);
  t->requires(Task::NewDW, lb->vel_CCLabel,       gac, 1);
  t->requires(Task::NewDW, lb->vol_frac_CCLabel,  gac, 1);
  t->requires(Task::NewDW, lb->press_CCLabel,    d_press_matl,oims,gac, 1);
  
  t->computes(lb->rho_CC_gradLabel);
  t->computes(lb->temp_CC_gradLabel);
  t->computes(lb->vel_CC_mag_gradLabel);
  t->computes(lb->vol_frac_CC_gradLabel);
  t->computes(lb->press_CC_gradLabel);
  
  t->modifies(d_sharedState->get_refineFlag_label(), d_sharedState->refineFlagMaterials());
  t->modifies(d_sharedState->get_refinePatchFlag_label(), d_sharedState->refineFlagMaterials());
  
  sched->addTask(t, coarseLevel->eachPatch(), d_sharedState->allMaterials());
  
  //__________________________________
  // Models
  for(vector<ModelInterface*>::iterator iter = d_models.begin();
     iter != d_models.end(); iter++){
    (*iter)->scheduleErrorEstimate(coarseLevel, sched);;
  }
}
/*_____________________________________________________________________ 
Function~  AMRICE::compute_q_CC_gradient--
Purpose~   computes the gradient of q_CC in each direction.
           First order central difference.
______________________________________________________________________*/
void AMRICE::compute_q_CC_gradient( constCCVariable<double>& q_CC,
                                    CCVariable<Vector>& q_CC_grad,
                                    const Patch* patch) 
{                  
  Vector dx = patch->dCell(); 
      
  for(int dir = 0; dir <3; dir ++ ) { 
    double inv_dx = 0.5 /dx[dir];
    for(CellIterator iter = patch->getCellIterator();!iter.done();iter++){
        IntVector c = *iter;
        IntVector r = c;
        IntVector l = c;
        r[dir] += 1;
        l[dir] -= 1;
        q_CC_grad[c][dir] = (q_CC[r] - q_CC[l])*inv_dx;
    }
  }
}
//______________________________________________________________________
//          vector version
void AMRICE::compute_q_CC_gradient( constCCVariable<Vector>& q_CC,
                                    CCVariable<Vector>& q_CC_grad,
                                    const Patch* patch) 
{                  
  Vector dx = patch->dCell(); 
  
  //__________________________________
  // Vectors:  take the gradient of the magnitude
  for(int dir = 0; dir <3; dir ++ ) { 
    double inv_dx = 0.5 /dx[dir];
    for(CellIterator iter = patch->getCellIterator();!iter.done();iter++){
        IntVector c = *iter;
        IntVector r = c;
        IntVector l = c;
        r[dir] += 1;
        l[dir] -= 1;
        q_CC_grad[c][dir] = (q_CC[r].length() - q_CC[l].length())*inv_dx;
    }
  }
}
/*_____________________________________________________________________
 Function~  AMRICE::set_refinementFlags
______________________________________________________________________*/         
void AMRICE::set_refineFlags( CCVariable<Vector>& q_CC_grad,
                              double threshold,
                              CCVariable<int>& refineFlag,
                              PerPatch<PatchFlagP>& refinePatchFlag,
                              const Patch* patch) 
{                  
  PatchFlag* refinePatch = refinePatchFlag.get().get_rep();
  for(CellIterator iter = patch->getCellIterator();!iter.done();iter++){
    IntVector c = *iter;
    if( q_CC_grad[c].length() > threshold){
      refineFlag[c] = true;
      refinePatch->set();
    }
  }
}
/*_____________________________________________________________________
 Function~  AMRICE::errorEstimate--
______________________________________________________________________*/
void
AMRICE::errorEstimate(const ProcessorGroup*,
                      const PatchSubset* patches,
                      const MaterialSubset* /*matls*/,
                      DataWarehouse*,
                      DataWarehouse* new_dw,
                      bool /*initial*/)
{
  const Level* level = getLevel(patches);
  cout_doing << "Doing errorEstimate \t\t\t\t\t AMRICE L-"<< level->getIndex();
  for(int p=0;p<patches->size();p++){
    const Patch* patch = patches->get(p);
    
    cout_doing << " patch " << patch->getID()<< endl;
    Ghost::GhostType  gac  = Ghost::AroundCells;
    const VarLabel* refineFlagLabel = d_sharedState->get_refineFlag_label();
    const VarLabel* refinePatchLabel= d_sharedState->get_refinePatchFlag_label();
    
    CCVariable<int> refineFlag;
    new_dw->getModifiable(refineFlag, refineFlagLabel, 0, patch);      

    PerPatch<PatchFlagP> refinePatchFlag;
    new_dw->get(refinePatchFlag, refinePatchLabel, 0, patch);


    //__________________________________
    //  PRESSURE       --- just computes the gradient
    //  I still need to figure out how 
    //  set the refinement flags for this index 0
    //  and then do it again in the matl loop below
    constCCVariable<double> press_CC;
    CCVariable<Vector> press_CC_grad;
    
    new_dw->get(press_CC, lb->press_CCLabel,    0,patch,gac,1);
    new_dw->allocateAndPut(press_CC_grad,
                       lb->press_CC_gradLabel,  0,patch);
    
    compute_q_CC_gradient(press_CC, press_CC_grad, patch);
    set_refineFlags( press_CC_grad, d_press_threshold,refineFlag, 
                            refinePatchFlag, patch);
    //__________________________________
    //  RHO, TEMP, VEL_CC, VOL_FRAC
    int numICEMatls = d_sharedState->getNumICEMatls();
    for(int m=0;m < numICEMatls;m++){
      Material* matl = d_sharedState->getICEMaterial( m );
      int indx = matl->getDWIndex();
              
      constCCVariable<double> rho_CC, temp_CC, vol_frac_CC;
      constCCVariable<Vector> vel_CC;
      CCVariable<Vector> rho_CC_grad, temp_CC_grad; 
      CCVariable<Vector> vel_CC_mag_grad, vol_frac_CC_grad;
      
      new_dw->get(rho_CC,      lb->rho_CCLabel,      indx,patch,gac,1);
      new_dw->get(temp_CC,     lb->temp_CCLabel,     indx,patch,gac,1);
      new_dw->get(vel_CC,      lb->vel_CCLabel,      indx,patch,gac,1);
      new_dw->get(vol_frac_CC, lb->vol_frac_CCLabel, indx,patch,gac,1);

      new_dw->allocateAndPut(rho_CC_grad,     
                         lb->rho_CC_gradLabel,     indx,patch);
      new_dw->allocateAndPut(temp_CC_grad,    
                         lb->temp_CC_gradLabel,    indx,patch);
      new_dw->allocateAndPut(vel_CC_mag_grad, 
                         lb->vel_CC_mag_gradLabel, indx,patch);
      new_dw->allocateAndPut(vol_frac_CC_grad,
                         lb->vol_frac_CC_gradLabel,indx,patch);
      
      //__________________________________
      // compute the gradients and set the refinement flags
                                        // Density
      compute_q_CC_gradient(rho_CC,      rho_CC_grad,      patch); 
      set_refineFlags( rho_CC_grad,     d_rho_threshold,refineFlag, 
                            refinePatchFlag, patch);
      
                                        // Temperature
      compute_q_CC_gradient(temp_CC,     temp_CC_grad,     patch); 
      set_refineFlags( temp_CC_grad,    d_temp_threshold,refineFlag, 
                            refinePatchFlag, patch);
      
                                        // Vol Fraction
      compute_q_CC_gradient(vol_frac_CC, vol_frac_CC_grad, patch); 
      set_refineFlags( vol_frac_CC_grad, d_vol_frac_threshold,refineFlag, 
                            refinePatchFlag, patch);
      
                                        // Velocity
      compute_q_CC_gradient(vel_CC,      vel_CC_mag_grad,  patch); 
      set_refineFlags( vel_CC_mag_grad, d_vel_threshold,refineFlag, 
                            refinePatchFlag, patch);
    }  // matls
    
    //______________________________________________________________________
    //    Hardcoding to move the error flags around every nTimeSteps
    if(d_regridderTest){
      double nTimeSteps = 5;  // how ofter to move the error flags
      const Level* level = getLevel(patches);
      int dw = d_sharedState->getCurrentTopLevelTimeStep();
      double timeToMove = fmod((double)dw, nTimeSteps);
      static int counter = 0;

      IntVector lo = patch->getInteriorCellLowIndex();
      IntVector hi = patch->getInteriorCellHighIndex() - IntVector(1,1,1);

      if (level->getIndex() == 0 ){
        //__________________________________
        // counter to move the error flag around
        if(timeToMove == 0){
          counter += 1;
          if (counter == 8) {
            counter = 0;
          }
        }
        //__________________________________
        //  find the 8 corner cells of level 0
        vector<IntVector> corners;

        for(int k = 0; k< 2; k++){
          for(int j = 0; j< 2; j++){
            for(int i = 0; i< 2; i++){
              int x = (i) * lo.x() + (1-i)*hi.x();
              int y = (j) * lo.y() + (1-j)*hi.y();
              int z = (k) * lo.z() + (1-k)*hi.z();
              corners.push_back(IntVector(x,y,z));     
            }
          }
        }
        if(timeToMove == 0){
          cout << "RegridderTest:  moving the error flag to "
               << corners[counter]<< " on level " 
               << level->getIndex() <<endl; 
        }
        //__________________________________
        //  Set the refinement flag      
        PatchFlag* refinePatch = refinePatchFlag.get().get_rep();
        refineFlag[corners[counter]] = true;
        refinePatch->set();
      }
      
      //__________________________________
      //  Levels other than 0
      if(level->getIndex() && timeToMove == 0){
        Vector randNum(drand48());
        
        IntVector diff = hi - lo;
        Vector here = randNum * diff.asVector();
        int i = RoundUp(here.x());
        int j = RoundUp(here.y());
        int k = RoundUp(here.z());
        
        IntVector twk(i,j,k);
        IntVector c = twk + lo;
        
        c = Max(c, lo+IntVector(2,2,2));
        c = Min(c, hi-IntVector(2,2,2));
        
        cout << "RegridderTest:  moving the error flag to "
               << c << " on level " 
               << level->getIndex() <<endl;
    
        //  Set the refinement flag      
        PatchFlag* refinePatch = refinePatchFlag.get().get_rep();
        refineFlag[c] = true;
        refinePatch->set();        
      }
    }  // regridderTest
    
  }  // patches
}
