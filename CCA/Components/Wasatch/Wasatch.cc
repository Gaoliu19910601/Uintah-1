/*
 * The MIT License
 *
 * Copyright (c) 2010-2012 The University of Utah
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */
#include <fstream>
#include <limits>
//-- Uintah framework includes --//
#include <sci_defs/uintah_defs.h>
#include <CCA/Ports/Scheduler.h>
#include <CCA/Components/Schedulers/OnDemandDataWarehouse.h>
#include <Core/ProblemSpec/ProblemSpec.h>
#include <Core/Grid/SimulationState.h>
#include <Core/Grid/Level.h>
#include <Core/Grid/Variables/VarTypes.h>

#include <Core/Grid/SimpleMaterial.h>
#include <Core/Grid/Task.h>
#include <Core/Parallel/Parallel.h>
#include <Core/Parallel/ProcessorGroup.h>
#include <Core/Malloc/Allocator.h>
#include <Core/Exceptions/ProblemSetupException.h>
#include <Core/Grid/Patch.h>
#include <Core/Grid/BoundaryConditions/BCDataArray.h>
#include <Core/Grid/BoundaryConditions/BoundCond.h>

//-- SpatialOps includes --//
#include <CCA/Components/Wasatch/Operators/OperatorTypes.h>
#include <CCA/Components/Wasatch/Expressions/StableTimestep.h>

#include <spatialops/structured/FVStaggered.h>
#include <spatialops/structured/FVStaggeredBCTools.h>
#ifdef ENABLE_THREADS
#include <spatialops/SpatialOpsTools.h>
#include <expression/SchedulerBase.h>
#endif

//-- ExprLib includes --//
#include <expression/ExprLib.h>

//-- TabProps --//
#include <tabprops/TabPropsConfig.h>

//-- Wasatch includes --//
#include "Wasatch.h"
#include "WasatchMaterial.h"
#include "CoordHelper.h"
#include "FieldAdaptor.h"
#include "TagNames.h"
#include "TaskInterface.h"
#include "TimeStepper.h"
#include "Properties.h"
#include "Operators/Operators.h"
#include "Expressions/BasicExprBuilder.h"
#include "Expressions/EmbeddedGeometry/EmbeddedGeometryHelper.h"
#include "Expressions/SetCurrentTime.h"
#include "transport/ParseEquation.h"
#include "transport/TransportEquation.h"
#include "BCHelperTools.h"
#include "ParseTools.h"
#include "FieldClippingTools.h"
#include "OldVariable.h"
#include "ReductionHelper.h"

using std::endl;

namespace Wasatch{

  //--------------------------------------------------------------------

  Wasatch::Wasatch( const Uintah::ProcessorGroup* myworld )
    : Uintah::UintahParallelComponent( myworld ),
      buildTimeIntegrator_ ( true ),
      buildWasatchMaterial_( true ),
      nRKStages_(1)
  {
    proc0cout << std::endl
              << "-------------------------------------------------------------" << std::endl
              << "Wasatch was built against:" << std::endl
              << "  SpatialOps HASH: " << SOPS_REPO_HASH << std::endl
              << "             DATE: " << SOPS_REPO_DATE << std::endl
              << "     ExprLib HASH: " << EXPR_REPO_HASH << std::endl
              << "             DATE: " << EXPR_REPO_DATE << std::endl
              << "    TabProps HASH: " << TabPropsVersionHash << std::endl
              << "             DATE: " << TabPropsVersionDate << std::endl
              << "-------------------------------------------------------------" << std::endl
              << std::endl;

    materials_     = NULL;
    icCoordHelper_ = NULL;
    timeStepper_   = NULL;
    linSolver_     = NULL;

    isRestarting_ = false;

    // disable memory windowing on variables.  This will ensure that
    // each variable is allocated its own memory on each patch,
    // precluding memory blocks being defined across multiple patches.
    Uintah::OnDemandDataWarehouse::d_combineMemory = false;

    const bool log = false;
    graphCategories_[ INITIALIZATION     ] = scinew GraphHelper( scinew Expr::ExpressionFactory(log) );
    graphCategories_[ TIMESTEP_SELECTION ] = scinew GraphHelper( scinew Expr::ExpressionFactory(log) );
    graphCategories_[ ADVANCE_SOLUTION   ] = scinew GraphHelper( scinew Expr::ExpressionFactory(log) );

    icCoordHelper_  = new CoordHelper( *(graphCategories_[INITIALIZATION]->exprFactory) );

    OldVariable::self().sync_with_wasatch( this );
    ReductionHelper::self().sync_with_wasatch( this );
  }

  //--------------------------------------------------------------------

  Wasatch::~Wasatch()
  {
    for( PatchInfoMap::iterator i=patchInfoMap_.begin(); i!=patchInfoMap_.end(); ++i ){
      delete i->second.operators;
    }

    for( EquationAdaptors::iterator i=adaptors_.begin(); i!=adaptors_.end(); ++i ){
      delete *i;
    }

    for( std::list<const TaskInterface*>::iterator i=taskInterfaceList_.begin(); i!=taskInterfaceList_.end(); ++i ){
      delete *i;
    }

    for( std::map<const Uintah::LevelP, const Uintah::PatchSet*>::iterator i=patchSetList_.begin(); i!=patchSetList_.end(); ++i ){
      delete i->second;
    }

    delete icCoordHelper_;
    if (buildTimeIntegrator_) delete timeStepper_;

    for( GraphCategories::iterator igc=graphCategories_.begin(); igc!=graphCategories_.end(); ++igc ){
      delete igc->second->exprFactory;
      delete igc->second;
    }
  }

  //--------------------------------------------------------------------
  
  void force_expressions_on_graph( Expr::TagList& exprTagList,
                                   GraphHelper* const graphHelper )
  {    
    Expr::Tag exprtag;
    for( Expr::TagList::iterator exprtag=exprTagList.begin();
        exprtag!=exprTagList.end();
        ++exprtag ){
      const Expr::ExpressionID exprID = graphHelper->exprFactory->get_id(*exprtag);
      graphHelper->rootIDs.insert( exprID );
      graphHelper->forcedIDs.insert( exprID );
    }    
  }
  
  //--------------------------------------------------------------------

  void force_expressions_on_graph( Uintah::ProblemSpecP forceOnGraphParams,
                                   GraphHelper* const gh )
  {
    for( Uintah::ProblemSpecP exprParams = forceOnGraphParams->findBlock("NameTag");
        exprParams != 0;
        exprParams = exprParams->findNextBlock("NameTag") )
    {
      const Expr::Tag tag = parse_nametag( exprParams );
      const Expr::ExpressionID exprID = gh->exprFactory->get_id(tag);
      gh->rootIDs.insert( exprID );
      gh->forcedIDs.insert( exprID );
    }
  }

  //--------------------------------------------------------------------
  
  void check_periodicity_extra_cells( const Uintah::ProblemSpecP& params,
                                      Uintah::IntVector& extraCells )
  {
    // disallow different periodicities on multiple levels
    std::vector<Uintah::IntVector> levelPeriodicityVectors;
    // disallow specification of extraCells and different periodicities on multiple levels
    int nlevels = 0;
    std::ostringstream msg;
    bool foundExtraCells = false;
    Uintah::ProblemSpecP gridspec = params->findBlock("Grid");
    for( Uintah::ProblemSpecP level = gridspec->findBlock("Level");
        level != 0;
        level = gridspec->findNextBlock("Level") ) {
      nlevels++;
      Uintah::IntVector periodicDirs(0,0,0);
      level->get("periodic", periodicDirs);
      levelPeriodicityVectors.push_back(periodicDirs);
      
      for( Uintah::ProblemSpecP box = level->findBlock("Box");
          box != 0;
          box = level->findNextBlock("Box") ){
        // note that a [0,0,0] specification gets added by default,
        // so we will check to ensure that something other than
        // [0,0,0] has not been specified.
        if( box->findBlock("extraCells") ){
          //Uintah::IntVector extraCells;
          box->get("extraCells",extraCells);
          if( extraCells != Uintah::IntVector(0,0,0) ){
            foundExtraCells = true;
          }
        }
      }
    }
    
    // check for different periodicities on different levels
    std::vector<Uintah::IntVector>::iterator periodicityIter = levelPeriodicityVectors.begin();
    int xPeriodSum = 0, yPeriodSum = 0, zPeriodSum = 0;
    while (periodicityIter != levelPeriodicityVectors.end()) {
      Uintah::IntVector& pvector = *periodicityIter;
      xPeriodSum += pvector.x();
      yPeriodSum += pvector.y();
      zPeriodSum += pvector.z();
      ++periodicityIter;
    }
    
    if  ( ( xPeriodSum !=0 && xPeriodSum < nlevels ) ||
          ( yPeriodSum !=0 && yPeriodSum < nlevels ) ||
          ( zPeriodSum !=0 && zPeriodSum < nlevels ) ) {
      msg << endl
      << "  Specification of different periodicities for different levels is not supported in Wasatch." << endl
      << "  Please revise your input file." << endl
      << endl;
      throw std::runtime_error( msg.str() );
    }
    
    Uintah::IntVector periodicityVector = levelPeriodicityVectors[0];
    const bool isXPeriodic = (periodicityVector.x() == 1) ? true : false;
    const bool isYPeriodic = (periodicityVector.y() == 1) ? true : false;
    const bool isZPeriodic = (periodicityVector.z() == 1) ? true : false;

#     ifdef WASATCH_IN_ARCHES
    // we are only allowing for a single extra cell :(
    // make sure that extra cell and periodicity are consistent
    bool isPeriodic = periodicityVector.x() == 1 || periodicityVector.y() == 1 || periodicityVector.z() == 1;
    
    std::cout << "periodicity = " << isPeriodic << std::endl;
    
    if( !foundExtraCells && !isPeriodic ){
      msg << endl
      << "  Specification of 'extraCells' is required when wasatch-in-arches is enabled." << endl
      << "  Please add an 'extraCells' block to your input file" << endl
      << endl;
      throw std::runtime_error( msg.str() );
    }
#     else
    if( foundExtraCells ){
      msg << endl
      << "  Specification of 'extraCells' is forbidden in Wasatch. The number of extraCells is automatically determined." << endl
      << "  Please remove it from your input file." << endl
      << endl;
      throw std::runtime_error( msg.str() );
    }
#     endif
        
    extraCells = Uintah::IntVector( (isXPeriodic) ? 0 : 1,
                                    (isYPeriodic) ? 0 : 1,
                                    (isZPeriodic) ? 0 : 1 );

  }

  //--------------------------------------------------------------------

  
  void Wasatch::preGridProblemSetup(const Uintah::ProblemSpecP& params,
                           Uintah::GridP& grid,
                           Uintah::SimulationStateP& state)
  {
    Uintah::IntVector extraCells;
    check_periodicity_extra_cells( params, extraCells);    
    grid->setExtraCells(extraCells);
  }
  //--------------------------------------------------------------------

  void Wasatch::problemSetup( const Uintah::ProblemSpecP& params,
                              const Uintah::ProblemSpecP& ,  /* jcs not sure what this param is for */
                              Uintah::GridP& grid,
                              Uintah::SimulationStateP& sharedState )
  {
    wasatchSpec_ = params->findBlock("Wasatch");
    if (!wasatchSpec_) return;

    sharedState_ = sharedState;
    
    double deltMin, deltMax;
    params->findBlock("Time")->require("delt_min", deltMin);
    params->findBlock("Time")->require("delt_max", deltMax);
    const bool useAdaptiveDt = std::abs(deltMax - deltMin) > 2.0*std::numeric_limits<double>::epsilon();
    
    // Multithreading in ExprLib and SpatialOps
    if( wasatchSpec_->findBlock("FieldParallelThreadCount") ){
#    ifdef ENABLE_THREADS
      int spatialOpsThreads=0;
      wasatchSpec_->get( "FieldParallelThreadCount", spatialOpsThreads );
      SpatialOps::set_hard_thread_count(NTHREADS);
      SpatialOps::set_soft_thread_count( spatialOpsThreads );
      proc0cout << "-> Wasatch is running with " << SpatialOps::get_soft_thread_count()
      << " / " << SpatialOps::get_hard_thread_count()
      << " data-parallel threads (SpatialOps)" << std::endl;
#    else
      proc0cout << "NOTE: cannot specify thread counts unless SpatialOps is built with multithreading" << std::endl;
#    endif
    }
    if( wasatchSpec_->findBlock("TaskParallelThreadCount") ){
#    ifdef ENABLE_THREADS
      int exprLibThreads=0;
      wasatchSpec_->get( "TaskParallelThreadCount", exprLibThreads );
      Expr::set_hard_thread_count( NTHREADS );
      Expr::set_soft_thread_count( exprLibThreads );
      proc0cout << "-> Wasatch is running with " << Expr::get_soft_thread_count()
      << " / " << Expr::get_hard_thread_count()
      << " task-parallel threads (ExprLib)" << std::endl;
#    else
      proc0cout << "NOTE: cannot specify thread counts unless SpatialOps is built with multithreading" << std::endl;
#    endif
    }

    // PARSE BC FUNCTORS
    Uintah::ProblemSpecP bcParams = params->findBlock("Grid")->findBlock("BoundaryConditions");
    if (bcParams) {
      for( Uintah::ProblemSpecP faceBCParams=bcParams->findBlock("Face");
          faceBCParams != 0;
          faceBCParams=faceBCParams->findNextBlock("BCType") ){
        
        for( Uintah::ProblemSpecP bcTypeParams=faceBCParams->findBlock("BCType");
            bcTypeParams != 0;
            bcTypeParams=bcTypeParams->findNextBlock("BCType") ){
          std::string functorName;
          if ( bcTypeParams->get("functor_name",functorName) ) {
            
            std::string phiName;
            bcTypeParams->getAttribute("label",phiName);
            std::cout << "functor applies to " << phiName << std::endl;
            
            std::map< std::string,std::set<std::string> >::iterator iter = bcFunctorMap_.find(phiName);
            // check if we already have an entry for phiname
            if ( iter != bcFunctorMap_.end() ) {
              (*iter).second.insert(functorName);
            } else if ( iter == bcFunctorMap_.end() ) {
              std::set<std::string> functorSet;
              functorSet.insert(functorName);
              bcFunctorMap_.insert(std::pair< std::string, std::set<std::string> >(phiName,functorSet) );
            }
          }          
        }
      }
    }
    
    // Here we add the functors name of the "*" stage variables BC to the functormap
    if (wasatchSpec_->findBlock("MomentumEquations") && wasatchSpec_->findBlock("Density") && wasatchSpec_->findBlock("TransportEquation")) {

      for( Uintah::ProblemSpecP transEqnParams=wasatchSpec_->findBlock("TransportEquation");
          transEqnParams != 0;
          transEqnParams=transEqnParams->findNextBlock("TransportEquation") ){
        std::string solnVarName;
        transEqnParams->get("SolutionVariable",solnVarName);
        std::set<std::string> functorSet;
        std::string functorName = solnVarName+TagNames::self().star+"_bc";
        std::string phiName     = solnVarName+TagNames::self().star;
        functorSet.insert(functorName);
        bcFunctorMap_.insert(std::pair< std::string, std::set<std::string> >(phiName,functorSet) );        
      }

      Uintah::ProblemSpecP densityParams  = wasatchSpec_->findBlock("Density");
      Expr::Tag densityTag = parse_nametag( densityParams->findBlock("NameTag") );
      std::set<std::string> functorSet;
      std::string functorName = densityTag.name()+TagNames::self().star+"_bc";
      std::string phiName     = densityTag.name()+TagNames::self().star;
      functorSet.insert(functorName);
      bcFunctorMap_.insert(std::pair< std::string, std::set<std::string> >(phiName,functorSet) );
      
    }

    // PARSE IO FIELDS
    Uintah::ProblemSpecP archiverParams = params->findBlock("DataArchiver");
    for( Uintah::ProblemSpecP saveLabelParams=archiverParams->findBlock("save");
        saveLabelParams != 0;
        saveLabelParams=saveLabelParams->findNextBlock("save") ){
      std::string saveTheLabel;
      saveLabelParams->getAttribute("label",saveTheLabel);
      lockedFields_.insert(saveTheLabel);
    }

    //
    // Material
    //
    if (buildWasatchMaterial_) {
      Uintah::WasatchMaterial* mat= scinew Uintah::WasatchMaterial();
      sharedState->registerWasatchMaterial(mat);
    }

    // we are able to get the solver port from here
    linSolver_ = dynamic_cast<Uintah::SolverInterface*>(getPort("solver"));
    if(!linSolver_) {
      throw Uintah::InternalError("Wasatch: couldn't get solver port", __FILE__, __LINE__);
    } else if (linSolver_) {
      proc0cout << "Detected solver: " << linSolver_->getName() << std::endl;
      const bool needPressureSolve = wasatchSpec_->findBlock("MomentumEquations") && !(wasatchSpec_->findBlock("MomentumEquations")->findBlock("DisablePressureSolve"));
      if ( (linSolver_->getName()).compare("hypre") != 0 && needPressureSolve) {
        std::ostringstream msg;
        msg << "  Invalid solver specified: "<< linSolver_->getName() << std::endl
        << "  Wasatch currently works with hypre solver only. Please change your solver type." << std::endl
        << std::endl;
        throw std::runtime_error( msg.str() );
      }
    }
    
    //
    std::string timeIntName;
    wasatchSpec_->get("TimeIntegrator",timeIntName);
    TimeIntegrator timeInt(timeIntName);
    nRKStages_ = timeInt.nStages;

    //
    // create expressions explicitly defined in the input file.  These
    // are typically associated with, e.g. initial conditions.
    //
    create_expressions_from_input( wasatchSpec_, graphCategories_ );
    parse_embedded_geometry(wasatchSpec_,graphCategories_);
    setup_property_evaluation( wasatchSpec_, graphCategories_ );

    //
    // get the turbulence params, if any, and parse them.
    //
    Uintah::ProblemSpecP turbulenceModelParams = wasatchSpec_->findBlock("Turbulence");
    struct TurbulenceParameters turbParams = {1.0,0.1,NOTURBULENCE};
    parse_turbulence_input(turbulenceModelParams, turbParams);
    
    //
    // extract the density tag for scalar transport equations and momentum equations
    // and perform error handling
    //
    Uintah::ProblemSpecP momEqnParams   = wasatchSpec_->findBlock("MomentumEquations");
    Uintah::ProblemSpecP densityParams  = wasatchSpec_->findBlock("Density");
    bool existSrcTerm=false;

    for( Uintah::ProblemSpecP transEqnParams=wasatchSpec_->findBlock("TransportEquation");
        transEqnParams != 0;
        transEqnParams=transEqnParams->findNextBlock("TransportEquation") ){
      existSrcTerm = (existSrcTerm || transEqnParams->findBlock("SourceTermExpression") );
    }
    Uintah::ProblemSpecP transEqnParams = wasatchSpec_->findBlock("TransportEquation");

    Expr::Tag densityTag = Expr::Tag();
    bool isConstDensity = true;

    if (transEqnParams || momEqnParams) {
      if( !densityParams ) {
        std::ostringstream msg;
        msg << "ERROR: You must include a 'Density' block in your input file when solving transport equations" << endl;
        throw Uintah::ProblemSetupException( msg.str(), __FILE__, __LINE__ );
      }
      const bool existDensity = densityParams->findBlock("NameTag");
      if (existDensity) densityTag = parse_nametag( densityParams->findBlock("NameTag") );
      densityParams->get("IsConstant",isConstDensity);

      if( !isConstDensity || existSrcTerm || momEqnParams) {
        if( !existDensity ) {
          std::ostringstream msg;
          msg << "ERROR: For variable density cases or when source terms exist in transport equations (scalar, momentum, etc...), the density expression tag" << endl
              << "       must be provided in the <Density> block" << endl;
          throw Uintah::ProblemSetupException( msg.str(), __FILE__, __LINE__ );
        }
      }
    }

    //
    // Build transport equations.  This registers all expressions as
    // appropriate for solution of each transport equation.
    //
    const bool hasEmbeddedGeometry = wasatchSpec_->findBlock("EmbeddedGeometry");

    for( Uintah::ProblemSpecP transEqnParams=wasatchSpec_->findBlock("TransportEquation");
         transEqnParams != 0;
         transEqnParams=transEqnParams->findNextBlock("TransportEquation") ){
      adaptors_.push_back( parse_equation( transEqnParams, turbParams, hasEmbeddedGeometry, densityTag, isConstDensity, graphCategories_ ) );
    }

    //
    // Build coupled transport equations scalability test for wasatch.
    //
    for( Uintah::ProblemSpecP scalEqnParams=wasatchSpec_->findBlock("ScalabilityTest");
        scalEqnParams != 0;
        scalEqnParams=scalEqnParams->findNextBlock("ScalabilityTest") ) {
      try{
        // note - parse_scalability_test returns a vector of equation adaptors
        EquationAdaptors scalEqnAdaptors = parse_scalability_test( scalEqnParams, graphCategories_ );
        adaptors_.insert( adaptors_.end(), scalEqnAdaptors.begin(), scalEqnAdaptors.end() );
      }
      catch( std::runtime_error& err ){
        std::ostringstream msg;
        msg << endl
        << "Problems setting up scalability test equations.  Details follow:" << endl
        << err.what() << endl;
        throw Uintah::ProblemSetupException( msg.str(), __FILE__, __LINE__ );
      }
    }

    //
    // Build momentum transport equations.  This registers all expressions
    // required for solution of each momentum equation.
    //
    for( Uintah::ProblemSpecP momEqnParams=wasatchSpec_->findBlock("MomentumEquations");
        momEqnParams != 0;
        momEqnParams=momEqnParams->findNextBlock("MomentumEquations") ){
      bool hasMovingBoundaries = false;
      if (hasEmbeddedGeometry) hasMovingBoundaries = wasatchSpec_->findBlock("EmbeddedGeometry")->findBlock("MovingGeometry") ;
      // note - parse_momentum_equations returns a vector of equation adaptors
      try{
          const EquationAdaptors adaptors = parse_momentum_equations( momEqnParams,
                                                                      turbParams,
                                                                      useAdaptiveDt,
                                                                      isConstDensity,
                                                                      hasEmbeddedGeometry,
                                                                      hasMovingBoundaries,
                                                                      densityTag,
                                                                      graphCategories_,
                                                                      *linSolver_,
                                                                      sharedState );
        adaptors_.insert( adaptors_.end(), adaptors.begin(), adaptors.end() );
      }
      catch( std::runtime_error& err ){
        std::ostringstream msg;
        msg << endl
            << "Problems setting up momentum transport equations.  Details follow:" << endl
            << err.what() << endl;
        throw Uintah::ProblemSetupException( msg.str(), __FILE__, __LINE__ );
      }
    }

    //
    // Build moment transport equations.  This registers all expressions
    // required for solution of each momentum equation.
    //
    for( Uintah::ProblemSpecP momEqnParams=wasatchSpec_->findBlock("MomentTransportEquation");
        momEqnParams != 0;
        momEqnParams=momEqnParams->findNextBlock("MomentTransportEquation") ){
      // note - parse_moment_transport_equations returns a vector of equation adaptors
      try{
        //For the Multi-Environment mixing model, the entire Wasatch Block must be passed to find values for initial moments
        const EquationAdaptors adaptors =
            parse_moment_transport_equations( momEqnParams, wasatchSpec_, isConstDensity,
                                              hasEmbeddedGeometry, graphCategories_ );
        adaptors_.insert( adaptors_.end(), adaptors.begin(), adaptors.end() );
      }
      catch( std::runtime_error& err ){
        std::ostringstream msg;
        msg << endl
        << "Problems setting up moment transport equations.  Details follow:" << endl
        << err.what() << endl;
        throw Uintah::ProblemSetupException( msg.str(), __FILE__, __LINE__ );
      }
    }

    //
    // Build poisson equations
    for( Uintah::ProblemSpecP poissonEqnParams=wasatchSpec_->findBlock("PoissonEquation");
        poissonEqnParams != 0;
        poissonEqnParams=poissonEqnParams->findNextBlock("PoissonEquation") ){
      try{
        parse_poisson_equation( poissonEqnParams, graphCategories_,
                                *linSolver_, sharedState );
      }
      catch( std::runtime_error& err ){
        std::ostringstream msg;
        msg << endl
        << "Problems setting up momentum transport equations.  Details follow:" << endl
        << err.what() << endl;
        throw Uintah::ProblemSetupException( msg.str(), __FILE__, __LINE__ );
      }
    }
    
    if( buildTimeIntegrator_ ){
      timeStepper_ = scinew TimeStepper( sharedState_, *graphCategories_[ ADVANCE_SOLUTION ], timeInt );
    }    
    
    //
    // force additional expressions on the graph
    //
    for( Uintah::ProblemSpecP forceOnGraphParams=wasatchSpec_->findBlock("ForceOnGraph");
        forceOnGraphParams != 0;
        forceOnGraphParams=forceOnGraphParams->findNextBlock("ForceOnGraph") )
    {
      std::vector<std::string> taskListNames;
      forceOnGraphParams->getAttribute("tasklist", taskListNames);
      std::vector<std::string>::iterator taskListIter = taskListNames.begin();
      for( ; taskListIter != taskListNames.end(); ++taskListIter ){
        force_expressions_on_graph( forceOnGraphParams, graphCategories_[select_tasklist(*taskListIter)] );
      }
    }

    parse_cleave_requests    ( wasatchSpec_, graphCategories_ );
    parse_attach_dependencies( wasatchSpec_, graphCategories_ );
    //
    // get the variable density params, if any, and parse them.
    //
    Uintah::ProblemSpecP VarDensMMSParams = wasatchSpec_->findBlock("VariableDensityMMS");
    if (VarDensMMSParams) {
      parse_var_dens_mms(VarDensMMSParams, graphCategories_);
    }
    
    //
    // process any reduction variables specified through the input file
    //
    ReductionHelper::self().parse_reduction_spec(wasatchSpec_);
  }

  //--------------------------------------------------------------------

  void Wasatch::scheduleInitialize( const Uintah::LevelP& level,
                                    Uintah::SchedulerP& sched )
  {
    // accessing the sharedState_->allWasatchMaterials() must be done after
    // problemSetup. The sharedstate class will create this material set
    // in postgridsetup, which is called after problemsetup. This is dictated
    // by Uintah.
    if (buildWasatchMaterial_) {
      set_wasatch_materials(sharedState_->allWasatchMaterials());
    }

    setup_patchinfo_map( level, sched );

    const Uintah::PatchSet* const localPatches = get_patchset( USE_FOR_TASKS, level, sched );

#ifndef WASATCH_IN_ARCHES // this is a bit annoying... when warches is turned on, disable any linearsolver calls from Wasatch
    if( linSolver_ ) {
      linSolver_->scheduleInitialize( level, sched, 
                                      sharedState_->allMaterials() );

      sched->overrideVariableBehavior("hypre_solver_label",false,false,
                                      false,true,true);
    }
#endif

    GraphHelper* const icGraphHelper = graphCategories_[ INITIALIZATION ];

    Expr::ExpressionFactory& exprFactory = *icGraphHelper->exprFactory;

    //_______________________________________
    // set the time
    Expr::TagList timeTags;
    timeTags.push_back( TagNames::self().time     );
    timeTags.push_back( TagNames::self().timestep );
    const Expr::Tag timeTag = TagNames::self().time;
    exprFactory.register_expression( scinew SetCurrentTime::Builder(timeTags), true );

    //_____________________________________________
    // Build the initial condition expression graph
    if( !icGraphHelper->rootIDs.empty() ){
      
      // -----------------------------------------------------------------------
      // INITIAL BOUNDARY CONDITIONS TREATMENT
      // -----------------------------------------------------------------------
      const Uintah::PatchSet* const localPatches2 = get_patchset( USE_FOR_OPERATORS, level, sched );
      const GraphHelper* icGraphHelper2 = graphCategories_[ INITIALIZATION ];
      typedef std::vector<EqnTimestepAdaptorBase*> EquationAdaptors;
      
      for( EquationAdaptors::const_iterator ia=adaptors_.begin(); ia!=adaptors_.end(); ++ia ){
        EqnTimestepAdaptorBase* const adaptor = *ia;
        TransportEquation* transEq = adaptor->equation();
        std::string eqnLabel = transEq->solution_variable_name();
        //______________________________________________________
        // set up initial boundary conditions on this transport equation
        try{
          proc0cout << "Setting Initial BCs for transport equation '" << eqnLabel << "'" << std::endl;
          transEq->setup_initial_boundary_conditions( *icGraphHelper2, localPatches2,
              patchInfoMap_, materials_->getUnion(), bcFunctorMap_);
        }
        catch( std::runtime_error& e ){
          std::ostringstream msg;
          msg << e.what()
          << std::endl
          << "ERORR while setting initial boundary conditions on equation '" << eqnLabel << "'"
          << std::endl;
          throw Uintah::ProblemSetupException( msg.str(), __FILE__, __LINE__ );
        }
      }
      // -----------------------------------------------------------------------          
      try{
        TaskInterface* const task = scinew TaskInterface( icGraphHelper->rootIDs,
                                                          "initialization",
                                                          *icGraphHelper->exprFactory,
                                                          level, sched,
                                                          localPatches,
                                                          materials_,
                                                          patchInfoMap_,
                                                          1, lockedFields_ );

        // set coordinate values as required by the IC graph.
        icCoordHelper_->create_task( sched, localPatches, materials_ );

        //_______________________________________________________
        // create the TaskInterface and schedule this task for
        // execution.  Note that field dependencies are assigned
        // within the TaskInterface object.
        task->schedule( icCoordHelper_->field_tags(), 1 );
        taskInterfaceList_.push_back( task );
      }
      catch( std::exception& err ){
        std::ostringstream msg;
        msg << "ERROR SETTING UP GRAPH FOR INITIALIZATION" << std::endl
            << err.what() << std::endl;
        throw Uintah::ProblemSetupException( msg.str(), __FILE__, __LINE__ );
      }
    }
    proc0cout << "Wasatch: done creating initialization task(s)" << std::endl;
  }

  //--------------------------------------------------------------------

  void Wasatch::restartInitialize()
  {
    isRestarting_ = true;

    // accessing the sharedState_->allWasatchMaterials() must be done after
    // problemSetup. The sharedstate class will create this material set
    // in postgridsetup, which is called after problemsetup. This is dictated
    // by Uintah.
    if( buildWasatchMaterial_ ){
      set_wasatch_materials( sharedState_->allWasatchMaterials() );
    }
  }

  //--------------------------------------------------------------------

  void Wasatch::setup_patchinfo_map( const Uintah::LevelP& level,
                                     Uintah::SchedulerP& sched )
  {
    //_______________________________________________________________
     // Set up the operators associated with the local patches.  We
     // only need to do this once, so we choose to do it here.  It
     // could just as well be done on any other schedule callback that
     // has access to the levels (patches).
     //
     // Also save off the timestep label information.
     //
     {
       const Uintah::PatchSet* patches = get_patchset( USE_FOR_OPERATORS, level, sched );

       for( int ipss=0; ipss<patches->size(); ++ipss ){
         const Uintah::PatchSubset* pss = patches->getSubset(ipss);
         for( int ip=0; ip<pss->size(); ++ip ){
           SpatialOps::OperatorDatabase* const opdb = scinew SpatialOps::OperatorDatabase();
           const Uintah::Patch* const patch = pss->get(ip);
           build_operators( *patch, *opdb );
           PatchInfo& pi = patchInfoMap_[patch->getID()];
           pi.operators = opdb;
           pi.patchID = patch->getID();
           //std::cout << "Set up operators for Patch ID: " << patch->getID() << " on process " << Uintah::Parallel::getMPIRank() << std::endl;
         }
       }
     }
  }

  //--------------------------------------------------------------------

  void Wasatch::scheduleComputeStableTimestep( const Uintah::LevelP& level,
                                               Uintah::SchedulerP& sched )
  {
    GraphHelper* const tsGraphHelper = graphCategories_[ TIMESTEP_SELECTION ];
    const Uintah::PatchSet* const localPatches = get_patchset(USE_FOR_TASKS,level,sched);

    if( tsGraphHelper->rootIDs.size() > 0 ){
      //_______________________________________________________
      // create the TaskInterface and schedule this task for
      // execution.  Note that field dependencies are assigned
      // within the TaskInterface object.
      TaskInterface* const task = scinew TaskInterface( tsGraphHelper->rootIDs,
                                                        "compute timestep",
                                                        *tsGraphHelper->exprFactory,
                                                        level, sched,
                                                        localPatches,
                                                        materials_,
                                                        patchInfoMap_,
                                                        1, lockedFields_ );
      task->schedule(1);
      taskInterfaceList_.push_back( task );
    }
    else{ // default

      proc0cout << "Scheduling Task 'compute timestep' COMPUTES 'delT' in NEW data warehouse" << endl;

      Uintah::Task* task = scinew Uintah::Task( "compute timestep", this, &Wasatch::computeDelT );

      // jcs it appears that for reduction variables we cannot specify the patches - only the materials.
      	task->computes( sharedState_->get_delt_label(),
                      level.get_rep() );
      //              materials_->getUnion() );
      // jcs why can't we specify a metrial here?  It doesn't seem to be working if I do.
      
      const GraphHelper* slnGraphHelper = graphCategories_[ADVANCE_SOLUTION];
      const TagNames& tagNames = TagNames::self();
      const bool useStableDT = slnGraphHelper->exprFactory->have_entry( tagNames.stableTimestep );
      // since the StableDT expression is only registered on the time_advance graph,
      // make the necessary checks before adding a requires for that
      if (sharedState_->getCurrentTopLevelTimeStep() > 0) {
        if (useStableDT) {
          task->requires(Uintah::Task::NewDW, Uintah::VarLabel::find(tagNames.stableTimestep.name()),  Uintah::Ghost::None, 0);
        }
      }
                  
      sched->addTask( task, localPatches, materials_ );
    }

    proc0cout << "Wasatch: done creating timestep task(s)" << std::endl;
  }

  //--------------------------------------------------------------------

  void
  Wasatch::scheduleTimeAdvance( const Uintah::LevelP& level,
                                Uintah::SchedulerP& sched )
  {
    if( isRestarting_ ){
      setup_patchinfo_map( level, sched );
      isRestarting_ = false;
    }

    const Uintah::PatchSet* const allPatches = get_patchset( USE_FOR_TASKS, level, sched );
    const Uintah::PatchSet* const localPatches = get_patchset( USE_FOR_OPERATORS, level, sched );
    const GraphHelper* advSolGraphHelper = graphCategories_[ ADVANCE_SOLUTION ];

    for( int iStage=1; iStage<=nRKStages_; iStage++ ){
      // jcs why do we need this instead of getting the level?
      // jcs notes:
      //
      //   eachPatch() returns a PatchSet that will result in the task
      //       being executed asynchronously accross all patches.  This
      //       can improve performance but will deadlock if any global MPI
      //       calls occur.
      //
      //   allPatches() returns a PatchSet that results in the task being
      //       executed together across all patches.  This is required if
      //       any global MPI syncronizations occurr (e.g. in a linear
      //       solve)
      //    also need to set a flag on the task: task->setType(Task::OncePerProc);
      
      // set up any "old" variables that have been requested.
      OldVariable::self().setup_tasks( allPatches, materials_, sched );

      // -----------------------------------------------------------------------
      // BOUNDARY CONDITIONS TREATMENT
      // -----------------------------------------------------------------------
      typedef std::vector<EqnTimestepAdaptorBase*> EquationAdaptors;
      
      for( EquationAdaptors::const_iterator ia=adaptors_.begin(); ia!=adaptors_.end(); ++ia ){
        EqnTimestepAdaptorBase* const adaptor = *ia;
        TransportEquation* transEq = adaptor->equation();
        std::string eqnLabel = transEq->solution_variable_name();
        //______________________________________________________
        // set up boundary conditions on this transport equation
        try{
          proc0cout << "Setting BCs for transport equation '" << eqnLabel << "'" << std::endl;
          transEq->setup_boundary_conditions(*advSolGraphHelper, localPatches, patchInfoMap_, materials_->getUnion(),bcFunctorMap_);
        }
        catch( std::runtime_error& e ){
          std::ostringstream msg;
          msg << e.what()
          << std::endl
          << "ERORR while setting boundary conditions on equation '" << eqnLabel << "'"
          << std::endl;
          throw Uintah::ProblemSetupException( msg.str(), __FILE__, __LINE__ );
        }
      }
      
      if( buildTimeIntegrator_ ){
        create_timestepper_on_patches( allPatches, materials_, level, sched, iStage );
      }

      proc0cout << "Wasatch: done creating solution task(s)" << std::endl;
      
      //
      // process clipping on fields
      //
      process_field_clipping( wasatchSpec_, graphCategories_, localPatches );      
    }

    // ensure that any "CARRY_FORWARD" variable has an initialization provided for it.
    if( buildTimeIntegrator_ ) { // make sure that we have a timestepper created - this is needed for wasatch-in-arches
      const Expr::ExpressionFactory* const icFactory = graphCategories_[INITIALIZATION]->exprFactory;
      typedef std::list< TaskInterface* > TIList;
      bool isOk = true;
      Expr::TagList missingTags;
      const TIList& tilist = timeStepper_->get_task_interfaces();
      for( TIList::const_iterator iti=tilist.begin(); iti!=tilist.end(); ++iti ){
        const Expr::TagList tags = (*iti)->collect_tags_in_task();
        for( Expr::TagList::const_iterator itag=tags.begin(); itag!=tags.end(); ++itag ){
          if( itag->context() == Expr::CARRY_FORWARD ){
            if( !icFactory->have_entry(*itag) ) missingTags.push_back( *itag );
          }
        }
      }
      if( !isOk ){
        std::ostringstream msg;
        msg << "ERORR: The following fields were marked 'CARRY_FORWARD' but were not initialized." << std::endl
            << "       Ensure that all of these fields are present on the initialization graph:" << std::endl;
        for( Expr::TagList::const_iterator it=missingTags.begin(); it!=missingTags.end(); ++it ){
          msg << "         " << *it << std::endl;
        }
        throw Uintah::ProblemSetupException( msg.str(), __FILE__, __LINE__ );
      }
    }

  }

  //--------------------------------------------------------------------

  void
  Wasatch::create_timestepper_on_patches( const Uintah::PatchSet* const localPatches,
                                          const Uintah::MaterialSet* const materials,
                                          const Uintah::LevelP& level,
                                          Uintah::SchedulerP& sched,
                                          const int rkStage )
  {
    GraphHelper* const gh = graphCategories_[ ADVANCE_SOLUTION ];
    Expr::ExpressionFactory& exprFactory = *gh->exprFactory;

    if( adaptors_.size() == 0 && gh->rootIDs.empty()) return; // no equations registered.

    //_____________________________________________________________
    // create an expression to set the current time as a field that
    // will be available to all expressions if needed.
    const Expr::Tag timeTag = TagNames::self().time;
    Expr::ExpressionID timeID;
    if( rkStage==1 ){
      Expr::TagList timeTags;
      timeTags.push_back( TagNames::self().time     );
      timeTags.push_back( TagNames::self().timestep );
      const Expr::Tag timeTag = TagNames::self().time;
      timeID = exprFactory.register_expression( scinew SetCurrentTime::Builder(timeTags), true );
    } else {
      timeID = exprFactory.get_id(timeTag);
    }

    //___________________________________________
    // Plug in each equation that has been set up
    if (rkStage==1) {
      for( EquationAdaptors::const_iterator ia=adaptors_.begin(); ia!=adaptors_.end(); ++ia ){
        const EqnTimestepAdaptorBase* const adaptor = *ia;
        try{
          adaptor->hook( *timeStepper_ );
        }
        catch( std::exception& e ){
          std::ostringstream msg;
          msg << "Problems plugging transport equation for '"
          << adaptor->equation()->solution_variable_name()
          << "' into the time integrator" << std::endl
          << e.what() << std::endl;
          proc0cout << msg.str() << endl;
          throw Uintah::ProblemSetupException( msg.str(), __FILE__, __LINE__ );
        }
      }
    }

    //____________________________________________________________________
    // create all of the required tasks on the timestepper.  This involves
    // the task(s) that compute(s) the RHS for each transport equation and
    // the task that updates the variables from time "n" to "n+1"
    timeStepper_->create_tasks( timeID, patchInfoMap_, localPatches,
                                materials, level, sched,
                                rkStage, lockedFields_ );
  }

  //--------------------------------------------------------------------

  void
  Wasatch::computeDelT( const Uintah::ProcessorGroup*,
                        const Uintah::PatchSubset* patches,
                        const Uintah::MaterialSubset* matls,
                        Uintah::DataWarehouse* old_dw,
                        Uintah::DataWarehouse* new_dw )
  {
    Uintah::delt_vartype deltat = 1.0;
    double val = 9999999999999.0;
    
    const GraphHelper* slnGraphHelper = graphCategories_[ADVANCE_SOLUTION];
    const TagNames& tagNames = TagNames::self();
    const bool useStableDT = slnGraphHelper->exprFactory->have_entry( tagNames.stableTimestep );
    if (sharedState_->getCurrentTopLevelTimeStep() > 0) {
      if (useStableDT) {
        //__________________
        // loop over patches
        for( int ip=0; ip<patches->size(); ++ip ){
          // grab the stable timestep value calculated by the StableDT expression
          Uintah::PerPatch<double*> tempDtP;
          new_dw->get(tempDtP, Uintah::VarLabel::find(tagNames.stableTimestep.name()), 0, patches->get(ip));          
          val = std::min( val, *tempDtP );
        }

      } else {
        // FOR FIXED dt: (min = max in input file)
        // if the this is not the first timestep, then grab dt from the olddw.
        // This will avoid Uintah's message that it is setting dt to max dt/min dt
        old_dw->get( deltat, sharedState_->get_delt_label() );
      }
    }
    
    if (useStableDT) {
      new_dw->put(Uintah::delt_vartype(val),sharedState_->get_delt_label(),
                  Uintah::getLevel(patches) );
    } else {
      new_dw->put( deltat,
                  sharedState_->get_delt_label(),
                  Uintah::getLevel(patches) );
    }
      //                   material );
      // jcs it seems that we cannot specify a material here.  Why not?
  }

  //------------------------------------------------------------------

  const Uintah::PatchSet*
  Wasatch::get_patchset( const PatchsetSelector pss,
                         const Uintah::LevelP& level,
                         Uintah::SchedulerP& sched )
  {
    switch ( pss ) {

    case USE_FOR_TASKS:
      // return sched->getLoadBalancer()->getPerProcessorPatchSet(level);
      return level->eachPatch();
      break;

    case USE_FOR_OPERATORS: {
      const Uintah::PatchSet* const allPatches = sched->getLoadBalancer()->getPerProcessorPatchSet(level);
      const Uintah::PatchSubset* const localPatches = allPatches->getSubset( d_myworld->myrank() );
      Uintah::PatchSet* patches = new Uintah::PatchSet;
      // jcs: this results in "normal" scheduling and WILL NOT WORK FOR LINEAR SOLVES
      //      in that case, we need to use "gang" scheduling: addAll( localPatches )
      patches->addEach( localPatches->getVector() );
      //     const std::set<int>& procs = sched->getLoadBalancer()->getNeighborhoodProcessors();
      //     for( std::set<int>::const_iterator ip=procs.begin(); ip!=procs.end(); ++ip ){
      //       patches->addEach( allPatches->getSubset( *ip )->getVector() );
      //     }
      patchSetList_[level] = patches;
      return patches;
    }
    }
    return NULL;
  }

 //------------------------------------------------------------------

 void
 Wasatch::scheduleCoarsen( const Uintah::LevelP& /*coarseLevel*/,
                           Uintah::SchedulerP& /*sched*/ )
 {
   // do nothing for now
 }

 //------------------------------------------------------------------

 void
 Wasatch::scheduleRefineInterface( const Uintah::LevelP& /*fineLevel*/,
                                   Uintah::SchedulerP& /*scheduler*/,
                                   bool, bool )
 {
   // do nothing for now
 }
//------------------------------------------------------------------

} // namespace Wasatch
