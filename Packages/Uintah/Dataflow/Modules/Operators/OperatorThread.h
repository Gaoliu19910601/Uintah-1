#if ! defined(UINTAH_OPERATOR_THREAD_H)
#define UINTAH_OPERATOR_THREAD_H

#include <Core/Thread/Thread.h>
#include <Core/Thread/Runnable.h>
#include <Core/Thread/Semaphore.h>
#include <Core/Thread/Mutex.h>
#include <Core/Geometry/IntVector.h>
#include <Packages/Uintah/Core/Grid/Array3.h>
#include <iostream>
using std::cerr;
using std::endl;

namespace Uintah {
  using namespace SCIRun;



template<class Data, class ScalarField, class Op>
class OperatorThread : public Runnable
{
public:
  OperatorThread( Array3<Data>&  data, ScalarField *scalarField,
		  IntVector offset, Op op,
		  Semaphore *sema ) :
    data_(data), sf_(scalarField), offset_(offset),
    op_(op), sema_(sema) {}

  void run()
{
  typename ScalarField::mesh_type *m =
    sf_->get_typed_mesh().get_rep();
  Array3<Data>::iterator it(data_.begin());
  Array3<Data>::iterator it_end(data_.end());
  IntVector min(data_.getLowIndex() - offset_);
  IntVector size(data_.getHighIndex() - min);
  typename ScalarField::mesh_type mesh(m, min.x(), min.y(), min.z(),
				       size.x(), size.y(), size.z());
  if( sf_->data_at() == Field::CELL ){
    typename ScalarField::mesh_type::Cell::iterator s_it = mesh.cell_begin();
    typename ScalarField::mesh_type::Cell::iterator s_it_end = mesh.cell_end();
    for(; s_it != s_it_end; ++it, ++s_it){
      sf_->fdata()[*s_it] = op_( *it );
    }
  } else {
    typename ScalarField::mesh_type::Node::iterator s_it = mesh.node_begin();
    typename ScalarField::mesh_type::Node::iterator s_it_end = mesh.node_end();
    for(; s_it != s_it_end; ++it, ++s_it){
      sf_->fdata()[*s_it] = op_( *it );
    }
  }
  sema_->up();
}
private:
  Array3<Data>& data_;
  ScalarField *sf_;
  IntVector offset_;
  Op op_;
  Semaphore *sema_;
  Mutex* lock_;
};

} // namespace Uintah
#endif //UINTAH_OPERATOR_THREAD_H
