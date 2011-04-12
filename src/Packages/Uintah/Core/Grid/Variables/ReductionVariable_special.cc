/*

The MIT License

Copyright (c) 1997-2009 Center for the Simulation of Accidental Fires and 
Explosions (CSAFE), and  Scientific Computing and Imaging Institute (SCI), 
University of Utah.

License for the specific language governing rights and limitations under
Permission is hereby granted, free of charge, to any person obtaining a 
copy of this software and associated documentation files (the "Software"),
to deal in the Software without restriction, including without limitation 
the rights to use, copy, modify, merge, publish, distribute, sublicense, 
and/or sell copies of the Software, and to permit persons to whom the 
Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included 
in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS 
OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, 
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL 
THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER 
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING 
FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER 
DEALINGS IN THE SOFTWARE.

*/


#include <Packages/Uintah/Core/Grid/Variables/ReductionVariable.h>

#include <Core/Geometry/Vector.h>
#include <Core/Util/FancyAssert.h>

#include <sci_defs/bits_defs.h> // for SCI_32BITS

#include <Packages/Uintah/Core/Disclosure/TypeUtils.h>
#include <Packages/Uintah/Core/Grid/uintahshare.h>

using namespace Uintah;
using namespace SCIRun;

namespace Uintah { // <- This is necessary for IBM SP AIX xlC Compiler

#if !defined(__digital__) || defined(__GNUC__)
template<>
#endif
UINTAHSHARE void
ReductionVariable<double, Reductions::Min<double> >
::getMPIInfo(int& count, MPI_Datatype& datatype, MPI_Op& op)
{
   datatype = MPI_DOUBLE;
   count = 1;
   op = MPI_MIN;
}

#if !defined(__digital__) || defined(__GNUC__)
template<>
#endif
UINTAHSHARE void
ReductionVariable<double, Reductions::Min<double> >
::getMPIData(vector<char>& data, int& index)
{
  ASSERTRANGE(index, 0, static_cast<int>(data.size()+1-sizeof(double)));
  double* ptr = reinterpret_cast<double*>(&data[index]);
  *ptr = value;
  index += sizeof(double);
}

#if !defined( SCI_32BITS )
#  if !defined(__digital__) || defined(__GNUC__)
template<>
#  endif
UINTAHSHARE void
ReductionVariable<long long, Reductions::Min<long long> >
::getMPIData(vector<char>& data, int& index)
{
  ASSERTRANGE(index, 0, static_cast<int>(data.size()+1-sizeof(long long)));
  long long* ptr = reinterpret_cast<long long*>(&data[index]);
  *ptr = value;
  index += sizeof(long long);
}

#  if !defined(__digital__) || defined(__GNUC__)
template<>
#  endif
UINTAHSHARE void
ReductionVariable<long long, Reductions::Sum<long long> >
::getMPIData(vector<char>& data, int& index)
{
  ASSERTRANGE(index, 0, static_cast<int>(data.size()+1-sizeof(long long)));
  long long* ptr = reinterpret_cast<long long*>(&data[index]);
  *ptr = value;
  index += sizeof(long long);
}
#endif

#if !defined(__digital__) || defined(__GNUC__)
template<>
#endif
UINTAHSHARE void
ReductionVariable<double, Reductions::Min<double> >
::putMPIData(vector<char>& data, int& index)
{
  ASSERTRANGE(index, 0, static_cast<int>(data.size()+1-sizeof(double)));
  double* ptr = reinterpret_cast<double*>(&data[index]);
  value = *ptr;
  index += sizeof(double);
}

#if !defined(__digital__) || defined(__GNUC__)
template<>
#endif

UINTAHSHARE void
ReductionVariable<double, Reductions::Max<double> >
   ::getMPIInfo(int& count, MPI_Datatype& datatype, MPI_Op& op)
{
   datatype = MPI_DOUBLE;
   count = 1;
   op = MPI_MAX;
}

#if !defined(__digital__) || defined(__GNUC__)
template<>
#endif

UINTAHSHARE void
ReductionVariable<double, Reductions::Max<double> >
   ::getMPIData(vector<char>& data, int& index)
{
  ASSERTRANGE(index, 0, static_cast<int>(data.size()+1-sizeof(double)));
  double* ptr = reinterpret_cast<double*>(&data[index]);
  *ptr = value;
  index += sizeof(double);
}

#if !defined(__digital__) || defined(__GNUC__)
template<>
#endif
UINTAHSHARE void
ReductionVariable<double, Reductions::Max<double> >
::putMPIData(vector<char>& data, int& index)
{
  ASSERTRANGE(index, 0, static_cast<int>(data.size()+1-sizeof(double)));
  double* ptr = reinterpret_cast<double*>(&data[index]);
  value = *ptr;
  index += sizeof(double);
}

#if !defined(__digital__) || defined(__GNUC__)
template<>
#endif
UINTAHSHARE void
ReductionVariable<double, Reductions::Sum<double> >
::getMPIInfo(int& count, MPI_Datatype& datatype, MPI_Op& op)
{
   datatype = MPI_DOUBLE;
   count = 1;
   op = MPI_SUM;
}

#if !defined( SCI_32BITS )
#  if !defined(__digital__) || defined(__GNUC__)
template<>
#  endif
UINTAHSHARE void
ReductionVariable<long long, Reductions::Sum<long long> >
::getMPIInfo(int& count, MPI_Datatype& datatype, MPI_Op& op)
{
   datatype = MPI_LONG_LONG;
   count = 1;
   op = MPI_SUM;
}
#endif

#if !defined(__digital__) || defined(__GNUC__)
template<>
#endif
UINTAHSHARE void
ReductionVariable<double, Reductions::Sum<double> >
::getMPIData(vector<char>& data, int& index)
{
  ASSERTRANGE(index, 0, static_cast<int>(data.size()+1-sizeof(double)));
  double* ptr = reinterpret_cast<double*>(&data[index]);
  *ptr = value;
  index += sizeof(double);
}

#if !defined(__digital__) || defined(__GNUC__)
template<>
#endif

UINTAHSHARE void
ReductionVariable<double, Reductions::Sum<double> >
   ::putMPIData(vector<char>& data, int& index)
{
  ASSERTRANGE(index, 0, static_cast<int>(data.size()+1-sizeof(double)));
  double* ptr = reinterpret_cast<double*>(&data[index]);
  value = *ptr;
  index += sizeof(double);
}

#if !defined(__digital__) || defined(__GNUC__)
template<>
#endif

UINTAHSHARE void
ReductionVariable<bool, Reductions::And<bool> >
   ::getMPIInfo(int& count, MPI_Datatype& datatype, MPI_Op& op)
{
   datatype = MPI_CHAR;
   count = 1;
   op = MPI_LAND;
}

#if !defined(__digital__) || defined(__GNUC__)
template<>
#endif

UINTAHSHARE void
ReductionVariable<bool, Reductions::And<bool> >
   ::getMPIData(vector<char>& data, int& index)
{
  ASSERTRANGE(index, 0, static_cast<int>(data.size()+1-sizeof(char)));
  char* ptr = reinterpret_cast<char*>(&data[index]);
  *ptr = value;
  index += sizeof(char);
}

#if !defined(__digital__) || defined(__GNUC__)
template<>
#endif
UINTAHSHARE void
ReductionVariable<bool, Reductions::And<bool> >
::putMPIData(vector<char>& data, int& index)
{
  ASSERTRANGE(index, 0, static_cast<int>(data.size()+1-sizeof(char)));
  char* ptr = reinterpret_cast<char*>(&data[index]);
  value = *ptr;
  index += sizeof(char);
}

#if defined( SCI_32BITS ) || !defined(__APPLE__)
#  if defined( REDSTORM ) || !defined( __PGI ) 
// We reduce a "long", not a long64 because on 2/24/03, LAM-MPI did not
// support MPI_Reduce for LONG_LONG_INT.  We could use MPI_Create_op instead?
#    if !defined(__digital__) || defined(__GNUC__)
template<>
#    endif
UINTAHSHARE void
ReductionVariable<long64, Reductions::Sum<long64> >
::getMPIInfo(int& count, MPI_Datatype& datatype, MPI_Op& op)
{
   datatype = MPI_LONG;
   count = 1;
   op = MPI_SUM;
}
#  endif

#  if defined( REDSTORM) || !defined( __PGI )
#    if !defined(__digital__) || defined(__GNUC__)
template<>
#    endif
UINTAHSHARE void
ReductionVariable<long64, Reductions::Sum<long64> >
::getMPIData(vector<char>& data, int& index)
{
  ASSERTRANGE(index, 0, static_cast<int>(data.size()+1-sizeof(long)));
  long* ptr = reinterpret_cast<long*>(&data[index]);
  *ptr = value;
  index += sizeof(long);
}
#  endif
#endif

#if !defined(__digital__) || defined(__GNUC__)
template<>
#endif
UINTAHSHARE void
ReductionVariable<long64, Reductions::Sum<long64> >
::putMPIData(vector<char>& data, int& index)
{
  ASSERTRANGE(index, 0, static_cast<int>(data.size()+1-sizeof(long)));
  long* ptr = reinterpret_cast<long*>(&data[index]);
  value = *ptr;
  index += sizeof(long);
}

#if defined( SCI_32BITS ) || !defined(__APPLE__)
#  if defined( REDSTORM ) || !defined( __PGI )
#    if !defined( SCI_32BITS )
#      if !defined(__digital__) || defined(__GNUC__)
template<>
#      endif
UINTAHSHARE void
ReductionVariable<long long, Reductions::Sum<long long> >
::putMPIData(vector<char>& data, int& index)
{
  ASSERTRANGE(index, 0, static_cast<int>(data.size()+1-sizeof(long long)));
  long long* ptr = reinterpret_cast<long long*>(&data[index]);
  value = *ptr;
  index += sizeof(long long);
}
#    endif
#  endif
#endif

#if !defined(__digital__) || defined(__GNUC__)
template<>
#endif
UINTAHSHARE void
ReductionVariable<Vector, Reductions::Sum<Vector> >
::getMPIInfo(int& count, MPI_Datatype& datatype, MPI_Op& op)
{
   datatype = MPI_DOUBLE;
   count = 3;
   op = MPI_SUM;
}

#if !defined(__digital__) || defined(__GNUC__)
template<>
#endif
UINTAHSHARE void
ReductionVariable<Vector, Reductions::Sum<Vector> >
::getMPIData(vector<char>& data, int& index)
{	
  ASSERTRANGE(index, 0, static_cast<int>(data.size()+1-3*sizeof(double)));
  double* ptr = reinterpret_cast<double*>(&data[index]);
  *ptr++ = value.x();
  *ptr++ = value.y();
  *ptr++ = value.z();
}

#if !defined(__digital__) || defined(__GNUC__)
template<>
#endif

UINTAHSHARE void
ReductionVariable<Vector, Reductions::Sum<Vector> >
   ::putMPIData(vector<char>& data, int& index)
{
  ASSERTRANGE(index, 0, static_cast<int>(data.size()+1-3*sizeof(double)));
  double* ptr = reinterpret_cast<double*>(&data[index]);
  value.x(*ptr++);
  value.y(*ptr++);
  value.z(*ptr++);
}

} // end namespace Uintah
