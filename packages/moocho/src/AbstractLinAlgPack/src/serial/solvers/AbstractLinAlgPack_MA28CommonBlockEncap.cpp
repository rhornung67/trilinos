// ////////////////////////////////////////////////////////////////////////////
// MA28CommonBlockEncap.cpp
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

#ifdef SPARSE_SOLVER_PACK_USE_MA28

#include "../include/MA28CommonBlockEncap.h"

using std::cout;
using std::endl;

// /////////////////////////////////////////////////////////////////////////////
// MA28CommonBlockReferences

MA28_Cpp::MA28CommonBlockReferences& MA28_Cpp::MA28CommonBlockReferences::operator=(
	const MA28CommonBlockStorage& ma28cb)
{
	ma28ed_ = ma28cb.ma28ed_;
	ma28fd_ = ma28cb.ma28fd_;
	ma28gd_ = ma28cb.ma28gd_;
	ma28hd_ = ma28cb.ma28hd_;
	ma30ed_ = ma28cb.ma30ed_;
	ma30fd_ = ma28cb.ma30fd_;
	ma30gd_ = ma28cb.ma30gd_;
	ma30hd_ = ma28cb.ma30hd_;
	ma30id_ = ma28cb.ma30id_;
	mc23bd_ = ma28cb.mc23bd_;
	return *this;
}


void MA28_Cpp::MA28CommonBlockReferences::dump_values(std::ostream& o) {
	o		<< "ma28ed.lp = "		<< ma28ed_.lp		<< endl
			<< "ma28ed.mp = "		<< ma28ed_.mp		<< endl
			<< "ma28ed.lblock = "	<< ma28ed_.lblock	<< endl
			<< "ma28ed.grow = "		<< ma28ed_.grow		<< endl
			<< "ma28fd.eps = "		<< ma28fd_.eps		<< endl
			<< "ma28fd.rmin = "		<< ma28fd_.rmin		<< endl
			<< "ma28fd.resid = "	<< ma28fd_.resid	<< endl
			<< "ma28fd.irncp = "	<< ma28fd_.irncp	<< endl
			<< "ma28fd.icncp = "	<< ma28fd_.icncp	<< endl
			<< "ma28fd.minirn = "	<< ma28fd_.minirn	<< endl
			<< "ma28fd.minicn = "	<< ma28fd_.minicn	<< endl
			<< "ma28fd.abort1 = "	<< ma28fd_.abort1	<< endl
			<< "ma28fd.abort2 = "	<< ma28fd_.abort2	<< endl
			<< "ma28gd.idisp[0] = "	<< ma28gd_.idisp[0] << endl
			<< "ma28gd.idisp[1] = "	<< ma28gd_.idisp[1] << endl
			<< "ma28hd.tol = "		<< ma28hd_.tol		<< endl
			<< "ma28hd.themax = "	<< ma28hd_.themax	<< endl
			<< "ma28hd.big = "		<< ma28hd_.big		<< endl
			<< "ma28hd.dxmax = "	<< ma28hd_.dxmax	<< endl
			<< "ma28hd.errmax = "	<< ma28hd_.errmax	<< endl
			<< "ma28hd.dres = "		<< ma28hd_.dres		<< endl
			<< "ma28hd.cgce = "		<< ma28hd_.cgce		<< endl
			<< "ma28hd.ndrop = "	<< ma28hd_.ndrop	<< endl
			<< "ma28hd.maxit = "	<< ma28hd_.maxit	<< endl
			<< "ma28hd.noiter = "	<< ma28hd_.noiter	<< endl
			<< "ma28hd.nsrch = "	<< ma28hd_.nsrch	<< endl
			<< "ma28hd.istart = "	<< ma28hd_.istart	<< endl
			<< "ma28hd.lbig = "		<< ma28hd_.lbig		<< endl
			<< "ma30ed.lp = "		<< ma30ed_.lp		<< endl
			<< "ma30ed.abort1 = "	<< ma30ed_.abort1	<< endl
			<< "ma30ed.abort2 = "	<< ma30ed_.abort2	<< endl
			<< "ma30ed.abort3 = "	<< ma30ed_.abort3	<< endl
			<< "ma30fd.irncp = "	<< ma30fd_.irncp	<< endl
			<< "ma30fd.icncp = "	<< ma30fd_.icncp	<< endl
			<< "ma30fd.irank = "	<< ma30fd_.irank	<< endl
			<< "ma30fd.minirn = "	<< ma30fd_.minirn	<< endl
			<< "ma30fd.minicn = "	<< ma30fd_.minicn	<< endl
			<< "ma30gd.eps = "		<< ma30gd_.eps		<< endl
			<< "ma30gd.rmin = "		<< ma30gd_.rmin		<< endl
			<< "ma30hd.resid = "	<< ma30hd_.resid	<< endl
			<< "ma30id.tol = "		<< ma30id_.tol		<< endl
			<< "ma30id.big = "		<< ma30id_.big		<< endl
			<< "ma30id.ndrop = "	<< ma30id_.ndrop	<< endl
			<< "ma30id.nsrch = "	<< ma30id_.nsrch	<< endl
			<< "ma30id.lbig = "		<< ma30id_.lbig		<< endl;
}

// /////////////////////////////////////////////////////////////////////////////
// MA28CommonBlockStorage

MA28_Cpp::MA28CommonBlockStorage& MA28_Cpp::MA28CommonBlockStorage::operator=(
	const MA28CommonBlockReferences& ma28cb)
{
	ma28ed_ = ma28cb.ma28ed_;
	ma28fd_ = ma28cb.ma28fd_;
	ma28gd_ = ma28cb.ma28gd_;
	ma28hd_ = ma28cb.ma28hd_;
	ma30ed_ = ma28cb.ma30ed_;
	ma30fd_ = ma28cb.ma30fd_;
	ma30gd_ = ma28cb.ma30gd_;
	ma30hd_ = ma28cb.ma30hd_;
	ma30id_ = ma28cb.ma30id_;
	mc23bd_ = ma28cb.mc23bd_;
	return *this;
}

void MA28_Cpp::MA28CommonBlockStorage::dump_values(std::ostream& o) {
	o		<< "ma28ed.lp = "		<< ma28ed_.lp		<< endl
			<< "ma28ed.mp = "		<< ma28ed_.mp		<< endl
			<< "ma28ed.lblock = "	<< ma28ed_.lblock	<< endl
			<< "ma28ed.grow = "		<< ma28ed_.grow		<< endl
			<< "ma28fd.eps = "		<< ma28fd_.eps		<< endl
			<< "ma28fd.rmin = "		<< ma28fd_.rmin		<< endl
			<< "ma28fd.resid = "	<< ma28fd_.resid	<< endl
			<< "ma28fd.irncp = "	<< ma28fd_.irncp	<< endl
			<< "ma28fd.icncp = "	<< ma28fd_.icncp	<< endl
			<< "ma28fd.minirn = "	<< ma28fd_.minirn	<< endl
			<< "ma28fd.minicn = "	<< ma28fd_.minicn	<< endl
			<< "ma28fd.abort1 = "	<< ma28fd_.abort1	<< endl
			<< "ma28fd.abort2 = "	<< ma28fd_.abort2	<< endl
			<< "ma28gd.idisp[0] = "	<< ma28gd_.idisp[0] << endl
			<< "ma28gd.idisp[1] = "	<< ma28gd_.idisp[1] << endl
			<< "ma28hd.tol = "		<< ma28hd_.tol		<< endl
			<< "ma28hd.themax = "	<< ma28hd_.themax	<< endl
			<< "ma28hd.big = "		<< ma28hd_.big		<< endl
			<< "ma28hd.dxmax = "	<< ma28hd_.dxmax	<< endl
			<< "ma28hd.errmax = "	<< ma28hd_.errmax	<< endl
			<< "ma28hd.dres = "		<< ma28hd_.dres		<< endl
			<< "ma28hd.cgce = "		<< ma28hd_.cgce		<< endl
			<< "ma28hd.ndrop = "	<< ma28hd_.ndrop	<< endl
			<< "ma28hd.maxit = "	<< ma28hd_.maxit	<< endl
			<< "ma28hd.noiter = "	<< ma28hd_.noiter	<< endl
			<< "ma28hd.nsrch = "	<< ma28hd_.nsrch	<< endl
			<< "ma28hd.istart = "	<< ma28hd_.istart	<< endl
			<< "ma28hd.lbig = "		<< ma28hd_.lbig		<< endl
			<< "ma30ed.lp = "		<< ma30ed_.lp		<< endl
			<< "ma30ed.abort1 = "	<< ma30ed_.abort1	<< endl
			<< "ma30ed.abort2 = "	<< ma30ed_.abort2	<< endl
			<< "ma30ed.abort3 = "	<< ma30ed_.abort3	<< endl
			<< "ma30fd.irncp = "	<< ma30fd_.irncp	<< endl
			<< "ma30fd.icncp = "	<< ma30fd_.icncp	<< endl
			<< "ma30fd.irank = "	<< ma30fd_.irank	<< endl
			<< "ma30fd.minirn = "	<< ma30fd_.minirn	<< endl
			<< "ma30fd.minicn = "	<< ma30fd_.minicn	<< endl
			<< "ma30gd.eps = "		<< ma30gd_.eps		<< endl
			<< "ma30gd.rmin = "		<< ma30gd_.rmin		<< endl
			<< "ma30hd.resid = "	<< ma30hd_.resid	<< endl
			<< "ma30id.tol = "		<< ma30id_.tol		<< endl
			<< "ma30id.big = "		<< ma30id_.big		<< endl
			<< "ma30id.ndrop = "	<< ma30id_.ndrop	<< endl
			<< "ma30id.nsrch = "	<< ma30id_.nsrch	<< endl
			<< "ma30id.lbig = "		<< ma30id_.lbig		<< endl;
}

#endif // SPARSE_SOLVER_PACK_USE_MA28
