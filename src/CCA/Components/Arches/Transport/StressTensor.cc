#include <CCA/Components/Arches/Transport/StressTensor.h>
#include <CCA/Components/Arches/GridTools.h>

using namespace Uintah;
using namespace ArchesCore;

//--------------------------------------------------------------------------------------------------
StressTensor::StressTensor( std::string task_name, int matl_index ) :
TaskInterface( task_name, matl_index ){

  m_sigma_t_names.resize(6);
  m_sigma_t_names[0] = "sigma11";
  m_sigma_t_names[1] = "sigma12";
  m_sigma_t_names[2] = "sigma13";
  m_sigma_t_names[3] = "sigma22";
  m_sigma_t_names[4] = "sigma23";
  m_sigma_t_names[5] = "sigma33";

}

//--------------------------------------------------------------------------------------------------
StressTensor::~StressTensor(){
}

//--------------------------------------------------------------------------------------------------
TaskAssignedExecutionSpace StressTensor::loadTaskComputeBCsFunctionPointers()
{
  return create_portable_arches_tasks<TaskInterface::BC>( this
                                     , &StressTensor::compute_bcs<UINTAH_CPU_TAG>     // Task supports non-Kokkos builds
                                     , &StressTensor::compute_bcs<KOKKOS_OPENMP_TAG>  // Task supports Kokkos::OpenMP builds
                                     , &StressTensor::compute_bcs<KOKKOS_CUDA_TAG>    // Task supports Kokkos::Cuda builds
                                     );
}

//--------------------------------------------------------------------------------------------------
TaskAssignedExecutionSpace StressTensor::loadTaskInitializeFunctionPointers()
{
  return create_portable_arches_tasks<TaskInterface::INITIALIZE>( this
                                     , &StressTensor::initialize<UINTAH_CPU_TAG>     // Task supports non-Kokkos builds
                                     , &StressTensor::initialize<KOKKOS_OPENMP_TAG>  // Task supports Kokkos::OpenMP builds
                                     , &StressTensor::initialize<KOKKOS_CUDA_TAG>    // Task supports Kokkos::Cuda builds
                                     );
}

//--------------------------------------------------------------------------------------------------
TaskAssignedExecutionSpace StressTensor::loadTaskEvalFunctionPointers()
{
  return create_portable_arches_tasks<TaskInterface::TIMESTEP_EVAL>( this
                                     , &StressTensor::eval<UINTAH_CPU_TAG>     // Task supports non-Kokkos builds
                                     , &StressTensor::eval<KOKKOS_OPENMP_TAG>  // Task supports Kokkos::OpenMP builds
                                     , &StressTensor::eval<KOKKOS_CUDA_TAG>    // Task supports Kokkos::Cuda builds
                                     );
}

TaskAssignedExecutionSpace StressTensor::loadTaskTimestepInitFunctionPointers()
{
  return create_portable_arches_tasks<TaskInterface::TIMESTEP_INITIALIZE>( this
                                     , &StressTensor::timestep_init<UINTAH_CPU_TAG>     // Task supports non-Kokkos builds
                                     , &StressTensor::timestep_init<KOKKOS_OPENMP_TAG>  // Task supports Kokkos::OpenMP builds
                                     , &StressTensor::timestep_init<KOKKOS_CUDA_TAG>  // Task supports Kokkos::OpenMP builds
                                     );
}

TaskAssignedExecutionSpace StressTensor::loadTaskRestartInitFunctionPointers()
{
  return  TaskAssignedExecutionSpace::NONE_EXECUTION_SPACE;
}

//--------------------------------------------------------------------------------------------------
void StressTensor::problemSetup( ProblemSpecP& db ){

  using namespace Uintah::ArchesCore;

    m_u_vel_name = parse_ups_for_role( UVELOCITY, db, "uVelocitySPBC" );
    m_v_vel_name = parse_ups_for_role( VVELOCITY, db, "vVelocitySPBC" );
    m_w_vel_name = parse_ups_for_role( WVELOCITY, db, "wVelocitySPBC" );
    m_t_vis_name = parse_ups_for_role( TOTAL_VISCOSITY, db );
//
  /* It is going to use central scheme as default   */
  diff_scheme = "central";
  Nghost_cells = 1;

}

//--------------------------------------------------------------------------------------------------
void StressTensor::create_local_labels(){
  for (auto iter = m_sigma_t_names.begin(); iter != m_sigma_t_names.end(); iter++ ){
    register_new_variable<CCVariable<double> >(*iter);
  }
}

//--------------------------------------------------------------------------------------------------
void StressTensor::register_initialize( AVarInfo& variable_registry , const bool pack_tasks){
  for (auto iter = m_sigma_t_names.begin(); iter != m_sigma_t_names.end(); iter++ ){
    register_variable( *iter, ArchesFieldContainer::COMPUTES, variable_registry, m_task_name );
  }
}

//--------------------------------------------------------------------------------------------------
template<typename ExecutionSpace, typename MemSpace>
void StressTensor::initialize( const Patch* patch, ArchesTaskInfoManager* tsk_info, ExecutionObject<ExecutionSpace, MemSpace>& exObj ){


  auto sigma11 = tsk_info->get_uintah_field_add<CCVariable<double>,double, MemSpace >(m_sigma_t_names[0]);
  auto sigma12 = tsk_info->get_uintah_field_add<CCVariable<double>,double, MemSpace >(m_sigma_t_names[1]);
  auto sigma13 = tsk_info->get_uintah_field_add<CCVariable<double>,double, MemSpace >(m_sigma_t_names[2]);
  auto sigma22 = tsk_info->get_uintah_field_add<CCVariable<double>,double, MemSpace >(m_sigma_t_names[3]);
  auto sigma23 = tsk_info->get_uintah_field_add<CCVariable<double>,double, MemSpace >(m_sigma_t_names[4]);
  auto sigma33 = tsk_info->get_uintah_field_add<CCVariable<double>,double, MemSpace >(m_sigma_t_names[5]);

  parallel_initialize(exObj,0.0,sigma11
                               ,sigma12
                               ,sigma13
                               ,sigma22
                               ,sigma23
                               ,sigma33);
}

//--------------------------------------------------------------------------------------------------
void StressTensor::register_timestep_eval( VIVec& variable_registry, const int time_substep , const bool packed_tasks){
  // time_substep?
  for (auto iter = m_sigma_t_names.begin(); iter != m_sigma_t_names.end(); iter++ ){
    register_variable( *iter, ArchesFieldContainer::COMPUTES, variable_registry, m_task_name );
  }
  register_variable( m_u_vel_name, ArchesFieldContainer::REQUIRES, Nghost_cells, ArchesFieldContainer::LATEST, variable_registry, time_substep);
  register_variable( m_v_vel_name, ArchesFieldContainer::REQUIRES, Nghost_cells, ArchesFieldContainer::LATEST, variable_registry, time_substep);
  register_variable( m_w_vel_name, ArchesFieldContainer::REQUIRES, Nghost_cells, ArchesFieldContainer::LATEST, variable_registry, time_substep);
  register_variable( m_t_vis_name, ArchesFieldContainer::REQUIRES, Nghost_cells, ArchesFieldContainer::NEWDW, variable_registry, time_substep);
}

//--------------------------------------------------------------------------------------------------
template<typename ExecutionSpace, typename MemSpace>
void StressTensor::eval( const Patch* patch, ArchesTaskInfoManager* tsk_info, ExecutionObject<ExecutionSpace, MemSpace>& exObj ){

  auto uVel = tsk_info->get_const_uintah_field_add<constSFCXVariable<double>, const double, MemSpace>(m_u_vel_name);
  auto vVel = tsk_info->get_const_uintah_field_add<constSFCYVariable<double>, const double, MemSpace>(m_v_vel_name);
  auto wVel = tsk_info->get_const_uintah_field_add<constSFCZVariable<double>, const double, MemSpace>(m_w_vel_name);
  auto   D  = tsk_info->get_const_uintah_field_add<constCCVariable<double>, const double, MemSpace >(m_t_vis_name);

  auto sigma11 = tsk_info->get_uintah_field_add<CCVariable<double>, double, MemSpace >(m_sigma_t_names[0]);
  auto sigma12 = tsk_info->get_uintah_field_add<CCVariable<double>, double, MemSpace >(m_sigma_t_names[1]);
  auto sigma13 = tsk_info->get_uintah_field_add<CCVariable<double>, double, MemSpace >(m_sigma_t_names[2]);
  auto sigma22 = tsk_info->get_uintah_field_add<CCVariable<double>, double, MemSpace >(m_sigma_t_names[3]);
  auto sigma23 = tsk_info->get_uintah_field_add<CCVariable<double>, double, MemSpace >(m_sigma_t_names[4]);
  auto sigma33 = tsk_info->get_uintah_field_add<CCVariable<double>, double, MemSpace >(m_sigma_t_names[5]);

  // initialize all velocities
  parallel_initialize(exObj,0.0, sigma11, sigma12, sigma13, sigma22, sigma23,sigma33);

  Vector Dx = patch->dCell();

  IntVector low = patch->getCellLowIndex();
  IntVector high = patch->getCellHighIndex();

  GET_WALL_BUFFERED_PATCH_RANGE(low, high,0,1,0,1,0,1);  
  Uintah::BlockRange x_range(low, high);
 
  //auto apply_uVelStencil=functorCreationWrapper(  uVel,  Dx); // non-macro approach gives cuda streaming error downstream
  //auto apply_vVelStencil=functorCreationWrapper(  vVel,  Dx);
  //auto apply_wVelStencil=functorCreationWrapper(  wVel,  Dx);

  Uintah::parallel_for(exObj, x_range, KOKKOS_LAMBDA (int i, int j, int k){

    double dudx = 0.0;
    double dudy = 0.0;
    double dudz = 0.0;
    double dvdx = 0.0;
    double dvdy = 0.0;
    double dvdz = 0.0;
    double dwdx = 0.0;
    double dwdy = 0.0;
    double dwdz = 0.0;
    double mu11 = 0.0;
    double mu12 = 0.0;
    double mu13 = 0.0;
    double mu22 = 0.0;
    double mu23 = 0.0;
    double mu33 = 0.0;

    mu11 = D(i-1,j,k); // it does not need interpolation
    mu22 = D(i,j-1,k);  // it does not need interpolation
    mu33 = D(i,j,k-1);  // it does not need interpolation
    mu12  = 0.5*(D(i-1,j,k)+D(i,j,k)); // First interpolation at j
    mu12 += 0.5*(D(i-1,j-1,k)+D(i,j-1,k));// Second interpolation at j-1
    mu12 *= 0.5;
    mu13  = 0.5*(D(i-1,j,k-1)+D(i,j,k-1));//First interpolation at k-1
    mu13 += 0.5*(D(i-1,j,k)+D(i,j,k));//Second interpolation at k
    mu13 *= 0.5;
    mu23  = 0.5*(D(i,j,k)+D(i,j,k-1));// First interpolation at j
    mu23 += 0.5*(D(i,j-1,k)+D(i,j-1,k-1));// Second interpolation at j-1
    mu23 *= 0.5;

    //apply_uVelStencil(dudx,dudy,dudz,i,j,k);  // non-macro approach gives cuda streaming error downstream, likely due to saving templated value as reference instead of value. But must save by reference to suppor legacy code.  poosibly Use getKokkosView in functor constructor
    //apply_vVelStencil(dvdx,dvdy,dvdz,i,j,k);
    //apply_wVelStencil(dwdx,dwdy,dwdz,i,j,k);
    dVeldDir(uVel, Dx, dudx,dudy,dudz,i,j,k);
    dVeldDir(vVel, Dx, dvdx,dvdy,dvdz,i,j,k);
    dVeldDir(wVel, Dx, dwdx,dwdy,dwdz,i,j,k);

    sigma11(i,j,k) =  mu11 * 2.0*dudx;
    sigma12(i,j,k) =  mu12 * (dudy + dvdx );
    sigma13(i,j,k) =  mu13 * (dudz + dwdx );
    sigma22(i,j,k) =  mu22 * 2.0*dvdy;
    sigma23(i,j,k) =  mu23 * (dvdz + dwdy );
    sigma33(i,j,k) =  mu33 * 2.0*dwdz;

  });
}
