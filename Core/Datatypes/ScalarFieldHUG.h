
/*
 *  ScalarFieldHUG.h: Scalar Fields defined on a hexahedral grid
 *
 *  Written by:
 *   Peter A. Jensen
 *   Sourced from ScalarFieldHUG.h
 *   Department of Computer Science
 *   University of Utah
 *   April 1997
 *
 *  Copyright (C) 1997 SCI Group
 */

/*******************************************************************************
* Version control
*******************************************************************************/

#ifndef SCI_project_ScalarFieldHUG_h
#define SCI_project_ScalarFieldHUG_h 1


/*******************************************************************************
* Includes
*******************************************************************************/

#include <Core/Datatypes/ScalarField.h>
#include <Core/Datatypes/HexMesh.h>
#include <Core/Containers/Array1.h>


/*******************************************************************************
* Hexahedral unstructured grid class
*******************************************************************************/

namespace SCIRun {

class SCICORESHARE ScalarFieldHUG : public ScalarField 
{
  public:
  
    HexMesh * mesh;
    Array1<double> data;
  
    ScalarFieldHUG();
    ScalarFieldHUG(HexMesh * m);
    virtual ~ScalarFieldHUG();
    virtual ScalarField* clone();

    virtual void compute_bounds();
    virtual void compute_minmax();
    
    virtual Vector gradient(const Point&);
    virtual int interpolate(const Point&, double&, double epsilon1=1.e-6, double epsilon2=1.e-6);
    virtual int interpolate(const Point&, double&, int& ix, double epsilon1=1.e-6, double epsilon2=1.e-6, int exhaustive=0);
    virtual void get_boundary_lines(Array1<Point>& lines);

    virtual void io(Piostream&);
    
    static PersistentTypeID type_id;
};

} // End namespace SCIRun


#endif
