
#include <Packages/Uintah/Core/Grid/Task.h>
#include <Packages/Uintah/Core/Grid/Material.h>
#include <Packages/Uintah/Core/Grid/Patch.h>
#include <Packages/Uintah/Core/Grid/TypeDescription.h>
#include <Packages/Uintah/CCA/Ports/DataWarehouse.h>
#include <Core/Exceptions/InternalError.h>
#include <Core/Malloc/Allocator.h>
#include <iostream>

using namespace Uintah;

MaterialSubset* Task::globalMatlSubset = 0;

void Task::initialize()
{
  d_resourceIndex=-1;
  comp_head=comp_tail=0;
  req_head=req_tail=0;
  mod_head=mod_tail=0;
  patch_set=0;
  matl_set=0;
}

Task::ActionBase::~ActionBase()
{
}

Task::~Task()
{
  delete d_action;
  Dependency* dep = req_head;
  while(dep){
    Dependency* next = dep->next;
    delete dep;
    dep=next;
  }
  dep = comp_head;
  while(dep){
    Dependency* next = dep->next;
    delete dep;
    dep=next;
  }
  dep = mod_head;
  while(dep){
    Dependency* next = dep->next;
    delete dep;
    dep=next;
  }
  if(matl_set && matl_set->removeReference())
    delete matl_set;
  if(patch_set && patch_set->removeReference())
    delete patch_set;
}

void Task::setSets(const PatchSet* ps, const MaterialSet* ms)
{
  ASSERT(patch_set == 0);
  ASSERT(matl_set == 0);
  patch_set=ps;
  if(patch_set)
    patch_set->addReference();
  matl_set=ms;
  if(matl_set)
    matl_set->addReference();
}

const MaterialSubset* Task::getGlobalMatlSubset()
{
  if (globalMatlSubset == 0) {
    globalMatlSubset = new MaterialSubset();
    globalMatlSubset->add(-1);
    globalMatlSubset->addReference();
  }
  return globalMatlSubset;
}

void
Task::usesMPI(bool state)
{
  d_usesMPI = state;
}

void
Task::usesThreads(bool state)
{
  d_usesThreads = state;
}

void
Task::subpatchCapable(bool state)
{
  d_subpatchCapable = state;
}

void
Task::requires(WhichDW dw, const VarLabel* var,
	       const PatchSubset* patches, DomainSpec patches_dom,
	       const MaterialSubset* matls, DomainSpec matls_dom,
	       Ghost::GhostType gtype, int numGhostCells)
{
  if (matls == 0 && var->typeDescription()->isReductionVariable()) {
    // default material for a reduction variable is the global material (-1)
    matls = getGlobalMatlSubset();
    matls_dom = OutOfDomain;
  }  
  Dependency* dep = scinew Dependency(this, dw, var, patches, matls,
				      patches_dom, matls_dom,
				      gtype, numGhostCells);
  dep->next=0;
  if(req_tail)
    req_tail->next=dep;
  else
    req_head=dep;
  req_tail=dep;
}


void
Task::requires(WhichDW dw, const VarLabel* var,
	       const PatchSubset* patches, const MaterialSubset* matls,
	       Ghost::GhostType gtype, int numGhostCells)
{
  requires(dw, var, patches, NormalDomain, matls, NormalDomain,
	   gtype, numGhostCells);
}


void
Task::requires(WhichDW dw, const VarLabel* var,
	       Ghost::GhostType gtype, int numGhostCells)
{
  requires(dw, var, 0, NormalDomain, 0, NormalDomain, gtype, numGhostCells);
}

void
Task::requires(WhichDW dw, const VarLabel* var,
	       const MaterialSubset* matls,
	       Ghost::GhostType gtype, int numGhostCells)
{
  requires(dw, var, 0, NormalDomain, matls, NormalDomain, gtype, numGhostCells);
}

void
Task::requires(WhichDW dw, const VarLabel* var,
	       const MaterialSubset* matls, DomainSpec matls_dom,
	       Ghost::GhostType gtype, int numGhostCells)
{
  requires(dw, var, 0, NormalDomain, matls, matls_dom, gtype, numGhostCells);
}

void
Task::requires(WhichDW dw, const VarLabel* var,
	       const PatchSubset* patches,
	       Ghost::GhostType gtype, int numGhostCells)
{
  requires(dw, var, patches, NormalDomain, 0, NormalDomain, gtype, numGhostCells);
}

void
Task::requires(WhichDW dw, const VarLabel* var, const PatchSubset* patches,
	       const MaterialSubset* matls)
{
  TypeDescription::Type vartype = var->typeDescription()->getType();
  if(!(vartype == TypeDescription::PerPatch
       || vartype == TypeDescription::ReductionVariable))
    throw InternalError("Requires should specify ghost type for this variable");
  requires(dw, var, patches, NormalDomain, matls, NormalDomain, Ghost::None, 0);
}

void
Task::requires(WhichDW dw, const VarLabel* var,
	       const MaterialSubset* matls)
{
  TypeDescription::Type vartype = var->typeDescription()->getType();
  if(!(vartype == TypeDescription::PerPatch
       || vartype == TypeDescription::ReductionVariable))
    throw InternalError("Requires should specify ghost type for this variable");
  requires(dw, var, 0, NormalDomain, matls, NormalDomain, Ghost::None, 0);
}

void
Task::computes(const VarLabel* var,
	       const PatchSubset* patches, DomainSpec patches_dom,
	       const MaterialSubset* matls, DomainSpec matls_dom)
{
  if (matls == 0 && var->typeDescription()->isReductionVariable()) {
    // default material for a reduction variable is the global material (-1)
    matls = getGlobalMatlSubset();
    matls_dom = OutOfDomain;
  }  

  Dependency* dep = scinew Dependency(this, NewDW, var, patches, matls,
				      patches_dom, matls_dom);
  dep->next=0;
  if(comp_tail)
    comp_tail->next=dep;
  else
    comp_head=dep;
  comp_tail=dep;
}

void
Task::computes(const VarLabel* var, const PatchSubset* patches,
               const MaterialSubset* matls)
{
  computes(var, patches, NormalDomain, matls, NormalDomain);
}

void
Task::computes(const VarLabel* var, const MaterialSubset* matls)
{
  computes(var, 0, NormalDomain, matls, NormalDomain);
}

void
Task::computes(const VarLabel* var, const MaterialSubset* matls,
	       DomainSpec matls_dom)
{
  computes(var, 0, NormalDomain, matls, matls_dom);
}

void
Task::modifies(const VarLabel* var,
	       const PatchSubset* patches, DomainSpec patches_dom,
	       const MaterialSubset* matls, DomainSpec matls_dom)
{
  if (matls == 0 && var->typeDescription()->isReductionVariable()) {
    // default material for a reduction variable is the global material (-1)
    matls = getGlobalMatlSubset();
    matls_dom = OutOfDomain;
  }  

  Dependency* dep = scinew Dependency(this, NewDW, var, patches, matls,
				      patches_dom, matls_dom);
  dep->next=0;
  if (mod_tail)
    mod_tail->next=dep;
  else
    mod_head=dep;
  mod_tail=dep;
}

void
Task::modifies(const VarLabel* var, const PatchSubset* patches,
               const MaterialSubset* matls)
{
  TypeDescription::Type vartype = var->typeDescription()->getType();
  if(!(vartype == TypeDescription::PerPatch
       || vartype == TypeDescription::ReductionVariable))
    throw InternalError("Modifies should specify ghost type for this variable");
  
  modifies(var, patches, NormalDomain, matls, NormalDomain);
}

void
Task::modifies(const VarLabel* var, const MaterialSubset* matls)
{
  modifies(var, 0, NormalDomain, matls, NormalDomain);
}

void
Task::modifies(const VarLabel* var, const MaterialSubset* matls,
	       DomainSpec matls_dom)
{
  modifies(var, 0, NormalDomain, matls, matls_dom);
}


Task::Dependency::Dependency(Task* task, WhichDW dw, const VarLabel* var,
			     const PatchSubset* patches,
			     const MaterialSubset* matls,
			     DomainSpec patches_dom,
			     DomainSpec matls_dom,
			     Ghost::GhostType gtype,
			     int numGhostCells)
: task(task), var(var), patches(patches), matls(matls),
  patches_dom(patches_dom), matls_dom(matls_dom),
  gtype(gtype), dw(dw), numGhostCells(numGhostCells)
{
  req_head=req_tail=comp_head=comp_tail=0;
  if(patches)
    patches->addReference();
  if(matls)
    matls->addReference();
}

Task::Dependency::~Dependency()
{
  if(patches && patches->removeReference())
    delete patches;
  if(matls && matls->removeReference())
    delete matls;
}

void
Task::doit(const ProcessorGroup* pc, const PatchSubset* patches,
           const MaterialSubset* matls, DataWarehouse* fromDW,
           DataWarehouse* toDW)
{
  if(d_action)
     d_action->doit(pc, patches, matls, fromDW, toDW);
}

void
Task::display( ostream & out ) const
{
  out << getName() << " (" << d_tasktype << "): [Own: " << d_resourceIndex
      << ", ";
  if( patch_set != 0 ){
    out << "Patches: {";
    for(int i=0;i<patch_set->size();i++){
      const PatchSubset* ps = patch_set->getSubset(i);
      if(i != 0)
	out << ", ";
      out << "{";
      for(int j=0;j<ps->size();j++){
	if(j != 0)
	  out << ",";
	const Patch* patch = ps->get(j);
	out << patch->getID();
      }
      out << "}";
    }
    out << "}";
  } else {
    out << "(No Patch)";
  }
  out << ", ";
  if( matl_set != 0 ){
    out << "Matls: {";
    for(int i=0;i< matl_set->size();i++){
      const MaterialSubset* ms = matl_set->getSubset(i);
      if(i != 0)
	out << ", ";
      out << "{";
      for(int j=0;j<ms->size();j++){
	if(j != 0)
	  out << ",";
	out << ms->get(j);
      }
      out << "}";
    }
    out << "}";
  } else {
    out << "(No Matls)";
  }
  out << "]";
}

ostream &
operator << ( ostream & out, const Uintah::Task::Dependency & dep )
{
  out << "[" << *(dep.var) << " Patches: ";
  if( dep.patches ){
    for(int i=0;i<dep.patches->size();i++){
      if(i > 0)
	out << ",";
      out << dep.patches->get(i)->getID();
    }
  } else {
    out << "none";
  }
  out << " MI: ";
  if(dep.matls){
    for(int i=0;i<dep.matls->size();i++){
      if(i>0)
	out << ",";
      out << dep.matls->get(i);
    }
  } else {
    out << "none";
  }
  out << ", ";
  switch(dep.dw){
  case Task::OldDW:
    out << "OldDW";
    break;
  case Task::NewDW:
    out << "NewDW";
    break;
  }
  out << ", ";
  switch(dep.gtype){
  case Ghost::None:
    out << "Ghost::None";
    break;
  case Ghost::AroundNodes:
    out << "Ghost::AroundNodes";
    break;
  case Ghost::AroundCells:
    out << "Ghost::AroundCells";
    break;
  case Ghost::AroundXFaces:
    out << "Ghost::AroundXFaces";
    break;
  case Ghost::AroundYFaces:
    out << "Ghost::AroundYFaces";
    break;
  case Ghost::AroundZFaces:
    out << "Ghost::AroundZFaces";
    break;
  case Ghost::AroundAllFaces:
    out << "Ghost::AroundAllFaces";
    break;
  }
  if(dep.gtype != Ghost::None)
    out << ":" << dep.numGhostCells;

  out << "]";
  return out;
}

void
Task::displayAll(ostream& out) const
{
   display(out);
   out << '\n';
   for(Task::Dependency* req = req_head; req != 0; req = req->next)
      out << "requires: " << *req << '\n';
   for(Task::Dependency* comp = comp_head; comp != 0; comp = comp->next)
      out << "computes: " << *comp << '\n';
   for(Task::Dependency* mod = mod_head; mod != 0; mod = mod->next)
      out << "modifies: " << *mod << '\n';
}

ostream &
operator << (ostream &out, const Task & task)
{
  task.display( out );
  return out;
}

ostream &
operator << (ostream &out, const Task::TaskType & tt)
{
  switch( tt ) {
  case Task::Normal:
    out << "Normal";
    break;
  case Task::Reduction:
    out << "Reduction";
    break;
  case Task::InitialSend:
    out << "InitialSend";
    break;
  }
  return out;
}
