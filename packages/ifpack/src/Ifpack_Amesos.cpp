#include "Ifpack_ConfigDefs.h"
#if defined(HAVE_IFPACK_AMESOS) && defined(HAVE_IFPACK_TEUCHOS)
#include "Ifpack_Preconditioner.h"
#include "Ifpack_Amesos.h"
#include "Ifpack_Condest.h"
#include "Epetra_MultiVector.h"
#include "Epetra_Map.h"
#include "Epetra_Comm.h"
#include "Amesos.h"
#include "Epetra_LinearProblem.h"
#include "Epetra_RowMatrix.h"
#include "Epetra_Time.h"
#include "Teuchos_ParameterList.hpp"

//==============================================================================
Ifpack_Amesos::Ifpack_Amesos(Epetra_RowMatrix* Matrix) :
  Matrix_(Matrix),
  Solver_(0),
  Problem_(0),
  Label_("Amesos_Klu"),
  IsInitialized_(false),
  IsComputed_(false),
  NumInitialize_(0),
  NumCompute_(0),
  NumApplyInverse_(0),
  InitializeTime_(0.0),
  ComputeTime_(0.0),
  ApplyInverseTime_(0.0),
  Time_(0),
  ComputeFlops_(0),
  ApplyInverseFlops_(0),
  Condest_(-1.0)
{
  Problem_ = new Epetra_LinearProblem;
}

//==============================================================================
Ifpack_Amesos::Ifpack_Amesos(const Ifpack_Amesos& rhs) :
  Matrix_(&rhs.Matrix()),
  Solver_(0),
  Problem_(0),
  Label_(rhs.Label()),
  IsInitialized_(false),
  IsComputed_(false),
  NumInitialize_(rhs.NumInitialize()),
  NumCompute_(rhs.NumCompute()),
  NumApplyInverse_(rhs.NumApplyInverse()),
  InitializeTime_(rhs.InitializeTime()),
  ComputeTime_(rhs.ComputeTime()),
  ApplyInverseTime_(rhs.ApplyInverseTime()),
  Time_(0),
  ComputeFlops_(rhs.ComputeFlops()),
  ApplyInverseFlops_(rhs.ApplyInverseFlops()),
  Condest_(rhs.Condest())
{

  Problem_ = new Epetra_LinearProblem;

  // copy the RHS list in *this.List
  Teuchos::ParameterList RHSList(rhs.List());
  List_ = RHSList;

  // I do not have a copy constructor for Amesos,
  // so Initialize() and Compute() of this object 
  // are called if the rhs did so
  if (rhs.IsInitialized()) {
    IsInitialized_ = true;
    Initialize();
  }
  if (rhs.IsComputed()) {
    IsComputed_ = true;
    Compute();
  }

}
//==============================================================================
Ifpack_Amesos::~Ifpack_Amesos()
{
  if (Problem_)
    delete Problem_;

  if (Solver_)
    delete Solver_;

  if (Time_)
    delete Time_;
}

//==============================================================================
int Ifpack_Amesos::SetParameters(Teuchos::ParameterList& List)
{

  List_ = List;
  Label_ = List.get("amesos: solver type", Label_);
  return(0);
}

//==============================================================================
int Ifpack_Amesos::Initialize()
{

  IsInitialized_ = false;
  IsComputed_ = false;

  if (Matrix_ == 0)
    IFPACK_CHK_ERR(-1);

#if 0
  // better to avoid strange games with maps, this class should be
  // used for Ifpack_LocalFilter'd matrices only
  if (Comm().NumProc() != 1) {
    cout << "Class Ifpack_Amesos must be used for serial runs;" << endl;
    cout << "for parallel runs you should declare objects as:" << endl; 
    cout << "Ifpack_AdditiveSchwarz<Ifpack_Amesos> APrec(Matrix)" << endl;
    exit(EXIT_FAILURE);
  }
#endif

  // only square matrices
  if (Matrix_->NumGlobalRows() != Matrix_->NumGlobalCols())
    IFPACK_CHK_ERR(-1);

  // at least one nonzero
  if (Matrix_->NumMyNonzeros() == 0) 
    IFPACK_CHK_ERR(-1);

  Problem_->SetOperator(const_cast<Epetra_RowMatrix*>(Matrix_));

  if (Time_ == 0)
    Time_ = new Epetra_Time(Comm());

  // reallocate the solver. 
  if (Solver_)
    delete Solver_;

  Amesos Factory;
  Solver_ = Factory.Create((char*)Label_.c_str(),*Problem_);
  
  if (Solver_ == 0) {
    // try to create KLU, it is generally enabled
    Solver_ = Factory.Create("Amesos_Klu",*Problem_);
  }
  if (Solver_ == 0)
    IFPACK_CHK_ERR(-1);

  Solver_->SetParameters(List_);
  IFPACK_CHK_ERR(Solver_->SymbolicFactorization());

  IsInitialized_ = true;
  ++NumInitialize_;
  InitializeTime_ += Time_->ElapsedTime();
  return(0);
}

//==============================================================================
int Ifpack_Amesos::Compute()
{

  if (!IsInitialized())
    IFPACK_CHK_ERR(Initialize());

  IsComputed_ = false;
  Time_->ResetStartTime();

  if (Matrix_ == 0)
    IFPACK_CHK_ERR(-1);

  IFPACK_CHK_ERR(Solver_->NumericFactorization());

  IsComputed_ = true;
  ++NumCompute_;
  ComputeTime_ += Time_->ElapsedTime();
  return(0);
}

//==============================================================================
int Ifpack_Amesos::SetUseTranspose(bool UseTranspose)
{
  IFPACK_CHK_ERR(-99); // not implemented
}

//==============================================================================
int Ifpack_Amesos::
Apply(const Epetra_MultiVector& X, Epetra_MultiVector& Y) const
{
  // check for maps ???
  IFPACK_CHK_ERR(Matrix_->Apply(X,Y));
  return(0);
}

//==============================================================================
int Ifpack_Amesos::
ApplyInverse(const Epetra_MultiVector& X, Epetra_MultiVector& Y) const
{

  if (IsComputed() == false)
    IFPACK_CHK_ERR(-1);

  if (X.NumVectors() != Y.NumVectors())
    IFPACK_CHK_ERR(-1); // wrong input
  
  Time_->ResetStartTime();

  // AztecOO gives X and Y pointing to the same memory location,
  // need to create an auxiliary vector, Xcopy
  const Epetra_MultiVector* Xcopy;
  if (X.Pointers()[0] == Y.Pointers()[0])
    Xcopy = new Epetra_MultiVector(X);
  else
    Xcopy = &X;
    
  Problem_->SetLHS(&Y);
  Problem_->SetRHS((Epetra_MultiVector*)Xcopy);
  IFPACK_CHK_ERR(Solver_->Solve());

  if (Xcopy != &X)
    delete Xcopy;

  ++NumApplyInverse_;
  ApplyInverseTime_ += Time_->ElapsedTime();

  return(0);
}

//==============================================================================
double Ifpack_Amesos::NormInf() const
{
  return(-1.0);
}

//==============================================================================
const char* Ifpack_Amesos::Label() const
{
  return((char*)Label_.c_str());
}

//==============================================================================
bool Ifpack_Amesos::UseTranspose() const
{
  return(false);
}

//==============================================================================
bool Ifpack_Amesos::HasNormInf() const
{
  return(false);
}

//==============================================================================
const Epetra_Comm & Ifpack_Amesos::Comm() const
{
  return(Matrix_->Comm());
}

//==============================================================================
const Epetra_Map & Ifpack_Amesos::OperatorDomainMap() const
{
  return(Matrix_->OperatorDomainMap());
}

//==============================================================================
const Epetra_Map & Ifpack_Amesos::OperatorRangeMap() const
{
  return(Matrix_->OperatorRangeMap());
}

//==============================================================================
double Ifpack_Amesos::Condest(const Ifpack_CondestType CT,
                              const int MaxIters, const double Tol,
			      Epetra_RowMatrix* Matrix)
{

  if (!IsComputed()) // cannot compute right now
    return(-1.0);

  if (Condest_ == -1.0)
    Condest_ = Ifpack_Condest(*this, CT, MaxIters, Tol, Matrix);

  return(Condest_);
}

//==============================================================================
std::ostream& Ifpack_Amesos::Print(std::ostream& os) const
{
  if (!Comm().MyPID()) {
    os << endl;
    os << "================================================================================" << endl;
    os << "Ifpack_Amesos: " << Label () << endl << endl;
    os << "Condition number estimate = " << Condest() << endl;
    os << "Global number of rows            = " << Matrix_->NumGlobalRows() << endl;
    os << endl;
    os << "Phase           # calls   Total Time (s)       Total MFlops     MFlops/s" << endl;
    os << "-----           -------   --------------       ------------     --------" << endl;
    os << "Initialize()    "   << std::setw(5) << NumInitialize_ 
       << "  " << std::setw(15) << InitializeTime_ 
       << "              0.0              0.0" << endl;
    os << "Compute()       "   << std::setw(5) << NumCompute_ 
       << "  " << std::setw(15) << ComputeTime_
       << "  " << std::setw(15) << 1.0e-6 * ComputeFlops_;
    if (ComputeTime_ != 0.0) 
      os << "  " << std::setw(15) << 1.0e-6 * ComputeFlops_ / ComputeTime_ << endl;
    else
      os << "  " << std::setw(15) << 0.0 << endl;
    os << "ApplyInverse()  "   << std::setw(5) << NumApplyInverse_ 
       << "  " << std::setw(15) << ApplyInverseTime_
       << "  " << std::setw(15) << 1.0e-6 * ApplyInverseFlops_;
    if (ApplyInverseTime_ != 0.0) 
      os << "  " << std::setw(15) << 1.0e-6 * ApplyInverseFlops_ / ApplyInverseTime_ << endl;
    else
      os << "  " << std::setw(15) << 0.0 << endl;
    os << "================================================================================" << endl;
    os << endl;
  }

  return(os);
}
#endif // HAVE_IFPACK_AMESOS && HAVE_IFPACK_TEUCHOS
