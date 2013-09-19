/*
 * The MIT License
 *
 * Copyright (c) 1997-2013 The University of Utah
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

#include <CCA/Components/Models/Radiation/RMCRT/RayGPU.cuh>
#include <Core/Grid/Variables/GPUGridVariable.h>
#include <Core/Grid/Variables/Stencil7.h>
#include <CCA/Components/Schedulers/GPUDataWarehouse.h>
// linker support for device code not ready yet, need to include the whole source...
#include <CCA/Components/Schedulers/GPUDataWarehouse.cu>
#include <Core/Grid/Variables/Stencil7.h>
#include <sci_defs/cuda_defs.h>
#include <curand.h>
#include <curand_kernel.h>

namespace Uintah {

//---------------------------------------------------------------------------
// Kernel: The GPU ray tracer kernel
//---------------------------------------------------------------------------
__global__ void rayTraceKernel(dim3 dimGrid,
                               dim3 dimBlock,
                               int matl,
                               patchParams patch,
                               const uint3 domainLo,
                               const uint3 domainHi,
                               curandState* globalDevRandStates,
                               bool virtRad,
                               bool isSeedRandom,
                               bool ccRays,
                               int numRays,
                               double viewAngle,
                               double threshold,
                               bool modifies_divQ,
                               varLabelNames labelNames,
                               GPUDataWarehouse* abskg_gdw,
                               GPUDataWarehouse* sigmaT4_gdw,
                               GPUDataWarehouse* celltype_gdw,
                               GPUDataWarehouse* old_gdw,
                               GPUDataWarehouse* new_gdw)
{
printf( " AAA \n" );
  GPUGridVariable<double> divQ;
  GPUGridVariable<double> VRFlux;
  GPUGridVariable<Stencil7> boundFlux;
  GPUGridVariable<double> radiationVolQ;
  GPUGridVariable<int> celltype;


  if( modifies_divQ ){
    new_gdw->get( divQ,         labelNames.divQ,          patch.ID, matl );
    new_gdw->get( VRFlux,       labelNames.VRFlux,        patch.ID, matl );
    new_gdw->get( boundFlux,    labelNames.boundFlux,     patch.ID, matl );
    new_gdw->get( radiationVolQ,labelNames.radVolQ,       patch.ID, matl );
  }else{
    new_gdw->put( divQ,         labelNames.divQ,          patch.ID, matl );
    new_gdw->put( VRFlux,       labelNames.VRFlux,        patch.ID, matl );
    new_gdw->put( boundFlux,    labelNames.boundFlux,     patch.ID, matl );
    new_gdw->put( radiationVolQ,labelNames.radVolQ,       patch.ID, matl );
    
 #if 0
    new_gdw->allocateAndPut( divQ,      d_divQLabel,      d_matl, patch );
    divQ.initialize( 0.0 ); 
    new_gdw->allocateAndPut( VRFlux,    d_VRFluxLabel,    d_matl, patch );
    VRFlux.initialize( 0.0 );
    new_gdw->allocateAndPut( boundFlux,    d_boundFluxLabel, d_matl, patch );
    new_gdw->allocateAndPut( radiationVolq, d_radiationVolqLabel, d_matl, patch );
    radiationVolq.initialize( 0.0 );

    for (CellIterator iter = patch->getExtraCellIterator(); !iter.done(); iter++){
      IntVector origin = *iter;

      boundFlux[origin].p = 0.0;
      boundFlux[origin].w = 0.0;
      boundFlux[origin].e = 0.0;
      boundFlux[origin].s = 0.0;
      boundFlux[origin].n = 0.0;
      boundFlux[origin].b = 0.0;
      boundFlux[origin].t = 0.0;
    }
    #endif
 }

#if 0
  // calculate the thread indices
  int tidX = threadIdx.x + blockIdx.x * blockDim.x;
  int tidY = threadIdx.y + blockIdx.y * blockDim.y;

  // Get the extents of the data block in which the variables reside.
  // This is essentially the stride in the index calculations.
  int dx = patchSize.x;
  int dy = patchSize.y;

  // GPU equivalent of GridIterator loop - calculate sets of rays per thread
  if (tidX >= patchLo.x && tidY >= patchLo.y && tidX <= patchHi.x && tidY <= patchHi.y) { // patch boundary check
    #pragma unroll
    for (int z = patchLo.z; z <= patchHi.z; z++) { // loop through z slices

      // calculate the index for individual threads
      int idx = INDEX3D(dx,dy,tidX,tidY,z);

      uint3 origin = make_uint3(tidX, tidY, z);  // for each thread
      double sumI = 0;

      //_______________________________________________________________________
      // ray loop
      #pragma unroll

      for (int iRay = 0; iRay < numRays; iRay++) {

        // initialize device RNG states
        if (isSeedRandom == false) {
          curand_init(hashDevice(tidX), tidX, 0, &globalDevStates[tidX]);
        }

        // for explanation see: http://www.cgafaq.info/wiki/Random_Points_On_Sphere
        double plusMinus_one = 2 * randDblExcDevice(globalDevStates) - 1;
        double r = sqrt(1 - plusMinus_one * plusMinus_one);    // Radius of circle at z
        double theta = 2 * M_PI * randDblExcDevice(globalDevStates);      // Uniform between 0-2Pi

        // Convert to cartesian coordinates
        double3 direction_vector;
        direction_vector.x = r * cos(theta);
        direction_vector.y = r * sin(theta);
        direction_vector.z = plusMinus_one;

        double3 inv_direction_vector;
        inv_direction_vector.x = 1.0 / direction_vector.x;
        inv_direction_vector.y = 1.0 / direction_vector.y;
        inv_direction_vector.z = 1.0 / direction_vector.z;

        double DyDxRatio = cellSpacing.y / cellSpacing.x;  //noncubic
        double DzDxRatio = cellSpacing.z / cellSpacing.x;  //noncubic

        double3 ray_location;

        if (ccRays) {
          ray_location.x = origin.x + 0.5;
          ray_location.y = origin.y + 0.5 * DyDxRatio;  //noncubic
          ray_location.z = origin.z + 0.5 * DzDxRatio;  //noncubic
        } else {
          ray_location.x = origin.x + randDevice(globalDevStates);
          ray_location.y = origin.y + randDevice(globalDevStates) * DyDxRatio;  //noncubic
          ray_location.z = origin.z + randDevice(globalDevStates) * DzDxRatio;  //noncubic
        }

        updateSumIDevice(domainLo, domainHi, patchSize, origin, cellSpacing, inv_direction_vector,
                         ray_location, dev_sigmaT4, dev_abskg, &threshold, &sumI);

      }  // end ray loop

      //__________________________________
      //  Compute divQ
      dev_divQ[idx] = 4.0 * M_PI * dev_abskg[idx] * (dev_sigmaT4[idx] - (sumI / numRays));

      dev_divQ[idx]       = 0.0;
      dev_VRFlux[idx]     = 0.0;
      dev_boundFlux[idx]  = 0.0;
      dev_radVolq[idx]    = 0.0;

    } // end z-slice loop
  }  // end domain boundary check
#endif
}  // end ray trace kernel


//---------------------------------------------------------------------------
// Device Function:
//---------------------------------------------------------------------------
__device__ void updateSumIDevice(const uint3& domainLo,
                                 const uint3& domainHi,
                                 const uint3& patchSize,
                                 const uint3& origin,
                                 const double3& cellSpacing,
                                 const double3& inv_direction_vector,
                                 const double3& ray_location,
                                 double* dev_sigmaT4,
                                 double* dev_abskg,
                                 double* threshold,
                                 double* sumI)
{
#if 0
  // Get the size of the data block in which the variables reside.
  // This is essentially the stride in the index calculations.
  int dx = patchSize.x;
  int dy = patchSize.y;;

  uint3 cur = origin;
  uint3 prevCell = cur;

  // Step and sign for ray marching, gives +1 or -1 based on sign
  int step[3];
  bool sign[3];

  // unrolled
  if (inv_direction_vector.x > 0) {
    step[0] = 1;
    sign[0] = 1;
  } else {
    step[0] = -1;
    sign[0] = 0;
  }

  if (inv_direction_vector.y > 0) {
    step[1] = 1;
    sign[1] = 1;
  } else {
    step[1] = -1;
    sign[1] = 0;
  }

  if (inv_direction_vector.z > 0) {
    step[2] = 1;
    sign[2] = 1;
  } else {
    step[2] = -1;
    sign[2] = 0;
  }

  double DyDxRatio = cellSpacing.y / cellSpacing.x;  //noncubic
  double DzDxRatio = cellSpacing.z / cellSpacing.x;  //noncubic

  double tMaxX = (origin.x + sign[0] - ray_location.x) * inv_direction_vector.x;
  double tMaxY = (origin.y + sign[1] * DyDxRatio - ray_location.y) * inv_direction_vector.y;
  double tMaxZ = (origin.z + sign[2] * DzDxRatio - ray_location.z) * inv_direction_vector.z;

  //Length of t to traverse one cell
  double tDeltaX = abs(inv_direction_vector.x);
  double tDeltaY = abs(inv_direction_vector.y) * DyDxRatio;
  double tDeltaZ = abs(inv_direction_vector.z) * DzDxRatio;
  double tMax_prev = 0;
  bool in_domain = true;

  //Initializes the following values for each ray
  double intensity = 1.0;
  double fs = 1.0;
  double optical_thickness = 0;

  // begin ray tracing
  int nReflect = 0;  // Number of reflections that a ray has undergone
  while (intensity > *threshold) { // threshold while loop
    int face = -9;
    while (in_domain) {
      prevCell = cur;
      double disMin = -9;  // Common variable name in ray tracing. Represents ray segment length.

      //__________________________________
      //  Determine which cell the ray will enter next
      if (tMaxX < tMaxY) {
        if (tMaxX < tMaxZ) {
          cur.x = cur.x + step[0];
          disMin = tMaxX - tMax_prev;
          tMax_prev = tMaxX;
          tMaxX = tMaxX + tDeltaX;
          face = 0;
        } else {
          cur.z = cur.z + step[2];
          disMin = tMaxZ - tMax_prev;
          tMax_prev = tMaxZ;
          tMaxZ = tMaxZ + tDeltaZ;
          face = 2;
        }
      } else {
        if (tMaxY < tMaxZ) {
          cur.y = cur.y + step[1];
          disMin = tMaxY - tMax_prev;
          tMax_prev = tMaxY;
          tMaxY = tMaxY + tDeltaY;
          face = 1;
        } else {
          cur.z = cur.z + step[2];
          disMin = tMaxZ - tMax_prev;
          tMax_prev = tMaxZ;
          tMaxZ = tMaxZ + tDeltaZ;
          face = 2;
        }
      }

      in_domain = containsCellDevice(domainLo, domainHi, cur, face);

      //__________________________________
      //  Update the ray location
      double optical_thickness_prev = optical_thickness;
      int prev_index = INDEX3D(dx,dy,prevCell.x,prevCell.y,prevCell.z) + (dx*dy);
      optical_thickness += cellSpacing.x * dev_abskg[prev_index] * disMin;
      // device_sigmaT4[idx] always 0.3183314161909468?
      *sumI += dev_sigmaT4[prev_index] * ( exp(-optical_thickness_prev) - exp(-optical_thickness) ) * fs;

    } // end domain while loop

    intensity = exp(-optical_thickness);
    int cur_index = INDEX3D(dx,dy,cur.x,cur.y,cur.z) + (dx*dy);
    *sumI += dev_abskg[cur_index] * dev_sigmaT4[cur_index] * intensity;
    intensity = intensity * (1 - dev_abskg[cur_index]);

    //__________________________________
    //  Reflections
    if (intensity > *threshold) {

      ++nReflect;
      fs = fs * (1 - dev_abskg[cur_index]);

      // put cur back inside the domain
      cur = prevCell;

      // apply reflection condition
      step[face] *= -1;                        // begin stepping in opposite direction
      sign[face] = (sign[face] == 1) ? 0 : 1;  // swap sign from 1 to 0 or vice versa

      in_domain = true;

    }  // end if reflection
  }  // end threshold while loop.
#endif
}  // end of updateSumI function


//---------------------------------------------------------------------------
// Device Function:
//---------------------------------------------------------------------------
__device__ bool containsCellDevice(const uint3& domainLo,
                                   const uint3& domainHi,
                                   const uint3& cell,
                                   const int& face)
{
  switch (face) {
    case 0 :
      return domainLo.x <= cell.x && domainHi.x > cell.x;
    case 1 :
      return domainLo.y <= cell.y && domainHi.y > cell.y;
    case 2 :
      return domainLo.z <= cell.z && domainHi.z > cell.z;
    default :
      return false;
  }
}


//---------------------------------------------------------------------------
// Device Function:
//---------------------------------------------------------------------------
__device__ double randDevice(curandState* globalState)
{
    int tid = threadIdx.x + blockIdx.x * blockDim.x;
    curandState localState = globalState[tid];
    double val = curand(&localState);
    globalState[tid] = localState;

    return (double)val * (1.0/4294967295.0);
}


//---------------------------------------------------------------------------
// Device Function:
//---------------------------------------------------------------------------
__device__ double randDblExcDevice(curandState* globalState)
{
    int tid = threadIdx.x + blockIdx.x * blockDim.x;
    curandState localState = globalState[tid];
    double val = curand_uniform(&localState);
    globalState[tid] = localState;

    return ( (double)val + 0.5 ) * (1.0/4294967296.0);
}


//---------------------------------------------------------------------------
// Device Function:
//---------------------------------------------------------------------------
__device__ unsigned int hashDevice(unsigned int a)
{
    a = (a+0x7ed55d16) + (a<<12);
    a = (a^0xc761c23c) ^ (a>>19);
    a = (a+0x165667b1) + (a<<5);
    a = (a+0xd3a2646c) ^ (a<<9);
    a = (a+0xfd7046c5) + (a<<3);
    a = (a^0xb55a4f09) ^ (a>>16);

    return a;
}
//______________________________________________________________________

__host__ void launchRayTraceKernel(dim3 dimGrid,
                                   dim3 dimBlock,
                                   int matlIndex,
                                   patchParams patch,
                                   const uint3 domainLo,
                                   const uint3 domainHi,
                                   curandState* globalDevRandStates,
                                   cudaStream_t* stream,
                                   bool virtRad,
                                   bool isSeedRandom,
                                   bool ccRays,
                                   int numDivQRays,
                                   double viewAngle,
                                   double threshold, 
                                   bool modifies_divQ,
                                   varLabelNames labelNames,                           
                                   GPUDataWarehouse* abskg_gdw,
                                   GPUDataWarehouse* sigmaT4_gdw,
                                   GPUDataWarehouse* celltype_gdw,
                                   GPUDataWarehouse* old_gdw,
                                   GPUDataWarehouse* new_gdw)
{
  cout << " inside " << endl;
  rayTraceKernel<<< dimGrid, dimBlock, 0, *stream >>>(dimGrid, 
                                                      dimBlock, 
                                                      matlIndex,
                                                      patch,
                                                      domainLo, 
                                                      domainHi,
                                                      globalDevRandStates,
                                                      virtRad,
                                                      isSeedRandom,
                                                      ccRays,
                                                      numDivQRays,
                                                      viewAngle,
                                                      threshold,
                                                      modifies_divQ,
                                                      labelNames,
                                                      abskg_gdw,
                                                      sigmaT4_gdw,
                                                      celltype_gdw,
                                                      old_gdw,
                                                      new_gdw);
  cout << " BBB inside" << endl;
}

} //end namespace Uintah
