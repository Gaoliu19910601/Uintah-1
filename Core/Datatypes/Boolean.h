
/*
 *  sciBoolean.h: Specification of a range [x..y]
 *
 *  Written by:
 *   Steven G. Parker
 *   Department of Computer Science
 *   University of Utah
 *   May 1996
 *
 *  Copyright (C) 1996 SCI Group
 */

#ifndef SCI_Datatypes_sciBoolean_h
#define SCI_Datatypes_sciBoolean_h 1

#include <Core/Datatypes/Datatype.h>
#include <Core/Containers/LockingHandle.h>

namespace SCIRun {


class sciBoolean;
typedef LockingHandle<sciBoolean> sciBooleanHandle;

class SCICORESHARE sciBoolean : public Datatype {
public:
    int value;
    sciBoolean(int value);
    virtual ~sciBoolean();
    sciBoolean(const sciBoolean&);
    virtual sciBoolean* clone() const;

    // Persistent representation...
    virtual void io(Piostream&);
    static PersistentTypeID type_id;
};

} // End namespace SCIRun


#endif
