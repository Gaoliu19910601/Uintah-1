
#ifndef UINTAH_GRID_BOX_H
#define UINTAH_GRID_BOX_H

#include <Core/Geometry/Point.h>
#include <sgi_stl_warnings_off.h>
#include <deque>
#include   <iosfwd>
#include <sgi_stl_warnings_on.h>

#include <Packages/Uintah/Core/Grid/share.h>
namespace Uintah {

using std::deque;
using namespace SCIRun;

/**************************************

  CLASS
        Box
   
  GENERAL INFORMATION

        Box.h

	Steven G. Parker
	Department of Computer Science
	University of Utah

	Center for the Simulation of Accidental Fires and Explosions (C-SAFE)
	
	Copyright (C) 2000 SCI Group

  KEYWORDS
        Box

  DESCRIPTION
        Long description...
  
  WARNING
  
****************************************/

   class Box {
   public:
      inline Box() {}
      inline ~Box() {}

      inline Box(const Box& copy)
	 : d_lower(copy.d_lower), d_upper(copy.d_upper)
      {
      }

      inline Box(const Point& lower, const Point& upper)
	 : d_lower(lower), d_upper(upper)
      {
      }

      inline Box& operator=(const Box& copy)
      {
	 d_lower = copy.d_lower;
	 d_upper = copy.d_upper;
	 return *this;
      }

      SCISHARE bool overlaps(const Box&, double epsilon=1.e-6) const;
      SCISHARE bool contains(const Point& p) const {
	 return p.x() >= d_lower.x() && p.y() >= d_lower.y()
	    && p.z() >= d_lower.z() && p.x() < d_upper.x()
	    && p.y() < d_upper.y() && p.z() < d_upper.z();
      }

      inline Point lower() const {
	 return d_lower;
      }
      inline Point upper() const {
	 return d_upper;
      }
      inline Box intersect(const Box& b) const {
	 return Box(Max(d_lower, b.d_lower),
		    Min(d_upper, b.d_upper));
      }
      inline bool degenerate() const {
	 return d_lower.x() >= d_upper.x() || d_lower.y() >= d_upper.y() || d_lower.z() >= d_upper.z();
      }

      SCISHARE static deque<Box> difference(const Box& b1, const Box& b2);
      SCISHARE static deque<Box> difference(deque<Box>& boxSet1, deque<Box>& boxSet2);

      SCISHARE friend std::ostream& operator<<(std::ostream& out, const Uintah::Box& b);

   private:
      Point d_lower;
      Point d_upper;
   };

} // End namespace Uintah

#endif
