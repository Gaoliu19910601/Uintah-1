
#ifndef UINTAH_HOMEBREW_CellIterator_H
#define UINTAH_HOMEBREW_CellIterator_H

#include <Uintah/Grid/Array3Index.h>
#include <Uintah/Grid/Region.h>
#include <SCICore/Geometry/IntVector.h>

namespace Uintah {
   using SCICore::Geometry::IntVector;

/**************************************

CLASS
   CellIterator
   
   Short Description...

GENERAL INFORMATION

   CellIterator.h

   Steven G. Parker
   Department of Computer Science
   University of Utah

   Center for the Simulation of Accidental Fires and Explosions (C-SAFE)
  
   Copyright (C) 2000 SCI Group

KEYWORDS
   CellIterator

DESCRIPTION
   Long description...
  
WARNING
  
****************************************/

   class CellIterator {
   public:
      inline ~CellIterator() {}
      
      //////////
      // Insert Documentation Here:
      inline CellIterator operator++(int) {
	 CellIterator old(*this);
	 
	 if(++d_iz > d_ez){
	    d_iz = d_ez;
	    if(++d_iy > d_ey){
	       d_iy = d_ey;
	       ++d_ix;
	    }
	 }
	 return old;
      }
      
      //////////
      // Insert Documentation Here:
      inline bool done() const {
	 return d_ix < d_ex;
      }

      IntVector operator*() {
	 return IntVector(d_ix, d_iy, d_iz);
      }
      IntVector index() const {
	 return IntVector(d_ix, d_iy, d_iz);
      }
      inline CellIterator(int ix, int iy, int iz, int ex, int ey, int ez)
	 : d_ix(ix), d_iy(iy), d_iz(iz), d_ex(ex), d_ey(ey), d_ez(ez) {
      }

   private:
      CellIterator();
      inline CellIterator(const CellIterator& copy)
	 : d_ix(copy.d_ix), d_iy(copy.d_iy), d_iz(copy.d_iz),
	   d_ex(copy.d_ex), d_ey(copy.d_ey), d_ez(copy.d_ez) {
      }
      CellIterator& operator=(const CellIterator& copy);
      inline CellIterator(const Region* region, int ix, int iy, int iz)
	 : d_ix(ix), d_iy(iy), d_iz(iz), d_ex(region->getNCells().x()), d_ey(region->getNCells().y()), d_ez(region->getNCells().z()) {
      }
      
      //////////
      // Insert Documentation Here:
      int     d_ix, d_iy, d_iz;
      int     d_ex, d_ey, d_ez;
   };
   
} // end namespace Uintah

//
// $Log$
// Revision 1.1  2000/04/27 23:19:10  sparker
// Object to iterator over cells
//
//

#endif
