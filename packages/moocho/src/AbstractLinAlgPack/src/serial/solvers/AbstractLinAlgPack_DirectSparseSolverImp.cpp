// @HEADER
// ***********************************************************************
// 
// Moocho: Multi-functional Object-Oriented arCHitecture for Optimization
//                  Copyright (2003) Sandia Corporation
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
// Questions? Contact Roscoe A. Bartlett (rabartl@sandia.gov) 
// 
// ***********************************************************************
// @HEADER

#include <assert.h>

#include "AbstractLinAlgPack_DirectSparseSolverImp.hpp"
#include "Teuchos_AbstractFactoryStd.hpp"
#include "Teuchos_Assert.hpp"
#include "Teuchos_dyn_cast.hpp"

namespace AbstractLinAlgPack {

// /////////////////////////////////////////
// DirectSparseSolverImp::BasisMatrixImp

DirectSparseSolverImp::BasisMatrixImp::BasisMatrixImp()
  : dim_(0)
{}

DirectSparseSolverImp::BasisMatrixImp::BasisMatrixImp(
  size_type                      dim
  ,const fact_struc_ptr_t        &fact_struc
  ,const fact_nonzeros_ptr_t     &fact_nonzeros
  )
{
  this->initialize(dim,fact_struc,fact_nonzeros);
}

void DirectSparseSolverImp::BasisMatrixImp::initialize(
  size_type                      dim
  ,const fact_struc_ptr_t        &fact_struc
  ,const fact_nonzeros_ptr_t     &fact_nonzeros
  )
{
#ifdef TEUCHOS_DEBUG
  const char msg_err[] = "DirectSparseSolverImp::BasisMatrixImp::initialize(...): Error!";
  TEUCHOS_TEST_FOR_EXCEPTION( dim < 0, std::logic_error, msg_err );
  TEUCHOS_TEST_FOR_EXCEPTION( fact_struc.get() == NULL, std::logic_error, msg_err );
  TEUCHOS_TEST_FOR_EXCEPTION( fact_nonzeros.get() == NULL, std::logic_error, msg_err );
#endif
  dim_            = dim;
  fact_struc_     = fact_struc;
  fact_nonzeros_  = fact_nonzeros;
  vec_space_.initialize(dim);
}

void DirectSparseSolverImp::BasisMatrixImp::set_uninitialized()
{
  dim_            = 0;
  fact_struc_     = Teuchos::null;
  fact_nonzeros_  = Teuchos::null;
  vec_space_.initialize(0);
}

const DirectSparseSolverImp::BasisMatrixImp::fact_nonzeros_ptr_t&
DirectSparseSolverImp::BasisMatrixImp::get_fact_nonzeros() const
{
  return fact_nonzeros_;
}

// Overridden from MatrixBase

const VectorSpace& DirectSparseSolverImp::BasisMatrixImp::space_cols() const
{
  return vec_space_;
}

const VectorSpace& DirectSparseSolverImp::BasisMatrixImp::space_rows() const
{
  return vec_space_;
}

size_type DirectSparseSolverImp::BasisMatrixImp::rows() const
{
  return dim_;
}

size_type DirectSparseSolverImp::BasisMatrixImp::cols() const
{
  return dim_;
}

// Overridden from MatrixNonsinguar

MatrixNonsing::mat_mns_mut_ptr_t
DirectSparseSolverImp::BasisMatrixImp::clone_mns()
{
  namespace rcp = MemMngPack;
  Teuchos::RCP<BasisMatrixImp> bm = this->create_matrix();
  // A shallow copy is okay if the educated client DirectSparseSolverImp is careful!
  bm->initialize(dim_,fact_struc_,fact_nonzeros_);
  return bm;
}

// Overridden from BasisMatrix

const DirectSparseSolver::BasisMatrix::fact_struc_ptr_t&
DirectSparseSolverImp::BasisMatrixImp::get_fact_struc() const
{
  return fact_struc_;
}

// //////////////////////////
// DirectSparseSolverImp

// Overridden from DirectSparseSolver

void DirectSparseSolverImp::analyze_and_factor(
  const AbstractLinAlgPack::MatrixConvertToSparse   &A
  ,DenseLinAlgPack::IVector                            *row_perm
  ,DenseLinAlgPack::IVector                            *col_perm
  ,size_type                                      *rank
  ,BasisMatrix                                    *basis_matrix
  ,std::ostream                                   *out
  )
{
  namespace mmp = MemMngPack;
  using Teuchos::dyn_cast;
#ifdef TEUCHOS_DEBUG
  const char msg_err[] = "DirectSparseSolverImp::analyze_and_factor(...): Error!";
  TEUCHOS_TEST_FOR_EXCEPTION( row_perm == NULL, std::logic_error, msg_err );
  TEUCHOS_TEST_FOR_EXCEPTION( col_perm == NULL, std::logic_error, msg_err );
  TEUCHOS_TEST_FOR_EXCEPTION( rank == NULL, std::logic_error, msg_err );
  TEUCHOS_TEST_FOR_EXCEPTION( basis_matrix == NULL, std::logic_error, msg_err );
#endif
  BasisMatrixImp
    &basis_matrix_imp = dyn_cast<BasisMatrixImp>(*basis_matrix);
  // Get reference to factorization structure object or determine that we have to
  // allocate a new factorization structure object.
  const BasisMatrix::fact_struc_ptr_t        &bm_fact_struc   = basis_matrix_imp.get_fact_struc();
  const BasisMatrix::fact_struc_ptr_t        &this_fact_struc = this->get_fact_struc();
  BasisMatrix::fact_struc_ptr_t              fact_struc;
  bool alloc_new_fact_struc    = false;
  if( bm_fact_struc.count() == 1 )
    fact_struc = bm_fact_struc;
  else if( this_fact_struc.count() == 1 )
    fact_struc = this_fact_struc;
  else if( bm_fact_struc.get() == this_fact_struc.get() && bm_fact_struc.count() == 2 )
    fact_struc = bm_fact_struc;
  else
    alloc_new_fact_struc = true;
  if( alloc_new_fact_struc )
    fact_struc = this->create_fact_struc();
  // Get references to factorization nonzeros object or allocate a new factorization nonzeros object.
  const BasisMatrixImp::fact_nonzeros_ptr_t    &bm_fact_nonzeros  = basis_matrix_imp.get_fact_nonzeros();
  BasisMatrixImp::fact_nonzeros_ptr_t          fact_nonzeros;
  if( bm_fact_nonzeros.count() == 1 )
    fact_nonzeros = bm_fact_nonzeros;
  else
    fact_nonzeros = this->create_fact_nonzeros();
  // Now ask the subclass to do the work
  this->imp_analyze_and_factor(
    A,fact_struc.get(),fact_nonzeros.get(),row_perm,col_perm,rank,out
    );
  // Setup the basis matrix
  basis_matrix_imp.initialize(*rank,fact_struc,fact_nonzeros);
  // Remember rank and factorization structure
  rank_       = *rank;
  fact_struc_ = fact_struc;
}

void DirectSparseSolverImp::factor(
  const AbstractLinAlgPack::MatrixConvertToSparse   &A
  ,BasisMatrix                                    *basis_matrix
  ,const BasisMatrix::fact_struc_ptr_t            &fact_struc_in
  ,std::ostream                                   *out
  )
{
  namespace mmp = MemMngPack;
  using Teuchos::dyn_cast;
#ifdef TEUCHOS_DEBUG
  const char msg_err[] = "DirectSparseSolverImp::analyze_and_factor(...): Error!";
  // ToDo: Validate that A is compatible!
  TEUCHOS_TEST_FOR_EXCEPTION( basis_matrix == NULL, std::logic_error, msg_err );
#endif
  BasisMatrixImp
    &basis_matrix_imp = dyn_cast<BasisMatrixImp>(*basis_matrix);
  // Get reference to factorization structure object
  const BasisMatrix::fact_struc_ptr_t        &this_fact_struc = this->get_fact_struc();
  BasisMatrix::fact_struc_ptr_t              fact_struc;
#ifdef TEUCHOS_DEBUG
  TEUCHOS_TEST_FOR_EXCEPTION(
    fact_struc_in.get() == NULL && this_fact_struc.get() == NULL
    ,std::logic_error
    ,msg_err );
#endif
  if( fact_struc_in.get() != NULL )
    fact_struc = fact_struc_in;
  else
    fact_struc = this_fact_struc;
  // Get references to factorization nonzeros object or allocate a new factorization nonzeros object.
  const BasisMatrixImp::fact_nonzeros_ptr_t    &bm_fact_nonzeros  = basis_matrix_imp.get_fact_nonzeros();
  BasisMatrixImp::fact_nonzeros_ptr_t          fact_nonzeros;
  if( bm_fact_nonzeros.count() == 1 )
    fact_nonzeros = bm_fact_nonzeros;
  else
    fact_nonzeros = this->create_fact_nonzeros();
  // Now ask the subclass to do the work
  this->imp_factor(A,*fact_struc,fact_nonzeros.get(),out);
  // Setup the basis matrix
  basis_matrix_imp.initialize(rank_,fact_struc,fact_nonzeros);
}

const DirectSparseSolver::BasisMatrix::fact_struc_ptr_t&
DirectSparseSolverImp::get_fact_struc() const
{
  return fact_struc_;
}

void DirectSparseSolverImp::set_uninitialized()
{
  fact_struc_ = Teuchos::null;
}

}	// end namespace AbstractLinAlgPack 
