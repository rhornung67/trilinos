// @HEADER
// ***********************************************************************
// 
//                    Teuchos: Common Tools Package
//                 Copyright (2004) Sandia Corporation
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
// Questions? Contact Michael A. Heroux (maherou@sandia.gov) 
// 
// ***********************************************************************
// @HEADER

//#include "Teuchos_XMLParameterListWriter.hpp"
#include "Teuchos_XMLParameterListReader.hpp"
#include "Teuchos_TestForException.hpp"
//#include "Teuchos_StrUtils.hpp"
#include "Teuchos_ParameterEntryXMLConverterDB.hpp"


using namespace Teuchos;

XMLParameterListReader::XMLParameterListReader()
{;}

ParameterList XMLParameterListReader::toParameterList(const XMLObject& xml) const
{
  TEST_FOR_EXCEPTION(xml.getTag() != "ParameterList", std::runtime_error,
                     "XMLParameterListReader expected tag ParameterList, found "
                     << xml.getTag());

  ParameterList rtn;

  for (int i=0; i<xml.numChildren(); i++)
    {
      XMLObject child = xml.getChild(i);

      TEST_FOR_EXCEPTION( (child.getTag() != "ParameterList" 
                           && child.getTag() != "Parameter"), 
                         std::runtime_error,
                         "XMLParameterListReader expected tag "
                         "ParameterList or Parameter, found "
                         << child.getTag());

      if (child.getTag()=="ParameterList")
        {
          TEST_FOR_EXCEPTION( !child.hasAttribute("name"), 
                         std::runtime_error,
                         "ParameterList tags must "
						 "have a name attribute"
                         << child.getTag());

		  const std::string& name = child.getRequired("name");

          ParameterList sublist = toParameterList(child);
          sublist.setName(name);

          rtn.set(name, sublist);
        }
      else
        {
          const std::string& name = child.getRequired("name");
			RCP<ParameterEntryXMLConverter> converter = ParameterEntryXMLConverterDB::getConverter(child);
			rtn.setEntry(name, converter->fromXMLtoParameterEntry(child));
        }
    }
  return rtn;
}

