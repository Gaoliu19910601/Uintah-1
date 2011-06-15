/*

The MIT License

Copyright (c) 1997-2010 Center for the Simulation of Accidental Fires and 
Explosions (CSAFE), and  Scientific Computing and Imaging Institute (SCI), 
University of Utah.

License for the specific language governing rights and limitations under
Permission is hereby granted, free of charge, to any person obtaining a 
copy of this software and associated documentation files (the "Software"),
to deal in the Software without restriction, including without limitation 
the rights to use, copy, modify, merge, publish, distribute, sublicense, 
and/or sell copies of the Software, and to permit persons to whom the 
Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included 
in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS 
OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, 
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL 
THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER 
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING 
FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER 
DEALINGS IN THE SOFTWARE.

*/


//----- ExplicitSolver.h -----------------------------------------------

#ifndef Uintah_Component_Arches_ExplicitSolver_h
#define Uintah_Component_Arches_ExplicitSolver_h

#include <CCA/Components/Arches/Arches.h>
#include <CCA/Components/Arches/NonlinearSolver.h>
#include <CCA/Ports/DataWarehouseP.h>

namespace Uintah {

/** 
  @class  ExplicitSolver
  @author Rajesh Rawat, Stanislav Borodai, Jeremy Thornock
  @date   March 2000
  
  @brief  Subclass of NonlinearSolver class implementing forward Euler, RK2, and RK3 methods.
  
  */

class PressureSolver;
class MomentumSolver;
class ScalarSolver;
class ReactiveScalarSolver; 
class TurbulenceModel;
class ScaleSimilarityModel;
class Properties;
class BoundaryCondition;
class PhysicalConstants;
class EnthalpySolver;
class RMCRTRadiationModel; 
class PartVel; 
class DQMOM; 
class ExplicitSolver: public NonlinearSolver {

public:
  
  // GROUP: Constructors:
  ////////////////////////////////////////////////////////////////////////
  // Solver initialized with all input data 
  ExplicitSolver(ArchesLabel* label,
                 const MPMArchesLabel* MAlb,
                 Properties* props, 
                 BoundaryCondition* bc,
                 TurbulenceModel* turbModel, 
                 ScaleSimilarityModel* scaleSimilarityModel, 
                 PhysicalConstants* physConst,
                 bool calcScalar,
                 bool calcReactscalar,
                 bool calcEnthalpy,
                 bool calcVariance,
                 const ProcessorGroup* myworld);

  // GROUP: Destructors:
  ////////////////////////////////////////////////////////////////////////
  // Virtual destructor for ExplicitSolver.
  virtual ~ExplicitSolver();


  // GROUP: Problem Setup :
  ///////////////////////////////////////////////////////////////////////
  // Set up the problem specification database
  virtual void problemSetup(const ProblemSpecP& input_db);

  // GROUP: Schedule Action :
  ///////////////////////////////////////////////////////////////////////
  // Solve the nonlinear system. (also does some actual computations)
  // The code returns 0 if there are no errors and
  // 1 if there is a nonlinear failure.
  //    [in] 
  //        documentation here
  //    [out] 
  //        documentation here
  virtual int nonlinearSolve(const LevelP& level,
                             SchedulerP& sched);


  ///////////////////////////////////////////////////////////////////////
  // Do not solve the nonlinear system but just copy variables to end
  // so that they retain the right guess for the next step

  virtual int noSolve(const LevelP& level,
                      SchedulerP& sched);

  ///////////////////////////////////////////////////////////////////////
  // Schedule the Initialization of non linear solver
  //    [in] 
  //        data User data needed for solve 
  void sched_setInitialGuess(SchedulerP&, 
                             const PatchSet* patches,
                             const MaterialSet* matls);

  ///////////////////////////////////////////////////////////////////////
  // Schedule dummy solve (data copy) for first time step of MPMArches
  // to overcome scheduler limitation on getting pset from old_dw

  void sched_dummySolve(SchedulerP& sched,
                        const PatchSet* patches,
                        const MaterialSet* matls);

  ///////////////////////////////////////////////////////////////////////
  // Schedule the interpolation of velocities from Face Centered Variables
  //    to a Cell Centered Vector
  //    [in] 
  void sched_interpolateFromFCToCC(SchedulerP&, 
                                   const PatchSet* patches,
                                   const MaterialSet* matls,
                                   const TimeIntegratorLabel* timelabels);

  void sched_probeData(SchedulerP&, 
                       const PatchSet* patches,
                       const MaterialSet* matls);

  // GROUP: Action Computations :
  void sched_printTotalKE(SchedulerP& sched,
                          const PatchSet* patches,
                          const MaterialSet* matls,
                          const TimeIntegratorLabel* timelabels);

  void sched_updatePressure(SchedulerP& sched,
                          const PatchSet* patches,
                          const MaterialSet* matls,
                          const TimeIntegratorLabel* timelabels);

  void sched_saveTempCopies(SchedulerP&, 
                            const PatchSet* patches,
                            const MaterialSet* matls,
                            const TimeIntegratorLabel* timelabels);

  void sched_saveFECopies(SchedulerP&, 
                          const PatchSet* patches,
                          const MaterialSet* matls,
                          const TimeIntegratorLabel* timelabels);

  void sched_computeDensityLag(SchedulerP&, 
                               const PatchSet* patches,
                               const MaterialSet* matls,
                               const TimeIntegratorLabel* timelabels,
                               bool after_average);

  void sched_getDensityGuess(SchedulerP&, 
                             const PatchSet* patches,
                             const MaterialSet* matls,
                             const TimeIntegratorLabel* timelabels,
                             bool EKTCorrection, 
                             bool doing_EKT_now);

  void sched_checkDensityGuess(SchedulerP&, 
                               const PatchSet* patches,
                               const MaterialSet* matls,
                               const TimeIntegratorLabel* timelabels,
                               bool EKTCorrection, 
                               bool doing_EKT_now);

  void sched_checkDensityLag(SchedulerP&, 
                             const PatchSet* patches,
                             const MaterialSet* matls,
                             const TimeIntegratorLabel* timelabels,
                             bool after_average);

  void sched_updateDensityGuess(SchedulerP&, 
                                const PatchSet* patches,
                                const MaterialSet* matls,
                                const TimeIntegratorLabel* timelabels);

  void sched_syncRhoF(SchedulerP&, 
                      const PatchSet* patches,
                      const MaterialSet* matls,
                      const TimeIntegratorLabel* timelabels);

  void sched_computeMMSError(SchedulerP&, 
                             const PatchSet* patches,
                             const MaterialSet* matls,
                             const TimeIntegratorLabel* timelabels);

  inline double recomputeTimestep(double current_dt) {
    return current_dt/2;
  }

  inline bool restartableTimesteps() {
    return true;
  }

  inline double getAdiabaticAirEnthalpy() const{
    return d_H_air;
  }
  inline void setMMS(bool doMMS) {
    d_doMMS=doMMS;
  }
  inline bool getMMS() const {
    return d_doMMS;
  }
  inline void setExtraProjection(bool extraProjection) {
    d_extraProjection=extraProjection;
  }
  inline void setEKTCorrection(bool EKTCorrection) {
    d_EKTCorrection=EKTCorrection;
  }
  inline void setNumSourceBoundaries(int numSourceBoundaries){
    d_numSourceBoundaries = numSourceBoundaries;
  }

protected :

private:

  // GROUP: Constructors (private):
  ////////////////////////////////////////////////////////////////////////
  // Should never be used
  ExplicitSolver();

  // GROUP: Action Methods (private) :
  ///////////////////////////////////////////////////////////////////////
  // Actually Initialize the non linear solver
  //    [in] 
  //        data User data needed for solve 
  void setInitialGuess(const ProcessorGroup* pc,
                       const PatchSubset* patches,
                       const MaterialSubset* matls,
                       DataWarehouse* old_dw,
                       DataWarehouse* new_dw);

  ///////////////////////////////////////////////////////////////////////
  // actual data copy for first time step of MPMArches to overcome
  // scheduler limitation on getting pset from old_dw

  void dummySolve(const ProcessorGroup* pc,
                  const PatchSubset* patches,
                  const MaterialSubset* matls,
                  DataWarehouse* old_dw,
                  DataWarehouse* new_dw);

  ///////////////////////////////////////////////////////////////////////
  // Actually Interpolate from SFCX, SFCY, SFCZ to CC<Vector>
  //    [in] 
  void interpolateFromFCToCC(const ProcessorGroup* pc,
                             const PatchSubset* patches,
                             const MaterialSubset* matls,
                             DataWarehouse* old_dw,
                             DataWarehouse* new_dw,
                             const TimeIntegratorLabel* timelabels);

  void computeVorticity(const ProcessorGroup* pc,
                        const PatchSubset* patches,
                        const MaterialSubset* matls,
                        DataWarehouse* old_dw,
                        DataWarehouse* new_dw,
                        const TimeIntegratorLabel* timelabels);

  void probeData(const ProcessorGroup* pc,
                 const PatchSubset* patches,
                 const MaterialSubset* matls,
                 DataWarehouse* old_dw,
                 DataWarehouse* new_dw);

  void printTotalKE(const ProcessorGroup* ,
                    const PatchSubset* patches,
                    const MaterialSubset*,
                    DataWarehouse*,
                    DataWarehouse* new_dw,
                    const TimeIntegratorLabel* timelabels);

  void updatePressure(const ProcessorGroup* ,
                      const PatchSubset* patches,
                      const MaterialSubset*,
                      DataWarehouse* old_dw,
                      DataWarehouse* new_dw,
                      const TimeIntegratorLabel* timelabels);

  void saveTempCopies(const ProcessorGroup*,
                      const PatchSubset* patches,
                      const MaterialSubset* matls,
                      DataWarehouse* old_dw,
                      DataWarehouse* new_dw,
                      const TimeIntegratorLabel* timelabels);

  void saveFECopies(const ProcessorGroup*,
                      const PatchSubset* patches,
                      const MaterialSubset* matls,
                      DataWarehouse* old_dw,
                      DataWarehouse* new_dw,
                      const TimeIntegratorLabel* timelabels);

  void computeDensityLag(const ProcessorGroup*,
                      const PatchSubset* patches,
                      const MaterialSubset* matls,
                      DataWarehouse* old_dw,
                      DataWarehouse* new_dw,
                      const TimeIntegratorLabel* timelabels,
                      bool after_average);

  void getDensityGuess(const ProcessorGroup*,
                      const PatchSubset* patches,
                      const MaterialSubset* matls,
                      DataWarehouse* old_dw,
                      DataWarehouse* new_dw,
                      const TimeIntegratorLabel* timelabels,
                      bool EKTCorrection,
                      bool doing_EKT_now);

  void checkDensityGuess(const ProcessorGroup*,
                      const PatchSubset* patches,
                      const MaterialSubset* matls,
                      DataWarehouse* old_dw,
                      DataWarehouse* new_dw,
                      const TimeIntegratorLabel* timelabels,
                      bool EKTCorrection,
                      bool doing_EKT_now);

  void checkDensityLag(const ProcessorGroup*,
                      const PatchSubset* patches,
                      const MaterialSubset* matls,
                      DataWarehouse* old_dw,
                      DataWarehouse* new_dw,
                      const TimeIntegratorLabel* timelabels,
                      bool after_average);

  void updateDensityGuess(const ProcessorGroup*,
                      const PatchSubset* patches,
                      const MaterialSubset* matls,
                      DataWarehouse* old_dw,
                      DataWarehouse* new_dw,
                      const TimeIntegratorLabel* timelabels);

  void syncRhoF(const ProcessorGroup*,
                      const PatchSubset* patches,
                      const MaterialSubset* matls,
                      DataWarehouse* old_dw,
                      DataWarehouse* new_dw,
                      const TimeIntegratorLabel* timelabels);

  void computeMMSError(const ProcessorGroup*,
                      const PatchSubset* patches,
                      const MaterialSubset* matls,
                      DataWarehouse* old_dw,
                      DataWarehouse* new_dw,
                      const TimeIntegratorLabel* timelabels);

  //void setDQMOMSolver( DQMOM* dqmomSolver ) { 
  //  d_dqmomSolver = dqmomSolver; }; 

private:
  // const VarLabel*
  ArchesLabel* d_lab;
  const MPMArchesLabel* d_MAlab;
  // generation variable for DataWarehouse creation

  // Total number of nonlinear iterates
  int d_nonlinear_its;
  // for probing data for debuging or plotting
  bool d_probe_data;
  // properties...solves density, temperature and specie concentrations
  Properties* d_props;
  // Boundary conditions
  BoundaryCondition* d_boundaryCondition;
  // Turbulence Model
  TurbulenceModel* d_turbModel;
  ScaleSimilarityModel* d_scaleSimilarityModel;
  bool d_mixedModel;

  bool d_calScalar;
  bool d_reactingScalarSolve;
  bool d_enthalpySolve;
  bool d_calcVariance;
  vector<IntVector> d_probePoints;
  // Pressure Eqn Solver
  PressureSolver* d_pressSolver;
  // Momentum Eqn Solver 
  MomentumSolver* d_momSolver;
  // Scalar solver
  ScalarSolver* d_scalarSolver;
  // reacting scalar solver
  ReactiveScalarSolver* d_reactingScalarSolver;
  // physcial constatns
  PhysicalConstants* d_physicalConsts;

  std::vector<TimeIntegratorLabel* > d_timeIntegratorLabels;
  TimeIntegratorLabel* nosolve_timelabels;
  int numTimeIntegratorLevels;
  bool nosolve_timelabels_allocated;
  bool d_pressure_correction;
  bool d_3d_periodic;
  bool d_dynScalarModel;
  int d_turbModelCalcFreq;
  bool d_turbModelRKsteps;
  int d_turbCounter;
  double d_H_air;
  bool d_doMMS;
  bool d_restart_on_negative_density_guess;
  bool d_noisyDensityGuess; 
  string d_mms;
  string d_mmsErrorType;
  double d_airDensity, d_heDensity;
  Vector d_gravity;
  double d_viscosity;

  bool d_extraProjection;
  bool d_EKTCorrection;
  bool d_KE_fromFC;
  double d_maxDensityLag;

  //linear mms
  double cu, cv, cw, cp, phi0;
  // sine mms
  double amp;
  bool d_calcExtraScalars;
  vector<ExtraScalarSolver*>* d_extraScalars;

  bool d_carbon_balance_es;
  bool d_sulfur_balance_es;
  bool d_turbinlet;
  int d_numSourceBoundaries;

  //RMCRT Stuff
  bool d_standAloneRMCRT; 
  RMCRTRadiationModel* d_RMCRTRadiationModel;  


}; // End class ExplicitSolver
} // End namespace Uintah


#endif


