#include "EigenEvaluator.h"
#include <math.h>
#include <Core/Malloc/Allocator.h>
#include <Core/Datatypes/LatVolMesh.h>
#include <Core/Datatypes/LatticeVol.h>
#include <Core/Geometry/BBox.h>
#include <Packages/Uintah/Core/Datatypes/LevelField.h>
#include <Packages/Uintah/Core/Datatypes/LevelMesh.h>

namespace Uintah {

extern "C" Module* make_EigenEvaluator( const string& id ) { 
  return scinew EigenEvaluator( id );
}

template<class TensorField, class VectorField, class ScalarField>
void computeGridEigens(TensorField* tensorField,
		       ScalarField* eValueField, VectorField* eVectorField,
		       int chosenEValue);
 

EigenEvaluator::EigenEvaluator(const string& id)
  : Module("EigenEvaluator",id,Source),
    guiEigenSelect("eigenSelect", id, this)
    //    tcl_status("tcl_status", id, this),
{
  // Create Ports
  in = new FieldIPort(this, "TensorField");
  sfout = new FieldOPort(this, "EigenValueField");
  vfout = new FieldOPort(this, "EigenVectorField");

  // Add ports to the Module
  add_iport(in);
  add_oport(sfout);
  add_oport(vfout);
}
  
void EigenEvaluator::execute(void) {
  //  tcl_status.set("Calling EigenEvaluator!"); 
  FieldHandle hTF;
  
  if(!in->get(hTF)){
    std::cerr<<"Didn't get a handle\n";
    return;
  } else if ( hTF->get_type_name(1) != "Matrix3" ){
    std::cerr<<"Input is not a Tensor field\n";
    return;
  }

  LatticeVol<double> *eValueField = 0;
  LatticeVol<Vector> *eVectorField = 0;

  if( LevelField<Matrix3> *tensorField =
      dynamic_cast<LevelField<Matrix3>*>(hTF.get_rep())) {

    eValueField = scinew LatticeVol<double>(hTF->data_at());
    eVectorField = scinew LatticeVol<Vector>(hTF->data_at());
    computeGridEigens(tensorField, eValueField,
		      eVectorField, guiEigenSelect.get());
  }

  if( eValueField )
    sfout->send(eValueField);
  if( eVectorField )
    vfout->send(eVectorField);  
}

template<class TensorField, class VectorField, class ScalarField>
void computeGridEigens(TensorField* tensorField,
		       ScalarField* eValueField, VectorField* eVectorField,
		       int chosenEValue)
{
  ASSERT( tensorField->data_at() == Field::CELL ||
	  tensorField->data_at() == Field::NODE );
  typename TensorField::mesh_handle_type tmh = tensorField->get_typed_mesh();
  typename ScalarField::mesh_handle_type smh = eValueField->get_typed_mesh();
  typename VectorField::mesh_handle_type vmh = eVectorField->get_typed_mesh();

  BBox box;
  Point lb(0, 0, 0), ub(0, 0, 0);
  box = tmh->get_bounding_box();

  //resize the geometry
  smh->set_nx(tmh->get_nx());
  smh->set_ny(tmh->get_ny());
  smh->set_nz(tmh->get_nz());
  smh->set_min( box.min() );
  smh->set_max( box.max() );
  vmh->set_nx(tmh->get_nx());
  vmh->set_ny(tmh->get_ny());
  vmh->set_nz(tmh->get_nz());
  vmh->set_min( box.min() );
  vmh->set_max( box.max() );
  //resize the data storage
  eValueField->resize_fdata();
  eVectorField->resize_fdata();


  int num_eigen_values;
  Matrix3 M;
  double e[3];
  std::vector<Vector> eigenVectors;
  
  if( tensorField->data_at() == Field::CELL){
    typename TensorField::mesh_type::cell_iterator t_it = tmh->cell_begin();
    typename ScalarField::mesh_type::cell_iterator s_it = smh->cell_begin();
    typename VectorField::mesh_type::cell_iterator v_it = vmh->cell_begin();
    typename TensorField::mesh_type::cell_iterator t_end = tmh->cell_end();
    
    for( ; t_it != t_end; ++t_it, ++s_it, ++v_it){
      M = tensorField->fdata()[*t_it];
      num_eigen_values = M.getEigenValues(e[0], e[1], e[2]);
      if (num_eigen_values <= chosenEValue) {
	eValueField->fdata()[*s_it] = 0;
	eVectorField->fdata()[*v_it] = Vector(0,0,0);
      } else {
	eValueField->fdata()[*s_it] = e[chosenEValue];
	eigenVectors = M.getEigenVectors(e[chosenEValue], e[0]);
	if (eigenVectors.size() != 1) {
	  eVectorField->fdata()[*v_it] = Vector(0, 0, 0);
	} else {
	  eVectorField->fdata()[*v_it] = eigenVectors[0].normal();
	}
      }
    }
  } else {
    typename TensorField::mesh_type::node_iterator t_it = tmh->node_begin();
    typename ScalarField::mesh_type::node_iterator s_it = smh->node_begin();
    typename VectorField::mesh_type::node_iterator v_it = vmh->node_begin();
    typename TensorField::mesh_type::node_iterator t_end = tmh->node_end();
    
    for( ; t_it != t_end; ++t_it, ++s_it, ++v_it){
      M = tensorField->fdata()[*t_it];
      num_eigen_values = M.getEigenValues(e[0], e[1], e[2]);
      if (num_eigen_values <= chosenEValue) {
	eValueField->fdata()[*s_it] = 0;
	eVectorField->fdata()[*v_it] = Vector(0,0,0);
      } else {
	eValueField->fdata()[*s_it] = e[chosenEValue];
	eigenVectors = M.getEigenVectors(e[chosenEValue], e[0]);
	if (eigenVectors.size() != 1) {
	  eVectorField->fdata()[*v_it] = Vector(0, 0, 0);
	} else {
	  eVectorField->fdata()[*v_it] = eigenVectors[0].normal();
	}
      }
    }
  }    
}
  
}



