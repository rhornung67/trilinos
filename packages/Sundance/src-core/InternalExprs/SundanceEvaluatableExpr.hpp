/* @HEADER@ */
// ************************************************************************
// 
//                              Sundance
//                 Copyright (2005) Sandia Corporation
// 
// Copyright (year first published) Sandia Corporation.  Under the terms 
// of Contract DE-AC04-94AL85000 with Sandia Corporation, the U.S. Government 
// retains certain rights in this software.
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
// Questions? Contact Kevin Long (krlong@sandia.gov), 
// Sandia National Laboratories, Livermore, California, USA
// 
// ************************************************************************
/* @HEADER@ */

#ifndef SUNDANCE_EVALUATABLEEXPR_H
#define SUNDANCE_EVALUATABLEEXPR_H



#include "SundanceDefs.hpp"
#include "SundanceScalarExpr.hpp"
#include "SundanceEvaluatorFactory.hpp"
#include "SundanceMap.hpp"
#include "SundanceDerivSet.hpp"
#include "SundanceEvalContext.hpp"
#include "SundanceEvalVector.hpp"
#include "SundanceSparsitySuperset.hpp"
#include "TSFObjectWithVerbosity.hpp"
#include "Teuchos_TimeMonitor.hpp"

#ifndef DOXYGEN_DEVELOPER_ONLY

namespace SundanceCore
{
  using namespace SundanceUtils;
  using namespace Teuchos;
  using namespace SundanceCore::Internal;

  using std::string;
  using std::ostream;
  class Expr;

  namespace Internal
  {
    class MultipleDeriv;
    class EvalManager;
    class Evaluator;
    class EvaluatorFactory;

    /**
     *
     */
    enum DerivSubsetSpecifier {AllNonzeros, 
                               RequiredNonzeros,
                               VariableNonzeros,
                               ConstantNonzeros};

    /**
     *
     */

    class EvaluatableExpr : public virtual ScalarExpr,
                            public virtual EvaluatorFactory,
                            public TSFExtended::ObjectWithVerbosity<EvaluatableExpr>
    {
      typedef OrderedQuartet<EvalContext, 
                             Set<MultiIndex>,
                             Set<MultiSet<int> >,
                             bool> NonzeroSpecifier ;


    public:
      /** Ctor is empty, but has some internal initialization to do
       * and so must be called by all subclasses */
      EvaluatableExpr();

      /** virtual dtor */
      virtual ~EvaluatableExpr(){;}


      /** \name Evaluation */
      //@{
      /**
       * Evaluate this expression in the given region, putting the results
       * of the evaluation in the results argument. 
       */
      void evaluate(const EvalManager& mgr,
                    Array<double>& constantResults,
                    Array<RefCountPtr<EvalVector> >& vectorResults) const ;
      //@}

      /** \name Preprocessing */
      //@{
      /**
       * 
       */
      virtual void setupEval(const EvalContext& context) const ;

      


      /** Return the set of all nonzero derivatives
       * required in the given context */
      RefCountPtr<SparsitySuperset> sparsitySuperset(const EvalContext& context) const ;

      //@}
      
      /** \name Error checking */
      //@{
      /** Test whether all terms have test functions. We'll use this
       * to check the validity of weak forms */
      virtual bool allTermsHaveTestFunctions() const {return false;}
      //@}


      /** Utility to downcast an expression to an evaluatable expr. Throws
       * an exception if the cast fails. */
      static const EvaluatableExpr* getEvalExpr(const Expr& expr);

      /** Return the evaluator to be used for the given context */
      const RefCountPtr<Evaluator>& evaluator(const EvalContext& context) const; 

      /** */
      virtual void showSparsity(ostream& os, 
                                const EvalContext& context) const ;

      /** */
      virtual void getUnknowns(Set<int>& unkID, Array<Expr>& unks) const {;}

      /** */
      virtual int countNodes() const ;

      /** */
      virtual bool nodesHaveBeenCounted() const {return nodesHaveBeenCounted_;}

      /** */
      static unsigned int maxFuncDiffOrder() {static int rtn=3; return rtn;}


      /** */
      virtual Set<MultipleDeriv> 
      internalFindW(int order, const EvalContext& context) const = 0 ;

      /** Find spatially-variable functional derivatives. Default implementation
       * returns R */
      virtual Set<MultipleDeriv> 
      internalFindV(int order, const EvalContext& context) const
      {return findR(order, context);}

      /** Find spatially-constant functional derivatives. Default implementation
       * returns the empty set */
      virtual Set<MultipleDeriv> 
      internalFindC(int order, const EvalContext& context) const
      {Set<MultipleDeriv> rtn; return rtn;}

      

      /** */
      const Set<MultipleDeriv>& 
      findDerivSubset(int order,
                      const DerivSubsetSpecifier& dss,
                      const EvalContext& context) const ;
      /** */
      const Set<MultipleDeriv>& 
      findDerivSubset(const DerivSubsetSpecifier& dss,
                      const EvalContext& context) const ;

      /** */
      const Set<MultipleDeriv>& findW(int order, 
                                      const EvalContext& context) const ;

      /** */
      const Set<MultipleDeriv>& findR(int order, 
                                      const EvalContext& context) const ;
      /** */
      const Set<MultipleDeriv>& findV(int order, 
                                      const EvalContext& context) const ;
      /** */
      const Set<MultipleDeriv>& findC(int order, 
                                      const EvalContext& context) const ;


      /** */
      const Set<MultipleDeriv>& findW(const EvalContext& context) const ;

      /** */
      const Set<MultipleDeriv>& findR(const EvalContext& context) const ;
      /** */
      const Set<MultipleDeriv>& findV(const EvalContext& context) const ;
      /** */
      const Set<MultipleDeriv>& findC(const EvalContext& context) const ;

      /** */
      virtual void displayNonzeros(ostream& os, 
                                   const EvalContext& context) const ;


      /** */
      Set<MultipleDeriv> setProduct(const Set<MultipleDeriv>& a,
                                    const Set<MultipleDeriv>& b) const ;
      /** */
      Set<MultipleDeriv> setDivision(const Set<MultipleDeriv>& a,
                                     const Set<MultipleDeriv>& b) const ;
      /** */
      Set<MultiSet<int> > setDivision(const Set<MultiSet<int> >& s,
                                      int index) const ;
      
      /** */
      void determineR(const EvalContext& context,
                      const Array<Set<MultipleDeriv> >& RInput) const ;

      /** */
      virtual RefCountPtr<Array<Set<MultipleDeriv> > > 
      internalDetermineR(const EvalContext& context,
                         const Array<Set<MultipleDeriv> >& RInput) const ;

      /** */
      const Set<MultipleDeriv>& getR(int order, const EvalContext& context) const ;

      
      /** */
      Array<Set<MultipleDeriv> > 
      computeInputR(const EvalContext& context,
                    const Array<Set<MultiSet<int> > >& funcIDCombinations,
                    const Array<Set<MultiIndex> >& spatialDerivs) const ;


      /** */
      virtual void registerSpatialDerivs(const EvalContext& context, 
                                         const Set<MultiIndex>& miSet) const ;
      
                
      
    protected:

      /** Record the evaluator to be used for the given context */
      void registerEvaluator(const EvalContext& context,
                             const RefCountPtr<Evaluator>& evaluator) const 
      {return evaluators_.put(context, evaluator);}

      /** */
      static bool isEvaluatable(const ExprBase* expr);

      /** */
      Map<EvalContext, RefCountPtr<Evaluator> >& evaluators() const 
      {return evaluators_;}


      /** */
      int maxOrder(const Set<MultiIndex>& m) const ;

      /** */
      const Set<MultiIndex>& activeSpatialDerivs(const EvalContext& context) const ;

      /** */
      string derivType(const DerivSubsetSpecifier& dss) const;

    private:
      
      /** */
      static int numDerivSubsetTypes() {static int rtn=4; return rtn;}

      /** 
       * evaluators, indexed by context 
       */
      mutable Map<EvalContext, RefCountPtr<Evaluator> > evaluators_;

      /** 
       * supersets of nonzero derivatives to be computed, index by
       * context
       */
      mutable Map<EvalContext, RefCountPtr<SparsitySuperset> > sparsity_;

      /** Polynomial order of the dependency upon each coordinate direction */
      Array<int> orderOfDependency_;

      /** Set of function combinations appearing in nonzero mixed partials */ 
      Set<MultiSet<int> > funcIDSet_;

      /** */
      Set<int> funcDependencies_;

      mutable Set<NonzeroSpecifier> knownNonzeros_;

      mutable bool nodesHaveBeenCounted_; 

      typedef Array<Map<EvalContext, Set<MultipleDeriv> > > contextToDSSMap_ele_t;
      mutable Array<contextToDSSMap_ele_t> contextToDSSMap_;

      mutable bool rIsReady_;

      mutable Array<Map<EvalContext, Set<MultipleDeriv> > > allOrdersMap_;

      mutable Map<EvalContext, Set<MultiIndex> > activeSpatialDerivMap_;
    };
  }
}

#endif /* DOXYGEN_DEVELOPER_ONLY */
#endif
