/* ***************************************************************** 
    MESQUITE -- The Mesh Quality Improvement Toolkit

    Copyright 2006 Sandia National Laboratories.  Developed at the
    University of Wisconsin--Madison under SNL contract number
    624796.  The U.S. Government and the University of Wisconsin
    retain certain rights to this software.

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2.1 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public License 
    (lgpl.txt) along with this library; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 
    (2006) kraftche@cae.wisc.edu
   
  ***************************************************************** */


/** \file Target2DShapeOrient.cpp
 *  \brief 
 *  \author Jason Kraftcheck 
 */

#include "Mesquite.hpp"
#include "Target2DShapeOrient.hpp"
#include "MsqMatrix.hpp"
#include "TMPDerivs.hpp"

namespace MESQUITE_NS {

std::string Target2DShapeOrient::get_name() const
  { return "ShapeOrient"; }

bool Target2DShapeOrient::evaluate( const MsqMatrix<2,2>& A, 
                                    const MsqMatrix<2,2>& W, 
                                    double& result, 
                                    MsqError&  )
{
  MsqMatrix<2,2> T = A * inverse(W);
  result = Frobenius( T ) - trace(T)/MSQ_SQRT_TWO;
  return true;
}

bool Target2DShapeOrient::evaluate_with_grad( const MsqMatrix<2,2>& A,
                                              const MsqMatrix<2,2>& W,
                                              double& result,
                                              MsqMatrix<2,2>& deriv,
                                              MsqError& err )
{
  const MsqMatrix<2,2> Winv = inverse(W);
  const MsqMatrix<2,2> T = A * Winv;
  const double norm = Frobenius(T);
  const double invroot = 1.0/MSQ_SQRT_TWO;
  result = norm - invroot * trace(T);
  
  if (norm < 1e-50) {
    deriv = MsqMatrix<2,2>(0.0);
    return true;
  }

  deriv = 1.0/norm * T;
  deriv(0,0) -= invroot;
  deriv(1,1) -= invroot;
  deriv = deriv * transpose(Winv);
  return true;
}

bool Target2DShapeOrient::evaluate_with_hess( const MsqMatrix<2,2>& A,
                                              const MsqMatrix<2,2>& W,
                                              double& result,
                                              MsqMatrix<2,2>& deriv,
                                              MsqMatrix<2,2> second[3],
                                              MsqError& err )
{
  const MsqMatrix<2,2> Winv = inverse(W);
  const MsqMatrix<2,2> T = A * Winv;
  const double norm = Frobenius(T);
  const double invroot = 1.0/MSQ_SQRT_TWO;
  result = norm - invroot * trace(T);
  
  if (norm < 1e-50) {
    deriv = second[1] = second[2] = second[4] = MsqMatrix<2,2>(0.0);
    second[0] = second[3] = second[5] = MsqMatrix<2,2>(1.0);
    second_deriv_wrt_product_factor( second, Winv );
    return true;
  }

  const double invnorm = 1.0/norm;
  deriv = invnorm * T;
  deriv(0,0) -= invroot;
  deriv(1,1) -= invroot;
  deriv = deriv * transpose(Winv);
  
  set_scaled_outer_product( second, -invnorm*invnorm*invnorm, T );
  pluseq_scaled_I( second, invnorm );
  second_deriv_wrt_product_factor( second, Winv );
  return true;
}


} // namespace Mesquite
