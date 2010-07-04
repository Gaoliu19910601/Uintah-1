#include <CCA/Components/Arches/CoalModels/Balachandar.h>
#include <CCA/Components/Arches/CoalModels/ParticleVelocity.h>
#include <CCA/Components/Arches/TransportEqns/EqnFactory.h>
#include <CCA/Components/Arches/TransportEqns/EqnBase.h>
#include <CCA/Components/Arches/TransportEqns/DQMOMEqn.h>
#include <CCA/Components/Arches/ArchesLabel.h>
#include <CCA/Components/Arches/Directives.h>
#include <CCA/Components/Arches/BoundaryCond_new.h>

#include <Core/ProblemSpec/ProblemSpec.h>
#include <CCA/Ports/Scheduler.h>
#include <Core/Grid/SimulationState.h>
#include <Core/Grid/Variables/VarTypes.h>
#include <Core/Grid/Variables/CCVariable.h>
#include <Core/Exceptions/InvalidValue.h>
#include <Core/Parallel/Parallel.h>

//===========================================================================

using namespace std;
using namespace Uintah; 

//---------------------------------------------------------------------------
// Builder:
BalachandarBuilder::BalachandarBuilder( const std::string         & modelName,
                                        const vector<std::string> & reqICLabelNames,
                                        const vector<std::string> & reqScalarLabelNames,
                                        const ArchesLabel         * fieldLabels,
                                        SimulationStateP          & sharedState,
                                        int qn ) :
  ModelBuilder( modelName, reqICLabelNames, reqScalarLabelNames, fieldLabels, sharedState, qn )
{
}

BalachandarBuilder::~BalachandarBuilder(){}

ModelBase* BalachandarBuilder::build() {
  return scinew Balachandar( d_modelName, d_sharedState, d_fieldLabels, d_icLabels, d_scalarLabels, d_quadNode );
}
// End Builder
//---------------------------------------------------------------------------

Balachandar::Balachandar( std::string modelName, 
                          SimulationStateP& sharedState,
                          const ArchesLabel* fieldLabels,
                          vector<std::string> icLabelNames, 
                          vector<std::string> scalarLabelNames,
                          int qn ) 
: ParticleVelocity(modelName, sharedState, fieldLabels, icLabelNames, scalarLabelNames, qn)
{
  // particle velocity label is created in parent class
}

Balachandar::~Balachandar()
{}

//---------------------------------------------------------------------------
// Method: Problem Setup
//---------------------------------------------------------------------------
  void 
Balachandar::problemSetup(const ProblemSpecP& params)
{
  // call parent's method first
  ParticleVelocity::problemSetup(params);

  ProblemSpecP db = params; 

  // no longer using <kinematic_viscosity> b/c grabbing kinematic viscosity from physical properties section
  // no longer using <rho_ratio> b/c density obtained dynamically
  // no longer using <beta> because density (and beta) will be obtained dynamically
  db->getWithDefault("iter", d_totIter, 15);
  db->getWithDefault("tol", d_tol, 1e-15);
  db->getWithDefault("L", d_L, 1.0);
  db->getWithDefault("eta", d_eta, 1e-5);
  db->getWithDefault("regime", regime, 1);
  db->getWithDefault("min_vel_ratio", d_min_vel_ratio, 0.1);

  if (regime==1) {
    d_power = 1;
  } else if (regime == 2) {
    d_power = 0.5;
  } else if (regime == 3) {
    d_power = 1.0/3.0;
  }

  string label_name;
  string role_name;
  string temp_label_name;
  
  string temp_ic_name;
  string temp_ic_name_full;

  // -----------------------------------------------------------------
  // Look for required internal coordinates
  ProblemSpecP db_icvars = params->findBlock("ICVars");
  if (db_icvars) {
    for (ProblemSpecP variable = db_icvars->findBlock("variable"); variable != 0; variable = variable->findNextBlock("variable") ) {

      variable->getAttribute("label",label_name);
      variable->getAttribute("role", role_name);

      temp_label_name = label_name;
      
      std::stringstream out;
      out << d_quadNode;
      string node = out.str();
      temp_label_name += "_qn";
      temp_label_name += node;

      // user specifies "role" of each internal coordinate

      if ( role_name == "particle_length" ) {
        LabelToRoleMap[temp_label_name] = role_name;
        d_useLength = true;
      } else {
        std::string errmsg;
        errmsg = "ERROR: Arches: Invalid variable role for Balachandar particle velocity model: ";
        errmsg += "must be \"particle_length\", \"u_velocity\", \"v_velocity\", or \"w_velocity\", you specified \"" + role_name + "\".";
        throw ProblemSetupException(errmsg,__FILE__,__LINE__);
      }
    }
  }

  // fix the d_icLabels to point to the correct quadrature node (since there is 1 model per quad node)
  for ( vector<std::string>::iterator iString = d_icLabels.begin(); 
        iString != d_icLabels.end(); ++iString) {

    temp_ic_name        = *iString;
    temp_ic_name_full   = temp_ic_name;

    std::stringstream out;
    out << d_quadNode;
    string node = out.str();
    temp_ic_name_full += "_qn";
    temp_ic_name_full += node;

    std::replace( d_icLabels.begin(), d_icLabels.end(), temp_ic_name, temp_ic_name_full);
  }
 
  // -----------------------------------------------------------------
  // Look for required scalars
  // (Not used by Balachandar)
  /*
  ProblemSpecP db_scalarvars = params->findBlock("scalarVars");
  if (db_scalarvars) {
    for( ProblemSpecP variable = db_scalarvars->findBlock("variable");
         variable != 0; variable = variable->findNextBlock("variable") ) {

      variable->getAttribute("label", label_name);
      variable->getAttribute("role",  role_name);

      // user specifies "role" of each scalar
      // if it isn't an internal coordinate or a scalar, it's required explicitly
      // ( see comments in Arches::registerModels() for details )
      if ( role_name == "length") {
        LabelToRoleMap[label_name] = role_name;
      } else {
        std::string errmsg;
        errmsg = "Invalid variable role for Balachandar particle velocity model: must be \"length\", you specified \"" + role_name + "\".";
        throw InvalidValue(errmsg,__FILE__,__LINE__);
      }
    }
  }
  */

  if(!d_useLength) {
    string errmsg = "ERROR: Arches: Balachandar: No particle length internal coordinate was specified.  Quitting...";
    throw ProblemSetupException(errmsg,__FILE__,__LINE__);
  }


  ///////////////////////////////////////////


  DQMOMEqnFactory& dqmom_eqn_factory = DQMOMEqnFactory::self();
  EqnFactory& eqn_factory = EqnFactory::self();

  // assign labels for each internal coordinate
  for( map<string,string>::iterator iter = LabelToRoleMap.begin();
       iter != LabelToRoleMap.end(); ++iter ) {

    EqnBase* current_eqn;
    if( dqmom_eqn_factory.find_scalar_eqn(iter->first) ) {
      current_eqn = &(dqmom_eqn_factory.retrieve_scalar_eqn(iter->first));
    } else if( eqn_factory.find_scalar_eqn(iter->first) ) {
      current_eqn = &(eqn_factory.retrieve_scalar_eqn(iter->first));
    } else {
      string errmsg = "ERROR: Arches: Balachandar: Invalid variable \"" + iter->first + 
                      "\" given for \""+iter->second+"\" role, could not find in EqnFactory or DQMOMEqnFactory!";
      throw ProblemSetupException(errmsg,__FILE__,__LINE__);
    }


    if( iter->second == "particle_length" ){
      d_length_label = current_eqn->getTransportEqnLabel();
      d_length_scaling_factor = current_eqn->getScalingConstant();
    } else {
      // can't find this required variable in the labels-to-roles map!
      std::string errmsg = "ERROR: Arches: Balachandar: You specified that the variable \"" + iter->first + 
                           "\" was required, but you did not specify a valid role for it! (You specified \"" + iter->second + "\"\n";
      throw ProblemSetupException( errmsg, __FILE__, __LINE__);
    }
  }

  // // set model clipping (not used)
  //db->getWithDefault( "low_clip", d_lowModelClip,   1.0e-6 );
  //db->getWithDefault( "high_clip", d_highModelClip, 999999 );  

}


//---------------------------------------------------------------------------
// Method: Schedule the calculation of the Model 
//---------------------------------------------------------------------------
void 
Balachandar::sched_computeModel( const LevelP& level, SchedulerP& sched, int timeSubStep )
{
  std::string taskname = "Balachandar::computeModel";
  Task* tsk = scinew Task(taskname, this, &Balachandar::computeModel, timeSubStep);

  //Ghost::GhostType gn = Ghost::None;

  if( timeSubStep == 0 && !d_labelSchedInit) {
    // Every model term needs to set this flag after the varLabel is computed. 
    // transportEqn.cleanUp should reinitialize this flag at the end of the time step. 
    d_labelSchedInit = true;
  }
  
  if( timeSubStep == 0 ) {
    tsk->computes(d_modelLabel);
    tsk->computes(d_gasLabel); 
  } else {
    tsk->modifies(d_modelLabel);
    tsk->modifies(d_gasLabel);  
  }

  sched->addTask(tsk, level->eachPatch(), d_sharedState->allArchesMaterials()); 

}

//---------------------------------------------------------------------------
// Method: Actually compute the source term 
//---------------------------------------------------------------------------
/**
@details
The Balachandar model doesn't have any model term G associated with
 it (since it is not a source term for an internal coordinate).
Therefore, the computeModel() method simply sets this model term
 equal to zero.
*/
void
Balachandar::computeModel( const ProcessorGroup * pc, 
                           const PatchSubset    * patches, 
                           const MaterialSubset * matls, 
                           DataWarehouse        * old_dw, 
                           DataWarehouse        * new_dw,
                           int timeSubStep )
{
  for( int p=0; p < patches->size(); p++ ) {

    const Patch* patch = patches->get(p);
    int archIndex = 0;
    int matlIndex = d_fieldLabels->d_sharedState->getArchesMaterial(archIndex)->getDWIndex(); 

    CCVariable<Vector> model;
    CCVariable<Vector> model_gasSource;

    if( timeSubStep == 0 ) {
      new_dw->allocateAndPut( model, d_modelLabel, matlIndex, patch );
      new_dw->allocateAndPut( model_gasSource, d_gasLabel, matlIndex, patch ); 
    } else {
      new_dw->getModifiable( model, d_modelLabel, matlIndex, patch ); 
      new_dw->getModifiable( model_gasSource, d_gasLabel, matlIndex, patch ); 
    }

    model.initialize( Vector(0.0,0.0,0.0) );
    model_gasSource.initialize( Vector(0.0,0.0,0.0) );

  }//end patch loop
}

void
Balachandar::sched_computeParticleVelocity( const LevelP& level,
                                            SchedulerP&   sched,
                                            const int timeSubStep )
{
  string taskname = "Balachandar::computeParticleVelocity";
  Task* tsk = scinew Task(taskname, this, &Balachandar::computeParticleVelocity, timeSubStep);

  Ghost::GhostType gn = Ghost::None;

  CoalModelFactory& coal_model_factory = CoalModelFactory::self();

  const VarLabel* density_label = coal_model_factory.getParticleDensityLabel(d_quadNode);

  if( timeSubStep == 0 ) {

    tsk->computes( d_velocity_label );

    tsk->requires( Task::OldDW, d_fieldLabels->d_newCCVelocityLabel, Ghost::None, 0); // gas velocity
    tsk->requires( Task::OldDW, d_fieldLabels->d_densityCPLabel, Ghost::None, 0); // gas density
    tsk->requires( Task::OldDW, density_label, gn, 0); // density label for this environment
    tsk->requires( Task::OldDW, d_weight_label, gn, 0); // require weight label
    tsk->requires( Task::OldDW, d_length_label, Ghost::None, 0); // require internal coordinates

  } else {

    tsk->modifies( d_velocity_label );

    tsk->requires( Task::NewDW, d_fieldLabels->d_newCCVelocityLabel, Ghost::None, 0); // gas velocity
    tsk->requires( Task::NewDW, d_fieldLabels->d_densityCPLabel, Ghost::None, 0); // gas density
    tsk->requires( Task::NewDW, density_label, gn, 0); // density label for this environment
    tsk->requires( Task::NewDW, d_weight_label, gn, 0); // require weight label
    tsk->requires( Task::NewDW, d_length_label, Ghost::None, 0); // require internal coordinates

  }

  sched->addTask(tsk, level->eachPatch(), d_fieldLabels->d_sharedState->allArchesMaterials());
}


void
Balachandar::computeParticleVelocity( const ProcessorGroup* pc,
                                      const PatchSubset*    patches,
                                      const MaterialSubset* matls,
                                      DataWarehouse*        old_dw,
                                      DataWarehouse*        new_dw,
                                      int timeSubStep )
{
  for( int p=0; p < patches->size(); p++ ) {  // Patch loop

    Ghost::GhostType  gn  = Ghost::None;

    const Patch* patch = patches->get(p);
    int archIndex = 0;
    int matlIndex = d_fieldLabels->d_sharedState->getArchesMaterial(archIndex)->getDWIndex(); 

    CoalModelFactory& coal_model_factory = CoalModelFactory::self();

    constCCVariable<double> weight;
    constCCVariable<double> wtd_length;
    constCCVariable<double> gas_density;
    constCCVariable<double> particle_density;
    constCCVariable<Vector> gas_velocity;
    CCVariable<Vector> particle_velocity;

    const VarLabel* particle_density_label = coal_model_factory.getParticleDensityLabel(d_quadNode);

    if( timeSubStep == 0 ) {

      old_dw->get( weight, d_weight_label, matlIndex, patch, gn, 0 );
      old_dw->get( wtd_length, d_length_label, matlIndex, patch, gn, 0 );

      old_dw->get( gas_density, d_fieldLabels->d_densityCPLabel, matlIndex, patch, gn, 0 );
      old_dw->get( particle_density, particle_density_label, matlIndex, patch, gn, 0 );

      old_dw->get( gas_velocity, d_fieldLabels->d_newCCVelocityLabel, matlIndex, patch, gn, 0 );

      new_dw->allocateAndPut( particle_velocity, d_velocity_label, matlIndex, patch );
      particle_velocity.initialize( Vector(0.0,0.0,0.0) );

    } else {

      new_dw->get( weight, d_weight_label, matlIndex, patch, gn, 0 );
      new_dw->get( wtd_length, d_length_label, matlIndex, patch, gn, 0 );

      new_dw->get( gas_density, d_fieldLabels->d_densityCPLabel, matlIndex, patch, gn, 0 );
      new_dw->get( particle_density, particle_density_label, matlIndex, patch, gn, 0 );

      new_dw->get( gas_velocity, d_fieldLabels->d_newCCVelocityLabel, matlIndex, patch, gn, 0 );

      new_dw->getModifiable( particle_velocity, d_velocity_label, matlIndex, patch );

    }

    for (CellIterator iter=patch->getCellIterator(); !iter.done(); iter++){
      IntVector c = *iter; 

      Vector gasVel = gas_velocity[c];

      if( weight[c] < d_w_small ) {
        particle_velocity[c] = gasVel;

      } else {

        if( gas_density[c] > TINY ) {
          rhoRatio = particle_density[c]/gas_density[c];
          //rhoRatio = particle_density/gas_density[c];
        } else {
          rhoRatio = particle_density[c]/TINY;
          //rhoRatio = particle_density/TINY;
        }

        double length = (wtd_length[c]/weight[c])*d_length_scaling_factor;

        double length_ratio = length/d_eta;

        //double epsilon = pow(gasVel.length(),3) / d_L;

        double uk = 0.0;
        if( length>0.0 ) {
          uk = pow(d_eta/d_L, 1.0/3.0);
          uk *= gasVel.length();
        }

        double diff = 0.0;
        double prev_diff = 0.0;

        // now iterate to convergence
        for (int iter=0; iter<d_totIter; ++iter) {
          prev_diff = diff;
          double Re = fabs(diff)*length / kvisc;
          double phi = 1.0 + 0.15*pow(Re, 0.687);
          double t_p_by_t_k = ( (2*rhoRatio + 1)/36.0 )*( 1/phi )*( pow(length_ratio,2) );

          diff = uk*(1-beta)*pow(t_p_by_t_k, d_power);
          double error = abs(diff - prev_diff)/diff;

          if( abs(diff) < 1e-16 ) {
            error = 0.0;
          }
          if( abs(error) < d_tol ) {
            break;
          }
        }

        double newPartMag = gasVel.length() - diff;

        particle_velocity[c] = ( gasVel.safe_normalize() )*(newPartMag);

      }//end if weight is small

    }//end cell iterator

    // Now apply boundary conditions
    if (d_gasBC) {
      // assume particle velocity = gas velocity at boundary
      // DON'T DO THIS, IT'S WRONG!
      d_boundaryCond->setVectorValueBC( 0, patch, particle_velocity, gas_velocity, d_velocity_label->getName() );
    } else {
      // Particle velocity at boundary is set by user
      d_boundaryCond->setVectorValueBC( 0, patch, particle_velocity, d_velocity_label->getName() );
    }

  }//end patch loop
}

