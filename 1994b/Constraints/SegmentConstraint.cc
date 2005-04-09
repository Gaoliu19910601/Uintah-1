
/*
 *  SegmentConstraint.h
 *
 *  Written by:
 *   James Purciful
 *   Department of Computer Science
 *   University of Utah
 *   Aug. 1994
 *
 *  Copyright (C) 1994 SCI Group
 */


#include <Constraints/SegmentConstraint.h>
#include <Geometry/Vector.h>
#include <Classlib/Debug.h>

static DebugSwitch sc_debug("BaseConstraint", "Segment");

SegmentConstraint::SegmentConstraint( const clString& name,
				      const Index numSchemes,
				      Variable* segment_p1, Variable* segment_p2,
				      Variable* p )
:BaseConstraint(name, numSchemes, 3)
{
   vars[0] = segment_p1;
   vars[1] = segment_p2;
   vars[2] = p;
   whichMethod = 0;

   // Tell the variables about ourself.
   Register();
};

SegmentConstraint::~SegmentConstraint()
{
}


void
SegmentConstraint::Satisfy( const Index index, const Scheme scheme )
{
   Variable& v0 = *vars[0];
   Variable& v1 = *vars[1];
   Variable& v2 = *vars[2];
   Vector norm;
   double t;
   Point p;

   if (sc_debug) {
      ChooseChange(index, scheme);
      print();
   }
   
   switch (ChooseChange(index, scheme)) {
   case 0:
      NOT_FINISHED("Segment Constraint:  segment_p1");
      break;
   case 1:
      NOT_FINISHED("Segment Constraint:  segment_p2");
      break;
   case 2:
      norm = (v1.Get() - v0.Get());
      t = -((Dot(v0.Get(), norm) - Dot(v2.Get(), norm))
	    / Dot(v1.Get(), norm));
      p = v0.Get() + (v1.Get().vector() * t);
      // Check if new point is outside segment.
      t = ((v0.Get().vector() - p.vector()).length()
	   / ((v0.Get().vector() + v1.Get().vector())
	      - p.vector()).length());
      if (t < 0.0)
	 p = v0.Get();
      else if (t > 1.0)
	 p = v1.Get();
      v2.Assign(p, scheme);
      break;
   default:
      cerr << "Unknown variable in Segment Constraint!" << endl;
      break;
   }
}

