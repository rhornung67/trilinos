// //////////////////////////////////////////////////////////////////////////
// TSFCoreSolversGmresSolver.hpp
//

#ifndef TSFCORE_SOLVERS_GMRES_SOLVER_HPP
#define TSFCORE_SOLVERS_GMRES_SOLVER_HPP

#include "TSFCoreVectorSpace.hpp"
#include "TSFCoreVector.hpp"
#include "TSFCoreVectorStdOps.hpp"
#include "TSFCoreMultiVector.hpp"
#include "TSFCoreTypes.hpp"
#include "Teuchos_ScalarTraits.hpp"
#include "Teuchos_DenseMatrix.hpp"
#include "Teuchos_BLAS.hpp"

namespace TSFCore {
namespace Solvers {

template<class Scalar>
class GMRESSolver {

public:
  GMRESSolver( 	int		default_max_iter = 1000,
		Scalar		default_tol = 1e-10			
		);
				
  int currIteration() const { return curr_iter; };
  
  Scalar currEstRelResidualNorm() const { return curr_res; };
  
  void solve( const LinearOp<Scalar> &Op,
	      const Vector<Scalar>   &b,
	      Vector<Scalar>         *curr_soln,
	      const ETransp          trans_in,
	      int                    max_iter_in,
	      Scalar                 tol_in 
	      );	

  MemMngPack::ref_count_ptr<const GMRESSolver<Scalar> > clone () const;
	
private:

  void doIteration( const LinearOp<Scalar> &Op, const ETransp Op_trans );
  
  int				max_iter, curr_iter;
  bool				isConverged;
  Scalar			tol, curr_res, r0; 
  Scalar			*ptr_H, *z;
  MemMngPack::ref_count_ptr<Teuchos::DenseMatrix<int,Scalar> >
                                H_;
  MemMngPack::ref_count_ptr< MultiVector<Scalar> >
                                V_;
  MemMngPack::ref_count_ptr< Vector<Scalar> >
                                w, r;
  std::vector<Scalar>		cs, sn;	

}; // end class GMRESSolver


template<class Scalar>
GMRESSolver<Scalar>::GMRESSolver( int			default_max_iter,
				  Scalar		default_tol
				  )
    : max_iter(default_max_iter), tol(default_tol),
      isConverged(false), curr_iter(0), ptr_H(0), z(0), r0( 1.0 ), curr_res( 1.0 )
{	
    H_ = MemMngPack::rcp( new Teuchos::DenseMatrix<int,Scalar>() );
    H_->shape( max_iter+1, max_iter );
    ptr_H = H_->values();
    z = new Scalar[max_iter+1];
    cs.resize(max_iter); sn.resize(max_iter); 
}

	
template<class Scalar>
void GMRESSolver<Scalar>::solve( const LinearOp<Scalar> &Op,
				 const Vector<Scalar>   &b,
				 Vector<Scalar>         *curr_soln,
				 const ETransp          Op_trans,
				 int                    max_iter_in,
				 Scalar                 tol_in
				 )
{
    // 
    //  Check compatability of linear operator with rhs and solution vector.
    //
    const VectorSpace<Scalar> &Op_domain = ( ( Op_trans == NOTRANS ) ? *Op.domain() : *Op.range() );
    const VectorSpace<Scalar> &Op_range = ( ( Op_trans == NOTRANS ) ? *Op.range() : *Op.domain() );
    const bool 	domain_compatable = Op_domain.isCompatible( *curr_soln->space() ),
		range_compatable = Op_range.isCompatible( *b.space() );
    if (!(domain_compatable && range_compatable) ) { std::cout<<"Op is not compatable with x or b"<<std::endl; }
    //
    //  Initialize internal data structures
    //		
    V_ = Op_domain.createMembers(max_iter+1);
    r = Op_domain.createMember();
    //
    if ( tol_in != tol ) { tol = tol_in; }
    if ( max_iter_in != max_iter ) {
      max_iter = max_iter_in;
      H_->shape( max_iter+1, max_iter );
      delete [] z;
      ptr_H = H_->values();
      z = new Scalar[max_iter+1];
      cs.resize(max_iter); sn.resize(max_iter);
    }    
    //
    // Determine the residual from the current solution.
    //
    assign( r.get(), b );
    Op.apply( Op_trans, *curr_soln, r.get(), 1.0, -1.0 );
    curr_res = norm_2( *r ) / norm_2( b ); 	        
    if (curr_res < tol) { isConverged = true; }
    //
    // Set up initial vector.
    //
    r0 = norm_2( *r );
    z[0] = r0;
    w = V_->col(1);		// get a mutable view of the first column of V_.
    assign( w.get(), *r );		// copy r to the first column of V_.
    Vt_S( w.get(), 1.0/r0 );		// v_1 = r_0 / ||r_0||
    w = MemMngPack::null;
//
// Calls doIteration() using the current linear system parameters.
//
    while( !isConverged && (curr_iter < max_iter) ) { doIteration( Op, Op_trans ); }
//
// Solve least squares problem.
//
   Teuchos::BLAS<int, Scalar> blas;
   blas.TRSM(Teuchos::LEFT_SIDE, Teuchos::UPPER_TRI, Teuchos::NO_TRANS, Teuchos::NON_UNIT_DIAG, 
	     curr_iter, 1, 1.0, ptr_H, max_iter+1, z, max_iter+1 );
//
// Compute the new solution.
//
   for( int i = 0; i < curr_iter; i++ )
	Vp_StV( curr_soln, -z[i], *V_->col(i+1) );
}

template<class Scalar>
MemMngPack::ref_count_ptr<const GMRESSolver<Scalar> >
GMRESSolver<Scalar>::clone() const
{
	return MemMngPack::rcp( new GMRESSolver<Scalar>(*this) );
}

template<class Scalar>
void GMRESSolver<Scalar>::doIteration( const LinearOp<Scalar> &Op, const ETransp Op_trans )
{
    int i;
    Scalar temp;
    Teuchos::BLAS<int, Scalar> blas;
    Teuchos::DenseMatrix<int, Scalar> &H = *H_;
    // 
    w = V_->col(curr_iter+2);                             // w = v_{j+1}
    Op.apply( Op_trans, *V_->col(curr_iter+1), w.get() ); // w = Op * v_{j}	
    //
    // Perform MGS to orthogonalize new Krylov vector.
    //
    for( i=0; i<curr_iter+1; i++ ) {	
      H( i, curr_iter ) = dot( *w, *V_->col(i+1) );	// h_{i,j} = ( w, v_{i} )
      Vp_StV( w.get(), -H( i, curr_iter ), *V_->col(i+1) );// w = w - h_{i,j} * v_{i}
    }
    H( curr_iter+1, curr_iter ) = norm_2( *w );	// h_{j+1,j} = || w ||
    Vt_S( w.get(), 1.0 / H( curr_iter+1, curr_iter ) ); 	// v_{j+1} = w / h_{j+1,j}			
    //
    // Apply previous Givens rotations
    //
    for( i=0; i<curr_iter; i++ ) {
	temp = cs[i]*H( i, curr_iter ) + sn[i]*H( i+1, curr_iter );
	H( i+1, curr_iter ) = -sn[i]*H( i, curr_iter ) + cs[i]*H( i+1, curr_iter );
	H( i, curr_iter ) = temp;
    }
    //
    // Calculate new Givens rotation
    //
    blas.ROTG( &H( curr_iter, curr_iter ), &H( curr_iter+1, curr_iter ), 
	       &cs[curr_iter], &sn[curr_iter] );
    //
    // Update RHS and residual w/ new transform and compute residual.
    //
    z[curr_iter+1] = -sn[curr_iter]*z[curr_iter];
    z[curr_iter] *= cs[curr_iter];
    curr_res = Teuchos::ScalarTraits<Scalar>::magnitude( z[curr_iter+1] ) / r0; 
    if (curr_res < tol) { isConverged = true; }
    //    
    // Increment the iteration counter.
    //
    curr_iter++;
}

} // namespace Solvers
} // namespace TSFCore

#endif // TSFCORE_SOLVERS_GMRES_SOLVER_HPP
