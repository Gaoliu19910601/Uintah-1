#include <Packages/Uintah/CCA/Components/MPM/MPMBoundCond.h>
#include <Packages/Uintah/Core/Grid/BoundaryConditions/VelocityBoundCond.h>
#include <Packages/Uintah/Core/Grid/BoundaryConditions/SymmetryBoundCond.h>
#include <Packages/Uintah/Core/Grid/BoundaryConditions/TemperatureBoundCond.h>
#include <Packages/Uintah/Core/Grid/BoundaryConditions/PressureBoundCond.h>
#include <Core/Geometry/IntVector.h>
#include <Packages/Uintah/Core/Grid/BoundaryConditions/BCDataArray.h>
#include <Packages/Uintah/Core/Grid/Variables/NodeIterator.h>
#include <vector>
#include <iostream>

using namespace Uintah;
using namespace SCIRun;
using std::vector;
using std::cout;
using std::endl;

MPMBoundCond::MPMBoundCond()
{
}

MPMBoundCond::~MPMBoundCond()
{
}

void MPMBoundCond::setBoundaryCondition(const Patch* patch,int dwi,
                                        const string& type, 
                                        NCVariable<Vector>& variable,
                                        string interp_type)
{
  for(Patch::FaceType face = Patch::startFace;
      face <= Patch::endFace; face=Patch::nextFace(face)){
    IntVector oneCell = patch->faceDirection(face);

    if (patch->getBCType(face) == Patch::None) {
      int numChildren = patch->getBCDataArray(face)->getNumberChildren(dwi);
      IntVector l(0,0,0),h(0,0,0),off(0,0,0);
      if(interp_type=="gimp" || interp_type=="3rdorderBS"){
        patch->getFaceExtraNodes(face,0,l,h);
      }
      for (int child = 0; child < numChildren; child++) {
        Iterator nbound_ptr;
        Iterator nu;        // not used;

        if (type == "Velocity"){
         const  BoundCondBase* bcb = 
            patch->getArrayBCValues(face,dwi,"Velocity",nu,nbound_ptr,child);

          const VelocityBoundCond* bc = 
            dynamic_cast<const VelocityBoundCond*>(bcb); 
          if (bc != 0) {
            if (bc->getKind() == "Dirichlet") {
              Vector bcv = bc->getValue();
              for (nbound_ptr.reset();!nbound_ptr.done();nbound_ptr++){ 
                IntVector nd = *nbound_ptr;
                variable[nd] = bcv;
              }
              if(interp_type=="gimp" || interp_type=="3rdorderBS"){
                for(NodeIterator it(l,h); !it.done(); it++) {
                  IntVector nd = *it;
                  variable[nd] = bcv;
                }
              }
            }
            delete bc;
          } else
          delete bcb;

        } else if (type == "Symmetric"){
          const BoundCondBase* bcb =
            patch->getArrayBCValues(face,dwi,"Symmetric",nu,nbound_ptr,child);
          const SymmetryBoundCond* bc = 
            dynamic_cast<const SymmetryBoundCond*>(bcb); 
          if (bc != 0) {
            if (face == Patch::xplus || face == Patch::xminus){
              if(interp_type!="gimp"){
               for (nbound_ptr.reset(); !nbound_ptr.done();nbound_ptr++) {
                IntVector nd = *nbound_ptr;
                variable[nd] = Vector(0.,variable[nd].y(), variable[nd].z());
               }
              }
              if(interp_type=="gimp"){
                IntVector off = IntVector(1,0,0);
                IntVector L,H;
                if(face==Patch::xminus){
                  L = l+off; H = h+off;
                } else if(face==Patch::xplus){
                  L = l-off; H = h-off;
                }
                for(NodeIterator it(L,H); !it.done(); it++) {
                  IntVector nd = *it;
                  variable[nd] = Vector(0.,variable[nd].y(), variable[nd].z());
                }
                for(NodeIterator it(l,h); !it.done(); it++) {
                  IntVector nd = *it;
                  variable[nd] = Vector(0.,variable[nd].y(), variable[nd].z());
                }
              }
              if(interp_type=="3rdorderBS"){
                if (face == Patch::xplus){
                   off=IntVector(-2,0,0);
                }
                else{
                   off=IntVector(2,0,0);
                }
                for(NodeIterator it(l,h); !it.done(); it++) {
                  IntVector nd = *it;
                  IntVector od = nd + off;
                  variable[nd] = Vector(-variable[od].x(),
                                         variable[od].y(), variable[od].z());
                }
              }
            }
            if (face == Patch::yplus || face == Patch::yminus){
              if(interp_type!="gimp"){
               for (nbound_ptr.reset(); !nbound_ptr.done();nbound_ptr++){
                IntVector nd = *nbound_ptr;
                variable[nd] = Vector(variable[nd].x(),0.,variable[nd].z());
               }
              }
              if(interp_type=="gimp"){
                IntVector off = IntVector(0,1,0);
                IntVector L,H;
                if(face==Patch::yminus){
                  L = l+off; H = h+off;
                } else if(face==Patch::yplus){
                  L = l-off; H = h-off;
                }
                for(NodeIterator it(L,H); !it.done(); it++) {
                  IntVector nd = *it;
                  variable[nd] = Vector(variable[nd].x(),0.,variable[nd].z());
                }
                for(NodeIterator it(l,h); !it.done(); it++) {
                  IntVector nd = *it;
                  variable[nd] = Vector(variable[nd].x(),0.,variable[nd].z());
                }
              }
              if(interp_type=="3rdorderBS"){
                if (face == Patch::yplus){
                   off=IntVector(0,-2,0);
                }
                else{
                   off=IntVector(0,2,0);
                }
                for(NodeIterator it(l,h); !it.done(); it++) {
                  IntVector nd = *it;
                  IntVector od = nd + off;
                  variable[nd] = Vector(variable[od].x(),
                                       -variable[od].y(), variable[od].z());
                }
              }
            }
            if (face == Patch::zplus || face == Patch::zminus){
              if(interp_type!="gimp"){
               for (nbound_ptr.reset(); !nbound_ptr.done();nbound_ptr++){
                IntVector nd = *nbound_ptr;
                variable[nd] = Vector(variable[nd].x(), variable[nd].y(),0.);
               }
              }
              if(interp_type=="gimp"){
                IntVector off = IntVector(0,0,1);
                IntVector L,H;
                if(face==Patch::zminus){
                  L = l+off; H = h+off;
                } else if(face==Patch::zplus){
                  L = l-off; H = h-off;
                }
                for(NodeIterator it(L,H); !it.done(); it++) {
                  IntVector nd = *it;
                  variable[nd] = Vector(variable[nd].x(), variable[nd].y(),0.);
                }
                for(NodeIterator it(l,h); !it.done(); it++) {
                  IntVector nd = *it;
                  variable[nd] = Vector(variable[nd].x(), variable[nd].y(),0.);
                }
              }
              if(interp_type=="3rdorderBS"){
                if (face == Patch::zplus){
                   off=IntVector(0,0,-2);
                }
                else{
                   off=IntVector(0,0,2);
                }
                for(NodeIterator it(l,h); !it.done(); it++) {
                  IntVector nd = *it;
                  IntVector od = nd + off;
                  variable[nd] = Vector(variable[od].x(),
                                        variable[od].y(), -variable[od].z());
                }
              }
            }
            delete bc;
          } else
          delete bcb;
        }
      }
    } else
      continue;
  }
}

void MPMBoundCond::setBoundaryCondition(const Patch* patch,int dwi,
                                        const string& type, 
                                        NCVariable<double>& variable,
                                        string interp_type)
{
  for(Patch::FaceType face = Patch::startFace;
      face <= Patch::endFace; face=Patch::nextFace(face)){
    IntVector oneCell = patch->faceDirection(face);
    if (patch->getBCType(face) == Patch::None) {
      int numChildren = patch->getBCDataArray(face)->getNumberChildren(dwi);
      IntVector l(0,0,0),h(0,0,0);
      if(interp_type=="gimp" || interp_type=="3rdorderBS"){
        patch->getFaceExtraNodes(face,0,l,h);
      }
      for (int child = 0; child < numChildren; child++) {
        Iterator nbound_ptr;
        Iterator nu;  // not used

        if(type=="Temperature"){
          const BoundCondBase *bcb = 
            patch->getArrayBCValues(face,dwi,type,nu,nbound_ptr,child);

          const TemperatureBoundCond* bc = 
            dynamic_cast<const TemperatureBoundCond*>(bcb);
          if (bc != 0){
            if (bc->getKind() == "Dirichlet") {
              double bcv = bc->getValue();
              for (nbound_ptr.reset(); !nbound_ptr.done();nbound_ptr++){
                IntVector nd = *nbound_ptr;
                variable[nd] = bcv;
              }
              if(interp_type=="gimp" || interp_type=="3rdorderBS"){
                for(NodeIterator it(l,h); !it.done(); it++) {
                  IntVector nd = *it;
                  variable[nd] = bcv;
                }
              }
            }
            delete bc;
          } else
            delete bcb;
        }

// Used in MPMICE.
#if 1
        if(type=="Pressure"){
          const BoundCondBase *bcb = 
            patch->getArrayBCValues(face,dwi,type,nu,nbound_ptr, child);

          const PressureBoundCond* bc = 
            dynamic_cast<const PressureBoundCond*>(bcb);
          
          if (bc != 0) {
            if (bc->getKind() == "Dirichlet") {
              double bcv = bc->getValue();
              for (nbound_ptr.reset(); !nbound_ptr.done();nbound_ptr++){
                IntVector nd = *nbound_ptr;
                variable[nd] = bcv;
              }
              if(interp_type=="gimp" || interp_type=="3rdorderBS"){
                for(NodeIterator it(l,h); !it.done(); it++) {
                  IntVector nd = *it;
                  variable[nd] = bcv;
                }
              }
            }
            
            if (bc->getKind() == "Neumann" && (interp_type=="gimp" 
                                               ||  interp_type=="3rdorderBS")) {
              Vector deltax = patch->dCell();
              double dx = -9;
              IntVector off(-9,-9,-9);
              if (face == Patch::xplus){
                dx = deltax.x();
                off=IntVector(1,0,0);
              }
              else if (face == Patch::xminus){
                dx = deltax.x();
                off=IntVector(-1,0,0);
              }
              else if (face == Patch::yplus){
                dx = deltax.y();
                off=IntVector(0,1,0);
              }
              else if (face == Patch::yminus){
                dx = deltax.y();
                off=IntVector(0,-1,0);
              }
              else if (face == Patch::zplus){
                dx = deltax.z();
                off=IntVector(0,0,1);
              }
              else if (face == Patch::zminus){
                dx = deltax.z();
                off=IntVector(0,0,-1);
              }
              
              double gradv = bc->getValue();
              for(NodeIterator it(l,h); !it.done(); it++) {
                IntVector nd = *it;
                variable[nd] = variable[nd-off] + gradv*dx;
              }
            }
            
            delete bc;
          } else
          delete bcb;
        }
#endif
      }  // child
    } else
      continue;
  }
}
