/*
 * ExtendedHashtable.hpp
 *
 *  Created on: Aug 29, 2011
 *      Author: wiesner
 */
#ifndef MUELU_EXTENDENDEDHASHTABLE_HPP
#define MUELU_EXTENDENDEDHASHTABLE_HPP

#include <MueLu_Exceptions.hpp>
#include <MueLu_BaseClass.hpp>
#include <MueLu_FactoryBase.hpp>
#include <Teuchos_TabularOutputter.hpp>
#include <Teuchos_ParameterEntry.hpp>
#include <Teuchos_map.hpp>


namespace MueLu
{
using std::string;

namespace UTILS
{
class ExtendedHashtable : MueLu::BaseClass
{
    //! map container typedef (factory ptr -> value)
    typedef Teuchos::map<const MueLu::FactoryBase*, Teuchos::ParameterEntry> dataMapType;

    //! hashtable container typedef (map of a map)
    typedef Teuchos::map<const string, dataMapType > dataTableType;

    //! hashtable container iterator typedef
    typedef dataTableType::iterator Iterator;

    //! hashtable container iterator typedef
    typedef dataTableType::const_iterator ConstIterator;

public:
    //! @name Public types
    //@{
    //! map container iterator typedef
    typedef dataMapType::iterator MapIterator;

    //! map container const iterator typedef
    typedef dataMapType::const_iterator ConstMapIterator;
    //@}

    inline ExtendedHashtable() {};


    template<typename Value> inline void Set(const string& ename, const Value& evalue, const FactoryBase* factory)
    {
        // if ename does not exist at all
        if (!dataTable_.count(ename) > 0)
        {
            Teuchos::map<const MueLu::FactoryBase*,Teuchos::ParameterEntry> newmapData;
            dataTable_[ename] = newmapData; // empty map
        }

        dataMapType& mapData = dataTable_[ename];
        Teuchos::ParameterEntry &foundEntry = mapData[factory]; // will add the entry for key 'factory' if not exists
        foundEntry.setValue(evalue);
    }

    template<typename Value> Value& Get(const string& ename, const FactoryBase* factory)
    {
        if(!dataTable_.count(ename) > 0)
        {
            std::stringstream str; str << "key" << ename << " does not exist in Hashtable.";
            throw(Exceptions::RuntimeError(str.str()));
        }

        if(!dataTable_[ename].count(factory) > 0)
        {
            std::stringstream str; str << "key " << ename << " generated by " << factory << " does not exist in Hashtable.";
            throw(Exceptions::RuntimeError(str.str()));
        }

        dataMapType& mapData = dataTable_[ename];

        ConstMapIterator i = mapData.find(factory);
        return Teuchos::getValue<Value>(entry(i));
    }

    template<typename Value>
    const Value& Get(const string& ename, const FactoryBase* factory) const
    {
        if(!dataTable_.count(ename) > 0)
        {
            std::stringstream str; str << "key" << ename << " does not exist in Hashtable.";
            throw(Exceptions::RuntimeError(str.str()));
        }

        ConstIterator k = dataTable_.find(ename);
        dataMapType mapData = k->second;

        if(!mapData.count(factory) > 0)
        {
            std::stringstream str; str << "key " << ename << " generated by " << factory << " does not exist in Hashtable.";
            throw(Exceptions::RuntimeError(str.str()));
        }

        ConstMapIterator i = mapData.find(factory);
        return Teuchos::getValue<Value>(entry(i));
    }

    template<typename Value> void Get(const string& ename, Value& value, const FactoryBase* factory)
    {
        if(!dataTable_.count(ename) > 0)
        {
            std::stringstream str; str << "key" << ename << " does not exist in Hashtable.";
            throw(Exceptions::RuntimeError(str.str()));
        }

        if(!dataTable_[ename].count(factory) > 0)
        {
            std::stringstream str; str << "key " << ename << " generated by " << factory << " does not exist in Hashtable.";
            throw(Exceptions::RuntimeError(str.str()));
        }
        dataMapType& mapData = dataTable_[ename];
        ConstMapIterator i = mapData.find(factory);
        value = Teuchos::getValue<Value>(entry(i));
    }

    template<typename Value> void Get(const string& ename, Value& value, const FactoryBase* factory) const
    {
        if(!dataTable_.count(ename) > 0)
        {
            std::stringstream str; str << "key" << ename << " does not exist in Hashtable.";
            throw(Exceptions::RuntimeError(str.str()));
        }


        ConstIterator k = dataTable_.find(ename);
        dataMapType mapData = k->second;

        if(!mapData.count(factory) > 0)
        {
            std::stringstream str; str << "key " << ename << " generated by " << factory << " does not exist in Hashtable.";
            throw(Exceptions::RuntimeError(str.str()));
        }

        ConstMapIterator i = mapData.find(factory);
        value = Teuchos::getValue<Value>(entry(i));
    }

    void Remove(const string& ename, const FactoryBase* factory)
    {
        if(!dataTable_.count(ename) > 0)
        {
            std::stringstream str; str << "key" << ename << " does not exist in Hashtable.";
            throw(Exceptions::RuntimeError(str.str()));
        }

        if(!dataTable_[ename].count(factory) > 0)
        {
            std::stringstream str; str << "key " << ename << " generated by " << factory << " does not exist in Hashtable.";
            throw(Exceptions::RuntimeError(str.str()));
        }

        if(dataTable_[ename].erase(factory)!=1)
        {
            std::stringstream str; str << "error: could not erase " << ename << "generated gy " << factory;
            throw(Exceptions::RuntimeError(str.str()));
        }

        // check if there exist other instances of 'ename' (generated by other factories than 'factory')
        if(dataTable_.count(ename) == 0)
            dataTable_.erase(ename); // last instance of 'ename' can be removed
    }

    inline std::string GetType(const string& ename, const FactoryBase* factory) const
    {
        if(!dataTable_.count(ename) > 0)
        {
            std::stringstream str; str << "key" << ename << " does not exist in Hashtable.";
            return std::string("requested, but not existing?");
            //throw(Exceptions::RuntimeError(str.str()));
        }

        ConstIterator k = dataTable_.find(ename);
        dataMapType mapData = k->second;

        if(!mapData.count(factory) > 0)
        {
            std::stringstream str; str << "key " << ename << " generated by " << factory << " does not exist in Hashtable.";
            return std::string("requested, but not existing?");
            //throw(Exceptions::RuntimeError(str.str()));
        }

        ConstMapIterator i = mapData.find(factory);
        return entry(i).getAny(true).typeName();
    }

    bool isKey(const string& ename, const FactoryBase* factory)
    {
        // check if ename exists
        if (!dataTable_.count(ename) > 0) return false;

        // resolve factory to a valid factory ptr
        if (dataTable_[ename].count(factory) > 0) return true;

        return false;
    }

    std::vector<string> keys() const
    {
        std::vector<string> v;
        for(ConstIterator it = dataTable_.begin(); it!=dataTable_.end(); ++it)
        {
            v.push_back(it->first);
        }
        return v;
    }

    std::vector<const MueLu::FactoryBase*> handles(const string& ename) const
    {
        if(!dataTable_.count(ename) > 0)
        {
            std::stringstream str; str << "key" << ename << " does not exist in Hashtable.";
            throw(Exceptions::RuntimeError(str.str()));
        }


        std::vector<const MueLu::FactoryBase*> v;

        //const dataMapType& mapData = dataTable_[ename];
        ConstIterator k = dataTable_.find(ename);
        dataMapType mapData = k->second;
         for(ConstMapIterator it = mapData.begin(); it!=mapData.end(); ++it)
        {
            v.push_back(it->first);
        }
        return v;
    }

    void Print(std::ostream &out)
    {
        Teuchos::TabularOutputter outputter(out);
        outputter.pushFieldSpec("name", Teuchos::TabularOutputter::STRING,Teuchos::TabularOutputter::LEFT,Teuchos::TabularOutputter::GENERAL,12);
        outputter.pushFieldSpec("gen. factory addr.", Teuchos::TabularOutputter::STRING,Teuchos::TabularOutputter::LEFT, Teuchos::TabularOutputter::GENERAL, 18);
        outputter.pushFieldSpec("type", Teuchos::TabularOutputter::STRING,Teuchos::TabularOutputter::LEFT, Teuchos::TabularOutputter::GENERAL, 18);
        outputter.outputHeader();

        std::vector<std::string> ekeys = keys();
        for (std::vector<std::string>::iterator it = ekeys.begin(); it != ekeys.end(); it++)
        {
            std::vector<const MueLu::FactoryBase*> ehandles = handles(*it);
            for (std::vector<const MueLu::FactoryBase*>::iterator kt = ehandles.begin(); kt != ehandles.end(); kt++)
            {
                outputter.outputField(*it);
                outputter.outputField(*kt);
                outputter.outputField(GetType(*it,*kt));
                outputter.nextRow();
            }
        }
    }

    //! Return a simple one-line description of this object.
    std::string description() const
    {
        return "ExtendedHashtable";
    }

    //! Print the object with some verbosity level to an FancyOStream object.
    void describe(Teuchos::FancyOStream &out, const Teuchos::EVerbosityLevel verbLevel=Teuchos::Describable::verbLevel_default) const
    {
        if (verbLevel != Teuchos::VERB_NONE)
        {
            out << description() << std::endl;
        }
    }

private:
    //! \name Access to ParameterEntry in dataMap

    //@{

    const Teuchos::ParameterEntry& entry(ConstMapIterator i) const
    {
        return (i->second);
    }

    Teuchos::ParameterEntry& entry(MapIterator i)
    {
        return (i->second);
    }

    /*inline
    Teuchos::ParameterEntry* getEntryPtr(const std::string& ename, const FactoryBase* factory)
    {
        dataMapType& mapData = dataTable_[ename];
        MapIterator i = mapData.find(factory);
        if (i == mapData.end() )
            return NULL;
        return &entry(i);
    }

    inline
    const Teuchos::ParameterEntry* getEntryPtr(const std::string& ename, const FactoryBase* factory) const
    {
        ConstIterator k = dataTable_.find(ename);
        const dataMapType mapData = k->second;
        ConstMapIterator i = mapData.find(factory);
        if (i == mapData.end() )
            return NULL;
        return &entry(i);
    }*/

    //@}

    /*const MueLu::FactoryBase* resolveFactoryPtr(const string& ename, const RCP<const FactoryBase>& fact)
      {
        const FactoryBase* ptrFactory = fact.get(); // this is the memory ptr to the Factory, we are searching for

        // ptrFactory = NULL: no factory at all
        if(ptrFactory==NULL)
          return NULL;

        std::vector<const MueLu::FactoryBase*> ehandles = handles(ename);
        for (std::vector<const MueLu::FactoryBase*>::iterator kt = ehandles.begin(); kt != ehandles.end(); kt++)
        {
          if(*kt==ptrFactory)
          {
            if((*kt)->getID() == fact->getID())
              return *kt;
            else
              throw(Exceptions::RuntimeError("Ooops. Two factories have the same memory address but different ids?"));
          }
        }

        // no corresponding already existing Teuchos::RCP<FactoryBase> found
        // return memory ptr of fact
        return fact.get();
      }*/


private:
    //! data storage object for extended hashtable
    dataTableType dataTable_;

};

} // namespace UTILS
} // namespace MueLu


#endif /* MUELU_EXTENDENDEDHASHTABLE_HPP */
