/*--------------------------------------------------------------------*/
/*    Copyright 2014 Sandia Corporation.                              */
/*    Under the terms of Contract DE-AC04-94AL85000, there is a       */
/*    non-exclusive license for use of this work by or on behalf      */
/*    of the U.S. Government.  Export of this program may require     */
/*    a license from the United States Government.                    */
/*--------------------------------------------------------------------*/

#ifndef STK_IO_MeshField_h
#define STK_IO_MeshField_h

#include <string>
#include "stk_mesh/base/Types.hpp"
#include "stk_mesh/base/Part.hpp"  

namespace Ioss {
  class GroupingEntity;
  class Region;
}

namespace stk {
  namespace mesh {
    class FieldBase;
  }
  
  namespace io {
    class DBStepTimeInterval;
    
    class MeshFieldPart {
    public:
      MeshFieldPart(stk::mesh::EntityRank rank, Ioss::GroupingEntity *io_entity, const std::string db_field_name)
	: m_rank(rank), m_ioEntity(io_entity), m_dbName(db_field_name),
	  m_preStep(0), m_postStep(0)
      {}

      void get_interpolated_field_data(const DBStepTimeInterval &sti, std::vector<double> &values);
      void release_field_data();
      
      stk::mesh::EntityRank get_entity_rank() const {return m_rank;}
      Ioss::GroupingEntity* get_io_entity() const {return m_ioEntity;}

    private:
      void load_field_data(const DBStepTimeInterval &sti);

      stk::mesh::EntityRank m_rank;
      Ioss::GroupingEntity *m_ioEntity;
      std::string m_dbName;
      std::vector<double> m_preData;
      std::vector<double> m_postData;
      size_t m_preStep;
      size_t m_postStep;
    };
    
    class MeshField
    {
    public:

      // Options:
      // * Frequency:
      //   -- one time only
      //   -- multiple times
      // * Matching of time
      //   -- Linear Interpolation
      //   -- Closest
      //   -- specified time
      
      enum TimeMatchOption {
	LINEAR_INTERPOLATION,
	CLOSEST,
	SPECIFIED }; // Use time specified on MeshField

      MeshField();

      // Read 'db_name' field data into 'field' using 'tmo' (default CLOSEST) time on database.
      // Analysis time will be mapped to db time.
      MeshField(stk::mesh::FieldBase *field,
		const std::string &db_name="",
		TimeMatchOption tmo = CLOSEST);
      MeshField(stk::mesh::FieldBase &field,
		const std::string &db_name="",
		TimeMatchOption tmo = CLOSEST);

      ~MeshField();

      // MeshField(const MeshField&); Default version is good.
      // MeshField& operator=(const MeshField&); Default version is good

      MeshField& set_read_time(double time_to_read);
      MeshField& set_active();
      MeshField& set_inactive();
      MeshField& set_single_state(bool yesno);
      MeshField& set_read_once(bool yesno);
      
      bool is_active() const {return m_isActive;}
      
      void restore_field_data(stk::mesh::BulkData &bulk,
			      const DBStepTimeInterval &sti);
      
      const std::string &db_name() const {return m_dbName;}
      stk::mesh::FieldBase *field() const {return m_field;}
	
      void add_part(const stk::mesh::EntityRank rank,
		    Ioss::GroupingEntity *io_entity);
      
    private:
      std::vector<MeshFieldPart> m_fieldParts;
      
      stk::mesh::FieldBase *m_field;
      std::string m_dbName; ///<  Name of the field on the input/output database.

      double m_timeToRead;
      
      TimeMatchOption m_timeMatch;
      bool m_oneTimeOnly;
      bool m_singleState;
      bool m_isActive;
    };
  } 
} 
#endif
