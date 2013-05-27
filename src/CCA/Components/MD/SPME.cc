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
#include <Core/Grid/Variables/VarTypes.h>
#include <Core/Grid/Box.h>
#include <Core/Geometry/IntVector.h>
#include <Core/Geometry/Point.h>
#include <Core/Math/MiscMath.h>
#include <Core/Util/DebugStream.h>

#include <iostream>
#include <iomanip>

#include <sci_values.h>
#include <sci_defs/fftw_defs.h>

#ifdef DEBUG
#include <Core/Util/FancyAssert.h>
#endif

using namespace Uintah;

extern SCIRun::Mutex cerrLock;

static DebugStream spme_dbg("SPMEDBG", false);

SPME::SPME()
{

}

SPME::~SPME()
{
  // cleanup the memory we have dynamically allocated
  std::vector<SPMEPatch*>::iterator PatchIterator;
  for (PatchIterator = d_spmePatches.begin(); PatchIterator != d_spmePatches.end(); ++PatchIterator) {
    SPMEPatch* spmePatch = *PatchIterator;

    SimpleGrid<std::complex<double> >* q = spmePatch->getQ();
    if (q) {
      delete q;
    }

    SimpleGrid<double>* theta = spmePatch->getTheta();
    if (theta) {
      delete theta;
    }

    SimpleGrid<Matrix3>* stressPrefactor = spmePatch->getStressPrefactor();
    if (stressPrefactor) {
      delete stressPrefactor;
    }

    delete spmePatch;
  }

  if (d_Q) {
    delete d_Q;
  }

  // FFTW cleanup
  fftw_destroy_plan(d_forwardTransformPlan);
  fftw_destroy_plan(d_backwardTransformPlan);
  fftw_cleanup();
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
void SPME::initialize(const ProcessorGroup* pg,
                      const PatchSubset* patches,
                      const MaterialSubset* materials,
                      DataWarehouse* old_dw,
                      DataWarehouse* new_dw)
{
  // We call SPME::initialize from MD::initialize, or if we've somehow maintained our object across a system change

  const Patch* patch = patches->get(0);
  Vector totalCellExtent = (d_system->getCellExtent()).asVector();
  Vector patchLowIndex = (patch->getCellLowIndex()).asVector();
  Vector patchHighIndex = (patch->getCellHighIndex()).asVector();

  SCIRun::IntVector patchKLow, patchKHigh;
  for (size_t idx = 0; idx < 3; ++idx) {
    int KComponent = d_kLimits(idx);
    patchKLow[idx] = ceil(static_cast<double>(KComponent) * (patchLowIndex[idx] / totalCellExtent[idx]));
    patchKHigh[idx] = floor(static_cast<double>(KComponent) * (patchHighIndex[idx] / totalCellExtent[idx]));
  }

  IntVector patchKGridExtents = (patchKHigh - patchKLow);
  IntVector patchKGridOffset = patchKLow;
  int splineHalfMaxSupport = d_interpolatingSpline.getHalfMaxSupport();
  IntVector plusGhostExtents = IntVector(splineHalfMaxSupport, splineHalfMaxSupport, splineHalfMaxSupport);
  IntVector minusGhostExtents = plusGhostExtents;  // ensure symmetry

  // Check to make sure plusGhostExtents+minusGhostExtents is right way to enter number of ghost cells (i.e. total, not per offset)
  d_Q = scinew SimpleGrid<dblcomplex>(patchKGridExtents, patchKGridOffset, 2 * splineHalfMaxSupport);
  d_Q->initialize(dblcomplex(0.0, 0.0));

  IntVector extents = d_Q->getExtents();
  int xdim = extents[0];
  int ydim = extents[1];
  int zdim = extents[2];

  fftw_complex* array_fft = reinterpret_cast<fftw_complex*>(d_Q->getDataPtr());

  d_forwardTransformPlan = fftw_plan_dft_3d(xdim, ydim, zdim, array_fft, array_fft, FFTW_FORWARD, FFTW_ESTIMATE);
  d_backwardTransformPlan = fftw_plan_dft_3d(xdim, ydim, zdim, array_fft, array_fft, FFTW_BACKWARD, FFTW_ESTIMATE);

  new_dw->put(q_kgrid_sum(*(LinearArray3<dblcomplex>*)d_Q->getDataPtr()), d_lb->QLabel);

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
  size_t numPatches = patches->size();
  d_spmePatches.reserve(numPatches);

  for (size_t p = 0; p < numPatches; p++) {
    const Patch* patch = patches->get(p);

    Vector totalCellExtent = (d_system->getCellExtent()).asVector();
    Vector patchLowIndex = (patch->getCellLowIndex()).asVector();
    Vector patchHighIndex = (patch->getCellHighIndex()).asVector();

    SCIRun::IntVector patchKLow, patchKHigh;
    for (size_t idx = 0; idx < 3; ++idx) {
      int KComponent = d_kLimits(idx);
      patchKLow[idx] = ceil(static_cast<double>(KComponent) * (patchLowIndex[idx] / totalCellExtent[idx]));
      patchKHigh[idx] = floor(static_cast<double>(KComponent) * (patchHighIndex[idx] / totalCellExtent[idx]));
    }

    IntVector patchKGridExtents = (patchKHigh - patchKLow);
    IntVector patchKGridOffset = patchKLow;
    int splineHalfMaxSupport = d_interpolatingSpline.getHalfMaxSupport();
    IntVector plusGhostExtents = IntVector(splineHalfMaxSupport, splineHalfMaxSupport, splineHalfMaxSupport);
    IntVector minusGhostExtents = plusGhostExtents;  // ensure symmetry

    SPMEPatch* spmePatch = new SPMEPatch(patchKGridExtents, patchKGridOffset, plusGhostExtents, minusGhostExtents, patch);

    // Check to make sure plusGhostExtents+minusGhostExtents is right way to enter number of ghost cells (i.e. total, not per offset)
    SimpleGrid<dblcomplex>* q = scinew SimpleGrid<dblcomplex>(patchKGridExtents, patchKGridOffset, 2 * splineHalfMaxSupport);
    q->initialize(std::complex<double>(0.0, 0.0));

    // No ghost cells; internal only
    SimpleGrid<Matrix3>* stressPrefactor = scinew SimpleGrid<Matrix3>(patchKGridExtents, patchKGridOffset, 0);
    calculateStressPrefactor(stressPrefactor, patchKGridExtents, patchKGridOffset);

    // No ghost cells; internal only
    SimpleGrid<double>* fTheta = scinew SimpleGrid<double>(patchKGridExtents, patchKGridOffset, 0);
    fTheta->initialize(0.0);

    // Calculate B and C - we should only have to do this if KLimits or the inverse cell changes
    SimpleGrid<double> fBGrid = calculateBGrid(patchKGridExtents, patchKGridOffset);
    SimpleGrid<double> fCGrid = calculateCGrid(patchKGridExtents, patchKGridOffset);

    // Composite B and C into Theta
    size_t xExtent = patchKGridExtents.x();
    size_t yExtent = patchKGridExtents.y();
    size_t zExtent = patchKGridExtents.z();
    for (size_t xidx = 0; xidx < xExtent; ++xidx) {
      for (size_t yidx = 0; yidx < yExtent; ++yidx) {
        for (size_t zidx = 0; zidx < zExtent; ++zidx) {

          // Composite B and C into Theta
          (*fTheta)(xidx, yidx, zidx) = fBGrid(xidx, yidx, zidx) * fCGrid(xidx, yidx, zidx);

          if (spme_dbg.active()) {
            cerrLock.lock();
            // looking for "nan" values interspersed in B and C
            if (std::isnan(fBGrid(xidx, yidx, zidx))) {
              std::cout << "B: " << xidx << " " << yidx << " " << zidx << std::endl;
              std::cin.get();
            }
            if (std::isnan(fCGrid(xidx, yidx, zidx))) {
              std::cout << "C: " << xidx << " " << yidx << " " << zidx << std::endl;
              std::cin.get();
            }
            cerrLock.unlock();
          }
        }
      }
    }
    spmePatch->setTheta(fTheta);
    spmePatch->setStressPrefactor(stressPrefactor);
    spmePatch->setQ(q);
    d_spmePatches.push_back(spmePatch);
  }
}

bool SPME::checkConvergence()
{
  // Subroutine determines if polarizable component has converged
  bool polarizable = getPolarizableCalculation();
  if (!polarizable) {
    return true;
  } else {
    // Do nothing at the moment, but eventually will check convergence here.
    std::cerr << "Error: Polarizable force field not yet implemented!";
    return false;
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
  int maxIterations = d_system->getMaxPolarizableIterations();
  while (!converged && (numIterations < maxIterations)) {

    // Do calculation steps until the Real->Fourier space transform
    calculatePreTransform(pg, patches, materials, old_dw, new_dw);

    // !FIXME We need to force Q reduction here
    // Reduce, forward transform, and redistribute charge grid
    transformRealToFourier(pg, patches, materials, old_dw, new_dw);

    // Do Fourier space calculations on transformed data
    calculateInFourierSpace(pg, patches, materials, old_dw, new_dw);

    // !FIXME We need to force Q reduction here
    // Reduce, reverse transform, and redistribute
    transformFourierToReal(pg, patches, materials, old_dw, new_dw);

    checkConvergence();
    numIterations++;
  }

  // Do force spreading and clean up calculations -- or does this go in finalize?
  SPME::calculatePostTransform(pg, patches, materials, old_dw, new_dw);
}

void SPME::calculatePreTransform(const ProcessorGroup* pg,
                                 const PatchSubset* patches,
                                 const MaterialSubset* materials,
                                 DataWarehouse* old_dw,
                                 DataWarehouse* new_dw)
{
  std::vector<SPMEPatch*>::iterator PatchIterator;
  for (PatchIterator = d_spmePatches.begin(); PatchIterator != d_spmePatches.end(); ++PatchIterator) {
    SPMEPatch* spmePatch = *PatchIterator;

    const Patch* patch = spmePatch->getPatch();
    ParticleSubset* pset = old_dw->getParticleSubset(materials->get(0), patch);

    //------------------------------------------------------------------
    // Carry over what was calculated in non-bonded.
    constParticleVariable<double> penergy;
    ParticleVariable<double> penergynew;
    old_dw->get(penergy, d_lb->pEnergyLabel, pset);
    new_dw->allocateAndPut(penergynew, d_lb->pEnergyLabel_preReloc, pset);
    penergynew.copyData(penergy);
    new_dw->put(sum_vartype(0.0), d_lb->vdwEnergyLabel);
    //------------------------------------------------------------------

    constParticleVariable<Point> px;
    constParticleVariable<double> pcharge;
    constParticleVariable<long64> pids;
    old_dw->get(px, d_lb->pXLabel, pset);
    old_dw->get(pcharge, d_lb->pChargeLabel, pset);
    old_dw->get(pids, d_lb->pParticleIDLabel, pset);

    // When we have a material iterator in here, we should store/get charge by material.
    // Charge represents the static charge on a particle, which is set by particle type.
    // No need to store one for each particle. -- JBH

    // Generate the data that maps the charges in the patch onto the grid
    d_gridMap = generateChargeMap(pset, px, pids, d_interpolatingSpline);

    // !FIXME Need to put Q in for reduction
    // We have now set up the real-space Q grid.
    // We need to store this patch's Q grid on the data warehouse (?) to pass through to the transform
    SimpleGrid<std::complex<double> >* Q = spmePatch->getQ();
//    LinearArray3<std::complex<double> > q;
//    q.initialize(std::complex<double>(0.0, 0.0));
//    new_dw->put(q_kgrid_sum(q), d_lb->QLabel);

    // Calculate Q(r)
    mapChargeToGrid(spmePatch, d_gridMap, pset, pcharge, d_interpolatingSpline.getHalfMaxSupport());
  }
}

void SPME::calculateInFourierSpace(const ProcessorGroup* pg,
                                   const PatchSubset* patches,
                                   const MaterialSubset* materials,
                                   DataWarehouse* old_dw,
                                   DataWarehouse* new_dw)
{
  std::vector<SPMEPatch*>::iterator PatchIterator;
  for (PatchIterator = d_spmePatches.begin(); PatchIterator != d_spmePatches.end(); PatchIterator++) {
    SPMEPatch* spmePatch = *PatchIterator;
    SimpleGrid<std::complex<double> >* Q = spmePatch->getQ();
    SimpleGrid<double>* fTheta = spmePatch->getTheta();
    SimpleGrid<Matrix3>* stressPrefactor = spmePatch->getStressPrefactor();

    // Multiply the transformed Q by B*C to get Theta
    IntVector localExtents = spmePatch->getLocalExtents();
    size_t xMax = localExtents.x();
    size_t yMax = localExtents.y();
    size_t zMax = localExtents.z();

    double spmeFourierEnergy = 0.0;
    Matrix3 spmeFourierStress(0.0);

    if (spme_dbg.active()) {
      cerrLock.lock();
      std::cout << std::endl << "Calculate (Q*Q^)*(B*C)" << std::endl;
      std::cin.get();
      cerrLock.unlock();
    }

    for (size_t kX = 0; kX < xMax; ++kX) {
      for (size_t kY = 0; kY < yMax; ++kY) {
        for (size_t kZ = 0; kZ < zMax; ++kZ) {
          std::complex<double> gridValue = (*Q)(kX, kY, kZ);

          // Calculate (Q*Q^)*(B*C)
          (*Q)(kX, kY, kZ) *= std::conj(gridValue) * (*fTheta)(kX, kY, kZ);
          spmeFourierEnergy += std::abs((*Q)(kX, kY, kZ));
          spmeFourierStress += std::abs((*Q)(kX, kY, kZ)) * (*stressPrefactor)(kX, kY, kZ);
        }
      }
    }

    // !FIXME Need to put Q in for reduction
    // Ready to go back to real space now -->  Need to store modified Q as well as localEnergy/localStress (which should get accumulated)
    new_dw->put(sum_vartype(0.5 * spmeFourierEnergy), d_lb->spmeFourierEnergyLabel);
    new_dw->put(matrix_sum(0.5 * spmeFourierStress), d_lb->spmeFourierStressLabel);
  }
}

void SPME::calculatePostTransform(const ProcessorGroup* pg,
                                  const PatchSubset* patches,
                                  const MaterialSubset* materials,
                                  DataWarehouse* old_dw,
                                  DataWarehouse* new_dw)
{
  std::vector<SPMEPatch*>::iterator PatchIterator;
  for (PatchIterator = d_spmePatches.begin(); PatchIterator != d_spmePatches.end(); ++PatchIterator) {
    SPMEPatch* spmePatch = *PatchIterator;

    const Patch* patch = spmePatch->getPatch();

    ParticleSubset* pset = old_dw->getParticleSubset(materials->get(0), patch);

    constParticleVariable<double> pcharge;
    old_dw->get(pcharge, d_lb->pChargeLabel, pset);

    ParticleVariable<Vector> pforcenew;
    new_dw->allocateAndPut(pforcenew, d_lb->pForceLabel_preReloc, pset);

    // Calculate electrostatic contribution to f_ij(r)
    mapForceFromGrid(spmePatch, d_gridMap, pset, pcharge, pforcenew, d_interpolatingSpline.getHalfMaxSupport());

    ParticleVariable<double> pchargenew;
    new_dw->allocateAndPut(pchargenew, d_lb->pChargeLabel_preReloc, pset);
    // carry these values over for now
    pchargenew.copyData(pcharge);
  }
}

void SPME::transformRealToFourier(const ProcessorGroup* pg,
                                  const PatchSubset* patches,
                                  const MaterialSubset* materials,
                                  DataWarehouse* old_dw,
                                  DataWarehouse* new_dw)
{
//  std::vector<SPMEPatch*>::iterator PatchIterator;
//  for (PatchIterator = d_spmePatches.begin(); PatchIterator != d_spmePatches.end(); PatchIterator++) {
//    SPMEPatch* spmePatch = *PatchIterator;
//    SimpleGrid<dblcomplex>* Q = spmePatch->getQ();
//
//
//  }
  fftw_execute(d_forwardTransformPlan);
}

void SPME::transformFourierToReal(const ProcessorGroup* pg,
                                  const PatchSubset* patches,
                                  const MaterialSubset* materials,
                                  DataWarehouse* old_dw,
                                  DataWarehouse* new_dw)
{
//  std::vector<SPMEPatch*>::iterator PatchIterator;
//  for (PatchIterator = d_spmePatches.begin(); PatchIterator != d_spmePatches.end(); PatchIterator++) {
//    SPMEPatch* spmePatch = *PatchIterator;
//    SimpleGrid<dblcomplex>* Q = spmePatch->getQ();
//
//
//  }
  fftw_execute(d_backwardTransformPlan);
}

void SPME::finalize(const ProcessorGroup* pg,
                    const PatchSubset* patches,
                    const MaterialSubset* materials,
                    DataWarehouse* old_dw,
                    DataWarehouse* new_dw)
{
  // Do cleanup here
  new_dw->put(q_kgrid_sum(*(LinearArray3<dblcomplex>*)d_Q->getDataPtr()), d_lb->QLabel);
}

std::vector<SCIRun::dblcomplex> SPME::generateBVector(const std::vector<double>& mFractional,
                                                      const int initialIndex,
                                                      const int localGridExtent,
                                                      const CenteredCardinalBSpline& interpolatingSpline) const
{
  double PI = acos(-1.0);
  double twoPI = 2.0 * PI;
  double splineOrder = interpolatingSpline.getOrder();
  int halfSupport = interpolatingSpline.getHalfMaxSupport();

  std::vector<dblcomplex> B(localGridExtent);
  std::vector<double> zeroAlignedSpline = interpolatingSpline.evaluateGridAligned(0);

  size_t endingIndex = initialIndex + localGridExtent;

// Formula looks significantly different from given SPME for offset splines.
//   See Essmann et. al., J. Chem. Phys. 103 8577 (1995). for conversion, particularly formula C3 pt. 2 (paper uses pt. 3)
// Note:  twoPi_m_over_K is almost a stand-in for z in the paper's language, however we defer the complex exponentiation
  for (size_t BIndex = initialIndex; BIndex < endingIndex; ++BIndex) {

    /*    int AdjustedIndex = BIndex - SplineZeroIndex;
     if (AdjustedIndex < 0) AdjustedIndex += KMax;
     if (AdjustedIndex >= KMax) AdjustedIndex -= KMax;
     */
    double twoPi_m_over_K = twoPI * mFractional[BIndex];
    dblcomplex phi_N = dblcomplex(0, 0);
    for (int denomIndex = -halfSupport; denomIndex <= halfSupport; ++denomIndex) {
      double j = static_cast<double>(denomIndex - splineOrder);
      phi_N += zeroAlignedSpline[denomIndex + halfSupport] * dblcomplex(cos(j * twoPi_m_over_K), sin(j * twoPi_m_over_K));
    }
    B[BIndex] = 1.0 / phi_N;
  }
  return B;
}

SimpleGrid<double> SPME::calculateBGrid(const IntVector& localExtents,
                                        const IntVector& globalOffset) const
{
  size_t limit_Kx = d_kLimits.x();
  size_t limit_Ky = d_kLimits.y();
  size_t limit_Kz = d_kLimits.z();

  std::vector<double> mf1 = SPME::generateMFractionalVector(limit_Kx);
  std::vector<double> mf2 = SPME::generateMFractionalVector(limit_Ky);
  std::vector<double> mf3 = SPME::generateMFractionalVector(limit_Kz);

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
  std::vector<double> mp1 = SPME::generateMPrimeVector(d_kLimits.x());
  std::vector<double> mp2 = SPME::generateMPrimeVector(d_kLimits.y());
  std::vector<double> mp3 = SPME::generateMPrimeVector(d_kLimits.z());

//  if (spme_dbg.active()) {
//    cerrLock.lock();
//    std::cout << " DEBUG: " << std::endl;
//    std::cout << "Expect mp1 size: " << d_kLimits.x() << "  Actual mp1 size: " << mp1.size() << std::endl;
//    for (size_t idx = 0; idx < mp1.size(); ++idx) {
//      std::cout << "mp1(" << std::setw(3) << idx << "): " << mp1[idx] << std::endl;
//    }
//    std::cout << "Expect mp2 size: " << d_kLimits.y() << "  Actual mp2 size: " << mp2.size() << std::endl;
//    for (size_t idx = 0; idx < mp2.size(); ++idx) {
//      std::cout << "mp2(" << std::setw(3) << idx << "): " << mp2[idx] << std::endl;
//    }
//    std::cout << "Expect mp3 size: " << d_kLimits.x() << "  Actual mp3 size: " << mp3.size() << std::endl;
//    for (size_t idx = 0; idx < mp3.size(); ++idx) {
//      std::cout << "mp3(" << std::setw(3) << idx << "): " << mp3[idx] << std::endl;
//    }
//    std::cout << " END DEBUG: " << std::endl;
//    cerrLock.unlock();
//  }

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

  std::cerr << "System Volume: " << d_systemVolume << endl;
//  std::cin.get();

  SimpleGrid<double> CGrid(extents, offset, 0);  // No ghost cells; internal only
  for (size_t kX = 0; kX < xExtents; ++kX) {
    for (size_t kY = 0; kY < yExtents; ++kY) {
      for (size_t kZ = 0; kZ < zExtents; ++kZ) {
        if (kX != 0 || kY != 0 || kZ != 0) {
          SCIRun::Vector m(mp1[kX + xOffset], mp2[kY + yOffset], mp3[kZ + zOffset]);
          m = m * d_inverseUnitCell;
          double M2 = m.length2();
          double factor = PI2 * M2 * invBeta2;
          CGrid(kX, kY, kZ) = invVolFactor * exp(-factor) / M2;
        }
      }
    }
  }
  CGrid(0, 0, 0) = 0;
  return CGrid;
}

void SPME::calculateStressPrefactor(SimpleGrid<Matrix3>* stressPrefactor,
                                    const IntVector& extents,
                                    const IntVector& offset)
{
  std::vector<double> mp1 = SPME::generateMPrimeVector(d_kLimits.x());
  std::vector<double> mp2 = SPME::generateMPrimeVector(d_kLimits.y());
  std::vector<double> mp3 = SPME::generateMPrimeVector(d_kLimits.z());

  size_t xExtents = extents.x();
  size_t yExtents = extents.y();
  size_t zExtents = extents.z();

  int XOffset = offset.x();
  int YOffset = offset.y();
  int ZOffset = offset.z();

  double PI = acos(-1.0);
  double PI2 = PI * PI;
  double invBeta2 = 1.0 / (d_ewaldBeta * d_ewaldBeta);

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

          (*stressPrefactor)(kX, kY, kZ) = localStressContribution;
        }
      }
    }
  }
  (*stressPrefactor)(0, 0, 0) = Matrix3(0.0);
}

std::vector<SPMEMapPoint> SPME::generateChargeMap(ParticleSubset* pset,
                                                  constParticleVariable<Point>& particlePositions,
                                                  constParticleVariable<long64>& particleIDs,
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
    // Should we put that in (requires modifying Uintah framework)?
    SCIRun::Vector kReal, splineValues;
    IntVector particleGridOffset;
    for (size_t idx = 0; idx < 3; ++idx) {
      kReal[idx] = (d_kLimits.asVector())[idx];  //static_cast<double>(kLimits[idx]);  // For some reason I can't construct a Vector from an IntVector -- Maybe we should fix that instead?
      particleGridCoordinates[idx] *= kReal[idx];         // Recast particle into charge grid based representation
      particleGridOffset[idx] = static_cast<int>(particleGridCoordinates[idx]);  // Reference grid point for particle
      splineValues[idx] = particleGridOffset[idx] - particleGridCoordinates[idx];  // spline offset for spline function
    }

    vector<double> xSplineArray = spline.evaluateGridAligned(splineValues[0]);
    vector<double> ySplineArray = spline.evaluateGridAligned(splineValues[1]);
    vector<double> zSplineArray = spline.evaluateGridAligned(splineValues[2]);

    vector<double> xSplineDeriv = spline.derivativeGridAligned(splineValues[0]);
    vector<double> ySplineDeriv = spline.derivativeGridAligned(splineValues[1]);
    vector<double> zSplineDeriv = spline.derivativeGridAligned(splineValues[2]);

    IntVector extents(xSplineArray.size(), ySplineArray.size(), zSplineArray.size());
    SimpleGrid<double> chargeGrid(extents, particleGridOffset, 0);

    SimpleGrid<SCIRun::Vector> forceGrid(extents, particleGridOffset, 0);
    size_t XExtent = xSplineArray.size();
    size_t YExtent = ySplineArray.size();
    size_t ZExtent = zSplineArray.size();
    for (size_t xidx = 0; xidx < XExtent; ++xidx) {
      double dampX = xSplineArray[xidx];
      for (size_t yidx = 0; yidx < YExtent; ++yidx) {
        double dampY = ySplineArray[yidx];
        double dampXY = dampX * dampY;
        for (size_t zidx = 0; zidx < ZExtent; ++zidx) {
          double dampZ = zSplineArray[zidx];
          double dampYZ = dampY * dampZ;
          double dampXZ = dampX * dampZ;

          chargeGrid(xidx, yidx, zidx) = dampXY * dampZ;
          forceGrid(xidx, yidx, zidx) = SCIRun::Vector(dampYZ * xSplineDeriv[xidx], dampXZ * ySplineDeriv[yidx],
                                                       dampXY * zSplineDeriv[zidx]);

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
                           constParticleVariable<double>& charges,
                           int halfSupport)
{
  // Reset charges before we start adding onto them.
  SimpleGrid<dblcomplex>* Q = spmePatch->getQ();
  Q->initialize(0.0);

  ParticleSubset::iterator particleIter;
  for (particleIter = pset->begin(); particleIter != pset->end(); particleIter++) {
    particleIndex pidx = *particleIter;

    double charge = charges[pidx];
    const SimpleGrid<double> chargeMap = gridMap[pidx].getChargeGrid();

    IntVector QAnchor = chargeMap.getOffset();  // Location of the 0,0,0 origin for the charge map grid
    IntVector SupportExtent = chargeMap.getExtents();  // Extents of the charge map grid
    for (int xmask = -halfSupport; xmask <= halfSupport - 2; ++xmask) {
      for (int ymask = -halfSupport; ymask <= halfSupport - 2; ++ymask) {
        for (int zmask = -halfSupport; zmask <= halfSupport - 2; ++zmask) {

          dblcomplex val = charge * chargeMap(xmask + halfSupport, ymask + halfSupport, zmask + halfSupport);

          int x_anchor = QAnchor.x() + xmask;
          if (x_anchor < 0)
            x_anchor += d_kLimits.x();
          if (x_anchor >= d_kLimits.x())
            x_anchor -= d_kLimits.x();

          int y_anchor = QAnchor.y() + ymask;
          if (y_anchor < 0)
            y_anchor += d_kLimits.y();
          if (y_anchor >= d_kLimits.y())
            y_anchor -= d_kLimits.y();

          int z_anchor = QAnchor.z() + zmask;
          if (z_anchor < 0)
            z_anchor += d_kLimits.z();
          if (z_anchor >= d_kLimits.z())
            z_anchor -= d_kLimits.z();

          // Wrapping done for single patch problem, solution will be totally different for multipatch problem!  !FIXME
          (*Q)(x_anchor, y_anchor, z_anchor) += val;
        }
      }
    }
  }
}

void SPME::mapForceFromGrid(SPMEPatch* spmePatch,
                            const std::vector<SPMEMapPoint>& gridMap,
                            ParticleSubset* pset,
                            constParticleVariable<double>& charges,
                            ParticleVariable<Vector>& pforcenew,
                            int halfSupport)
{
  SimpleGrid<std::complex<double> >* Q = spmePatch->getQ();

  for (ParticleSubset::iterator iter = pset->begin(); iter != pset->end(); iter++) {
    particleIndex pidx = *iter;

    SimpleGrid<SCIRun::Vector> forceMap = gridMap[pidx].getForceGrid();
    double charge = charges[pidx];

    double DimensionalCorrection = 1.0 / static_cast<double>(d_kLimits.x() * d_kLimits.y() * d_kLimits.z());
    SCIRun::Vector newForce = Vector(0, 0, 0);
    IntVector QAnchor = forceMap.getOffset();  // Location of the 0,0,0 origin for the force map grid
    IntVector supportExtent = forceMap.getExtents();  // Extents of the force map grid

    for (int xmask = -halfSupport; xmask <= halfSupport; ++xmask) {

      for (int ymask = -halfSupport; ymask <= halfSupport; ++ymask) {
        for (int zmask = -halfSupport; zmask <= halfSupport; ++zmask) {
          SCIRun::Vector currentForce;
          int x_anchor = QAnchor.x() + xmask;
          int y_anchor = QAnchor.y() + ymask;
          int z_anchor = QAnchor.z() + zmask;

          if (x_anchor < 0)
            x_anchor += d_kLimits.x();
          if (y_anchor < 0)
            y_anchor += d_kLimits.y();
          if (z_anchor < 0)
            z_anchor += d_kLimits.z();

          if (x_anchor >= d_kLimits.x())
            x_anchor -= d_kLimits.x();
          if (y_anchor >= d_kLimits.y())
            y_anchor -= d_kLimits.y();
          if (z_anchor >= d_kLimits.z())
            z_anchor -= d_kLimits.z();

          double QReal = real((*Q)(x_anchor, y_anchor, z_anchor));
          double QMag = std::abs((*Q)(x_anchor, y_anchor, z_anchor));
//          ASSERTRANGE(imag((*Q)(x_anchor, y_anchor, z_anchor)),-1e-18,1e-18);

          currentForce = forceMap(xmask + halfSupport, ymask + halfSupport, zmask + halfSupport) * QReal * charge;
          newForce += currentForce;
        }
      }
    }
    newForce *= DimensionalCorrection;
    //49227.43325439056
    /*
     *  f1=  -2.8440279063396052        2.6027710155811845       0.99069892785288971
     f2=  -1.2959262337781086       -2.2140864761359698        2.5686305300200476
     f3=   4.2899558647618461       -2.2060911777299874       -3.0672208465506792
     f4=   3.1706091601611490        2.0423127670957384       -4.2814464749644880
     f5=  -2.8145420677778201        2.3389711802598274        1.3094128311605495
     */

    if (pidx < 5) {
      std::cerr << "DimensionalCorrection: " << DimensionalCorrection << endl;
      double twoPI = 2.0 * acos(-1.0);
      std::cerr << " Force Check (" << pidx << "): " << newForce / twoPI << endl;
      pforcenew[pidx] = newForce;
    }

  }
}

