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

#include "MoochoPack_MoochoThyraSolver.hpp"
#include "NLPInterfacePack_NLPDirectThyraModelEvaluator.hpp"
#include "NLPInterfacePack_NLPFirstOrderThyraModelEvaluator.hpp"
#include "Thyra_DefaultFiniteDifferenceModelEvaluator.hpp"
#include "Thyra_DefaultStateEliminationModelEvaluator.hpp"
#include "Thyra_DefaultEvaluationLoggerModelEvaluator.hpp"
#include "Thyra_DefaultInverseModelEvaluator.hpp"
#include "Thyra_DefaultSpmdMultiVectorFileIO.hpp"
#include "Thyra_DampenedNewtonNonlinearSolver.hpp"
#include "Thyra_ModelEvaluatorHelpers.hpp"
#include "Thyra_VectorStdOps.hpp"
#include "Teuchos_StandardParameterEntryValidators.hpp"
#include "Teuchos_XMLParameterListHelpers.hpp"

namespace {

//
// ParameterList parameters and sublists
//

const std::string SolveMode_name = "Solve Mode";
const Teuchos::RefCountPtr<
  Teuchos::StringToIntegralParameterEntryValidator<
    MoochoPack::MoochoThyraSolver::ESolveMode
  >
>
solveModeValidator = Teuchos::rcp(
  new Teuchos::StringToIntegralParameterEntryValidator<MoochoPack::MoochoThyraSolver::ESolveMode>(
    Teuchos::tuple<std::string>(
      "Forward Solve"
      ,"Optimize"
      )
    ,Teuchos::tuple<std::string>(
      "Only solve state equaitons f(x,p)=0 for states x\n"
      "given fixed parameters values p."
      ,"Solve the simulation constrained optimization problem\n"
      "  min  g(x,p)\n"
      "  s.t. f(x,p)=0\n"
      "for the state varaibles x and parameters p."
      )
    ,Teuchos::tuple<MoochoPack::MoochoThyraSolver::ESolveMode>(
      MoochoPack::MoochoThyraSolver::SOLVE_MODE_FORWARD
      ,MoochoPack::MoochoThyraSolver::SOLVE_MODE_OPTIMIZE
      )
    ,""
    )
  );
const std::string SolveMode_default = "Optimize";

const std::string NLPType_name = "NLP Type";
const Teuchos::RefCountPtr<
  Teuchos::StringToIntegralParameterEntryValidator<
    MoochoPack::MoochoThyraSolver::ENLPType
    >
  >
nlpTypeValidator = Teuchos::rcp(
  new Teuchos::StringToIntegralParameterEntryValidator<MoochoPack::MoochoThyraSolver::ENLPType>(
    Teuchos::tuple<std::string>(
      "First Order"
      ,"Direct"
      )
    ,Teuchos::tuple<std::string>(
      "Support the NLPInterfacePack::NLPFirstOrder interface which assumes\n"
      "that full adjoints for the objective and constraint derivatives are\n"
      "available."
      ,"Support the NLPInterfacePack::NLPDirect interface which only assumes\n"
      "that forward or direct sensitivities and state solves are supported."
      )
    ,Teuchos::tuple<MoochoPack::MoochoThyraSolver::ENLPType>(
      MoochoPack::MoochoThyraSolver::NLP_TYPE_FIRST_ORDER
      ,MoochoPack::MoochoThyraSolver::NLP_TYPE_DIRECT
      )
    ,""
    )
  );
const std::string NLPType_default = "First Order";

const std::string NonlinearlyEliminateStates_name = "Nonlinearly Eliminate States";
const bool NonlinearlyEliminateStates_default = false;

const std::string UseFiniteDifferencesForObjective_name = "Use Finite Differences For Objective";
const bool UseFiniteDifferencesForObjective_default = false;

const std::string ObjectiveFiniteDifferenceSettings_name = "Objective Finite Difference Settings";

const std::string UseFiniteDifferencesForConstraints_name = "Use Finite Differences For Constraints";
const bool UseFiniteDifferencesForConstraints_default = false;

const std::string ConstraintsFiniteDifferenceSettings_name = "Constraints Finite Difference Settings";

const std::string FwdNewtonTol_name = "Forward Newton Tolerance";
const double FwdNewtonTol_default = -1.0;

const std::string FwdNewtonMaxIters_name = "Forward Newton Max Iters";
const int FwdNewtonMaxIters_default = 20;

const std::string UseBuiltInverseObjectiveFunction_name = "Use Built-in Inverse Objective Function";
const bool UseBuiltInverseObjectiveFunction_default = false;

const std::string InverseObjectiveFunctionSettings_name = "Inverse Objective Function Settings";

const std::string OutputFileTag_name = "Output File Tag";
const std::string OutputFileTag_default = "";

const std::string ShowModelEvaluatorTrace_name = "Show Model Evaluator Trace";
const bool ShowModelEvaluatorTrace_default = "false";

const std::string StateGuess_name = "State Guess";

const std::string ParamGuess_name = "Parameter Guess";

const std::string ParamLowerBounds_name = "Parameter Lower Bounds";

const std::string ParamUpperBounds_name = "Parameter Upper Bounds";

const std::string StateSoluFileBaseName_name = "State Solution File Base Name";
const std::string StateSoluFileBaseName_default = "";

const std::string ParamSoluFileBaseName_name = "Parameters Solution File Base Name";
const std::string ParamSoluFileBaseName_default = "";

} // namespace

namespace MoochoPack {

// Constructors/initialization

MoochoThyraSolver::MoochoThyraSolver(
  const std::string    &paramsXmlFileName
  ,const std::string   &extraParamsXmlString
  ,const std::string   &paramsUsedXmlOutFileName
  ,const std::string   &paramsXmlFileNameOption
  ,const std::string   &extraParamsXmlStringOption
  ,const std::string   &paramsUsedXmlOutFileNameOption
  )
  :paramsXmlFileName_(paramsXmlFileName)
  ,extraParamsXmlString_(extraParamsXmlString)
  ,paramsUsedXmlOutFileName_(paramsUsedXmlOutFileName)
  ,paramsXmlFileNameOption_(paramsXmlFileNameOption)
  ,extraParamsXmlStringOption_(extraParamsXmlStringOption)
  ,paramsUsedXmlOutFileNameOption_(paramsUsedXmlOutFileNameOption)
  ,stateVectorIO_(Teuchos::rcp(new Thyra::DefaultSpmdMultiVectorFileIO<value_type>))
  ,parameterVectorIO_(Teuchos::rcp(new Thyra::DefaultSpmdMultiVectorFileIO<value_type>))
  ,solveMode_(SOLVE_MODE_OPTIMIZE)
  ,nlpType_(NLP_TYPE_FIRST_ORDER)
  ,nonlinearlyElimiateStates_(false)
  ,use_finite_diff_for_obj_(false)
  ,use_finite_diff_for_con_(false)
  ,fwd_newton_tol_(-1.0)
  ,fwd_newton_max_iters_(20)
  ,useInvObjFunc_(false)
  ,outputFileTag_("")
  ,showModelEvaluatorTrace_(false)
  ,stateSoluFileBase_("")
  ,paramSoluFileBase_("")
{}

MoochoThyraSolver::~MoochoThyraSolver()
{}

void MoochoThyraSolver::setupCLP(
  Teuchos::CommandLineProcessor *clp
  )
{
  TEST_FOR_EXCEPT(0==clp);
  solver_.setup_commandline_processor(clp);
  clp->setOption(
    paramsXmlFileNameOption().c_str(),&paramsXmlFileName_
    ,"Name of an XML file containing parameters for linear solver options to be appended first."
    );
  clp->setOption(
    extraParamsXmlStringOption().c_str(),&extraParamsXmlString_
    ,"An XML string containing linear solver parameters to be appended second."
    );
  clp->setOption(
    paramsUsedXmlOutFileNameOption().c_str(),&paramsUsedXmlOutFileName_
    ,"Name of an XML file that can be written with the parameter list after it has been used on completion of this program."
    );
}

void MoochoThyraSolver::readParameters( std::ostream *out_arg )
{
  Teuchos::RefCountPtr<Teuchos::FancyOStream>
    out = Teuchos::getFancyOStream(Teuchos::rcp(out_arg,false));
  Teuchos::OSTab tab(out);
  if(out.get()) *out << "\nMoochoThyraSolver::readParameters(...):\n";
  Teuchos::OSTab tab2(out);
  Teuchos::RefCountPtr<Teuchos::ParameterList>
    paramList = this->getParameterList();
  if(!paramList.get()) {
    if(out.get()) *out << "\nCreating a new Teuchos::ParameterList ...\n";
    paramList = Teuchos::rcp(new Teuchos::ParameterList("MoochoThyraSolver"));
  }
  if(paramsXmlFileName().length()) {
    if(out.get()) *out << "\nReading parameters from XML file \""<<paramsXmlFileName()<<"\" ...\n";
    Teuchos::updateParametersFromXmlFile(paramsXmlFileName(),&*paramList);
  }
  if(extraParamsXmlString().length()) {
    if(out.get())
      *out << "\nAppending extra parameters from the XML string \""<<extraParamsXmlString()<<"\" ...\n";
    Teuchos::updateParametersFromXmlString(extraParamsXmlString(),&*paramList);
  }
  if( paramsXmlFileName().length() || extraParamsXmlString().length() ) {
    typedef Teuchos::ParameterList::PrintOptions PLPrintOptions;
    if(out.get()) {
      *out  << "\nUpdated parameter list:\n";
      paramList->print(
        *out,PLPrintOptions().indent(2).showTypes(true)
        );
    }
    this->setParameterList(paramList);
  }
}

// Overridden from ParameterListAcceptor

void MoochoThyraSolver::setParameterList(
  Teuchos::RefCountPtr<Teuchos::ParameterList> const& paramList
  )
{
  TEST_FOR_EXCEPT(!paramList.get());
  paramList->validateParameters(*getValidParameters(),0); // Just validate my level!
  paramList_ = paramList;
  solveMode_ = solveModeValidator->getIntegralValue(
    *paramList_,SolveMode_name,SolveMode_default);
  nlpType_ = nlpTypeValidator->getIntegralValue(
    *paramList_,NLPType_name,NLPType_default);
  nonlinearlyElimiateStates_ = paramList_->get(
    NonlinearlyEliminateStates_name,NonlinearlyEliminateStates_default);
  use_finite_diff_for_obj_ = paramList_->get(
    UseFiniteDifferencesForObjective_name,UseFiniteDifferencesForObjective_default);
  use_finite_diff_for_con_ = paramList_->get(
    UseFiniteDifferencesForConstraints_name,UseFiniteDifferencesForConstraints_default);
  fwd_newton_tol_ = paramList_->get(
    FwdNewtonTol_name,FwdNewtonTol_default);
  fwd_newton_max_iters_ = paramList_->get(
    FwdNewtonMaxIters_name,FwdNewtonMaxIters_default);
  useInvObjFunc_ = paramList_->get(
    UseBuiltInverseObjectiveFunction_name,UseBuiltInverseObjectiveFunction_default);
  outputFileTag_ = paramList->get(
    OutputFileTag_name,OutputFileTag_default);
  solver_.set_output_file_tag(outputFileTag_);
  showModelEvaluatorTrace_ = paramList->get(
    ShowModelEvaluatorTrace_name,ShowModelEvaluatorTrace_default);
  x_reader_.setParameterList(
    sublist(paramList_,StateGuess_name)
    );
  p_reader_.setParameterList(
    sublist(paramList_,ParamGuess_name)
    );
  p_l_reader_.setParameterList(
    sublist(paramList_,ParamLowerBounds_name)
    );
  p_u_reader_.setParameterList(
    sublist(paramList_,ParamUpperBounds_name)
    );
  stateSoluFileBase_ = paramList_->get(
    StateSoluFileBaseName_name,StateSoluFileBaseName_default);
  paramSoluFileBase_ = paramList_->get(
    ParamSoluFileBaseName_name,ParamSoluFileBaseName_default);
#ifdef TEUCHOS_DEBUG
  paramList->validateParameters(*getValidParameters(),0); // Just validate my level!
#endif
}

Teuchos::RefCountPtr<Teuchos::ParameterList>
MoochoThyraSolver::getParameterList()
{
  return paramList_;
}

Teuchos::RefCountPtr<Teuchos::ParameterList>
MoochoThyraSolver::unsetParameterList()
{
  Teuchos::RefCountPtr<Teuchos::ParameterList> _paramList = paramList_;
  paramList_ = Teuchos::null;
  return _paramList;
}

Teuchos::RefCountPtr<const Teuchos::ParameterList>
MoochoThyraSolver::getParameterList() const
{
  return paramList_;
}

Teuchos::RefCountPtr<const Teuchos::ParameterList>
MoochoThyraSolver::getValidParameters() const
{
  static Teuchos::RefCountPtr<Teuchos::ParameterList> pl;
  if(pl.get()==NULL) {
    pl = Teuchos::rcp(new Teuchos::ParameterList());
    pl->set(
      SolveMode_name,SolveMode_default
      ,"The type of solve to perform."
      ,solveModeValidator
      );
    pl->set(
      NLPType_name,NLPType_default
      ,"The type of MOOCHO NLP subclass to use."
      ,nlpTypeValidator
      );
    pl->set(
      NonlinearlyEliminateStates_name,NonlinearlyEliminateStates_default
      ,"If true, then the model's state equations and state variables\n"
      "are nonlinearlly eliminated using a forward solver."
      );
    pl->set(
      UseFiniteDifferencesForObjective_name,UseFiniteDifferencesForObjective_default
      ,"Use finite differences for missing objective function derivatives (Direct NLP only).\n"
      "See the options in the sublist \"" + ObjectiveFiniteDifferenceSettings_name + "\"."
      );
    {
      Thyra::DirectionalFiniteDiffCalculator<Scalar> dfdcalc;
      {
        Teuchos::ParameterList
          &fdSublist = pl->sublist(ObjectiveFiniteDifferenceSettings_name);
        fdSublist.setParameters(*dfdcalc.getValidParameters());
      }
      pl->set(
        UseFiniteDifferencesForConstraints_name,UseFiniteDifferencesForConstraints_default
        ,"Use  finite differences for missing constraint derivatives (Direct NLP only).\n"
        "See the   options in the sublist \"" + ConstraintsFiniteDifferenceSettings_name + "\"."
        );
      {
        Teuchos::ParameterList
          &fdSublist = pl->sublist(ConstraintsFiniteDifferenceSettings_name);
        fdSublist.setParameters(*dfdcalc.getValidParameters());
      }
    }
    pl->set(
      FwdNewtonTol_name,FwdNewtonTol_default
      ,"Tolarance used for the forward state solver in eliminating\n"
      "the state equations/variables."
      );
    pl->set(
      FwdNewtonMaxIters_name,FwdNewtonMaxIters_default
      ,"Maximum number of iterations allows for the forward state\n"
      "solver in eliminating the state equations/variables."
      );
    pl->set(
      UseBuiltInverseObjectiveFunction_name,UseBuiltInverseObjectiveFunction_default
      ,"Use a built-in form of a simple inverse objection function instead\n"
      "of a a response function contained in the underlying model evaluator\n"
      "object itself.  The settings are contained in the sublist\n"
      "\""+InverseObjectiveFunctionSettings_name+"\".\n"
      "Note that this feature allows the client to form a useful type\n"
      "of optimization problem just with a model that supports only the\n"
      "parameterized state function f(x,p)=0."
      );
    {
      Teuchos::RefCountPtr<Thyra::DefaultInverseModelEvaluator<Scalar> >
        inverseModel = rcp(new Thyra::DefaultInverseModelEvaluator<Scalar>());
      pl->sublist(
        InverseObjectiveFunctionSettings_name,false
        ,"Settings for the built-in inverse objective function.\n"
        "See the outer parameter \""+UseBuiltInverseObjectiveFunction_name+"\"."
        ).setParameters(*inverseModel->getValidParameters());
    }
    pl->set(OutputFileTag_name,OutputFileTag_default,
      "A tag that is attached to every output file that is created by the\n"
      "solver.  If empty \"\", then no tag is used." );
    pl->set(ShowModelEvaluatorTrace_name,ShowModelEvaluatorTrace_default
      ,"Determine if a trace of the objective function will be shown or not\n"
      "when the NLP is evaluated."
      );
    if(this->get_stateVectorIO().get())
      x_reader_.set_fileIO(this->get_stateVectorIO());
    pl->sublist(StateGuess_name).setParameters(*x_reader_.getValidParameters());
    if(this->get_parameterVectorIO().get()) {
      p_reader_.set_fileIO(this->get_parameterVectorIO());
      p_l_reader_.set_fileIO(this->get_parameterVectorIO());
      p_u_reader_.set_fileIO(this->get_parameterVectorIO());
      pl->sublist(ParamGuess_name).setParameters(*p_reader_.getValidParameters());
      pl->sublist(ParamLowerBounds_name).setParameters(*p_l_reader_.getValidParameters());
      pl->sublist(ParamUpperBounds_name).setParameters(*p_u_reader_.getValidParameters());
    }
    pl->set(
      StateSoluFileBaseName_name,StateSoluFileBaseName_default
      ,"If specified, a file with this basename will be written to with\n"
      "the final value of the state variables.  A different file for each\n"
      "process will be created.  Note that these files can be used for the\n"
      "initial guess for the state variables."
      );
    pl->set(
      ParamSoluFileBaseName_name,ParamSoluFileBaseName_default
      ,"If specified, a file with this basename will be written to with\n"
      "the final value of the parameters.  A different file for each\n"
      "process will be created.  Note that these files can be used for the\n"
      "initial guess for the parameters."
      );
  }
  return pl;
}

// Misc Access/Setup

void MoochoThyraSolver::setSolveMode( const ESolveMode solveMode )
{
  solveMode_ = solveMode_;
}

MoochoThyraSolver::ESolveMode
MoochoThyraSolver::getSolveMode() const
{
  return solveMode_;
}

MoochoSolver& MoochoThyraSolver::getSolver()
{
  return solver_;
}

const MoochoSolver& MoochoThyraSolver::getSolver() const
{
  return solver_;
}

// Model specification, setup, solve, and solution extraction.

void MoochoThyraSolver::setModel(
  const Teuchos::RefCountPtr<Thyra::ModelEvaluator<value_type> > &model
  ,const int                                                     p_idx
  ,const int                                                     g_idx
  )
{

  using Teuchos::rcp;
  using Teuchos::RefCountPtr;
  using NLPInterfacePack::NLP;
  using NLPInterfacePack::NLPDirectThyraModelEvaluator;
  using NLPInterfacePack::NLPFirstOrderThyraModelEvaluator;

  origModel_ = model;
  p_idx_ = p_idx;
  g_idx_ = g_idx;

  const int procRank = Teuchos::GlobalMPISession::getRank();
  //const int numProcs = Teuchos::GlobalMPISession::getNProc();

  //
  // Wrap the orginal model in different decorators
  //

  outerModel_ = origModel_;

  if(useInvObjFunc_) {
    Teuchos::RefCountPtr<Thyra::DefaultInverseModelEvaluator<Scalar> >
      inverseModel
      = rcp(new Thyra::DefaultInverseModelEvaluator<Scalar>(outerModel_));
    inverseModel->setVerbLevel(Teuchos::VERB_LOW);
    inverseModel->set_observationTargetIO(get_stateVectorIO());
    inverseModel->set_parameterBaseIO(get_parameterVectorIO());
    inverseModel->setParameterList(
      Teuchos::sublist(paramList_,InverseObjectiveFunctionSettings_name) );
    outerModel_ = inverseModel; 
    g_idx_ = inverseModel->Ng()-1;
  }
  
  Teuchos::RefCountPtr<std::ostream>
    modelEvalLogOut = Teuchos::fancyOStream(
      solver_.generate_output_file("ModelEvaluationLog")
      );
  Teuchos::RefCountPtr<Thyra::DefaultEvaluationLoggerModelEvaluator<Scalar> >
    loggerThyraModel
    = rcp(
      new Thyra::DefaultEvaluationLoggerModelEvaluator<Scalar>(
        outerModel_,modelEvalLogOut
        )
      );
  outerModel_ = loggerThyraModel; 
  
  nominalModel_
    = rcp(
      new Thyra::DefaultNominalBoundsOverrideModelEvaluator<Scalar>(outerModel_,Teuchos::null)
      );
  outerModel_ = nominalModel_; 

  finalPointModel_
    = rcp(
      new Thyra::DefaultFinalPointCaptureModelEvaluator<value_type>(outerModel_)
      );
  outerModel_ = finalPointModel_;

  //
  // Create the NLP
  //
    
  Teuchos::RefCountPtr<NLP> nlp;

  switch(solveMode_) {
    case SOLVE_MODE_FORWARD: {
      RefCountPtr<NLPFirstOrderThyraModelEvaluator>
        nlpFirstOrder = rcp(
          new NLPFirstOrderThyraModelEvaluator(outerModel_,-1,-1)
          );
      nlpFirstOrder->showModelEvaluatorTrace(showModelEvaluatorTrace_);
      nlp = nlpFirstOrder;
      break;
    }
    case SOLVE_MODE_OPTIMIZE: {
      // Setup finite difference object
      RefCountPtr<Thyra::DirectionalFiniteDiffCalculator<Scalar> > objDirecFiniteDiffCalculator;
      if(use_finite_diff_for_obj_) {
        objDirecFiniteDiffCalculator = rcp(new Thyra::DirectionalFiniteDiffCalculator<Scalar>());
        if(paramList_.get())
          objDirecFiniteDiffCalculator->setParameterList(
            Teuchos::sublist(paramList_,ObjectiveFiniteDifferenceSettings_name)
            );
      }
      RefCountPtr<Thyra::DirectionalFiniteDiffCalculator<Scalar> > conDirecFiniteDiffCalculator;
      if(use_finite_diff_for_con_) {
        conDirecFiniteDiffCalculator = rcp(new Thyra::DirectionalFiniteDiffCalculator<Scalar>());
        if(paramList_.get())
          conDirecFiniteDiffCalculator->setParameterList(
            Teuchos::sublist(paramList_,ConstraintsFiniteDifferenceSettings_name)
            );
      }
      if( nonlinearlyElimiateStates_ ) {
        // Create a Thyra::NonlinearSolverBase object to solve and eliminate the
        // state variables and the state equations
        Teuchos::RefCountPtr<Thyra::DampenedNewtonNonlinearSolver<Scalar> >
          stateSolver = rcp(new Thyra::DampenedNewtonNonlinearSolver<Scalar>()); // ToDo: Replace with MOOCHO!
        stateSolver->defaultTol(fwd_newton_tol_);
        stateSolver->defaultMaxNewtonIterations(fwd_newton_max_iters_);
        // Create the reduced Thyra::ModelEvaluator object for p -> g_hat(p)
        Teuchos::RefCountPtr<Thyra::DefaultStateEliminationModelEvaluator<Scalar> >
          reducedThyraModel = rcp(new Thyra::DefaultStateEliminationModelEvaluator<Scalar>(outerModel_,stateSolver));
        Teuchos::RefCountPtr<Thyra::ModelEvaluator<Scalar> >
          finalReducedThyraModel;
        if(use_finite_diff_for_obj_) {
          // Create the finite-difference wrapped Thyra::ModelEvaluator object
          Teuchos::RefCountPtr<Thyra::DefaultFiniteDifferenceModelEvaluator<Scalar> >
            fdReducedThyraModel = rcp(
              new Thyra::DefaultFiniteDifferenceModelEvaluator<Scalar>(
                reducedThyraModel,objDirecFiniteDiffCalculator
                )
              );
          finalReducedThyraModel = fdReducedThyraModel;
        }
        else {
          finalReducedThyraModel = reducedThyraModel;
        }
        // Wrap the reduced NAND Thyra::ModelEvaluator object in an NLP object
        RefCountPtr<NLPFirstOrderThyraModelEvaluator>
          nlpFirstOrder = rcp(
            new NLPFirstOrderThyraModelEvaluator(finalReducedThyraModel,p_idx_,g_idx_)
            );
        nlpFirstOrder->showModelEvaluatorTrace(showModelEvaluatorTrace_);
        nlp = nlpFirstOrder;
      }
      else {
        switch(nlpType_) {
          case NLP_TYPE_DIRECT: {
            Teuchos::RefCountPtr<NLPDirectThyraModelEvaluator>
              nlpDirect = rcp(
                new NLPDirectThyraModelEvaluator(
                  outerModel_,p_idx_,g_idx_
                  ,objDirecFiniteDiffCalculator
                  ,conDirecFiniteDiffCalculator
                  )
                );
            nlpDirect->showModelEvaluatorTrace(showModelEvaluatorTrace_);
            nlp = nlpDirect;
            break;
          }
          case NLP_TYPE_FIRST_ORDER: {
            RefCountPtr<NLPFirstOrderThyraModelEvaluator>
              nlpFirstOrder = rcp(
                new NLPFirstOrderThyraModelEvaluator(outerModel_,p_idx_,g_idx_)
                );
            nlpFirstOrder->showModelEvaluatorTrace(showModelEvaluatorTrace_);
            nlp = nlpFirstOrder;
            break;
          }
          default:
            TEST_FOR_EXCEPT(true);
        }
      }
      break;
    }
    default:
      TEST_FOR_EXCEPT(true);
  }
    
  // Set the NLP
  solver_.set_nlp(nlp);

}

const Teuchos::RefCountPtr<Thyra::ModelEvaluator<value_type> >
MoochoThyraSolver::getOuterModel() const
{
  return outerModel_;
}

void MoochoThyraSolver::readInitialGuess(
  std::ostream *out_arg
  )
{
  using Teuchos::OSTab;
  using Teuchos::RefCountPtr;
  using Thyra::clone;
  typedef Thyra::ModelEvaluatorBase MEB;

  RefCountPtr<Teuchos::FancyOStream>
    out = Teuchos::getFancyOStream(Teuchos::rcp(out_arg,false));
  RefCountPtr<MEB::InArgs<value_type> >
    initialGuess = clone(origModel_->getNominalValues()),
    lowerBounds = clone(origModel_->getLowerBounds()),
    upperBounds = clone(origModel_->getUpperBounds());

  RefCountPtr<const Thyra::VectorSpaceBase<value_type> >
    x_space = origModel_->get_x_space();
  if( 0 != x_space.get() ) {
    x_reader_.set_vecSpc(origModel_->get_x_space());
    if(this->get_stateVectorIO().get())
      x_reader_.set_fileIO(this->get_stateVectorIO());
    Teuchos::VerboseObjectTempState<Thyra::ParameterDrivenMultiVectorInput<value_type> >
      vots_x_reader(rcp(&x_reader_,false),out,Teuchos::VERB_LOW);
    initialGuess->set_x(x_reader_.readVector("initial guess for the state \'x\'"));
  }
  if( origModel_->Np() > 0 ) {
    p_reader_.set_vecSpc(origModel_->get_p_space(p_idx_));
    p_l_reader_.set_vecSpc(p_reader_.get_vecSpc());
    p_u_reader_.set_vecSpc(p_reader_.get_vecSpc());
    if(this->get_parameterVectorIO().get()) {
      p_reader_.set_fileIO(this->get_parameterVectorIO());
      p_l_reader_.set_fileIO(p_reader_.get_fileIO());
      p_u_reader_.set_fileIO(p_reader_.get_fileIO());
    }
    Teuchos::VerboseObjectTempState<Thyra::ParameterDrivenMultiVectorInput<value_type> >
      vots_p_reader(rcp(&p_reader_,false),out,Teuchos::VERB_LOW);
    initialGuess->set_p(p_idx_,p_reader_.readVector("initial guess for the parameters \'p\'"));
    lowerBounds->set_p(p_idx_,p_l_reader_.readVector("lower bounds for the parameters \'p\'"));
    upperBounds->set_p(p_idx_,p_u_reader_.readVector("upper bounds for the parameters \'p\'"));
  }
  nominalModel_->setNominalValues(initialGuess);
  nominalModel_->setLowerBounds(lowerBounds);
  nominalModel_->setUpperBounds(upperBounds);
}

void MoochoThyraSolver::setInitialGuess(
  const Teuchos::RefCountPtr<const Thyra::ModelEvaluatorBase::InArgs<value_type> > &initialGuess
  )
{
  nominalModel_->setNominalValues(initialGuess);
}

void MoochoThyraSolver::setInitialGuess(
  const Thyra::ModelEvaluatorBase::InArgs<value_type> &initialGuess
  )
{
  nominalModel_->setNominalValues(
    Teuchos::rcp(new Thyra::ModelEvaluatorBase::InArgs<value_type>(initialGuess))
    );
}
  
MoochoSolver::ESolutionStatus MoochoThyraSolver::solve()
{
  using Teuchos::RefCountPtr; using Teuchos::null;
  solver_.update_solver();
  std::ostringstream os;
  os
    << "\n**********************************"
    << "\n*** MoochoThyraSolver::solve() ***"
    << "\n**********************************\n";
  const RefCountPtr<const Thyra::VectorSpaceBase<value_type> >
    x_space = outerModel_->get_x_space(),
    p_space = (
      ( p_idx_ >= 0 && outerModel_->Np() > 0 )
      ? outerModel_->get_p_space(p_idx_)
      : null
      );
  if( x_space != null )
    os << "\nx_space: " << x_space->description() << "\n";
  if( p_space != null )
    os << "\np_space: " << p_space->description() << "\n";
  *solver_.get_console_out() << os.str();
  *solver_.get_summary_out() << os.str();
  *solver_.get_journal_out() << os.str();
  return solver_.solve_nlp();
}

const Thyra::ModelEvaluatorBase::InArgs<value_type>&
MoochoThyraSolver::getFinalPoint() const
{
  return finalPointModel_->getFinalPoint();
}

void MoochoThyraSolver::writeFinalSolution(
  std::ostream *out_arg
  ) const
{
  using Teuchos::OSTab;
  Teuchos::RefCountPtr<Teuchos::FancyOStream>
    out = Teuchos::getFancyOStream(Teuchos::rcp(out_arg,false));
  if( stateSoluFileBase_ != "" && finalPointModel_->getFinalPoint().get_x().get() ) {
    if(out.get())
      *out << "\nWriting the state solution \'x\' to the file(s) with base name \""<<stateSoluFileBase_<<"\" ...\n";
    stateVectorIO().writeMultiVectorToFile(
      *finalPointModel_->getFinalPoint().get_x(),stateSoluFileBase_
      );
  }
  if(
    ( "" != paramSoluFileBase_ )
    && ( origModel_->Np() > 0 )
    && ( 0 != finalPointModel_->getFinalPoint().get_p(p_idx_).get() )
    )
  {
    if(out.get())
      *out << "\nWriting the parameter solution \'p\' to the file(s) with base name \""<<paramSoluFileBase_<<"\" ...\n";
    parameterVectorIO().writeMultiVectorToFile(
      *finalPointModel_->getFinalPoint().get_p(p_idx_),paramSoluFileBase_
      );
  }
}

void MoochoThyraSolver::writeParamsFile(
  const std::string &outputXmlFileName
  ) const
{
  std::string xmlOutputFile
    = ( outputXmlFileName.length() ? outputXmlFileName : paramsUsedXmlOutFileName() );
  if( paramList_.get() && xmlOutputFile.length() ) {
    Teuchos::writeParameterListToXmlFile(*paramList_,xmlOutputFile);
  }
}

} // namespace MoochoPack
