
#include <Packages/Uintah/Core/Grid/SimulationState.h>
#include <Packages/Uintah/Core/Grid/VarLabel.h>
#include <Packages/Uintah/Core/Grid/ReductionVariable.h>
#include <Packages/Uintah/Core/Grid/Material.h>
#include <Packages/Uintah/CCA/Components/MPM/ConstitutiveModel/MPMMaterial.h>
#include <Packages/Uintah/CCA/Components/ICE/ICEMaterial.h>
#include <Packages/Uintah/Core/Grid/Reductions.h>
#include <Core/Malloc/Allocator.h>

using namespace Uintah;

SimulationState::SimulationState(ProblemSpecP &ps)
{
   VarLabel* nonconstDelt = scinew VarLabel("delT",
    ReductionVariable<double, Reductions::Min<double> >::getTypeDescription());
   nonconstDelt->allowMultipleComputes();
   delt_label = nonconstDelt;
   d_mpm_cfd = false;

  // Get the physical constants that are shared between codes.
  // For now it is just gravity.

  ProblemSpecP phys_cons_ps = ps->findBlock("PhysicalConstants");
  phys_cons_ps->require("gravity",d_gravity);
  phys_cons_ps->get("reference_pressure",d_ref_press);

}

void SimulationState::registerMaterial(Material* matl)
{
   matl->setDWIndex((int)matls.size());
   matls.push_back(matl);
}

void SimulationState::registerMPMMaterial(MPMMaterial* matl)
{
   mpm_matls.push_back(matl);
   registerMaterial(matl);
}

void SimulationState::registerICEMaterial(ICEMaterial* matl)
{
   ice_matls.push_back(matl);
   registerMaterial(matl);
}

int SimulationState::getNumVelFields() const {
  int num_vf=0;
  for (int i = 0; i < (int)matls.size(); i++) {
    num_vf = Max(num_vf,matls[i]->getVFIndex());
  }
  return num_vf+1;
}

SimulationState::~SimulationState()
{
  delete delt_label;

  for (int i = 0; i < (int)matls.size(); i++) 
    delete matls[i];
  for (int i = 0; i < (int)mpm_matls.size(); i++) 
    delete mpm_matls[i];
}


