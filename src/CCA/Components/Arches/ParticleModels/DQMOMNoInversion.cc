#include <CCA/Components/Arches/ParticleModels/DQMOMNoInversion.h>
#include <CCA/Components/Arches/ParticleModels/ParticleTools.h>

using namespace Uintah;

//--------------------------------------------------------------------------------------------------
DQMOMNoInversion::DQMOMNoInversion( std::string task_name, int matl_index, const int N ) :
                  TaskInterface( task_name, matl_index ), m_N(N){
}

//--------------------------------------------------------------------------------------------------
DQMOMNoInversion::~DQMOMNoInversion(){}

//--------------------------------------------------------------------------------------------------
TaskAssignedExecutionSpace DQMOMNoInversion::loadTaskComputeBCsFunctionPointers()
{
  return create_portable_arches_tasks<TaskInterface::BC>( this
                                     , &DQMOMNoInversion::compute_bcs<UINTAH_CPU_TAG>     // Task supports non-Kokkos builds
                                     //, &DQMOMNoInversion::compute_bcs<KOKKOS_OPENMP_TAG>  // Task supports Kokkos::OpenMP builds
                                     //, &DQMOMNoInversion::compute_bcs<KOKKOS_CUDA_TAG>    // Task supports Kokkos::Cuda builds
                                     );
}

//--------------------------------------------------------------------------------------------------
TaskAssignedExecutionSpace DQMOMNoInversion::loadTaskInitializeFunctionPointers()
{
  return create_portable_arches_tasks<TaskInterface::INITIALIZE>( this
                                     , &DQMOMNoInversion::initialize<UINTAH_CPU_TAG>     // Task supports non-Kokkos builds
                                     //, &DQMOMNoInversion::initialize<KOKKOS_OPENMP_TAG>  // Task supports Kokkos::OpenMP builds
                                     //, &DQMOMNoInversion::initialize<KOKKOS_CUDA_TAG>    // Task supports Kokkos::Cuda builds
                                     );
}

//--------------------------------------------------------------------------------------------------
TaskAssignedExecutionSpace DQMOMNoInversion::loadTaskEvalFunctionPointers()
{
  return create_portable_arches_tasks<TaskInterface::TIMESTEP_EVAL>( this
                                     , &DQMOMNoInversion::eval<UINTAH_CPU_TAG>     // Task supports non-Kokkos builds
                                     , &DQMOMNoInversion::eval<KOKKOS_OPENMP_TAG>  // Task supports Kokkos::OpenMP builds
                                     //, &DQMOMNoInversion::eval<KOKKOS_CUDA_TAG>    // Task supports Kokkos::Cuda builds
                                     );
}

TaskAssignedExecutionSpace DQMOMNoInversion::loadTaskTimestepInitFunctionPointers()
{
  return create_portable_arches_tasks<TaskInterface::TIMESTEP_INITIALIZE>( this
                                     , &DQMOMNoInversion::timestep_init<UINTAH_CPU_TAG>     // Task supports non-Kokkos builds
                                     , &DQMOMNoInversion::timestep_init<KOKKOS_OPENMP_TAG>  // Task supports Kokkos::OpenMP builds
                                     );
}

TaskAssignedExecutionSpace DQMOMNoInversion::loadTaskRestartInitFunctionPointers()
{
    return  TaskAssignedExecutionSpace::NONE_EXECUTION_SPACE;
}


//--------------------------------------------------------------------------------------------------
void DQMOMNoInversion::problemSetup( ProblemSpecP& db ){

  m_ic_names = ArchesCore::getICNames( db );

  for ( auto i = m_ic_names.begin(); i != m_ic_names.end(); i++ ){
    std::vector<std::string> models = ArchesCore::getICModels(db, *i);
    m_ic_model_map.insert(std::make_pair(*i, models));
  }

  //Check weights separately
  std::vector<std::string> models = ArchesCore::getICModels(db, "w");
  m_ic_model_map.insert(std::make_pair("w", models));

  std::map<std::string, std::vector<std::string> > model_map;
  for ( int i = 0; i < m_N; i++ ){
    std::stringstream sQN;
    sQN << i;
    for ( auto j = m_ic_names.begin(); j != m_ic_names.end(); j++ ){
      std::vector<std::string> models = m_ic_model_map.find(*j)->second;
      std::string source_name = *j + "_qn"+sQN.str()+"_src";
      m_ic_qn_srcnames.push_back(source_name);
      std::vector<std::string> models_mod;
      for ( auto k = models.begin(); k != models.end(); k++ ){
        models_mod.push_back(*k+"_qn"+sQN.str());
      }
      model_map.insert(std::make_pair(source_name, models_mod));
    }

    //do weight separately
    std::string source_name = "w_qn"+sQN.str()+"_src";
    m_ic_qn_srcnames.push_back(source_name);
    std::vector<std::string> models = m_ic_model_map.find("w")->second;
    std::vector<std::string> models_mod;
    for ( auto k = models.begin(); k != models.end(); k++ ){
      models_mod.push_back(*k+"_qn"+sQN.str());
    }
    model_map.insert(std::make_pair(source_name, models_mod));

  }

  m_ic_model_map = model_map; //replace it with the values mapped with qn numbers.

  // for ( auto i = m_ic_model_map.begin(); i != m_ic_model_map.end(); i++){
  //   std::cout << "--------- IC = " << i->first << std::endl;
  //   for ( auto j = i->second.begin(); j != i->second.end(); j++){
  //     std::cout << "                  " << *j << std::endl;
  //   }
  // }

}

//--------------------------------------------------------------------------------------------------
void DQMOMNoInversion::create_local_labels(){

  for ( auto i = m_ic_qn_srcnames.begin(); i != m_ic_qn_srcnames.end(); i++ ){
    register_new_variable<CCVariable<double> >( *i );
  }

}

//--------------------------------------------------------------------------------------------------
void DQMOMNoInversion::register_initialize(
  std::vector<ArchesFieldContainer::VariableInformation>& variable_registry,
  const bool packed_tasks){

  for  ( auto i = m_ic_qn_srcnames.begin(); i != m_ic_qn_srcnames.end(); i++ ){

    register_variable( *i, ArchesFieldContainer::COMPUTES, variable_registry );

  }
}

//--------------------------------------------------------------------------------------------------
template<typename ExecutionSpace, typename MemSpace>
void DQMOMNoInversion::initialize( const Patch* patch, ArchesTaskInfoManager* tsk_info, ExecutionObject<ExecutionSpace, MemSpace>& executionObject ){

  for  ( auto i = m_ic_qn_srcnames.begin(); i != m_ic_qn_srcnames.end(); i++ ){
    CCVariable<double>& var = tsk_info->get_uintah_field_add<CCVariable<double> >( *i );
    var.initialize(0.0);
  }

}

//--------------------------------------------------------------------------------------------------
void DQMOMNoInversion::register_timestep_init(
  std::vector<ArchesFieldContainer::VariableInformation>& variable_registry,
  const bool packed_tasks){

  for  ( auto i = m_ic_qn_srcnames.begin(); i != m_ic_qn_srcnames.end(); i++ ){

    register_variable( *i, ArchesFieldContainer::COMPUTES, variable_registry );

  }
}

//--------------------------------------------------------------------------------------------------
template<typename ExecutionSpace, typename MemSpace> void
DQMOMNoInversion::timestep_init( const Patch* patch, ArchesTaskInfoManager* tsk_info, ExecutionObject<ExecutionSpace, MemSpace>& executionObject ){

  for  ( auto i = m_ic_qn_srcnames.begin(); i != m_ic_qn_srcnames.end(); i++ ){

    //Just allocating the memory here. The source is initialized in the eval function.
    // but we have to do something with var to avoid the compiler warning.
    CCVariable<double>& var = tsk_info->get_uintah_field_add<CCVariable<double> >(*i);
    var.initialize(0.0);

  }
}

//--------------------------------------------------------------------------------------------------
void DQMOMNoInversion::register_timestep_eval(
  std::vector<ArchesFieldContainer::VariableInformation>& variable_registry,
  const int time_substep ,
  const bool packed_tasks ){

  for  ( auto i = m_ic_qn_srcnames.begin(); i != m_ic_qn_srcnames.end(); i++ ){

    register_variable( *i, ArchesFieldContainer::MODIFIES, variable_registry, time_substep, m_task_name );

    std::vector<std::string> models = m_ic_model_map[*i];
    for ( auto imodel = models.begin(); imodel != models.end(); imodel++ ){

      register_variable( *imodel, ArchesFieldContainer::REQUIRES, 0,
                         ArchesFieldContainer::NEWDW, variable_registry,
                         time_substep, m_task_name );

    }

  }
}

//--------------------------------------------------------------------------------------------------
template<typename ExecutionSpace, typename MemSpace>
void DQMOMNoInversion::eval( const Patch* patch, ArchesTaskInfoManager* tsk_info, ExecutionObject<ExecutionSpace, MemSpace>& executionObject ){

  for  ( auto i = m_ic_qn_srcnames.begin(); i != m_ic_qn_srcnames.end(); i++ ){

    std::vector<std::string> models = m_ic_model_map[*i];
    CCVariable<double>& src = tsk_info->get_uintah_field_add<CCVariable<double> >( *i );

    //Must initialize here to deal with higher-order timestepping (otherwise the values add up over the
    //sub-steps)
    src.initialize(0.0);

    for ( auto j = models.begin(); j != models.end(); j++ ){

      constCCVariable<double>& model = tsk_info->get_const_uintah_field_add<constCCVariable<double> >(*j);

      Uintah::BlockRange range(patch->getCellLowIndex(), patch->getCellHighIndex() );
      Uintah::parallel_for( range, [&]( int i, int j, int k){
        src(i,j,k) += model(i,j,k);
      });

    }
  }
}
