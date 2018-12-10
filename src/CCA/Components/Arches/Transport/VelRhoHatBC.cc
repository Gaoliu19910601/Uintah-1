#include <CCA/Components/Arches/Transport/VelRhoHatBC.h>

using namespace Uintah;
typedef ArchesFieldContainer AFC;

//--------------------------------------------------------------------------------------------------
VelRhoHatBC::VelRhoHatBC( std::string task_name, int matl_index ) :
AtomicTaskInterface( task_name, matl_index )
{
}

//--------------------------------------------------------------------------------------------------
VelRhoHatBC::~VelRhoHatBC()
{
}

//--------------------------------------------------------------------------------------------------
TaskAssignedExecutionSpace VelRhoHatBC::loadTaskComputeBCsFunctionPointers()
{
  return create_portable_arches_tasks<TaskInterface::BC>( this
                                     , &VelRhoHatBC::compute_bcs<UINTAH_CPU_TAG>     // Task supports non-Kokkos builds
                                     , &VelRhoHatBC::compute_bcs<KOKKOS_OPENMP_TAG>  // Task supports Kokkos::OpenMP builds
                                     , &VelRhoHatBC::compute_bcs<KOKKOS_CUDA_TAG>    // Task supports Kokkos::Cuda builds
                                     );
}

//--------------------------------------------------------------------------------------------------
TaskAssignedExecutionSpace VelRhoHatBC::loadTaskInitializeFunctionPointers()
{
  return create_portable_arches_tasks<TaskInterface::INITIALIZE>( this
                                     , &VelRhoHatBC::initialize<UINTAH_CPU_TAG>     // Task supports non-Kokkos builds
                                     , &VelRhoHatBC::initialize<KOKKOS_OPENMP_TAG>  // Task supports Kokkos::OpenMP builds
                                     , &VelRhoHatBC::initialize<KOKKOS_CUDA_TAG>    // Task supports Kokkos::Cuda builds
                                     );
}

//--------------------------------------------------------------------------------------------------
TaskAssignedExecutionSpace VelRhoHatBC::loadTaskEvalFunctionPointers()
{
  return create_portable_arches_tasks<TaskInterface::TIMESTEP_EVAL>( this
                                     , &VelRhoHatBC::eval<UINTAH_CPU_TAG>     // Task supports non-Kokkos builds
                                     , &VelRhoHatBC::eval<KOKKOS_OPENMP_TAG>  // Task supports Kokkos::OpenMP builds
                                     , &VelRhoHatBC::eval<KOKKOS_CUDA_TAG>    // Task supports Kokkos::Cuda builds
                                     );
}

TaskAssignedExecutionSpace VelRhoHatBC::loadTaskTimestepInitFunctionPointers()
{
  return  TaskAssignedExecutionSpace::NONE_EXECUTION_SPACE;
}

TaskAssignedExecutionSpace VelRhoHatBC::loadTaskRestartInitFunctionPointers()
{
  return  TaskAssignedExecutionSpace::NONE_EXECUTION_SPACE;
}

//--------------------------------------------------------------------------------------------------
void VelRhoHatBC::problemSetup( ProblemSpecP& db ){
  m_xmom = "x-mom";
  m_ymom = "y-mom";
  m_zmom = "z-mom";
  m_uVel ="uVel";
  m_vVel ="vVel";
  m_wVel ="wVel";
}

//--------------------------------------------------------------------------------------------------
void VelRhoHatBC::create_local_labels(){
}

//--------------------------------------------------------------------------------------------------
void VelRhoHatBC::register_timestep_eval( std::vector<AFC::VariableInformation>& variable_registry,
                                 const int time_substep, const bool pack_tasks ){
  register_variable( m_xmom, AFC::MODIFIES, variable_registry, m_task_name );
  register_variable( m_ymom, AFC::MODIFIES, variable_registry, m_task_name );
  register_variable( m_zmom, AFC::MODIFIES, variable_registry, m_task_name );
//  register_variable( m_uVel, AFC::REQUIRES, 0, AFC::NEWDW, variable_registry, time_substep, m_task_name );
//  register_variable( m_vVel, AFC::REQUIRES, 0, AFC::NEWDW, variable_registry, time_substep, m_task_name );
//  register_variable( m_wVel, AFC::REQUIRES, 0, AFC::NEWDW, variable_registry, time_substep, m_task_name );
  register_variable( m_uVel, AFC::REQUIRES, 0, AFC::OLDDW, variable_registry, time_substep, m_task_name );
  register_variable( m_vVel, AFC::REQUIRES, 0, AFC::OLDDW, variable_registry, time_substep, m_task_name );
  register_variable( m_wVel, AFC::REQUIRES, 0, AFC::OLDDW, variable_registry, time_substep, m_task_name );

  
  
}

// wrapper templated function to deal with different types
template<typename ExecutionSpace, typename MemSpace, typename grid_T, typename Cgrid_T>
void VelRhoHatBC::set_mom_bc( ExecutionObject<ExecutionSpace,MemSpace> &exObj,grid_T& var, const Cgrid_T& old_var, IntVector& iDir,  const double &possmall , const int sign, ListOfCellsIterator& cell_iter){
     int move_to_face_value = ( (iDir[0]+iDir[1]+iDir[2]) < 1 ) ? 1 : 0;

     IntVector move_to_face(std::abs(iDir[0])*move_to_face_value,
                            std::abs(iDir[1])*move_to_face_value,
                            std::abs(iDir[2])*move_to_face_value);

      parallel_for_unstructured(exObj, cell_iter.get_ref_to_iterator<MemSpace>(),cell_iter.size(), KOKKOS_LAMBDA (const int i,const int j,const int k) {
        int i_f = i + move_to_face[0]; // cell on the face
        int j_f = j + move_to_face[1];
        int k_f = k + move_to_face[2];

        int im = i_f - iDir[0];// first interior cell
        int jm = j_f - iDir[1];
        int km = k_f - iDir[2];

        int ipp = i_f + iDir[0];// extra cell face in the last index (mostly outwardly position) 
        int jpp = j_f + iDir[1];
        int kpp = k_f + iDir[2];

        if ( sign * old_var(i_f,j_f,k_f) > possmall ){
          // du/dx = 0
          var(i_f,j_f,k_f)= var(im,jm,km);
        } else {
          // shut off the hatted value to encourage the mostly-* condition
          var(i_f,j_f,k_f) = 0.0;
        }
          var(ipp,jpp,kpp) = var(i_f,j_f,k_f); 

      });
  }

template<typename ExecutionSpace, typename MemSpace>
void VelRhoHatBC::eval( const Patch* patch, ArchesTaskInfoManager* tsk_info, ExecutionObject<ExecutionSpace, MemSpace>& exObj ){

  auto xmom = tsk_info->get_uintah_field_add<SFCXVariable<double>, double, MemSpace >( m_xmom );
  auto ymom = tsk_info->get_uintah_field_add<SFCYVariable<double>, double, MemSpace >( m_ymom );
  auto zmom = tsk_info->get_uintah_field_add<SFCZVariable<double>, double, MemSpace >( m_zmom );

  auto old_uVel = tsk_info->get_const_uintah_field_add<constSFCXVariable<double>, const double, MemSpace >( m_uVel );
  auto old_vVel = tsk_info->get_const_uintah_field_add<constSFCYVariable<double>, const double, MemSpace >( m_vVel );
  auto old_wVel = tsk_info->get_const_uintah_field_add<constSFCZVariable<double>, const double, MemSpace >( m_wVel );

  const BndMapT& bc_info = m_bcHelper->get_boundary_information();
  const double possmall = 1e-16;

  for ( auto i_bc = bc_info.begin(); i_bc != bc_info.end(); i_bc++ ){

    const bool on_this_patch = i_bc->second.has_patch(patch->getID());
    if ( !on_this_patch ) continue;

    Uintah::ListOfCellsIterator& cell_iter = m_bcHelper->get_uintah_extra_bnd_mask( i_bc->second, patch->getID() );
    IntVector iDir = patch->faceDirection( i_bc->second.face );
    Patch::FaceType face = i_bc->second.face;
    BndTypeEnum my_type = i_bc->second.type;
    int sign = iDir[0] + iDir[1] + iDir[2];
    int bc_sign = 0;

    if ( my_type == OUTLET ){
      bc_sign = 1.;
    } else if ( my_type == PRESSURE){
      bc_sign = -1.;
    }
  
    sign = bc_sign * sign;

    if ( my_type == OUTLET || my_type == PRESSURE ){
      if ( face == Patch::xminus || face == Patch::xplus ){
        set_mom_bc(exObj, xmom, old_uVel, iDir, possmall , sign, cell_iter);
      }else if ( face == Patch::yminus || face == Patch::yplus ){
        set_mom_bc(exObj, ymom, old_vVel, iDir, possmall , sign, cell_iter);
      }else {
        set_mom_bc(exObj, zmom, old_wVel, iDir, possmall , sign, cell_iter);
      }
      // This applies the mostly in (pressure)/mostly out (outlet) boundary condition
    }
  }
}
