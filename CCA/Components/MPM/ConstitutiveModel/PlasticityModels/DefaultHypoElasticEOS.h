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


#ifndef __DEFAULT_HYPOELASTIC_EOS_MODEL_H__
#define __DEFAULT_HYPOELASTIC_EOS_MODEL_H__


#include "MPMEquationOfState.h" 
#include <Core/ProblemSpec/ProblemSpecP.h>

namespace Uintah {

  ////////////////////////////////////////////////////////////////////////////
  /*! 
    \class DefaultHypoElasticEOS
    \brief Not really an equation of state but just an isotropic
    hypoelastic pressure calculator based on bulk modulus.
    \author Biswajit Banerjee, \n
    C-SAFE and Department of Mechanical Engineering, \n
    University of Utah \n
    Copyright (C) 2002-2003 University of Utah

    The equation of state is given by
    \f[
    p = Tr(D) K \Delta T
    \f]
    where \n
    \f$p\f$ = pressure\n
    \f$D\f$ = rate of deformation tensor\n
    \f$K\f$ = bulk modulus\n
    \f$\Delta T\f$ = time increment
  */
  ////////////////////////////////////////////////////////////////////////////

  class DefaultHypoElasticEOS : public MPMEquationOfState {

  private:

    // Store reference bulk modulus
    double d_bulk;

    // Prevent copying of this class
    // copy constructor
    //DefaultHypoElasticEOS(const DefaultHypoElasticEOS &cm);
    DefaultHypoElasticEOS& operator=(const DefaultHypoElasticEOS &cm);

  public:
    // constructors
    DefaultHypoElasticEOS(); // This constructor is used when there is
                                        // no equation_of_state tag in the input
                                        // file  ** WARNING **
    DefaultHypoElasticEOS(ProblemSpecP& ps); 
    DefaultHypoElasticEOS(const DefaultHypoElasticEOS* cm);
         
    // destructor 
    virtual ~DefaultHypoElasticEOS();

    virtual void outputProblemSpec(ProblemSpecP& ps);
         
    // Set the bulk modulus
    void setBulkModulus(const double& bulk)
    {
      d_bulk = bulk;
    }

    //////////
    // Calculate the pressure using a equation of state
    double computePressure(const MPMMaterial* matl,
                           const PlasticityState* state,
                           const Matrix3& deformGrad,
                           const Matrix3& rateOfDeformation,
                           const double& delT);
  
    double eval_dp_dJ(const MPMMaterial* matl,
                      const double& detF,
                      const PlasticityState* state);
    
    // Compute pressure (option 1)
    double computePressure(const double& rho_orig,
                           const double& rho_cur);

    // Compute pressure (option 2)
    void computePressure(const double& rho_orig,
                         const double& rho_cur,
                         double& pressure,
                         double& dp_drho,
                         double& csquared);

    // Compute bulk modulus
    double computeBulkModulus(const double& rho_orig,
                              const double& rho_cur);

    // Compute strain energy
    double computeStrainEnergy(const double& rho_orig,
                               const double& rho_cur);

    // Compute density given pressure
    double computeDensity(const double& rho_orig,
                          const double& pressure);
  };

} // End namespace Uintah

#endif  // __DEFAULT_HYPOELASTIC_EOS_MODEL_H__ 
