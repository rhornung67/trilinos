# Find the petsc4py Python module
#
#  PETSC4PY_INCLUDES     - where to find petsc4py/petsc4py.i, etc.
#  PETSC4PY_FOUND        - True if petsc4py is found

IF(PETSC4PY_INCLUDES)
  SET(PETSC4PY_FIND_QUIETLY TRUE)
ENDIF(PETSC4PY_INCLUDES)

EXECUTE_PROCESS(
  COMMAND ${PYTHON_EXECUTABLE} -c "import petsc4py; from sys import stdout; stdout.write(petsc4py.get_include())"
  OUTPUT_VARIABLE PETSC4PY_INCLUDES
  RESULT_VARIABLE PETSC4PY_NOT_FOUND)

INCLUDE(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(PETSc4Py DEFAULT_MSG PETSC4PY_INCLUDES)

# if(PETSC4PY_INCLUDES)
#   set(PETSC4PY_FOUND TRUE)
#   set(PETSC4PY_INCLUDES ${PETSC4PY_INCLUDES} CACHE STRING "PETSc4Py include path")
# else(PETSC4PY_INCLUDES)
#   set(PETSC4PY_FOUND FALSE)
# endif(PETSC4PY_INCLUDES)
# 
# if(PETSC4PY_FOUND)
#   if(NOT PETSC4PY_FIND_QUIETLY)
#     message(STATUS "Found petsc4py: ${PETSC4PY_INCLUDES}")
#   endif(NOT PETSC4PY_FIND_QUIETLY)
# else(PETSC4PY_FOUND)
#   if(PETSC4PY_FIND_REQUIRED)
#     message(FATAL_ERROR "petsc4py headers missing")
#   endif(PETSC4PY_FIND_REQUIRED)
# endif(PETSC4PY_FOUND)
#
MARK_AS_ADVANCED(PETSC4PY_INCLUDES)
