/*
  The contents of this file are subject to the University of Utah Public
  License (the "License"); you may not use this file except in compliance
  with the License.
  
  Software distributed under the License is distributed on an "AS IS"
  basis, WITHOUT WARRANTY OF ANY KIND, either express or implied. See the
  License for the specific language governing rights and limitations under
  the License.
  
  The Original Source Code is SCIRun, released March 12, 2001.
  
  The Original Source Code was developed by the University of Utah.
  Portions created by UNIVERSITY are Copyright (C) 2001, 1994 
  University of Utah. All Rights Reserved.
*/

/*
 *  NrrdFieldConverter.cc:
 *
 *  Written by:
 *   Allen Sanderson
 *   School of Computing
 *   University of Utah
 *   July 2003
 *
 *  Copyright (C) 2002 SCI Group
 */

#include <Dataflow/Network/Module.h>
#include <Core/Malloc/Allocator.h>

#include <Dataflow/share/share.h>

#include <Dataflow/Ports/FieldPort.h>
#include <Teem/Dataflow/Ports/NrrdPort.h>

#include <Packages/Fusion/Dataflow/Modules/Fields/VULCANConverter.h>

#include <sci_defs.h>

namespace Fusion {

using namespace SCIRun;
using namespace SCITeem;

class PSECORESHARE VULCANConverter : public Module {
public:
  VULCANConverter(GuiContext*);

  virtual ~VULCANConverter();

  virtual void execute();

  virtual void tcl_command(GuiArgs&, void*);

private:
  enum { NONE = 0, MESH = 1, SCALAR = 2, REALSPACE = 4, CONNECTION = 8 };
  enum { ZR = 0, PHI = 1, LIST = 2 };

  GuiString datasetsStr_;
  GuiInt nModes_;
  vector< GuiInt* > gModes_;

  int nmodes_;
  vector< int > modes_;

  unsigned int conversion_;

  GuiInt allowUnrolling_;
  GuiInt unRolling_;
  int unrolling_;

  vector< int > mesh_;
  vector< int > data_;

  vector< int > nGenerations_;

  NrrdDataHandle nHandle_;

  bool error_;
};


DECLARE_MAKER(VULCANConverter)
VULCANConverter::VULCANConverter(GuiContext* context)
  : Module("VULCANConverter", context, Source, "Fields", "Fusion"),
    datasetsStr_(context->subVar("datasets")),
    nModes_(context->subVar("nmodes")),
    nmodes_(0),
    conversion_(NONE),
    allowUnrolling_(context->subVar("allowUnrolling")),
    unRolling_(context->subVar("unrolling")),
    unrolling_(0),
    error_(false)
{
}

VULCANConverter::~VULCANConverter(){
}

void
VULCANConverter::execute(){

  vector< NrrdDataHandle > nHandles;
  NrrdDataHandle nHandle;

  // Assume a range of ports even though four are needed.
  port_range_type range = get_iports("Input Nrrd");

  if (range.first == range.second)
    return;

  port_map_type::iterator pi = range.first;

  while (pi != range.second) {
    NrrdIPort *inrrd_port = (NrrdIPort*) get_iport(pi->second); 

    ++pi;

    if (!inrrd_port) {
      error( "Unable to initialize "+name+"'s iport " );
      return;
    }

    // Save the field handles.
    if (inrrd_port->get(nHandle) && nHandle.get_rep()) {
      unsigned int tuples = nHandle->get_tuple_axis_size();

      // Store only single nrrds
      if( tuples == 0 ) {
	error("Zero tuples???");
	return;
      } else if( tuples == 1 ) {
	nHandles.push_back( nHandle );
      } else {

	// Multiple nrrds
	vector< string > dataset;
	nHandle->get_tuple_indecies(dataset);
	
	const unsigned int nrrdDim = nHandle->nrrd->dim;
	
	int *min = scinew int[nrrdDim];
	int *max = scinew int[nrrdDim];

	// Keep the same dims except for the tuple axis.
	for( int j=1; j<nHandle->nrrd->dim; j++) {
	  min[j] = 0;
	  max[j] = nHandle->nrrd->axis[j].size-1;
	}

	// Separate via the tupple axis.
	for( unsigned int i=0; i<tuples; i++ ) {

	  Nrrd *nout = nrrdNew();

	  // Translate the tuple index into the real offsets for a tuple axis.
	  int tmin, tmax;
	  if (! nHandle->get_tuple_index_info( i, i, tmin, tmax)) {
	    error("Tuple index out of range");
	    return;
	  }
	  
	  min[0] = tmin;
	  max[0] = tmax;

	  // Crop the tupple axis.
	  if (nrrdCrop(nout, nHandle->nrrd, min, max)) {

	    char *err = biffGetDone(NRRD);
	    error(string("Trouble resampling: ") + err);
	    msgStream_ << "input Nrrd: nHandle->nrrd->dim="<<nHandle->nrrd->dim<<"\n";
	    free(err);
	  }

	  // Form the new nrrd and store.
	  NrrdData *nrrd = scinew NrrdData;
	  nrrd->nrrd = nout;
	  nout->axis[0].label = strdup(dataset[i].c_str());

	  NrrdDataHandle handle = NrrdDataHandle(nrrd);

	  // Copy the properties.
	  *((PropertyManager *) handle.get_rep()) =
	    *((PropertyManager *) nHandle.get_rep());

	  nHandles.push_back( handle );
	}

	delete min;
	delete max;
       }
    } else if( pi != range.second ) {
      error( "No handle or representation" );
      return;
    }
  }

  string datasetsStr;

  // Get each of the dataset names for the GUI.
  for( unsigned int ic=0; ic<nHandles.size(); ic++ ) {

    nHandle = nHandles[ic];

    // Get the tuple axis name - there is only one.
    vector< string > dataset;
    nHandle->get_tuple_indecies(dataset);

    // Save the name of the dataset.
    if( nHandles.size() == 1 )
      datasetsStr.append( dataset[0] );
    else
      datasetsStr.append( "{" + dataset[0] + "} " );
  }      

  if( datasetsStr != datasetsStr_.get() ) {
    // Update the dataset names and dims in the GUI.
    ostringstream str;
    str << id << " set_names " << " {" << datasetsStr << "}";
    
    gui->execute(str.str().c_str());
  }

  if( nHandles.size() != 1 && nHandles.size() != 2 && nHandles.size() != 3 &&
      nHandles.size() != 4 && nHandles.size() != 8 ){
    error( "Not enough or too many handles or representations" );
    return;
  }

  int generation = 0;

  // See if input data has been added or removed.
  if( nGenerations_.size() == 0 ||
      nGenerations_.size() != nHandles.size() )
    generation = nHandles.size();
  else {
    // See if any of the input data has changed.
    for( unsigned int ic=0; ic<nHandles.size() &&
	   ic<nGenerations_.size(); ic++ ) {
      if( nGenerations_[ic] != nHandles[ic]->generation )
	++generation;
    }
  }

  string property;

  // If data change, update the GUI the field if needed.
  if( generation ) {
    conversion_ = NONE;

    remark( "Found new data ... updating." );

    nGenerations_.resize( nHandles.size() );
    mesh_.resize(4);
    mesh_[0] = mesh_[1] = mesh_[2] = mesh_[3] = -1;

    for( unsigned int ic=0; ic<nHandles.size(); ic++ )
      nGenerations_[ic] = nHandles[ic]->generation;

    // Get each of the dataset names for the GUI.
    for( unsigned int ic=0; ic<nHandles.size(); ic++ ) {

      nHandle = nHandles[ic];

      // Get the tuple axis name - there is only one.
      vector< string > dataset;
      nHandle->get_tuple_indecies(dataset);

      if( nHandle->get_property( "Topology", property ) ) {

	// Structured mesh.
	if( property.find( "Structured" ) != string::npos ||
	    property.find( "Unstructured" ) != string::npos ) {

	  if( nHandle->get_property( "Coordinate System", property ) ) {

	    // Special Case - VULCAN data which has multiple components.
	    if( property.find("Cylindrical - VULCAN") != string::npos ) {

	      // Sort the components.
	      if( dataset[0].find( "ZR:Scalar" ) != string::npos &&
		  nHandle->nrrd->dim == 3 ) {
		conversion_ = MESH;
		mesh_[ZR] = ic;
	      } else if( dataset[0].find( "ZR:Vector" ) != string::npos &&
			 nHandle->nrrd->dim == 2 ) {
		conversion_ = MESH;
		mesh_[ZR] = ic;
	      } else if( dataset[0].find( "PHI:Scalar" ) != string::npos && 
			 nHandle->nrrd->dim == 2 ) {
		mesh_[PHI] = ic;
	      } else {
		error( dataset[0] + " is unknown VULCAN mesh data." );
		error_ = true;
		return;
	      }
	    } else {
	      error( dataset[0] + " " + property + " is an unsupported coordinate system." );
	      error_ = true;
	      return;
	    }
	  } else if( nHandle->get_property( "Cell Type", property ) ) {
	    conversion_ = CONNECTION;
	    mesh_[LIST] = ic;
	  } else {
	    error( dataset[0] + "No coordinate system or cell type found." );
	    error_ = true;
	    return;
	  }
	} else {
	  error( dataset[0] + " " + property + " is an unsupported topology." );
	  error_ = true;
	  return;
	}
      } else if( nHandle->get_property( "DataSpace", property ) ) {

	if( property.find( "REALSPACE" ) != string::npos ) {

	  if( data_.size() == 0 ) {
	    data_.resize(1);
	    data_[0] = -1;
	  }

	  if( dataset[0].find( "ZR:Scalar" ) != string::npos && 
	      nHandle->nrrd->dim == 3 ) {
	    conversion_ = REALSPACE;
	    data_[ZR] = ic; 
	  } else if( dataset[0].find( "ZR:Vector" ) != string::npos && 
	      nHandle->nrrd->dim == 2 ) {
	    conversion_ = REALSPACE;
	    data_[ZR] = ic; 
	  } else if( dataset[0].find( ":Scalar" ) != string::npos && 
		     nHandle->nrrd->dim == 2 ) {
	    conversion_ = SCALAR;
	    data_[0] = ic;
	  } else {
	    error( dataset[0] + " is unknown VULCAN node data." );
	    error_ = true;
	    return;
	  }
	} else {	
	  error( dataset[0] + " " + property + " Unsupported Data Space." );
	  error_ = true;
	  return;
	}
      } else {
	error( dataset[0] + " No DataSpace property." );
	error_ = true;
	return;
      }
    }
  }

  unsigned int i = 0;
  while( i<data_.size() && data_[i] != -1 )
    i++;
  
  if( conversion_ & MESH ) {
    if( mesh_[PHI] == -1 || mesh_[ZR] == -1 ) {
      error( "Not enough data for the mesh conversion." );
      error_ = true;
      return;
    }
  } else if( conversion_ & CONNECTION ) {
    if( mesh_[PHI] == -1 || mesh_[ZR] == -1 || mesh_[LIST] == -1 ) {
      error( "Not enough data for the connection conversion." );
      error_ = true;
      return;
    }
  } else if ( conversion_ & REALSPACE ) {
    if( mesh_[PHI] == -1 || data_[ZR] == -1 ) {

      error( "Not enough data for the realspace conversion." );

      error_ = true;
      return;
    }
  } else if ( conversion_ & SCALAR ) {
    if( mesh_[PHI] == -1 || mesh_[ZR] == -1 || mesh_[LIST] == -1 || data_[ZR] == -1 ) {

      error( "Not enough data for the scalar conversion." );

      error_ = true;
      return;
    }
  }

  if( (int) (conversion_ & MESH) != allowUnrolling_.get() ) {
    ostringstream str;
    str << id << " set_unrolling " << (conversion_ & MESH);
    
    gui->execute(str.str().c_str());

    if( conversion_ & MESH ) {
      warning( "Select the mesh rolling for the calculation" );
      error_ = true; // Not really an error but it so it will execute.
      return;
    }
  }

  nmodes_ = 0;

  int nmodes = gModes_.size();

  // Remove the GUI entries that are not needed.
  for( int ic=nmodes-1; ic>nmodes_; ic-- ) {
    delete( gModes_[ic] );
    gModes_.pop_back();
    modes_.pop_back();
  }

  if( nmodes_ > 0 ) {
    // Add new GUI entries that are needed.
    for( int ic=nmodes; ic<=nmodes_; ic++ ) {
      char idx[24];
      
      sprintf( idx, "mode-%d", ic );
      gModes_.push_back(new GuiInt(ctx->subVar(idx)) );
      
      modes_.push_back(0);
    }
  }

  if( nModes_.get() != nmodes_ ) {

    // Update the modes in the GUI
    ostringstream str;
    str << id << " set_modes " << nmodes_ << " 1";

    gui->execute(str.str().c_str());
    
  }


  bool updateMode = false;

  if( nmodes_ > 0 ) {
    bool haveMode = false;

    for( int ic=0; ic<=nmodes_; ic++ ) {
      gModes_[ic]->reset();
      if( modes_[ic] != gModes_[ic]->get() ) {
	modes_[ic] = gModes_[ic]->get();
	updateMode = true;
      }

      if( !haveMode ) haveMode = (modes_[ic] ? true : false );
    }

    if( !haveMode ) {
      warning( "Select the mode for the calculation" );
      error_ = true; // Not really an error but it so it will execute.
      return;
    }
  }

  bool updateRoll = false;

  if( unrolling_ != unRolling_.get() ) {
    unrolling_ = unRolling_.get();
    updateRoll = true;
  }

  // If no data or data change, recreate the field.
  if( error_ ||
      updateMode ||
      updateRoll ||
      !nHandle_.get_rep() ||
      generation ) {
    
    error_ = false;

    string convertStr;
    unsigned int ntype;

    if( conversion_ & MESH ) {
      ntype = nHandles[mesh_[PHI]]->nrrd->type;

      nHandles[mesh_[PHI]]->get_property( "Coordinate System", property );

      if( property.find("Cylindrical - VULCAN") != string::npos ) {
	modes_.resize(1);
	modes_[0] = unrolling_;
      }

      convertStr = "Mesh";

    } else if( conversion_ & CONNECTION ) {
      ntype = nHandles[mesh_[LIST]]->nrrd->type;

      nHandles[mesh_[PHI]]->get_property( "Coordinate System", property );

      if( property.find("Cylindrical - VULCAN") != string::npos ) {
	modes_.resize(1);
	modes_[0] = unrolling_;
      }

      convertStr = "Connection";

    } else if( conversion_ & SCALAR ) {
      ntype = nHandles[data_[0]]->nrrd->type;

      convertStr = "Scalar";

     } else if( conversion_ & REALSPACE ) {
      ntype = nHandles[mesh_[PHI]]->nrrd->type;

      convertStr = "RealSpace";
    }

    if( conversion_ ) {
      remark( "Converting the " + convertStr );
    
      CompileInfoHandle ci_mesh =
	VULCANConverterAlgo::get_compile_info( convertStr, ntype );
    
      Handle<VULCANConverterAlgo> algo_mesh;
    

      if( !module_dynamic_compile(ci_mesh, algo_mesh) ) {
	error( "No Module" );
	return;
      }

      nHandle_ = algo_mesh->execute( nHandles, mesh_, data_, modes_ );
    } else {
      error( "Nothing to convert." );
      error_ = true;
      return;
    }

    if( conversion_ & MESH )
      modes_.clear();
  }
  
  // Get a handle to the output field port.
  if( nHandle_.get_rep() ) {
    NrrdOPort *ofield_port = (NrrdOPort *) get_oport("Output Nrrd");
    
    if (!ofield_port) {
      error("Unable to initialize "+name+"'s oport\n");
      return;
    }

    // Send the data downstream
    ofield_port->send( nHandle_ );
  }
}

void
VULCANConverter::tcl_command(GuiArgs& args, void* userdata)
{
  Module::tcl_command(args, userdata);
}


void get_nrrd_compile_type( const unsigned int type,
			    string & typeStr,
			    string & typeName );

CompileInfoHandle
VULCANConverterAlgo::get_compile_info( const string converter,
				       const unsigned int ntype )
{
  // use cc_to_h if this is in the .cc file, otherwise just __FILE__
  static const string include_path(TypeDescription::cc_to_h(__FILE__));
  const string base_class_name( "VULCANConverterAlgo");
  const string template_class_name( "VULCAN" + converter + "ConverterAlgoT");

  string nTypeStr, nTypeName;

  get_nrrd_compile_type( ntype, nTypeStr, nTypeName );

  CompileInfo *rval = 
    scinew CompileInfo(template_class_name + "." +
		       nTypeName + ".",
                       base_class_name, 
                       template_class_name,
                       nTypeStr );

  // Add in the include path to compile this obj
  rval->add_include(include_path);
  rval->add_namespace("Fusion");
  return rval;
}

}
