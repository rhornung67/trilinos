// ////////////////////////////////////////////////////////////////////////////
// ReducedGradientStd_Step.h
//
// Copyright (C) 2001 Roscoe Ainsworth Bartlett
//
// This is free software; you can redistribute it and/or modify it
// under the terms of the "Artistic License" (see the web site
//   http://www.opensource.org/licenses/artistic-license.html).
// This license is spelled out in the file COPYING.
//
// This software is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// above mentioned "Artistic License" for more details.

#ifndef REDUCED_GRADIENT_STD_STEP_H
#define REDUCED_GRADIENT_STD_STEP_H

#include "../rSQPAlgo_StepBaseClasses.h"
#include "../rSQPAlgo.h"
#include "Misc/include/StandardCompositionRelationshipsPack.h"

namespace ReducedSpaceSQPPack {

///
/** rGf_k = Z_k' * Gf_k
  */
class ReducedGradientStd_Step : public ReducedGradient_Step {
public:

	// ////////////////////
	// Overridden

	///
	bool do_step(Algorithm& algo, poss_type step_poss, GeneralIterationPack::EDoStepType type
		, poss_type assoc_step_poss);

	///
	void print_step( const Algorithm& algo, poss_type step_poss, GeneralIterationPack::EDoStepType type
		, poss_type assoc_step_poss, std::ostream& out, const std::string& leading_str ) const;

};	// end class ReducedGradientStd_Step

}	// end namespace ReducedSpaceSQPPack 

#endif	// REDUCED_GRADIENT_STD_STEP_H
