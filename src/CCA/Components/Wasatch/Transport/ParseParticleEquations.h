/*
 * The MIT License
 *
 * Copyright (c) 2012-2016 The University of Utah
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

#ifndef Wasatch_ParseParticleEquations_h
#define Wasatch_ParseParticleEquations_h

#include <Core/ProblemSpec/ProblemSpec.h>
#include <Core/ProblemSpec/ProblemSpecP.h>
#include <CCA/Ports/SolverInterface.h>

//-- Wasatch includes --//
#include <CCA/Components/Wasatch/Transport/EquationAdaptors.h>
#include <CCA/Components/Wasatch/Transport/ParticleSizeEquation.h>
#include <CCA/Components/Wasatch/Transport/ParticleMassEquation.h>
#include <CCA/Components/Wasatch/Transport/ParticlePositionEquation.h>
#include <CCA/Components/Wasatch/Transport/ParticleMomentumEquation.h>
#include <CCA/Components/Wasatch/Expressions/Particles/ParticleGasMomentumSrc.h>
#include <CCA/Components/Wasatch/Expressions/Turbulence/TurbulenceParameters.h>

namespace WasatchCore{


  template<typename GasVel1T, typename GasVel2T, typename GasVel3T>
  std::vector<EqnTimestepAdaptorBase*>
  parse_particle_transport_equations( Uintah::ProblemSpecP particleSpec,
                                      Uintah::ProblemSpecP wasatchSpec,
                                      const bool useAdaptiveDt,
                                      GraphCategories& gc)
  {
    typedef std::vector<EqnTimestepAdaptorBase*> EquationAdaptors;
    EquationAdaptors adaptors;
    
    std::string pxname,pyname,pzname;
    Uintah::ProblemSpecP posSpec = particleSpec->findBlock("ParticlePosition");
    posSpec->getAttribute( "x", pxname );
    posSpec->getAttribute( "y", pyname );
    posSpec->getAttribute( "z", pzname );
    
    const Expr::Tag pXTag(pxname,Expr::STATE_DYNAMIC);
    const Expr::Tag pYTag(pyname,Expr::STATE_DYNAMIC);
    const Expr::Tag pZTag(pzname,Expr::STATE_DYNAMIC);
    const Expr::TagList pPosTags( tag_list(pXTag,pYTag,pZTag) );
    
    const Expr::Tag pSizeTag = parse_nametag(particleSpec->findBlock("ParticleSize"));
    const std::string pSizeName=pSizeTag.name();
    
    //___________________________________________________________________________
    // resolve the particle equations
    //
    proc0cout << "------------------------------------------------" << std::endl
    << "Creating particle equations..." << std::endl;
    proc0cout << "------------------------------------------------" << std::endl;
    
    proc0cout << "Setting up particle x-coordinate equation" << std::endl;
    EquationBase* pxeq = new ParticlePositionEquation( pxname,
                                                         XDIR,
                                                         pPosTags,
                                                         pSizeTag,
                                                         particleSpec,
                                                         gc );
    adaptors.push_back( new EqnTimestepAdaptor<ParticleField>(pxeq) );
    
    proc0cout << "Setting up particle y-coordinate equation" << std::endl;
    EquationBase* pyeq = new ParticlePositionEquation( pyname,
                                                         YDIR,
                                                         pPosTags,
                                                         pSizeTag,
                                                         particleSpec,
                                                         gc );
    adaptors.push_back( new EqnTimestepAdaptor<ParticleField>(pyeq) );
    
    proc0cout << "Setting up particle z-coordinate equation" << std::endl;
    EquationBase* pzeq = new ParticlePositionEquation( pzname,
                                                         ZDIR,
                                                         pPosTags,
                                                         pSizeTag,
                                                         particleSpec,
                                                         gc );
    adaptors.push_back( new EqnTimestepAdaptor<ParticleField>(pzeq) );
    
    
    std::string puname,pvname,pwname;
    Uintah::ProblemSpecP pMomSpec = particleSpec->findBlock("ParticleMomentum");
    pMomSpec->getAttribute( "x", puname );
    pMomSpec->getAttribute( "y", pvname );
    pMomSpec->getAttribute( "z", pwname );
    
    //___________________________________________________________________________
    // resolve the particle mass equation to be solved and create the adaptor for it.
    //
    const Expr::Tag pMassTag    = parse_nametag(particleSpec->findBlock("ParticleMass"));
    const std::string pMassName = pMassTag.name();
    proc0cout << "Setting up particle mass equation" << std::endl;
    EquationBase* pmeq = new ParticleMassEquation( pMassName,
                                                     NODIR,
                                                     pPosTags,
                                                     pSizeTag,
                                                     particleSpec,
                                                     gc );
    adaptors.push_back( new EqnTimestepAdaptor<ParticleField>(pmeq) );
    
    //___________________________________________________________________________
    // resolve the momentum equation to be solved and create the adaptor for it.
    //
    Expr::ExpressionFactory& factory = *(gc[ADVANCE_SOLUTION]->exprFactory);
    proc0cout << "Setting up particle x-momentum equation" << std::endl;
    EquationBase* pueq = new ParticleMomentumEquation<GasVel1T,GasVel2T,GasVel3T>( puname,
                                                                                     XDIR,
                                                                                     pPosTags,
                                                                                     pSizeTag,
                                                                                     particleSpec,
                                                                                     gc );
    adaptors.push_back( new EqnTimestepAdaptor<ParticleField>(pueq) );
    
    proc0cout << "Setting up particle y-momentum equation" << std::endl;
    EquationBase* pveq = new ParticleMomentumEquation<GasVel1T,GasVel2T,GasVel3T>( pvname,
                                                                                     YDIR,
                                                                                     pPosTags,
                                                                                     pSizeTag,
                                                                                     particleSpec,
                                                                                     gc );
    adaptors.push_back( new EqnTimestepAdaptor<ParticleField>(pveq) );
    
    proc0cout << "Setting up particle z-momentum equation" << std::endl;
    EquationBase* pweq = new ParticleMomentumEquation<GasVel1T,GasVel2T,GasVel3T>( pwname,
                                                                                     ZDIR,
                                                                                     pPosTags,
                                                                                     pSizeTag,
                                                                                     particleSpec,
                                                                                     gc );
    adaptors.push_back( new EqnTimestepAdaptor<ParticleField>(pweq) );
    
    //___________________________________________________________________________
    // resolve the particle size equation to be solved and create the adaptor for it.
    //
    proc0cout << "Setting up particle size equation" << std::endl;
    EquationBase* psizeeq = new ParticleSizeEquation( pSizeName,
                                                        NODIR,
                                                        pPosTags,
                                                        pSizeTag,
                                                        particleSpec,
                                                        gc );
    adaptors.push_back( new EqnTimestepAdaptor<ParticleField>(psizeeq) );
    
    //___________________________________________________________________________
    // Two way coupling between particles and the gas phase
    //
    if (!particleSpec->findBlock("ParticleMomentum")->findBlock("DisableTwoWayCoupling"))
    {
      Uintah::ProblemSpecP momentumSpec  = wasatchSpec->findBlock("MomentumEquations");
      if (momentumSpec) {
        
        std::string xmomname, ymomname, zmomname;
        const Uintah::ProblemSpecP doxmom = momentumSpec->get( "X-Momentum", xmomname );
        const Uintah::ProblemSpecP doymom = momentumSpec->get( "Y-Momentum", ymomname );
        const Uintah::ProblemSpecP dozmom = momentumSpec->get( "Z-Momentum", zmomname );
        
        const TagNames tNames = TagNames::self();
        if (doxmom) {
          typedef ParticleGasMomentumSrc<XVolField>::Builder XMomSrcT;
          const Expr::Tag xMomRHSTag (xmomname + "_rhs_partial", Expr::STATE_NONE);
          factory.register_expression( new XMomSrcT( tNames.pmomsrcx, tNames.pdragx, pMassTag, pSizeTag, pPosTags ));
          factory.attach_dependency_to_expression(tNames.pmomsrcx, xMomRHSTag);
        }
        
        if (doymom) {
          typedef ParticleGasMomentumSrc<YVolField>::Builder YMomSrcT;
          const Expr::Tag yMomRHSTag (ymomname + "_rhs_partial", Expr::STATE_NONE);
          factory.register_expression( new YMomSrcT( tNames.pmomsrcy, tNames.pdragy, pMassTag, pSizeTag, pPosTags ));
          factory.attach_dependency_to_expression(tNames.pmomsrcy, yMomRHSTag);
        }
        
        if (dozmom) {
          typedef ParticleGasMomentumSrc<ZVolField>::Builder ZMomSrcT;
          const Expr::Tag zMomRHSTag (zmomname + "_rhs_partial", Expr::STATE_NONE);
          factory.register_expression( new ZMomSrcT( tNames.pmomsrcz, tNames.pdragz, pMassTag, pSizeTag, pPosTags ));
          factory.attach_dependency_to_expression(tNames.pmomsrcz, zMomRHSTag);
        }
      }
    }
    
    //
    // loop over the local adaptors and set the initial and boundary conditions on each equation attached to that adaptor
    for( EquationAdaptors::const_iterator ia=adaptors.begin(); ia!=adaptors.end(); ++ia ){
      EqnTimestepAdaptorBase* const adaptor = *ia;
      EquationBase* particleEq = adaptor->equation();
      //_____________________________________________________
      // set up initial conditions on this momentum equation
      try{
        proc0cout << "Setting initial conditions for particle equation: "
        << particleEq->solution_variable_name()
        << std::endl;
        GraphHelper* const icGraphHelper = gc[INITIALIZATION];
        icGraphHelper->rootIDs.insert( particleEq->initial_condition( *icGraphHelper->exprFactory ) );
      }
      catch( std::runtime_error& e ){
        std::ostringstream msg;
        msg << e.what()
        << std::endl
        << "ERORR while setting initial conditions on particle equation "
        << particleEq->solution_variable_name()
        << std::endl;
        throw Uintah::ProblemSetupException( msg.str(), __FILE__, __LINE__ );
      }
    }
    
    proc0cout << "------------------------------------------------" << std::endl;
    
    //
    // ADD ADAPTIVE TIMESTEPPING in case it was not parsed in the momentum equations
    if( useAdaptiveDt ){
      // if no stabletimestep expression has been registered, then register one. otherwise return.
      if (!factory.have_entry(TagNames::self().stableTimestep)) {
        
        std::string gasViscosityName, gasDensityName;
        Uintah::ProblemSpecP gasSpec = particleSpec->findBlock("ParticleMomentum")->findBlock("GasProperties");
        gasSpec->findBlock("GasViscosity")->getAttribute( "name", gasViscosityName );
        gasSpec->findBlock("GasDensity")->getAttribute( "name", gasDensityName );
        Uintah::ProblemSpecP gasVelSpec = gasSpec->findBlock("GasVelocity");
        std::string uVelName, vVelName, wVelName;
        gasVelSpec->findBlock("XVel")->getAttribute("name",uVelName);
        gasVelSpec->findBlock("YVel")->getAttribute("name",vVelName);
        gasVelSpec->findBlock("ZVel")->getAttribute("name",wVelName);
        const Expr::Tag xVelTag = Expr::Tag(uVelName, Expr::STATE_NONE);
        const Expr::Tag yVelTag = Expr::Tag(vVelName, Expr::STATE_NONE);
        const Expr::Tag zVelTag = Expr::Tag(wVelName, Expr::STATE_NONE);
        const Expr::Tag viscTag = Expr::Tag(gasViscosityName, Expr::STATE_NONE);
        const Expr::Tag densityTag = Expr::Tag(gasDensityName, Expr::STATE_NONE);
        
        const Expr::Tag puTag = Expr::Tag(puname, Expr::STATE_DYNAMIC);
        const Expr::Tag pvTag = Expr::Tag(pvname, Expr::STATE_DYNAMIC);
        const Expr::Tag pwTag = Expr::Tag(pwname, Expr::STATE_DYNAMIC);
        
        const Expr::ExpressionID stabDtID = factory.register_expression(new StableTimestep::Builder( TagNames::self().stableTimestep,
                                                                                                       densityTag,
                                                                                                       viscTag,
                                                                                                       xVelTag,yVelTag,zVelTag, puTag, pvTag, pwTag ), true);
        // force this onto the graph.
        gc[ADVANCE_SOLUTION]->rootIDs.insert( stabDtID );
      }
    }
    
    //
    return adaptors;
  }
}// namespace WasatchCore

#endif // Wasatch_ParseEquations_h
