#ifndef TPETRACLASSIC_DEFAULTNODE_CONFIG_H
#define TPETRACLASSIC_DEFAULTNODE_CONFIG_H

#define KOKKOSCLASSIC_DEFAULTNODE Kokkos::Compat::KokkosSerialWrapperNode
/* #undef HAVE_KOKKOSCLASSIC_DEFAULTNODE_TPINODE */
/* #undef HAVE_KOKKOSCLASSIC_DEFAULTNODE_TBBNODE */
/* #undef HAVE_KOKKOSCLASSIC_DEFAULTNODE_OPENMPNODE */
/* #undef HAVE_KOKKOSCLASSIC_DEFAULTNODE_CUDAWRAPPERNODE */
/* #undef HAVE_KOKKOSCLASSIC_DEFAULTNODE_OPENMPWRAPPERNODE */
/* #undef HAVE_KOKKOSCLASSIC_DEFAULTNODE_THREADSWRAPPERNODE */
#define HAVE_KOKKOSCLASSIC_DEFAULTNODE_SERIALWRAPPERNODE
/* #undef HAVE_KOKKOSCLASSIC_DEFAULTNODE_SERIALNODE */

#endif /* TPETRACLASSIC_DEFAULTNODE_CONFIG_H */
