#ifndef Uintah_Component_Arches_TransportEquationBase_h
#define Uintah_Component_Arches_TransportEquationBase_h
#include <Core/ProblemSpec/ProblemSpec.h>
#include <CCA/Ports/Scheduler.h>
#include <Core/Grid/SimulationState.h>
#include <Core/Grid/Variables/VarTypes.h>
#include <CCA/Components/Arches/BoundaryCond_new.h>
#include <CCA/Components/Arches/ExplicitTimeInt.h>
#include <CCA/Components/Arches/TransportEqns/Discretization_new.h>
#include <CCA/Components/Arches/ArchesMaterial.h>

#define YDIM
#define ZDIM

//========================================================================

/** 
* @class TransportEquationBase
* @author Jeremy Thornock
* @date Oct 16, 2008
*
* @brief A base class for a transport equations.
*
*/

namespace Uintah {
class ArchesLabel; 
class BoundaryCondition_new;
class Discretization_new; 
class ExplicitTimeInt;  
class EqnBase{

public:

  EqnBase( ArchesLabel* fieldLabels, ExplicitTimeInt* timeIntegrator, string eqnName );

  virtual ~EqnBase();

  /** @brief Set any parameters from input file, initialize any constants, etc.. */
  virtual void problemSetup(const ProblemSpecP& inputdb) = 0;
  virtual void problemSetup(const ProblemSpecP& inputdb, int qn) = 0;

  /** @brief Creates instances of variables in the new_dw at the begining of the timestep 
             and copies old data into the new variable */
  virtual void sched_initializeVariables( const LevelP&, SchedulerP& sched ) = 0;
  
  /** @brief Schedule a transport equation to be built and solved */
  virtual void sched_evalTransportEqn( const LevelP&, 
                                       SchedulerP& sched, int timeSubStep ) = 0; 

  /** @brief Build the terms needed in the transport equation */
  virtual void sched_buildTransportEqn( const LevelP&, SchedulerP& sched, int timeSubStep ) = 0;

  /** @brief Solve the transport equation */
  virtual void sched_solveTransportEqn( const LevelP&, SchedulerP& sched, int timeSubStep ) = 0;

  /** @brief Dummy init for MPMArches */ 
  virtual void sched_dummyInit( const LevelP&, SchedulerP& sched ) = 0; 

  /** @brief Compute the convective terms */ 
  template <class fT, class oldPhiT>  
  void computeConv( const Patch* patch, fT& Fdiff, 
                         oldPhiT& oldPhi );

  /** @brief Compute the diffusion terms */
  template <class fT, class oldPhiT, class lambdaT> 
  void computeDiff( const Patch* patch, fT& Fdiff, 
                    oldPhiT& oldPhi, lambdaT& lambda );

  /** @brief Method for cleaning up after a transport equation at the end of a timestep */
  virtual void sched_cleanUp( const LevelP&, SchedulerP& sched ) = 0; 

  /** @brief Apply boundary conditions */
  // probably want to make this is a template
  template <class phiType> void computeBCs( const Patch* patch, string varName, phiType& phi );

  /** @brief Set the initial value of the transported variable to some function */
  template <class phiType> void fcnInit( const Patch* patch, phiType& phi ); 

  // Access functions:
  inline void setBoundaryCond( BoundaryCondition_new* boundaryCond ) {
  d_boundaryCond = boundaryCond; 
  }
  inline void setTimeInt( ExplicitTimeInt* timeIntegrator ) {
  d_timeIntegrator = timeIntegrator; 
  }
  inline const VarLabel* getTransportEqnLabel(){
    return d_transportVarLabel; };
  inline const VarLabel* getoldTransportEqnLabel(){
    return d_oldtransportVarLabel; };
  inline const std::string getEqnName(){
    return d_eqnName; };
  inline const double getInitValue(){
    return d_initValue; };
  inline const string getInitFcn(){
    return d_initFcn; }; 


  template<class phiType> void
  computeBCsSpecial( const Patch* patch, 
                       string varName,
                       phiType& phi )
  {
    d_boundaryCond->setScalarValueBC( 0, patch, phi, varName ); 
  }

protected:

  template<class T> 
  struct FaceData {
    // 0 = e, 1=w, 2=n, 3=s, 4=t, 5=b
    //vector<T> values_[6];
    T p; 
    T e; 
    T w; 
    T n; 
    T s;
    T t;
    T b;
  };

  ArchesLabel* d_fieldLabels;
  const VarLabel* d_transportVarLabel;
  const VarLabel* d_oldtransportVarLabel; 
  std::string d_eqnName;  
  bool d_doConv, d_doDiff, d_addSources;

  const VarLabel* d_FdiffLabel;
  const VarLabel* d_FconvLabel; 
  const VarLabel* d_RHSLabel;

  std::string d_convScheme; 

  BoundaryCondition_new* d_boundaryCond;
  ExplicitTimeInt* d_timeIntegrator; 
  Discretization_new* d_disc;  

  double d_initValue; // The initial value for this eqn. 
  std::string d_initFcn; // A functional form for initial value.


  // These parameters are for the functional initialization
  // -- constant --
  double d_constant_init; 

  // -- step function -- 
  std::string d_step_dir; 
  double d_step_start; 
  double d_step_end; 
  double d_step_value; 

private:


}; // end EqnBase
//---------------------------------------------------------------------------
// Method: Phi initialization using a function 
//---------------------------------------------------------------------------
template <class phiType>  
void EqnBase::fcnInit( const Patch* patch, phiType& phi ) 
{
  for (CellIterator iter=patch->getCellIterator__New(0); !iter.done(); iter++){

    IntVector c = *iter; 
    Vector Dx = patch->dCell(); 
    double x,y,z; 
    x = c[0]*Dx.x() + Dx.x()/2.;
#ifdef YDIM
    y = c[1]*Dx.y() + Dx.y()/2.; 
#endif
#ifdef ZDIM
    z = c[2]*Dx.z() + Dx.z()/2.;
#endif 

    if (d_initFcn == "constant") {
      // ========== CONSTANT VALUE INITIALIZATION ============
      phi[c] = d_constant_init; 

    } else if (d_initFcn == "step") {
     // =========== STEP FUNCTION INITIALIZATION =============
      if (d_step_dir == "x") {
        if ( x > d_step_start && x < d_step_end )
          phi[c] = d_step_value; 
        else
          phi[c] = 0.0;
      } else if (d_step_dir == "y") {
#ifdef YDIM
        if ( y > d_step_start && y < d_step_end )
          phi[c] = d_step_value; 
        else
          phi[c] = 0.0;
#else
        cout << "WARNING!! YDIM NOT TURNED ON (COMPILED) WITH THIS VERION OF CODE" << endl;
        cout << "To get this to work, made sure YDIM is defined in ScalarEqn.h" << endl;
        cout << "Cannot initialize your scalar in y-dim with step function" << endl;
#endif

      } else if (d_step_dir == "z") {
#ifdef YDIM
        if ( z > d_step_start && z < d_step_end )
          phi[c] = d_step_value; 
        else
          phi[c] = 0.0;
#else
        cout << "WARNING!! YDIM NOT TURNED ON (COMPILED) WITH THIS VERION OF CODE" << endl;
        cout << "To get this to work, made sure ZDIM is defined in ScalarEqn.h" << endl;
        cout << "Cannot initialize your scalar in z-dim with step function" << endl;
#endif
      }
    // ======= add others below here ======
    } else {
        cout << "WARNING!! Your initialization function wasn't found." << endl;
    } 
  }
}
} // end namespace Uintah

#endif
