/*
 * The MIT License
 *
 * Copyright (c) 1997-2012 The University of Utah
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

#ifndef UINTAH_MD_ELECTROSTATICS_H
#define UINTAH_MD_ELECTROSTATICS_H

#include <CCA/Components/MD/MDSystem.h>
#include <CCA/Components/MD/SPMEGrid.h>
#include <CCA/Components/MD/SPMEGridMap.h>
#include <CCA/Components/MD/Transformation3D.h>
#include <CCA/Components/MD/SimpleGrid.h>
#include <Core/Geometry/Vector.h>
#include <Core/Geometry/Point.h>
#include <Core/Geometry/IntVector.h>
#include <Core/Grid/Patch.h>
#include <Core/Grid/Variables/Array3.h>

#include <vector>
#include <complex>

namespace Uintah {

using SCIRun::Vector;
using SCIRun::IntVector;

/**
 *  @class Electrostatics
 *  @ingroup MD
 *  @author Alan Humphrey and Justin Hooper
 *  @date   December, 2012
 *
 *  @brief
 *
 *  @param
 */
class Electrostatics {

  public:

    /**
     * @brief
     * @param
     */
    Electrostatics();

    /**
     * @brief
     * @param
     */
    ~Electrostatics();

    SPMEGrid<double> initializeSPME(const MDSystem& system,
                                    const IntVector& ewaldMeshLimits,
                                    const Matrix3& cellInverse,
                                    const Matrix3& cell,
                                    const double& ewaldScale,
                                    int splineOrder);

    /**
     * @brief
     * @param
     * @return
     */
    void performSPME(const MDSystem& system,
                     const PatchSet* patches);

  private:

    /**
     * @brief
     * @param
     * @return
     */
    SPMEGridMap<double> createChargeGridMap(const SPMEGrid<std::complex<double> >& grid,
                                            const MDSystem& system,
                                            const Patch* patch);

    /**
     * @brief
     * @param
     * @return
     */
    std::vector<Point> calcReducedCoords(const std::vector<Point>& localRealCoordinates,
                                         const Transformation3D<std::complex<double> >& invertSpace);

    /**
     * @brief
     * @param
     * @return
     */
    void calculateStaticGrids(const IntVector& gridExtents,
                              const IntVector& offset,
                              const MDSystem& system,
                              SimpleGrid<std::complex<double> >& fBGrid,
                              SimpleGrid<double>& fCGrid,
                              SimpleGrid<std::complex<double> >& fStressPre,
                              Vector& M1,
                              Vector& M2,
                              Vector& M3);

    /**
     * @brief
     * @param
     * @return
     */
    SimpleGrid<double> fC(const IntVector& gridExtents,
                          const MDSystem& system);

    /**
     * @brief
     * @param
     * @return
     */
    SimpleGrid<std::complex<double> > fB(const IntVector& gridExtents,
                                         const MDSystem& system,
                                         int splineOrder);

    /**
     * @brief
     * @param
     * @return
     */
    vector<std::complex<double> > calculateOrdinalSpline(int orderMinusOne,
                                                         int splineOrder);

    /**
     * @brief
     * @param
     * @return
     */
    vector<std::complex<double> > generateBVector(int points,
                                                  const std::vector<double>& M,
                                                  int max,
                                                  int splineOrder,
                                                  const std::vector<double>& splineCoeff);

    /**
     * @brief
     * @param
     * @return
     */
    inline vector<double> generateMVector(unsigned int points,
                                          int shift,
                                          int max) const
    {
      std::vector<double> m(points);
      int halfMax = max / 2;

      for (size_t i = 0; i < points; ++i) {
        m[i] = i + shift;
        if (m[i] > halfMax) {
          m[i] -= max;
        }
      }
      return m;
    }

};

}  // End namespace Uintah

#endif
