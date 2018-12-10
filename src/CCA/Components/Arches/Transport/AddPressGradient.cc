#include <CCA/Components/Arches/Transport/AddPressGradient.h>
#include <CCA/Components/Arches/GridTools.h>

using namespace Uintah;
typedef ArchesFieldContainer AFC;

//--------------------------------------------------------------------------------------------------
AddPressGradient::AddPressGradient( std::string task_name, int matl_index ) :
AtomicTaskInterface( task_name, matl_index )
{
}

//--------------------------------------------------------------------------------------------------
AddPressGradient::~AddPressGradient()
{
}

//--------------------------------------------------------------------------------------------------
TaskAssignedExecutionSpace AddPressGradient::loadTaskComputeBCsFunctionPointers()
{
  return create_portable_arches_tasks<TaskInterface::BC>( this
                                     , &AddPressGradient::compute_bcs<UINTAH_CPU_TAG>     // Task supports non-Kokkos builds
                                     , &AddPressGradient::compute_bcs<KOKKOS_OPENMP_TAG>  // Task supports Kokkos::OpenMP builds
                                     , &AddPressGradient::compute_bcs<KOKKOS_CUDA_TAG>    // Task supports Kokkos::Cuda builds
                                     );
}

//--------------------------------------------------------------------------------------------------
TaskAssignedExecutionSpace AddPressGradient::loadTaskInitializeFunctionPointers()
{
  return create_portable_arches_tasks<TaskInterface::INITIALIZE>( this
                                     , &AddPressGradient::initialize<UINTAH_CPU_TAG>     // Task supports non-Kokkos builds
                                     , &AddPressGradient::initialize<KOKKOS_OPENMP_TAG>  // Task supports Kokkos::OpenMP builds
                                     , &AddPressGradient::initialize<KOKKOS_CUDA_TAG>    // Task supports Kokkos::Cuda builds
                                     );
}

//--------------------------------------------------------------------------------------------------
TaskAssignedExecutionSpace AddPressGradient::loadTaskEvalFunctionPointers()
{
  return create_portable_arches_tasks<TaskInterface::TIMESTEP_EVAL>( this
                                     , &AddPressGradient::eval<UINTAH_CPU_TAG>     // Task supports non-Kokkos builds
                                     , &AddPressGradient::eval<KOKKOS_OPENMP_TAG>  // Task supports Kokkos::OpenMP builds
                                     , &AddPressGradient::eval<KOKKOS_CUDA_TAG>    // Task supports Kokkos::Cuda builds
                                     );
}

TaskAssignedExecutionSpace AddPressGradient::loadTaskTimestepInitFunctionPointers()
{
  return  TaskAssignedExecutionSpace::NONE_EXECUTION_SPACE;
}

TaskAssignedExecutionSpace AddPressGradient::loadTaskRestartInitFunctionPointers()
{
  return  TaskAssignedExecutionSpace::NONE_EXECUTION_SPACE;
}
//--------------------------------------------------------------------------------------------------
void AddPressGradient::problemSetup( ProblemSpecP& db ){

  m_eps_name = "volFraction";
  m_xmom = "x-mom";
  m_ymom = "y-mom";
  m_zmom = "z-mom";
  m_press = "pressure";
}

//--------------------------------------------------------------------------------------------------
void AddPressGradient::create_local_labels(){
}

//--------------------------------------------------------------------------------------------------
void AddPressGradient::register_timestep_eval( std::vector<AFC::VariableInformation>& variable_registry,
  const int time_substep, const bool pack_tasks ){
  register_variable( m_xmom, AFC::MODIFIES, variable_registry, time_substep, m_task_name );
  register_variable( m_ymom, AFC::MODIFIES, variable_registry, time_substep, m_task_name );
  register_variable( m_zmom, AFC::MODIFIES, variable_registry, time_substep, m_task_name );
  register_variable( m_press, AFC::REQUIRES, 1, AFC::NEWDW, variable_registry, time_substep, m_task_name );
  register_variable( m_eps_name, AFC::REQUIRES, 1, AFC::NEWDW, variable_registry, time_substep, m_task_name  );
}

//--------------------------------------------------------------------------------------------------
template<typename ExecutionSpace, typename MemSpace>
void AddPressGradient::eval( const Patch* patch, ArchesTaskInfoManager* tsk_info, ExecutionObject<ExecutionSpace, MemSpace>& exObj ){

  const double dt = tsk_info->get_dt();
  Vector DX = patch->dCell();
  auto xmom = tsk_info->get_uintah_field_add<SFCXVariable<double>,double, MemSpace >( m_xmom );
  auto ymom = tsk_info->get_uintah_field_add<SFCYVariable<double>,double, MemSpace >( m_ymom );
  auto zmom = tsk_info->get_uintah_field_add<SFCZVariable<double>,double, MemSpace >( m_zmom );
  auto p = tsk_info->get_const_uintah_field_add<constCCVariable<double>, const double, MemSpace >(m_press);
  auto eps = tsk_info->get_const_uintah_field_add<constCCVariable<double>, const double, MemSpace >(m_eps_name);

  // because the hypre solve required a positive diagonal
  // so we -1 * ( Ax = b ) requiring that we change the sign
  // back.

  // boundary conditions on the pressure fields are applied
  // post linear solve in the PressureBC.cc class.

  GET_EXTRACELL_FX_BUFFERED_PATCH_RANGE(0, 1)
  Uintah::BlockRange x_range( low_fx_patch_range, high_fx_patch_range );

  Uintah::parallel_for(exObj, x_range, KOKKOS_LAMBDA (int i, int j, int k){

    const double afc = floor(( eps(i,j,k) + eps(i-1,j,k) ) / 2. );
    xmom(i,j,k) += dt * ( p(i-1,j,k) - p(i,j,k) ) / DX.x()*afc;

  });

  GET_EXTRACELL_FY_BUFFERED_PATCH_RANGE(0, 1)
  Uintah::BlockRange y_range( low_fy_patch_range, high_fy_patch_range );

  Uintah::parallel_for(exObj, y_range, KOKKOS_LAMBDA (int i, int j, int k){

    const double afc = floor(( eps(i,j,k) + eps(i,j-1,k) ) / 2. );
    ymom(i,j,k) += dt * ( p(i,j-1,k) - p(i,j,k) ) / DX.y()*afc;

  });

  GET_EXTRACELL_FZ_BUFFERED_PATCH_RANGE(0, 1)
  Uintah::BlockRange z_range( low_fz_patch_range, high_fz_patch_range );
  Uintah::parallel_for(exObj, z_range, KOKKOS_LAMBDA (int i, int j, int k){

    const double afc = floor(( eps(i,j,k) + eps(i,j,k-1) ) / 2. );
    zmom(i,j,k) += dt * ( p(i,j,k-1) - p(i,j,k) ) / DX.z()*afc;

  });
}
