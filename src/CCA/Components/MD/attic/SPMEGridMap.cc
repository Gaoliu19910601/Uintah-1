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

#include <CCA/Components/MD/SPMEGridMap.h>
#include <CCA/Components/MD/SPMEGridPoint.h>
#include <Core/Math/UintahMiscMath.h>
#include <Core/Geometry/BBox.h>
#include <Core/Math/MiscMath.h>
#include <Core/Grid/Variables/CCVariable.h>
#include <Core/Grid/Variables/CellIterator.h>

#include <iostream>
#include <complex>

#include <sci_values.h>

using namespace Uintah;
using namespace SCIRun;

template<class T>
SPMEGridMap<T>::SPMEGridMap()
{

}

template<class T>
SPMEGridMap<T>::~SPMEGridMap()
{

}

template<class T>
double SPMEGridMap<T>::mapChargeFromAtoms()
{
  return NULL;
}

template<class T>
void SPMEGridMap<T>::mapChargeFromAtoms(constCCVariable<std::complex<double> >& atoms)
{

}

template<class T>
void SPMEGridMap<T>::mapForceToAtoms(const constCCVariable<std::complex<double> >& atoms)
{

}

template<class T>
SPMEGridPoint<std::complex<double> >& SPMEGridMap<T>::addMapPoint()
{
  return NULL;
}