#include <Packages/Uintah/CCA/Components/ICE/BoundaryCond.h>
#include <Packages/Uintah/CCA/Components/ICE/ICEMaterial.h>
#include <Packages/Uintah/Core/Grid/Grid.h>
#include <Packages/Uintah/Core/Grid/Level.h>
#include <Packages/Uintah/Core/Grid/Patch.h>
#include <Packages/Uintah/Core/Grid/PerPatch.h>
#include <Packages/Uintah/Core/Grid/SimulationState.h>
#include <Packages/Uintah/Core/Grid/Task.h>
#include <Packages/Uintah/Core/Grid/BoundCond.h>
#include <Packages/Uintah/Core/Grid/VarTypes.h>
#include <Packages/Uintah/Core/Grid/CellIterator.h>
#include <Packages/Uintah/Core/Grid/fillFace.h>
#include <typeinfo>
#include <Core/Util/DebugStream.h>
#include <Core/Exceptions/InternalError.h>

 // setenv SCI_DEBUG "ICE_BC_DBG:+,ICE_BC_DOING:+"
 // Note:  cout_dbg doesn't work if the iterator bound is
 //        not defined
static DebugStream BC_dbg(  "ICE_BC_DBG", false);
static DebugStream BC_doing("ICE_BC_DOING", false);

using namespace Uintah;
namespace Uintah {

/* --------------------------------------------------------------------- 
 Function~  ImplicitMatrixBC--      
 Purpose~   Along each face of the domain set the stencil weight in
           that face = 0
 Naming convention
      +x -x +y -y +z -z
       e, w, n, s, t, b 
 ---------------------------------------------------------------------  */
void ImplicitMatrixBC( CCVariable<Stencil7>& A, 
                   const Patch* patch)        
{ 
  for(Patch::FaceType face = Patch::startFace;
      face <= Patch::endFace; face=Patch::nextFace(face)){
      
    // only apply the BC when on the edge of the computational domain.
    // skip the faces between neighboring patches  
    bool onEdgeOfDomain = false;  
    if (patch->getBCType(face) == Patch::None) {
      onEdgeOfDomain = true;
    }
    
    if (onEdgeOfDomain){
      switch (face) {
      case Patch::xplus:
        for(CellIterator iter = patch->getFaceCellIterator(face); 
                                                 !iter.done(); iter++) { 
          IntVector c(*iter - IntVector(1,0,0));
          A[c].e = 0.0;
        }
        break;
      case Patch::xminus:
        for(CellIterator iter = patch->getFaceCellIterator(face); 
                                                 !iter.done(); iter++) { 
          IntVector c(*iter + IntVector(1,0,0));
          A[c].w = 0.0;
        }
        break;
      case Patch::yplus:
        for(CellIterator iter = patch->getFaceCellIterator(face); 
                                                 !iter.done(); iter++) { 
          IntVector c(*iter - IntVector(0,1,0));
          A[c].n = 0.0;
        }
        break;
      case Patch::yminus:
        for(CellIterator iter = patch->getFaceCellIterator(face); 
                                                 !iter.done(); iter++) { 
          IntVector c(*iter + IntVector(0,1,0)); 
          A[c].s = 0.0;
        }
        break;
      case Patch::zplus:
        for(CellIterator iter = patch->getFaceCellIterator(face); 
                                                 !iter.done(); iter++) { 
          IntVector c(*iter - IntVector(0,0,1));
          A[c].t = 0.0;
        }
        break;
      case Patch::zminus:
        for(CellIterator iter = patch->getFaceCellIterator(face); 
                                                 !iter.done(); iter++) { 
          IntVector c(*iter + IntVector(0,0,1));
          A[c].b = 0.0;
        }
        break;
      case Patch::numFaces:
        break;
      case Patch::invalidFace:
        break; 
      } 
    }  // face on edge of domain?
  }  // face loop
}
/* --------------------------------------------------------------------- 
 Function~  get_rho_micro--
 Purpose~  This handles all the logic of getting rho_micro on the faces
    a) when using lodi bcs get rho_micro for all ice and mpm matls
    b) with gravity != 0 get rho_micro on P_dir faces, for all ICE matls
    c) during initialization only get rho_micro for ice_matls, you can't
       get rho_micro for mpm_matls
 ---------------------------------------------------------------------  */
void get_rho_micro(StaticArray<CCVariable<double> >& rho_micro,
                   StaticArray<CCVariable<double> >& rho_micro_tmp,
                   StaticArray<constCCVariable<double> >& sp_vol_CC,
                   const Patch* patch,
                   const string& which_Var,
                   SimulationStateP& sharedState,
                   DataWarehouse* new_dw,
                   Lodi_vars_pressBC* lv)
{
  BC_doing << "get_rho_micro: Which_var " << which_Var << endl;
  
  if( which_Var !="rho_micro" && which_Var !="sp_vol" ){
    throw InternalError("setBC (pressure): Invalid option for which_var");
  }
  
  Vector gravity = sharedState->getGravity(); 
  int timestep = sharedState->getCurrentTopLevelTimeStep();
  int numMatls  = sharedState->getNumICEMatls();
  if ( lv->usingLODI && timestep > 0 ) {
    numMatls += sharedState->getNumMPMMatls();
  }
      
  for (int m = 0; m < numMatls; m++) {
    new_dw->allocateTemporary(rho_micro[m],  patch);
  }
  //__________________________________
  // Iterate over the faces encompassing the domain
  vector<Patch::FaceType>::const_iterator iter;
  
  for (iter  = patch->getBoundaryFaces()->begin(); 
       iter != patch->getBoundaryFaces()->end(); ++iter){
    Patch::FaceType face = *iter;
    
    if(is_LODI_face(patch, face, sharedState) || gravity.length() > 0) {
      
      //__________________________________
      // Create an iterator that iterates over the face
      // + 2 cells inward (hydrostatic press tweak).  
      // We don't need to hit every  cell on the patch. 
      CellIterator iter_tmp = patch->getFaceCellIterator(face, "plusEdgeCells");
      IntVector lo = iter_tmp.begin();
      IntVector hi = iter_tmp.end();
    
      int P_dir = patch->faceAxes(face)[0];  //principal dir.
      if(face==Patch::xminus || face==Patch::yminus || face==Patch::zminus){
        hi[P_dir] += 2;
      }
      if(face==Patch::xplus || face==Patch::yplus || face==Patch::zplus){
        lo[P_dir] -= 2;
      }
      CellIterator iterLimits(lo,hi);
      
      for (int m = 0; m < numMatls; m++) {
        if (which_Var == "rho_micro") { 
          for (CellIterator iter=iterLimits; !iter.done();iter++) {
            IntVector c = *iter;
            rho_micro[m][c] =  rho_micro_tmp[m][c];
          }
        }
        if (which_Var == "sp_vol") { 
          for (CellIterator iter=iterLimits; !iter.done();iter++) {
            IntVector c = *iter;
            rho_micro[m][c] =  1.0/sp_vol_CC[m][c];
          }
        }  // sp_vol
      }  // numMatls
    }  // LODI face or gravity != 0
  }  // face iter 
}

/* --------------------------------------------------------------------- 
 Function~  setBC-- (pressure)
 ---------------------------------------------------------------------  */
void setBC(CCVariable<double>& press_CC,
           StaticArray<CCVariable<double> >& rho_micro_tmp,   //or placeHolder
           StaticArray<constCCVariable<double> >& sp_vol_CC,  //or placeHolder
           const int surroundingMatl_indx,
           const string& which_Var,
           const string& kind, 
           const Patch* patch,
           SimulationStateP& sharedState, 
           const int mat_id,
           DataWarehouse* new_dw,
           Lodi_vars_pressBC* lv)
{
  BC_doing << "setBC (press_CC) "<< kind <<" " << which_Var
           << " mat_id = " << mat_id << endl;

  int numMatls = sharedState->getNumMatls();  
  Vector gravity = sharedState->getGravity();
  StaticArray<CCVariable<double> > rho_micro(numMatls);

  get_rho_micro(rho_micro, rho_micro_tmp, sp_vol_CC, 
                patch, which_Var, sharedState,  new_dw, lv);
  //__________________________________
  // Iterate over the faces encompassing the domain
  vector<Patch::FaceType>::const_iterator iter;

  for (iter  = patch->getBoundaryFaces()->begin(); 
       iter != patch->getBoundaryFaces()->end(); ++iter){
    Patch::FaceType face = *iter;
    bool IveSetBC = false;
    //__________________________________
    //  -Set the LODI BC's first, then let the other BC's wipe out what
    //   was set in the corners and edges. 
    //  -Ignore lodi bcs during intialization phase AND when
    //   lv->setLodiBcs = false
    bool is_lodi_pressBC = patch->haveBC(face,mat_id,"LODI","Pressure");
    int topLevelTimestep = sharedState->getCurrentTopLevelTimeStep();

    if(kind == "Pressure"      && is_lodi_pressBC 
       && topLevelTimestep > 0 && lv->setLodiBcs){
       FacePress_LODI(patch, press_CC, rho_micro, sharedState,face,lv);
    }

    //__________________________________
    //  N O N  -  L O D I
    IntVector dir= patch->faceAxes(face);
    Vector cell_dx = patch->dCell();
    double dx = cell_dx[dir[0]];
    int numChildren = patch->getBCDataArray(face)->getNumberChildren(mat_id);

    for (int child = 0;  child < numChildren; child++) {
      double bc_value = -9;
      string bc_kind = "NotSet";
      vector<IntVector> bound;
      
      getIteratorBCValueBCKind<double>( patch, face, child, kind, mat_id,
					bc_value, bound,bc_kind); 
                                   
      if(bc_kind != "NotSet" && bc_kind != "LODI") {
        // define what a symmetric  pressure BC means
        if( bc_kind == "symmetric"){
          bc_kind = "zeroNeumann";
        }
        
        IveSetBC = setNeumanDirichletBC<double>(patch, face, press_CC,bound, 
						  bc_kind, bc_value, cell_dx,
						  mat_id,child);
        //__________________________________________________________
        // Tack on hydrostatic pressure after Dirichlet or Neumann BC
        // has been applied.  Note, during the intializaton phase the 
        // hydrostatic pressure adjustment is  handled by a completely
        // separate task, therefore don't do this on the first DW
        // 
        //  Change gravity sign according to the face direction
        int p_dir = patch->faceAxes(face)[0];     // principal  face direction
        
        if (gravity[p_dir] != 0 && topLevelTimestep > 0) {
          Vector faceDir = patch->faceDirection(face).asVector();
          double grav = gravity[p_dir] * (double)faceDir[p_dir]; 
          IntVector oneCell = patch->faceDirection(face);
          vector<IntVector>::const_iterator iter;

          for (iter=bound.begin();iter != bound.end(); iter++) { 
            IntVector L = *iter - oneCell;
            IntVector R = *iter;
            double rho_R = rho_micro[surroundingMatl_indx][R];
            double rho_L = rho_micro[surroundingMatl_indx][L];
            double rho_micro_brack = 2.*(rho_L * rho_R)/(rho_L + rho_R);
            press_CC[R] += grav * dx * rho_micro_brack; 
          }
          IveSetBC = true;
        }  // with gravity 
      }  // if bcKind != notSet
      //__________________________________
      //  debugging
      if( BC_dbg.active() ) {
        BC_dbg <<"Face: "<< face <<" I've set BC " << IveSetBC
             <<"\t child " << child  <<" NumChildren "<<numChildren 
             <<"\t BC kind "<< bc_kind <<" \tBC value "<< bc_value
             <<"\t bound limits = "<< *bound.begin()<< " "<< *(bound.end()-1)
	      << endl;
      }
    }  // child loop
  }  // faces loop
}
/* --------------------------------------------------------------------- 
 Function~  setBC--
 Purpose~   Takes care any CC variable, except Pressure
 ---------------------------------------------------------------------  */
void setBC(CCVariable<double>& var_CC,
           const string& desc,
           const CCVariable<double>& gamma,
           const CCVariable<double>& cv,
           const Patch* patch,
           SimulationStateP& sharedState, 
           const int mat_id,
           Lodi_vars* lv)
{
  BC_doing << "setBC (double) "<< desc << " mat_id = " << mat_id << endl;
  Vector cell_dx = patch->dCell();
  
  //__________________________________
  // Iterate over the faces encompassing the domain
  vector<Patch::FaceType>::const_iterator iter;
  for (iter  = patch->getBoundaryFaces()->begin(); 
       iter != patch->getBoundaryFaces()->end(); ++iter){
    Patch::FaceType face = *iter;
          
    bool IveSetBC = false;
    
    //__________________________________
    //  -Set the LODI BC's first, then let the other BC's wipe out what
    //   was set in the corners and edges. 
    //  -Ignore lodi bcs during intialization phase and when
    //   lv->setLodiBcs = false
    bool is_tempBC_lodi=  patch->haveBC(face,mat_id,"LODI","Temperature");  
    bool is_rhoBC_lodi =  patch->haveBC(face,mat_id,"LODI","Density");   
    int topLevelTimestep = sharedState->getCurrentTopLevelTimeStep();   
    
    if( desc == "Temperature"  && is_tempBC_lodi 
        && topLevelTimestep >0 && lv->setLodiBcs ){
      FaceTemp_LODI(patch, face, var_CC, lv, cell_dx);
    }   
    if (desc == "Density"  && is_rhoBC_lodi 
        && topLevelTimestep >0 && lv->setLodiBcs){
      FaceDensity_LODI(patch, face, var_CC, lv, cell_dx);
    }
    
    //__________________________________
    //  N O N  -  L O D I
    int numChildren = patch->getBCDataArray(face)->getNumberChildren(mat_id);

    for (int child = 0;  child < numChildren; child++) {
      double bc_value = -9;
      string bc_kind = "NotSet";
      vector<IntVector> bound;
      
      getIteratorBCValueBCKind<double>( patch, face, child, desc, mat_id,
					     bc_value, bound,bc_kind); 
      
      if (bc_kind != "NotSet" && bc_kind != "LODI") {
        //__________________________________
        // LOGIC
        // Any CC Variable
        if (desc == "set_if_sym_BC" && bc_kind == "symmetric"){
          bc_kind = "zeroNeumann";
        }
        if ( bc_kind == "symmetric"){
          bc_kind = "zeroNeumann";
        }

        //__________________________________
        // Apply the boundary condition
        IveSetBC =  setNeumanDirichletBC<double>
	  (patch, face, var_CC,bound, bc_kind, bc_value, cell_dx,mat_id,child);

        //__________________________________
        // Temperature and Gravity and ICE Matls
        // -Ignore this during intialization phase,
        //  since we backout the temperature field
        Vector gravity = sharedState->getGravity();                             
        Material *matl = sharedState->getMaterial(mat_id);
        ICEMaterial* ice_matl = dynamic_cast<ICEMaterial*>(matl);
        int P_dir =  patch->faceAxes(face)[0];  // principal direction
        
        if (gravity[P_dir] != 0 && desc == "Temperature" && ice_matl 
             && topLevelTimestep >0) {
          ice_matl->getEOS()->
              hydrostaticTempAdjustment(face, patch, bound, gravity,
                                        gamma, cv, cell_dx, var_CC);
        }
        //__________________________________
        //  debugging
        if( BC_dbg.active() ) {
          BC_dbg <<"Face: "<< face <<" I've set BC " << IveSetBC
               <<"\t child " << child  <<" NumChildren "<<numChildren 
               <<"\t BC kind "<< bc_kind <<" \tBC value "<< bc_value
               <<"\t bound limits = "<< *bound.begin()<< " "<< *(bound.end()-1)
	        << endl;
        }
      }  // if bc_kind != notSet  
    }  // child loop
  }  // faces loop
}

/* --------------------------------------------------------------------- 
 Function~  setBC--
 Purpose~   Takes care vector boundary condition
 ---------------------------------------------------------------------  */
void setBC(CCVariable<Vector>& var_CC,
           const string& desc,
           const Patch* patch,
           SimulationStateP& sharedState, 
           const int mat_id,
           Lodi_vars* lv)
{
  BC_doing <<"setBC (Vector_CC) "<< desc <<" mat_id = " <<mat_id<< endl;
  Vector cell_dx = patch->dCell();
  
  //__________________________________
  // Iterate over the faces encompassing the domain
  vector<Patch::FaceType>::const_iterator iter;
  for (iter  = patch->getBoundaryFaces()->begin(); 
       iter != patch->getBoundaryFaces()->end(); ++iter){
    Patch::FaceType face = *iter;
    bool IveSetBC = false;
    
    //__________________________________
    //  -Set the LODI BC's first, then let the other BC's wipe out what
    //   was set in the corners and edges. 
    //  -Ignore lodi bcs during intialization phase and when
    //   lv->setLodiBcs = false
    bool is_velBC_lodi   =  patch->haveBC(face,mat_id,"LODI","Velocity");
    int topLevelTimestep = sharedState->getCurrentTopLevelTimeStep();
    
    if( desc == "Velocity"      && is_velBC_lodi 
        && topLevelTimestep > 0 && lv->setLodiBcs) {
      FaceVel_LODI( patch, face, var_CC, lv, cell_dx, sharedState);
    }
    
    //__________________________________
    //  N O N  -  L O D I
    IntVector oneCell = patch->faceDirection(face);
    int numChildren = patch->getBCDataArray(face)->getNumberChildren(mat_id);

    for (int child = 0;  child < numChildren; child++) {
      Vector bc_value = Vector(-9,-9,-9);
      string bc_kind = "NotSet";
      vector<IntVector> bound;
      getIteratorBCValueBCKind<Vector>(patch, face, child, desc, mat_id,
				           bc_value, bound,bc_kind);
     
      if (bc_kind != "NotSet" && bc_kind != "LODI") {
 
        IveSetBC = setNeumanDirichletBC<Vector>(patch, face, var_CC,bound, 
						bc_kind, bc_value, cell_dx,
						mat_id,child);

        //__________________________________
        //  Tangent components Neumann = 0
        //  Normal components = -variable[Interior]
        //  It's negInterior since it's on the opposite side of the
        //  plane of symetry  
        if ( bc_kind == "symmetric" &&
            (desc == "Velocity" || desc == "set_if_sym_BC" ) ) {
          int P_dir = patch->faceAxes(face)[0];  // principal direction
          IntVector sign = IntVector(1,1,1);
          sign[P_dir] = -1;
          vector<IntVector>::const_iterator iter;

          for (iter=bound.begin(); iter != bound.end(); iter++) {
            IntVector adjCell = *iter - oneCell;
            var_CC[*iter] = sign.asVector() * var_CC[adjCell];
          }
          IveSetBC = true;
          bc_value = Vector(0,0,0); // so the debugging output is accurate
        }
        //__________________________________
        //  debugging
        if( BC_dbg.active() ) {
          BC_dbg <<"Face: "<< face <<" I've set BC " << IveSetBC
               <<"\t child " << child  <<" NumChildren "<<numChildren 
               <<"\t BC kind "<< bc_kind <<" \tBC value "<< bc_value
               <<"\t bound limits = " <<*bound.begin()<<" "<< *(bound.end()-1)
	        << endl;
        }
      }  // if (bcKind != "notSet"    
    }  // child loop
  }  // faces loop
}


//______________________________________________________________________
//______________________________________________________________________
//      S T U B   F U N C T I O N S

void setBC(CCVariable<double>& var,     
          const std::string& type,     // so gcc compiles
          const Patch* patch,  
          SimulationStateP& sharedState,
          const int mat_id)
{
  Lodi_vars* lv = new Lodi_vars();
  lv->setLodiBcs = false;
  constCCVariable<double> placeHolder;
  
  setBC(var, type, placeHolder, placeHolder, patch, sharedState, mat_id, lv);
  
  delete lv;
} 
  
void setBC(CCVariable<double>& press_CC,          
         StaticArray<CCVariable<double> >& rho_micro,
         StaticArray<constCCVariable<double> >& sp_vol,
         const int surroundingMatl_indx,
         const std::string& whichVar, 
         const std::string& kind, 
         const Patch* p, 
         SimulationStateP& sharedState,
         const int mat_id, 
         DataWarehouse* new_dw) {
  Lodi_vars_pressBC* lv = new Lodi_vars_pressBC(0);
  lv->setLodiBcs = false;
  
  setBC(press_CC, rho_micro, sp_vol, surroundingMatl_indx,
        whichVar, kind, p, sharedState, mat_id, new_dw, lv); 
        
  delete lv;           
}          
void setBC(CCVariable<Vector>& variable,
          const std::string& type,
          const Patch* p,
          SimulationStateP& sharedState,
          const int mat_id)
{ 
  Lodi_vars* lv = new Lodi_vars();
  lv->setLodiBcs = false;
  
  setBC( variable, type, p, sharedState, mat_id, lv);
  
  delete lv; 
}


}  // using namespace Uintah
