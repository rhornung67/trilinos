
/*! @HEADER */
/*
************************************************************************

                CTrilinos:  C interface to Trilinos
                Copyright (2009) Sandia Corporation

Under terms of Contract DE-AC04-94AL85000, there is a non-exclusive
license for use of this work by or on behalf of the U.S. Government.

This library is free software; you can redistribute it and/or modify
it under the terms of the GNU Lesser General Public License as
published by the Free Software Foundation; either version 2.1 of the
License, or (at your option) any later version.

This library is distributed in the hope that it will be useful, but
WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public
License along with this library; if not, write to the Free Software
Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
USA
Questions? Contact M. Nicole Lemaster (mnlemas@sandia.gov)

************************************************************************
*/
/*! @HEADER */


#ifndef CPLIRIS_CPP_HPP
#define CPLIRIS_CPP_HPP


#include "CTrilinos_config.h"

#ifdef HAVE_CTRILINOS_PLIRIS


#include "CTrilinos_enums.h"
#include "Teuchos_RCP.hpp"
#include "Pliris.h"


namespace CPliris {


using Teuchos::RCP;


/*! get Pliris from non-const table using CT_Pliris_ID */
const RCP<Pliris>
getPliris( CT_Pliris_ID_t id );

/*! get Pliris from non-const table using CTrilinos_Universal_ID_t */
const RCP<Pliris>
getPliris( CTrilinos_Universal_ID_t id );

/*! get const Pliris from either the const or non-const table
 * using CT_Pliris_ID */
const RCP<const Pliris>
getConstPliris( CT_Pliris_ID_t id );

/*! get const Pliris from either the const or non-const table
 * using CTrilinos_Universal_ID_t */
const RCP<const Pliris>
getConstPliris( CTrilinos_Universal_ID_t id );

/*! store Pliris (owned) in non-const table */
CT_Pliris_ID_t
storeNewPliris( Pliris *pobj );

/*! store Pliris in non-const table */
CT_Pliris_ID_t
storePliris( Pliris *pobj );

/*! store const Pliris in const table */
CT_Pliris_ID_t
storeConstPliris( const Pliris *pobj );

/* remove Pliris from table using CT_Pliris_ID */
void
removePliris( CT_Pliris_ID_t *id );

/* remove Pliris from table using CTrilinos_Universal_ID_t */
void
removePliris( CTrilinos_Universal_ID_t *aid );

/* purge Pliris table */
void
purgePliris(  );

} // namespace CPliris


#endif /* HAVE_CTRILINOS_PLIRIS */

#endif // CPLIRIS_CPP_HPP


