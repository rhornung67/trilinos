/*------------------------------------------------------------------------*/
/*                 Copyright 2010 Sandia Corporation.                     */
/*  Under terms of Contract DE-AC04-94AL85000, there is a non-exclusive   */
/*  license for use of this work by or on behalf of the U.S. Government.  */
/*  Export of this program may require a license from the                 */
/*  United States Government.                                             */
/*------------------------------------------------------------------------*/


//----------------------------------------------------------------------
#include <stk_mesh/baseImpl/PartImpl.hpp>
#include <iostream>                     // for operator<<, basic_ostream, etc
#include <stk_mesh/base/Part.hpp>       // for insert
#include <stk_mesh/base/Trace.hpp>      // for TraceIfWatching, etc
#include <stk_util/environment/ReportHandler.hpp>  // for ThrowErrorMsgIf
#include "stk_mesh/base/Types.hpp"      // for EntityRank, etc
#include "stk_topology/topology.hpp"    // for topology, operator<<, etc
#include "stk_topology/topology.hpp"    // for topology::rank

#ifdef STK_MESH_TRACE_ENABLED
#include <stk_mesh/base/Entity.hpp>
#endif

namespace stk { namespace mesh { class MetaData; } }

//----------------------------------------------------------------------

namespace stk {
namespace mesh {

namespace impl {

void PartImpl::add_part_to_subset( Part & part)
{
  TraceIfWatching("stk::mesh::impl::PartImpl::add_part_to_subset", LOG_PART, static_cast<size_t>(m_ordinal));
  DiagIfWatching(LOG_PART, static_cast<size_t>(m_ordinal), "New subset is: " << part );

  insert( m_subsets, part );
}

void PartImpl::add_part_to_superset( Part & part )
{
  TraceIfWatching("stk::mesh::impl::PartImpl::add_part_to_superset", LOG_PART, static_cast<size_t>(m_ordinal));
  DiagIfWatching(LOG_PART, static_cast<size_t>(m_ordinal), "New superset is: " << part );

  insert( m_supersets, part );
}

// Subset part constructor:
PartImpl::PartImpl( MetaData          * arg_meta_data,
                    const std::string & arg_name,
                    EntityRank          arg_rank,
                    size_t              arg_ordinal,
                    bool                arg_force_no_induce)
  : m_name( arg_name ),
    m_id( -1 ),
    m_attribute(),
    m_subsets() ,
    m_supersets() ,
    m_mesh_meta_data( arg_meta_data ),
    m_ordinal( arg_ordinal ),
    m_entity_rank( arg_rank ),
    m_topology(stk::topology::INVALID_TOPOLOGY),
    m_force_no_induce(arg_force_no_induce)
{
  TraceIfWatching("stk::mesh::impl::PartImpl::PartImpl", LOG_PART, arg_ordinal);
  DiagIfWatching(LOG_PART, static_cast<size_t>(m_ordinal), "Name is: " << arg_name << ", rank is : " << arg_rank );
}

void PartImpl::set_primary_entity_rank( EntityRank entity_rank )
{
  TraceIfWatching("stk::mesh::impl::PartImpl::set_primary_entity_rank", LOG_PART, static_cast<size_t>(m_ordinal));
  if ( entity_rank == m_entity_rank ) return;

  const bool rank_already_set = m_entity_rank != InvalidEntityRank && entity_rank != m_entity_rank;

//const bool has_subsets = m_subsets.size() > 0;
//ThrowErrorMsgIf( has_subsets, " Error: Part '" << m_name  << "' has subsets");

  if ( entity_rank == InvalidEntityRank ) return;
  ThrowErrorMsgIf( rank_already_set, " Error: Different entity rank has already been set on Part");

  m_entity_rank = entity_rank;
}


//----------------------------------------------------------------------

void PartImpl::set_topology( stk::topology topo )
{
  TraceIfWatching("stk::mesh::impl::PartImpl::set_topology", LOG_PART, static_cast<size_t>(m_ordinal));
  if ( topo == stk::topology::INVALID_TOPOLOGY || topo == m_topology ) return;

  ThrowErrorMsgIf( m_topology != stk::topology::INVALID_TOPOLOGY && m_topology != topo,
      "Error set_topology: part "
      << name()
      << " already defined with "
      << m_topology
      << " conflicts with  "
      << topo
  );

  ThrowErrorMsgIf( static_cast<stk::topology::rank_t>(m_entity_rank) != stk::topology::INVALID_RANK
      && static_cast<stk::topology::rank_t>(m_entity_rank) != topo.rank(),
      "Error set_topology: part "
      << name()
      << " already defined with "
      << static_cast<stk::topology::rank_t>(m_entity_rank)
      << " conflicts with  "
      << topo << " which has rank " << topo.rank()
  );

  m_entity_rank = topo.rank();
  m_topology = topo;
}


//----------------------------------------------------------------------

} // namespace impl

} // namespace mesh
} // namespace stk


