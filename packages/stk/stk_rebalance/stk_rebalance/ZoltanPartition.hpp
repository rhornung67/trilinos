/*----------------------------------------------------------------------*/
/*                                                                      */
/*       author: Jonathan Scott Rath                                    */
/*      author2: Michael W. Glass   (DEC/2000)                          */
/*     filename: ZoltanPartition.h                                      */
/*      purpose: header file for stk toolkit zoltan methods             */
/*                                                                      */
/*----------------------------------------------------------------------*/
/*    Copyright 2001,2010 Sandia Corporation.                           */
/*    Under the terms of Contract DE-AC04-94AL85000, there is a         */
/*    non-exclusive license for use of this work by or on behalf        */
/*    of the U.S. Government.  Export of this program may require       */
/*    a license from the United States Government.                      */
/*----------------------------------------------------------------------*/

// Copyright 2001 Sandia Corporation, Albuquerque, NM.

#ifndef stk_rebalance_ZoltanPartition_hpp    
#define stk_rebalance_ZoltanPartition_hpp

#include <utility>
#include <vector>
#include <string>

#include <Teuchos_ParameterList.hpp>
#include <stk_rebalance/GeomDecomp.h>

//Forward declaration for pointer to a Zoltan structrue.
struct Zoltan_Struct;

namespace stk {
namespace rebalance {

class Zoltan : public GeomDecomp {

public:

  /**
   * Constructor
   */

  static Zoltan create_default(const Teuchos::ParameterList & rebal_region_parameters);

  explicit Zoltan(const std::string &Parameters_Name=GeomDecomp::default_parameters_name());

  void init(const std::vector< std::pair<std::string, std::string> >
            &dynamicLoadRebalancingParameters);

  static double init_zoltan_library();
  /**
   * Destructor
   */

  virtual ~Zoltan();

  /** Name Conversion Functions.
   * Long friendly string prarameters
   * need to be converted into short Zoltan names.  These
   * two functions do that.  Merge_Default_Values should be called
   * first because it merges with the long names.  Convert_Names_and_Values
   * should then be called second to convert long names to short names.
   */
  static void merge_default_values   (const Teuchos::ParameterList &from,
                                      Teuchos::ParameterList &to);
  static void convert_names_and_values (const Teuchos::ParameterList &from,
                                        Teuchos::ParameterList &to);

  /**
   * Register SIERRA Framework Zoltan call-back functions
   */
  Int register_callbacks();

  virtual Int determine_new_partition (bool & RebalancingNeeded);

  /**
   * Evaluate the performance/quality of dynamic load rebalancing
   */
  Int evaluate ( Int   print_stats,
                 Int   *nobj,
                 Real  *obj_wgt,
                 Int   *ncuts,
                 Real  *cut_wgt,
                 Int   *nboundary,
                 Int   *nadj         );

  /**
   * Decomposition Augmentation
   */
  virtual Int point_assign( Real *coords,
                            Int  *proc ) const;

  virtual Int box_assign ( Real min[],
                           Real max[],
                           std::vector<Int> &procs) const;

  /**
   * Inline functions to access private data
   */

  Real zoltan_version()  const;
  const std::string & parameter_entry_name() const;

  virtual Diag::Writer &verbose_print(Diag::Writer &dout) const;

  Zoltan_Struct * zoltan() {
    return zoltan_id;
  }
  const Zoltan_Struct * zoltan() const {
    return zoltan_id;
  }
private:
  /** Zoltan load balancing struct       */
  struct    Zoltan_Struct *zoltan_id;

  /** Name that was used to initialize this Zoltan_Struct
   * if the parameter constructor was used.
   */
  std::string  parameter_entry_Name;

};

}
} // namespace sierra

#endif