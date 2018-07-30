#include <CCA/Components/Arches/PropertyModelsV2/partRadProperties.h>
#include <CCA/Components/Arches/Radiation/RadPropertyCalculator.h>
#include <CCA/Components/Arches/BoundaryCond_new.h>
#include <CCA/Components/Arches/ChemMix/ChemHelper.h>
#include <CCA/Components/Arches/ParticleModels/ParticleTools.h>
#include <CCA/Components/Arches/ChemMixV2/ClassicTableUtility.h>

#include <Core/Parallel/Portability.h>
#include <Core/Parallel/LoopExecution.hpp>
// includes for Arches
#define MAX_DQMOM_ENV 3
using namespace Uintah;

//---------------------------------------------------------------------------
////Method: Constructor
////---------------------------------------------------------------------------
partRadProperties::partRadProperties( std::string task_name, int matl_index ) : TaskInterface( task_name, matl_index)
{
}

//---------------------------------------------------------------------------
//Method: Destructor
//---------------------------------------------------------------------------
partRadProperties::~partRadProperties( )
{
  // Destroying all local VarLabels stored in _extra_local_labels:

#ifdef HAVE_RADPROPS
if (_particle_calculator_type == "constantCIF"){
 delete _part_radprops;
}

if (_particle_calculator_type == "coal"){
  delete _3Dpart_radprops;
}
#endif

 delete myTable;
}

//---------------------------------------------------------------------------
//Method: Load task function pointers for portability
//---------------------------------------------------------------------------
TaskAssignedExecutionSpace partRadProperties::loadTaskEvalFunctionPointers(){

  return create_portable_arches_tasks( this,
                                       &partRadProperties::eval<UINTAH_CPU_TAG>,
                                       &partRadProperties::eval<KOKKOS_OPENMP_TAG>,
                                       &partRadProperties::eval<KOKKOS_CUDA_TAG>);

}

//---------------------------------------------------------------------------
//Method: Problem Setup
//---------------------------------------------------------------------------
void partRadProperties::problemSetup(  Uintah::ProblemSpecP& db )
{


  ProblemSpecP db_calc = db->findBlock("calculator");
  db->getWithDefault("absorption_modifier",_absorption_modifier,1.0);

    _scatteringOn = false;
    _isCoal = false;

    //------------ check to see if scattering is turned on --//
    ProblemSpecP db_source = db->getRootNode()->findBlock("CFD")->findBlock("ARCHES")->findBlock("TransportEqns")->findBlock("Sources") ;
    for ( ProblemSpecP db_src = db_source->findBlock("src"); db_src != nullptr;
        db_src = db_src->findNextBlock("src")){
      std::string radiation_model;
      db_src->getAttribute("type", radiation_model);
      if (radiation_model == "do_radiation"){
        db_src->findBlock("DORadiationModel")->getWithDefault("ScatteringOn" ,_scatteringOn,false) ;
        break;
      }
      else if ( radiation_model == "rmcrt_radiation"){
        //db->findBlock("RMCRT")->getWithDefault("ScatteringOn" ,_scatteringOn,false) ;
        _scatteringOn=false ;
        break;
      }
    }
    //-------------------------------------------------------//

    db->require( "subModel", _particle_calculator_type);
    if (_particle_calculator_type=="tabulated"){
      std::string table_directory;
      db->require("TablePath",table_directory);
      _nIVs=2; // two independent variables, radius and tempreature
      _nDVs= _scatteringOn ? 2 : 1; // two independent variables, radius and tempreature
      db->getWithDefault("const_asymmFact",_constAsymmFact,0.0);
      std::string which_model = "none";
      db->require("model_type", which_model);
      _p_planck_abskp = false;
      _p_ros_abskp = false;
      std::string prefix;
      if ( which_model == "planck" ){
        _p_planck_abskp = true;
        prefix="planck";
      } else if ( which_model == "rossland" ){
        _p_ros_abskp = true;
        prefix="ross";
      } else {
        throw InvalidValue( "Error: Particle model not recognized for abskp.",__FILE__,__LINE__);
      }

      std::vector<std::string> dep_vars;
      dep_vars.push_back(prefix+"_abs"); // must push_back _abs first or fields will be mismatched downstream
      if (_scatteringOn){
        dep_vars.push_back(prefix+"_scat");
      }

      myTable = SCINEW_ClassicTable<DEP_VAR_SIZE>(table_directory,dep_vars);

    }else if(_particle_calculator_type == "basic"){
      if (_scatteringOn){
        throw ProblemSetupException("Scattering not enabled for basic-radiative-particle-properties.  Use radprops, a table with scattering coefficients, OR turn off scattering!",__FILE__, __LINE__);
      }
      db->getWithDefault("Qabs",_Qabs,0.8);
    }else if(_particle_calculator_type == "coal"){
#ifdef HAVE_RADPROPS
    _isCoal = true;
      //throw ProblemSetupException("Error: The model for Coal radiation properties is incomplete, please alternative model.",__FILE__,__LINE__);
      _ncomp=2;
      ProblemSpecP db_coal=db->getRootNode()->findBlock("CFD")->findBlock("ARCHES")->findBlock("ParticleProperties");

      if (db_coal == nullptr){
        throw ProblemSetupException("Error: Coal properties not found! Need Optical Coal properties!",__FILE__, __LINE__);
      }else if (db_coal->findBlock("optics")==nullptr){
        throw ProblemSetupException("Error: Coal properties not found! Need Optical Coal properties!",__FILE__, __LINE__);
      }

      db_coal->findBlock("optics")->require( "RawCoal_real", _rawCoalReal );
      db_coal->findBlock("optics")->require( "RawCoal_imag", _rawCoalImag );
      db_coal->findBlock("optics")->require( "Ash_real", _ashReal );
      db_coal->findBlock("optics")->require( "Ash_imag", _ashImag );

      _charReal=_rawCoalReal; // assume char and RC have same optical props
      _charImag=_rawCoalImag; // assume char and RC have same optical props

      if (_rawCoalReal > _ashReal) {
        _HighComplex=std::complex<double> ( _rawCoalReal, _rawCoalImag );
        _LowComplex=std::complex<double> ( _ashReal, _ashImag );
      } else{
        _HighComplex=std::complex<double> ( _ashReal, _ashImag );
        _LowComplex=std::complex<double>  (_rawCoalReal, _rawCoalImag );
      }

      /// complex index of refraction for pure coal components
      ///  asymmetry parameters for pure coal components
      _charAsymm=1.0;
      _rawCoalAsymm=1.0;
      _ashAsymm=-1.0;

      std::string which_model = "none";
      db->require("model_type", which_model);
        _p_planck_abskp = false;
        _p_ros_abskp = false;
      if ( which_model == "planck" ){
        _p_planck_abskp = true;
      } else if ( which_model == "rossland" ){
        _p_ros_abskp = true;
      } else {
        throw InvalidValue( "Error: Particle model not recognized for abskp.",__FILE__,__LINE__);
      }
     _3Dpart_radprops = scinew RadProps::ParticleRadCoeffs3D( _LowComplex, _HighComplex,3, 1e-6, 3e-4, 10  );
     _ash_mass_v = std::vector<double>(_nQn_part);        /// particle sizes in diameters
  //--------------- Get initial ash mass -----------//
     double density;
     db->findBlock("abskg")->getRootNode()->findBlock("CFD")->findBlock("ARCHES")->findBlock("ParticleProperties")->require( "density", density );

     std::vector<double>  particle_sizes ;        /// particle sizes in diameters
     db->findBlock("abskg")->getRootNode()->findBlock("CFD")->findBlock("ARCHES")->findBlock("ParticleProperties")->require( "diameter_distribution", particle_sizes );

     double ash_massfrac;
     db->findBlock("abskg")->getRootNode()->findBlock("CFD")->findBlock("ARCHES")->findBlock("ParticleProperties")->findBlock("ultimate_analysis")->require("ASH", ash_massfrac);

     for (int i=0; i< _nQn_part ; i++ ){
       _ash_mass_v[i] = pow(particle_sizes[i], 3.0)/6*M_PI*density*ash_massfrac;
     }
  //------------------------------------------------//
#else
      throw ProblemSetupException("Error: Use of the Coal radiation properties model requires compilation with Radprops and TabProps.",__FILE__,__LINE__);
#endif
    }else if(_particle_calculator_type == "constantCIF"){
#ifdef HAVE_RADPROPS
      double realCIF;
      double imagCIF;
      db->require("complex_ir_real",realCIF);
      db->require("complex_ir_imag",imagCIF);
      db->getWithDefault("const_asymmFact",_constAsymmFact,0.0);
      std::complex<double>  CIF(realCIF, imagCIF );
      _part_radprops = scinew RadProps::ParticleRadCoeffs(CIF,1e-6,3e-4,10);
      std::string which_model = "none";
      db->require("model_type", which_model);
        _p_planck_abskp = false;
        _p_ros_abskp = false;
      if ( which_model == "planck" ){
        _p_planck_abskp = true;
      } else if ( which_model == "rossland" ){
        _p_ros_abskp = true;
      } else {
        throw InvalidValue( "Error: Particle model not recognized for abskp.",__FILE__,__LINE__);
      }
#else
      throw ProblemSetupException("Error: Use of the constCIF radiation properties model requires compilation with Radprops and TabProps.",__FILE__,__LINE__);
#endif

    }else{
      throw InvalidValue("Particle radiative property model not found!! Name:"+_particle_calculator_type,__FILE__, __LINE__);
    }

    _nQn_part = 0;
    bool doing_dqmom = ArchesCore::check_for_particle_method(db,ArchesCore::DQMOM_METHOD);
    bool doing_cqmom = ArchesCore::check_for_particle_method(db,ArchesCore::CQMOM_METHOD);

    if ( doing_dqmom ){
      _nQn_part = ArchesCore::get_num_env( db, ArchesCore::DQMOM_METHOD );
    } else if ( doing_cqmom ){
      _nQn_part = ArchesCore::get_num_env( db, ArchesCore::CQMOM_METHOD );
    } else {
      throw ProblemSetupException("Error: This particle radiation property method only supports DQMOM/CQMOM.",__FILE__,__LINE__);
    }

    if (_nQn_part > MAX_DQMOM_ENV){
      throw ProblemSetupException("Portability Error:Number of DQMOM environments exceeds compile time limits.",__FILE__,__LINE__);
    }
    std::string base_temperature_name = ArchesCore::parse_for_particle_role_to_label( db, ArchesCore::P_TEMPERATURE);
    std::string base_size_name        = ArchesCore::parse_for_particle_role_to_label( db, ArchesCore::P_SIZE );
    std::string base_weight_name      = "w"; //hard coded as w
    std::string char_name = "Charmass";
    std::string RC_names = "RCmass";



    for (int i=0; i<_nQn_part; i++){
      _size_name_v.push_back(ArchesCore::append_env( base_size_name, i));
      _temperature_name_v.push_back (ArchesCore::append_env( base_temperature_name, i ));
      _weight_name_v.push_back(ArchesCore::append_env( base_weight_name, i ));
      _RC_name_v.push_back(ArchesCore::append_env( RC_names, i ));
      _Char_name_v.push_back (ArchesCore::append_env( char_name, i ));
    }


   db->getAttribute("label",_abskp_name);
  _abskp_name_vector = std::vector<std::string> (_nQn_part);
  for (int i=0; i< _nQn_part ; i++){
    std::stringstream out;
    out << _abskp_name << "_" << i;
    _abskp_name_vector[i] = out.str();
  }

    _asymmetryParam_name="asymmetryParam";
    _scatkt_name = "scatkt";

}


void
partRadProperties::create_local_labels(){

  register_new_variable<CCVariable<double> >(_abskp_name);
  for (int i=0; i< _nQn_part ; i++){
    register_new_variable<CCVariable<double> >(_abskp_name_vector[i]);
  }
  if (_scatteringOn ){
    register_new_variable<CCVariable<double> >(_scatkt_name);
    register_new_variable<CCVariable<double> >(_asymmetryParam_name);
  }

  for (int i=0; i<_nDVs; i++){
    register_new_variable<CCVariable<double> >("partRadProps_temporary_"+std::to_string(i));
  } 

}



void
partRadProperties::register_initialize( VIVec& variable_registry , const bool pack_tasks){
  register_variable( _abskp_name , Uintah::ArchesFieldContainer::COMPUTES, variable_registry);
  for (int i=0; i< _nQn_part ; i++){
    register_variable( _abskp_name_vector[i] , Uintah::ArchesFieldContainer::COMPUTES, variable_registry);
  }
  if (_scatteringOn ){
    register_variable( _scatkt_name , Uintah::ArchesFieldContainer::COMPUTES, variable_registry);
    register_variable( _asymmetryParam_name , Uintah::ArchesFieldContainer::COMPUTES , variable_registry);
  }


}

void
partRadProperties::initialize( const Patch* patch, ArchesTaskInfoManager* tsk_info ){


  CCVariable<double>& abskp = *(tsk_info->get_uintah_field<CCVariable<double> >(_abskp_name));
  abskp.initialize(0.0);
  for (int i=0; i< _nQn_part ; i++){
    CCVariable<double>& abskpQuad = *(tsk_info->get_uintah_field<CCVariable<double> >(_abskp_name_vector[i]));
    abskpQuad.initialize(0.0);
  }
  if (_scatteringOn ){
    CCVariable<double>& scatkt = *(tsk_info->get_uintah_field<CCVariable<double> >(_scatkt_name));
    scatkt.initialize(0.0);
    CCVariable<double>& asymm = *(tsk_info->get_uintah_field<CCVariable<double> >(_asymmetryParam_name));
    asymm.initialize(0.0);
  }
}

void partRadProperties::register_restart_initialize( VIVec& variable_registry , const bool packed_tasks){

}

void partRadProperties::restart_initialize( const Patch* patch, ArchesTaskInfoManager* tsk_info ){

}

void partRadProperties::register_timestep_init( VIVec& variable_registry , const bool packed_tasks){

}


void partRadProperties::timestep_init( const Patch* patch, ArchesTaskInfoManager* tsk_info ){

}

void
partRadProperties::register_timestep_eval( std::vector<ArchesFieldContainer::VariableInformation>& variable_registry, const int time_substep , const bool packed_tasks){

  register_variable( _abskp_name , Uintah::ArchesFieldContainer::COMPUTES, variable_registry, time_substep);

  for (int i=0; i< _nQn_part ; i++){
    register_variable( _abskp_name_vector[i] , Uintah::ArchesFieldContainer::COMPUTES, variable_registry, time_substep);

    register_variable( _temperature_name_v[i] , Uintah::ArchesFieldContainer::REQUIRES, variable_registry, time_substep);
    register_variable( _size_name_v[i] , Uintah::ArchesFieldContainer::REQUIRES, variable_registry, time_substep);
    register_variable( _weight_name_v[i] , Uintah::ArchesFieldContainer::REQUIRES, variable_registry, time_substep);

  if(_isCoal){
      register_variable( _RC_name_v[i] , Uintah::ArchesFieldContainer::REQUIRES, variable_registry, time_substep);
      register_variable( _Char_name_v[i] , Uintah::ArchesFieldContainer::REQUIRES, variable_registry, time_substep);
  }

  }
  if (_scatteringOn ){
    register_variable( _scatkt_name , Uintah::ArchesFieldContainer::COMPUTES, variable_registry, time_substep);
    register_variable( _asymmetryParam_name , Uintah::ArchesFieldContainer::COMPUTES , variable_registry, time_substep);
  }

    for (int i=0; i<_nDVs; i++){
    register_variable( "partRadProps_temporary_"+std::to_string(i) , Uintah::ArchesFieldContainer::COMPUTES, variable_registry, time_substep);
    } 

  register_variable("volFraction" , ArchesFieldContainer::REQUIRES,0,ArchesFieldContainer::NEWDW,variable_registry, time_substep );

}



template<typename ExecutionSpace, typename MemorySpace> void
partRadProperties::eval( const Patch* patch, ArchesTaskInfoManager* tsk_info, ExecutionObject<ExecutionSpace, MemorySpace>& executionObject ){
  auto abskp = createContainer<CCVariable<double>, double, 1 , MemorySpace>(1);
  auto scatkt =  createContainer<CCVariable<double>, double, 1 , MemorySpace>(_scatteringOn ? 1 :0);
  auto asymm   =  createContainer<CCVariable<double>, double, 1 , MemorySpace>(_scatteringOn ? 1 :0);
  const int TSS = tsk_info->get_time_substep();
  //auto abskp           = tsk_info->get_uintah_field_add<T, double, MemorySpace>(_abskp_name, abskp, _matl_index, _new_dw_time_substep);
  tsk_info->get_unmanaged_uintah_field<CCVariable<double>, double , MemorySpace >(abskp[0] ,_abskp_name,patch->getID(),_matl_index,1);
  if (_scatteringOn){
    tsk_info->get_unmanaged_uintah_field<CCVariable<double>, double , MemorySpace >(scatkt[0],_scatkt_name,patch->getID(),_matl_index,TSS);
    tsk_info->get_unmanaged_uintah_field<CCVariable<double>, double , MemorySpace >(asymm[0],_asymmetryParam_name,patch->getID(),_matl_index,TSS);
  }

  auto vol_fraction   = tsk_info->get_const_uintah_field_add<constCCVariable<double>,const double, MemorySpace>("volFraction",patch->getID() , _matl_index, 1);

  auto RC_mass =  createContainer<constCCVariable<double>, const double, MAX_DQMOM_ENV , MemorySpace>(_nQn_part);
  auto Char_mass =  createContainer<constCCVariable<double>, const double, MAX_DQMOM_ENV , MemorySpace>(_nQn_part);
  auto weightQuad =  createContainer<constCCVariable<double>, const double, MAX_DQMOM_ENV , MemorySpace>(_nQn_part);
  auto sizeQuad =  createContainer<constCCVariable<double>, const double, MAX_DQMOM_ENV , MemorySpace>(_nQn_part);
  auto temperatureQuad =  createContainer<constCCVariable<double>, const double, MAX_DQMOM_ENV , MemorySpace>(_nQn_part);

  for (int ix=0; ix< _nQn_part ; ix++){
    if (_isCoal){
      RC_mass[ix]  =tsk_info->get_const_uintah_field_add<constCCVariable<double>, const double, MemorySpace >(_RC_name_v[ix]  ,patch->getID(),_matl_index,TSS);
      Char_mass[ix]=tsk_info->get_const_uintah_field_add<constCCVariable<double>, const double, MemorySpace >(_Char_name_v[ix],patch->getID(),_matl_index,TSS);
    }
      weightQuad[ix]  = tsk_info->get_const_uintah_field_add<constCCVariable<double> , const double, MemorySpace>(_weight_name_v[ix],patch->getID(),_matl_index,1);
      temperatureQuad[ix] = tsk_info->get_const_uintah_field_add<constCCVariable<double> , const double, MemorySpace>(_temperature_name_v[ix],patch->getID(),_matl_index,1);
      sizeQuad[ix] = tsk_info->get_const_uintah_field_add<constCCVariable<double> , const double, MemorySpace>(_size_name_v[ix],patch->getID(),_matl_index,1);
  }


  Uintah::BlockRange range(patch->getCellLowIndex(),patch->getCellHighIndex());
  auto abskpQuad =  createContainer<CCVariable<double>, double, MAX_DQMOM_ENV , MemorySpace>(_nQn_part);
  for (int i=0; i< _nQn_part ; i++){
    tsk_info->get_unmanaged_uintah_field<CCVariable<double>, double , MemorySpace >(abskpQuad[i],_abskp_name_vector[i],patch->getID(),_matl_index,1);
  }


  parallel_initialize<ExecutionSpace,MemorySpace>(executionObject,0.0,abskp[0],scatkt,asymm,abskpQuad);
  auto abs_scat_coeff =  createContainer<CCVariable<double>, double,DEP_VAR_SIZE, MemorySpace>(_nDVs); // no need to initialize;
  for (int i=0; i<_nDVs; i++){
    tsk_info->get_unmanaged_uintah_field<CCVariable<double>, double , MemorySpace >(abs_scat_coeff[i],"partRadProps_temporary_"+std::to_string(i),patch->getID(),_matl_index,1);
  } 

  for (int ix=0; ix< _nQn_part ; ix++){
    if(_particle_calculator_type == "basic"){
      const double geomFactor=M_PI/4.0*_Qabs;
      Uintah::parallel_for<ExecutionSpace>(executionObject, range, KOKKOS_LAMBDA (int i, int j, int k) {
        double particle_absorption=geomFactor*weightQuad[ix](i,j,k)*sizeQuad[ix](i,j,k)*sizeQuad[ix](i,j,k)*_absorption_modifier;
        abskpQuad[ix](i,j,k)= (vol_fraction(i,j,k) > 1e-16) ? particle_absorption : 0.0;
        abskp[0](i,j,k)+= abskpQuad[ix](i,j,k);
      });
#ifdef HAVE_RADPROPS
    }else if(_particle_calculator_type == "constantCIF" &&  _p_planck_abskp ){
        Uintah::parallel_for<ExecutionSpace>( executionObject,range,  KOKKOS_LAMBDA(int i, int j, int k) {
          double particle_absorption=_part_radprops->planck_abs_coeff( sizeQuad[ix](i,j,k)/2.0, temperatureQuad[ix](i,j,k))*weightQuad[ix](i,j,k)*_absorption_modifier;
          abskpQuad[ix](i,j,k)= (vol_fraction(i,j,k) > 1e-16) ? particle_absorption : 0.0;
          abskp[0](i,j,k)+= abskpQuad[ix](i,j,k);
        });
      if (_scatteringOn){
        Uintah::parallel_for<ExecutionSpace>(executionObject, range,  KOKKOS_LAMBDA(int i, int j, int k) {
          double particle_scattering=_part_radprops->planck_sca_coeff( sizeQuad[ix](i,j,k)/2.0, temperatureQuad[ix](i,j,k))*weightQuad[ix](i,j,k);
          scatkt[0](i,j,k)+= (vol_fraction(i,j,k) > 1e-16) ? particle_scattering : 0.0;
          asymm[0](i,j,k) =_constAsymmFact;
        });
      }
    }else if(_particle_calculator_type == "constantCIF" &&  _p_ros_abskp ){
        Uintah::parallel_for<ExecutionSpace>( executionObject,range,  KOKKOS_LAMBDA(int i, int j, int k) {
          double particle_absorption=_part_radprops->ross_abs_coeff( sizeQuad[ix](i,j,k)/2.0, temperatureQuad[ix](i,j,k))*weightQuad[ix](i,j,k)*_absorption_modifier;
          abskpQuad[ix](i,j,k)= (vol_fraction(i,j,k) > 1e-16) ? particle_absorption : 0.0;
          abskp[0](i,j,k)+= abskpQuad[ix](i,j,k);
        });
      if (_scatteringOn){
        Uintah::parallel_for<ExecutionSpace>( executionObject,range,  KOKKOS_LAMBDA(int i, int j, int k) {
          double particle_scattering=_part_radprops->ross_sca_coeff( sizeQuad[ix](i,j,k)/2.0, temperatureQuad[ix](i,j,k))*weightQuad[ix](i,j,k);
          scatkt[0](i,j,k)+= (vol_fraction(i,j,k) > 1e-16) ? particle_scattering : 0.0;
          asymm[0](i,j,k) =_constAsymmFact;
        });
      }
#endif
    }else if(_particle_calculator_type == "tabulated"  ){
        enum independentVariables{ en_size, en_temp};
        enum DependentVariables{ abs_coef, sca_coef};
        auto IV =  createContainer<constCCVariable<double>, const double, IND_VAR_SIZE , MemorySpace>(_nIVs);
        IV[en_size]=  tsk_info->get_const_uintah_field_add<constCCVariable<double>,const double, MemorySpace >(_size_name_v[ix],patch->getID(),_matl_index,1);
        IV[en_temp]=  tsk_info->get_const_uintah_field_add<constCCVariable<double>,const double, MemorySpace >(_temperature_name_v[ix],patch->getID(),_matl_index,1);
        parallel_initialize<ExecutionSpace,MemorySpace>(executionObject,0.0,abs_scat_coeff);
        myTable->getState<ExecutionSpace, MemorySpace>(executionObject,IV, abs_scat_coeff, patch); // indepvar)names diameter, size
        const double     portable_absorption_modifier =_absorption_modifier;
        Uintah::parallel_for<ExecutionSpace>( executionObject,range,  KOKKOS_LAMBDA(int i, int j, int k) {
          double particle_absorption=abs_scat_coeff[abs_coef](i,j,k)*weightQuad[ix](i,j,k)*portable_absorption_modifier;
          abskpQuad[ix](i,j,k)= (vol_fraction(i,j,k) > 1e-16) ? particle_absorption : 0.0;
          abskp[0](i,j,k)+= abskpQuad[ix](i,j,k);
        });
      if (_scatteringOn){
        Uintah::parallel_for<ExecutionSpace>( executionObject,range,  KOKKOS_LAMBDA(int i, int j, int k) {
          double particle_scattering=abs_scat_coeff[sca_coef](i,j,k)*weightQuad[ix](i,j,k);
          scatkt[0](i,j,k)+= (vol_fraction(i,j,k) > 1e-16) ? particle_scattering : 0.0;
          asymm[0](i,j,k) =_constAsymmFact;
        });
      }
    } // End calc_type if
  } // End For



#ifdef HAVE_RADPROPS
  if(_particle_calculator_type == "coal" &&  _p_planck_abskp ){
      for (int ix=0; ix<_nQn_part; ix++){
        auto abskpQuad = tsk_info->get_uintah_field_add           <CCVariable<double> >(_abskp_name_vector[ix]);  // ConstCC and CC behave differently
        abskpQuad.initialize(0.0);
        Uintah::parallel_for<ExecutionSpace>( executionObject,range,  KOKKOS_LAMBDA(int i, int j, int k) {
            double total_mass = RC_mass[ix](i,j,k)+Char_mass[ix](i,j,k)+_ash_mass_v[ix];
            double complexReal =  (Char_mass[ix](i,j,k)*_charReal+RC_mass[ix](i,j,k)*_rawCoalReal+_ash_mass_v[ix]*_ashReal)/total_mass;
            double particle_absorption=_3Dpart_radprops->planck_abs_coeff( sizeQuad[ix](i,j,k)/2.0, temperatureQuad[ix](i,j,k),complexReal)*weightQuad[ix](i,j,k)*_absorption_modifier;
            abskpQuad(i,j,k)= (vol_fraction(i,j,k) > 1e-16) ? particle_absorption : 0.0;
            abskp[0](i,j,k)+= abskpQuad(i,j,k);
         });
      }
      if (_scatteringOn){
        Uintah::parallel_for<ExecutionSpace>( executionObject,range,  KOKKOS_LAMBDA(int i, int j, int k) {
             std::vector<double> total_mass(_nQn_part);
             std::vector<double> scatQuad(_nQn_part);
            for (int ix=0; ix<_nQn_part; ix++){
              total_mass[ix] = RC_mass[ix](i,j,k)+Char_mass[ix](i,j,k)+_ash_mass_v[ix];
              double complexReal =  (Char_mass[ix](i,j,k)*_charReal+RC_mass[ix](i,j,k)*_rawCoalReal+_ash_mass_v[ix]*_ashReal)/total_mass[ix];
              scatQuad[ix]=_3Dpart_radprops->planck_sca_coeff( sizeQuad[ix](i,j,k)/2.0, temperatureQuad[ix](i,j,k),complexReal)*weightQuad[ix](i,j,k);
              scatkt[0](i,j,k)= (vol_fraction(i,j,k) > 1e-16) ? scatkt[0](i,j,k)+scatQuad[ix] : 0.0;
            }
            for (int ix=0; ix<_nQn_part; ix++){
              asymm[0](i,j,k) += (scatkt[0](i,j,k) < 1e-8) ? 0.0 :  (Char_mass[ix](i,j,k)*_charAsymm+RC_mass[ix](i,j,k)*_rawCoalAsymm+_ash_mass_v[ix]*_ashAsymm)/(total_mass[ix]*scatkt[0](i,j,k))*scatQuad[ix] ;
            }
        });
      }
    }else if(_particle_calculator_type == "coal" &&  _p_ros_abskp ){
      for (int ix=0; ix<_nQn_part; ix++){
        CCVariable<double>& abskpQuad = tsk_info->get_uintah_field_add           <CCVariable<double> >(_abskp_name_vector[ix]);  // ConstCC and CC behave differently
        abskpQuad.initialize(0.0);
        Uintah::parallel_for<ExecutionSpace>( executionObject,range,  KOKKOS_LAMBDA(int i, int j, int k) {
            double total_mass = RC_mass[ix](i,j,k)+Char_mass[ix](i,j,k)+_ash_mass_v[ix];
            double complexReal =  (Char_mass[ix](i,j,k)*_charReal+RC_mass[ix](i,j,k)*_rawCoalReal+_ash_mass_v[ix]*_ashReal)/total_mass;
            double particle_absorption=_3Dpart_radprops->ross_abs_coeff( sizeQuad[ix](i,j,k)/2.0, temperatureQuad[ix](i,j,k),complexReal)*weightQuad[ix](i,j,k)*_absorption_modifier;
            abskpQuad(i,j,k)= (vol_fraction(i,j,k) > 1e-16) ? particle_absorption : 0.0;
            abskp[0](i,j,k)+= abskpQuad(i,j,k);
         });
      }
      if (_scatteringOn){
        Uintah::parallel_for<ExecutionSpace>( executionObject,range,  KOKKOS_LAMBDA(int i, int j, int k) {
             std::vector<double> total_mass(_nQn_part);
             std::vector<double> scatQuad(_nQn_part);
            for (int ix=0; ix<_nQn_part; ix++){
              total_mass[ix] = RC_mass[ix](i,j,k)+Char_mass[ix](i,j,k)+_ash_mass_v[ix];
              double complexReal =  (Char_mass[ix](i,j,k)*_charReal+RC_mass[ix](i,j,k)*_rawCoalReal+_ash_mass_v[ix]*_ashReal)/total_mass[ix];
              scatQuad[ix]=_3Dpart_radprops->ross_sca_coeff( sizeQuad[ix](i,j,k)/2.0, temperatureQuad[ix](i,j,k),complexReal)*weightQuad[ix](i,j,k);
              scatkt[0](i,j,k)= (vol_fraction(i,j,k) > 1e-16) ? scatkt[0](i,j,k)+scatQuad[ix] : 0.0;
            }
            for (int ix=0; ix<_nQn_part; ix++){
              asymm(i,j,k) += (scatkt[0](i,j,k) < 1e-8) ? 0.0 :  (Char_mass[ix](i,j,k)*_charAsymm+RC_mass[ix](i,j,k)*_rawCoalAsymm+_ash_mass_v[ix]*_ashAsymm)/(total_mass[ix]*scatkt[0](i,j,k))*scatQuad[ix] ;
            }
        });
      }
  }
#endif

} // end eval
