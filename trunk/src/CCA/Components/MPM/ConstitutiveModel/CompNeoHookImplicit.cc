#include <Packages/Uintah/CCA/Components/MPM/ConstitutiveModel/CompNeoHookImplicit.h>
#include <Packages/Uintah/Core/Grid/LinearInterpolator.h>
#include <Core/Malloc/Allocator.h>
#include <Packages/Uintah/Core/Grid/Patch.h>
#include <Packages/Uintah/CCA/Ports/DataWarehouse.h>
#include <Packages/Uintah/Core/Grid/Variables/NCVariable.h>
#include <Packages/Uintah/Core/Grid/Variables/ParticleSet.h>
#include <Packages/Uintah/Core/Grid/Variables/ParticleVariable.h>
#include <Packages/Uintah/Core/Grid/Task.h>
#include <Packages/Uintah/Core/Grid/Level.h>
#include <Packages/Uintah/Core/Grid/Variables/VarLabel.h>
#include <Packages/Uintah/Core/Grid/Variables/VarTypes.h>
#include <Packages/Uintah/Core/Labels/MPMLabel.h>
#include <Packages/Uintah/Core/Math/Matrix3.h>
#include <Packages/Uintah/CCA/Components/MPM/ConstitutiveModel/MPMMaterial.h>
#include <Packages/Uintah/Core/ProblemSpec/ProblemSpec.h>
#include <Packages/Uintah/Core/Exceptions/ParameterNotFound.h>
#include <Core/Math/MinMax.h>
#include <Core/Malloc/Allocator.h>
#include <sgi_stl_warnings_off.h>
#include <fstream>
#include <iostream>
#include <sgi_stl_warnings_on.h>
#include <TauProfilerForSCIRun.h>

using std::cerr;
using namespace Uintah;
using namespace SCIRun;

CompNeoHookImplicit::CompNeoHookImplicit(ProblemSpecP& ps,  MPMFlags* Mflag)
  : ConstitutiveModel(Mflag), ImplicitCM()
{
  d_useModifiedEOS = false;
  ps->require("bulk_modulus", d_initialData.Bulk);
  ps->require("shear_modulus",d_initialData.Shear);
  ps->get("active",d_active); 
  ps->get("useModifiedEOS",d_useModifiedEOS); 
}

CompNeoHookImplicit::CompNeoHookImplicit(const CompNeoHookImplicit* cm)
  : ConstitutiveModel(cm), ImplicitCM(cm)
{

  d_useModifiedEOS = cm->d_useModifiedEOS;
  d_initialData.Bulk = cm->d_initialData.Bulk;
  d_initialData.Shear = cm->d_initialData.Shear;
}

CompNeoHookImplicit::~CompNeoHookImplicit()
{
}

void 
CompNeoHookImplicit::outputProblemSpec(ProblemSpecP& ps,bool output_cm_tag)
{
  ProblemSpecP cm_ps = ps;
  if (output_cm_tag) {
    cm_ps = ps->appendChild("constitutive_model");
    cm_ps->setAttribute("type","comp_neo_hook");
  }
  
  cm_ps->appendElement("bulk_modulus",d_initialData.Bulk);
  cm_ps->appendElement("shear_modulus",d_initialData.Shear);
  cm_ps->appendElement("useModifiedEOS",d_useModifiedEOS);
  cm_ps->appendElement("active",d_active);
}


CompNeoHookImplicit* CompNeoHookImplicit::clone()
{
  return scinew CompNeoHookImplicit(*this);
}

void CompNeoHookImplicit::initializeCMData(const Patch* patch,
                                        const MPMMaterial* matl,
                                        DataWarehouse* new_dw)
{
   // Put stuff in here to initialize each particle's
   // constitutive model parameters and deformationMeasure
   Matrix3 Identity, zero(0.);
   Identity.Identity();

   ParticleSubset* pset = new_dw->getParticleSubset(matl->getDWIndex(), patch);
   ParticleVariable<Matrix3> deformationGradient, pstress;

   new_dw->allocateAndPut(deformationGradient,lb->pDeformationMeasureLabel,
                                                                        pset);
   new_dw->allocateAndPut(pstress,lb->pStressLabel,                     pset);

   for(ParticleSubset::iterator iter = pset->begin();
          iter != pset->end(); iter++) {
          deformationGradient[*iter] = Identity;
          pstress[*iter] = zero;
   }
}

void CompNeoHookImplicit::allocateCMDataAddRequires(Task* task,
                                                    const MPMMaterial* matl,
                                                    const PatchSet* ,
                                                    MPMLabel* lb) const
{

  const MaterialSubset* matlset = matl->thisMaterial();
  Ghost::GhostType  gnone = Ghost::None;

  task->requires(Task::NewDW,lb->pStressLabel_preReloc, matlset, gnone);
  task->requires(Task::NewDW,lb->pDeformationMeasureLabel_preReloc,
                                                        matlset, gnone);
}


void CompNeoHookImplicit::allocateCMDataAdd(DataWarehouse* new_dw,
                                            ParticleSubset* addset,
                                            map<const VarLabel*,
                                            ParticleVariableBase*>* newState,
                                            ParticleSubset* delset,
                                            DataWarehouse* )
{
  // Put stuff in here to initialize each particle's
  // constitutive model parameters and deformationMeasure
  
  ParticleVariable<Matrix3> deformationGradient, pstress;
  constParticleVariable<Matrix3> o_deformationGradient, o_stress;
  
  new_dw->allocateTemporary(deformationGradient,addset);
  new_dw->allocateTemporary(pstress,            addset);
  
  new_dw->get(o_deformationGradient,lb->pDeformationMeasureLabel_preReloc,
                                                                 delset);
  new_dw->get(o_stress,lb->pStressLabel_preReloc,                delset);

  ParticleSubset::iterator o,n = addset->begin();
  for (o=delset->begin(); o != delset->end(); o++, n++) {
    deformationGradient[*n] = o_deformationGradient[*o];
    pstress[*n] = o_stress[*o];
  }
  
  (*newState)[lb->pDeformationMeasureLabel]=deformationGradient.clone();
  (*newState)[lb->pStressLabel]=pstress.clone();
}

void CompNeoHookImplicit::addParticleState(std::vector<const VarLabel*>& from,
                                   std::vector<const VarLabel*>& to)
{
}

void CompNeoHookImplicit::computeStableTimestep(const Patch*,
                                           const MPMMaterial*,
                                           DataWarehouse*)
{
  // Not used for the implicit models.
}

void 
CompNeoHookImplicit::computeStressTensor(const PatchSubset* patches,
                                         const MPMMaterial* matl,
                                         DataWarehouse* old_dw,
                                         DataWarehouse* new_dw,
                                         Solver* solver,
                                         const bool )

{
  for(int pp=0;pp<patches->size();pp++){
    const Patch* patch = patches->get(pp);
//    cerr <<"Doing computeStressTensor on " << patch->getID()
//       <<"\t\t\t\t IMPM"<< "\n" << "\n";

    IntVector lowIndex = patch->getInteriorNodeLowIndex();
    IntVector highIndex = patch->getInteriorNodeHighIndex()+IntVector(1,1,1);
    Array3<int> l2g(lowIndex,highIndex);
    solver->copyL2G(l2g,patch);

    Matrix3 Shear,deformationGradientInc;
    Matrix3 Identity;
    Identity.Identity();

    LinearInterpolator* interpolator = new LinearInterpolator(patch);
    vector<IntVector> ni(interpolator->size());
    vector<Vector> d_S(interpolator->size());
    
    Vector dx = patch->dCell();
    double oodx[3] = {1./dx.x(), 1./dx.y(), 1./dx.z()};

    int dwi = matl->getDWIndex();

    ParticleSubset* pset;
    constParticleVariable<Point> px;
    ParticleVariable<Matrix3> deformationGradient_new, pstress;
    constParticleVariable<Matrix3> deformationGradient;
    constParticleVariable<double> pvolumeold, pmass;
    ParticleVariable<double> pvolume_deformed;
    delt_vartype delT;
    
    DataWarehouse* parent_old_dw = 
      new_dw->getOtherDataWarehouse(Task::ParentOldDW);
    pset = parent_old_dw->getParticleSubset(dwi, patch);
    parent_old_dw->get(delT,           lb->delTLabel,   getLevel(patches));
    parent_old_dw->get(px,             lb->pXLabel,                  pset);
    parent_old_dw->get(pvolumeold,     lb->pVolumeLabel,             pset);
    parent_old_dw->get(pmass,          lb->pMassLabel,               pset);
    parent_old_dw->get(deformationGradient,
                                       lb->pDeformationMeasureLabel, pset);

    new_dw->allocateAndPut(pstress,          lb->pStressLabel_preReloc, pset);
    new_dw->allocateAndPut(pvolume_deformed, lb->pVolumeDeformedLabel,  pset);

    new_dw->allocateTemporary(deformationGradient_new,pset);
    Ghost::GhostType  gac   = Ghost::AroundCells;

    double shear = d_initialData.Shear;
    double bulk  = d_initialData.Bulk;

    double rho_orig = matl->getInitialDensity();
    
    double B[6][24];
    double Bnl[3][24];
    int dof[24];
    double v[576];

    if(matl->getIsRigid()){
      for(ParticleSubset::iterator iter = pset->begin();
                                   iter != pset->end(); iter++){
        particleIndex idx = *iter;
        pstress[idx] = Matrix3(0.0);
        pvolume_deformed[idx] = pvolumeold[idx];
      }
    }
    else{
      int extraFlushesLeft = flag->d_extraSolverFlushes;
      int flushSpacing = 0;

      if ( extraFlushesLeft > 0 ) {
	flushSpacing = pset->numParticles() / flag->d_extraSolverFlushes;
      }

      if(flag->d_doGridReset){
        constNCVariable<Vector> dispNew;
        old_dw->get(dispNew,lb->dispNewLabel,dwi,patch, gac, 1);
        computeDeformationGradientFromIncrementalDisplacement(
                                                      dispNew, pset, px,
                                                      deformationGradient,
                                                      deformationGradient_new,
                                                      dx, interpolator);
      }
      else if(!flag->d_doGridReset){
        constNCVariable<Vector> gdisplacement;
        old_dw->get(gdisplacement, lb->gDisplacementLabel,dwi,patch,gac,1);
        computeDeformationGradientFromTotalDisplacement(gdisplacement,
                                                        pset, px,
                                                        deformationGradient_new,
                                                        dx, interpolator);
      }

      double time = d_sharedState->getElapsedTime();

      for(ParticleSubset::iterator iter = pset->begin();
                                   iter != pset->end(); iter++){
        particleIndex idx = *iter;

        // Fill in the B and Bnl matrices and the dof vector
        vector<IntVector> ni(8);
        vector<Vector> d_S(8);
        interpolator->findCellAndShapeDerivatives(px[idx], ni, d_S);
        loadBMats(l2g,dof,B,Bnl,d_S,ni,oodx);

        // get the volumetric part of the deformation
        double J = deformationGradient_new[idx].Determinant();

        Matrix3 bElBar_new = deformationGradient_new[idx]
                           * deformationGradient_new[idx].Transpose()
                           * pow(J,-(2./3.));

        // Shear is equal to the shear modulus times dev(bElBar)
        double mubar = 1./3. * bElBar_new.Trace()*shear;
        Matrix3 shrTrl = (bElBar_new*shear - Identity*mubar);

        double active_stress = d_active*(time+delT);

        // get the hydrostatic part of the stress
        double p = bulk*log(J)/J + active_stress;

        // compute the total stress (volumetric + deviatoric)
        pstress[idx] = Identity*p + shrTrl/J;
        //cout << "p = " << p << " J = " << J << " tdev = " << shrTrl << endl;

        double coef1 = bulk;
        double coef2 = 2.*bulk*log(J);
        double D[6][6];

        D[0][0] = coef1 - coef2 + 2.*mubar*2./3. - 2./3.*(2.*shrTrl(0,0));
        D[0][1] = coef1 - 2.*mubar*1./3. - 2./3.*(shrTrl(0,0) + shrTrl(1,1));
        D[0][2] = coef1 - 2.*mubar*1./3. - 2./3.*(shrTrl(0,0) + shrTrl(2,2));
        D[0][3] =  - 2./3.*(shrTrl(0,1));
        D[0][4] =  - 2./3.*(shrTrl(0,2));
        D[0][5] =  - 2./3.*(shrTrl(1,2));
        D[1][1] = coef1 - coef2 + 2.*mubar*2./3. - 2./3.*(2.*shrTrl(1,1));
        D[1][2] = coef1 - 2.*mubar*1./3. - 2./3.*(shrTrl(1,1) + shrTrl(2,2));
        D[1][3] =  - 2./3.*(shrTrl(0,1));
        D[1][4] =  - 2./3.*(shrTrl(0,2));
        D[1][5] =  - 2./3.*(shrTrl(1,2));
        D[2][2] = coef1 - coef2 + 2.*mubar*2./3. - 2./3.*(2.*shrTrl(2,2));
        D[2][3] =  - 2./3.*(shrTrl(0,1));
        D[2][4] =  - 2./3.*(shrTrl(0,2));
        D[2][5] =  - 2./3.*(shrTrl(1,2));
        D[3][3] =  -.5*coef2 + mubar;
        D[3][4] = 0.;
        D[3][5] = 0.;
        D[4][4] =  -.5*coef2 + mubar;
        D[4][5] = 0.;
        D[5][5] =  -.5*coef2 + mubar;

        // kmat = B.transpose()*D*B*volold
        double kmat[24][24];
        BtDB(B,D,kmat);
        // kgeo = Bnl.transpose*sig*Bnl*volnew;
        double sig[3][3];
        for (int i = 0; i < 3; i++) {
          for (int j = 0; j < 3; j++) {
            sig[i][j]=pstress[idx](i,j);
          }
        }
        double kgeo[24][24];
        BnltDBnl(Bnl,sig,kgeo);

        // Print out stuff
        /*
        cout.setf(ios::scientific,ios::floatfield);
        cout.precision(10);
        cout << "B = " << endl;
        for(int kk = 0; kk < 24; kk++) {
          for (int ll = 0; ll < 6; ++ll) {
            cout << B[ll][kk] << " " ;
          }
          cout << endl;
        }
        cout << "Bnl = " << endl;
        for(int kk = 0; kk < 24; kk++) {
          for (int ll = 0; ll < 3; ++ll) {
            cout << Bnl[ll][kk] << " " ;
          }
          cout << endl;
        }
        cout << "D = " << endl;
        for(int kk = 0; kk < 6; kk++) {
          for (int ll = 0; ll < 6; ++ll) {
            cout << D[ll][kk] << " " ;
          }
          cout << endl;
        }
        cout << "Kmat = " << endl;
        for(int kk = 0; kk < 24; kk++) {
          for (int ll = 0; ll < 24; ++ll) {
            cout << kmat[ll][kk] << " " ;
          }
          cout << endl;
        }
        cout << "Kgeo = " << endl;
        for(int kk = 0; kk < 24; kk++) {
          for (int ll = 0; ll < 24; ++ll) {
            cout << kgeo[ll][kk] << " " ;
          }
          cout << endl;
        }
        */
        double volold = (pmass[idx]/rho_orig);
        double volnew = volold*J;

        pvolume_deformed[idx] = volnew;

        for(int ii = 0;ii<24;ii++){
          for(int jj = 0;jj<24;jj++){
            kmat[ii][jj]*=volold;
            kgeo[ii][jj]*=volnew;
          }
        }

        for (int I = 0; I < 24;I++){
          for (int J = 0; J < 24; J++){
            v[24*I+J] = kmat[I][J] + kgeo[I][J];
          }
        }
        solver->fillMatrix(24,dof,24,dof,v);

	flushSpacing--;

	if ( ( flushSpacing <= 0 ) && ( extraFlushesLeft > 0 ) ) {
	  flushSpacing = pset->numParticles() / flag->d_extraSolverFlushes;
	  extraFlushesLeft--;
	  solver->flushMatrix();
	}

      }  // end of loop over particles
      
      while ( extraFlushesLeft ) {
	extraFlushesLeft--;
	solver->flushMatrix();
      }

    }
    delete interpolator;
  }
  solver->flushMatrix();
}


void 
CompNeoHookImplicit::computeStressTensor(const PatchSubset* patches,
                                         const MPMMaterial* matl,
                                         DataWarehouse* old_dw,
                                         DataWarehouse* new_dw)


{
   for(int pp=0;pp<patches->size();pp++){
     const Patch* patch = patches->get(pp);
     Matrix3 Shear,deformationGradientInc;

     Matrix3 Identity;
     Identity.Identity();

     Vector dx = patch->dCell();

     int dwi = matl->getDWIndex();
     ParticleSubset* pset = old_dw->getParticleSubset(dwi, patch);
     constParticleVariable<Point> px;
     ParticleVariable<Matrix3> deformationGradient_new;
     constParticleVariable<Matrix3> deformationGradient;
     ParticleVariable<Matrix3> pstress;
     constParticleVariable<double> pvolumeold, pmass;
     ParticleVariable<double> pvolume_deformed;
     delt_vartype delT;

     old_dw->get(delT,lb->delTLabel, getLevel(patches));
     old_dw->get(px,                  lb->pXLabel,                  pset);
     old_dw->get(pvolumeold,          lb->pVolumeLabel,             pset);
     old_dw->get(pmass,               lb->pMassLabel,               pset);

     new_dw->allocateAndPut(pstress,         lb->pStressLabel_preReloc,   pset);
     new_dw->allocateAndPut(pvolume_deformed,lb->pVolumeDeformedLabel,    pset);
     old_dw->get(deformationGradient,        lb->pDeformationMeasureLabel,pset);
     new_dw->allocateAndPut(deformationGradient_new,
                            lb->pDeformationMeasureLabel_preReloc, pset);

     double shear = d_initialData.Shear;
     double bulk  = d_initialData.Bulk;

     double rho_orig = matl->getInitialDensity();

    if(matl->getIsRigid()){
      for(ParticleSubset::iterator iter = pset->begin();
                                   iter != pset->end(); iter++){
        particleIndex idx = *iter;
        pstress[idx] = Matrix3(0.0);
        deformationGradient_new[idx] = Identity;
        pvolume_deformed[idx] = pvolumeold[idx];
      }
    }
    else{
     LinearInterpolator* interpolator = new LinearInterpolator(patch);
     Ghost::GhostType  gac   = Ghost::AroundCells;
     if(flag->d_doGridReset){
        constNCVariable<Vector> dispNew;
        new_dw->get(dispNew,lb->dispNewLabel,dwi,patch, gac, 1);
        computeDeformationGradientFromIncrementalDisplacement(
                                                      dispNew, pset, px,
                                                      deformationGradient,
                                                      deformationGradient_new,
                                                      dx, interpolator);
     }
     else if(!flag->d_doGridReset){
        constNCVariable<Vector> gdisplacement;
        new_dw->get(gdisplacement, lb->gDisplacementLabel,dwi,patch,gac,1);
        computeDeformationGradientFromTotalDisplacement(gdisplacement,
                                                        pset, px,
                                                        deformationGradient_new,                                                        dx, interpolator);
     }

     double time = d_sharedState->getElapsedTime();

     for(ParticleSubset::iterator iter = pset->begin();
                                  iter != pset->end(); iter++){
        particleIndex idx = *iter;

        // get the volumetric part of the deformation
        double J = deformationGradient_new[idx].Determinant();

        Matrix3 bElBar_new = deformationGradient_new[idx]
                           * deformationGradient_new[idx].Transpose()
                           * pow(J,-(2./3.));

        // Shear is equal to the shear modulus times dev(bElBar)
        double mubar = 1./3. * bElBar_new.Trace()*shear;
        Matrix3 shrTrl = (bElBar_new*shear - Identity*mubar);

        double active_stress = d_active*(time+delT);
        // get the hydrostatic part of the stress
        double p = bulk*log(J)/J + active_stress;

        // compute the total stress (volumetric + deviatoric)
        pstress[idx] = Identity*p + shrTrl/J;

        double volold = (pmass[idx]/rho_orig);
        pvolume_deformed[idx] = volold*J;
      }
      delete interpolator;
     }
   }
}

void CompNeoHookImplicit::addComputesAndRequires(Task* task,
                                                 const MPMMaterial* matl,
                                                 const PatchSet* ,
                                                 const bool ) const
{
  const MaterialSubset* matlset = matl->thisMaterial();
  Ghost::GhostType  gac   = Ghost::AroundCells;

  task->requires(Task::ParentOldDW, lb->pXLabel,         matlset,Ghost::None);
  task->requires(Task::ParentOldDW, lb->pMassLabel,      matlset,Ghost::None);
  task->requires(Task::ParentOldDW, lb->pVolumeLabel,    matlset,Ghost::None);
  task->requires(Task::ParentOldDW, lb->pDeformationMeasureLabel,
                                                         matlset,Ghost::None);
  if(flag->d_doGridReset){
    task->requires(Task::OldDW, lb->dispNewLabel,        matlset,gac,1);
  }
  if(!flag->d_doGridReset){
    task->requires(Task::OldDW, lb->gDisplacementLabel,  matlset, gac, 1);
  }

  task->computes(lb->pStressLabel_preReloc,matlset);  
  task->computes(lb->pVolumeDeformedLabel, matlset);
}

void CompNeoHookImplicit::addComputesAndRequires(Task* task,
                                                 const MPMMaterial* matl,
                                                 const PatchSet*) const
{
  const MaterialSubset* matlset = matl->thisMaterial();
  Ghost::GhostType  gac   = Ghost::AroundCells;
  task->requires(Task::OldDW, lb->pXLabel,                 matlset,Ghost::None);
  task->requires(Task::OldDW, lb->pMassLabel,              matlset,Ghost::None);
  task->requires(Task::OldDW, lb->pVolumeLabel,            matlset,Ghost::None);
  task->requires(Task::OldDW, lb->pDeformationMeasureLabel,matlset,Ghost::None);
  task->requires(Task::OldDW, lb->delTLabel);
  if(flag->d_doGridReset){
    task->requires(Task::NewDW, lb->dispNewLabel,          matlset,gac,1);
  }
  if(!flag->d_doGridReset){
    task->requires(Task::NewDW, lb->gDisplacementLabel,    matlset,gac,1);
  }

  task->computes(lb->pDeformationMeasureLabel_preReloc, matlset);
  task->computes(lb->pVolumeDeformedLabel,              matlset);
  task->computes(lb->pStressLabel_preReloc,             matlset);
}

// The "CM" versions use the pressure-volume relationship of the CNH model
double CompNeoHookImplicit::computeRhoMicroCM(double pressure, 
                                      const double p_ref,
                                      const MPMMaterial* matl)
{
  double rho_orig = matl->getInitialDensity();
  double bulk = d_initialData.Bulk;
  
  double p_gauge = pressure - p_ref;
  double rho_cur;
 
  if(d_useModifiedEOS && p_gauge < 0.0) {
    double A = p_ref;           // MODIFIED EOS
    double n = p_ref/bulk;
    rho_cur = rho_orig*pow(pressure/A,n);
  } else {                      // STANDARD EOS
    rho_cur = rho_orig*(p_gauge/bulk + sqrt((p_gauge/bulk)*(p_gauge/bulk) +1));
  }
  return rho_cur;
}

void CompNeoHookImplicit::computePressEOSCM(const double rho_cur,double& pressure, 
                                    const double p_ref,
                                    double& dp_drho, double& tmp,
                                    const MPMMaterial* matl)
{
  double bulk = d_initialData.Bulk;
  double rho_orig = matl->getInitialDensity();

  if(d_useModifiedEOS && rho_cur < rho_orig){
    double A = p_ref;           // MODIFIED EOS
    double n = bulk/p_ref;
    pressure = A*pow(rho_cur/rho_orig,n);
    dp_drho  = (bulk/rho_orig)*pow(rho_cur/rho_orig,n-1);
    tmp      = dp_drho;         // speed of sound squared
  } else {                      // STANDARD EOS            
    double p_g = .5*bulk*(rho_cur/rho_orig - rho_orig/rho_cur);
    pressure   = p_ref + p_g;
    dp_drho    = .5*bulk*(rho_orig/(rho_cur*rho_cur) + 1./rho_orig);
    tmp        = bulk/rho_cur;  // speed of sound squared
  }
}

double CompNeoHookImplicit::getCompressibility()
{
  return 1.0/d_initialData.Bulk;
}

#if defined(__sgi) && !defined(__GNUC__) && (_MIPS_SIM != _MIPS_SIM_ABI32)
#pragma set woff 1209
#endif

namespace Uintah {
  
#if 0
  static MPI_Datatype makeMPI_CMData()
  {
    ASSERTEQ(sizeof(CompNeoHookImplicit::StateData), sizeof(double)*0);
    MPI_Datatype mpitype;
    MPI_Type_vector(1, 0, 0, MPI_DOUBLE, &mpitype);
    MPI_Type_commit(&mpitype);
    return mpitype;
  }
  
  const TypeDescription* fun_getTypeDescription(CompNeoHookImplicit::StateData*)
  {
    static TypeDescription* td = 0;
    if(!td){
      td = scinew TypeDescription(TypeDescription::Other,
                                  "CompNeoHookImplicit::StateData", true, 
                                  &makeMPI_CMData);
    }
    return td;
  }
#endif
} // End namespace Uintah
