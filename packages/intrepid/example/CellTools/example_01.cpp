// @HEADER
// ************************************************************************
//
//                           Intrepid Package
//                 Copyright (2007) Sandia Corporation
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
// Questions? Contact Pavel Bochev (pbboche@sandia.gov) or
//                    Denis Ridzal (dridzal@sandia.gov).
//
// ************************************************************************
// @HEADER


/** \file
\brief  Example of the CellTools class.
\author Created by P. Bochev and D. Ridzal
*/
#include "Intrepid_FieldContainer.hpp"
#include "Intrepid_CellTools.hpp"
#include "Intrepid_RealSpaceTools.hpp"
#include "Shards_CellTopology.hpp"

using namespace std;
using namespace Intrepid;
using namespace shards;

int main(int argc, char *argv[]) {
  std::cout \
  << "===============================================================================\n" \
  << "|                                                                             |\n" \
  << "|                   Example use of the CellTools class                        |\n" \
  << "|                                                                             |\n" \
  << "|     1) Using shards::CellTopology to get cell types and topology            |\n" \
  << "|     2) Using CellTools to get cell Jacobians and their inverses and dets    |\n" \
  << "|     3) Testing points for inclusion in reference and physical cells         |\n" \
  << "|     4) Mapping points to and from reference cells                           |\n" \
  << "|                                                                             |\n" \
  << "|  Questions? Contact  Pavel Bochev (pbboche@sandia.gov) or                   |\n" \
  << "|                      Denis Ridzal (dridzal@sandia.gov).                     |\n" \
  << "|                                                                             |\n" \
  << "|  Intrepid's website: http://trilinos.sandia.gov/packages/intrepid           |\n" \
  << "|  Shards's website:   http://trilinos.sandia.gov/packages/shards             |\n" \
  << "|  Trilinos website:   http://trilinos.sandia.gov                             |\n" \
  << "|                                                                             |\n" \
  << "===============================================================================\n"\
  << "| EXAMPLE 1: Query of cell types and topology                                 |\n"\
  << "===============================================================================\n";
  
  
  typedef CellTools<double>       CellTools;
  typedef RealSpaceTools<double>  RealSpaceTools;
  typedef shards::CellTopology    CellTopology;
  
  
  std::vector<CellTopology> allShardsTopologies;
  CellTools::getShardsTopologies(allShardsTopologies);
  
  std::cout << " Number of Shards cell topologies = " << allShardsTopologies.size() << "\n";
  
  for(unsigned i = 0; i < allShardsTopologies.size(); i++){
    std::cout << allShardsTopologies[i] << "\n"; 
  }
  
  

cout \
<< "===============================================================================\n"\
<< "| EXAMPLE 2: Using CellTools to get Jacobian, Jacobian inverse & Jacobian det |\n"\
<< "===============================================================================\n";

// 4 triangles with basic triangle topology: number of nodes = number of vertices
CellTopology triangle_3(shards::getCellTopologyData<Triangle<3> >() );
int numCells = 4;
int numNodes = triangle_3.getNodeCount();
int spaceDim = triangle_3.getDimension();

// Rank-3 array with dimensions (C,N,D) for the node coordinates of 3 traingle cells
FieldContainer<double> triNodes(numCells, numNodes, spaceDim);

// Initialize node data: accessor is (cellOrd, nodeOrd, coordinateOrd)
triNodes(0, 0, 0) = 0.0;  triNodes(0, 0, 1) = 0.0;    // 1st triangle =  the reference tri
triNodes(0, 1, 0) = 1.0;  triNodes(0, 1, 1) = 0.0;
triNodes(0, 2, 0) = 0.0;  triNodes(0, 2, 1) = 1.0;

triNodes(1, 0, 0) = 1.0;  triNodes(1, 0, 1) = 1.0;    // 2nd triangle = 1st shifted by (1,1)
triNodes(1, 1, 0) = 2.0;  triNodes(1, 1, 1) = 1.0;
triNodes(1, 2, 0) = 1.0;  triNodes(1, 2, 1) = 2.0;

triNodes(2, 0, 0) = 0.0;  triNodes(2, 0, 1) = 0.0;    // 3rd triangle = flip 1st vertical
triNodes(2, 1, 0) =-1.0;  triNodes(2, 1, 1) = 0.0;
triNodes(2, 2, 0) = 0.0;  triNodes(2, 2, 1) = 1.0;

triNodes(3, 0, 0) = 2.0;  triNodes(3, 0, 1) = 1.0;    // 4th triangle = just a triangle
triNodes(3, 1, 0) = 3.0;  triNodes(3, 1, 1) = 0.5;
triNodes(3, 2, 0) = 3.5;  triNodes(3, 2, 1) = 2.0;


// Rank-2 array with dimensions (P,D) for some points on the reference triangle
int numRefPoints = 2;
FieldContainer<double> refPoints(numRefPoints, spaceDim);
refPoints(0,0) = 0.0;   refPoints(0,1) = 0.0;
refPoints(1,0) = 0.5;   refPoints(1,1) = 0.5;


// Rank-4 array (C,P,D,D) for the Jacobian and its inverse and Rank-2 array (C,P) for its determinant
FieldContainer<double> triJacobian(numCells, numRefPoints, spaceDim, spaceDim);
FieldContainer<double> triJacobInv(numCells, numRefPoints, spaceDim, spaceDim);
FieldContainer<double> triJacobDet(numCells, numRefPoints);

// Rank-4 and Rank-2 auxiliary arrays
FieldContainer<double> rank4Aux (numCells, numRefPoints, spaceDim, spaceDim);
FieldContainer<double> rank2Aux (numCells, numRefPoints);


// Methods to compute cell Jacobians, their inverses and their determinants
CellTools::setJacobian(triJacobian, refPoints, triNodes, triangle_3); 
CellTools::setJacobianInv(triJacobInv, triJacobian );
CellTools::setJacobianDet(triJacobDet, triJacobian );

// Checks: compute det(Inv(DF)) and Inv(Inv(DF))
RealSpaceTools::det(rank2Aux, triJacobInv);
RealSpaceTools::inverse(rank4Aux, triJacobInv);

// Print data
std::cout 
<< std::scientific<< std::setprecision(4)
<< std::right << std::setw(16) << "DF(P)" 
<< std::right << std::setw(30) << "Inv(DF(P))"
<< std::right << std::setw(30) << "Inv(Inv(DF(P)))\n";

for(int cellOrd = 0; cellOrd < numCells; cellOrd++){
  std::cout 
  << "===============================================================================\n"
  << "Cell " << cellOrd << "\n";
  for(int pointOrd = 0; pointOrd < numRefPoints; pointOrd++){
    std::cout << "Point = ("
    << std::setw(4) << std::right << refPoints(pointOrd, 0) << ","<< std::setw(4) << std::right<< refPoints(pointOrd,1) << ")\n";
    for(int row = 0; row < spaceDim; row++){
      std::cout 
      << std::setw(11) << std::right << triJacobian(cellOrd, pointOrd, row, 0) << " "
      << std::setw(11) << std::right << triJacobian(cellOrd, pointOrd, row, 1) 
      //
      << std::setw(16) << std::right << triJacobInv(cellOrd, pointOrd, row, 0) << " "
      << std::setw(11) << std::right << triJacobInv(cellOrd, pointOrd, row, 1)
      //
      << std::setw(16) << std::right << rank4Aux(cellOrd, pointOrd, row, 0) << " "
      << std::setw(11) << std::right << rank4Aux(cellOrd, pointOrd, row, 1) << "\n";
    }
    std::cout 
      << setw(5)<<std::left<< "Determinant:\n"
      << std::setw(11) << std::right << triJacobDet(cellOrd, pointOrd)
      << std::setw(28) << std::right << rank2Aux(cellOrd, pointOrd)
      << std::setw(28) << std::right << " product = " << triJacobDet(cellOrd, pointOrd)*rank2Aux(cellOrd, pointOrd);
    std::cout<< "\n\n";
  }
}


// 2 Quadrilateral cells with base topology 
CellTopology quad_4(shards::getCellTopologyData<Quadrilateral<4> >() );
numCells = 2;
numNodes = quad_4.getNodeCount();
spaceDim = quad_4.getDimension();

FieldContainer<double> quadNodes(numCells, numNodes, spaceDim);
// 1st QUAD
quadNodes(0,0,0) = 1.00;  quadNodes(0,0,1) = 1.00;               
quadNodes(0,1,0) = 2.00;  quadNodes(0,1,1) = 0.75;
quadNodes(0,2,0) = 1.75;  quadNodes(0,2,1) = 2.00;  
quadNodes(0,3,0) = 1.25;  quadNodes(0,3,1) = 2.00, 
// 2ND QUAD
quadNodes(1,0,0) = 2.00;  quadNodes(1,0,1) = 0.75;               
quadNodes(1,1,0) = 3.00;  quadNodes(1,1,1) = 1.25;
quadNodes(1,2,0) = 2.75;  quadNodes(1,2,1) = 2.25;
quadNodes(1,3,0) = 1.75;  quadNodes(1,3,1) = 2.00;



// 1 Hexahedron cell with base topology: number of nodes = number of vertices
CellTopology hex_8(shards::getCellTopologyData<Hexahedron<8> >() );
numCells = 1;
numNodes = hex_8.getNodeCount();
spaceDim = hex_8.getDimension();

FieldContainer<double> hexNodes(numCells, numNodes, spaceDim);
hexNodes(0,0,0) =
// bottom face vertices
hexNodes(0,0,0) = 1.00;   hexNodes(0,0,1) = 1.00;   hexNodes(0,0,2) = 0.00;          
hexNodes(0,1,0) = 2.00;   hexNodes(0,1,1) = 0.75;   hexNodes(0,1,2) =-0.25;
hexNodes(0,2,0) = 1.75;   hexNodes(0,2,1) = 2.00;   hexNodes(0,2,2) = 0.00;
hexNodes(0,3,0) = 1.25;   hexNodes(0,3,1) = 2.00;   hexNodes(0,3,1) = 0.25;
// top face vertices
hexNodes(0,4,0) = 1.25;   hexNodes(0,4,1) = 0.75;   hexNodes(0,4,2) = 0.75;          
hexNodes(0,5,0) = 1.75;   hexNodes(0,5,1) = 1.00;   hexNodes(0,5,2) = 1.00;
hexNodes(0,6,0) = 2.00;   hexNodes(0,6,1) = 2.00;   hexNodes(0,6,2) = 1.25;
hexNodes(0,7,0) = 1.00;   hexNodes(0,7,1) = 2.00;   hexNodes(0,7,2) = 1.00;





std::cout << std::setprecision(16) << "\n" \
<< "===============================================================================\n"\
<< "| EXAMPLE 3: Using single point inclusion test method                         |\n"\
<< "===============================================================================\n";

// Define cell topologies
CellTopology edge3(shards::getCellTopologyData<Line<3> >() );
CellTopology tri6 (shards::getCellTopologyData<Triangle<6> >() );
CellTopology quad9(shards::getCellTopologyData<Quadrilateral<9> >() );
CellTopology tet4 (shards::getCellTopologyData<Tetrahedron<> >() );
CellTopology hex27(shards::getCellTopologyData<Hexahedron<27> >() );
CellTopology wedge(shards::getCellTopologyData<Wedge<> >() );
CellTopology pyr  (shards::getCellTopologyData<Pyramid<> >() );

// Points that are close to the boundaries of their reference cells
double point_in_edge[1]    = {1.0-INTREPID_EPSILON};
double point_in_quad[2]    = {1.0,                  1.0-INTREPID_EPSILON};
double point_in_tri[2]     = {0.5-INTREPID_EPSILON, 0.5-INTREPID_EPSILON};
double point_in_tet[3]     = {0.5-INTREPID_EPSILON, 0.5-INTREPID_EPSILON, 2.0*INTREPID_EPSILON};
double point_in_hex[3]     = {1.0-INTREPID_EPSILON, 1.0-INTREPID_EPSILON, 1.0-INTREPID_EPSILON};
double point_in_wedge[3]   = {0.5,                  0.25,                 1.0-INTREPID_EPSILON};
double point_in_pyramid[3] = {-INTREPID_EPSILON,    INTREPID_EPSILON,     1.0-INTREPID_EPSILON};

// Run the inclusion test for each point and print results
int in_edge     = CellTools::checkPointInclusion( point_in_edge,    1, edge3, INTREPID_THRESHOLD );
int in_quad     = CellTools::checkPointInclusion( point_in_quad,    2, quad9 );
int in_tri      = CellTools::checkPointInclusion( point_in_tri,     2, tri6 );
int in_tet      = CellTools::checkPointInclusion( point_in_tet,     3, tet4 );
int in_hex      = CellTools::checkPointInclusion( point_in_hex,     3, hex27 );
int in_prism    = CellTools::checkPointInclusion( point_in_wedge,   3, wedge );
int in_pyramid  = CellTools::checkPointInclusion( point_in_pyramid, 3, pyr );

if(in_edge) {
  std::cout << "(" << point_in_edge[0] << ")" 
  << " is inside reference Line " << endl;
}
if(in_quad) {
  std::cout << "(" << point_in_quad[0] << "," << point_in_quad[1]  << ")" 
  << " is inside reference Quadrilateral " << endl;
}
if(in_tri) {
  std::cout << "(" << point_in_tri[0] << "," << point_in_tri[1] << ")"  
  << " is inside reference Triangle " << endl;
}
if(in_tet) {
  std::cout << "(" << point_in_tet[0] << "," << point_in_tet[1] << "," << point_in_tet[2]<<")" 
  << " is inside reference Tetrahedron " << endl;
}
if(in_hex) {
  std::cout << "(" << point_in_hex[0] << "," << point_in_hex[1] << "," << point_in_hex[2]<<")" 
  << " is inside reference Hexahedron " << endl;
}
if(in_prism) {
  std::cout << "(" << point_in_wedge[0] << "," << point_in_wedge[1] << "," << point_in_wedge[2]<<")" 
  << " is inside reference Wedge " << endl;
}
if(in_pyramid) {
  std::cout << "(" << point_in_pyramid[0] << "," << point_in_pyramid[1] << "," << point_in_pyramid[2]<<")" 
  << " is inside reference Pyramid " << endl;
}

// Change the points to be outside their reference cells.
double small = 2.0*INTREPID_THRESHOLD;
point_in_edge[0] += small;

point_in_tri[0] += small;
point_in_tri[1] += small;

point_in_pyramid[0] += small;
point_in_pyramid[1] += small;
point_in_pyramid[2] += small;

in_edge     = CellTools::checkPointInclusion(point_in_edge,    1,   edge3);
in_tri      = CellTools::checkPointInclusion(point_in_tri,     2,   tri6);
in_pyramid  = CellTools::checkPointInclusion(point_in_pyramid, 3,   pyr);

std::cout << "\nChecking if perturbed Points belong to reference cell: " << endl;
if(!in_edge) {
  std::cout << "(" << point_in_edge[0] << ")" << " is NOT inside reference Line " << endl;
}
if(!in_tri) {
  std::cout << "(" << point_in_tri[0] << "," << point_in_tri[1] << ")"  << " is NOT inside reference Triangle " << endl;
}
if(!in_pyramid) {
  std::cout << "(" << point_in_pyramid[0] << "," << point_in_pyramid[1] << "," << point_in_pyramid[2]<<")" 
  << " is NOT inside reference Pyramid " << endl;
}



std::cout << std::setprecision(6) << "\n" \
<< "===============================================================================\n"\
<< "| EXAMPLE 4-A: Using pointwise inclusion test method for reference cells      |\n"\
<< "===============================================================================\n";


// Rank-1 array for one 2D reference point and rank-1 array for the test result
FieldContainer<double> onePoint(2);
FieldContainer<int> testOnePoint(1);

onePoint(0) = 0.2;   onePoint(1) = 0.3;

std::cout <<"\t Pointwise inclusion test for Triangle<6>: rank-1 array with a single 2D point: \n";

CellTools::checkPointwiseInclusion(testOnePoint, onePoint, tri6);  

std::cout << "point(" 
<< std::setw(13) << std::right << onePoint(0) << "," 
<< std::setw(13) << std::right << onePoint(1) << ") ";
if( testOnePoint(0) ) {
  std::cout << " is inside. \n";
}
else{
  std::cout << " is not inside. \n";
}
std::cout << "\n";


// Rank-2 array for 4 2D reference points (vector of points) and rank-1 array for the test result
FieldContainer<double>  fourPoints(4, 2);
FieldContainer<int> testFourPoints(4);

fourPoints(0,0) = 0.5;   fourPoints(0,1) = 0.5;
fourPoints(1,0) = 1.0;   fourPoints(1,1) = 1.1;
fourPoints(2,0) =-1.0;   fourPoints(2,1) =-1.1;
fourPoints(3,0) =-1.0;   fourPoints(3,1) = 0.5;

std::cout <<"\t  Pointwise inclusion test for Quadrilateral<9>: rank-2 array with 4 2D points: \n";

CellTools::checkPointwiseInclusion(testFourPoints, fourPoints, quad9);

for(int i1 = 0; i1 < fourPoints.dimension(0); i1++) {
  std::cout << " point(" << i1 << ") = (" 
  << std::setw(13) << std::right << fourPoints(i1, 0) << "," 
  << std::setw(13) << std::right << fourPoints(i1, 1) << ") ";
  if( testFourPoints(i1) ) {
    std::cout << " is inside. \n";
  }
  else{
    std::cout << " is not inside. \n";
  }
}
std::cout << "\n";


// Rank-3 array for 6 2D points and rank-2 array for the test result
FieldContainer<double>  sixPoints(2, 3, 2);
FieldContainer<int> testSixPoints(2, 3);

sixPoints(0,0,0) = -1.0;   sixPoints(0,0,1) =  1.0;
sixPoints(0,1,0) =  1.0;   sixPoints(0,1,1) =  0.0;
sixPoints(0,2,0) =  0.0;   sixPoints(0,2,1) =  1.0;
sixPoints(1,0,0) = -1.0;   sixPoints(1,0,1) = -1.0;
sixPoints(1,1,0) =  0.1;   sixPoints(1,1,1) =  0.2;
sixPoints(1,2,0) =  0.2;   sixPoints(1,2,1) =  0.3;

std::cout <<"\t  Pointwise inclusion test for Triangle<6>: rank-3 array with six 2D points: \n";

CellTools::checkPointwiseInclusion(testSixPoints, sixPoints, tri6);

for(int i0 = 0; i0 < sixPoints.dimension(0); i0++){
  for(int i1 = 0; i1 < sixPoints.dimension(1); i1++) {
    std::cout << " point(" << i0 << "," << i1 << ") = (" 
    << std::setw(13) << std::right << sixPoints(i0, i1, 0) << "," 
    << std::setw(13) << std::right << sixPoints(i0, i1, 1) << ") ";
    if( testSixPoints(i0, i1) ) {
      std::cout << " is inside. \n";
    }
    else{
      std::cout << " is not inside. \n";
    }
    
  }
}
std::cout << "\n";


// Rank-3 array for 6 3D reference points and rank-2 array for the test results
FieldContainer<double> six3DPoints(2, 3, 3);
FieldContainer<int> testSix3DPoints(2, 3);

six3DPoints(0,0,0) = -1.0;   six3DPoints(0,0,1) =  1.0;   six3DPoints(0,0,2) =  1.0;
six3DPoints(0,1,0) =  1.0;   six3DPoints(0,1,1) =  1.0;   six3DPoints(0,1,2) = -1.0;
six3DPoints(0,2,0) =  0.0;   six3DPoints(0,2,1) =  1.1;   six3DPoints(0,2,2) =  1.0;
six3DPoints(1,0,0) = -1.1;   six3DPoints(1,0,1) = -1.0;   six3DPoints(1,0,2) = -1.0;
six3DPoints(1,1,0) =  0.1;   six3DPoints(1,1,1) =  0.2;   six3DPoints(1,1,2) =  0.2;
six3DPoints(1,2,0) =  1.1;   six3DPoints(1,2,1) =  0.3;   six3DPoints(1,2,2) =  0.3;

std::cout <<"\t  Pointwise inclusion test for Hexahedron<27>: rank-3 array with six 3D points: \n";

CellTools::checkPointwiseInclusion(testSix3DPoints, six3DPoints, hex27);



for(int i0 = 0; i0 < six3DPoints.dimension(0); i0++){
  for(int i1 = 0; i1 < six3DPoints.dimension(1); i1++) {
    std::cout << " point(" << i0 << "," << i1 << ") = (" 
    << std::setw(13) << std::right << six3DPoints(i0, i1, 0) << "," 
    << std::setw(13) << std::right << six3DPoints(i0, i1, 1) << "," 
    << std::setw(13) << std::right << six3DPoints(i0, i1, 2) << ") ";
    if( testSix3DPoints(i0, i1) ) {
      std::cout << " is inside. \n";
    }
    else{
      std::cout << " is not inside. \n";
    }
  }
}


std::cout << std::setprecision(6) << "\n" \
<< "===============================================================================\n"\
<< "| EXAMPLE 4-B: Using pointwise inclusion test method for physical cells       |\n"\
<< "===============================================================================\n";

// Rank-2 array for 5 2D physical points and rank-1 array for the test results
FieldContainer<double>  fivePoints(5,2);
FieldContainer<int> testFivePoints(5);

// These points will be tested for inclusion in the last Triangle cell specified by triNodes
fivePoints(0, 0) = 2.1 ;   fivePoints(0, 1) = 1.0 ;       // in
fivePoints(1, 0) = 3.0 ;   fivePoints(1, 1) = 0.75;       // in
fivePoints(2, 0) = 3.5 ;   fivePoints(2, 1) = 1.9 ;       // out
fivePoints(3, 0) = 2.5 ;   fivePoints(3, 1) = 1.0 ;       // in
fivePoints(4, 0) = 2.75;   fivePoints(4, 1) = 2.0 ;       // out

CellTools::checkPointwiseInclusion(testFivePoints, fivePoints, triNodes, 3, triangle_3);

std::cout << " Vertices of Triangle #3: \n" 
<< "\t(" << triNodes(3, 0, 0) << ", " << triNodes(3, 0, 1) << ")\n"
<< "\t(" << triNodes(3, 1, 0) << ", " << triNodes(3, 1, 1) << ")\n"
<< "\t(" << triNodes(3, 1, 0) << ", " << triNodes(3, 1, 1) << ")\n"
<< " Inclusion test results for the physical points: \n\n";

for(int i1 = 0; i1 < fivePoints.dimension(0); i1++) {
  std::cout << " point(" << i1 << ") = (" 
  << std::setw(13) << std::right << fivePoints(i1, 0) << "," 
  << std::setw(13) << std::right << fivePoints(i1, 1) << ") ";
  if( testFivePoints(i1) ) {
    std::cout << " is inside. \n";
  }
  else{
    std::cout << " is not inside. \n";
  }
}
std::cout << "\n";



std::cout << "\n" \
<< "===============================================================================\n"\
<< "| EXAMPLE 5: Using point set inclusion test method                            |\n"\
<< "===============================================================================\n";

std::cout <<"\t  Point set inclusion test for Triangle<6>: rank-2 array with four 2D point: \n";

if( CellTools::checkPointsetInclusion(fourPoints, tri6) ) {
  std::cout << "\t - All points are inside the reference Triangle<6> cell. \n\n ";
}
else{
  std::cout << "\t - At least one point is not inside the reference Triangle<6> cell. \n\n";
}

std::cout <<"\t  Point set inclusion test for Hexahedron<27>: rank-3 array with six 3D point: \n";

if( CellTools::checkPointsetInclusion(six3DPoints, hex27) ) {
  std::cout << "\t - All points are inside the reference Hexahedron<27> cell. \n\n ";
}
else{
  std::cout << "\t - At least one point is not inside the reference Hexahedron<27> cell. \n\n";
}



std::cout << std::setprecision(4) << "\n" \
<< "===============================================================================\n"\
<< "| EXAMPLE 6: mapping to physical cells                                        |\n"\
<< "===============================================================================\n";

// Rank-3 array with dimensions (P, D) for points on the reference triangle
FieldContainer<double> refTriPoints(3,triangle_3.getDimension() );
refTriPoints(0,0) = 0.2;  refTriPoints(0,1) = 0.0;      // on edge 0
refTriPoints(1,0) = 0.4;  refTriPoints(1,1) = 0.6;      // on edge 1
refTriPoints(2,0) = 0.0;  refTriPoints(2,1) = 0.8;      // on edge 2

// Reference points will be mapped to physical cells with vertices in triNodes: define the appropriate array
FieldContainer<double> physTriPoints(triNodes.dimension(0),      // cell count
                                     refTriPoints.dimension(0),     // point count
                                     refTriPoints.dimension(1));    // point dimension (=2)

CellTools::mapToPhysicalFrame(physTriPoints, refTriPoints, triNodes, triangle_3);

for(int cell = 0; cell < triNodes.dimension(0); cell++){
  std::cout << "====== Triangle " << cell << " ====== \n";
  for(int pt = 0; pt < refTriPoints.dimension(0); pt++){
    std::cout 
    <<  "(" 
    << std::setw(13) << std::right << refTriPoints(pt,0) << "," 
    << std::setw(13) << std::right << refTriPoints(pt,1) << ") -> "
    <<  "(" 
    << std::setw(13) << std::right << physTriPoints(cell, pt, 0) << "," 
    << std::setw(13) << std::right << physTriPoints(cell, pt, 1) << ") \n"; 
  }
  std::cout << "\n";
}



std::cout << "\n" \
<< "===============================================================================\n"\
<< "| EXAMPLE 7: mapping to reference cells                                        |\n"\
<< "===============================================================================\n";

// Rank-2 arrays with dimensions (P, D) for physical points and their preimages
FieldContainer<double> physPoints(5, triangle_3.getDimension() ); 
FieldContainer<double> preImages (5, triangle_3.getDimension() ); 

// First 3 points are the vertices of the last triangle (ordinal = 3) stored in triNodes
physPoints(0,0) = triNodes(3, 0, 0);   physPoints(0,1) = triNodes(3, 0, 1) ;
physPoints(1,0) = triNodes(3, 1, 0);   physPoints(1,1) = triNodes(3, 1, 1);
physPoints(2,0) = triNodes(3, 2, 0);   physPoints(2,1) = triNodes(3, 2, 1);

// last 2 points are just some arbitrary points contained in the last triangle
physPoints(3,0) = 3.0;                    physPoints(3,1) = 1.0;
physPoints(4,0) = 2.5;                    physPoints(4,1) = 1.1;


// Map physical points from triangle 3 to the reference triangle
CellTools::mapToReferenceFrame(preImages, physPoints, triNodes, triangle_3, 3  );

std::cout << " Mapping from Triangle #3 with vertices: \n" 
<< "\t(" << triNodes(3, 0, 0) << ", " << triNodes(3, 0, 1) << ")\n"
<< "\t(" << triNodes(3, 1, 0) << ", " << triNodes(3, 1, 1) << ")\n"
<< "\t(" << triNodes(3, 1, 0) << ", " << triNodes(3, 1, 1) << ")\n\n"
<< " Physical points and their reference cell preimages: \n";

for(int pt = 0; pt < physPoints.dimension(0); pt++){
  std::cout 
  <<  "(" << std::setw(13) << std::right << physPoints(pt,0) << "," 
  << std::setw(13) << std::right << physPoints(pt,1) << ") -> "
  <<  "(" 
  << std::setw(13) << std::right << preImages(pt, 0) << "," 
  << std::setw(13) << std::right << preImages(pt, 1) << ") \n"; 
}
std::cout << "\n";

// As a check, map pre-images back to Triangle #3
FieldContainer<double> images(5, triangle_3.getDimension() );
CellTools::mapToPhysicalFrame(images, preImages, triNodes, triangle_3, 3);

std::cout << " Check: map preimages back to Triangle #3: \n"; 
for(int pt = 0; pt < images.dimension(0); pt++){
  std::cout 
  <<  "(" << std::setw(13) << std::right << preImages(pt,0) << "," 
  << std::setw(13) << std::right << preImages(pt,1) << ") -> "
  <<  "(" 
  << std::setw(13) << std::right << images(pt, 0) << "," 
  << std::setw(13) << std::right << images(pt, 1) << ") \n"; 
}
std::cout << "\n";





return 0;
}




























