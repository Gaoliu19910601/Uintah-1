/*

The MIT License

Copyright (c) 1997-2011 Center for the Simulation of Accidental Fires and
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


#ifndef __Arenisca3_H__
#define __Arenisca3_H__


#include <CCA/Components/MPM/ConstitutiveModel/ConstitutiveModel.h>
#include <Core/Math/Matrix3.h>
#include <Core/ProblemSpec/ProblemSpecP.h>
#include <CCA/Ports/DataWarehouseP.h>

#include <cmath>

namespace Uintah {
  class MPMLabel;
  class MPMFlags;

  /**************************************

  ****************************************/

  class Arenisca3 : public ConstitutiveModel {
    // Create datatype for storing model parameters
  public:
    struct CMData {
      double PEAKI1;
      double FSLOPE;
      double STREN;
      double YSLOPE;
      double BETA_nonassociativity;
      double B0;
      double B1;
      double B2;
      double B3;
      double B4;
      double G0;
      double G1;
      double G2;
      double G3;
      double G4;
      double p0_crush_curve;
      double p1_crush_curve;
      double p2_crush_curve;
      double p3_crush_curve;
      double CR;
      double fluid_B0;
      double fluid_pressure_initial;
      double T1_rate_dependence;
      double T2_rate_dependence;
      double gruneisen_parameter;
      double subcycling_characteristic_number;
    };
    const VarLabel* pLocalizedLabel;
    const VarLabel* pLocalizedLabel_preReloc;
    const VarLabel* pAreniscaFlagLabel;          //0: ok, 1: pevp<-p3
    const VarLabel* pAreniscaFlagLabel_preReloc;
    const VarLabel* pScratchDouble1Label;
    const VarLabel* pScratchDouble1Label_preReloc;
    const VarLabel* pScratchDouble2Label;
    const VarLabel* pScratchDouble2Label_preReloc;
    const VarLabel* pPorePressureLabel;
    const VarLabel* pPorePressureLabel_preReloc;
    const VarLabel* pepLabel;               //Plastic Strain
    const VarLabel* pepLabel_preReloc;
    const VarLabel* pevpLabel;              //Plastic Volumetric Strain
    const VarLabel* pevpLabel_preReloc;
    const VarLabel* peveLabel;              //Elastic Volumetric Strain
    const VarLabel* peveLabel_preReloc;
    const VarLabel* pCapXLabel;
    const VarLabel* pCapXLabel_preReloc;
    const VarLabel* pCapXQSLabel;
    const VarLabel* pCapXQSLabel_preReloc;
    const VarLabel* pKappaLabel;
    const VarLabel* pKappaLabel_preReloc;
    const VarLabel* pZetaLabel;
    const VarLabel* pZetaLabel_preReloc;
    const VarLabel* pZetaQSLabel;
    const VarLabel* pZetaQSLabel_preReloc;
    const VarLabel* pIotaLabel;
    const VarLabel* pIotaLabel_preReloc;
    const VarLabel* pIotaQSLabel;
    const VarLabel* pIotaQSLabel_preReloc;
    const VarLabel* pStressQSLabel;
    const VarLabel* pStressQSLabel_preReloc;
    const VarLabel* pScratchMatrixLabel;
    const VarLabel* pScratchMatrixLabel_preReloc;

  private:
    double one_third,
           two_third,
           four_third,
           sqrt_three,
           one_sqrt_three,
           small_number,
           big_number;
    CMData d_cm;

    // Prevent copying of this class
    // copy constructor

    Arenisca3& operator=(const Arenisca3 &cm);

    void initializeLocalMPMLabels();

  public:
    // constructor
    Arenisca3(ProblemSpecP& ps, MPMFlags* flag);
    Arenisca3(const Arenisca3* cm);

    // destructor
    virtual ~Arenisca3();

    virtual void outputProblemSpec(ProblemSpecP& ps,bool output_cm_tag = true);

    // clone

    Arenisca3* clone();

    // compute stable timestep for this patch
    virtual void computeStableTimestep(const Patch* patch,
                                       const MPMMaterial* matl,
                                       DataWarehouse* new_dw);

    // compute stress at each particle in the patch
    virtual void computeStressTensor(const PatchSubset* patches,
                                     const MPMMaterial* matl,
                                     DataWarehouse* old_dw,
                                     DataWarehouse* new_dw);

  private: //non-uintah mpm constitutive specific functions
    int computeStep(const Matrix3& D,
                    const double & Dt,
                    const Matrix3& sigma_n,
                    const double & X_n,
                    const double & Zeta_n,
                    const Matrix3& ep_n,
                    Matrix3& sigma_p,
                    double & X_p,
                    double & Zeta_p,
                    Matrix3& ep_p,
                    long64 ParticleID);

    void computeElasticProperties(double & bulk,
                                  double & shear);

    void computeElasticProperties(const Matrix3 stress,
                                  const Matrix3 ep,
                                  double & bulk,
                                  double & shear
                                 );

    Matrix3 computeTrialStress(const Matrix3& sigma_old,  // old stress
                               const Matrix3& d_e,        // Strain increment
                               const double& bulk,        // bulk modulus
                               const double& shear);      // shear modulus

    int computeStepDivisions(const double& X,
                             const double& Zeta,
                             const Matrix3& ep,
                             const Matrix3& sigma_n,
                             const Matrix3& sigma_trial);

    void computeInvariants(const Matrix3& stress,
                           Matrix3& S,
                           double& I1,
                           double& J2,
                           double& rJ2);

    int computeSubstep(const Matrix3& D,         // Strain "rate"
                       const double & dt,        // time substep (s)
                       const Matrix3& sigma_old, // stress at start of substep
                       const Matrix3& ep_old,    // plastic strain at start of substep
                       const double & X_old,     // hydrostatic comrpessive strength at start of substep
                       const double & Zeta_old,  // trace of isotropic backstress at start of substep
                       Matrix3& sigma_new, // stress at end of substep
                       Matrix3& ep_new,    // plastic strain at end of substep
                       double & X_new,     // hydrostatic comrpessive strength at end of substep
                       double & Zeta_new   // trace of isotropic backstress at end of substep
                      );

    double computeX(double evp);

    double computedZetadevp(double Zeta,
                            double evp);
    double computeev0();

    int nonHardeningReturn(const double & I1_trial,
                           const double & rJ2_trial,
                           const Matrix3& S_trial,
                           const double & I1_old,
                           const double &rJ2_old,
                           const Matrix3& S_old,
                           const Matrix3& d_e,
                           const double & X,
                           const double & Zeta,
                           const double & bulk,
                           const double & shear,
                                 double & I1_new,
                                 double & rJ2_new,
                                 Matrix3& S_new,
                                 Matrix3& d_ep_new);

    void transformedBisection(double& z_0,
                              double& r_0,
                              const double& z_trial,
                              const double& r_trial,
                              const double& X,
                              const double& Zeta,
                              const double& bulk,
                              const double& shear
                             );

    int transformedYieldFunction(const double& z,
                                 const double& r,
                                 const double& X,
                                 const double& Zeta,
                                 const double& bulk,
                                 const double& shear
                                );
    int computeYieldFunction(const double& I1,
                             const double& rJ2,
                             const double& X,
                             const double& Zeta
                            );

  public: //Uintah MPM constitutive model specific functions
    ////////////////////////////////////////////////////////////////////////
    /* Make the value for pLocalized computed locally available outside of the model. */
    ////////////////////////////////////////////////////////////////////////
    virtual void addRequiresDamageParameter(Task* task,
                                            const MPMMaterial* matl,
                                            const PatchSet* patches) const;


    ////////////////////////////////////////////////////////////////////////
    /* Make the value for pLocalized computed locally available outside of the model */
    ////////////////////////////////////////////////////////////////////////
    virtual void getDamageParameter(const Patch* patch,
                                    ParticleVariable<int>& damage, int dwi,
                                    DataWarehouse* old_dw,
                                    DataWarehouse* new_dw);


    // carry forward CM data for RigidMPM
    virtual void carryForward(const PatchSubset* patches,
                              const MPMMaterial* matl,
                              DataWarehouse* old_dw,
                              DataWarehouse* new_dw);


    // initialize  each particle's constitutive model data
    virtual void initializeCMData(const Patch* patch,
                                  const MPMMaterial* matl,
                                  DataWarehouse* new_dw);


    virtual void addInitialComputesAndRequires(Task* task,
                                               const MPMMaterial* matl,
                                               const PatchSet* patches) const;

    virtual void addComputesAndRequires(Task* task,
                                        const MPMMaterial* matl,
                                        const PatchSet* patches) const;

    virtual void addComputesAndRequires(Task* task,
                                        const MPMMaterial* matl,
                                        const PatchSet* patches,
                                        const bool recursion) const;

    virtual void addParticleState(std::vector<const VarLabel*>& from,
                                  std::vector<const VarLabel*>& to);

    virtual double computeRhoMicroCM(double pressure,
                                     const double p_ref,
                                     const MPMMaterial* matl,
                                     double temperature,
                                     double rho_guess);

    virtual void computePressEOSCM(double rho_m, double& press_eos,
                                   double p_ref,
                                   double& dp_drho, double& ss_new,
                                   const MPMMaterial* matl,
                                   double temperature);

    virtual double getCompressibility();

  };
} // End namespace Uintah


#endif  // __Arenisca3_H__
