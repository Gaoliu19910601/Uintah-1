
/*
 *  VectorFieldUG.h: Vector Fields defined on an unstructured grid
 *
 *  Written by:
 *   Steven G. Parker
 *   Department of Computer Science
 *   University of Utah
 *   July 1994
 *
 *  Copyright (C) 1994 SCI Group
 */

#ifndef SCI_project_VectorFieldUG_h
#define SCI_project_VectorFieldUG_h 1

#include <Datatypes/VectorField.h>

#include <Classlib/Array1.h>
#include <Datatypes/Mesh.h>

class VectorFieldUG : public VectorField {
public:
    MeshHandle mesh;
    Array1<Vector> data;

    VectorFieldUG();
    VectorFieldUG(const MeshHandle&);
    virtual ~VectorFieldUG();
    virtual VectorField* clone();

    virtual void compute_bounds();
    virtual int interpolate(const Point&, Vector&);

    virtual void io(Piostream&);
    static PersistentTypeID type_id;
};

#endif
