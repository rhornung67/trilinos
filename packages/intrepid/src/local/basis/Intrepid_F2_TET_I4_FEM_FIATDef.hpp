// @HEADER
// ************************************************************************
//
//                           Intrepid Package
//                 Copyright (2007) Sandia Corporation
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
// Questions? Contact Pavel Bochev (pbboche@sandia.gov) or
//                    Denis Ridzal (dridzal@sandia.gov) or
//                    Robert Kirby (robert.c.kirby@ttu.edu)
//                    FIAT (fiat-dev@fenics.org) (must join mailing list first, see www.fenics.org)
//
// ************************************************************************
// @HEADER

/** \file   Intrepid_F2_TET_I4_FEM_FIATDef.hpp
    \brief  Definition file for FEM basis functions of degree 3 for 2-forms on TET cells.
    \author Created by R. Kirby via the FIAT project
*/

namespace Intrepid {

template<class Scalar>
void Basis_F2_TET_I4_FEM_FIAT<Scalar>::initialize() {

  // Basis-dependent initializations
  int tagSize  = 4;         // size of DoF tag
  int posScDim = 0;         // position in the tag, counting from 0, of the subcell dim 
  int posScId  = 1;         // position in the tag, counting from 0, of the subcell id
  int posBfId  = 2;         // position in the tag, counting from 0, of DoF Id relative to the subcell

  // An array with local DoF tags assigned to the basis functions, in the order of their local enumeration 
  int tags[] = { 2 , 0 , 0 , 10 , 2 , 0 , 1 , 10 , 2 , 0 , 2 , 10 , 2 , 0 , 3 , 10 , 2 , 0 , 4 , 10 , 2 , 0 , 5 , 10 , 2 , 0 , 6 , 10 , 2 , 0 , 7 , 10 , 2 , 0 , 8 , 10 , 2 , 0 , 9 , 10 , 2 , 1 , 0 , 10 , 2 , 1 , 1 , 10 , 2 , 1 , 2 , 10 , 2 , 1 , 3 , 10 , 2 , 1 , 4 , 10 , 2 , 1 , 5 , 10 , 2 , 1 , 6 , 10 , 2 , 1 , 7 , 10 , 2 , 1 , 8 , 10 , 2 , 1 , 9 , 10 , 2 , 2 , 0 , 10 , 2 , 2 , 1 , 10 , 2 , 2 , 2 , 10 , 2 , 2 , 3 , 10 , 2 , 2 , 4 , 10 , 2 , 2 , 5 , 10 , 2 , 2 , 6 , 10 , 2 , 2 , 7 , 10 , 2 , 2 , 8 , 10 , 2 , 2 , 9 , 10 , 2 , 3 , 0 , 10 , 2 , 3 , 1 , 10 , 2 , 3 , 2 , 10 , 2 , 3 , 3 , 10 , 2 , 3 , 4 , 10 , 2 , 3 , 5 , 10 , 2 , 3 , 6 , 10 , 2 , 3 , 7 , 10 , 2 , 3 , 8 , 10 , 2 , 3 , 9 , 10 , 3 , 0 , 0 , 30 , 3 , 0 , 1 , 30 , 3 , 0 , 2 , 30 , 3 , 0 , 3 , 30 , 3 , 0 , 4 , 30 , 3 , 0 , 5 , 30 , 3 , 0 , 6 , 30 , 3 , 0 , 7 , 30 , 3 , 0 , 8 , 30 , 3 , 0 , 9 , 30 , 3 , 0 , 10 , 30 , 3 , 0 , 11 , 30 , 3 , 0 , 12 , 30 , 3 , 0 , 13 , 30 , 3 , 0 , 14 , 30 , 3 , 0 , 15 , 30 , 3 , 0 , 16 , 30 , 3 , 0 , 17 , 30 , 3 , 0 , 18 , 30 , 3 , 0 , 19 , 30 , 3 , 0 , 20 , 30 , 3 , 0 , 21 , 30 , 3 , 0 , 22 , 30 , 3 , 0 , 23 , 30 , 3 , 0 , 24 , 30 , 3 , 0 , 25 , 30 , 3 , 0 , 26 , 30 , 3 , 0 , 27 , 30 , 3 , 0 , 28 , 30 , 3 , 0 , 29 , 30 };
  
  // Basis-independent function sets tag and enum data in the static arrays:
  Intrepid::setEnumTagData(tagToEnum_,
                           enumToTag_,
                           tags,
                           numDof_,
                           tagSize,
                           posScDim,
                           posScId,
                           posBfId);
}

template<class Scalar> 
void Basis_F2_TET_I4_FEM_FIAT<Scalar>::getValues(FieldContainer<Scalar>&                  outputValues,
          const Teuchos::Array< Point<Scalar> >& inputPoints,
          const EOperator                        operatorType) const {

  // Determine parameters to shape outputValues: number of points = size of input array
  int numPoints = inputPoints.size();
	
  // Incomplete polynomial basis of degree 3 (I3) has 70 basis functions on a tetrahedron that are 2-forms in 3D
  int    numFields = 70;
  EField fieldType = FIELD_FORM_2;
  int    spaceDim  = 3;

  // temporaries
  int countPt  = 0;               // point counter
  Teuchos::Array<int> indexV(3);  // multi-index for values
  Teuchos::Array<int> indexD(2);  // multi-index for divergences

  // Shape the FieldContainer for the output values using these values:
  outputValues.resize(numPoints,
                     numFields,
                     fieldType,
                     operatorType,
                     spaceDim);

#ifdef HAVE_INTREPID_DEBUG
  for (countPt=0; countPt<numPoints; countPt++) {
    // Verify that all points are inside the TET reference cell
    TEST_FOR_EXCEPTION( !MultiCell<Scalar>::inReferenceCell(CELL_TET, inputPoints[countPt]),
                        std::invalid_argument,
                        ">>> ERROR (Basis_F2_TET_I4_FEM_FIAT): Evaluation point is outside the TET reference cell");
  }
#endif
  switch(operatorType) {
    case OPERATOR_VALUE:   
    {
      Teuchos::SerialDenseMatrix<int,Scalar> expansions(35,numPoints);
      Teuchos::SerialDenseMatrix<int,Scalar> result(70,numPoints);
      OrthogonalExpansions<Scalar>::tabulate(CELL_TET,4,inputPoints,expansions);
      result.multiply(Teuchos::NO_TRANS,Teuchos::NO_TRANS,1,*vdm0_,expansions,0);
      for (countPt=0;countPt<numPoints;countPt++) {
        for (int i=0;i<70;i++) {
          outputValues(countPt,i,0) = result(i,countPt);
        }
      }
      result.multiply(Teuchos::NO_TRANS,Teuchos::NO_TRANS,1,*vdm1_,expansions,0);
      for (countPt=0;countPt<numPoints;countPt++) {
        for (int i=0;i<70;i++) {
          outputValues(countPt,i,1) = result(i,countPt);
        }
      }
      result.multiply(Teuchos::NO_TRANS,Teuchos::NO_TRANS,1,*vdm2_,expansions,0);
      for (countPt=0;countPt<numPoints;countPt++) {
        for (int i=0;i<70;i++) {
          outputValues(countPt,i,2) = result(i,countPt);
        }
      }
    }
    break;

    case OPERATOR_D1:
    case OPERATOR_D2:
    case OPERATOR_D3:
    case OPERATOR_D4:
    case OPERATOR_D5:
    case OPERATOR_D6:
    case OPERATOR_D7:
    case OPERATOR_D8:
    case OPERATOR_D9:
    case OPERATOR_D10:
    {
      TEST_FOR_EXCEPTION( true , std::invalid_argument, ">>>ERROR F2_TET_I4_FEM_FIAT: operator not implemented" );
    }
    break; 
    case OPERATOR_CURL:
    {
      TEST_FOR_EXCEPTION( true , std::invalid_argument, ">>>ERROR F2_TET_I4_FEM_FIAT: operator not implemented" );
    }
    break; 
    case OPERATOR_DIV:
    {
      Teuchos::SerialDenseMatrix<int,Scalar> expansions(35,numPoints);
      Teuchos::SerialDenseMatrix<int,Scalar> tmp(35,35);
      Teuchos::SerialDenseMatrix<int,Scalar> div(70,numPoints);
      div.putScalar(0.0);
      OrthogonalExpansions<Scalar>::tabulate(CELL_TET,4,inputPoints,expansions);
      tmp.multiply(Teuchos::TRANS,Teuchos::NO_TRANS,1.0,*dmats0_,expansions,0.0);
      div.multiply(Teuchos::NO_TRANS,Teuchos::NO_TRANS,1.0,*vdm0_,tmp,1.0);
      tmp.multiply(Teuchos::TRANS,Teuchos::NO_TRANS,1.0,*dmats1_,expansions,0.0);
      div.multiply(Teuchos::NO_TRANS,Teuchos::NO_TRANS,1.0,*vdm1_,tmp,1.0);
      tmp.multiply(Teuchos::TRANS,Teuchos::NO_TRANS,1.0,*dmats2_,expansions,0.0);
      div.multiply(Teuchos::NO_TRANS,Teuchos::NO_TRANS,1.0,*vdm2_,tmp,1.0);
      for (countPt=0;countPt<numPoints;countPt++) {
        for (int i=0;i<70;i++) {
          outputValues(countPt,i) = div(i,countPt);
        }
      }
    }
    break;

    default:
      TEST_FOR_EXCEPTION( ( (operatorType != OPERATOR_VALUE) &&
                            (operatorType != OPERATOR_GRAD)  &&
                            (operatorType != OPERATOR_CURL)  &&
                            (operatorType != OPERATOR_DIV)   &&
                            (operatorType != OPERATOR_D1)    &&
                            (operatorType != OPERATOR_D2)    &&
                            (operatorType != OPERATOR_D3)    &&
                            (operatorType != OPERATOR_D4)    &&
                            (operatorType != OPERATOR_D5)    &&
                            (operatorType != OPERATOR_D6)    &&
                            (operatorType != OPERATOR_D7)    &&
                            (operatorType != OPERATOR_D8)    &&
                            (operatorType != OPERATOR_D9)    &&
                            (operatorType != OPERATOR_D10) ),
                          std::invalid_argument,
                          ">>> ERROR (Basis_F2_TET_I4_FEM_DEFAULT): Invalid operator type");
  }


}


template<class Scalar>
void Basis_F2_TET_I4_FEM_FIAT<Scalar>::getValues(FieldContainer<Scalar>&                  outputValues,
                                                    const Teuchos::Array< Point<Scalar> >& inputPoints,
                                                    const Cell<Scalar>&                    cell) const {
  TEST_FOR_EXCEPTION( (true),
                      std::logic_error,
                      ">>> ERROR (Basis_F2_TET_I4_FEM_FIAT): FEM Basis calling an FV/D member function");
}



template<class Scalar>
int Basis_F2_TET_I4_FEM_FIAT<Scalar>::getNumLocalDof() const {
    return numDof_;   
}



template<class Scalar>
int Basis_F2_TET_I4_FEM_FIAT<Scalar>::getLocalDofEnumeration(const LocalDofTag dofTag) {
  if (!isSet_) {
    initialize();
    isSet_ = true;
  }
  return tagToEnum_[dofTag.tag_[0]][dofTag.tag_[1]][dofTag.tag_[2]];
}



template<class Scalar>
LocalDofTag Basis_F2_TET_I4_FEM_FIAT<Scalar>::getLocalDofTag(int id) {
  if (!isSet_) {
    initialize();
    isSet_ = true;
  }
  return enumToTag_[id];
}



template<class Scalar>
const Teuchos::Array<LocalDofTag> & Basis_F2_TET_I4_FEM_FIAT<Scalar>::getAllLocalDofTags() {
  if (!isSet_) {
    initialize();
    isSet_ = true;
  }
  return enumToTag_;
}



template<class Scalar>
ECell Basis_F2_TET_I4_FEM_FIAT<Scalar>::getCellType() const {
  return CELL_TET;
}



template<class Scalar>
EBasis Basis_F2_TET_I4_FEM_FIAT<Scalar>::getBasisType() const {
  return BASIS_FEM_FIAT;
}



template<class Scalar>
ECoordinates Basis_F2_TET_I4_FEM_FIAT<Scalar>::getCoordinateSystem() const {
  return COORDINATES_CARTESIAN;
}



template<class Scalar>
int Basis_F2_TET_I4_FEM_FIAT<Scalar>::getDegree() const {
  return 1;
}


}// namespace Intrepid

