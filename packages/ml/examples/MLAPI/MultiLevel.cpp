
//@HEADER
// ************************************************************************
// 
//               ML: A Multilevel Preconditioner Package
//                 Copyright (2002) Sandia Corporation
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
// ************************************************************************
//@HEADER

#ifndef HAVE_CONFIG_H
#define HAVE_CONFIG_H
#endif
#include "ml_config.h"

#if defined(HAVE_ML_EPETRA) && defined(HAVE_ML_TEUCHOS) && defined(HAVE_ML_TRIUTILS)

#ifdef HAVE_MPI
#include "mpi.h"
#include "Epetra_MpiComm.h"
#else
#include "Epetra_SerialComm.h"
#endif
#include "ml_include.h"
#include "MLAPI.h"

using namespace Teuchos;
using namespace MLAPI;

// ============== //
// example driver //
// ============== //

int main(int argc, char *argv[])
{
  
#ifdef EPETRA_MPI
  MPI_Init(&argc,&argv);
  Epetra_MpiComm Comm(MPI_COMM_WORLD);
#else
  Epetra_SerialComm Comm;
#endif

  // Initialize the workspace and set the output level
  Init();

  try {

    int NumGlobalElements = 10000;

    SetPrintLevel(10);
    // define the space for fine level vectors and operators.
    Space FineSpace(NumGlobalElements);

    // define the linear system matrix, solution and RHS
    Operator FineMatrix = Gallery("laplace_2d", FineSpace);
    MultiVector LHS(FineSpace);
    MultiVector RHS(FineSpace);

    LHS = 0.0;
    RHS.Random();

    // set parameters for aggregation and smoothers
    // NOTE: only a limited subset of the parameters accepted by
    // class ML_Epetra::MultiLevelPreconditioner is supported
    // by MLAPI::MultiLevelSA
    
    Teuchos::ParameterList MLList;
    MLList.set("max levels",3);
    MLList.set("increasing or decreasing","increasing");
    MLList.set("aggregation: type", "Uncoupled");
    MLList.set("aggregation: damping factor", 0.0);
    MLList.set("smoother: type","symmetric Gauss-Seidel");
    MLList.set("smoother: sweeps",1);
    MLList.set("smoother: damping factor",1.0);
    MLList.set("coarse: max size",32);
    MLList.set("smoother: pre or post", "both");
    MLList.set("coarse: type","Amesos-KLU");

    // create the multilevel hierarchy using aggregation
    AggregationDataBase  ADB(MLList);
    SmootherDataBase     SDB(MLList);
    CoarseSolverDataBase CDB(MLList);

    MultiLevelSA Prec(FineMatrix, ADB, SDB, CDB);

    // solve with GMRES (through AztecOO)
    KrylovDataBase KDB;
    Krylov(FineMatrix, LHS, RHS, Prec, KDB);

  }
  catch (const char e[]) {
    cerr << "Caught exception: " << e << endl;
  }
  catch (...) {
    cerr << "Caught exception..." << endl;
  }

#ifdef HAVE_MPI
  MPI_Finalize();
#endif

    return(0);
}

#else

#include <stdlib.h>
#include <stdio.h>

  int main(int argc, char *argv[])
  {
    puts("Please configure ML with --enable-epetra --enable-teuchos --enable-triutils");

    return 0;
  }

#endif /* #if defined(ML_WITH_EPETRA) && defined(HAVE_ML_TEUCHOS) && defined(HAVE_ML_TRIUTILS) */
