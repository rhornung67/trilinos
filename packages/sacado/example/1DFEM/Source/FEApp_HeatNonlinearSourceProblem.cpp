// $Id$ 
// $Source$ 
// @HEADER
// ***********************************************************************
// 
//                           Sacado Package
//                 Copyright (2006) Sandia Corporation
// 
// Under the terms of Contract DE-AC04-94AL85000 with Sandia Corporation,
// the U.S. Government retains certain rights in this software.
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
// Questions? Contact David M. Gay (dmgay@sandia.gov) or Eric T. Phipps
// (etphipp@sandia.gov).
// 
// ***********************************************************************
// @HEADER

#include "FEApp_HeatNonlinearSourceProblem.hpp"
#include "FEApp_ConstantDirichletBC.hpp"

FEApp::HeatNonlinearSourceProblem::HeatNonlinearSourceProblem(
		 const Teuchos::RefCountPtr<Teuchos::ParameterList>& params_) :
  params(params_)
{
  leftBC = params->get("Left BC", 0.0);
  rightBC = params->get("Right BC", 0.0);
}

FEApp::HeatNonlinearSourceProblem::~HeatNonlinearSourceProblem()
{
}

unsigned int
FEApp::HeatNonlinearSourceProblem::numEquations() const
{
  return 1;
}

void
FEApp::HeatNonlinearSourceProblem:: buildPDEs(
		       FEApp::AbstractPDE_TemplateManager<ValidTypes>& pdeTM)
{
  FEApp::HeatNonlinearSourcePDE_TemplateBuilder pdeBuilder(params);
  pdeTM.buildObjects(pdeBuilder);
}

std::vector< Teuchos::RefCountPtr<const FEApp::AbstractBC> >
FEApp::HeatNonlinearSourceProblem::buildBCs(const Epetra_Map& dofMap)
{
  std::vector< Teuchos::RefCountPtr<const FEApp::AbstractBC> > bc(2);
  bc[0] = Teuchos::rcp(new FEApp::ConstantDirichletBC(dofMap.MinAllGID(),
						      leftBC));
  bc[1] = Teuchos::rcp(new FEApp::ConstantDirichletBC(dofMap.MaxAllGID(),
						      rightBC));

  return bc;
}

Teuchos::RefCountPtr<Epetra_Vector>
FEApp::HeatNonlinearSourceProblem::buildInitialSolution(
						     const Epetra_Map& dofMap)
{
  Teuchos::RefCountPtr<Epetra_Vector> u =
    Teuchos::rcp(new Epetra_Vector(dofMap, false));
  u->PutScalar(0.0);

  return u;
}
