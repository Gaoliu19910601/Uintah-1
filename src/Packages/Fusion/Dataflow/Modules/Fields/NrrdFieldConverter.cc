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

#include <Packages/Fusion/Dataflow/Modules/Fields/NrrdFieldConverter.h>

#include <sci_defs.h>

namespace Fusion {

using namespace SCIRun;
using namespace SCITeem;

class PSECORESHARE NrrdFieldConverter : public Module {
public:
  NrrdFieldConverter(GuiContext*);

  virtual ~NrrdFieldConverter();

  virtual void execute();

  virtual void tcl_command(GuiArgs&, void*);

private:
  GuiString datasetsStr_;

  GuiInt iWrap_;
  GuiInt jWrap_;
  GuiInt kWrap_;

  int iwrap_;
  int jwrap_;
  int kwrap_;

  vector< int > mesh_;
  vector< int > data_;

  vector< int > nGenerations_;

  FieldHandle  fHandle_;

  bool error_;
};


DECLARE_MAKER(NrrdFieldConverter)
NrrdFieldConverter::NrrdFieldConverter(GuiContext* context)
  : Module("NrrdFieldConverter", context, Source, "Fields", "Fusion"),
    datasetsStr_(context->subVar("datasets")),

    iWrap_(context->subVar("i-wrap")),
    jWrap_(context->subVar("j-wrap")),
    kWrap_(context->subVar("k-wrap")),

    iwrap_(0),
    jwrap_(0),
    kwrap_(0),

    error_(false)
{
}

NrrdFieldConverter::~NrrdFieldConverter(){
}

void
NrrdFieldConverter::execute(){

  vector< NrrdDataHandle > nHandles;
  NrrdDataHandle nHandle;
  MeshHandle mHandle;

  // Assume a range of ports even though only two are needed for the
  // mesh and data.
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
    if (inrrd_port->get(nHandle) && nHandle.get_rep())
      nHandles.push_back( nHandle );
    else if( pi != range.second ) {
      error( "No handle or representation" );
      return;
    }
  }

  if( nHandles.size() == 0 ){
    error( "No handle or representation" );
    return;
  }

  int generation = 0;

  // See if input data has been added or removed.
  if( nGenerations_.size() == 0 ||
      nGenerations_.size() != nHandles.size() )
    generation = nHandles.size();
  else {
    // See if any of the input data has changed.
    for( unsigned int ic=0; ic<nHandles.size() && ic<nGenerations_.size(); ic++ ) {
      if( nGenerations_[ic] != nHandles[ic]->generation )
	++generation;
    }
  }

  bool   structured = false;
  bool unstructured = false;
  string property;

  // If data change, update the GUI the field if needed.
  if( generation ) {

    nGenerations_.resize( nHandles.size() );
    mesh_.clear();
    data_.clear();

    for( unsigned int ic=0; ic++; ic<nHandles.size() )
      nGenerations_[ic] = nHandles[ic]->generation;

    string datasetsStr;

    vector< string > datasets;

    // Get each of the dataset names for the GUI.
    for( unsigned int ic=0; ic<nHandles.size(); ic++ ) {

      nHandle = nHandles[ic];

      vector< string > dataset;

      int tuples = nHandle->get_tuple_axis_size();

      if( tuples != 1 ) {
	error( "Too many tuples listed in the tuple axis." );
	error_ = true;
	return;
      }

      nHandle->get_tuple_indecies(dataset);

      // Do not allow joined Nrrds
      if( dataset.size() != 1 ) {
	error( "Too many sets listed in the tuple axis." );
	error_ = true;
	return;
      }

      // Save the name of the dataset.
      if( nHandles.size() == 1 )
	datasetsStr.append( dataset[0] );
      else
	datasetsStr.append( "{" + dataset[0] + "} " );
      
      if( nHandle->get_property( "Topology", property ) ) {

	// Structured mesh.
	if( property.find( "Structured" ) != string::npos ) {

	  if( nHandle->get_property( "Coordinate System", property ) ) {

	    // Cartesian Coordinates.
	    if( property.find("Cartesian") != string::npos ) {

	      // Check to make sure three are three coordinates.
	      // If Scalar the last dim must be three.
	      // If already Vector then nothing ...
	      if( !(dataset[0].find( ":Scalar" ) != string::npos &&
		    nHandle->nrrd->axis[ nHandle->nrrd->dim-1].size == 3) &&
		  !(dataset[0].find( ":Vector" ) != string::npos) ) {
		error( "Mesh dataset does not contain 3D points." );
		error( dataset[0] );
		error_ = true;
		return;
	      }

	      if( mesh_.size() == 0 ) {
		// Cartesian coordinates.
		mesh_.push_back( ic );
	      } else {
		error( dataset[0] + "is extra mesh data." );
		error_ = true;
		return;
	      }

	      structured = true;

	      // Special Case - NIMROD data which has multiple components.
	    } else if( property.find("Cylindrical - NIMROD") != string::npos ) {

	      if( mesh_.size() == 0 ) {
		mesh_.resize( 3 );
		mesh_[0] =  mesh_[1] =  mesh_[2] = -1;
	      }
	    
	      // Sort the three components.
	      if( dataset[0].find( "R:Scalar" ) != string::npos &&
		  nHandle->nrrd->dim == 3 ) 
		mesh_[0] = ic;
	      else if( dataset[0].find( "Z:Scalar" ) != string::npos && 
		       nHandle->nrrd->dim == 3 ) 
		mesh_[1] = ic; 
	      else if( dataset[0].find( "PHI:Scalar" ) != string::npos && 
		       nHandle->nrrd->dim == 2 )
		mesh_[2] = ic;
	      else {
		error( dataset[0] + "is unknown NIMROD mesh data." );
		error_ = true;
		return;
	      }	  

	      structured = true;

	    } else {
	      error( property + "is an unsupported coordinate system." );
	      error_ = true;
	      return;
	    }

	  } else {
	    error( "No coordinate system found." );
	    error_ = true;
	    return;
	  }

	  // Unstructured mesh.
	} else if( property.find( "Unstructured" ) != string::npos ) {

	  // For unstructured two lists are needed, points and cells.
	  if( mesh_.size() == 0 ) {
	    mesh_.resize( 2 );
	    mesh_[0] = mesh_[1] = -1;
	  }

	  // The point list has two attributes: Topology == Unstructured
	  // and Coordinate System == Cartesian
	  if( nHandle->get_property( "Coordinate System", property ) ) {

	    // Cartesian Coordinates.
	    if( property.find("Cartesian") != string::npos ) {
	      // Check to make sure the list of is rank two (Vector) or
	      // three (Scalar) and that three are three coordinates.
	      // If Scalar the last dim must be three.
	      // If already Vector then nothing ...
	      if( !(nHandle->nrrd->dim == 3 &&
		    dataset[0].find( ":Scalar" ) != string::npos &&
		    nHandle->nrrd->axis[2].size == 3) &&
		  !(nHandle->nrrd->dim == 2 &&
		    dataset[0].find( ":Vector" ) != string::npos) )
		{
		  error( "Mesh does not contain 3D points." );
		  error( dataset[0] );
		  error_ = true;
		  return;
		}

	      mesh_[0] = ic;
	    
	      unstructured = true;
	    } else {
	      error( property + "is an unsupported coordinate system." );
	      error_ = true;
	      return;
	    }
	  }
	  // The cell list has two attributes: Topology == Unstructured
	  // and Cell Type == (see check below).
	  else if( nHandle->get_property( "Cell Type", property ) ) {

	    if( !(nHandle->nrrd->dim == 2 &&
		  (dataset[0].find( ":Vector" ) != string::npos ||
		   dataset[0].find( ":Tensor" ) != string::npos)) &&

		!(nHandle->nrrd->dim == 3 &&
		  dataset[0].find( ":Scalar" ) != string::npos) ) {
	      error( "Malformed connectivity list." );
	      error_ = true;
	      return;
	    }

	    int connectivity = 0;

	    if( property.find( "Tet" ) != string::npos )
	      connectivity = 4;
//	    else if( property.find( "Pyramid" ) != string::npos )
//	      connectivity = 5;
	    else if( property.find( "Prism" ) != string::npos )
	      connectivity = 6;
	    else if( property.find( "Hex" ) != string::npos )
	      connectivity = 8;
	    else if( property.find( "Curve" ) != string::npos )
	      connectivity = 2;
	    else if( property.find( "Tri" ) != string::npos ) 
	      connectivity = 3;
	    else if( property.find( "Quad" ) != string::npos )
	      connectivity = 4;
	    else {
	      error( property + " Unsupported cell type." );
	      error_ = true;
	      return;
	    }

	    // If the dim is three then the last axis has the number
	    // of connections.
	    if( !(nHandle->nrrd->dim == 3 &&
		  nHandle->nrrd->axis[2].size == connectivity) &&

	    // If stored as Vector or Tensor the last dim will be the point
	    // list dim so do this check instead.
		!(connectivity == 3 &&
		  dataset[0].find( ":Vector" ) != string::npos ) &&
		!(connectivity == 6 &&
		  dataset[0].find( ":Tensor" ) != string::npos ) ) {

	      error( "Connectivity list set does not contain enough points." );
	      error_ = true;
	      return;
	    }

	    mesh_[1] = ic;
	    unstructured = true;

	  } else {
	    error( "Unknown unstructured mesh data found." );
	    error_ = true;
	    return;
	  }
	}
      } else
	// Anything else is considered to be data.
	data_.push_back( ic );
    }

    if( datasetsStr != datasetsStr_.get() ) {
      // Update the dataset names and dims in the GUI.
      ostringstream str;
      str << id << " set_names " << structured << " {" << datasetsStr << "}";
      
      gui->execute(str.str().c_str());
    }
  }

  if( mesh_.size() == 0 ) {
    error( "No mesh present." );
    error_ = true;
    return;
  }

  for( unsigned int ic=0; ic<mesh_.size(); ic++ ) {
    if( mesh_[ic] == -1 ) {
      error( "Not enough information to create the mesh." );
      error_ = true;
      return;
    }
  }

  if( !structured && !unstructured ) {
    error( "Found mesh data but no organization." );
    error_ = true;
    return;
  }

  // If no data or data change, recreate the field.
  if( error_ ||
      !fHandle_.get_rep() ||
      generation ||
      iwrap_ != iWrap_.get() ||
      jwrap_ != jWrap_.get() ||
      kwrap_ != kWrap_.get() ) {
 
    error_ = false;

    iwrap_ = iWrap_.get();
    jwrap_ = jWrap_.get();
    kwrap_ = kWrap_.get();

    vector<unsigned int> mdims;
    vector<unsigned int> mwraps;
    int idim, jdim, kdim;

    if( structured ) {
      
      nHandles[mesh_[0]]->get_property( "Coordinate System", property );

      if( mesh_.size() == 1 ) {
	if( property.find("Cartesian") != string::npos ) {
	  idim = nHandles[mesh_[0]]->nrrd->axis[1].size;
	  jdim = nHandles[mesh_[0]]->nrrd->axis[2].size;
	  kdim = nHandles[mesh_[0]]->nrrd->axis[3].size;

	  if( idim == 1) { iwrap_ = 0; iWrap_.set(0); }
	  if( jdim == 1) { jwrap_ = 0; jWrap_.set(0); }
	  if( kdim == 1) { kwrap_ = 0; kWrap_.set(0); }

	  if( idim > 1) { mdims.push_back( idim ); mwraps.push_back( iwrap_ ); }
	  if( jdim > 1) { mdims.push_back( jdim ); mwraps.push_back( jwrap_ ); }
	  if( kdim > 1) { mdims.push_back( kdim ); mwraps.push_back( kwrap_ ); }
	}
      } else if( mesh_.size() == 3 ) {
	if( property.find("Cylindrical - NIMROD") != string::npos ) {
	  if( nHandles[mesh_[0]]->nrrd->axis[1].size != 
	      nHandles[mesh_[1]]->nrrd->axis[1].size ||
	      nHandles[mesh_[0]]->nrrd->axis[2].size != 
	      nHandles[mesh_[1]]->nrrd->axis[2].size ) {
	    error( "Mesh dimension mismatch." );
	    error_ = true;
	    return;
	  }

	  jdim = nHandles[mesh_[0]]->nrrd->axis[1].size; // Radial
	  kdim = nHandles[mesh_[0]]->nrrd->axis[2].size; // Theta
	  idim = nHandles[mesh_[2]]->nrrd->axis[1].size; // Phi

	  if( idim == 1) { iwrap_ = 0; iWrap_.set(0); }
	  if( jdim == 1) { jwrap_ = 0; jWrap_.set(0); }
	  if( kdim == 1) { kwrap_ = 0; kWrap_.set(0); }

	  if( jdim > 1) { mdims.push_back( jdim ); mwraps.push_back( jwrap_ ); }
	  if( kdim > 1) { mdims.push_back( kdim ); mwraps.push_back( kwrap_ ); }
	  if( idim > 1) { mdims.push_back( idim ); mwraps.push_back( iwrap_ ); }
	}
      }

      structured = mdims.size();

      // Create the mesh.
      if( mdims.size() == 3 ) {
	// 3D StructHexVol
	mHandle = scinew StructHexVolMesh( mdims[0]+mwraps[0],
					   mdims[1]+mwraps[1],
					   mdims[2]+mwraps[2] );
	
      } else if( mdims.size() == 2 ) {
	// 2D StructQuadSurf
	mHandle = scinew StructQuadSurfMesh( mdims[0]+mwraps[0],
					     mdims[1]+mwraps[1] );
	iwrap_ = 0;

      } else if( mdims.size() == 1 ) {
	// 1D StructCurveMesh
	mHandle = scinew StructCurveMesh( mdims[0]+mwraps[0] );
	iwrap_ = jwrap_ = 0;

      } else {
	error( "Mesh dimensions do not make sense." );
	error_ = true;
	return;
      }

      const TypeDescription *mtd = mHandle->get_type_description();
    
      remark( "Creating a structured " + mtd->get_name() );

      CompileInfoHandle ci_mesh =
	NrrdFieldConverterMeshAlgo::get_compile_info( "Structured",
						 mtd,
						 nHandle->nrrd->type,
						 nHandle->nrrd->type);

      Handle<StructuredNrrdFieldConverterMeshAlgo> algo_mesh;
    
      if( !module_dynamic_compile(ci_mesh, algo_mesh) ) return;
    
      algo_mesh->execute(mHandle, nHandles, mesh_,
			 idim, jdim, kdim,
			 iwrap_, jwrap_, kwrap_);
    
    } else if( unstructured ) {

      NrrdDataHandle pHandle = nHandles[mesh_[0]];
      NrrdDataHandle cHandle = nHandles[mesh_[1]];

      mdims.push_back( pHandle->nrrd->axis[1].size );

      string property;

      if( cHandle->get_property( "Cell Type", property ) ) {
	if( property.find( "Tet" ) != string::npos )
	  mHandle = scinew TetVolMesh();
//	else if( property.find( "Pyramid" ) != string::npos )
//	  mHandle = scinew PyramidVolMesh();
	else if( property.find( "Prism" ) != string::npos )
	  mHandle = scinew PrismVolMesh();
	else if( property.find( "Hex" ) != string::npos )
	  mHandle = scinew HexVolMesh();
	else if( property.find( "Curve" ) != string::npos )
	  mHandle = scinew CurveMesh();
	else if( property.find( "Tri" ) != string::npos )
	  mHandle = scinew TriSurfMesh();
	else if( property.find( "Quad" ) != string::npos )
	  mHandle = scinew QuadSurfMesh();
      }

      const TypeDescription *mtd = mHandle->get_type_description();
      
      remark( "Creating an unstructured " + mtd->get_name() );

      CompileInfoHandle ci_mesh =
	NrrdFieldConverterMeshAlgo::get_compile_info("Unstructured",
						mtd,
						pHandle->nrrd->type,
						cHandle->nrrd->type);
      
      Handle<UnstructuredNrrdFieldConverterMeshAlgo> algo_mesh;
      
      if( !module_dynamic_compile(ci_mesh, algo_mesh) ) return;
      
      algo_mesh->execute(mHandle, pHandle, cHandle);    
    }

    
    // Set the rank of the data Scalar(1), Vector(3), Tensor(6).
    // Assume all of the input nrrd data is scalar with the last axis
    // size being the rank of the data.

    vector<unsigned int> ddims;

    int rank = 0;

    if( data_.size() == 0 ) {
      nHandle = NULL;
      rank = 1;
    } else if( data_.size() == 1 || data_.size() == 3 || data_.size() == 6 ) {

      for( unsigned int ic=0; ic<data_.size(); ic++ ) {
	nHandle = nHandles[data_[ic]];
	
	int tuples = nHandle->get_tuple_axis_size();
	
	if( tuples != 1 ) {
	  error( "Too many tuples listed in the tuple axis." );
	  error_ = true;
	  return;
	}

	// Do not allow joined Nrrds
	vector< string > dataset;

	nHandle->get_tuple_indecies(dataset);

	if( dataset.size() != 1 ) {
	  error( "Too many sets listed in the tuple axis." );
	  error_ = true;
	  return;
	}

	// If more than one dataset then all axii must be Scalar
	if( data_.size() > 1 ) {
	  if( dataset[0].find( ":Scalar" ) == string::npos ) {
	    error( "Data type must be scalar. Found: " + dataset[0] );
	    error_ = true;
	    return;
	  }
	}
	    
	ddims.clear();

	for( int jc=0; jc<nHandle->nrrd->dim; jc++ )
	  if( nHandle->nrrd->axis[jc].size > 1 )
	    ddims.push_back( nHandle->nrrd->axis[jc].size );

	if( ddims.size() == mdims.size() ||
	    ddims.size() == mdims.size() + 1 ) {

	  for( unsigned int jc=0; jc<mdims.size(); jc++ ) {
	    if( ddims[jc] != mdims[jc] ) {
	      error( "Data and mesh sizes do not match." );
	      cerr << "Data and mesh sizes do not match. " << endl;

	      for( unsigned int jc=0; jc<mdims.size(); jc++ )
		cerr << mdims[jc] << "  ";
	      cerr << endl;


	      for( unsigned int jc=0; jc<ddims.size(); jc++ )
		cerr << ddims[jc] << "  ";
	      cerr << endl;


	      return;
	    }
	  }
	} else {
	  error( "Data and mesh are not of the same rank." );

	  cerr << "Data and mesh are not of the same rank. ";
	  cerr << ddims.size() << "  " << mdims.size() << endl;
	  error_ = true;
	  return;
	}
      }

      if( data_.size() == 1 ) { 
	vector< string > dataset;

	nHandles[data_[0]]->get_tuple_indecies(dataset);

	if( ddims.size() == mdims.size() ) {
	  if( dataset[0].find( ":Scalar" ) != string::npos )
	    rank = 1;
	  else if( dataset[0].find( ":Vector" ) != string::npos )
	    rank = 3;
	  else if( dataset[0].find( ":Tensor" ) != string::npos )
	    rank = 6;
 	  else {
	    error( "Bad tuple axis - no data type must be scalar, vector, or tensor." );
	    error( dataset[0] );
	    error_ = true;
	    return;
	  }
	} else if(ddims.size() == mdims.size() + 1) {
	  rank = ddims[mdims.size()];
	}
      } else {
	rank = data_.size();
      }
    } else {
      error( "Impropper number of data handles." );
      error_ = true;
      return;

    }

    if( rank != 1 && rank != 3 && rank != 6 ) {
      error( "Bad data rank." );
      error_ = true;
      return;
    }

    if( data_.size() )
      remark( "Adding in the data." );

    const TypeDescription *mtd = mHandle->get_type_description();
    
    string fname = mtd->get_name();
    string::size_type pos = fname.find( "Mesh" );
    fname.replace( pos, 4, "Field" );
    
    CompileInfoHandle ci =
      NrrdFieldConverterFieldAlgo::get_compile_info(mtd,
					       fname,
					       data_.size() ? 
					       nHandles[data_[0]]->nrrd->type : 0,
					       rank);
    
    Handle<NrrdFieldConverterFieldAlgo> algo;
    
    if (!module_dynamic_compile(ci, algo)) return;
    
    if( structured ) {

      // For NIMROD vectors phi of the mesh is needed.
      if( data_.size() && rank == 3 &&
	  nHandles[mesh_[0]]->get_property( "Coordinate System", property ) &&
	  property.find("Cylindrical - NIMROD") != string::npos )
	data_.push_back( mesh_[2] );

      fHandle_ = algo->execute( mHandle, nHandles, data_,
			        idim, jdim, kdim,
			        iwrap_, jwrap_, kwrap_ );

    } else if( unstructured ) {
      fHandle_ = algo->execute( mHandle, nHandles, data_ );
    }
  }

  // Get a handle to the output field port.
  if( fHandle_.get_rep() ) {
    FieldOPort *ofield_port = 
      (FieldOPort *) get_oport("Output Field");
    
    if (!ofield_port) {
      error("Unable to initialize "+name+"'s oport\n");
      return;
    }

    // Send the data downstream
    ofield_port->send( fHandle_ );

  }
}

void
NrrdFieldConverter::tcl_command(GuiArgs& args, void* userdata)
{
  Module::tcl_command(args, userdata);
}


void get_nrrd_type( const unsigned int type,
		    string & typeStr,
		    string & typeName )
{
  switch (type) {
  case nrrdTypeChar :  
    typeStr = string("char");
    typeName = string("char");
    break;
  case nrrdTypeUChar : 
    typeStr = string("unsigned char");
    typeName = string("unsigned_char");
    break;
  case nrrdTypeShort : 
    typeStr = string("short");
    typeName = string("short");
    break;
  case nrrdTypeUShort :
    typeStr = string("unsigned short");
    typeName = string("unsigned_short");
    break;
  case nrrdTypeInt : 
    typeStr = string("int");
    typeName = string("int");
    break;
  case nrrdTypeUInt :  
    typeStr = string("unsigned int");
    typeName = string("unsigned_int");
    break;
  case nrrdTypeLLong : 
    typeStr = string("long long");
    typeName = string("long_long");
    break;
  case nrrdTypeULLong :
    typeStr = string("unsigned long long");
    typeName = string("unsigned_long_long");
    break;
  case nrrdTypeFloat :
    typeStr = string("float");
    typeName = string("float");
    break;
  case nrrdTypeDouble :
    typeStr = string("double");
    typeName = string("double");
    break;
  }
}

CompileInfoHandle
NrrdFieldConverterMeshAlgo::get_compile_info( const string topoStr,
					 const TypeDescription *mtd,
					 const unsigned int ptype,
					 const unsigned int ctype)
{
  // use cc_to_h if this is in the .cc file, otherwise just __FILE__
  static const string include_path(TypeDescription::cc_to_h(__FILE__));
  const string base_class_name(topoStr + "NrrdFieldConverterMeshAlgo");
  const string template_class_name(topoStr + "NrrdFieldConverterMeshAlgoT");

  string pTypeStr,  cTypeStr;
  string pTypeName, cTypeName;

  get_nrrd_type( ptype, pTypeStr, pTypeName );
  get_nrrd_type( ctype, cTypeStr, cTypeName );

  CompileInfo *rval = 
    scinew CompileInfo(template_class_name + "." +
		       mtd->get_filename() + "." +
		       pTypeName + "." + cTypeName + ".",
                       base_class_name, 
                       template_class_name,
                       mtd->get_name() + ", " + pTypeStr + ", " + cTypeStr );

  // Add in the include path to compile this obj
  rval->add_include(include_path);
  rval->add_namespace("Fusion");
  mtd->fill_compile_info(rval);
  return rval;
}

CompileInfoHandle
NrrdFieldConverterFieldAlgo::get_compile_info(const TypeDescription *mtd,
					 const string fname,
					 const unsigned int type,
					 int rank)
{
  // use cc_to_h if this is in the .cc file, otherwise just __FILE__
  static const string include_path(TypeDescription::cc_to_h(__FILE__));
  static const string base_class_name("NrrdFieldConverterFieldAlgo");

  string typeStr, typeName;

  get_nrrd_type( type, typeStr, typeName );

  string extension;
  switch (rank)
  {
  case 6:
    extension = "Tensor";
    break;

  case 3:
    extension = "Vector";
    break;

  default:
    extension = "Scalar";
    break;
  }

  CompileInfo *rval = 
    scinew CompileInfo(base_class_name + extension + "." +
		       mtd->get_filename() + "." + 
		       typeName + ".",
                       base_class_name,
                       base_class_name + extension, 
		       fname +
		       "< " + (rank==1 ? typeName : extension) + " >" + ", " + 
		       mtd->get_name() + ", " + 
		       typeStr );

  // Add in the include path to compile this obj
  rval->add_include(include_path);
  rval->add_namespace("Fusion");
  mtd->fill_compile_info(rval);
  return rval;
}

} // End namespace Fusion


