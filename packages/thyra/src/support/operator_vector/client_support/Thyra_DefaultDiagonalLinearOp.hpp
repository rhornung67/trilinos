// @HEADER
// ***********************************************************************
// 
//    Thyra: Interfaces and Support for Abstract Numerical Algorithms
//                 Copyright (2004) Sandia Corporation
// 
// Under terms of Contract DE-AC04-94AL85000, there is a non-exclusive
// license for use of this work by or on behalf of the U.S. Government.
// 
// This library is free software; you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as
// published by the Free Software Foundation; either version 2.1 of the
// License, or (at your option) any later version.
//  
// This library is distributed in the hope that it will be useful, but
// WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
// Lesser General Public License for more details.
//  
// You should have received a copy of the GNU Lesser General Public
// License along with this library; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
// USA
// Questions? Contact Michael A. Heroux (maherou@sandia.gov) 
// 
// ***********************************************************************
// @HEADER

#ifndef THYRA_DIAGONAL_LINEAR_OP_HPP
#define THYRA_DIAGONAL_LINEAR_OP_HPP


#include "Thyra_DefaultDiagonalLinearOpDecl.hpp"
#include "Thyra_SingleRhsLinearOpWithSolveBase.hpp"
#include "Thyra_VectorBase.hpp"


namespace Thyra {


// Constructors/initializers/accessors


template<class Scalar>
DefaultDiagonalLinearOp<Scalar>::DefaultDiagonalLinearOp()
{}


template<class Scalar>
DefaultDiagonalLinearOp<Scalar>::DefaultDiagonalLinearOp(
  const RCP<const VectorSpaceBase<Scalar> > &space
  )
{
  initialize(space);
}


template<class Scalar>
DefaultDiagonalLinearOp<Scalar>::DefaultDiagonalLinearOp(
  const RCP<VectorBase<Scalar> > &diag
  )
{
  initialize(diag);
}


template<class Scalar>
DefaultDiagonalLinearOp<Scalar>::DefaultDiagonalLinearOp(
  const RCP<const VectorBase<Scalar> > &diag
  )
{
  initialize(diag);
}


template<class Scalar>
void DefaultDiagonalLinearOp<Scalar>::initialize(
  const RCP<const VectorSpaceBase<Scalar> > &space
  )
{
#ifdef TEUCHOS_DEBUG
  TEST_FOR_EXCEPT(space.get()==NULL);
#endif
  initialize(createMember(space)); // Note that the space is guaranteed to be remembered here!
}


template<class Scalar>
void DefaultDiagonalLinearOp<Scalar>::initialize(
  const RCP<VectorBase<Scalar> > &diag
  )
{
  diag_.initialize(diag);
}


template<class Scalar>
void DefaultDiagonalLinearOp<Scalar>::initialize(
  const RCP<const VectorBase<Scalar> > &diag
  )
{
  diag_.initialize(diag);
}


template<class Scalar>
void DefaultDiagonalLinearOp<Scalar>::uninitialize()
{
  diag_.uninitialize();
}


// Overridden from DiagonalLinearOpBase


template<class Scalar>
bool DefaultDiagonalLinearOp<Scalar>::isDiagConst() const
{
  return diag_.isConst();
}


template<class Scalar>
RCP<VectorBase<Scalar> > 
DefaultDiagonalLinearOp<Scalar>::getNonconstDiag()
{
  return diag_.getNonconstObj();
}


template<class Scalar>
RCP<const VectorBase<Scalar> > 
DefaultDiagonalLinearOp<Scalar>::getDiag() const
{
  return diag_.getConstObj();
}


// Overridden from LinearOpBase


template<class Scalar>
RCP< const VectorSpaceBase<Scalar> >
DefaultDiagonalLinearOp<Scalar>::range() const
{
  return diag_.getConstObj()->space();
}


template<class Scalar>
RCP< const VectorSpaceBase<Scalar> >
DefaultDiagonalLinearOp<Scalar>::domain() const
{
  return diag_.getConstObj()->space();
}


template<class Scalar>
RCP<const LinearOpBase<Scalar> >
DefaultDiagonalLinearOp<Scalar>::clone() const
{
  return Teuchos::rcp(new DefaultDiagonalLinearOp<Scalar>(diag_.getConstObj()->clone_v()));
}


// protected


// Overridden from SingleScalarLinearOpBase


template<class Scalar>
bool DefaultDiagonalLinearOp<Scalar>::opSupported(EOpTransp M_trans) const
{
  return true;
}


// Overridden from SingleRhsLinearOpBase


template<class Scalar>
void DefaultDiagonalLinearOp<Scalar>::apply(
  const EOpTransp M_trans
  ,const VectorBase<Scalar> &x
  ,VectorBase<Scalar> *y
  ,const Scalar alpha
  ,const Scalar beta
  ) const
{
  typedef Teuchos::ScalarTraits<Scalar> ST;
#ifdef TEUCHOS_DEBUG
  THYRA_ASSERT_LINEAR_OP_VEC_APPLY_SPACES(
    "DefaultDiagonalLinearOp<Scalar>::apply(...)",*this,M_trans,x,y
    );
#endif // TEUCHOS_DEBUG
  if( beta != ST::one() ) Vt_S( y, beta );
  if (ST::isComplex) {
    if ( M_trans==NOTRANS || M_trans==TRANS ) {
      ele_wise_prod( alpha, *diag_.getConstObj(), x, y );
    }
    else {
      ele_wise_conj_prod( alpha, *diag_.getConstObj(), x, y );
    }
  }
  else {
    ele_wise_prod( alpha, *diag_.getConstObj(), x, y );
  }
}


}	// end namespace Thyra


#endif	// THYRA_DIAGONAL_LINEAR_OP_HPP
