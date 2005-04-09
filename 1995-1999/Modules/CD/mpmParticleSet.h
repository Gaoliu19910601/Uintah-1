
/*
 *  mpmParticleSet.h:
 *
 *  Written by:
 *   Steven G. Parker
 *   Department of Computer Science
 *   University of Utah
 *   May 1996
 *
 *  Copyright (C) 1998 SCI Group
 */

#ifndef SCI_Datatypes_mpmParticleSet_h
#define SCI_Datatypes_mpmParticleSet_h 1

#include <Datatypes/ParticleSet.h>
#include <Classlib/LockingHandle.h>

struct mpmTimeStep {
    double time;
    Array1<Vector> positions;
    Array1<double> scalars;
};

class mpmParticleSet : public ParticleSet {
    Array1<mpmTimeStep*> timesteps;
public:
    mpmParticleSet();
    virtual ~mpmParticleSet();

    virtual ParticleSet* clone() const;

    virtual int find_scalar(const clString& name);
    virtual void list_scalars(Array1<clString>& names);
    virtual int find_vector(const clString& name);
    virtual void list_vectors(Array1<clString>& names);

    virtual void list_natural_times(Array1<double>& times);

    virtual void get(int timestep, int vectorid, Array1<Vector>& value,
		     int start=-1, int end=-1);
    virtual void get(int timestep, int scalarid, Array1<double>& value,
		     int start=-1, int end=-1);
    virtual void interpolate(double time, int vectorid, Vector& value,
			     int start=-1, int end=-1);
    virtual void interpolate(double time, int scalarid, double& value,
			     int start=-1, int end=-1);

    virtual int position_vector();

    void add(mpmTimeStep* ts);

    // Persistent representation...
    virtual void io(Piostream&);
    static PersistentTypeID type_id;

    // testing
    void print();
};

#endif
