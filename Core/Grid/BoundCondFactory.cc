#include <Core/Malloc/Allocator.h>
#include <Packages/Uintah/Core/Grid/BoundCondFactory.h>
#include <Packages/Uintah/Core/Grid/NoneBoundCond.h>
#include <Packages/Uintah/Core/Grid/SymmetryBoundCond.h>
#include <Packages/Uintah/Core/Grid/NeighBoundCond.h>
#include <Packages/Uintah/Core/Grid/VelocityBoundCond.h>
#include <Packages/Uintah/Core/Grid/TemperatureBoundCond.h>
#include <Packages/Uintah/Core/Grid/PressureBoundCond.h>
#include <Packages/Uintah/Core/Grid/DensityBoundCond.h>
#include <Packages/Uintah/Core/Grid/MassFracBoundCond.h>
#include <Packages/Uintah/Core/ProblemSpec/ProblemSpec.h>
#include <Packages/Uintah/Core/ProblemSpec/ProblemSpecP.h>
#include <Packages/Uintah/Core/Exceptions/ProblemSetupException.h>
#include <iostream>
#include <string>
#include <map>
#include <stdlib.h>

using namespace std;
using namespace Uintah;

void BoundCondFactory::create(ProblemSpecP& child,
			      BoundCondBase* &bc, int& mat_id)

{
  map<string,string> bc_attr;
  child->getAttributes(bc_attr);
  
  bool massFractionBC = false;   // check for massFraction BC
  string::size_type pos1 = bc_attr["label"].find ("massFraction");
  string::size_type pos2 = bc_attr["label"].find ("scalar");
  if ( pos1 != std::string::npos || pos2 != std::string::npos ){
    massFractionBC = true;
  }
  
  // Check to see if "id" is defined
  if (bc_attr.find("id") == bc_attr.end()) 
    SCI_THROW(ProblemSetupException("id is not specified in the BCType tag"));
  
  if (bc_attr["id"] != "all")
    mat_id = atoi(bc_attr["id"].c_str());
  else
    mat_id = -1;  // Old setting was 0.

  if (bc_attr["var"] == "None") {
    bc = scinew NoneBoundCond(child);
  }
  
  else if (bc_attr["label"] == "Symmetric") {
    bc = scinew SymmetryBoundCond(child);
  }
  
  else if (bc_attr["var"] ==  "Neighbor") {
    bc = scinew NeighBoundCond(child);
  }
  
  else if (bc_attr["label"] == "Velocity" && 
	   (bc_attr["var"]   == "Neumann"  ||
	    bc_attr["var"]   == "LODI" ||
	    bc_attr["var"]   == "Dirichlet") ) {
    bc = scinew VelocityBoundCond(child,bc_attr["var"]);
  }
  
  else if (bc_attr["label"] == "Temperature" &&
	   (bc_attr["var"]   == "Neumann"  ||
	    bc_attr["var"]   == "LODI" ||
	    bc_attr["var"]   == "Dirichlet") ) {
    bc = scinew TemperatureBoundCond(child,bc_attr["var"]);
  }
  
  else if (bc_attr["label"] == "Pressure" &&
	   (bc_attr["var"]   == "Neumann"  ||
	    bc_attr["var"]   == "LODI" ||
	    bc_attr["var"]   == "Dirichlet") ) {
    bc = scinew PressureBoundCond(child,bc_attr["var"]);
  }
  
  else if (bc_attr["label"] == "Density" &&
	   (bc_attr["var"]   == "Neumann"  ||
	    bc_attr["var"]   == "LODI" ||
	    bc_attr["var"]   == "Dirichlet") ) {
    bc = scinew DensityBoundCond(child,bc_attr["var"]);
  } 
  else if (massFractionBC &&
	   (bc_attr["var"]   == "Neumann"  ||
	    bc_attr["var"]   == "Dirichlet") ) {  
    bc = scinew MassFractionBoundCond(child,bc_attr["var"],bc_attr["label"]);
  }
  else {
    cerr << "Unknown Boundary Condition Type " << "(" << bc_attr["var"] 
	 << ")  " << bc_attr["label"]<<endl;
    exit(1);
  }
}

