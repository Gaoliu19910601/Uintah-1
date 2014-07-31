/*
 * The MIT License
 *
 * Copyright (c) 2012 The University of Utah
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

#ifndef ParticleWallBC_Expr_h
#define ParticleWallBC_Expr_h

#include <expression/Expression.h>
#include <CCA/Components/Wasatch/TagNames.h>
#include "BoundaryConditionBase.h"

/**
 *  \class 	ParticleWallBC
 *  \ingroup 	Expressions
 *  \author 	Tony Saad
 *  \date    July, 2014
 *
 *  \brief Provides an expression to set Wall boundary conditions on particle variables
 *
 */
class ParticleWallBC
: public BoundaryConditionBase<ParticleField>
{
public:
  ParticleWallBC()
  {}
  
  class Builder : public Expr::ExpressionBuilder
  {
  public:
    /**
     * @param result Tag of the resulting expression.
     * @param bcValue   constant boundary condition value.
     */
    Builder( const Expr::Tag& resultTag) :
    ExpressionBuilder(resultTag)
    {}
    Expr::ExpressionBase* build() const{ return new ParticleWallBC(); }
  };
  
  ~ParticleWallBC(){}
  void advertise_dependents( Expr::ExprDeps& exprDeps ){}
  void bind_fields( const Expr::FieldManagerList& fml ){}
  void evaluate();
};

#endif // ParticleWallBC_Expr_h
