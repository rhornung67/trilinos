#ifndef CTHULHU_VECTOR_HPP
#define CTHULHU_VECTOR_HPP

#include "Cthulhu_ConfigDefs.hpp"
#include "Cthulhu_MultiVector.hpp"

namespace Cthulhu {

  template <class Scalar, class LocalOrdinal = int, class GlobalOrdinal = LocalOrdinal, class Node = Kokkos::DefaultNode::DefaultNodeType>
  class Vector
    : public virtual MultiVector< Scalar, LocalOrdinal, GlobalOrdinal, Node > {

  public:

    using MultiVector< Scalar, LocalOrdinal, GlobalOrdinal, Node >::dot;          // overloading, not hiding
    using MultiVector< Scalar, LocalOrdinal, GlobalOrdinal, Node >::norm1;        // overloading, not hiding
    using MultiVector< Scalar, LocalOrdinal, GlobalOrdinal, Node >::norm2;        // overloading, not hiding
    using MultiVector< Scalar, LocalOrdinal, GlobalOrdinal, Node >::normInf;      // overloading, not hiding
    using MultiVector< Scalar, LocalOrdinal, GlobalOrdinal, Node >::normWeighted; // overloading, not hiding
    using MultiVector< Scalar, LocalOrdinal, GlobalOrdinal, Node >::meanValue;    // overloading, not hiding

    //! @name Constructor/Destructor Methods
    //@{

    //! Destructor.
    virtual ~Vector() { }

    //@}

    //! @name Mathematical methods
    //@{

    //! Computes dot product of this Vector against input Vector x.
    virtual Scalar dot(const Vector< Scalar, LocalOrdinal, GlobalOrdinal, Node > &a) const = 0;

    //! Return 1-norm of this Vector.
    virtual typename Teuchos::ScalarTraits< Scalar >::magnitudeType norm1() const = 0;

    //! Compute 2-norm of this Vector.
    virtual typename Teuchos::ScalarTraits< Scalar >::magnitudeType norm2() const = 0;

    //! Compute Inf-norm of this Vector.
    virtual typename Teuchos::ScalarTraits< Scalar >::magnitudeType normInf() const = 0;

    //! Compute Weighted 2-norm (RMS Norm) of this Vector.
    virtual typename Teuchos::ScalarTraits< Scalar >::magnitudeType normWeighted(const Vector< Scalar, LocalOrdinal, GlobalOrdinal, Node > &weights) const = 0;

    //! Compute mean (average) value of this Vector.
    virtual Scalar meanValue() const = 0;

    //@}

    //! @name Overridden from Teuchos::Describable
    //@{

    //! Return a simple one-line description of this object.
    virtual std::string description() const = 0;

    //! Print the object with some verbosity level to an FancyOStream object.
    virtual void describe(Teuchos::FancyOStream &out, const Teuchos::EVerbosityLevel verbLevel=Teuchos::Describable::verbLevel_default) const = 0;

    //@}

    //! @name Cthulhu specific
    //@{

    using MultiVector< Scalar, LocalOrdinal, GlobalOrdinal, Node >::maxValue; // overloading, not hiding
    //! Compute max value of this Vector.
    virtual Scalar maxValue() const = 0;

    //@}

  }; // Vector class

} // Cthulhu namespace

#define CTHULHU_VECTOR_SHORT
#endif // CTHULHU_VECTOR_HPP
