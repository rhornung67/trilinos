// @HEADER
// ************************************************************************
//
//           Galeri: Finite Element and Matrix Generation Package
//                 Copyright (2006) ETHZ/Sandia Corporation
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
//
// Questions about Galeri? Contact Marzio Sala (marzio.sala _AT_ gmail.com)
//
// ************************************************************************
// @HEADER

#ifndef GALERI_ABSTRACTVARIATIONAL_H
#define GALERI_ABSTRACTVARIATIONAL_H

/*!
 * \file Galeri_AbstractVariational.h
 */
namespace Galeri {
namespace FiniteElements {

/*!
 * \class AbstractVariational
 *
 * \brief Pure virtual class that defines the variational form.
 *
AbstractVariational is a pure virtual class, that specifies a set
of abstract interfaces, required to integrate the variational form
and the right-hand side over an element, and to compute the norm
of compute solution, exact solution, and error over the element.
A concrete implementation of this class also defined how boundary
conditions are resolved.

The element on which the integration is performed is specified by 
providing the coordinates of the local vertices.
 *
 * \author Marzio Sala, SNL 9214.
 *
 * \date Last updated on Apr-05.
 */

class AbstractVariational
{
public:

  // @{ \name Constructors and destructors
  //! Destructor.
  virtual ~AbstractVariational () {}

  // @}
  // @{ \name Bilinear form and right-hand side
  
  //! Evaluates the bilinear form (without integral) at point (x,y,z).
  virtual double LHS(const double Phi, const double Psi,
                     const double PhiX, const double PsiX,
                     const double PhiY, const double PsiY,
                     const double PhiZ, const double PsiZ,
                     const double x, const double y, const double z) const = 0;

  //! Returns the value of the right-hand side (without integral) at point (x, y, z).
  virtual double RHS(const double Psi, const double PsiX,
                     const double PsiY, const double PsiZ, 
                     const double x, const double y, const double z) const = 0;

  // @}
  // @{ \name Boundary conditions
  
  //! Returns an integer identifying the boundary condition assigned to the specified patch.
  virtual int BC(const int PatchID) const = 0;

  //! Returns the value of the boundary condition at point (x, y, z).
  virtual double BC(const double x, const double y, const double z,
                    const int PatchID) const = 0;

  // @}
  // @{ \name Integrations and norms

  //! Integrates the bilinear form and the right-hand side over the element.
  virtual int IntegrateOverElement(const AbstractVariational& Variational,
				   const double* x, const double* y, const double* z,
                                   const double* data,
				   double* ElementMatrix, double* ElementRHS) const = 0;

  //! Computes the norm of the computed solution over the element.
  virtual int ElementNorm(const double* LocalSol, const double* x, 
                          const double* y, const double* z, double* Norm) const = 0;

  //! Computed the norm of the exact solution over the element.
  virtual int ElementNorm(int (*ExactSolution)(double, double, double, double *),
			  const double* x, const double* y, const double* z,
			  double* Norm) const = 0;

  //! Computed the norm of the computed and exact solution over the element.
  virtual int ElementNorm(const double* LocalSol,
			  int (*ExactSolution)(double, double, double, double *),
			  const double* x, const double* y, const double* z, double * Norm) const = 0;
  
  // @}
};

} // namespace FiniteElements
} // namespace Galeri
#endif
