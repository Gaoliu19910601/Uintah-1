/*
 * The MIT License
 *
 * Copyright (c) 1997-2014 The University of Utah
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

#include <CCA/Components/ICE_sm/EOS/EquationOfState.h>
#include <CCA/Components/ICE_sm/EOS/EquationOfStateFactory.h>

#include <CCA/Components/ICE_sm/ICEMaterial.h>
#include <CCA/Components/ICE_sm/SpecificHeatModel/SpecificHeatFactory.h>

#include <CCA/Ports/DataWarehouse.h>
#include <Core/Exceptions/ParameterNotFound.h>
#include <Core/Geometry/IntVector.h>
#include <Core/GeometryPiece/FileGeometryPiece.h>
#include <Core/GeometryPiece/GeometryObject.h>
#include <Core/GeometryPiece/GeometryPieceFactory.h>
#include <Core/GeometryPiece/UnionGeometryPiece.h>

#include <Core/Grid/Patch.h>
#include <Core/Grid/Variables/CellIterator.h>
#include <Core/ProblemSpec/ProblemSpec.h>

using namespace std;
using namespace Uintah;

// Constructor
oneICEMaterial::oneICEMaterial(ProblemSpecP& ps,
                               SimulationStateP& sharedState): Material(ps)
{
  //__________________________________
  //  Create the different Models for this material
  d_eos = EquationOfStateFactory::create(ps);
  if(!d_eos) {
    throw ParameterNotFound("ICE: No EOS specified", __FILE__, __LINE__);
  }

  ProblemSpecP cvModel_ps = ps->findBlock("SpecificHeatModel");
  d_cvModel = 0;
  if(cvModel_ps != 0) {
    proc0cout << "Creating Specific heat model." << endl;
    d_cvModel = SpecificHeatFactory::create(ps);
  }
  
  //__________________________________
  // Thermodynamic Transport Properties
  ps->require("thermal_conductivity",d_thermalConductivity);
  ps->require("specific_heat",       d_specificHeat);
  ps->require("dynamic_viscosity",   d_viscosity);
  ps->require("gamma",               d_gamma);

  //__________________________________
  // Loop through all of the pieces in this geometry object
  int piece_num = 0;

  list<GeometryObject::DataItem> geom_obj_data;
  geom_obj_data.push_back(GeometryObject::DataItem("res",        GeometryObject::IntVector));
  geom_obj_data.push_back(GeometryObject::DataItem("temperature",GeometryObject::Double));
  geom_obj_data.push_back(GeometryObject::DataItem("pressure",   GeometryObject::Double));
  geom_obj_data.push_back(GeometryObject::DataItem("density",    GeometryObject::Double)); 
  geom_obj_data.push_back(GeometryObject::DataItem("velocity",   GeometryObject::Vector));

  for (ProblemSpecP geom_obj_ps = ps->findBlock("geom_object");geom_obj_ps != 0;
       geom_obj_ps = geom_obj_ps->findNextBlock("geom_object") ) {

    vector<GeometryPieceP> pieces;
    GeometryPieceFactory::create(geom_obj_ps, pieces);

    GeometryPieceP mainpiece;
    if(pieces.size() == 0){
      throw ParameterNotFound("No piece specified in geom_object", __FILE__, __LINE__);
    } else if(pieces.size() > 1){
      mainpiece = scinew UnionGeometryPiece(pieces);
    } else {
      mainpiece = pieces[0];
    }

    piece_num++;
    d_geom_objs.push_back(scinew GeometryObject(mainpiece, geom_obj_ps, geom_obj_data));
  }
}
//__________________________________
// Destructor
oneICEMaterial::~oneICEMaterial()
{
  delete d_eos;
  
  if( d_cvModel ){
    delete d_cvModel;
  }
  
  for (int i = 0; i< (int)d_geom_objs.size(); i++) {
    delete d_geom_objs[i];
  }
}
//______________________________________________________________________
//
ProblemSpecP oneICEMaterial::outputProblemSpec(ProblemSpecP& ps)
{
  ProblemSpecP ice_ps = Material::outputProblemSpec(ps);

  d_eos->outputProblemSpec(ice_ps);
  ice_ps->appendElement("thermal_conductivity",d_thermalConductivity);
  ice_ps->appendElement("specific_heat",       d_specificHeat);
  ice_ps->appendElement("dynamic_viscosity",   d_viscosity);
  ice_ps->appendElement("gamma",               d_gamma);
    
  if(d_cvModel != 0){
    d_cvModel->outputProblemSpec(ice_ps);
  }
    
  for (vector<GeometryObject*>::const_iterator it = d_geom_objs.begin();
       it != d_geom_objs.end(); it++) {
    (*it)->outputProblemSpec(ice_ps);
  }

  return ice_ps;
}
//__________________________________
//
EquationOfState * oneICEMaterial::getEOS() const
{
  return d_eos;
}

SpecificHeat *oneICEMaterial::getSpecificHeatModel() const
{
  return d_cvModel;
}

double oneICEMaterial::getGamma() const
{
  return d_gamma;
}


double oneICEMaterial::getViscosity() const
{
  return d_viscosity;
}


double oneICEMaterial::getSpecificHeat() const
{
  return d_specificHeat;
}

double oneICEMaterial::getThermalConductivity() const
{
  return d_thermalConductivity;
}

double oneICEMaterial::getInitialDensity() const
{
  return d_geom_objs[0]->getInitialData_double("density");
}

/* --------------------------------------------------------------------- 
 Function~  oneICEMaterial::initializeCells--
 Purpose~ Initialize primitive variables
_____________________________________________________________________*/
void oneICEMaterial::initializeCells(CCVariable<double>& rho_CC,
                                     CCVariable<double>& temp_CC,
                                     CCVariable<Vector>& vel_CC,
                                     const Patch* patch,
                                     DataWarehouse* new_dw)
{
   
  // Zero the 
  vel_CC.initialize(Vector(0.,0.,0.));
  rho_CC.initialize(0.);
  temp_CC.initialize(0.);

  // Loop over geometry objects
  for(int obj=0; obj<(int)d_geom_objs.size(); obj++){
    GeometryPieceP piece = d_geom_objs[obj]->getPiece();

    IntVector ppc   = d_geom_objs[obj]->getInitialData_IntVector("res");
    Vector dxpp     = patch->dCell()/ppc;
    Vector dcorner  = dxpp*0.5;

    for(CellIterator iter = patch->getExtraCellIterator();!iter.done();iter++){
      IntVector c = *iter;
      Point lower = patch->nodePosition(c) + dcorner;
      int count = 0;

      for(int ix=0;ix < ppc.x(); ix++){
        for(int iy=0;iy < ppc.y(); iy++){
          for(int iz=0;iz < ppc.z(); iz++){

            IntVector idx(ix, iy, iz);
            Point p = lower + dxpp*idx;
            if(piece->inside(p))
              count++;
          }
        }
      }
      //__________________________________
      // For single materials with more than one object 
      if ( count > 0 ) {
        vel_CC[c]     = d_geom_objs[obj]->getInitialData_Vector("velocity");
        rho_CC[c]     = d_geom_objs[obj]->getInitialData_double("density");
        temp_CC[c]    = d_geom_objs[obj]->getInitialData_double("temperature");
      }
    }  // Loop over domain
  }  // Loop over geom_objects
}
