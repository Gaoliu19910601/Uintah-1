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

#include <CCA/Components/MD/SPME.h>
#include <CCA/Components/MD/CenteredCardinalBSpline.h>
#include <CCA/Components/MD/SPMEMapPoint.h>
#include <CCA/Components/MD/MDSystem.h>
#include <CCA/Components/MD/MDLabel.h>
#include <CCA/Components/MD/SimpleGrid.h>
#include <Core/Grid/Patch.h>
#include <Core/Grid/Variables/ParticleVariable.h>
#include <Core/Grid/Variables/ParticleSubset.h>
#include <Core/Grid/Variables/CCVariable.h>
#include <Core/Grid/Variables/CellIterator.h>
#include <Core/Geometry/IntVector.h>
#include <Core/Geometry/Point.h>
#include <Core/Math/MiscMath.h>
#include <Core/Util/DebugStream.h>

#include <iostream>
#include <iomanip>
#include <complex>

#include <sci_values.h>
#include <sci_defs/fftw_defs.h>

using namespace Uintah;

extern SCIRun::Mutex cerrLock;

static DebugStream md_spme("MDSPME", false);

SPME::SPME()
{

}

SPME::~SPME()
{
  std::vector<SPMEPatch*>::iterator iter;
  for (iter = d_spmePatches.begin(); iter != d_spmePatches.end(); iter++) {
    delete *iter;
  }
}

SPME::SPME(MDSystem* system,
           const double ewaldBeta,
           const bool isPolarizable,
           const double tolerance,
           const IntVector& kLimits,
           const int splineOrder) :
    d_system(system), d_ewaldBeta(ewaldBeta), d_polarizable(isPolarizable), d_polarizationTolerance(tolerance), d_kLimits(kLimits)
{
  d_interpolatingSpline = CenteredCardinalBSpline(splineOrder);
  d_electrostaticMethod = Electrostatics::SPME;
}

//-----------------------------------------------------------------------------
// Interface implementations
void SPME::initialize()
{
  // We call SPME::initialize from MD::initialize, or if we've somehow maintained our object across a system change

  // Get useful information from global system descriptor to work with locally.
  d_unitCell = d_system->getUnitCell();
  d_inverseUnitCell = d_system->getInverseCell();
  d_systemVolume = d_system->getCellVolume();
}

// Note:  Must run SPME->setup() each time there is a new box/K grid mapping (e.g. every step for NPT)
//          This should be checked for in the system electrostatic driver
void SPME::setup(const ProcessorGroup* pg,
                 const PatchSubset* patches,
                 const MaterialSubset* materials,
                 DataWarehouse* old_dw,
                 DataWarehouse* new_dw)
{
  int numGhostCells = d_system->getNumGhostCells();
  size_t numPatches = patches->size();
  d_spmePatches.reserve(numPatches);

  for (size_t p = 0; p < numPatches; p++) {
    const Patch* patch = patches->get(p);

//!FIXME -- These aren't extents/offsets/ghost extents in K grid, which they need to be.
    /*    IntVector localExtents = patch->getCellHighIndex() - patch->getCellLowIndex();
     IntVector globalOffset = localExtents - IntVector(0, 0, 0);
     IntVector plusGhostExtents = patch->getExtraCellHighIndex(numGhostCells) - patch->getCellHighIndex();
     IntVector minusGhostExtents = patch->getCellLowIndex() - patch->getExtraCellLowIndex(numGhostCells);
     */

    Point PatchPositionLow = patch->cellPosition(patch->getCellLowIndex());
    Point PatchPositionHigh = patch->cellPosition(patch->getCellHighIndex());

    IntVector PatchKLow, PatchKHigh;
    for (size_t idx = 0; idx < 3; ++idx) {  // Tedious stuff because operators aren't defined
      PatchKLow[idx] = ceil(d_kLimits(idx) * PatchPositionLow(idx) / d_unitCell(idx, idx));
      PatchKHigh[idx] = floor(d_kLimits(idx) * PatchPositionHigh(idx) / d_unitCell(idx, idx));
    }
    IntVector PatchKGridExtents = (PatchKHigh - PatchKLow) + IntVector(1, 1, 1);  // +1 for inclusive limits
    IntVector PatchKGridOffset = PatchKLow;

    int SplineHalfMaxSupport = d_interpolatingSpline.getHalfMaxSupport();
    IntVector plusGhostExtents = IntVector(SplineHalfMaxSupport, SplineHalfMaxSupport, SplineHalfMaxSupport);
    IntVector minusGhostExtents = plusGhostExtents;  // ensure symmetry

    // 
    SPMEPatch* spmePatch = new SPMEPatch(PatchKGridExtents, PatchKGridOffset, plusGhostExtents, minusGhostExtents, patch);

    // Calculate B and C - we should only have to do this if KLimits or the inverse cell changes
    SimpleGrid<double> fBGrid = calculateBGrid(PatchKGridExtents, PatchKGridOffset);
    SimpleGrid<double> fCGrid = calculateCGrid(PatchKGridExtents, PatchKGridOffset);
    SimpleGrid<double> fTheta(PatchKGridExtents, PatchKGridOffset, 0);  // No ghost cells; internal only
    fTheta.initialize(0.0);

    // Composite B and C into Theta
    size_t xExtent = PatchKGridExtents.x();
    size_t yExtent = PatchKGridExtents.y();
    size_t zExtent = PatchKGridExtents.z();
    for (size_t xidx = 0; xidx < xExtent; ++xidx) {
      for (size_t yidx = 0; yidx < yExtent; ++yidx) {
        for (size_t zidx = 0; zidx < zExtent; ++zidx) {
          fTheta(xidx, yidx, zidx) = fBGrid(xidx, yidx, zidx) * fCGrid(xidx, yidx, zidx);

          if (md_spme.active()) {
            cerrLock.unlock();
            // FIXME - some "nan" values interspersed in B and C
            if (isnan(fBGrid(xidx, yidx, zidx))) {
              std::cout << "B: " << xidx << " " << yidx << " " << zidx << std::endl;
              std::cin.get();
            }
            if (isnan(fCGrid(xidx, yidx, zidx))) {
              std::cout << "C: " << xidx << " " << yidx << " " << zidx << std::endl;
              std::cin.get();
            }
            cerrLock.unlock();
          }

        }
      }
    }
    spmePatch->setTheta(fTheta);
    spmePatch->setStressPrefactor(calculateStressPrefactor(PatchKGridExtents, PatchKGridOffset));
    SimpleGrid<dblcomplex> q(PatchKGridExtents, PatchKGridOffset, 2 * SplineHalfMaxSupport);  // Check to make sure plusGhostExtents+minusGhostExtents is right way to enter number of ghost cells (i.e. total, not per offset)
    spmePatch->setQ(q);
    d_spmePatches.push_back(spmePatch);
  }
}

void SPME::calculate(const ProcessorGroup* pg,
                     const PatchSubset* patches,
                     const MaterialSubset* materials,
                     DataWarehouse* old_dw,
                     DataWarehouse* new_dw)
{
  bool converged = false;
  int numIterations = 0;
  int maxIterations = d_system->getMaxIterations();
  while (!converged && (numIterations < maxIterations)) {

    std::vector<SPMEPatch*>::iterator iter;
    for (iter = d_spmePatches.begin(); iter != d_spmePatches.end(); iter++) {
      SPMEPatch* spmePatch = *iter;

      const Patch* patch = spmePatch->getPatch();
      ParticleSubset* pset = old_dw->getParticleSubset(materials->get(0), patch);

      constParticleVariable<Point> px;
      constParticleVariable<long64> pids;
      constParticleVariable<double> pcharge;
      old_dw->get(px, d_lb->pXLabel, pset);
      old_dw->get(pids, d_lb->pParticleIDLabel, pset);
      old_dw->get(pcharge, d_lb->pChargeLabel, pset);

      std::vector<SPMEMapPoint> gridMap = generateChargeMap(pset, px, pids, d_interpolatingSpline);
      mapChargeToGrid(spmePatch, gridMap, pset, pcharge, d_interpolatingSpline.getHalfMaxSupport());  // Calculate Q(r)

      SimpleGrid<complex<double> > Q = spmePatch->getQ();
      Q.initialize(complex<double>(0.0, 0.0));
      SimpleGrid<Matrix3> stressPrefactor = spmePatch->getStressPrefactor();

      // Map the local patch's charge grid into the global grid and transform
//      SPME::GlobalMPIReduceChargeGrid(Ghost::AroundNodes);  //Ghost points should get transferred here
//      SPME::ForwardTransformGlobalChargeGrid();  // Q(r) -> Q*(k)

      // Once reduced and transformed, we need the local grid re-populated with Q*(k)
//      SPME::MPIDistributeLocalChargeGrid(Ghost::None);

      // Multiply the transformed Q out
      IntVector localExtents = spmePatch->getLocalExtents();
      size_t xExtent = localExtents.x();
      size_t yExtent = localExtents.y();
      size_t zExtent = localExtents.z();
      double localEnergy = 0.0;  //Maybe should be global?
      Matrix3 localStress(0.0);  //Maybe should be global?
      for (size_t kX = 0; kX < xExtent; ++kX) {
        for (size_t kY = 0; kY < yExtent; ++kY) {
          for (size_t kZ = 0; kZ < zExtent; ++kZ) {
            SimpleGrid<double> fTheta = spmePatch->getTheta();
            complex<double> gridValue = Q(kX, kY, kZ);

            // Calculate (Q*Q^)*(B*C)
            Q(kX, kY, kZ) = gridValue * conj(gridValue) * fTheta(kX, kY, kZ);
            localEnergy += std::abs(Q(kX, kY, kZ));
            localStress += std::abs(Q(kX, kY, kZ)) * stressPrefactor(kX, kY, kZ);
          }
        }
      }

      // Transform back to real space
//      SPME::GlobalMPIReduceChargeGrid(Ghost::None);  //Ghost points should NOT get transferred here
//      SPME::ReverseTransformGlobalChargeGrid();
//      SPME::MPIDistributeLocalChargeGrid(Ghost::AroundNodes);

      //  This may need to be before we transform the charge grid back to real space if we can calculate
      //    polarizability from the fourier space component
      converged = true;
      if (d_polarizable) {
        // calculate polarization here
        // if (RMSPolarizationDifference > PolarizationTolerance) { ElectrostaticsConverged = false; }
        std::cerr << "Error:  Polarization not currently implemented!";
      }
      // Sanity check - Limit maximum number of polarization iterations we try
      ++numIterations;
    }
//    SPME::GlobalReduceEnergy();
//    SPME::GlobalReduceStress();  //Uintah framework?
  }
}

void SPME::finalize(const ProcessorGroup* pg,
                    const PatchSubset* patches,
                    const MaterialSubset* materials,
                    DataWarehouse* old_dw,
                    DataWarehouse* new_dw)
{
  std::vector<SPMEPatch*>::iterator iter;
  for (iter = d_spmePatches.begin(); iter != d_spmePatches.end(); iter++) {
    SPMEPatch* spmePatch = *iter;

    const Patch* patch = spmePatch->getPatch();
    ParticleSubset* pset = old_dw->getParticleSubset(materials->get(0), patch);

    constParticleVariable<Vector> pforce;
    old_dw->get(pforce, d_lb->pForceLabel, pset);

    ParticleVariable<Vector> pforcenew;
    new_dw->getModifiable(pforcenew, d_lb->pForceLabel_preReloc, pset);

    //FIXME need gridMap from each SPMEPatch

    // Calculate electrostatic contribution to f_ij(r)
//    SPME::mapForceFromGrid(pset, ChargeMap);
  }

  // Reduction for Energy, Pressure Tensor?
  // Something goes here, though I'm not sure what... output?
}

std::vector<SCIRun::dblcomplex> SPME::generateBVector(const std::vector<double>& mFractional,
                                                      const int initialIndex,
                                                      const int localGridExtent,
                                                      const CenteredCardinalBSpline& interpolatingSpline) const
{
  double PI = acos(-1.0);
  double twoPI = 2.0 * PI;
  double orderM12PI = twoPI * (interpolatingSpline.getOrder() - 1);

  int halfSupport = interpolatingSpline.getHalfMaxSupport();
  std::vector<dblcomplex> B(localGridExtent);
  std::vector<double> zeroAlignedSpline = interpolatingSpline.evaluate(0);

  const double* localMFractional = &(mFractional[initialIndex]);  // Reset MFractional zero so we can index into it negatively
  for (int idx = 0; idx < localGridExtent; ++idx) {
    double internal = twoPI * localMFractional[idx];
    // Formula looks significantly different from given SPME for offset splines.
    //   See Essmann et. al., J. Chem. Phys. 103 8577 (1995). for conversion, particularly formula C3 pt. 2 (paper uses pt. 4)
    dblcomplex phi_N = 0.0;
    for (int denomIndex = -halfSupport; denomIndex <= halfSupport; ++denomIndex) {
      phi_N += dblcomplex(cos(internal * denomIndex), sin(internal * denomIndex));
    }
    B[idx] = 1.0 / phi_N;
  }
  return B;
}

SimpleGrid<double> SPME::calculateBGrid(const IntVector& localExtents,
                                        const IntVector& globalOffset) const
{
  size_t limit_Kx = d_kLimits.x();
  size_t limit_Ky = d_kLimits.y();
  size_t limit_Kz = d_kLimits.z();

  std::vector<double> mf1 = SPME::generateMFractionalVector(limit_Kx, d_interpolatingSpline);
  std::vector<double> mf2 = SPME::generateMFractionalVector(limit_Ky, d_interpolatingSpline);
  std::vector<double> mf3 = SPME::generateMFractionalVector(limit_Kz, d_interpolatingSpline);

  std::cout << " DEBUG: " << std::endl;
  std::cout << "Expect mf1 size: " << d_kLimits.x() << "  Actual mf1 size: " << mf1.size() << std::endl;
  for (size_t idx = 0; idx < mf1.size(); ++idx) {
    std::cout << "mf1(" << std::setw(3) << idx << "): " << mf1[idx] << std::endl;
  }
  std::cout << "Expect mf2 size: " << d_kLimits.y() << "  Actual mf2 size: " << mf2.size() << std::endl;
  for (size_t idx = 0; idx < mf2.size(); ++idx) {
    std::cout << "mf2(" << std::setw(3) << idx << "): " << mf2[idx] << std::endl;
  }
  std::cout << "Expect mf3 size: " << d_kLimits.x() << "  Actual mf3 size: " << mf3.size() << std::endl;
  for (size_t idx = 0; idx < mf3.size(); ++idx) {
    std::cout << "mf3(" << std::setw(3) << idx << "): " << mf3[idx] << std::endl;
  }
  std::cout << " END DEBUG: " << std::endl;

  // localExtents is without ghost grid points
  std::vector<dblcomplex> b1 = generateBVector(mf1, globalOffset.x(), localExtents.x(), d_interpolatingSpline);
  std::vector<dblcomplex> b2 = generateBVector(mf2, globalOffset.y(), localExtents.y(), d_interpolatingSpline);
  std::vector<dblcomplex> b3 = generateBVector(mf3, globalOffset.z(), localExtents.z(), d_interpolatingSpline);

  SimpleGrid<double> BGrid(localExtents, globalOffset, 0);  // No ghost cells; internal only

  size_t xExtents = localExtents.x();
  size_t yExtents = localExtents.y();
  size_t zExtents = localExtents.z();

  int xOffset = globalOffset.x();
  int yOffset = globalOffset.y();
  int zOffset = globalOffset.z();

  for (size_t kX = 0; kX < xExtents; ++kX) {
    for (size_t kY = 0; kY < yExtents; ++kY) {
      for (size_t kZ = 0; kZ < zExtents; ++kZ) {
        BGrid(kX, kY, kZ) = norm(b1[kX + xOffset]) * norm(b2[kY + yOffset]) * norm(b3[kZ + zOffset]);
      }
    }
  }
  return BGrid;
}

SimpleGrid<double> SPME::calculateCGrid(const IntVector& extents,
                                        const IntVector& offset) const
{
  std::vector<double> mp1 = SPME::generateMPrimeVector(d_kLimits.x(), d_interpolatingSpline);
  std::vector<double> mp2 = SPME::generateMPrimeVector(d_kLimits.y(), d_interpolatingSpline);
  std::vector<double> mp3 = SPME::generateMPrimeVector(d_kLimits.z(), d_interpolatingSpline);

  std::cout << " DEBUG: " << std::endl;
  std::cout << "Expect mp1 size: " << d_kLimits.x() << "  Actual mp1 size: " << mp1.size() << std::endl;
  for (size_t idx = 0; idx < mp1.size(); ++idx) {
    std::cout << "mp1(" << std::setw(3) << idx << "): " << mp1[idx] << std::endl;
  }
  std::cout << "Expect mp2 size: " << d_kLimits.y() << "  Actual mp2 size: " << mp2.size() << std::endl;
  for (size_t idx = 0; idx < mp2.size(); ++idx) {
    std::cout << "mp2(" << std::setw(3) << idx << "): " << mp2[idx] << std::endl;
  }
  std::cout << "Expect mp3 size: " << d_kLimits.x() << "  Actual mp3 size: " << mp3.size() << std::endl;
  for (size_t idx = 0; idx < mp3.size(); ++idx) {
    std::cout << "mp3(" << std::setw(3) << idx << "): " << mp3[idx] << std::endl;
  }
  std::cout << " END DEBUG: " << std::endl;

  size_t xExtents = extents.x();
  size_t yExtents = extents.y();
  size_t zExtents = extents.z();

  int xOffset = offset.x();
  int yOffset = offset.y();
  int zOffset = offset.z();

  double PI = acos(-1.0);
  double PI2 = PI * PI;
  double invBeta2 = 1.0 / (d_ewaldBeta * d_ewaldBeta);
  double invVolFactor = 1.0 / (d_systemVolume * PI);

  SimpleGrid<double> CGrid(extents, offset, 0);  // No ghost cells; internal only
  for (size_t kX = 0; kX < xExtents; ++kX) {
    for (size_t kY = 0; kY < yExtents; ++kY) {
      for (size_t kZ = 0; kZ < zExtents; ++kZ) {
        if (kX != 0 || kY != 0 || kZ != 0) {
          SCIRun::Vector m(mp1[kX + xOffset], mp2[kY + yOffset], mp3[kZ + zOffset]);

          m = m * d_inverseUnitCell;

          double M2 = m.length2();
          double factor = PI2 * M2 * invBeta2;
          // FIXME  M2 always zero.... mp1[kX + xOffset], etc
          // always accessing garbage memory outside of mp bounds
          CGrid(kX, kY, kZ) = invVolFactor * exp(-factor) / M2;
          // CGrid(kX, kY, kZ) = invVolFactor * exp(-factor) / 1.0;
        }
      }
    }
  }
  CGrid(0, 0, 0) = 0;
  return CGrid;
}

SimpleGrid<Matrix3>& SPME::calculateStressPrefactor(const IntVector& extents,
                                                    const IntVector& offset)
{
  std::vector<double> mp1 = SPME::generateMPrimeVector(d_kLimits.x(), d_interpolatingSpline);
  std::vector<double> mp2 = SPME::generateMPrimeVector(d_kLimits.y(), d_interpolatingSpline);
  std::vector<double> mp3 = SPME::generateMPrimeVector(d_kLimits.z(), d_interpolatingSpline);

  size_t xExtents = extents.x();
  size_t yExtents = extents.y();
  size_t zExtents = extents.z();

  int XOffset = offset.x();
  int YOffset = offset.y();
  int ZOffset = offset.z();

  double PI = acos(-1.0);
  double PI2 = PI * PI;
  double invBeta2 = 1.0 / (d_ewaldBeta * d_ewaldBeta);

  SimpleGrid<Matrix3> stressPre(extents, offset, 0);  // No ghost cells; internal only
  for (size_t kX = 0; kX < xExtents; ++kX) {
    for (size_t kY = 0; kY < yExtents; ++kY) {
      for (size_t kZ = 0; kZ < zExtents; ++kZ) {
        if (kX != 0 || kY != 0 || kZ != 0) {
          SCIRun::Vector m(mp1[kX + XOffset], mp2[kY + YOffset], mp3[kZ + ZOffset]);
          m = m * d_inverseUnitCell;
          double M2 = m.length2();
          Matrix3 localStressContribution(-2.0 * (1.0 + PI2 * M2 * invBeta2) / M2);

          // Multiply by fourier vectorial contribution
          for (size_t s1 = 0; s1 < 3; ++s1) {
            for (size_t s2 = 0; s2 < 3; ++s2) {
              localStressContribution(s1, s2) *= (m[s1] * m[s2]);
            }
          }

          // Account for delta function
          for (size_t delta = 0; delta < 3; ++delta) {
            localStressContribution(delta, delta) += 1.0;
          }

          stressPre(kX, kY, kZ) = localStressContribution;
        }
      }
    }
  }
  stressPre(0, 0, 0) = Matrix3(0.0);
  return stressPre;
}

std::vector<SPMEMapPoint> SPME::generateChargeMap(ParticleSubset* pset,
                                                  constParticleVariable<Point> particlePositions,
                                                  constParticleVariable<long64> particleIDs,
                                                  CenteredCardinalBSpline& spline)
{
  std::vector<SPMEMapPoint> chargeMap;

  // Loop through particles
  for (ParticleSubset::iterator iter = pset->begin(); iter != pset->end(); iter++) {
    particleIndex pidx = *iter;
    SCIRun::Point position = particlePositions[pidx];
    particleId pid = particleIDs[pidx];
    SCIRun::Vector particleGridCoordinates;

    //Calculate reduced coordinates of point to recast into charge grid
    particleGridCoordinates = (position.asVector()) * d_inverseUnitCell;
    // ** NOTE: JBH --> We may want to do this with a bit more thought eventually, since multiplying by the InverseUnitCell
    //                  is expensive if the system is orthorhombic, however it's not clear it's more expensive than dropping
    //                  to call MDSystem->IsOrthorhombic() and then branching the if statement appropriately.

    // This bit is tedious since we don't have any cross-pollination between type Vector and type IntVector.
    // Should we put that in (requires modifying Uintah framework).
    SCIRun::Vector kReal, splineValues;
    IntVector particleGridOffset;
    for (size_t idx = 0; idx < 3; ++idx) {
      kReal[idx] = (d_kLimits.asVector())[idx];  //static_cast<double>(kLimits[idx]);  // For some reason I can't construct a Vector from an IntVector -- Maybe we should fix that instead?
      particleGridCoordinates[idx] *= kReal[idx];         // Recast particle into charge grid based representation
      particleGridOffset[idx] = static_cast<int>(particleGridCoordinates[idx]);  // Reference grid point for particle
      splineValues[idx] = particleGridOffset[idx] - particleGridCoordinates[idx];  // spline offset for spline function
    }
    vector<double> xSplineArray = spline.evaluate(splineValues[0]);
    vector<double> ySplineArray = spline.evaluate(splineValues[1]);
    vector<double> zSplineArray = spline.evaluate(splineValues[2]);

    vector<double> xSplineDeriv = spline.derivative(splineValues[0]);
    vector<double> ySplineDeriv = spline.derivative(splineValues[1]);
    vector<double> zSplineDeriv = spline.derivative(splineValues[2]);

    IntVector extents(xSplineArray.size(), ySplineArray.size(), zSplineArray.size());
    SimpleGrid<double> chargeGrid(extents, particleGridOffset, 0);

    SimpleGrid<SCIRun::Vector> forceGrid(extents, particleGridOffset, 0);
    size_t XExtent = xSplineArray.size();
    size_t YExtent = ySplineArray.size();
    size_t ZExtent = zSplineArray.size();
    for (size_t xidx = 0; xidx < XExtent; ++xidx) {
      for (size_t yidx = 0; yidx < YExtent; ++yidx) {
        for (size_t zidx = 0; zidx < ZExtent; ++zidx) {
          chargeGrid(xidx, yidx, zidx) = xSplineArray[xidx] * ySplineArray[yidx] * zSplineArray[zidx];
          forceGrid(xidx, yidx, zidx) = SCIRun::Vector(xSplineDeriv[xidx], ySplineDeriv[yidx], zSplineDeriv[zidx]);
        }
      }
    }
    SPMEMapPoint currentMapPoint(pid, particleGridOffset, chargeGrid, forceGrid);
    chargeMap.push_back(currentMapPoint);
  }
  return chargeMap;
}

void SPME::mapChargeToGrid(SPMEPatch* spmePatch,
                           const std::vector<SPMEMapPoint>& gridMap,
                           ParticleSubset* pset,
                           constParticleVariable<double> charges,
                           int halfSupport)
{

  // Reset charges before we start adding onto them.
  SimpleGrid<dblcomplex> Q = spmePatch->getQ();
  Q.initialize(0.0);

  ParticleSubset::iterator particleIter;
  for (particleIter = pset->begin(); particleIter != pset->end(); particleIter++) {
    particleIndex pidx = *particleIter;

    double charge = charges[pidx];
    const SimpleGrid<double> chargeMap = gridMap[pidx].getChargeGrid();

    IntVector QAnchor = chargeMap.getOffset();  // Location of the 0,0,0 origin for the charge map grid
    IntVector SupportExtent = chargeMap.getExtents();  // Extents of the charge map grid
    for (int xmask = -halfSupport; xmask <= halfSupport; ++xmask) {
      for (int ymask = -halfSupport; ymask <= halfSupport; ++ymask) {
        for (int zmask = -halfSupport; zmask <= halfSupport; ++zmask) {
          dblcomplex val = charge * chargeMap(xmask + halfSupport, ymask + halfSupport, zmask + halfSupport);
          Q(QAnchor.x() + xmask, QAnchor.y() + ymask, QAnchor.z() + zmask) += val;
        }
      }
    }
  }
  spmePatch->setQ(Q);
}

void SPME::mapForceFromGrid(SPMEPatch* spmePatch,
                            const std::vector<SPMEMapPoint>& gridMap,
                            ParticleSubset* pset,
                            constParticleVariable<Vector> pforce,
                            ParticleVariable<Vector> pforcenew,
                            int halfSupport)
{
  SimpleGrid<complex<double> > Q = spmePatch->getQ();

  for (ParticleSubset::iterator iter = pset->begin(); iter != pset->end(); iter++) {
    particleIndex pidx = *iter;

    SimpleGrid<SCIRun::Vector> forceMap = gridMap[pidx].getForceGrid();
    SCIRun::Vector newForce = pforce[pidx];
    IntVector QAnchor = forceMap.getOffset();  // Location of the 0,0,0 origin for the force map grid
    IntVector supportExtent = forceMap.getExtents();  // Extents of the force map grid

    for (int xmask = -halfSupport; xmask <= halfSupport; ++xmask) {
      for (int ymask = -halfSupport; ymask <= halfSupport; ++ymask) {
        for (int zmask = -halfSupport; zmask <= halfSupport; ++zmask) {
          SCIRun::Vector currentForce;
          //FIXME This operation doesn't make sense
//          currentForce = forceMap(xmask + halfSupport, ymask + halfSupport, zmask + halfSupport)
//                         * Q(QAnchor.x() + xmask, QAnchor.y() + ymask, QAnchor.z() + zmask);
          newForce += currentForce;
        }
      }
    }
    pforcenew[pidx] = newForce;
  }
}

