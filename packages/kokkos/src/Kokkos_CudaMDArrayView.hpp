/** \HEADER
 *************************************************************************
 *
 *                            Kokkos
 *                 Copyright 2010 Sandia Corporation
 *
 *  Under the terms of Contract DE-AC04-94AL85000 with Sandia Corporation,
 *  the U.S. Government retains certain rights in this software.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions are
 *  met:
 *
 *  1. Redistributions of source code must retain the above copyright
 *  notice, this list of conditions and the following disclaimer.
 *
 *  2. Redistributions in binary form must reproduce the above copyright
 *  notice, this list of conditions and the following disclaimer in the
 *  documentation and/or other materials provided with the distribution.
 *
 *  3. Neither the name of the Corporation nor the names of the
 *  contributors may be used to endorse or promote products derived from
 *  this software without specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY SANDIA CORPORATION "AS IS" AND ANY
 *  EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 *  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 *  PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL SANDIA CORPORATION OR THE
 *  CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 *  EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 *  PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 *  PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 *  LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 *  NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 *  SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *************************************************************************
 */

#ifndef KOKKOS_CUDAMDARRAYVIEW_HPP
#define KOKKOS_CUDAMDARRAYVIEW_HPP

#include <Kokkos_CudaDevice.hpp>
#include <Kokkos_CudaMap.hpp>

namespace Kokkos {

template< typename ValueType , class DeviceMapType > class MDArrayView ;

//----------------------------------------------------------------------------
//----------------------------------------------------------------------------

/** \brief  Multidimensional array allocated and mapped
 *          onto a compute device.
 *
 *  The array is a simple rank-N container of simple scalar values
 *  where 1 <= N <= 8.
 *
 *  The first N-1 ranks corresponds to a block of scalars.
 *  The last rank corresponds to the parallel work index.
 *
 *  No assumptions should be made as to the mapping, contiguity, or strides
 *  of the storage of these arrays.  The mapping will vary according to the
 *  underlying device.  The usage model is for algorithms to be parameterized
 *  with respect to the type of the mapped array and thus achieve portability
 *  across compute devices.
 */

template< typename ValueType >
class MDArrayView< ValueType , CudaMap > {
public:
  typedef CudaMap                        device_map_type ;
  typedef typename CudaMap::device_type  device_type ;
  typedef typename CudaMap::size_type    size_type ;
  typedef ValueType                      value_type ;

  /*------------------------------------------------------------------*/
  /** \brief  Query rank of the array */
  KOKKOS_DEVICE_AND_HOST_FUNCTION
  size_type rank() const { return m_data.m_rank ; }

  /** \brief  Query dimension of the given ordinate of the array */
  template < typename iType >
  KOKKOS_DEVICE_AND_HOST_FUNCTION
  size_type dimension( const iType & rank_ordinate ) const
    {
      KOKKOS_MDARRAY_CHECK( m_data.require_in_rank(rank_ordinate) );
      return m_data.m_dimension[rank_ordinate] ;
    }

  /*------------------------------------------------------------------*/
  /** \brief  Query value of a rank 8 array */
  template< typename iType0 , typename iType1 ,
            typename iType2 , typename iType3 ,
            typename iType4 , typename iType5 ,
            typename iType6 , typename iTypeP >
  KOKKOS_DEVICE_FUNCTION
  value_type & operator()( const iType0 & i0 , const iType1 & i1 ,
                           const iType2 & i2 , const iType3 & i3 ,
                           const iType4 & i4 , const iType5 & i5 ,
                           const iType6 & i6 , const iTypeP & iP ) const
    {
      KOKKOS_MDARRAY_CHECK( m_data.require_in_bounds(i0,i1,i2,i3,i4,i5,i6,iP) );

      return ( (value_type*) m_data.m_ptr_on_device )
             [ ( iP + m_data.m_dimension[7] *
               ( i0 + m_data.m_dimension[0] * ( i1 + m_data.m_dimension[1] *
               ( i2 + m_data.m_dimension[2] * ( i3 + m_data.m_dimension[3] *
               ( i4 + m_data.m_dimension[4] * ( i5 + m_data.m_dimension[5] *
               ( i6 )))))))) ];
    }

  /** \brief  Query value of a rank 7 array */
  template< typename iType0 , typename iType1 ,
            typename iType2 , typename iType3 ,
            typename iType4 , typename iType5 ,
            typename iTypeP >
  KOKKOS_DEVICE_FUNCTION
  value_type & operator()( const iType0 & i0 , const iType1 & i1 ,
                           const iType2 & i2 , const iType3 & i3 ,
                           const iType4 & i4 , const iType5 & i5 ,
                           const iTypeP & iP ) const
    {
      KOKKOS_MDARRAY_CHECK( m_data.require_in_bounds(i0,i1,i2,i3,i4,i5,iP) );

      return ( (value_type*) m_data.m_ptr_on_device )
             [ ( iP + m_data.m_dimension[6] *
               ( i0 + m_data.m_dimension[0] * ( i1 + m_data.m_dimension[1] *
               ( i2 + m_data.m_dimension[2] * ( i3 + m_data.m_dimension[3] *
               ( i4 + m_data.m_dimension[4] * ( i5 ))))))) ];
    }

  /** \brief  Query value of a rank 6 array */
  template< typename iType0 , typename iType1 ,
            typename iType2 , typename iType3 ,
            typename iType4 , typename iTypeP >
  KOKKOS_DEVICE_FUNCTION
  value_type & operator()( const iType0 & i0 , const iType1 & i1 ,
                           const iType2 & i2 , const iType3 & i3 ,
                           const iType4 & i4 , const iTypeP & iP ) const
    {
      KOKKOS_MDARRAY_CHECK( m_data.require_in_bounds(i0,i1,i2,i3,i4,iP) );

      return ( (value_type*) m_data.m_ptr_on_device )
             [ ( iP + m_data.m_dimension[5] *
               ( i0 + m_data.m_dimension[0] * ( i1 + m_data.m_dimension[1] *
               ( i2 + m_data.m_dimension[2] * ( i3 + m_data.m_dimension[3] *
               ( i4 )))))) ];
    }

  /** \brief  Query value of a rank 5 array */
  template< typename iType0 , typename iType1 ,
            typename iType2 , typename iType3 ,
            typename iTypeP >
  KOKKOS_DEVICE_FUNCTION
  value_type & operator()( const iType0 & i0 , const iType1 & i1 ,
                           const iType2 & i2 , const iType3 & i3 ,
                           const iTypeP & iP ) const
    {
      KOKKOS_MDARRAY_CHECK( m_data.require_in_bounds(i0,i1,i2,i3,iP) );

      return ( (value_type*) m_data.m_ptr_on_device )
             [ ( iP + m_data.m_dimension[4] *
               ( i0 + m_data.m_dimension[0] * ( i1 + m_data.m_dimension[1] *
               ( i2 + m_data.m_dimension[2] * ( i3 ))))) ];
    }

  /** \brief  Query value of a rank 4 array */
  template< typename iType0 , typename iType1 ,
            typename iType2 , typename iTypeP >
  KOKKOS_DEVICE_FUNCTION
  value_type & operator()( const iType0 & i0 , const iType1 & i1 ,
                           const iType2 & i2 , const iTypeP & iP ) const
    {
      KOKKOS_MDARRAY_CHECK( m_data.require_in_bounds(i0,i1,i2,iP) );

      return ( (value_type*) m_data.m_ptr_on_device )
             [ ( iP + m_data.m_dimension[3] *
               ( i0 + m_data.m_dimension[0] * ( i1 + m_data.m_dimension[1] *
               ( i2 )))) ];
    }

  /** \brief  Query value of a rank 3 array */
  template< typename iType0 , typename iType1 ,
            typename iTypeP >
  KOKKOS_DEVICE_FUNCTION
  value_type & operator()( const iType0 & i0 , const iType1 & i1 ,
                           const iTypeP & iP ) const
    {
      KOKKOS_MDARRAY_CHECK( m_data.require_in_bounds(i0,i1,iP) );

      return ( (value_type*) m_data.m_ptr_on_device )
             [ ( iP + m_data.m_dimension[2] *
               ( i0 + m_data.m_dimension[0] * ( i1 ))) ];
    }

  /** \brief  Query value of a rank 2 array */
  template< typename iType0 , typename iTypeP >
  KOKKOS_DEVICE_FUNCTION
  value_type & operator()( const iType0 & i0 , const iTypeP & iP ) const
    {
      KOKKOS_MDARRAY_CHECK( m_data.require_in_bounds(i0,iP) );

      return ( (value_type*) m_data.m_ptr_on_device )
             [ ( iP + m_data.m_dimension[1] * ( i0 )) ];
    }

  /** \brief  Query value of a rank 1 array */
  template< typename iTypeP >
  KOKKOS_DEVICE_FUNCTION
  value_type & operator()( const iTypeP & iP ) const
    {
      KOKKOS_MDARRAY_CHECK( m_data.require_in_bounds(iP) );

      return ( (value_type*) m_data.m_ptr_on_device )
             [ ( iP ) ];
    }

  /*------------------------------------------------------------------*/
  /** \brief  Construct a NULL view */
  inline
  MDArrayView() : m_data() {}

  /** \brief  Construct a view of the array */
  inline
  MDArrayView( const MDArrayView & rhs ) : m_data( rhs.m_data ) {}

  /** \brief  Assign to a view of the rhs array.
   *          If the old view is the last view
   *          then allocated memory is deallocated.
   */
  inline
  MDArrayView & operator = ( const MDArrayView & rhs )
    { m_data.operator=( rhs.m_data ); return *this ; }
  
  /**  \brief  Destroy this view of the array.
   *           If the last view then allocated memory is deallocated.
   */
  inline
  ~MDArrayView() {}

  void clear_view()
    { m_data.clear_view(); }

  /*------------------------------------------------------------------*/

private:

  friend class CudaMap ;

  typedef MDArrayViewRawData<value_type,CudaDevice> raw_data_type ;

  MDArrayView( raw_data_type & rhs ) : m_data( rhs ) {}

  /** \brief  Separate view and data management details from
   *          multi-index mapping details.
   */
  raw_data_type m_data ;
};

//----------------------------------------------------------------------------
//----------------------------------------------------------------------------

} // namespace Kokkos

#endif /* KOKKOS_CUDAMDARRAYVIEW_HPP */


