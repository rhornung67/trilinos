/*------------------------------------------------------------------------*/
/*      phdMesh : Parallel Heterogneous Dynamic unstructured Mesh         */
/*                Copyright (2007) Sandia Corporation                     */
/*                                                                        */
/*  Under terms of Contract DE-AC04-94AL85000, there is a non-exclusive   */
/*  license for use of this work by or on behalf of the U.S. Government.  */
/*                                                                        */
/*  This library is free software; you can redistribute it and/or modify  */
/*  it under the terms of the GNU Lesser General Public License as        */
/*  published by the Free Software Foundation; either version 2.1 of the  */
/*  License, or (at your option) any later version.                       */
/*                                                                        */
/*  This library is distributed in the hope that it will be useful,       */
/*  but WITHOUT ANY WARRANTY; without even the implied warranty of        */
/*  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU     */
/*  Lesser General Public License for more details.                       */
/*                                                                        */
/*  You should have received a copy of the GNU Lesser General Public      */
/*  License along with this library; if not, write to the Free Software   */
/*  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307   */
/*  USA                                                                   */
/*------------------------------------------------------------------------*/
/**
 * @author H. Carter Edwards
 */

#ifndef phdmesh_Mesh_hpp
#define phdmesh_Mesh_hpp

//----------------------------------------------------------------------

#include <util/Parallel.hpp>
#include <mesh/Types.hpp>
#include <mesh/Field.hpp>
#include <mesh/Entity.hpp>

//----------------------------------------------------------------------

namespace phdmesh {

void verify_parallel_consistency( const Schema & , ParallelMachine );

//----------------------------------------------------------------------
/** Parallel Heterogeneous Dynamic Mesh.
 *  An dynamic unstructured mesh of mesh entities with
 *  subsets of parts partitioned into homogeneous kernels.
 */

class Mesh {
public:

  ~Mesh();

  /** Construct mesh for the given Schema, parallel machine, and
   *  with the specified maximum number of entities per kernel,
   *  kernel_capacity[ EntityTypeMaximum ].
   */
  Mesh( const Schema & schema , ParallelMachine parallel ,
        const unsigned kernel_capacity[] );

  const Schema & schema() const { return m_schema ; }

  ParallelMachine parallel() const { return m_parallel_machine ; }
  unsigned parallel_size()   const { return m_parallel_size ; }
  unsigned parallel_rank()   const { return m_parallel_rank ; }

  //------------------------------------
  
  /** Rotation of states:
   *    StateNM1 <- StateNew
   *    StateNM2 <- StateNM1
   *    StateNM3 <- StateNM2
   *    StateNM3 <- StateNM2
   *  etc.
   */
  void update_state();

  //------------------------------------
  // Entities can only be created and deleted
  // after the mesh has been committed.

  /** All kernels of a given entity type */
  const KernelSet & kernels( EntityType ) const ;

  /** All entitities of a given entity type */
  const EntitySet & entities( EntityType ) const ;

  Entity * get_entity( unsigned long key ,
                       const char * required_by = NULL ) const ;

  Entity * get_entity( EntityType , unsigned long ,
                       const char * required_by = NULL ) const ;

  //------------------------------------
  /** Create or retrieve entity of the given type and id.
   *  If has different parts then merge the existing parts
   *  and the input parts.
   */
  Entity & declare_entity( EntityType ,
                           unsigned long ,
                           const std::vector<Part*> & ,
                           int owner = -1 );

  Entity & declare_entity( unsigned long key ,
                           const std::vector<Part*> & ,
                           int owner = -1 );

  void change_entity_identifier( Entity & , unsigned long );

  void change_entity_owner( Entity & , unsigned );

  void change_entity_parts( Entity & ,
                            const std::vector<Part*> & add_parts ,
                            const std::vector<Part*> & remove_parts );

  /** Change all entities within the given kernel */
  void change_entity_parts( const Kernel & ,
                            const std::vector<Part*> & add_parts ,
                            const std::vector<Part*> & remove_parts );

  /** Declare a connection and its converse between entities in the same mesh.
   *  The greater entity type 'uses' the lesser entity type.
   *  If 'required_unique_by' is set and declaration is non-unique
   *  then throws an error with the message.
   */
  void declare_connection( Entity & ,
                           Entity & ,
                           const unsigned identifier ,
                           const char * required_unique_by = NULL );

  /** Declare an anonymous connection between entities in the same mesh.
   *  The converse connection is not automatically introduced.
   */
  void declare_connection_anon( Entity & ,
                                Entity & ,
                                const unsigned identifier );

  /** Remove all connections between two entities. */
  void destroy_connection( Entity & , Entity & );

  /** Destroy an entity */
  void destroy_entity( Entity * );

  //------------------------------------
  /** Symmetric parallel connections for shared mesh entities.  */
  const std::vector<EntityProc> & shares() const { return m_shares_all ; }

  /** Asymmetric parallel connections for owner-to-aura mesh entities.
   *  Both the domain and the range are fully ordered.
   */
  const std::vector<EntityProc> & aura_domain() const 
    { return m_aura_domain ; }

  const std::vector<EntityProc> & aura_range() const 
    { return m_aura_range ; }

  /** The input must be symmetric, fully ordered, and contain
   *  every mesh entity with the 'shares_part'.
   */
  void set_shares( const std::vector<EntityProc> & );

  /** The domain and range inputs must be asymmetric, fully ordered,
   *  contain every mesh entity with the 'aura_part'.
   *  Domain entities must be owns and range entries must match
   *  the 'owner_field' value.
   */
  void set_aura( const std::vector<EntityProc> & ,
                 const std::vector<EntityProc> & );

private:

  Mesh();
  Mesh( const Mesh & );
  Mesh & operator = ( const Mesh & );

  const Schema           & m_schema ;
  ParallelMachine          m_parallel_machine ;
  unsigned                 m_parallel_size ;
  unsigned                 m_parallel_rank ;
  unsigned                 m_kernel_capacity[ EntityTypeMaximum ];
  KernelSet                m_kernels[  EntityTypeMaximum ];
  EntitySet                m_entities[ EntityTypeMaximum ];
  std::vector<EntityProc>  m_shares_all ;
  std::vector<EntityProc>  m_aura_domain ;
  std::vector<EntityProc>  m_aura_range ;

  void insert_entity( Entity & , const PartSet & );
  void remove_entity( KernelSet::iterator , unsigned );

  KernelSet::iterator declare_kernel( const EntityType , const PartSet & );
  void                destroy_kernel( KernelSet::iterator );
};

//----------------------------------------------------------------------

void partset_entity_count(
  Mesh & mesh ,
  Part & part ,
  unsigned long * const count /* [ EntityTypeMaximum ] */ );

void partset_entity_count(
  Mesh & mesh ,
  const PartSet & parts ,
  unsigned long * const count /* [ EntityTypeMaximum ] */ );

/** Get all kernels within the given part.
 *  Every kernel will have the part in its superset.
 */
void get_kernels( const KernelSet & , Part & , std::vector<Kernel*> & );

/** Get all kernels within all of the given parts.
 *  The input PartSet must be properly ordered,
 *  e.g. via the 'phdmesh::order( PartSet & )' function.
 *  Every kernel will have all of the parts in its superset.
 *  It is more efficient to pre-define an intersection part
 *  in the schema and then use the get_kernels function.
 */
void get_kernels_intersect( const KernelSet & ,
                            const PartSet & ,
                            std::vector<Kernel*> & );

/** Get all kernels within at least one of the given parts.
 *  Every kernel will have at least one of the parts in its superset.
 *  It is more efficient to pre-define a superset part
 *  in the schema and then use the get_kernels function.
 */
void get_kernels_union( const KernelSet & ,
                        const PartSet & ,
                        std::vector<Kernel*> & );

} // namespace phdmesh

//----------------------------------------------------------------------
//----------------------------------------------------------------------

#endif

