#include "ParticlesNeighbor.h"

#include "CellsNeighbor.h"
#include "Lattice.h"
#include "Cell.h"

#include <Uintah/Grid/Patch.h>

namespace Uintah {
namespace MPM {

void  ParticlesNeighbor::buildIncluding(const particleIndex& pIndex,
                                        const Lattice& lattice)
{
  CellsNeighbor cellsNeighbor;
  cellsNeighbor.buildIncluding(
    lattice.getPatch()->getLevel()->getCellIndex(lattice.getParticlesPosition()[pIndex]),
    lattice);
  
  for(CellsNeighbor::const_iterator iter_cell = cellsNeighbor.begin();
    iter_cell != cellsNeighbor.end();
    ++iter_cell )
  {
    list<particleIndex>& pList = lattice[*iter_cell].particleList;
    for( list<particleIndex>::const_iterator iter_p = pList.begin();
         iter_p != pList.end();
         ++iter_p )
    {
      push_back(*iter_p);
    }
  }
}

void  ParticlesNeighbor::buildExcluding(const particleIndex& pIndex,
                                        const Lattice& lattice)
{
  CellsNeighbor cellsNeighbor;
  cellsNeighbor.buildIncluding(
    lattice.getPatch()->getLevel()->getCellIndex(lattice.getParticlesPosition()[pIndex]),
    lattice);
  
  for(CellsNeighbor::const_iterator iter_cell = cellsNeighbor.begin();
    iter_cell != cellsNeighbor.end();
    ++iter_cell )
  {
    list<particleIndex>& pList = lattice[*iter_cell].particleList;
    for( list<particleIndex>::const_iterator iter_p = pList.begin();
         iter_p != pList.end();
         ++iter_p )
    {
      if( (*iter_p) != pIndex ) push_back(*iter_p);
    }
  }
}

void
ParticlesNeighbor::
interpolateVector( const ParticleVariable<Vector>& pVector,
                   Vector* vector, 
                   Matrix3* gradient) const
{
}

void
ParticlesNeighbor::
interpolatedouble(const ParticleVariable<double>& pdouble,
                  double* data,
                  Vector* gradient) const
{
}

} //namespace MPM
} //namespace Uintah

// $Log$
// Revision 1.4  2000/06/15 21:57:10  sparker
// Added multi-patch support (bugzilla #107)
// Changed interface to datawarehouse for particle data
// Particles now move from patch to patch
//
// Revision 1.3  2000/06/06 01:58:25  tan
// Finished functions build particles neighbor for a given particle
// index.
//
// Revision 1.2  2000/06/05 22:30:11  tan
// Added interpolateVector and interpolatedouble for least-square approximation.
//
// Revision 1.1  2000/06/05 21:15:36  tan
// Added class ParticlesNeighbor to handle neighbor particles searching.
//
