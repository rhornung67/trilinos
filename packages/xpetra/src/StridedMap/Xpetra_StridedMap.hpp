#ifndef XPETRA_STRIDEDMAP_HPP
#define XPETRA_STRIDEDMAP_HPP

/* this file is automatically generated - do not edit (see script/interfaces.py) */

#include <Kokkos_DefaultNode.hpp>
#include <Teuchos_Describable.hpp>
#include "Xpetra_Map.hpp"
#include "Xpetra_ConfigDefs.hpp"

#include "Xpetra_Exceptions.hpp"

namespace Xpetra {


  template <class LocalOrdinal, class GlobalOrdinal = LocalOrdinal, class Node = Kokkos::DefaultNode::DefaultNodeType>
  class StridedMap
    : public Map<LocalOrdinal, GlobalOrdinal, Node>
  {

  public:

    //! @name Constructor/Destructor Methods
    //@{ 

    StridedMap(global_size_t numGlobalElements, GlobalOrdinal indexBase, std::vector<size_t>& stridingInfo, const Teuchos::RCP< const Teuchos::Comm< int > > &comm, LocalOrdinal stridedBlockId)
    : stridingInfo_(stridingInfo), stridedBlockId_(stridedBlockId)
    { 
      TEUCHOS_TEST_FOR_EXCEPTION(stridingInfo.size() == 0, Exceptions::RuntimeError, "StridedMap::StridedMap: stridingInfo not valid: stridingInfo.size() = 0?");
      TEUCHOS_TEST_FOR_EXCEPTION(numGlobalElements % getFixedBlockSize() != 0, Exceptions::RuntimeError, "StridedMap::StridedMap: stridingInfo not valid: getFixedBlockSize is not an integer multiple of numGlobalElements.");

    }

    StridedMap(global_size_t numGlobalElements, size_t numLocalElements, GlobalOrdinal indexBase, std::vector<size_t>& stridingInfo, const Teuchos::RCP< const Teuchos::Comm< int > > &comm, LocalOrdinal stridedBlockId)
    : stridingInfo_(stridingInfo), stridedBlockId_(stridedBlockId)
    { 
      TEUCHOS_TEST_FOR_EXCEPTION(stridingInfo.size() == 0, Exceptions::RuntimeError, "StridedMap::StridedMap: stridingInfo not valid: stridingInfo.size() = 0?");
      TEUCHOS_TEST_FOR_EXCEPTION(numGlobalElements % getFixedBlockSize() != 0, Exceptions::RuntimeError, "StridedMap::StridedMap: stridingInfo not valid: getFixedBlockSize is not an integer multiple of numGlobalElements.");
      TEUCHOS_TEST_FOR_EXCEPTION(numLocalElements % getFixedBlockSize() != 0, Exceptions::RuntimeError, "StridedMap::StridedMap: stridingInfo not valid: getFixedBlockSize is not an integer multiple of numLocalElements.");
    
    }

#if 0
    StridedMap(global_size_t numGlobalElements, const Teuchos::ArrayView< const GlobalOrdinal > &elementList, GlobalOrdinal indexBase, std::vector<size_t>& stridingInfo, const Teuchos::RCP< const Teuchos::Comm< int > > &comm, LocalOrdinal stridedBlockId=-1)
    : stridingInfo_(stridingInfo), stridedBlockId_(stridedBlockId)
    {
      TEUCHOS_TEST_FOR_EXCEPTION(stridingInfo.size() == 0, Exceptions::RuntimeError, "StridedMap::StridedMap: stridingInfo not valid: stridingInfo.size() = 0?");
      TEUCHOS_TEST_FOR_EXCEPTION(numGlobalElements % getFixedBlockSize() != 0, Exceptions::RuntimeError, "StridedMap::StridedMap: stridingInfo not valid: getFixedBlockSize is not an integer multiple of numGlobalElements.");
      TEUCHOS_TEST_FOR_EXCEPTION(elementList.size() % getFixedBlockSize() != 0, Exceptions::RuntimeError, "StridedMap::StridedMap: stridingInfo not valid: getFixedBlockSize is not an integer multiple of elementList.size().");
       
    }     
#endif     
     
    //! Destructor.
    virtual ~StridedMap() { }

   //@}

    //! @name Access functions for striding data
    //@{
    
      std::vector<size_t> getStridingData() { return stridingInfo_; }
      
      void setStridingData(std::vector<size_t> stridingInfo) { stridingInfo_ = stridingInfo; }
      
      size_t getFixedBlockSize() const { 
	// sum up size of all strided blocks (= number of dofs per node)
	size_t blkSize = 0;
	std::vector<size_t>::const_iterator it;
	for(it = stridingInfo_.begin(); it != stridingInfo_.end(); ++it) {
	  blkSize += *it;
	}
	return blkSize;
      }
      
      /// returns strided block id of the dofs stored in this map
      /// or -1 if full strided map is stored in this map
      LocalOrdinal getStridedBlockId() { return stridedBlockId_; }
      
      /// returns true, if this is a strided map (i.e. more than 1 strided blocks)
      bool isStrided() { return stridingInfo_.size() > 1 ? true : false; }
      
      /// returns true, if this is a blocked map (i.e. more than 1 dof per node)
      /// either strided or just 1 block per node
      bool isBlocked() { return getFixedBlockSize() > 1 ? true : false; }
    //@}


    
  protected:
    std::vector<size_t> stridingInfo_;   // vector with size of strided blocks (dofs)
    LocalOrdinal stridedBlockId_;        // member variable denoting which dofs are stored in map
                                         // stridedBlock == -1: the full map (with all strided block dofs)
                                         // stridedBlock >  -1: only dofs of strided block with index "stridedBlockId" are stored in this map

  }; // StridedMap class

} // Xpetra namespace

#define XPETRA_STRIDEDMAP_SHORT
#endif // XPETRA_STRIDEDMAP_HPP
