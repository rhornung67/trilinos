//@HEADER
// ************************************************************************
// 
//               Tpetra: Linear Algebra Services Package 
//                 Copyright 2011 Sandia Corporation
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
//@HEADER

#ifndef __MatrixMarket_Tpetra_hpp
#define __MatrixMarket_Tpetra_hpp

#include "Tpetra_CrsMatrix.hpp"
#include "MatrixMarket_raw.hpp"
#include "MatrixMarket_Banner.hpp"
#include "MatrixMarket_CoordDataReader.hpp"
#include "MatrixMarket_util.hpp"
#include "Teuchos_StandardCatchMacros.hpp"

#include <algorithm>
#include <fstream>
#include <iostream>
#include <iterator>
#include <vector>
#include <stdexcept>

namespace Tpetra {
  namespace MatrixMarket {

    /// \class Reader
    /// \brief Matrix Market file reader for Tpetra::CrsMatrix.
    ///
    /// The readFile() and read() class methods read in a Matrix
    /// Market "coordinate" format sparse matrix file resp. input
    /// stream (valid on MPI Rank 0), and return a Tpetra::CrsMatrix
    /// sparse matrix (SparseMatrixType) distributed in block row
    /// fashion over all the MPI ranks represented in the given
    /// communicator.
    template<class SparseMatrixType>
    class Reader {
    public:
      typedef SparseMatrixType sparse_matrix_type;
      typedef Teuchos::RCP<sparse_matrix_type> sparse_matrix_ptr;
      /// \typedef scalar_type
      /// \brief Type of the entries of the sparse matrix.
      typedef typename SparseMatrixType::scalar_type scalar_type;
      /// \typedef local_ordinal_type
      /// \brief Only used to define map_type.
      typedef typename SparseMatrixType::local_ordinal_type local_ordinal_type;
      /// \typedef global_ordinal_type
      /// \brief Type of indices as read from the Matrix Market file.
      ///
      /// Indices of the sparse matrix are read in as global ordinals,
      /// since Matrix Market files represent the whole matrix and
      /// don't have a notion of distribution.
      typedef typename SparseMatrixType::global_ordinal_type global_ordinal_type;
      typedef typename SparseMatrixType::node_type node_type;

      typedef Teuchos::RCP<const node_type> node_ptr;
      typedef Teuchos::Comm<int> comm_type;
      typedef Teuchos::RCP<const comm_type> comm_ptr;
      typedef Map<local_ordinal_type, global_ordinal_type, node_type> map_type;
      typedef Teuchos::RCP<const map_type> map_ptr;

    private:

      /// \typedef size_type
      /// \brief Handy typedef for entries of arrays such as rowPtr.
      typedef typename ArrayRCP<global_ordinal_type>::size_type size_type;

      /// \brief Compute initial row map, or verify an existing one.
      ///
      /// The typical case when reading a sparse matrix from a file is to
      /// create a new row map.  However, we also give the option to use an
      /// existing row map, if you are already using a particular
      /// distribution for (say) vector data and don't want to stop using
      /// it.  In the latter case (pRowMap is not null), we validate the
      /// dimension, communicator, and node of the existing row map that you
      /// pass in.  In either case, you need to know the (global) number of
      /// rows in the matrix.
      ///
      /// \param pRowMap [in] If non-null, test the row map for validity,
      ///   and return it.  Otherwise, if null, initialize and return a
      ///   reasonable row map.  "Validity" includes that the number of
      ///   elements is numRows, and the map's communicator and node are the
      ///   same as the corresponding arguments.  The typical case is to
      ///   pass in null here, which is why we call this routine
      ///   "makeRowMap".
      ///
      /// \param pComm [in] Global communicator.
      ///
      /// \param pNode [in] Kokkos Node object.
      ///
      /// \param numRows [in] Global number of rows in the matrix.  If
      ///   pRowMap is nonnull, used only for error checking.
      ///
      /// \return If pRowMap is null, a new row map, otherwise pRowMap.
      ///
      static Teuchos::RCP<const map_type>
      makeRowMap (const Teuchos::RCP<const map_type>& pRowMap, 
		  const Teuchos::RCP<const comm_type>& pComm,
		  const Teuchos::RCP<const node_type>& pNode,
		  const global_ordinal_type numRows)
      {
	using Teuchos::rcp;

	// If the caller didn't provide a map, return a conventional,
	// uniformly partitioned, contiguous map.
	if (pRowMap.is_null())
	  return rcp (new map_type (static_cast<global_size_t> (numRows), 
				    static_cast<global_ordinal_type> (0),
				    pComm, GloballyDistributed, pNode));
	else 
	  {
	    const global_size_t globalNumElts = pRowMap->getGlobalNumElements();
	    TEST_FOR_EXCEPTION(globalNumElts != static_cast<global_size_t> (numRows),
			       std::invalid_argument,
			       "The specified row map should have " << numRows 
			       << " elements, but has " << globalNumElts 
			       << " elements instead.");
	    TEST_FOR_EXCEPTION(! pRowMap->isDistributed() && pComm->getSize() > 1, 
			       std::invalid_argument,
			       "The specified row map is not distributed, but the "
			       "given communicator includes > 1 ranks.");
	    TEST_FOR_EXCEPTION(pRowMap->getComm() != pComm, 
			       std::invalid_argument,
			       "The specified row map's communicator (pRowMap->"
			       "getComm()) is different than the given separately "
			       "supplied communicator pComm.");
	    TEST_FOR_EXCEPTION(pRowMap->getNode() != pNode,
			       std::invalid_argument,
			       "The specified row map's node (pRowMap->getNode()) "
			       "is different than the given separately supplied "
			       "node pNode.");
	    return pRowMap;
	  }
      }

      /// \brief Compute initial domain map.
      /// 
      /// \param pRowMap [in] Valid row / range map of the matrix, 
      ///   as returned by \c makeRowMap().
      /// \param numRows [in] Global number of rows in the matrix.
      /// \param numCols [in] Global number of columns in the matrix.
      ///
      /// \return The domain map.  If numRows == numCols, this is 
      ///   identical to the row / range map, otherwise we make a
      ///   new map for the domain.
      static map_ptr
      makeDomainMap (const map_ptr& pRowMap,
		     const global_ordinal_type numRows,
		     const global_ordinal_type numCols)
      {
	typedef local_ordinal_type LO;
	typedef global_ordinal_type GO;
	typedef node_type Node;

	if (numRows == numCols) 
	  return pRowMap;
	else
	  {
	    comm_ptr pComm = pRowMap->getComm();
	    node_ptr pNode = pRowMap->getNode();
	    return createUniformContigMapWithNode<LO,GO,Node> (numCols, pComm, pNode);
	  }
      }

      /// \brief Distribute the sparse matrix entries.
      ///
      /// This is one of those routines that just has to be messy.  We try
      /// to comment everything so you can see what's going on.  All of the
      /// matrix data starts out on Rank 0, stored in CSR format (rowPtr,
      /// colInd, values).  numEntriesPerRow applies to this data; it can be
      /// computed from rowPtr, and has one less entry than rowPtr.  Rank 0
      /// then engages in a dialog with Ranks 1 .. numProcs-1 to determine
      /// which part of the matrix data is theirs, and to send them their
      /// data.
      ///
      /// \param myNumEntriesPerRow [out] For my row indices, the number of
      ///   entries per row.  This array has
      ///   pRowMap->getNodeElementList().size() entries, and is indexed in
      ///   the same way, so that myNumEntriesPerRow[k] is the number of
      ///   entries in row pRowMap->getNodeElementList()[k].
      ///
      /// \param myColInd [out] My rows' column indices.  If myRows =
      ///   pRowMap->getNodeElementList(), start = sum(myNumEntriesPerRow[0
      ///   .. k-1]), and end = start + myNumEntriesPerRow[k], then 
      ///   myColInd[start .. end-1] are the column indices for myRows[k].
      ///
      /// \param myValues [out] My rows' stored matrix values.  If myRows =
      ///   pRowMap->getNodeElementList(), start = sum(myNumEntriesPerRow[0
      ///   .. k-1]), and end = start + myNumEntriesPerRow[k], then 
      ///   myValues[start .. end-1] are the column indices for myRows[k].
      ///
      /// \param pRowMap [in] Map describing the distribution of rows among
      ///   processors.  This must be a 1-1 map.
      ///
      /// \param numEntriesPerRow [in/out] For all row indices, the number
      ///   of entries per row.  You can construct this from the usual CSR
      ///   matrix "rowPtr" array by differences: numEntriesPerRow[k] =
      ///   rowPtr[k+1] - rowPtr[k].  As a result, numEntriesPerRow has one
      ///   less entry than rowPtr.  On output, the reference is invalidated
      ///   to save space.
      ///
      /// \param rowPtr [in/out] On input: the usual CSR matrix row pointer
      ///   array for the whole matrix.  On output, the reference is
      ///   invalidated to save space.
      ///
      /// \param colInd [in/out] On input: all rows' column indices.  If
      ///   start = sum(numEntriesPerRow[0 .. k-1]), and end = start +
      ///   numEntriesPerRow[k], then colInd[start .. end-1] are the column
      ///   indices for row k.  On output, the reference is invalidated to
      ///   save space.
      ///
      /// \param values [in/out] On input: all rows' stored matrix values.
      ///   If start = sum(numEntriesPerRow[0 .. k-1]), and end = start +
      ///   numEntriesPerRow[k], then values[start .. end-1] are the values
      ///   for row k.  On output, the reference is invalidated to save
      ///   space.
      /// 
      void
      distribute (ArrayRCP<size_type>& myNumEntriesPerRow,
		  ArrayRCP<global_ordinal_type>& myColInd,
		  ArrayRCP<Scalar>& myValues,
		  const Teuchos::RCP<const map_type>& pRowMap,
		  ArrayRCP<size_type>& numEntriesPerRow,
		  ArrayRCP<size_type>& rowPtr,
		  ArrayRCP<global_ordinal_type>& colInd,
		  ArrayRCP<Scalar>& values)
      {
	using Teuchos::arcp;
	using Teuchos::ArrayRCP;
	using Teuchos::Array;
	using Teuchos::ArrayView;
	using Teuchos::CommRequest;
	using Teuchos::Comm;
	using Teuchos::RCP;
	using Teuchos::rcpFromRef;
	using Teuchos::receive;
	using Teuchos::send;

	comm_ptr pComm = pRowMap->getComm();
	const int numProcs = Teuchos::size (*pComm);
	const int rootRank = 0;

	// List of the global indices of my rows.
	// They may or may not be contiguous.
	ArrayView<const global_ordinal_type> myRows = 
	  pRowMap->getNodeElementList();
	const size_type myNumRows = myRows.size();

	// Space for my proc's number of entries per row.  
	// Will be filled in below.
	myNumEntriesPerRow = arcp<size_type> (myNumRows);

	// Teuchos::receive() returns an int; here is space for it.
	int recvResult = 0;

	if (myRank != rootRank)
	  {
	    // Tell the root how many rows we have.  If we're sending none,
	    // then we don't have anything else to send, nor does the root
	    // have to receive anything else.
	    send (*pComm, myNumRows, rootRank);
	    if (myNumRows == 0)
	      continue; 

	    // Now send my rows' global indices.
	    send (*pComm, myNumRows, myRows.getRawPtr(), rootRank);

	    // I (this proc) don't care if my global row indices are
	    // contiguous, though the root proc does (since otherwise it
	    // needs to pack noncontiguous data into contiguous storage
	    // before sending).  That's why we don't check for
	    // contiguousness here.

	    // Ask the root processor for my part of the array of the
	    // number of entries per row.
	    recvResult = receive (*pComm, rootRank, myNumRows,
				  myNumEntriesPerRow.getRawPtr());

	    // Use the resulting array to figure out how many column
	    // indices and values for which I should ask from the root
	    // processor.  
	    const size_type myNumEntries = 
	      std::accumulate (myNumEntriesPerRow.begin(), 
			       myNumEntriesPerRow.end(), 
			       static_cast<size_type> (0));
	    // Make space for my entries of the sparse matrix.  Note that
	    // they don't have to be sorted by row index.  Iterating through
	    // all my rows requires computing a running sum over
	    // myNumEntriesPerRow.
	    myColInd = arcp<size_type> (myNumEntries);
	    myValues = arcp<scalar_type> (myNumEntries);
	    if (myNumEntries > 0)
	      { // Ask for that many column indices and values, if there are any.
		recvResult = receive (*pComm, rootRank, myNumEntries, 
				      myColInd.getRawPtr());
		recvResult = receive (*pComm, rootRank, myNumEntries, 
				      myValues.getRawPtr());
	      }
	  }
	else 
	  {
	    // Proc 0 still needs to (allocate, if not done already) and
	    // fill its part of the matrix (my*).
	    for (size_type k = 0; k < myNumRows; ++k)
	      myNumEntriesPerRow[k] = numEntriesPerRow[myRows[k]];
	    const size_type myNumEntries = 
	      std::accumulate (myNumEntriesPerRow.begin(), 
			       myNumEntriesPerRow.end(), 
			       static_cast<size_type> (0));
	    myColInd = arcp<size_type> (myNumEntries);
	    myValues = arcp<size_type> (myNumEntries);
	    // Copy Proc 0's part of the matrix into the my* arrays.
	    size_type curPos = 0;
	    for (size_type k = 0; k < myNumRows; ++k, curPos += myNumEntriesPerRow[k])
	      {
		ArrayView<global_ordinal_type> colIndView = 
		  colInd(myRows[k], myNumEntriesPerRow[k]);
		ArrayView<global_ordinal_type> myColIndView = 
		  myColInd(curPos, myNumEntriesPerRow[k]);
		std::copy (colIndView.begin(), colIndView.end(), myColInd.begin());

		ArrayView<scalar_type> valuesView = 
		  values(myRows[k], myNumEntriesPerRow[k]);
		ArrayView<scalar_type> myValuesView = 
		  myValues(curPos, myNumEntriesPerRow[k]);
		std::copy (valuesView.begin(), valuesView.end(), myValues.begin());
	      }

	    // Proc 0 just finished with its own rows above, so we count
	    // those as "done."  Now Proc 0 has to finish the rows belonging
	    // to all the other procs.
	    size_type numRowsDone = myNumRows;

	    // Proc 0 processes each other proc p in turn.
	    for (int p = 1; p < numProcs; ++p)
	      {
		size_type theirNumRows = 0;
		// Ask Proc p how many rows it has.  If it doesn't have any,
		// we can move on to the next proc.  This has to be a
		// standard receive so that we can avoid the degenerate case
		// of sending zero data.
		recvResult = receive (*pComm, p, &theirNumRows);
		if (theirNumRows == 0)
		  continue;

		// Ask Proc p which rows it owns.  The resulting global row
		// indices are not guaranteed to be contiguous or sorted.
		// Global row indices are themselves indices into the
		// numEntriesPerRow array.
		theirRows.resize (theirNumRows);
		recvResult = receive (*pComm, p, numTheirRows, theirRows.getRawPtr());

		// Check if Proc p's rows are contiguous.  If so, we can
		// extract Proc p's data easily.  Otherwise we have to do a
		// little more work.
		bool contiguous = true;
		for (size_type k = 1; k < theirRows.size(); ++k)
		  {
		    if (theirRows[k-1] + 1 != theirRows[k])
		      contiguous = false;
		  }

		ArrayRCP<size_type> theirNumEntriesPerRow;
		if (contiguous)
		  theirNumEntriesPerRow = 
		    numEntriesPerRow.persistingView (numRowsDone, theirNumRows);
		else
		  {
		    // Collect the number of entries in each of Proc p's rows.
		    theirNumEntriesPerRow = arcp<size_type> (theirNumRows);
		    for (size_type k = 0; k < theirNumRows; ++k)
		      theirNumEntriesPerRow[k] = numEntriesPerRow[theirRows[k]];
		  }
		// Tell Proc p the number of entries in each of its rows.
		send (*pComm, theirNumRows, theirNumEntriesPerRow.getRawPtr(), p);

		// Figure out how many entries Proc p owns.
		const size_type theirNumEntries = 
		  std::accumulate (theirNumEntriesPerRow.begin(),
				   theirNumEntriesPerRow.end(), 
				   static_cast<size_type> (0));
		if (theirNumEntries == 0)
		  continue; // No entries to send, so we're done with proc p

		// Construct (views of) proc p's column indices and values.
		ArrayRCP<global_ordinal_type> theirColInd;
		ArrayRCP<scalar_type> theirValues;
		if (contiguous)
		  { // Current position in which to look in colInd and values for
		    // Proc p's entries, assuming contiguous storage.
		    const size_type rowStart = rowPtr[numRowsDone];
		    theirColInd = colInd.persistingView(rowStart, theirNumEntries);
		    theirValues = values.persistingView(rowStart, theirNumEntries);
		  }
		else
		  {
		    theirColInd = arcp<global_ordinal_type> (theirNumEntries);
		    theirValues = arcp<scalar_type> (theirNumEntries);
		    size_type curPos = 0;
		    for (size_type k = 0; k < theirNumRows; 
			 ++k, curPos += theirNumEntriesPerRow[k])
		      {
			const size_type rowStart = rowPtr[theirRows[k]];

			ArrayView<global_ordinal_type> colIndView = 
			  colInd(rowStart, theirNumEntriesPerRow[k]);
			ArrayView<global_ordinal_type> theirColIndView = 
			  theirColInd(curPos, theirNumEntriesPerRow[k]);
			std::copy (colIndView.begin(), colIndView.end(), 
				   theirColInd.begin());

			ArrayView<scalar_type> valuesView = 
			  values(rowStart, theirNumEntriesPerRow[k]);
			ArrayView<scalar_type> theirValuesView = 
			  theirValues(curPos, theirNumEntriesPerRow[k]);
			std::copy (valuesView.begin(), valuesView.end(), 
				   theirValues.begin());
		      }
		  }

		// Send Proc p its column indices and values.
		send (*pComm, theirNumEntries, theirColInd.getRawPtr(), p);
		send (*pComm, theirNumEntries, theirValues.getRawPtr(), p);
		numRowsDone += theirNumRows;
	      } // For each proc p not the root proc 0
	  } // If I'm (not) the root proc 0

	// Invalidate the input data to save space, since we don't
	// need it anymore.
	numEntriesPerRow = null;
	rowPtr = null;
	colInd = null;
	values = null;
      }

      /// \brief Given my proc's data, return the completed sparse matrix.
      /// 
      /// Each proc inserts its data into the sparse matrix, and then
      /// all procs call fillComplete().
      sparse_matrix_ptr
      makeMatrix (Teuchos::ArrayRCP<typename Teuchos::ArrayRCP<global_ordinal_type>::size_type>& myNumEntriesPerRow,
		  Teuchos::ArrayRCP<typename Teuchos::ArrayRCP<global_ordinal_type>::size_type>& myRowPtr,
		  Teuchos::ArrayRCP<global_ordinal_type>& myColInd,
		  Teuchos::ArrayRCP<scalar_type>& myValues,
		  const map_ptr& pRowMap,
		  const map_ptr& pDomMap)
      {
	using Teuchos::ArrayRCP;
	using Teuchos::ArrayView;
	using Teuchos::rcp;
	typedef global_ordinal_type GO;
	typedef typename Teuchos::ArrayRCP<global_ordinal_type>::size_type size_type;

	// Create with DynamicProfile, so that the
	// fillComplete(DoOptimizeStorage) can do first-touch reallocation.
	sparse_matrix_ptr A = rcp (new sparse_matrix_type (pRowMap, myNumEntriesPerRow, DynamicProfile));

	// List of the global indices of my rows.
	// They may or may not be contiguous.
	ArrayView<const GO> myRows = pRowMap->getNodeElementList();
	const size_type myNumRows = myRows.size();

	for (size_type k = 0; k < myNumRows; ++k)
	  A->insertGlobalValues (myRows[k], 
				 myColInd(myRowPtr[k], myNumEntriesPerRow[k]),
				 myValues(myRowPtr[k], myNumEntriesPerRow[k]));
	// We've entered in all our matrix entries, so we can delete the
	// original data.  This will save memory when we call
	// fillComplete().
	myNumEntriesPerRow = null;
	myRowPtr = null;
	myColInd = null;
	myValues = null;

	A->fillComplete (pDomMap, pRowMap, DoOptimizeStorage);
	return A;
      }

    private:

      /// \brief Read in the Banner line from the given input stream.
      ///
      /// \param in [in/out, valid only on Rank 0] Input stream from 
      ///   which to read the Banner line.
      /// \param lineNumber [in/out, valid only on Rank 0] On input:
      ///   Current line number of the input stream.  On output: if 
      ///   any line(s) were successfully read from the input stream,
      ///   this is incremented by the number of line(s) read.  (This
      ///   includes comment lines.)
      /// \param pComm [in, global] Communicator.
      ///
      /// \return Banner [non-null and valid only on Rank 0]
      static Teuchos::RCP<const Banner>
      readBanner (std::istream& in,
		  size_t& lineNumber,
		  const Teuchos::RCP<const Teuchos::Comm<int> >& pComm,
		  const bool tolerant=false,
		  const bool debug=false)
      {
	using Teuchos::RCP;
	using Teuchos::rcp;
	const int myRank = Teuchos::rank (*pComm);

	// The pointer will be non-null on return only on MPI Rank 0.
	// Using a pointer lets the data persist outside the
	// "myRank==0" scopes.
	RCP<Banner> pBanner;
	if (myRank == 0)
	  {
	    typedef Teuchos::ScalarTraits<scalar_type> STS;

	    // Keep reading lines until we get a noncomment line.
	    std::string line;
	    size_t numLinesRead = 0;
	    bool commentLine = false;
	    do {
	      // Try to read a line from the input stream.
	      const bool readFailed = ! getline(in, line);
	      TEST_FOR_EXCEPTION(readFailed, std::invalid_argument,
				 "Failed to get Matrix Market banner line "
				 "from input, after reading " << numLinesRead
				 << "line" << (numLinesRead != 1 ? "s." : "."));
	      // We read a line from the input stream.
	      lineNumber++; 
	      numLinesRead++;
	      size_t start, size; // Output args of checkCommentLine
	      commentLine = checkCommentLine (line, start, size, 
					      lineNumber, tolerant);
	    } while (commentLine); // Loop until we find a noncomment line.

	    // Assume that the noncomment line we found is the banner line.
	    try {
	      pBanner = rcp (new Banner (line, tolerant));
	    } catch (std::exception& e) {
	      TEST_FOR_EXCEPTION(true, std::invalid_argument, 
				 "Matrix Market banner line contains syntax "
				 "error(s): " << e.what());
	    }
	    // Perform further validation for the special case of
	    // reading in a sparse matrix with entries of type
	    // scalar_type.
	    if (pBanner->matrixType() != "coordinate")
	      throw std::invalid_argument ("Matrix Market input file must contain a "
					   "\"coordinate\"-format sparse matrix in "
					   "order to create a Tpetra::CrsMatrix "
					   "object from it.");
	    else if (! STS::isComplex && pBanner->dataType() == "complex")
	      throw std::invalid_argument ("Matrix Market file contains complex-"
					   "valued data, but your chosen scalar "
					   "type is real.");
	    else if (pBanner->dataType() != "real" && pBanner->dataType() != "complex")
	      throw std::invalid_argument ("Only real or complex data types (no "
					   "pattern or integer matrices) are "
					   "currently supported");
	  }
	return pBanner;
      }

      /// \brief Read sparse matrix dimensions on Rank 0, and broadcast.
      ///
      /// Call on all MPI ranks.  MPI Rank 0 attempts to read in the
      /// coordinate dimensions from the input stream.  If it
      /// succeeds, it broadcasts them to all the other MPI ranks.
      /// (All ranks need to know the matrix dimensions in order to
      /// create domain, range, and column Maps.)
      ///
      /// \return (numRows, numCols, numNonzeros)
      static Teuchos::Tuple<global_ordinal_type, 3>
      readCoordDims (std::istream& in,
		     size_t& lineNumber,
		     const Teuchos::RCP<const Banner>& pBanner,
		     const comm_ptr& pComm,
		     const bool tolerant = false) // ignored except on Rank 0
      {
	// Packed coordinate matrix dimensions (numRows, numCols,
	// numNonzeros); computed on Rank 0 and broadcasted to all MPI
	// ranks.
	Teuchos::Tuple<global_ordinal_type, 3> dims;

	// Read in the coordinate matrix dimensions from the input
	// stream.  "success" tells us whether reading in the
	// coordinate matrix dimensions succeeded ("Guilty unless
	// proven innocent").
	bool success = false;
	if (Teuchos::rank(*pComm) == 0)
	  { 
	    TEST_FOR_EXCEPTION(pBanner->matrixType() != "coordinate", 
			       std::invalid_argument,
			       "The Tpetra::CrsMatrix Matrix Market reader "
			       "only accepts \"coordinate\" (sparse) matrix "
			       "data.");
	    // Unpacked coordinate matrix dimensions
	    global_ordinal_type numRows, numCols, numNonzeros;
	    // Only MPI Rank 0 reads from the input stream
	    success = readCoordinateDimensions (in, numRows, numCols, 
						numNonzeros, lineNumber, 
						tolerant);
	    // Pack up the data into a Tuple so we can send them with
	    // one broadcast instead of three.
	    dims[0] = numRows;
	    dims[1] = numCols;
	    dims[2] = numNonzeros;
	  }
	// Only Rank 0 did the reading, so it decides success.
	//
	// FIXME (mfh 02 Feb 2011) Teuchos::broadcast doesn't know how
	// to send bools.  For now, we convert to/from int instead,
	// using the usual "true is 1, false is 0" encoding.
	{
	  int the_success = success ? 1 : 0; // only matters on MPI Rank 0
	  Teuchos::broadcast (*pComm, 0, &the_success);
	  success = (the_success == 1);
	}
	if (success)
	  // Broadcast (numRows, numCols, numNonzeros) from Rank 0
	  // to all the other MPI ranks.  
	  Teuchos::broadcast (*pComm, 0, dims);
	else
	  // Perhaps in tolerant mode, we could set all the
	  // dimensions to zero for now, and deduce correct
	  // dimensions by reading all of the file's entries and
	  // computing the max(row index) and max(column index).
	  // However, for now we just error out in that case.
	  throw std::invalid_argument ("Error reading Matrix Market sparse "
				       "matrix: failed to read coordinate "
				       "matrix dimensions.");
	return dims;
      }
      
      /// \typedef adder_type
      /// \brief Type of object that "adds" entries to the sparse matrix.
      ///
      /// "Adds" here means that it collects and makes note of matrix
      /// entries read in from the input stream.  This object doesn't
      /// call insertGlobalEntries() or fillComplete() on the
      /// CrsMatrix.  Depending on the Matrix Market banner line
      /// information, it may "symmetrize" the matrix by adding entry
      /// A(j,i) (with the appropriate value depending on the symmetry
      /// type) if entry A(i,j) is seen.
      typedef SymmetrizingAdder<Raw::Adder<scalar_type, global_ordinal_type> > adder_type;

      /// \brief Make an "adder" object for processing matrix data.
      ///
      /// \param pComm [in] Communicator (across whose MPI ranks 
      ///   the sparse matrix will be distributed)
      ///
      /// \param banner [in, nonnull and valid on Rank 0 only]
      ///   Object describing the type and symmetry of matrix data.
      ///
      /// \param dims [in] (numRows, numCols, numEntries).  These are
      ///   the "expected" values as read from the top of the Matrix
      ///   Market input stream.  Whether they are the final values
      ///   depends on the "tolerant" parameter and the actual matrix
      ///   data read from the input stream.
      ///
      /// \param tolerant [in] Whether the adder should be "tolerant"
      ///   of syntax errors and missing/incorrect metadata.  (In
      ///   particular, this refers to the number of rows, columns,
      ///   and entries in the matrix.)
      /// \param debug [in] Whether to print verbose debug output
      ///   to stderr.
      ///
      /// \return An adder_type object [nonnull and valid on Rank 0
      ///   only] that optionally symmetrizes the entries of the
      ///   sparse matrix.
      ///
      static Teuchos::RCP<adder_type>
      makeAdder (const Teuchos::RCP<const Teuchos::Comm<int> >& pComm,
		 Teuchos::RCP<const Banner>& pBanner, 
		 const Teuchos::Tuple<global_ordinal_type, 3>& dims,
		 const bool tolerant=false,
		 const bool debug=false)
      {
	if (Teuchos::rank (*pComm) == 0)
	  {
	    using Teuchos::RCP;
	    using Teuchos::rcp;
	    typedef Raw::Adder<scalar_type, global_ordinal_type> raw_adder_type;

	    RCP<raw_adder_type> pRaw (new raw_adder_type (dims[0], dims[1], 
							  dims[2], tolerant, 
							  debug));
	    return rcp (new adder_type (pRaw, pBanner->symmType()));
	  }
	else
	  return Teuchos::null;
      }

    public:

      static Teuchos::RCP<sparse_matrix_type>
      readFile (const std::string& filename,
		const Teuchos::RCP<const Teuchos::Comm<int> >& pComm, 
		const Teuchos::RCP<node_type>& pNode,
		const bool tolerant=false,
		const bool debug=false)
      {
	std::ifstream in (filename.c_str());
	return read (in, pComm, pNode, tolerant, debug);
      }
      
      static Teuchos::RCP<sparse_matrix_type>
      read (std::istream& in,	
	    const Teuchos::RCP<const Teuchos::Comm<int> >& pComm, 
	    const Teuchos::RCP<node_type>& pNode,
	    const bool tolerant=false,
	    const bool debug=false)
      {
	using Teuchos::RCP;
	using Teuchos::rcp;
	using Teuchos::Tuple;
	using Teuchos::null;
	using std::cerr;
	using std::endl;

	typedef Teuchos::ScalarTraits<scalar_type> STS;
	const int myRank = Teuchos::rank (*pComm);

	// Current line number in the input stream.  Various calls
	// will modify this depending on the number of lines that are
	// read from the input stream.
	size_t lineNumber = 1; 	

	// The "Banner" tells you whether the input stream represents
	// a sparse matrix, the symmetry type of the matrix, and the
	// type of the data it contains.
	RCP<const Banner> pBanner = readBanner (in, lineNumber, pComm, tolerant);

	// dims = (numRows, numCols, numEntries) (all global),
	// as read from the Matrix Market metadata.
	Tuple<global_ordinal_type, 3> dims = 
	  readCoordDims (in, lineNumber, pBanner, pComm, tolerant);

	// "Adder" object for collecting all the sparse matrix entries
	// from the input stream.
	RCP<adder_type> pAdder = 
	  makeAdder (pComm, pBanner, dims, tolerant, debug);

	// Read the sparse matrix entries from the input stream.
	//
	// "Guilty until proven innocent."
	bool readSuccess = false;
	if (myRank == 0)
	  {
	    // Reader for "coordinate" format sparse matrix data.
	    typedef CoordDataReader<adder_type, global_ordinal_type, 
	      scalar_type, STS::isComplex> reader_type;
	    reader_type reader (*pAdder);

	    // Read the sparse matrix entries.
	    //
	    // FIXME (mfh 28 Mar 2011) We should catch exceptions
	    // here, and broadcast any error messages to all
	    // processors so that the exception can be rethrown
	    // everywhere (fail fast and hard, rather than hoping that
	    // MPI propagates the exceptions quickly).
	    std::pair<bool, std::vector<size_t> > results = 
	      reader.read (in, lineNumber, tolerant, debug);

	    readSuccess = results.first;
	  }
	// The broadcast of readSuccess from MPI Rank 0 serves as a
	// barrier.  Note that Tpetra::CrsMatrix::fillComplete() only
	// starts with a barrier in debug mode, so we need some kind
	// of barrier or synchronization beforehand.
	//
	// Currently, Teuchos::broadcast doesn't know how to send
	// bools.  For now, we convert to/from int instead, using the
	// usual "true is 1, false is 0" encoding.
	{
	  int the_readSuccess = readSuccess ? 1 : 0; // only matters on MPI Rank 0
	  Teuchos::broadcast (*pComm, 0, &the_readSuccess);
	  readSuccess = (the_readSuccess == 1);
	}
	// TODO (mfh 01 Feb 2011)
	//
	// In tolerant mode, given a "verbose" flag, report / log any
	// bad line number(s) on MPI Rank 0.
	//
	// Should we run fillComplete() in tolerant mode, if the read
	// did not succeed?
	TEST_FOR_EXCEPTION(! readSuccess, std::invalid_argument, 
			   "Failed to read in the Matrix Market sparse matrix.");

	// In tolerant mode, we need to rebroadcast the matrix
	// dimensions, since they may be different after reading the
	// actual matrix data.  We only need to broadcast the number
	// of rows and columns.  Only Rank 0 needs to know the actual
	// global number of entries, since (a) we need to merge
	// duplicates on Rank 0 first anyway, and (b) when we
	// distribute the entries, each rank other than Rank 0 will
	// only need to know how many entries it owns, not the total
	// number of entries.
	if (tolerant)
	  {
	    // Packed coordinate matrix dimensions (numRows, numCols).
	    Teuchos::Tuple<global_ordinal_type, 2> updatedDims;
	    if (myRank == 0)
	      {
		updatedDims[0] = pAdder->getAdder()->numRows();
		updatedDims[1] = pAdder->getAdder()->numCols();
	      }
	    Teuchos::broadcast (*pComm, 0, updatedDims);
	    dims[0] = updatedDims[0];
	    dims[1] = updatedDims[1];
	  }

	// Now that we've read in all the matrix entries from the
	// input stream into the adder on Rank 0, post-process them
	// into CSR format (still on Rank 0).  This will facilitate
	// distributing them to all the processors.
	//
	// These arrays represent the global matrix data as a CSR
	// matrix (with numEntriesPerRow as redundant but convenient
	// metadata, since it's computable from rowPtr and vice
	// versa).  They are valid only on Rank 0.
	ArrayRCP<size_type> numEntriesPerRow;
	ArrayRCP<size_type> rowPtr;
	ArrayRCP<global_ordinal_type> colInd;
	ArrayRCP<scalar_type> values;

	// Rank 0 first merges duplicate entries, and then converts
	// the coordinate-format matrix data to CSR.
	if (myRank == 0)
	  {
	    typedef Element<scalar_type, global_ordinal_type> element_type;
	    typedef std::vector<element_type>::const_iterator iter_type;

	    // Additively merge duplicate matrix entries.
	    pAdder->getAdder()->merge ();
	    // Get a temporary const view of the merged matrix entries.
	    const std::vector<element_type>& entries = 
	      pAdder->getAdder()->getEntries();
	    // Number of rows in the matrix.
	    const size_type numRows = pAdder->getAdder()->numRows();
	    // Number of matrix entries (after merging).
	    const size_type numEntries = entries.size();

	    // Make space for the CSR matrix data.
	    numEntriesPerRow = arcp<size_type> (numRows);
	    rowPtr = arcp<size_type> (numRows+1);	    
	    colInd = arcp<global_ordinal_type> (numEntries);	    
	    values = arcp<scalar_type> (numEntries);	    

	    if (entries.size() > 0)
	      {
		size_type numUniqueRowsSeen = 0;
		size_type curPos = 0;
		global_ordinal_type curRow = 
		  Teuchos::OrdinalTraits<global_ordinal_type>::invalid();

		for (size_type curPos = 0; curPos < numEntries; ++curPos)
		  {
		    const element_type& curEntry = entries[curPos];

		    if (curEntry.rowIndex() != curRow)
		      { // We're starting a new row.
			rowPtr[numUniqueRowsSeen] = curPos;
			numEntriesPerRow[numUniqueRowsSeen] = 0;
			numUniqueRowsSeen++;
			curRow = curEntry.rowIndex();
		      }
		    else
		      { // We're continuing the same row.
			numEntriesPerRow[numUniqueRowsSeen]++;
		      }
		    colInd[curPos] = curEntry.colIndex();
		    values[curPos] = curEntry.value();
		  }
	      }
	    // The last entry of rowPtr is the number of entries
	    // in colInd and values.
	    rowPtr[numRows] = numEntries;
	  }
	// Now we're done with the Adder, so we can release the
	// reference ("free" it) to save space.
	pAdder = null;

	// Make the maps that describe the distribution of the
	// matrix's range and domain.
	//
	// Row map and range map are identical; they are both 1-1.
	map_ptr pRowMap = makeRowMap (null, pComm, pNode, dims[0]);
	// Domain map.
	map_ptr pDomMap = makeDomainMap (pRowMap, dims[0], dims[1]);

	// Distribute the matrix data.  Each processor has to add the
	// rows that it owns.  If you try to make Rank 0 call
	// insertGlobalValues() for _all_ the rows, not just those it
	// owns, then fillComplete() will compute the number of
	// columns incorrectly.  That's why Rank 0 has to distribute
	// the matrix data and why we make all the processors (not
	// just Rank 0) call insertGlobalValues() on their own data.
	//
	// These arrays represent each processor's part of the matrix
	// data, in "CSR" format (sort of, since the row indices might
	// not be contiguous).
	ArrayRCP<size_type> myNumEntriesPerRow;
	ArrayRCP<global_ordinal_type> myColInd;
	ArrayRCP<scalar_type> myValues;
	// Distribute the matrix data.
	distribute (myNumEntriesPerRow, myColInd, myValues, 
		    pRowMap, numEntriesPerRow, rowPtr, colInd, values);

	// Each processor inserts its part of the matrix data, and
	// then they all call fillComplete().  This method invalidates
	// the my* distributed matrix data before calling
	// fillComplete(), in order to save space.  In general, we
	// never store more than two copies of the matrix's entries in
	// memory at once, which is no worse than what Tpetra
	// promises.
	sparse_matrix_ptr pMatrix = 
	  makeMatrix (myNumEntriesPerRow, myRowPtr, myColInd, myValues,
		      pRowMap, pDomMap);
	if (debug)
	  {
	    using Tpetra::global_size_t;
	    
	    // You're not supposed to call these until after
	    // fillComplete() has been called, which is why we wait
	    // until after completeMatrix() (which calls
	    // fillComplete()) has been called.
	    const global_size_t globalNumRows = pMatrix->getGlobalNumRows();
	    const global_size_t globalNumCols = pMatrix->getGlobalNumCols();

	    if (myRank == 0)
	      {
		cerr << "-- Matrix is "
		     << pMatrix->getGlobalNumRows() 
		     << " x " 
		     << pMatrix->getGlobalNumCols()
		     << " with " 
		     << pMatrix->getGlobalNumEntries()
		     << " entries, and index base " 
		     << pMatrix->getIndexBase()
		     << "." << endl;
	      }
	    Teuchos::barrier (*pComm);
	    for (int p = 0; p < Teuchos::size (*pComm); ++p)
	      {
		if (myRank == p)
		  {
		    cerr << "-- Proc " << p << " owns " 
			 << pMatrix->getNodeNumCols() 
			 << " columns, and " 
			 << pMatrix->getNodeNumEntries()
			 << " entries." << endl;
		  }
		Teuchos::barrier (*pComm);
	      }
	  } // if (debug)

	// Casting a positive signed integer (global_ordinal_type)
	// to an unsigned integer of no fewer bits (global_size_t)
	// shouldn't overflow.
	TEST_FOR_EXCEPTION(globalNumRows != static_cast<global_size_t>(dims[0]) || 
			   globalNumCols != static_cast<global_size_t>(dims[1]),
			   std::logic_error, 
			   "The newly created Tpetra::CrsMatrix claims it is " 
			   << globalNumRows << " x " << globalNumCols << ", "
			   "but the data in the Matrix Market input stream "
			   "says the matrix is " << dims[0] << " x "
			   << dims[1] << ".  Please report this bug to the "
			   "Tpetra developers.");
	return pMatrix;
      }
    };
    
  } // namespace MatrixMarket
} // namespace Tpetra

#endif // __MatrixMarket_Tpetra_hpp
