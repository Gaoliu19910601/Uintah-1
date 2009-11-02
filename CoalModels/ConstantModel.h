#ifndef Uintah_Component_Arches_ConstantModel_h
#define Uintah_Component_Arches_ConstantModel_h
#include <Core/ProblemSpec/ProblemSpec.h>
#include <Core/Grid/SimulationStateP.h>
#include <CCA/Components/Arches/CoalModels/ModelBase.h>
#include <CCA/Components/Arches/CoalModels/CoalModelFactory.h>

#include <CCA/Components/Arches/ArchesVariables.h>

#include <vector>
#include <string>

//===========================================================================

namespace Uintah{

//---------------------------------------------------------------------------
// Builder
class ConstantModelBuilder: public ModelBuilder
{
public: 
  ConstantModelBuilder( const std::string         & modelName, 
                        const vector<std::string>  & reqICLabelNames,
                        const vector<std::string>  & reqScalarLabelNames,
                        const ArchesLabel          * fieldLabels,
                        SimulationStateP           & sharedState,
                        int qn );
  ~ConstantModelBuilder(); 

  ModelBase* build(); 

private:

}; 
// End Builder
//---------------------------------------------------------------------------

class ConstantModel: public ModelBase {
public: 

  ConstantModel( std::string modelName, 
                 SimulationStateP& shared_state, 
                 const ArchesLabel* fieldLabels,
                 vector<std::string> reqICLabelNames, 
                 vector<std::string> reqScalarLabelNames,
                 int qn );

  ~ConstantModel();

  /** @brief Interface for the inputfile and set constants */ 
  void problemSetup(const ProblemSpecP& db, int qn);

  /** @brief Schedule the calculation of the source term */ 
  void sched_computeModel( const LevelP& level, 
                           SchedulerP& sched, 
                           int timeSubStep );

  /** @brief Actually compute the source term */ 
  void computeModel( const ProcessorGroup* pc, 
                     const PatchSubset* patches, 
                     const MaterialSubset* matls, 
                     DataWarehouse* old_dw, 
                     DataWarehouse* new_dw );

  /** @brief  Schedule the initialization of some special/local vars */ 
  void sched_initVars( const LevelP& level, SchedulerP& sched );

  /** @brief  Actually initialize some special/local vars */
  void initVars( const ProcessorGroup * pc, 
                 const PatchSubset    * patches, 
                 const MaterialSubset * matls, 
                 DataWarehouse        * old_dw, 
                 DataWarehouse        * new_dw );

private:

  double d_constant; 
  
  const ArchesLabel* d_fieldLabels;

}; // end ConstSrcTerm
} // end namespace Uintah
#endif

