/* @HEADER@ */
/* @HEADER@ */

#include "SundanceExprWithChildren.hpp"
#include "SundanceTabs.hpp"
#include "SundanceOut.hpp"
#include "SundanceExpr.hpp"
#include "SundanceEvaluatorFactory.hpp"
#include "SundanceEvaluator.hpp"
#include "SundanceUnknownFuncElement.hpp"
#include "SundanceUnaryExpr.hpp"

using namespace SundanceCore;
using namespace SundanceUtils;

using namespace SundanceCore::Internal;
using namespace Teuchos;
using namespace TSFExtended;



ExprWithChildren::ExprWithChildren(const Array<RefCountPtr<ScalarExpr> >& children)
	: EvaluatableExpr(), 
    children_(children)
{}


bool ExprWithChildren::isConstant() const
{
  for (unsigned int i=0; i<children_.size(); i++) 
    {
      if (!children_[i]->isConstant()) return false;
    }
  return true;
}

void ExprWithChildren::accumulateUnkSet(Set<int>& unkIDs) const
{
  for (unsigned int i=0; i<children_.size(); i++) 
    {
      children_[i]->accumulateUnkSet(unkIDs);
    }
}

void ExprWithChildren::accumulateTestSet(Set<int>& testIDs) const
{
  for (unsigned int i=0; i<children_.size(); i++) 
    {
      children_[i]->accumulateTestSet(testIDs);
    }
}

const EvaluatableExpr* ExprWithChildren::evaluatableChild(int i) const
{
  const EvaluatableExpr* e 
    = dynamic_cast<const EvaluatableExpr*>(children_[i].get());

  TEST_FOR_EXCEPTION(e==0, InternalError, 
                     "ExprWithChildren: cast of child [" 
                     << children_[i]->toString()
                     << " to evaluatable expr failed");

  return e;
}


void ExprWithChildren::findNonzeros(const EvalContext& context,
                                    const Set<MultiIndex>& multiIndices,
                                    const Set<MultiSet<int> >& activeFuncIDs,
                                    bool regardFuncsAsConstant) const
{
  Tabs tabs;
  SUNDANCE_VERB_MEDIUM(tabs << "finding nonzeros for " 
                       << toString() << " subject to multiindex set "
                       << multiIndices.toString());
  if (nonzerosAreKnown(context, multiIndices, activeFuncIDs,
                       regardFuncsAsConstant))
    {
      SUNDANCE_VERB_MEDIUM(tabs << "...reusing previously computed data");
      return;
    }

  const UnaryExpr* ue = dynamic_cast<const UnaryExpr*>(this);
  if (ue != 0) ue->addActiveFuncs(context, activeFuncIDs);

  RefCountPtr<SparsitySubset> subset = sparsitySubset(context, multiIndices, activeFuncIDs);

  /* The sparsity pattern is the union of the 
   * operands' sparsity patterns. If any functional derivatives
   * appear in multiple operands, the state of that derivative is
   * the more general of the states */
  for (unsigned int i=0; i<children_.size(); i++)
    {
      Tabs tab1;
      SUNDANCE_VERB_MEDIUM(tab1 << "finding nonzeros for child " 
                           << evaluatableChild(i)->toString());
      evaluatableChild(i)->findNonzeros(context, multiIndices,
                                        activeFuncIDs,
                                        regardFuncsAsConstant);

      RefCountPtr<SparsitySubset> childSparsitySubset 
        = evaluatableChild(i)->sparsitySubset(context, multiIndices, activeFuncIDs);
          

      SUNDANCE_VERB_MEDIUM(tabs << "child #" << i 
                           << " sparsity subset is " 
                           << endl << *childSparsitySubset);
      
      for (int j=0; j<childSparsitySubset->numDerivs(); j++)
        {
          subset->addDeriv(childSparsitySubset->deriv(j),
                           childSparsitySubset->state(j));
        }
    }

  SUNDANCE_VERB_HIGH(tabs << "expr " + toString() << ": my sparsity subset is " 
                     << endl << *subset);

  SUNDANCE_VERB_HIGH(tabs << "expr " + toString() 
                     << " my sparsity superset is " 
                     << endl << *sparsitySuperset(context));

  addKnownNonzero(context, multiIndices, activeFuncIDs,
                  regardFuncsAsConstant);
}

void ExprWithChildren::setupEval(const EvalContext& context) const
{
  Tabs tabs;
  SUNDANCE_VERB_HIGH(tabs << "expr " + toString() 
                     << ": creating evaluators for children");
  for (unsigned int i=0; i<children_.size(); i++)
    {
      Tabs tabs1;
      SUNDANCE_VERB_HIGH(tabs1 << "creating evaluator for child " 
                         << evaluatableChild(i)->toString());
      evaluatableChild(i)->setupEval(context);
    }

  if (!evaluators().containsKey(context))
    {
      RefCountPtr<Evaluator> eval = rcp(createEvaluator(this, context));
      evaluators().put(context, eval);
    }
}

void ExprWithChildren::showSparsity(ostream& os, 
                                    const EvalContext& context) const
{
  Tabs tab0;
  os << tab0 << "Node: " << toString() << endl;
  sparsitySuperset(context)->displayAll(os);
  for (unsigned int i=0; i<children_.size(); i++)
    {
      Tabs tab1;
      os << tab1 << "Child " << i << endl;
      evaluatableChild(i)->showSparsity(os, context);
    }
}


bool ExprWithChildren::allTermsHaveTestFunctions() const
{
  for (unsigned int i=0; i<children_.size(); i++)
    {
      if (evaluatableChild(i)->allTermsHaveTestFunctions()) return true;
    }
  return false;
}

void ExprWithChildren::getUnknowns(Set<int>& unkID, Array<Expr>& unks) const
{
  for (unsigned int i=0; i<children_.size(); i++)
    {
      const RefCountPtr<ExprBase>& e = children_[i];
      const UnknownFuncElement* u 
        = dynamic_cast<const UnknownFuncElement*>(e.get());
      if (u != 0)
        {
          Expr expr(e);
          if (!unkID.contains(u->funcID())) 
            {
              unks.append(expr);
              unkID.put(u->funcID());
            }
        }
      evaluatableChild(i)->getUnknowns(unkID, unks);
    }
  
}


int ExprWithChildren::countNodes() const
{
  if (nodesHaveBeenCounted()) 
    {
      return 0;
    }

  /* count self */
  int count = EvaluatableExpr::countNodes();

  /* count children */
  for (unsigned int i=0; i<children_.size(); i++)
    {
      if (!evaluatableChild(i)->nodesHaveBeenCounted())
        {
          count += evaluatableChild(i)->countNodes();
        }
    }
  return count;
}
