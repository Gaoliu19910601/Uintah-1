#include "ConstitutiveModelFactory.h"
#include "MWViscoElastic.h"
#include <Core/Malloc/Allocator.h>
#include <Packages/Uintah/Core/Grid/Patch.h>
#include <Packages/Uintah/CCA/Ports/DataWarehouse.h>
#include <Packages/Uintah/Core/Grid/NCVariable.h>
#include <Packages/Uintah/Core/Grid/ParticleSet.h>
#include <Packages/Uintah/Core/Grid/ParticleVariable.h>
#include <Packages/Uintah/Core/Grid/Task.h>
#include <Packages/Uintah/Core/Grid/VarLabel.h>
#include <Core/Math/MinMax.h>
#include <Packages/Uintah/Core/Math/Matrix3.h>
#include <Packages/Uintah/CCA/Components/MPM/ConstitutiveModel/MPMMaterial.h>
#include <Packages/Uintah/Core/Grid/VarTypes.h>
#include <Packages/Uintah/CCA/Components/MPM/MPMLabel.h>
#include <Packages/Uintah/Core/ProblemSpec/ProblemSpec.h>
#include <Core/Malloc/Allocator.h>
#include <Core/Util/NotFinished.h>
#include <fstream>
#include <iostream>

using std::cerr;
using namespace Uintah;
using namespace SCIRun;

MWViscoElastic::MWViscoElastic(ProblemSpecP& ps, MPMLabel* Mlb, int n8or27)
{
  lb = Mlb;

  ps->require("G",d_initialData.G);
  ps->require("K",d_initialData.K);
  d_se=0;
  d_8or27 = n8or27;
}

MWViscoElastic::~MWViscoElastic()
{
  // Destructor
}

void MWViscoElastic::initializeCMData(const Patch* patch,
                                        const MPMMaterial* matl,
                                        DataWarehouse* new_dw)
{
   // Put stuff in here to initialize each particle's
   // constitutive model parameters and deformationMeasure
   Matrix3 Identity, zero(0.);
   Identity.Identity();
   d_se=0;

   ParticleSubset* pset = new_dw->getParticleSubset(matl->getDWIndex(), patch);

   ParticleVariable<Matrix3> deformationGradient;
   new_dw->allocate(deformationGradient, lb->pDeformationMeasureLabel, pset);
   ParticleVariable<Matrix3> pstress;
   new_dw->allocate(pstress, lb->pStressLabel, pset);

   for(ParticleSubset::iterator iter = pset->begin();
          iter != pset->end(); iter++) {

      deformationGradient[*iter] = Identity;
      pstress[*iter] = zero;
   }
   new_dw->put(deformationGradient, lb->pDeformationMeasureLabel);
   new_dw->put(pstress, lb->pStressLabel);

   computeStableTimestep(patch, matl, new_dw);
}

void MWViscoElastic::addParticleState(std::vector<const VarLabel*>& from,
				   std::vector<const VarLabel*>& to)
{
   from.push_back(lb->pDeformationMeasureLabel);
   from.push_back(lb->pStressLabel);
   to.push_back(lb->pDeformationMeasureLabel_preReloc);
   to.push_back(lb->pStressLabel_preReloc);
}

void MWViscoElastic::computeStableTimestep(const Patch* patch,
                                           const MPMMaterial* matl,
                                           DataWarehouse* new_dw)
{
   // This is only called for the initial timestep - all other timesteps
   // are computed as a side-effect of computeStressTensor
  Vector dx = patch->dCell();
  int matlindex = matl->getDWIndex();
  ParticleSubset* pset = new_dw->getParticleSubset(matlindex, patch);
  constParticleVariable<double> pmass, pvolume;
  constParticleVariable<Vector> pvelocity;

  new_dw->get(pmass,     lb->pMassLabel,     pset);
  new_dw->get(pvolume,   lb->pVolumeLabel,   pset);
  new_dw->get(pvelocity, lb->pVelocityLabel, pset);

  double c_dil = 0.0;
  Vector WaveSpeed(1.e-12,1.e-12,1.e-12);

  double G = d_initialData.G;
  double bulk = d_initialData.K;
  for(ParticleSubset::iterator iter = pset->begin();iter != pset->end();iter++){
     particleIndex idx = *iter;

     // Compute wave speed at each particle, store the maximum
     c_dil = sqrt((bulk + 4.*G/3.)*pvolume[idx]/pmass[idx]);
     WaveSpeed=Vector(Max(c_dil+fabs(pvelocity[idx].x()),WaveSpeed.x()),
		      Max(c_dil+fabs(pvelocity[idx].y()),WaveSpeed.y()),
		      Max(c_dil+fabs(pvelocity[idx].z()),WaveSpeed.z()));
    }
    WaveSpeed = dx/WaveSpeed;
    double delT_new = WaveSpeed.minComponent();
    new_dw->put(delt_vartype(delT_new), lb->delTLabel);
}

void MWViscoElastic::computeStressTensor(const PatchSubset* patches,
                                        const MPMMaterial* matl,
                                        DataWarehouse* old_dw,
                                        DataWarehouse* new_dw)
{
  for(int p=0;p<patches->size();p++){
    const Patch* patch = patches->get(p);
    //
    //  FIX  To do:  Read in table for vres
    //               Obtain and modify particle temperature (deg K)
    //
    Matrix3 velGrad,deformationGradientInc,Identity,zero(0.),One(1.);
    double c_dil=0.0,Jinc;
    Vector WaveSpeed(1.e-12,1.e-12,1.e-12);
    double onethird = (1.0/3.0);

    Identity.Identity();

    Vector dx = patch->dCell();
    double oodx[3] = {1./dx.x(), 1./dx.y(), 1./dx.z()};

    int matlindex = matl->getDWIndex();
    // Create array for the particle position
    ParticleSubset* pset = old_dw->getParticleSubset(matlindex, patch);
    constParticleVariable<Point> px;
    ParticleVariable<Matrix3> deformationGradient, pstress;
    ParticleVariable<double> pvolume;
    constParticleVariable<double> pmass, ptemperature;
    constParticleVariable<Vector> pvelocity;
    constNCVariable<Vector> gvelocity;
    delt_vartype delT;
    if(d_8or27==27){
      constParticleVariable<Vector> psize;
      old_dw->get(psize,             lb->pSizeLabel,                  pset);
    }

    new_dw->allocate(pstress, lb->pStressLabel_preReloc,     pset);
    new_dw->allocate(deformationGradient, lb->pDeformationMeasureLabel_preReloc, pset);
    new_dw->allocate(pvolume, lb->pVolumeDeformedLabel, pset);
    
    old_dw->copyOut(deformationGradient, lb->pDeformationMeasureLabel, pset);
    old_dw->copyOut(pstress,             lb->pStressLabel,             pset);
    old_dw->copyOut(pvolume,             lb->pVolumeLabel,             pset);
    old_dw->get(px,                  lb->pXLabel,                  pset);
    old_dw->get(pmass,               lb->pMassLabel,               pset);
    old_dw->get(pvelocity,           lb->pVelocityLabel,           pset);
    old_dw->get(ptemperature,        lb->pTemperatureLabel,        pset);

    new_dw->get(gvelocity,           lb->gVelocityLabel, matlindex,patch,
		Ghost::AroundCells, 1);

    old_dw->get(delT, lb->delTLabel);

    constParticleVariable<int> pConnectivity;
    ParticleVariable<Vector> pRotationRate;
    ParticleVariable<double> pStrainEnergy;

    double G = d_initialData.G;
    double bulk = d_initialData.K;

    for(ParticleSubset::iterator iter = pset->begin();
					iter != pset->end(); iter++){
      particleIndex idx = *iter;

      velGrad.set(0.0);
      // Get the node indices that surround the cell
      IntVector ni[MAX_BASIS];
      Vector d_S[MAX_BASIS];
      if(d_8or27==8){
          patch->findCellAndShapeDerivatives(px[idx], ni, d_S);
       }
       else if(d_8or27==27){
          patch->findCellAndShapeDerivatives27(px[idx], ni, d_S);
       }

      for(int k = 0; k < d_8or27; k++) {
	  const Vector& gvel = gvelocity[ni[k]];
	  for (int j = 0; j<3; j++){
	    for (int i = 0; i<3; i++) {
	      velGrad(i+1,j+1)+=gvel(i) * d_S[k](j) * oodx[j];
	    }
	  }
      }

      // Calculate rate of deformation D, and deviatoric rate DPrime
    
      Matrix3 D = (velGrad + velGrad.Transpose())*.5;
      Matrix3 DPrime = D - Identity*onethird*D.Trace();

      // This is the (updated) Cauchy stress

      Matrix3 OldStress = pstress[idx];
      pstress[idx] += (DPrime*2.*G + Identity*bulk*D.Trace())*delT;

      // Compute the deformation gradient increment using the time_step
      // velocity gradient
      // F_n^np1 = dudx * dt + Identity
      deformationGradientInc = velGrad * delT + Identity;

      Jinc = deformationGradientInc.Determinant();

      // Update the deformation gradient tensor to its time n+1 value.
      deformationGradient[idx] = deformationGradientInc *
                             deformationGradient[idx];

      // get the volumetric part of the deformation
      // unused variable - Steve
      // double J = deformationGradient[idx].Determinant();

      pvolume[idx]=Jinc*pvolume[idx];

      // Compute the strain energy for all the particles
      OldStress = (pstress[idx] + OldStress)*.5;

      double e = (D(1,1)*OldStress(1,1) +
	          D(2,2)*OldStress(2,2) +
	          D(3,3)*OldStress(3,3) +
	       2.*(D(1,2)*OldStress(1,2) +
		   D(1,3)*OldStress(1,3) +
		   D(2,3)*OldStress(2,3))) * pvolume[idx]*delT;

      d_se += e;		   

      // Compute wave speed at each particle, store the maximum
      Vector pvelocity_idx = pvelocity[idx];
      if(pmass[idx] > 0){
        c_dil = sqrt((bulk + 4.*G/3.)*pvolume[idx]/pmass[idx]);
      }
      else{
        c_dil = 0.0;
        pvelocity_idx = Vector(0.0,0.0,0.0);
      }
      WaveSpeed=Vector(Max(c_dil+fabs(pvelocity_idx.x()),WaveSpeed.x()),
		       Max(c_dil+fabs(pvelocity_idx.y()),WaveSpeed.y()),
		       Max(c_dil+fabs(pvelocity_idx.z()),WaveSpeed.z()));
    }

    WaveSpeed = dx/WaveSpeed;
    double delT_new = WaveSpeed.minComponent();
    new_dw->put(delt_vartype(delT_new),lb->delTLabel);
    new_dw->put(pstress,               lb->pStressLabel_preReloc);
    new_dw->put(deformationGradient,   lb->pDeformationMeasureLabel_preReloc);
    new_dw->put(sum_vartype(d_se),     lb->StrainEnergyLabel);
    new_dw->put(pvolume,               lb->pVolumeDeformedLabel);
  }
}

void MWViscoElastic::addComputesAndRequires(Task* task,
					 const MPMMaterial* matl,
					 const PatchSet* ) const
{
  const MaterialSubset* matlset = matl->thisMaterial();
  task->requires(Task::OldDW, lb->delTLabel);
  task->requires(Task::OldDW, lb->pXLabel,           matlset, Ghost::None);
  task->requires(Task::OldDW, lb->pMassLabel,        matlset, Ghost::None);
  task->requires(Task::OldDW, lb->pStressLabel,      matlset, Ghost::None);
  task->requires(Task::OldDW, lb->pVolumeLabel,      matlset, Ghost::None);
  task->requires(Task::OldDW, lb->pTemperatureLabel, matlset, Ghost::None);
  task->requires(Task::OldDW, lb->pDeformationMeasureLabel,matlset,Ghost::None);
  task->requires(Task::NewDW, lb->gVelocityLabel,    matlset,
                  Ghost::AroundCells, 1);
  if(d_8or27==27){
    task->requires(Task::OldDW, lb->pSizeLabel,      matlset, Ghost::None);
  }

  task->computes(lb->pStressLabel_preReloc,             matlset);
  task->computes(lb->pDeformationMeasureLabel_preReloc, matlset);
  task->computes(lb->pVolumeDeformedLabel,              matlset);
}

double MWViscoElastic::computeRhoMicroCM(double pressure,
                                      const double p_ref,
                                      const MPMMaterial* matl)
{
  double rho_orig = matl->getInitialDensity();
  //double p_ref=101325.0;
  double p_gauge = pressure - p_ref;
  double rho_cur;
  //double G = d_initialData.G;
  double bulk = d_initialData.K;

  rho_cur = rho_orig/(1-p_gauge/bulk);

  return rho_cur;

#if 0
  cout << "NO VERSION OF computeRhoMicroCM EXISTS YET FOR MWViscoElastic"
       << endl;
#endif
}

void MWViscoElastic::computePressEOSCM(const double rho_cur, double& pressure,
                                    const double p_ref,
                                    double& dp_drho,      double& tmp,
                                    const MPMMaterial* matl)
{

  double G = d_initialData.G;
  double bulk = d_initialData.K;
  double rho_orig = matl->getInitialDensity();

  double p_g = bulk*(1.0 - rho_orig/rho_cur);
  pressure = p_ref + p_g;
  dp_drho  = bulk*rho_orig/(rho_cur*rho_cur);
  tmp = (bulk + 4.*G/3.)/rho_cur;  // speed of sound squared

#if 0
  cout << "NO VERSION OF computePressEOSCM EXISTS YET FOR MWViscoElastic"
       << endl;
#endif
}

double MWViscoElastic::getCompressibility()
{
  return 1.0/d_initialData.K;
}


#if defined(__sgi) && !defined(__GNUC__) && (_MIPS_SIM != _MIPS_SIM_ABI32)
#pragma set woff 1209
#endif

namespace Uintah {

#if 0
static MPI_Datatype makeMPI_CMData()
{
   ASSERTEQ(sizeof(MWViscoElastic::StateData), sizeof(double)*2);
   MPI_Datatype mpitype;
   MPI_Type_vector(1, 2, 2, MPI_DOUBLE, &mpitype);
   MPI_Type_commit(&mpitype);
   return mpitype;
}

const TypeDescription* fun_getTypeDescription(MWViscoElastic::StateData*)
{
   static TypeDescription* td = 0;
   if(!td){
      td = scinew
	TypeDescription(TypeDescription::Other,
			"MWViscoElastic::StateData", true, &makeMPI_CMData);
   }
   return td;
}
#endif

} // End namespace Uintah
