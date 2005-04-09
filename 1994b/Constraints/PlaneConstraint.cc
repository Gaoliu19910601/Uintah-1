
/*
 *  PlaneConstraint.h
 *
 *  Written by:
 *   James Purciful
 *   Department of Computer Science
 *   University of Utah
 *   Aug. 1994
 *
 *  Copyright (C) 1994 SCI Group
 */


#include <Constraints/PlaneConstraint.h>
#include <Geometry/Vector.h>
#include <Classlib/Debug.h>

static DebugSwitch pc_debug("BaseConstraint", "Plane");

PlaneConstraint::PlaneConstraint( const clString& name,
				  const Index numSchemes,
				  Variable* p,
				  Variable* norm, Variable* offsetInX )
:BaseConstraint(name, numSchemes, 3)
{
   vars[0] = p;
   vars[1] = norm;
   vars[2] = offsetInX;
   whichMethod = 0;

   // Tell the variables about ourself.
   Register();
};

PlaneConstraint::~PlaneConstraint()
{
}


void
PlaneConstraint::Satisfy( const Index index, const Scheme scheme )
{
   Variable& v0 = *vars[0];
   Variable& v1 = *vars[1];
   Variable& v2 = *vars[2];

   if (pc_debug) {
      ChooseChange(index, scheme);
      print();
   }
   
   switch (ChooseChange(index, scheme)) {
   case 0:
      v0.Assign(v0.Get() - (v1.Get().vector()
			    * ((v2.Get().x() + Dot(v1.Get(), v0.Get()))
			       / Dot(v1.Get(), v1.Get()))),
		scheme);
      break;
   case 1:
      NOT_FINISHED("Plane Constaint:  Assign norm");
      break;
   case 2:
      NOT_FINISHED("Plane Constaint:  Assign offset");
      break;
   default:
      cerr << "Unknown variable in Plane Constraint!" << endl;
      break;
   }
}

