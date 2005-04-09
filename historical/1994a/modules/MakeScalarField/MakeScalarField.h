
/*
 *  MakeScalarField.h: Visualization module
 *
 *  Written by:
 *   Steven G. Parker
 *   Department of Computer Science
 *   University of Utah
 *   March 1994
 *
 *  Copyright (C) 1994 SCI Group
 */

#ifndef SCI_project_module_MakeScalarField_h
#define SCI_project_module_MakeScalarField_h

#include <UserModule.h>

class MakeScalarField : public UserModule {
public:
    MakeScalarField();
    MakeScalarField(const MakeScalarField&, int deep);
    virtual ~MakeScalarField();
    virtual Module* clone(int deep);
    virtual void execute();
};

#endif
