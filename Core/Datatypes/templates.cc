/*
 * Manual template instantiations
 */


/*
 * These aren't used by Datatypes directly, but since they are used in
 * a lot of different modules, we instantiate them here to avoid bloat
 *
 * Find the bloaters with:
find . -name "*.ii" -print | xargs cat | sort | uniq -c | sort -nr | more
 */

#include <Core/Containers/LockingHandle.h>
#include <Core/Malloc/Allocator.h>

using namespace SCIRun;
#ifdef __sgi
#pragma set woff 1468
#endif


#include <Core/Datatypes/ColumnMatrix.h>
template class LockingHandle<ColumnMatrix>;

#include <Core/Datatypes/Matrix.h>
template class LockingHandle<Matrix>;

#include <Core/Datatypes/TetVol.h>
template class TetVol<double>;

#include <Core/Datatypes/LatticeVol.h>
template class LatticeVol<double>;

#include <Core/Datatypes/GenericField.h>
#include <Core/Persistent/PersistentSTL.h>

#include <Core/Datatypes/TriSurfMesh.h>
template class GenericField<TriSurfMesh, vector<double> >;

#ifdef __sgi
#pragma reset woff 1468
#endif










