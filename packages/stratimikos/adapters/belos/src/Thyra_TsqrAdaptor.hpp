// @HEADER
// ***********************************************************************
// 
//         Stratimikos: Thyra-based strategies for linear solvers
//                Copyright (2006) Sandia Corporation
// 
// Under terms of Contract DE-AC04-94AL85000, there is a non-exclusive
// license for use of this work by or on behalf of the U.S. Government.
// 
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
// 1. Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//
// 2. Redistributions in binary form must reproduce the above copyright
// notice, this list of conditions and the following disclaimer in the
// documentation and/or other materials provided with the distribution.
//
// 3. Neither the name of the Corporation nor the names of the
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY SANDIA CORPORATION "AS IS" AND ANY
// EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
// PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL SANDIA CORPORATION OR THE
// CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
// EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
// PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
// PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
// LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
// NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
// SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//
// Questions? Contact Roscoe A. Bartlett (rabartl@sandia.gov) 
// 
// ***********************************************************************
// @HEADER

#ifndef __Thyra_TsqrAdaptor_hpp
#define __Thyra_TsqrAdaptor_hpp

#include <BelosConfigDefs.hpp>

#include <Thyra_DetachedMultiVectorView.hpp>
#include <Thyra_MultiVectorBase.hpp>
#include <Thyra_MultiVectorStdOps.hpp>
#include <Teuchos_ParameterListAcceptorDefaultBase.hpp>
#include <stdexcept>


namespace Thyra {

  /// \class TsqrAdaptor
  /// \brief Stub adaptor from Thyra::MultiVectorBase to TSQR
  ///
  /// TSQR (Tall Skinny QR factorization) is an orthogonalization
  /// kernel that is as accurate as Householder QR, yet requires only
  /// \f$2 \log P\f$ messages between $P$ MPI processes, independently
  /// of the number of columns in the multivector.  
  ///
  /// TSQR works independently of the particular multivector
  /// implementation, and interfaces to the latter via an adaptor
  /// class.  This class is the adaptor class for \c MultiVectorBase.
  /// It templates on the MultiVector (MV) type so that it can pick up
  /// that class' typedefs.  In particular, TSQR chooses its intranode
  /// implementation based on the Kokkos Node type of the multivector.
  ///
  /// \warning This is a stub adaptor that just placates the compiler
  ///   and does nothing.  It's not hard to implement a Thyra adaptor,
  ///   but in order for the adaptor to be efficient, it requires
  ///   special cases for extracting the actual multivector
  ///   implementation (e.g., Epetra_MultiVector or
  ///   Tpetra::MultiVector) out of the Thyra wrapper.
  template<class Scalar>
  class TsqrAdaptor : public Teuchos::ParameterListAcceptorDefaultBase {
  public:
    typedef Thyra::MultiVectorBase<Scalar> MV;
    typedef Scalar scalar_type;
    typedef int ordinal_type; // MultiVectorBase really does use int for this
    typedef int node_type; // FIXME (mfh 26 Oct 2010) stub for now
    typedef Teuchos::SerialDenseMatrix<ordinal_type, scalar_type> dense_matrix_type;
    typedef typename Teuchos::ScalarTraits<scalar_type>::magnitudeType magnitude_type;

  public:
    /// \brief Constructor (that accepts a parameter list).
    ///
    /// \param plist [in] List of parameters for configuring TSQR.
    ///   The specific parameter keys that are read depend on the TSQR
    ///   implementation.  For details, call \c getValidParameters()
    ///   and examine the documentation embedded therein.
    TsqrAdaptor (const Teuchos::RCP<Teuchos::ParameterList>& plist) 
    {
      throw std::logic_error ("Thyra adaptor for TSQR not implemented");
    }

    //! Constructor (that uses default parameters).
    TsqrAdaptor ()
    {
      throw std::logic_error ("Thyra adaptor for TSQR not implemented");
    }

    Teuchos::RCP<const Teuchos::ParameterList>
    getValidParameters () const
    {
      throw std::logic_error ("Thyra adaptor for TSQR not implemented");
    }

    void 
    setParameterList (const Teuchos::RCP<Teuchos::ParameterList>& plist)
    {
      throw std::logic_error ("Thyra adaptor for TSQR not implemented");
    }

    /// \brief Compute QR factorization [Q,R] = qr(A,0).
    ///
    /// \param A [in/out] On input: the multivector to factor.
    ///   Overwritten with garbage on output.
    ///
    /// \param Q [out] On output: the (explicitly stored) Q factor in
    ///   the QR factorization of the (input) multivector A.
    ///
    /// \param R [out] On output: the R factor in the QR factorization
    ///   of the (input) multivector A.
    ///
    /// \param forceNonnegativeDiagonal [in] If true, then (if
    ///   necessary) do extra work (modifying both the Q and R
    ///   factors) in order to force the R factor to have a
    ///   nonnegative diagonal.
    ///
    /// \warning Currently, this method only works if A and Q have the
    ///   same communicator and row distribution ("map," in Petra
    ///   terms) as those of the multivector given to this TsqrAdaptor
    ///   instance's constructor.  Otherwise, the result of this
    ///   method is undefined.
    void
    factorExplicit (MV& A,
		    MV& Q,
		    dense_matrix_type& R,
		    const bool forceNonnegativeDiagonal=false)
    {
      throw std::logic_error ("Thyra adaptor for TSQR not implemented");
    }

    /// \brief Rank-revealing decomposition
    ///
    /// Using the R factor and explicit Q factor from
    /// factorExplicit(), compute the singular value decomposition
    /// (SVD) of R (\f$R = U \Sigma V^*\f$).  If R is full rank (with
    /// respect to the given relative tolerance tol), don't change Q
    /// or R.  Otherwise, compute \f$Q := Q \cdot U\f$ and \f$R :=
    /// \Sigma V^*\f$ in place (the latter may be no longer upper
    /// triangular).
    ///
    /// \param Q [in/out] On input: explicit Q factor computed by
    ///   factorExplicit().  (Must be an orthogonal resp. unitary
    ///   matrix.)  On output: If R is of full numerical rank with
    ///   respect to the tolerance tol, Q is unmodified.  Otherwise, Q
    ///   is updated so that the first rank columns of Q are a basis
    ///   for the column space of A (the original matrix whose QR
    ///   factorization was computed by factorExplicit()).  The
    ///   remaining columns of Q are a basis for the null space of A.
    ///
    /// \param R [in/out] On input: ncols by ncols upper triangular
    ///   matrix with leading dimension ldr >= ncols.  On output: if
    ///   input is full rank, R is unchanged on output.  Otherwise, if
    ///   \f$R = U \Sigma V^*\f$ is the SVD of R, on output R is
    ///   overwritten with $\Sigma \cdot V^*$.  This is also an ncols by
    ///   ncols matrix, but may not necessarily be upper triangular.
    ///
    /// \param tol [in] Relative tolerance for computing the numerical
    ///   rank of the matrix R.
    ///
    /// \return Rank \f$r\f$ of R: \f$ 0 \leq r \leq ncols\f$.
    ///
    int
    revealRank (MV& Q,
		dense_matrix_type& R,
		const magnitude_type& tol)
    {
      return 0; // FIXME (mfh 26 Oct 2010) Stub implementation
    }
  };

} // namespace Tpetra

#endif // __Thyra_TsqrAdaptor_hpp

