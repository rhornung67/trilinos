#
# Build all Secondary Stable CASL VRI add-on Trilnos packages with GCC 4.5.1 compiler
#

INCLUDE("${CTEST_SCRIPT_DIRECTORY}/TrilinosCTestDriverCore.pu241.icpc.12.0.4.cmake")
INCLUDE("${CTEST_SCRIPT_DIRECTORY}/SubmitToCaslDev.cmake")
INCLUDE("${CTEST_SCRIPT_DIRECTORY}/casl-vri-packages-native.cmake")

SET(COMM_TYPE SERIAL)
SET(BUILD_TYPE RELEASE)
SET(BUILD_DIR_NAME SERIAL_RELEASE_ICPC_NATIVEBUILDS)
#SET(CTEST_TEST_TYPE Experimental)
#SET(CTEST_TEST_TIMEOUT 900)
SET(Trilinos_ALL_PACKAGES                    OFF)
SET(Trilinos_ENABLE_SECONDARY_STABLE_CODE    OFF)
SET(Trilinos_ENABLE_ALL_FORWARD_DEP_PACKAGES OFF)
SET(Trilinos_ENABLE_ALL_OPTIONAL_PACKAGES    OFF)

SET(Trilinos_ALL_PACKAGES OFF)
SET(EXTRA_CONFIGURE_OPTIONS
  ${EXTRA_CONFIGURE_OPTIONS}
  -DTrilinos_ENABLE_CASLRAVE:BOOL=ON
  -DTrilinos_ENABLE_CASLBOA:BOOL=ON
  )

TRILINOS_SYSTEM_SPECIFIC_CTEST_DRIVER()
