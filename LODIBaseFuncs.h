#include <Packages/Uintah/CCA/Components/ICE/ICEMaterial.h>
#include <Packages/Uintah/CCA/Components/MPM/ConstitutiveModel/MPMMaterial.h>
#include <Packages/Uintah/CCA/Components/MPM/ConstitutiveModel/ConstitutiveModel.h>
#include <Packages/Uintah/Core/Grid/Patch.h>
#include <Packages/Uintah/Core/Grid/SimulationState.h>
#include <Packages/Uintah/Core/Grid/CellIterator.h>
#include <Core/Util/DebugStream.h>
#include <typeinfo>

using namespace Uintah;
namespace Uintah {

#ifdef LODI_BCS     // defined in BoundaryCond.h
//__________________________________
//  To turn on couts
//  setenv SCI_DEBUG "LODI_DOING_COUT:+, LODI_DBG_COUT:+"
static DebugStream cout_doing("LODI_DOING_COUT", false);
static DebugStream cout_dbg("LODI_DBG_COUT", false);
       
/*________________________________________________________
 Function~ computeConvection--
 Purpose~  Compute the convection term in conservation law
_________________________________________________________*/
double computeConvection(const double& nuFrt,     const double& nuMid, 
                         const double& nuLast,    const double& qFrt, 
                         const double& qMid,      const double& qLast,
                         const double& qConFrt,   const double& qConLast,
                         const double& deltaT,    const double& deltaX) 
{
   //__________________________________
   // Artifical dissipation term
   double eplus, eminus, dissipation;
   double k_const = 0.4;

   eplus  = 0.5 * k_const * deltaX * (nuFrt   + nuMid)/deltaT;
   eminus = 0.5 * k_const * deltaX * (nuLast  + nuMid)/deltaT;
   dissipation = (eplus * qFrt - (eplus + eminus) * qMid 
              +  eminus * qLast)/deltaX; 
 
/*`==========TESTING==========*/
 dissipation  = 0; 
/*==========TESTING==========`*/
             
   return  0.5 * (qConFrt - qConLast)/deltaX - dissipation;
} 

/*_________________________________________________________________
 Function~ getBoundaryEdges--
 Purpose~  returns a list of edges where boundary conditions
           should be applied.
___________________________________________________________________*/  
void getBoundaryEdges(const Patch* patch,
                      const Patch::FaceType face,
                      vector<Patch::FaceType>& face0)
{
  IntVector patchNeighborLow  = patch->neighborsLow();
  IntVector patchNeighborHigh = patch->neighborsHigh();
  
  //__________________________________
  // Looking down on the face, examine 
  // each edge (clockwise).  If there
  // are no neighboring patches then it's a valid edge
  
  IntVector axes = patch->faceAxes(face);
  int P_dir = axes[0];  // principal direction
  int dir1  = axes[1];  // other vector directions
  int dir2  = axes[2];   
  IntVector minus(Patch::xminus, Patch::yminus, Patch::zminus);
  IntVector plus(Patch::xplus,  Patch::yplus,  Patch::zplus);
  
  if ( patchNeighborLow[dir1] == 1 ){
   face0.push_back(Patch::FaceType(minus[dir1]));
  }  
  if ( patchNeighborHigh[dir1] == 1 ) {
   face0.push_back(Patch::FaceType(plus[dir1]));
  }
  if ( patchNeighborLow[dir2] == 1 ){
   face0.push_back(Patch::FaceType(minus[dir2]));
  }  
  if ( patchNeighborHigh[dir2] == 1 ) {
   face0.push_back(Patch::FaceType(plus[dir2]));
  }
  

  //__________________________________
  //  P A T R I C K:  
  // please uncomment this and 
  // verify for yourself that we're hitting the
  // correct edges for multipatch problems.
#if 0
  vector<Patch::FaceType>::iterator itr;
  for(itr = face0.begin(); itr != face0.end(); ++ itr ) {
    Patch::FaceType c = *itr;
    cout << "getBoundaryEdges: face " 
         << face << " patch " << patch->getID() << " face0 " << c << endl;
  }
#endif
}
/*_________________________________________________________________
 Function~ computeCornerCellIndices--
 Purpose~ return a list of indicies of the corner cells that are on the
           boundaries of the domain
___________________________________________________________________*/  
void computeCornerCellIndices(const Patch* patch,
                              const Patch::FaceType face,
                              vector<IntVector>& crn)
{     
  IntVector low,hi;
  low = patch->getLowIndex();
  hi  = patch->getHighIndex() - IntVector(1,1,1);  
  
  IntVector patchNeighborLow  = patch->neighborsLow();
  IntVector patchNeighborHigh = patch->neighborsHigh();
 
  IntVector axes = patch->faceAxes(face);
  int P_dir = axes[0];  // principal direction
  int dir1  = axes[1];  // other vector directions
  int dir2  = axes[2]; 

  //__________________________________
  // main index for that face plane
  int plusMinus = patch->faceDirection(face)[P_dir];
  int main_index = 0;
  if( plusMinus == 1 ) { // plus face
    main_index = hi[P_dir];
  } else {               // minus faces
    main_index = low[P_dir];
  }

  //__________________________________
  // Looking down on the face examine 
  // each corner (clockwise) and if there
  // are no neighboring patches then set the
  // index
  // 
  // Top-right corner
  
  IntVector corner(-9,-9,-9);
  if ( patchNeighborHigh[dir1] == 1 && patchNeighborHigh[dir2] == 1) {
   corner[P_dir] = main_index;
   corner[dir1]  = hi[dir1];
   corner[dir2]  = hi[dir2];
   crn.push_back(corner);
  }
  // bottom-right corner
  if ( patchNeighborLow[dir1] == 1 && patchNeighborHigh[dir2] == 1) {
   corner[P_dir] = main_index;
   corner[dir1]  = low[dir1];
   corner[dir2]  = hi[dir2];
   crn.push_back(corner);
  } 
  // bottom-left corner
  if ( patchNeighborLow[dir1] == 1 && patchNeighborLow[dir2] == 1) {
   corner[P_dir] = main_index;
   corner[dir1]  = low[dir1];
   corner[dir2]  = low[dir2];
   crn.push_back(corner);
  } 
  // Top-left corner
  if ( patchNeighborHigh[dir1] == 1 && patchNeighborLow[dir2] == 1) {
   corner[P_dir] = main_index;
   corner[dir1]  = hi[dir1];
   corner[dir2]  = low[dir2];
   crn.push_back(corner);
  } 
  
 //__________________________________
 //  P A T R I C K:  
 // please uncomment this and 
 // verify for yourself that we're hitting the
 // correct corners for multipatch problems.
#if 0
  vector<IntVector>::iterator itr;
  for(itr = crn.begin(); itr != crn.end(); ++ itr ) {
    IntVector c = *itr;
    cout << " face " << face << " patch " << patch->getID() << " corner idx " << c << endl;
  }
#endif
}
/*_________________________________________________________________
 Function~ otherDirection--
 Purpose~  returns the remaining vector component.
___________________________________________________________________*/  
int otherDirection(int dir1, int dir2)
{ 
  int dir3 = -9;
  if ((dir1 == 0 && dir2 == 1) || (dir1 == 1 && dir2 == 0) ){  // x, y
    dir3 = 2; // z
  }
  if ((dir1 == 0 && dir2 == 2) || (dir1 == 2 && dir2 == 0) ){  // x, z
    dir3 = 1; // y
  }
  if ((dir1 == 1 && dir2 == 2) || (dir1 == 2 && dir2 == 1) ){   // y, z
    dir3 = 0; //x
  }
  return dir3;
}
/*_________________________________________________________________
 Function~ FaceDensityLODI--
 Purpose~  Compute density in boundary cells on any face
___________________________________________________________________*/
void FaceDensityLODI(const Patch* patch,
                const Patch::FaceType face,
                CCVariable<double>& rho_CC,
                StaticArray<CCVariable<Vector> >& d,
                const CCVariable<Vector>& nu,
                const CCVariable<double>& rho_tmp,
                const CCVariable<Vector>& vel,
                const double delT,
                const Vector& dx)
{
  cout_doing << "I am in FaceDensityLODI on face " << face<<endl;
  double conv_dir1, conv_dir2;
  double qConFrt,qConLast;
  
  IntVector axes = patch->faceAxes(face);
  int P_dir = axes[0];  // principal direction
  int dir1  = axes[1];  // other vector directions
  int dir2  = axes[2];
  
  IntVector offset = IntVector(1,1,1) - Abs(patch->faceDirection(face));
  
  for(CellIterator iter=patch->getFaceCellIterator(face, "minusEdgeCells"); 
                                                      !iter.done();iter++) {
    IntVector c = *iter;
    
    IntVector r1 = c;
    IntVector l1 = c;
    r1[dir1] += offset[dir1];  // tweak the r and l cell indices
    l1[dir1] -= offset[dir1];
    qConFrt  = rho_tmp[r1] * vel[r1][dir1];
    qConLast = rho_tmp[l1] * vel[l1][dir1];
    
    conv_dir1 = computeConvection(nu[r1][dir1], nu[c][dir1], nu[l1][dir1], 
                                  rho_tmp[r1], rho_tmp[c], rho_tmp[l1], 
                                  qConFrt, qConLast, delT, dx[dir1]);
    IntVector r2 = c;
    IntVector l2 = c;
    r2[dir2] += offset[dir2];  // tweak the r and l cell indices
    l2[dir2] -= offset[dir2];
    
    qConFrt  = rho_tmp[r2] * vel[r2][dir2];
    qConLast = rho_tmp[l2] * vel[l2][dir2];
    conv_dir2 = computeConvection(nu[r2][dir2], nu[c][dir2], nu[l2][dir2],
                                  rho_tmp[r2], rho_tmp[c], rho_tmp[l2], 
                                  qConFrt, qConLast, delT, dx[dir2]);

    rho_CC[c] = rho_tmp[c] - delT * (d[1][c][P_dir] + conv_dir1 + conv_dir2);
  }
 
  //__________________________________
  //    E D G E S  -- on boundaryFaces only
  vector<Patch::FaceType> b_faces;
  getBoundaryEdges(patch,face,b_faces);
  
  vector<Patch::FaceType>::const_iterator iter;  
  for(iter = b_faces.begin(); iter != b_faces.end(); ++ iter ) {
    Patch::FaceType face0 = *iter;
    
    //__________________________________
    //  Find the offset for the r and l cells
    //  and the Vector components Edir1 and Edir2
    //  for this particular edge
    IntVector offset = IntVector(1,1,1)  - Abs(patch->faceDirection(face)) 
                                         - Abs(patch->faceDirection(face0));
           
    IntVector axes = patch->faceAxes(face0);
    int Edir1 = axes[0];
    int Edir2 = otherDirection(P_dir, Edir1);
     
    CellIterator iterLimits =  
                  patch->getEdgeCellIterator(face, face0, "minusCornerCells");
                  
    for(CellIterator iter = iterLimits;!iter.done();iter++){ 
      IntVector c = *iter;  
      IntVector r = c + offset;  
      IntVector l = c - offset;

      qConFrt  = rho_tmp[r] * vel[r][Edir2];
      qConLast = rho_tmp[l] * vel[l][Edir2];
      double conv = computeConvection(nu[r][Edir2], nu[c][Edir2], nu[l][Edir2],
                                rho_tmp[r], rho_tmp[c], rho_tmp[l], 
                                qConFrt, qConLast, delT, dx[Edir2]);
                                
      rho_CC[c] = rho_tmp[c] - delT * (d[1][c][P_dir] + d[1][c][Edir1] + conv);
    }
  }

  //__________________________________
  // C O R N E R S    
  vector<IntVector> crn;
  computeCornerCellIndices(patch, face, crn);
 
  vector<IntVector>::iterator itr;
  for(itr = crn.begin(); itr != crn.end(); ++ itr ) {
    IntVector c = *itr;
    rho_CC[c] = rho_tmp[c] 
              - delT * (d[1][c][P_dir] + d[1][c][dir1] + d[1][c][dir2]);
  }           
}

/*_________________________________________________________________
 Function~ FaceVelLODI--
 Purpose~  Compute velocity in boundary cells on x_plus face
___________________________________________________________________*/
void FaceVelLODI(const Patch* patch,
                 Patch::FaceType face,                 
                 CCVariable<Vector>& vel_CC,           
                 StaticArray<CCVariable<Vector> >& d,  
                 const CCVariable<Vector>& nu,         
                 const CCVariable<double>& rho_tmp,    
                 const CCVariable<double>& p,          
                 const CCVariable<Vector>& vel,        
                 const double delT,                    
                 const Vector& dx)                     

{
  cout_doing << " I am in FaceVelLODI on face " << face << endl;
  IntVector axes = patch->faceAxes(face);
  int P_dir = axes[0];  // principal direction
  int dir1  = axes[1];  // other vector directions
  int dir2  = axes[2];
  
  IntVector offset = IntVector(1,1,1) - Abs(patch->faceDirection(face));
 
  for(CellIterator iter=patch->getFaceCellIterator(face, "minusEdgeCells"); 
                                                      !iter.done();iter++) {
    IntVector c = *iter;
    
    //__________________________________
    // convective terms
    IntVector r1 = c;
    IntVector l1 = c;
    r1[dir1] += offset[dir1];  // tweak the r and l cell indices
    l1[dir1] -= offset[dir1]; 
       
    Vector convect1(0,0,0);
    for(int dir = 0; dir <3; dir ++ ) {
      convect1[dir] = 
        0.5 * ( (rho_tmp[r1] * vel[r1][dir] * vel[r1][dir1]
              -  rho_tmp[l1] * vel[l1][dir] * vel[l1][dir1] )/dx[dir1] );
    }
    
    IntVector r2 = c;
    IntVector l2 = c;
    r2[dir2] += offset[dir2];  // tweak the r and l cell indices
    l2[dir2] -= offset[dir2];  
    
    Vector convect2(0,0,0);
     for(int dir = 0; dir <3; dir ++ ) {
       convect2[dir] = 
         0.5 * ( (rho_tmp[r2] * vel[r2][dir] * vel[r2][dir2]
               -  rho_tmp[l2] * vel[l2][dir] * vel[l2][dir2] )/dx[dir2] );
     }       
    //__________________________________
    // Pressure gradient terms
    Vector pressGradient(0,0,0); 
    pressGradient[dir1] = 0.5 * (p[r1] - p[l1])/dx[dir1];  
    pressGradient[dir2] = 0.5 * (p[r2] - p[l2])/dx[dir2];   
    
    //__________________________________
    // Equation 9.9 - 9.10
    vel_CC[c][P_dir] =
           rho_tmp[c] * vel[c][P_dir] - delT * ( vel[c][P_dir] * d[1][c][P_dir] 
                                             + rho_tmp[c]      * d[3][c][P_dir]
                                             + convect1[P_dir] + convect2[P_dir]
                                             +   pressGradient[P_dir] );     
    vel_CC[c][dir1]  =
           rho_tmp[c] * vel[c][dir1] - delT * ( vel[c][dir1] * d[1][c][P_dir]
                                             +  rho_tmp[c]   * d[4][c][P_dir] 
                                             +  convect1[dir1] + convect2[dir1]
                                             +  pressGradient[dir1] );
    vel_CC[c][dir2] =
           rho_tmp[c] * vel[c][dir2] - delT * ( vel[c][dir2] * d[1][c][P_dir]
                                             +  rho_tmp[c]   * d[5][c][P_dir]
                                             +  convect1[dir2] + convect2[dir2]
                                             +  pressGradient[dir2] );
    vel_CC[c] /= rho_tmp[c];
  }
  //__________________________________
  //    E D G E S  -- on boundaryFaces only
  vector<Patch::FaceType> b_faces;
  getBoundaryEdges(patch,face,b_faces);
  
  vector<Patch::FaceType>::const_iterator iter;  
  for(iter = b_faces.begin(); iter != b_faces.end(); ++ iter ) {
    Patch::FaceType face0 = *iter;
       
    //__________________________________
    //  Find the offset for the r and l cells
    //  and the Vector components Edir1 and Edir2
    //  for this particular edge
    IntVector offset = IntVector(1,1,1)  - Abs(patch->faceDirection(face)) 
                                         - Abs(patch->faceDirection(face0));
           
    IntVector axes = patch->faceAxes(face0);
    int Edir1 = axes[0];
    int Edir2 = otherDirection(P_dir, Edir1);
     
    CellIterator iterLimits =  
                  patch->getEdgeCellIterator(face, face0, "minusCornerCells");
                  
    for(CellIterator iter = iterLimits;!iter.done();iter++){ 
      IntVector c = *iter;

      //__________________________________
      // convective terms
      IntVector r1 = c;
      IntVector l1 = c;
      r1[Edir2] += offset[Edir2];  // tweak the r and l cell indices
      l1[Edir2] -= offset[Edir2]; 

      Vector convect1(0,0,0);
      for(int dir = 0; dir <3; dir ++ ) {
        convect1[dir] = 
          0.5 * ( (rho_tmp[r1] * vel[r1][dir] * vel[r1][Edir2]
                -  rho_tmp[l1] * vel[l1][dir] * vel[l1][Edir2] )/dx[Edir2] );
      }
      //__________________________________
      // Pressure gradient terms
      Vector pressGradient(0,0,0); 
      pressGradient[Edir2] = 0.5 * (p[r1] - p[l1])/dx[Edir2];  

      //__________________________________
      // Equation 9.9 - 9.10
      vel_CC[c][P_dir]= rho_tmp[c] * vel[c][P_dir] 
                      - delT * ( vel[c][P_dir] * (d[1][c][P_dir] + d[1][c][Edir1])
                             +   rho_tmp[c]    * (d[3][c][P_dir] + d[4][c][Edir1])
                             +   convect1[P_dir]
                             +   pressGradient[P_dir] );
      vel_CC[c][Edir1]= rho_tmp[c] * vel[c][Edir1]
                      - delT * ( vel[c][Edir1] * (d[1][c][P_dir] + d[1][c][Edir1])
                             +  rho_tmp[c]     * (d[4][c][P_dir] + d[3][c][Edir1])
                             +  convect1[Edir1]
                             +  pressGradient[Edir1] );
      vel_CC[c][Edir2]= rho_tmp[c] * vel[c][Edir2]
                      - delT * ( vel[c][Edir2] * (d[1][c][P_dir] + d[1][c][Edir1])
                             +  rho_tmp[c]     * (d[5][c][P_dir] + d[5][c][Edir1])
                             +  convect1[Edir2]
                             +  pressGradient[Edir2] );
      vel_CC[c] /= rho_tmp[c];
    }
  }  
   //________________________________________________________
   // C O R N E R S    
   vector<IntVector> crn;
   double uVel, vVel, wVel;
   computeCornerCellIndices(patch, face, crn);

   vector<IntVector>::iterator itr;
   for(itr = crn.begin(); itr != crn.end(); ++ itr ) {
     IntVector c = *itr;
     uVel = rho_tmp[c] * vel[c].x()   - delT 
          * ((d[1][c].x() + d[1][c].y() + d[1][c].z()) * vel[c].x()  
          +  (d[3][c].x() + d[4][c].y() + d[5][c].z()) * rho_tmp[c]);

     vVel = rho_tmp[c] * vel[c].y()   - delT 
          * ((d[1][c].x() + d[1][c].y() + d[1][c].z()) * vel[c].y() 
          +  (d[4][c].x() + d[3][c].y() + d[4][c].z()) * rho_tmp[c]);

     wVel = rho_tmp[c] * vel[c].z()   - delT 
          * ((d[1][c].x() + d[1][c].y() + d[1][c].z()) * vel[c].z() 
          +  (d[5][c].x() + d[5][c].y() + d[3][c].z()) * rho_tmp[c]);
          
     vel_CC[c] = Vector(uVel, vVel, wVel)/ rho_tmp[c];
   }
} //end of the function FaceVelLODI() 

/*_________________________________________________________________
 Function~ FaceTempLODI--
 Purpose~  Compute temperature in boundary cells on faces
___________________________________________________________________*/
void FaceTempLODI(const Patch* patch,
             const Patch::FaceType face,
             CCVariable<double>& temp_CC, 
             StaticArray<CCVariable<Vector> >& d,
             const CCVariable<double>& e,
             const CCVariable<double>& rho_CC,
             const CCVariable<Vector>& nu,
             const CCVariable<double>& rho_tmp,
             const CCVariable<double>& p,
             const CCVariable<Vector>& vel,
             const double delT,
             const double cv,
             const double gamma, 
             const Vector& dx)
{
  cout_doing << " I am in FaceTempLODI on face " <<face<< endl;    

  double qConFrt,qConLast, conv_dir1, conv_dir2;
  double term1, term2, term3;
  
  IntVector axes = patch->faceAxes(face);
  int P_dir = axes[0];  // principal direction
  int dir1  = axes[1];  // other vector directions
  int dir2  = axes[2];
  
  IntVector offset = IntVector(1,1,1) - Abs(patch->faceDirection(face));
  
  for(CellIterator iter=patch->getFaceCellIterator(face, "minusEdgeCells"); 
                                                      !iter.done();iter++) {
    IntVector c = *iter;
    
    IntVector r1 = c;
    IntVector l1 = c;
    r1[dir1] += offset[dir1];  // tweak the r and l cell indices
    l1[dir1] -= offset[dir1];
    qConFrt  = vel[r1][dir1] * (e[r1] + p[r1]);
    qConLast = vel[l1][dir1] * (e[l1] + p[l1]);
    
    conv_dir1 = computeConvection(nu[r1][dir1], nu[c][dir1], nu[l1][dir1], 
                                  e[r1], e[c], e[l1], 
                                  qConFrt, qConLast, delT, dx[dir1]);
    IntVector r2 = c;
    IntVector l2 = c;
    r2[dir2] += offset[dir2];  // tweak the r and l cell indices
    l2[dir2] -= offset[dir2];
    
    qConFrt  = vel[r2][dir2] * (e[r2] + p[r2]);
    qConLast = vel[l2][dir2] * (e[l2] + p[l2]);
    conv_dir2 = computeConvection(nu[r2][dir2], nu[c][dir2], nu[l2][dir2],
                                  e[r2], e[c], e[l2], 
                                  qConFrt, qConLast, delT, dx[dir2]);


    double vel_sqr = vel[c].length2();
    term1 = 0.5 * d[1][c][P_dir] * vel_sqr;
    
    term2 = d[2][c][P_dir]/(gamma - 1.0) 
          + rho_tmp[c] * ( vel[c][P_dir] * d[3][c][P_dir] + 
                           vel[c][dir1]  * d[4][c][P_dir] +
                           vel[c][dir2]  * d[5][c][P_dir] );
                                 
    term3 = conv_dir1 + conv_dir2;
                                                      
    double e_tmp = e[c] - delT * (term1 + term2 + term3);               

    temp_CC[c] = e_tmp/rho_CC[c]/cv - 0.5 * vel_sqr/cv;
  }
  
  //__________________________________
  //    E D G E S  -- on boundaryFaces only
  vector<Patch::FaceType> b_faces;
  getBoundaryEdges(patch,face,b_faces);
  
  vector<Patch::FaceType>::const_iterator iter;  
  for(iter = b_faces.begin(); iter != b_faces.end(); ++ iter ) {
    Patch::FaceType face0 = *iter;
    
    //__________________________________
    //  Find the offset for the r and l cells
    //  and the Vector components Edir1 and Edir2
    //  for this particular edge
    
    IntVector edge = Abs(patch->faceDirection(face)) 
                   + Abs(patch->faceDirection(face0));
    IntVector offset = IntVector(1,1,1) - edge;
           
    IntVector axes = patch->faceAxes(face0);
    int Edir1 = axes[0];
    int Edir2 = otherDirection(P_dir, Edir1);
     
    CellIterator iterLimits =  
                  patch->getEdgeCellIterator(face, face0, "minusCornerCells");
                  
    for(CellIterator iter = iterLimits;!iter.done();iter++){ 
      IntVector c = *iter;  
      IntVector r = c + offset;  
      IntVector l = c - offset;

      qConFrt  = vel[r][Edir2] * (e[r] + p[r]);
      qConLast = vel[l][Edir2] * (e[l] + p[l]);
    
      double conv = computeConvection(nu[r][Edir2], nu[c][Edir2], nu[l][Edir2],
                                      e[r], e[c], e[l],               
                                      qConFrt, qConLast, delT, dx[Edir2]); 
                                      
      double vel_sqr = vel[c].length2();

      term1 = 0.5 * (d[1][c][P_dir] + d[1][c][Edir1]) * vel_sqr;

      term2 =  (d[2][c][P_dir] + d[2][c][Edir1])/(gamma - 1.0);
      
      if( edge == IntVector(1,1,0) ) { // Left/Right faces top/bottom edges
        term3 =  rho_tmp[c] * vel[c][P_dir] * (d[3][c][P_dir] + d[4][c][Edir1])
              +  rho_tmp[c] * vel[c][Edir1] * (d[4][c][P_dir] + d[3][c][Edir1]) 
              +  rho_tmp[c] * vel[c][Edir2] * (d[5][c][P_dir] + d[5][c][Edir1]);
      }
      
      if( edge == IntVector(1,0,1) ) { // Left/Right faces  Front/Back edges
        term3 =  rho_tmp[c] * vel[c][P_dir] * (d[3][c][P_dir] + d[5][c][Edir1])
              +  rho_tmp[c] * vel[c][Edir2] * (d[4][c][P_dir] + d[4][c][Edir1])
              +  rho_tmp[c] * vel[c][Edir1] * (d[5][c][P_dir] + d[3][c][Edir1]);
      }
      
      if( edge == IntVector(0,1,1) ) { // Top/Bottom faces  Front/Back edges
        term3 =  rho_tmp[c] * vel[c][Edir2] * (d[4][c][P_dir] + d[5][c][Edir1])
              +  rho_tmp[c] * vel[c][P_dir] * (d[3][c][P_dir] + d[4][c][Edir1])
              +  rho_tmp[c] * vel[c][Edir1] * (d[5][c][P_dir] + d[3][c][Edir1]);
      }      
      
      double e_tmp = e[c] - delT * ( term1 + term2 + term3 + conv);

      temp_CC[c] = e_tmp/(rho_CC[c] *cv) - 0.5 * vel_sqr/cv;
    }
  }  
 
  //________________________________________________________
  // C O R N E R S    
  vector<IntVector> crn;
  computeCornerCellIndices(patch, face, crn);

  vector<IntVector>::iterator itr;
  for(itr = crn.begin(); itr != crn.end(); ++ itr ) {
    IntVector c = *itr;
    double vel_sqr = vel[c].length2();

    term1 = 0.5 * (d[1][c].x() + d[1][c].y() + d[1][c].z()) * vel_sqr;
    term2 =       (d[2][c].x() + d[2][c].y() + d[2][c].z())/(gamma - 1.0);
    
    term3 = rho_tmp[c] * vel[c].x() * (d[3][c].x() + d[4][c].y() + d[5][c].z())  
          + rho_tmp[c] * vel[c].y() * (d[4][c].x() + d[3][c].y() + d[4][c].z())  
          + rho_tmp[c] * vel[c].z() * (d[5][c].x() + d[5][c].y() + d[3][c].z()); 

    double e_tmp = e[c] - delT * ( term1 + term2 + term3);

    temp_CC[c] = e_tmp/rho_CC[c]/cv - 0.5 * vel_sqr/cv;
  }
} //end of function FaceTempLODI()  


/* --------------------------------------------------------------------- 
 Function~  fillFacePressLODI--
 Purpose~   Back out the pressure from f_theta and P_EOS
---------------------------------------------------------------------  */
void fillFacePress_LODI(const Patch* patch,
                        CCVariable<double>& press_CC,
                        const StaticArray<CCVariable<double> >& rho_micro,
                        const StaticArray<constCCVariable<double> >& Temp_CC,
                        const StaticArray<CCVariable<double> >& f_theta,
                        const int numALLMatls,
                        SimulationStateP& sharedState, 
                        Patch::FaceType face)
{
    cout_doing << " I am in fillFacePress_LODI on face " <<face<< endl;         
    IntVector low,hi;
    low = press_CC.getLowIndex();
    hi = press_CC.getHighIndex();
    StaticArray<double> press_eos(numALLMatls);
    StaticArray<double> cv(numALLMatls);
    StaticArray<double> gamma(numALLMatls);
    double press_ref= sharedState->getRefPress();    
  
    for (int m = 0; m < numALLMatls; m++) {
      Material* matl = sharedState->getMaterial( m );
      ICEMaterial* ice_matl = dynamic_cast<ICEMaterial*>(matl);
      if(ice_matl){       
        cv[m]     = ice_matl->getSpecificHeat();
        gamma[m]  = ice_matl->getGamma();;        
      }
    } 
    
   for(CellIterator iter=patch->getFaceCellIterator(face, "plusEdgeCells"); 
    !iter.done();iter++) {
      IntVector c = *iter;
          
      press_CC[c] = 0.0;
      for (int m = 0; m < numALLMatls; m++) {
        Material* matl = sharedState->getMaterial( m );
        ICEMaterial* ice_matl = dynamic_cast<ICEMaterial*>(matl);
        MPMMaterial* mpm_matl = dynamic_cast<MPMMaterial*>(matl);
        double tmp;
        if(ice_matl){                // I C E
          ice_matl->getEOS()->computePressEOS(rho_micro[m][c],gamma[m],
                                         cv[m],Temp_CC[m][c],
                                         press_eos[m],tmp,tmp);        
        } 
        if(mpm_matl){                //  M P M
          mpm_matl->getConstitutiveModel()->
            computePressEOSCM(rho_micro[m][c],press_eos[m], press_ref,
                              tmp, tmp,mpm_matl);
        }              
        press_CC[c] += f_theta[m][c]*press_eos[m];
 //     cout << "press_CC" << c << press_CC[c] << endl;           
      }  // for ALLMatls...
    }
  } 
  
#endif    // LODI_BCS
}  // using namespace Uintah
