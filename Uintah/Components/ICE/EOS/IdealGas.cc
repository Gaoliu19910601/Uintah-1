#include <Uintah/Components/ICE/EOS/IdealGas.h>
#include <Uintah/Grid/VarLabel.h>
#include <Uintah/Grid/CCVariable.h>
#include <Uintah/Grid/CellIterator.h>
#include <Uintah/Interface/DataWarehouse.h>
#include <Uintah/Grid/Patch.h>
#include <Uintah/Grid/VarTypes.h>
#include <SCICore/Malloc/Allocator.h>
#include <Uintah/Grid/Task.h>
#include <Uintah/Components/ICE/ICEMaterial.h>

using namespace Uintah::ICESpace;

IdealGas::IdealGas(ProblemSpecP& ps)
{
   // Constructor
  ps->require("gas_constant",d_gas_constant);
  lb = scinew ICELabel();

}

IdealGas::~IdealGas()
{
  delete lb;
}


double IdealGas::getGasConstant() const
{
  return d_gas_constant;
}

void IdealGas::initializeEOSData(const Patch* patch, const ICEMaterial* matl,
			    DataWarehouseP& new_dw)
{
}

void IdealGas::addComputesAndRequiresSS(Task* task,
				 const ICEMaterial* matl, const Patch* patch,
				 DataWarehouseP& old_dw,
				 DataWarehouseP& new_dw) const
{
  task->requires(old_dw,lb->temp_CCLabel,
		matl->getDWIndex(),patch,Ghost::None);
  task->requires(old_dw,lb->rho_micro_CCLabel,
		matl->getDWIndex(),patch,Ghost::None);
  task->requires(old_dw,lb->cv_CCLabel,
		matl->getDWIndex(),patch,Ghost::None);
  task->computes(new_dw,lb->speedSound_CCLabel,matl->getDWIndex(), patch);

}

void IdealGas::addComputesAndRequiresRM(Task* task,
				 const ICEMaterial* matl, const Patch* patch,
				 DataWarehouseP& old_dw,
				 DataWarehouseP& new_dw) const
{
  task->requires(old_dw,lb->temp_CCLabel,
		matl->getDWIndex(),patch,Ghost::None);
  task->requires(old_dw,lb->press_CCLabel,
		matl->getDWIndex(),patch,Ghost::None);
  task->requires(old_dw,lb->cv_CCLabel,
		matl->getDWIndex(),patch,Ghost::None);
  task->computes(new_dw,lb->rho_micro_CCLabel,matl->getDWIndex(), patch);

}

void IdealGas::addComputesAndRequiresPEOS(Task* task,
				 const ICEMaterial* matl, const Patch* patch,
				 DataWarehouseP& old_dw,
				 DataWarehouseP& new_dw) const
{
  task->requires(old_dw,lb->rho_micro_CCLabel,
		matl->getDWIndex(),patch,Ghost::None);
  task->requires(old_dw,lb->temp_CCLabel,
		matl->getDWIndex(),patch,Ghost::None);
  task->requires(old_dw,lb->cv_CCLabel,
		matl->getDWIndex(),patch,Ghost::None);
  task->computes(new_dw,lb->press_CCLabel,matl->getDWIndex(), patch);

}



void IdealGas::computeSpeedSound(const Patch* patch,
                                 const ICEMaterial* matl,
                                 DataWarehouseP& old_dw,
                                 DataWarehouseP& new_dw)
{
  CCVariable<double> rho_micro;
  CCVariable<double> temp;
  CCVariable<double> cv;
  CCVariable<double> speedSound;

  int vfindex = matl->getVFIndex();
  double gamma = matl->getGamma();

  old_dw->get(temp, lb->temp_CCLabel, vfindex,patch,Ghost::None, 0); 
  old_dw->get(rho_micro, lb->rho_micro_CCLabel, vfindex,patch,Ghost::None, 0); 
  old_dw->get(cv, lb->cv_CCLabel, vfindex,patch,Ghost::None, 0); 
  new_dw->allocate(speedSound,lb->speedSound_CCLabel,vfindex,patch);


  for(CellIterator iter = patch->getCellIterator(); !iter.done(); iter++){
    double dp_drho = (gamma - 1.0) * cv[*iter] * temp[*iter];
    double dp_de   = (gamma - 1.0) * rho_micro[*iter];
    double press   = (gamma - 1.0) * rho_micro[*iter]*cv[*iter]*temp[*iter];
    double denom = rho_micro[*iter]*rho_micro[*iter];
    speedSound[*iter] =  sqrt(dp_drho + dp_de* (press/(denom*denom)));
  }

  new_dw->put(speedSound,lb->speedSound_CCLabel,vfindex,patch);

}

double IdealGas::computeRhoMicro(double& press, double& gamma,
				 double& cv, double& Temp)
{
  // Pointwise computation of microscopic density
  return  press/((gamma - 1.0)*cv*Temp);
}

void IdealGas::computePressEOS(double& rhoM, double& gamma,
			       double& cv, double& Temp,
			       double& press, double& dp_drho, double& dp_de)
{
  // Pointwise computation of thermodynamic quantities
  press   = (gamma - 1.0)*rhoM*cv*Temp;
  dp_drho = (gamma - 1.0)*cv*Temp;
  dp_de   = (gamma - 1.0)*rhoM;
}


void IdealGas::computeRhoMicro(const Patch* patch,
			       const ICEMaterial* matl,
                               DataWarehouseP& old_dw,
                               DataWarehouseP& new_dw)
{

  CCVariable<double> rho_micro;
  CCVariable<double> temp;
  CCVariable<double> cv;
  CCVariable<double> press;
  
  int vfindex = matl->getVFIndex();

  old_dw->get(temp, lb->temp_CCLabel, vfindex,patch,Ghost::None, 0); 
  old_dw->get(cv, lb->cv_CCLabel, vfindex,patch,Ghost::None, 0); 
  old_dw->get(press, lb->press_CCLabel, vfindex,patch,Ghost::None, 0); 
  new_dw->allocate(rho_micro,lb->rho_micro_CCLabel,vfindex,patch);

  double gamma = matl->getGamma();

  for(CellIterator iter = patch->getCellIterator(); !iter.done(); iter++){
    rho_micro[*iter] = press[*iter]/((gamma -1.)*cv[*iter]*temp[*iter]);
  }

  new_dw->put(rho_micro,lb->rho_micro_CCLabel,vfindex,patch);


}

void IdealGas::computePressEOS(const Patch* patch,
                               const ICEMaterial* matl,
                               DataWarehouseP& old_dw,
                               DataWarehouseP& new_dw)
{

  int vfindex = matl->getVFIndex();
  CCVariable<double> rho_micro;
  CCVariable<double> temp;
  CCVariable<double> cv;
  CCVariable<double> press;

  double gamma = matl->getGamma();

  old_dw->get(temp, lb->temp_CCLabel, vfindex,patch,Ghost::None, 0); 
  old_dw->get(cv, lb->cv_CCLabel, vfindex,patch,Ghost::None, 0); 
  old_dw->get(rho_micro, lb->cv_CCLabel, vfindex,patch,Ghost::None, 0); 
  new_dw->allocate(press,lb->rho_micro_CCLabel,vfindex,patch);


  for(CellIterator iter = patch->getCellIterator(); !iter.done(); iter++){
    press[*iter] = (gamma - 1.)* rho_micro[*iter] * cv[*iter] * temp[*iter];
  }

  new_dw->put(press,lb->press_CCLabel,vfindex,patch);

}


//$Log$
//Revision 1.6  2000/10/27 23:39:54  jas
//Added gas constant to lookup.
//
//Revision 1.5  2000/10/14 02:49:50  jas
//Added implementation of compute equilibration pressure.  Still need to do
//the update of BCS and hydrostatic pressure.  Still some issues with
//computes and requires - will compile but won't run.
//
//Revision 1.4  2000/10/10 22:18:27  guilkey
//Added some simple functions
//
//Revision 1.3  2000/10/10 20:35:12  jas
//Move some stuff around.
//
//Revision 1.2  2000/10/09 22:37:04  jas
//Cleaned up labels and added more computes and requires for EOS.
//
//Revision 1.1  2000/10/06 04:02:16  jas
//Move into a separate EOS directory.
//
//Revision 1.3  2000/10/06 03:47:26  jas
//Added computes for the initialization so that step 1 works.  Added a couple
//of CC labels for step 1. Can now go thru multiple timesteps doing work
//only in step 1.
//
//Revision 1.2  2000/10/05 04:26:48  guilkey
//Added code for part of the EOS evaluation.
//
//Revision 1.1  2000/10/04 23:40:12  jas
//The skeleton framework for an EOS model.  Does nothing.
//
