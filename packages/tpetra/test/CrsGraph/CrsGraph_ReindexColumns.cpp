/*
// @HEADER
// ***********************************************************************
//
//          Tpetra: Templated Linear Algebra Services Package
//                 Copyright (2008) Sandia Corporation
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
// Questions? Contact Michael A. Heroux (maherou@sandia.gov)
//
// ************************************************************************
// @HEADER
*/

// Ensure that if CUDA and KokkosCompat are enabled, then
// only the .cu version of this file is actually compiled.
#include <Tpetra_config.h>
#ifdef HAVE_TPETRA_KOKKOSCOMPAT
#  include <KokkosCore_config.h>
#  ifdef KOKKOS_USE_CUDA_BUILD
#    define DO_COMPILATION
#  else
#    ifndef KOKKOS_HAVE_CUDA
#      define DO_COMPILATION
#    endif // NOT KOKKOS_HAVE_CUDA
#  endif // KOKKOS_USE_CUDA_BUILD
#else
#  define DO_COMPILATION
#endif // HAVE_TPETRA_KOKKOSCOMPAT

#ifdef DO_COMPILATION

#include <Tpetra_CrsGraph.hpp>
#include <Tpetra_TestingUtilities.hpp>

namespace {
  //using Teuchos::broadcast;
  //using std::endl;

  using Tpetra::TestingUtilities::getNode;
  using Tpetra::TestingUtilities::getDefaultComm;
  using Teuchos::Array;
  using Teuchos::ArrayView;
  using Teuchos::Comm;
  using Teuchos::outArg;
  using Teuchos::ParameterList;
  using Teuchos::parameterList;
  using Teuchos::RCP;
  using Teuchos::rcp;
  using Teuchos::REDUCE_MIN;
  using Teuchos::reduceAll;

  //
  // UNIT TESTS
  //

  // Test CrsGraph::reindexColumns, with a local-only change in column
  // indices.  "Local-only" means that on all processes in the graph's
  // communicator, the graph's column indices in the original column
  // Map are also owned in the new column Map.  Thus, reindexColumns()
  // need not call getRemoteIndexList() in order for this test to
  // pass.  In order to test reindexColumns() completely, we also need
  // a "global" test, that requires a remote index lookup.
  TEUCHOS_UNIT_TEST_TEMPLATE_3_DECL( CrsGraph, ReindexColumns_Local, LO, GO, Node )
  {
    typedef Tpetra::global_size_t GST;
    typedef Tpetra::CrsGraph<LO, GO, Node> graph_type;
    // typedef Tpetra::Import<LO, GO, Node> import_type;
    typedef Tpetra::Map<LO, GO, Node> map_type;
    typedef typename Array<GO>::size_type size_type;

    const GST INVALID = Teuchos::OrdinalTraits<GST>::invalid ();
    int gblSuccess = 0;
    int lclSuccess = 1;

    RCP<const Comm<int> > comm = getDefaultComm ();
    RCP<Node> node = getNode<Node> ();
    const int numProcs = comm->getSize ();

    // Create the graph's row Map.
    // const size_t numLocalIndices = 5;
    const GST numGlobalIndices = static_cast<GST> (5 * numProcs);
    const GO indexBase = 0;
    RCP<const map_type> rowMap =
      rcp (new map_type (numGlobalIndices, indexBase, comm,
                         Tpetra::GloballyDistributed, node));
    TEUCHOS_TEST_FOR_EXCEPTION(
      ! rowMap->isContiguous (), std::logic_error, "The row Map is supposed "
      "to be contiguous, but is not.");

    const size_t maxNumEntPerRow = 3;

    graph_type graph (rowMap, maxNumEntPerRow, Tpetra::StaticProfile);

    // Make the usual tridiagonal graph.  Let the graph create its own
    // column Map.  We'll use that column Map to create a new column
    // Map, and give that column Map to reindexColumns().

    if (rowMap->getNodeNumElements () > 0) {
      const GO myMinGblInd = rowMap->getMinGlobalIndex ();
      const GO myMaxGblInd = rowMap->getMaxGlobalIndex ();
      const GO gblMinGblInd = rowMap->getMinAllGlobalIndex ();
      const GO gblMaxGblInd = rowMap->getMaxAllGlobalIndex ();

      Array<GO> gblInds (maxNumEntPerRow);
      for (GO gblRowInd = myMinGblInd; gblRowInd <= myMaxGblInd; ++gblRowInd) {
        size_t numInds = 0;
        if (gblRowInd == gblMinGblInd) {
          if (gblRowInd < gblMaxGblInd) {
            numInds = 2;
            gblInds[0] = gblRowInd;
            gblInds[1] = gblRowInd + 1;
          } else { // special case of 1 x 1 graph
            numInds = 1;
            gblInds[0] = gblRowInd;
          }
        } else if (gblRowInd == gblMaxGblInd) {
          if (gblRowInd > gblMinGblInd) {
            numInds = 2;
            gblInds[0] = gblRowInd - 1;
            gblInds[1] = gblRowInd;
          } else { // special case of 1 x 1 graph
            numInds = 1;
            gblInds[0] = gblRowInd;
          }
        } else {
          numInds = 3;
          gblInds[0] = gblRowInd - 1;
          gblInds[1] = gblRowInd;
          gblInds[2] = gblRowInd + 1;
        }
        ArrayView<const GO> gblIndsView = gblInds (0, numInds);
        graph.insertGlobalIndices (gblRowInd, gblIndsView);
      }
    }
    graph.fillComplete ();

    TEUCHOS_TEST_FOR_EXCEPTION(
      ! graph.isFillComplete (), std::logic_error, "The graph claims that it "
      "is not fill complete, after fillComplete was called.");
    TEUCHOS_TEST_FOR_EXCEPTION(
      ! graph.hasColMap () || graph.getColMap ().is_null (), std::logic_error,
      "The graph is fill complete, but doesn't have a column Map.");

    // Make a deep copy of the graph, to check later that the
    // conversion was correct.
    RCP<graph_type> graph2;
    {
      RCP<ParameterList> clonePlist = parameterList ("Tpetra::CrsGraph::clone");
      clonePlist->set ("Debug", true);
      graph2 = graph.clone (node, clonePlist);
    }

    gblSuccess = 0;
    lclSuccess = success ? 1 : 0;
    reduceAll<int, int> (*comm, REDUCE_MIN, lclSuccess, outArg (gblSuccess));
    TEST_EQUALITY_CONST( gblSuccess, 1 );

    // Create a new column Map, which has all the global indices on
    // each process (_locally_) in reverse order of the graph's
    // current column Map.

    const map_type& curColMap = * (graph.getColMap ());
    Array<GO> newGblInds (curColMap.getNodeNumElements ());
    if (curColMap.isContiguous ()) {
      // const GO myMinGblInd = curColMap.getMinGlobalIndex ();
      const GO myMaxGblInd = curColMap.getMaxGlobalIndex ();

      const size_type myNumInds =
        static_cast<size_type> (curColMap.getNodeNumElements ());
      if (myNumInds > 0) {
        GO curGblInd = myMaxGblInd;
        for (size_type k = 0; k < myNumInds; ++k, --curGblInd) {
          newGblInds[k] = curGblInd;
        }
      }
    } else {
      ArrayView<const GO> curGblInds = curColMap.getNodeElementList ();
      for (size_type k = 0; k < curGblInds.size (); ++k) {
        const size_type k_opposite = (newGblInds.size () - 1) - k;
        newGblInds[k] = curGblInds[k_opposite];
      }
    }

    RCP<const map_type> newColMap =
      rcp (new map_type (INVALID, newGblInds (), indexBase, comm, node));

    gblSuccess = 0;
    lclSuccess = success ? 1 : 0;
    reduceAll<int, int> (*comm, REDUCE_MIN, lclSuccess, outArg (gblSuccess));
    TEST_EQUALITY_CONST( gblSuccess, 1 );

    // Call the reindexColumns() method: the moment of truth!
    TEST_NOTHROW( graph.reindexColumns (newColMap) );

    // Does the graph now have the right column Map?
    TEST_ASSERT( ! graph.getColMap ().is_null () );
    // FIXME (mfh 18 Aug 2014) Some of these tests may hang if the
    // graph's column Map is null on some, but not all processes.
    if (! graph.getColMap ().is_null ()) {
      TEST_ASSERT( graph.getColMap ()->isSameAs (*newColMap) );
    }

    TEST_ASSERT( ! graph.getImporter ().is_null () );
    // FIXME (mfh 18 Aug 2014) Some of these tests may hang if the
    // graph's Import object is null on some, but not all processes.
    if (! graph.getImporter ().is_null ()) {
      TEST_ASSERT( ! graph.getImporter ()->getSourceMap ().is_null () &&
                   graph.getImporter ()->getSourceMap ()->isSameAs (* (graph.getDomainMap ())) );
      TEST_ASSERT( ! graph.getImporter ()->getTargetMap ().is_null () &&
                   graph.getImporter ()->getTargetMap ()->isSameAs (* newColMap) );
    }

    gblSuccess = 0;
    lclSuccess = success ? 1 : 0;
    reduceAll<int, int> (*comm, REDUCE_MIN, lclSuccess, outArg (gblSuccess));
    TEST_EQUALITY_CONST( gblSuccess, 1 );

    // Check that all the graph's indices are correct.  We know that
    // this is a local-only column Map change, so we don't have to
    // call getRemoteIndexList to do this; just convert the local
    // indices to global in the new column Map, and then back to local
    // in the old column Map, and compare with those in the original
    // graph.
    const LO myNumRows = static_cast<LO> (rowMap->getNodeNumElements ());
    if (myNumRows > 0) {
      Array<LO> lclColIndsTmp (graph.getNodeMaxNumRowEntries ());
      for (LO lclRowInd = 0; lclRowInd < myNumRows; ++lclRowInd) {
        ArrayView<const LO> lclColInds;
        graph.getLocalRowView (lclRowInd, lclColInds);

        for (size_type k = 0; k < lclColInds.size (); ++k) {
          const GO gblColInd = newColMap->getGlobalElement (lclColInds[k]);
          lclColIndsTmp[k] = curColMap.getLocalElement (gblColInd);
        }
        ArrayView<const LO> newLclColInds = lclColIndsTmp (0, lclColInds.size ());

        ArrayView<const LO> oldLclColInds;
        graph2->getLocalRowView (lclRowInd, oldLclColInds);

        TEST_EQUALITY( newLclColInds.size (), oldLclColInds.size () );
        TEST_COMPARE_ARRAYS( newLclColInds, oldLclColInds );
      }
    }

    gblSuccess = 0;
    lclSuccess = success ? 1 : 0;
    reduceAll<int, int> (*comm, REDUCE_MIN, lclSuccess, outArg (gblSuccess));
    TEST_EQUALITY_CONST( gblSuccess, 1 );
  }

//
// INSTANTIATIONS
//

// Tests to build and run in both debug and release modes.  We will
// instantiate them over all enabled local ordinal (LO), global
// ordinal (GO), and Kokkos Node (NODE) types.
#define UNIT_TEST_GROUP( LO, GO, NODE ) \
  TEUCHOS_UNIT_TEST_TEMPLATE_3_INSTANT( CrsGraph, ReindexColumns_Local, LO, GO, NODE )


  TPETRA_ETI_MANGLING_TYPEDEFS()

  TPETRA_INSTANTIATE_LGN( UNIT_TEST_GROUP )

}

#endif // DO_COMPILATION

