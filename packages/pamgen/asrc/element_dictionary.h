// $Id$

#ifndef element_dictionaryH
#define element_dictionaryH

#include "code_types.h"
namespace PAMGEN_NEVADA {


const uint MAX_ELEMENT_FACES = 24;        // for HEX27

enum Element_Type {
               UNKNOWN_ELEMENT = 0, CIRCLE,   SPHERE,   QUAD4,    QUAD8,
               QUAD9,               TRI3,     TRI6,     TRI7,     SHELL4,
               SHELL8,              HEX8,     HEX20,    HEX27,    TET4,     TET8,
               TET10,               TET15,    WEDGE6,   WEDGE18,  BEAM2,    
               BEAM3,               TRUSS2,   TRUSS3,   STRUCT1D, STRUCT2D, 
               STRUCT3D,            SFACET3D, PYRAMID5, SEG2,     SEG3,
               MAX_EL_TYPES
             };

enum Element_Class {
               EL_UNKNOWN = 0,  EL_CONTINUUM,  EL_STRUCTURAL
             };

struct Element_Info
{
  uint nodes;
  uint edges;
  uint faces;
  uint max_nodes_per_face;
  uint number_of_stress_points;
  Element_Class element_class;
};

extern const Element_Info element_info[MAX_EL_TYPES];
extern const int nodes_per_face[MAX_EL_TYPES];
extern const int faces_per_element[MAX_EL_TYPES];
extern const int edges_per_element[MAX_EL_TYPES];

/* translations tables do not include the nonstandard element TET8 */

/* node to side translation tables - 
 *   These tables are used to look up the side number based on the
 *   first and second node in the side/face list. The side node order
 *   is found in the original Exodus document, SAND87-2997. The element
 *   node order is found in the ExodusII document, SAND92-2137. These
 *   tables were generated by following the right-hand rule for determining
 *   the outward normal. Note: Only the more complex 3-D shapes require
 *   these tables, the simple shapes are trivial - the first node found
 *   is also the side number.
 */

extern const int shell_node_to_side_table[2][8];
extern const int tetra_node_to_side_table[2][12];
extern const int wedge_node_to_side_table[2][18];
extern const int hex_node_to_side_table[2][24];

/* side to node translation tables -
 *   These tables are used to look up the side number based on the
 *   first and second node in the side/face list. The side node order
 *   is found in the original Exodus document, SAND87-2997. The element
 *   node order is found in the ExodusII document, SAND92-2137. These
 *   tables were generated by following the right-hand rule for determining
 *   the outward normal.
 */

extern const int tri_side_to_node_table[3][3];
extern const int quad_side_to_node_table[4][3];
extern const int shell_side_to_node_table[2][8];
extern const int tetra_side_to_node_table[4][7];
extern const int wedge_side_to_node_table[5][8];
extern const int hex_side_to_node_table[6][8];

extern const char* Element_Type_Names[MAX_EL_TYPES];

} // end namespace PAMGEN_NEVADA {
#endif
