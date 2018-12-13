#ifndef Uintah_Component_Arches_SurfaceNormals_h
#define Uintah_Component_Arches_SurfaceNormals_h

#include <CCA/Components/Arches/Task/TaskInterface.h>

namespace Uintah{

  class SurfaceNormals : public TaskInterface {

public:

    SurfaceNormals( std::string task_name, int matl_index );
    ~SurfaceNormals();

    TaskAssignedExecutionSpace loadTaskComputeBCsFunctionPointers();

    TaskAssignedExecutionSpace loadTaskInitializeFunctionPointers();

    TaskAssignedExecutionSpace loadTaskEvalFunctionPointers();

    TaskAssignedExecutionSpace loadTaskTimestepInitFunctionPointers();

    TaskAssignedExecutionSpace loadTaskRestartInitFunctionPointers();

    void problemSetup( ProblemSpecP& db );

    void register_initialize(
      std::vector<ArchesFieldContainer::VariableInformation>& variable_registry,
      const bool packed_tasks );

    void register_timestep_init(
      std::vector<ArchesFieldContainer::VariableInformation>& variable_registry,
      const bool packed_tasks );

    void register_timestep_eval(
      std::vector<ArchesFieldContainer::VariableInformation>& variable_registry,
      const int time_substep, const bool packed_tasks ){};

    void register_compute_bcs(
      std::vector<ArchesFieldContainer::VariableInformation>& variable_registry,
      const int time_substep,
      const bool packed_tasks ){};

    template <typename ExecutionSpace, typename MemSpace>
    void compute_bcs( const Patch* patch, ArchesTaskInfoManager* tsk_info, ExecutionObject<ExecutionSpace, MemSpace>& execObj ){}

    template <typename ExecutionSpace, typename MemSpace>
    void initialize( const Patch* patch, ArchesTaskInfoManager* tsk_info, ExecutionObject<ExecutionSpace, MemSpace>& execObj );

    template<typename ExecutionSpace, typename MemSpace> void timestep_init( const Patch* patch, ArchesTaskInfoManager* tsk_info, ExecutionObject<ExecutionSpace,MemSpace>& execObj);

    template <typename ExecutionSpace, typename MemSpace>
    void eval( const Patch* patch, ArchesTaskInfoManager* tsk_info, ExecutionObject<ExecutionSpace, MemSpace>& execObj ){}

    void create_local_labels();


    //Build instructions for this (SurfaceNormals) class.
    class Builder : public TaskInterface::TaskBuilder {

      public:

      Builder( std::string task_name, int matl_index ) : m_task_name(task_name), m_matl_index(matl_index){}
      ~Builder(){}

      SurfaceNormals* build()
      { return scinew SurfaceNormals( m_task_name, m_matl_index ); }

      private:

      std::string m_task_name;
      int m_matl_index;

    };

private:

    double _value;

  };
}
#endif
