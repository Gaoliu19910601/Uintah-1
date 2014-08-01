#ifndef Uintah_Component_Arches_SampleTask_h
#define Uintah_Component_Arches_SampleTask_h

#include <CCA/Components/Arches/Task/TaskInterface.h>

namespace Uintah{ 

  class Operators; 
  class SampleTask : public TaskInterface { 

public: 

    SampleTask( std::string task_name, int matl_index ); 
    ~SampleTask(); 

    void problemSetup( ProblemSpecP& db ); 

    void register_all_variables( std::vector<VariableInformation>& variable_registry, const int time_substep ); 

    void register_initialize( std::vector<VariableInformation>& variable_registry );

    void eval( const Patch* patch, FieldCollector* field_collector, 
               SpatialOps::OperatorDatabase& opr, 
               SchedToTaskInfo& info ); 

    void initialize( const Patch* patch, FieldCollector* field_collector, 
                     SpatialOps::OperatorDatabase& opr );

    //Build instructions for this (SampleTask) class. 
    class Builder : public TaskInterface::TaskBuilder { 

      public: 

      Builder( std::string task_name, int matl_index ) : _task_name(task_name), _matl_index(matl_index){}
      ~Builder(){}

      SampleTask* build()
      { return scinew SampleTask( _task_name, _matl_index ); }

      private: 

      std::string _task_name; 
      int _matl_index; 

    };

private: 

    double _value; 
  
  };
}
#endif 
