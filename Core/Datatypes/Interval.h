
/*
 *  Interval.h: Specification of a range [x..y]
 *
 *  Written by:
 *   Steven G. Parker
 *   Department of Computer Science
 *   University of Utah
 *   May 1996
 *
 *  Copyright (C) 1996 SCI Group
 */

#ifndef SCI_Datatypes_Interval_h
#define SCI_Datatypes_Interval_h 1

#include <Core/Datatypes/Datatype.h>
#include <Core/Containers/LockingHandle.h>

namespace SCIRun {


class Interval;
typedef LockingHandle<Interval> IntervalHandle;

class SCICORESHARE Interval : public Datatype {
public:
    double low, high;
    Interval(double low, double high);
    virtual ~Interval();
    Interval(const Interval&);
    virtual Interval* clone() const;

    // Persistent representation...
    virtual void io(Piostream&);
    static PersistentTypeID type_id;
};

} // End namespace SCIRun


#endif
