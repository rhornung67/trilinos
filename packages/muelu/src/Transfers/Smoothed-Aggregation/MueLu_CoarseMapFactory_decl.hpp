// @HEADER
//
// ***********************************************************************
//
//        MueLu: A package for multigrid based preconditioning
//                  Copyright 2012 Sandia Corporation
//
// Under the terms of Contract DE-AC04-94AL85000 with Sandia Corporation,
// the U.S. Government retains certain rights in this software.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
// 1. Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//
// 2. Redistributions in binary form must reproduce the above copyright
// notice, this list of conditions and the following disclaimer in the
// documentation and/or other materials provided with the distribution.
//
// 3. Neither the name of the Corporation nor the names of the
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY SANDIA CORPORATION "AS IS" AND ANY
// EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
// PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL SANDIA CORPORATION OR THE
// CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
// EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
// PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
// PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
// LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
// NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
// SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//
// Questions? Contact
//                    Jeremie Gaidamour (jngaida@sandia.gov)
//                    Jonathan Hu       (jhu@sandia.gov)
//                    Ray Tuminaro      (rstumin@sandia.gov)
//
// ***********************************************************************
//
// @HEADER
/*
 * MueLu_CoarseMapFactory_decl.hpp
 *
 *  Created on: Oct 12, 2012
 *      Author: wiesner
 */

#ifndef MUELU_COARSEMAPFACTORY_DECL_HPP_
#define MUELU_COARSEMAPFACTORY_DECL_HPP_

#include "Xpetra_StridedMapFactory_fwd.hpp"

#include "MueLu_ConfigDefs.hpp"
#include "MueLu_SingleLevelFactoryBase.hpp"
#include "MueLu_CoarseMapFactory_fwd.hpp"

#include "MueLu_Level_fwd.hpp"
#include "MueLu_Aggregates_fwd.hpp"
#include "MueLu_Exceptions.hpp"

namespace MueLu {

  /*!
     @class CoarseMapFactory class.
     @brief Factory for generating coarse level map. Used by TentativePFactory.

     @ingroup MueLuTransferClasses
  */

  template <class Scalar = double, class LocalOrdinal = int, class GlobalOrdinal = LocalOrdinal, class Node = Kokkos::DefaultNode::DefaultNodeType, class LocalMatOps = typename Kokkos::DefaultKernels<void,LocalOrdinal,Node>::SparseOps>
  class CoarseMapFactory : public SingleLevelFactoryBase {
#undef MUELU_COARSEMAPFACTORY_SHORT
#include "MueLu_UseShortNames.hpp"

  public:

    //! @name Constructors/Destructors.
    //@{

    //! Constructor
    CoarseMapFactory(RCP<const FactoryBase> aggregatesFact = Teuchos::null, RCP<const FactoryBase> nullspaceFact = Teuchos::null);

    //! Destructor
    virtual ~CoarseMapFactory();

    //@}

    //! @name Input
    //@{
    /*! @brief Specifies the data that this class needs, and the factories that generate that data.

        If the Build method of this class requires some data, but the generating factory is not specified in DeclareInput, then this class
        will fall back to the settings in FactoryManager.
    */

    void DeclareInput(Level &currentLevel) const;

    //@}

    //! @name Build methods.
    //@{

    //! Build an object with this factory.
    void Build(Level &currentLevel) const;

    //@}


    /*! @brief getFixedBlockSize
     * returns the full block size (number of DOFs per node) of the domain DOF map (= coarse map).
     * This is the sum of all entries in the striding vector.
     * e.g. for 2 velocity dofs and 1 pressure dof the return value is 3.
     */
    size_t getFixedBlockSize() const {
      // sum up size of all strided blocks (= number of dofs per node)
      size_t blkSize = 0;
      std::vector<size_t>::const_iterator it;
      for(it = stridingInfo_.begin(); it != stridingInfo_.end(); ++it) {
        blkSize += *it;
      }
      return blkSize;
    }

  private:
    RCP<const FactoryBase> aggregatesFact_; //! Factory that creates aggregates
    RCP<const FactoryBase> nullspaceFact_;  //! Factory creating the nullspace

    GlobalOrdinal domainGidOffset_; //! offset for domain gids (coarse gids) of tentative prolongator  (default = 0). The GIDs for the domain dofs of Ptent start with domainGidOffset, are contiguous and distributed equally over the procs (unless some reordering is done).

    mutable std::vector<size_t> stridingInfo_;   // vector with size of strided blocks (dofs)
    LocalOrdinal stridedBlockId_;        // member variable denoting which dofs are stored in map
                                         // stridedBlock == -1: the full map (with all strided block dofs)
                                         // stridedBlock >  -1: only dofs of strided block with index "stridedBlockId" are stored in this map


  }; //class CoarseMapFactory

} //namespace MueLu

#define MUELU_COARSEMAPFACTORY_SHORT
#endif /* MUELU_COARSEMAPFACTORY_DECL_HPP_ */
