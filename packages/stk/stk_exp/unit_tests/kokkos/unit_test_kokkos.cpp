#include <gtest/gtest.h>

// restrict this file to only build on platforms that defined
// STK_EXP_BUILD_KOKKOS
#ifdef STK_EXP_BUILD_KOKKOS
#include <Kokkos_Threads.hpp>
#include <Kokkos_hwloc.hpp>
#include <Kokkos_View.hpp>

#include <iostream>

namespace {

// setup and tear down for the KokkosThreads unit tests
class KokkosThreads : public ::testing::Test {
protected:
  static void SetUpTestCase()
  {
    unsigned num_threads = 8;

    // if hwloc is present we will get better thread placement and
    // numa aware allocation.
    // Currently sierra does not have the hwloc TPL enabled
    if (Kokkos::hwloc::available()) {
      num_threads = Kokkos::hwloc::get_available_numa_count()
                    * Kokkos::hwloc::get_available_cores_per_numa()
                 // * Kokkos::hwloc::get_available_threads_per_core()
                    ;

    }

    std::cout << "Threads: " << num_threads << std::endl;

    Kokkos::Threads::initialize( num_threads );
  }

  static void TearDownTestCase()
  {
    Kokkos::Threads::finalize();
  }
};

const size_t RUN_TIME_DIMENSION = 4000000;
const size_t COMPILE_TIME_DIMENSION = 3;

} // unnamed namespace


TEST_F( KokkosThreads, SerialInitialize)
{
  // allocate a rank 2 array witn that is RUN_TIME_DIMENSION x COMPILE_TIME_DIMENSION

  // View will default initialize all the values unless it is explicitly disabled, ie,
  // Kokkos::View<unsigned*[COMPILE_TIME_DIMENSION], Kokkos::Threads> a("node views", RUN_TIME_DIMENSION);
  // zero fills the array, but
  // Kokkos::View<unsigned*[COMPILE_TIME_DIMENSION], Kokkos::Threads> a( Kokkos::allocate_without_initializing, "node views", RUN_TIME_DIMENSION);
  // will allocate without initializing the array

  Kokkos::View<unsigned*[COMPILE_TIME_DIMENSION], Kokkos::Threads> a( Kokkos::allocate_without_initializing, "node views", RUN_TIME_DIMENSION);

  for (size_t i=0; i < a.dimension_0(); ++i) {
    for (size_t x=0; x < a.dimension_1(); ++x) {
      a(i,x) = i;
    }
  }

  // get a const view to the same array
  // this view shares the same memory as a, but cannot modify the values
  Kokkos::View<const unsigned*[COMPILE_TIME_DIMENSION], Kokkos::Threads> b = a;

  for (size_t i=0; i < b.dimension_0(); ++i) {
    for (size_t x=0; x < b.dimension_1(); ++x) {
      EXPECT_EQ(i, b(i,x));
    }
  }
}

// Not available until c++11 support is enable in Sierra and Kokkos
#if defined (KOKKOS_HAVE_C_PLUS_PLUS_11_LAMBDA)
TEST_F( KokkosThreads, LambdaInitialize)
{
  Kokkos::View<unsigned*[COMPILE_TIME_DIMENSION], Kokkos::Threads> a( Kokkos::allocate_without_initializing, "node views", RUN_TIME_DIMENSION);

  Kokkos::parallel_for<Kokkos::Threads>(
    a.dimension_0() ,
    [=](size_t i) {
      for (size_t x=0; x < a.dimension_1(); ++x) {
        a(i,x) = i;
      }
    }
  );

  Kokkos::View<const unsigned*[COMPILE_TIME_DIMENSION], Kokkos::Threads> b = a;

  int num_error = 0;
  // Cannot portably call a GTEST macro in parallel
  // count the errors and test that they are equal to zero
  Kokkos::parallel_reduce<Kokkos::Threads, int /*reduction value type */>(
    b.dimension_0() ,
    [](int & local_errors)                                    // init lambda
    { local_errors = 0; } ,
    [=](size_t i, int & local_errors) {                       // operator() lambda
      for (size_t x=0; x < b.dimension_1(); ++x)
        local_errors += i == b(i,x) ? 0 : 1;
    } ,
    [](volatile int & dst_err, volatile int const& src_err)   // join lambda
    { dst_err += src_err; } ,
    num_errors                                                // where to store the result
  );
  EXPECT_EQ( 0, num_errors);

}
#endif


namespace {

// Functors need to initialize and check the view with out c++11 support

template <typename View>
struct InitializeView
{
  // need a device_type typedef for all parallel functors
  typedef typename View::device_type device_type;

  View a;

  // get a view to the a
  template <typename RhsView>
  InitializeView( RhsView const& arg_a )
    : a(arg_a)
  {}

  void apply()
  {
    // call parallel_for on this functor
    Kokkos::parallel_for( a.dimension_0(), *this);
  }

  // initialize the a
  KOKKOS_INLINE_FUNCTION
  void operator()(size_t i) const
  {
    for (size_t x=0; x < a.dimension_1(); ++x) {
      a(i,x) = i;
    }
  }
};

template <typename View>
struct CheckView
{
  // need a device_type typedef for all parallel functors
  typedef typename View::device_type device_type;

  // need a value_type typedef for the reduction
  typedef int value_type;

  View a;

  // get a view to the a
  template <typename RhsView>
  CheckView( RhsView const& arg_a )
    : a(arg_a)
  {}

  // return the number of errors found
  value_type apply()
  {
    int num_errors = 0;
    // call a parallel_reduce to count the errors
    Kokkos::parallel_reduce( a.dimension_0(), *this, num_errors);
    return num_errors;
  }

  // initialize the reduction type
  KOKKOS_INLINE_FUNCTION
  void init(value_type & v) const
  { v = 0; }

  // this threads contribution to the reduction type
  // check that the value is equal to the expected value
  // otherwise increment the error count
  KOKKOS_INLINE_FUNCTION
  void operator()(size_t i, value_type & error) const
  {
    for (size_t x=0; x < a.dimension_1(); ++x) {
      error += i == a(i,x) ? 0 : 1;
    }
  }

  // join two threads together
  KOKKOS_INLINE_FUNCTION
  void join( volatile value_type & dst, volatile value_type const& src) const
  { dst += src; }
};

} // unnameed namespace

TEST_F( KokkosThreads, ParallelInitialize)
{
  typedef Kokkos::View<unsigned*[COMPILE_TIME_DIMENSION], Kokkos::Threads> view_type;
  typedef Kokkos::View<const unsigned*[COMPILE_TIME_DIMENSION], Kokkos::Threads> const_view_type;

  view_type a(Kokkos::allocate_without_initializing, "node views", RUN_TIME_DIMENSION);

  // call the InitializeView functor
  {
    InitializeView<view_type> f(a);
    f.apply();
  }

  // call the CheckView functor
  // and expect no errors
  {
    const_view_type b = a;
    CheckView<view_type> f(a);
    const int num_errors = f.apply();
    EXPECT_EQ( 0, num_errors);
  }
}

#endif
