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

#ifndef SUNDANCE_CELLREORDERERBASE_H
#define SUNDANCE_CELLREORDERERBASE_H


#ifndef DOXYGEN_DEVELOPER_ONLY

#include "SundanceDefs.hpp"
#include "SundanceNoncopyable.hpp"
#include "SundanceCellReordererImplemBase.hpp"
#include "TSFHandleable.hpp"
#include "TSFPrintable.hpp"
#include "TSFDescribable.hpp"

namespace SundanceStdMesh
{
  namespace Internal
  {
    using namespace Teuchos;
using namespace SundanceUtils;

    /**
     * Factory class to instantiate cell reorderers for specific meshes
     */
    class CellReordererFactoryBase 
      : public TSFExtended::Handleable<CellReordererFactoryBase>, 
        public Noncopyable,
        public TSFExtended::Printable,
        public TSFExtended::Describable
    {
    public:
      /** */
      CellReordererFactoryBase() {;}
      
      /** virtual dtor */
      virtual ~CellReordererFactoryBase(){;}

      /** */
      virtual string description() const {return typeid(*this).name();}

      /** */
      virtual void print(ostream& os) const {os << description();}

      /** Instantiate a factory */
      virtual RefCountPtr<CellReordererImplemBase> 
      createInstance(const MeshBase* mesh) const = 0 ;
    };

    /**
     * Factory class to instantiate cell reorderers for specific meshes
     */
    template <class T>
    class GenericCellReordererFactory : public CellReordererFactoryBase
    {
    public:
      /** */
      GenericCellReordererFactory() {;}
      
      /** virtual dtor */
      virtual ~GenericCellReordererFactory(){;}

      /** Instantiate a factory */
      virtual RefCountPtr<CellReordererImplemBase> 
      createInstance(const MeshBase* mesh) const 
      {
        return rcp(new T(mesh));
      }
      
    };
  }
}

#endif /* DOXYGEN_DEVELOPER_ONLY */

#endif
