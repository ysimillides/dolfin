set(DOLFIN_CGAL_FOUND 0)

message(STATUS "checking for package 'CGAL'")

find_path(DOLFIN_CGAL_INCLUDE_DIR CGAL
  /usr/include
  /usr/local/include
  DOC "Directory where the CGAL header directory is located"
  )
mark_as_advanced(DOLFIN_CGAL_INCLUDE_DIR)

find_library(DOLFIN_CGAL_LIBRARY CGAL
  DOC "The CGAL library"
  )
mark_as_advanced(DOLFIN_CGAL_LIBRARY)

find_library(DOLFIN_MPFR_LIBRARY mpfr
  DOC "The mpfr library"
  )
mark_as_advanced(DOLFIN_MPFR_LIBRARY)

find_library(DOLFIN_GMP_LIBRARY gmp
  DOC "The GMP library"
  )
mark_as_advanced(DOLFIN_GMP_LIBRARY)

if(DOLFIN_CGAL_INCLUDE_DIR AND DOLFIN_CGAL_LIBRARY AND DOLFIN_MPFR_LIBRARY AND DOLFIN_GMP_LIBRARY)
  set(DOLFIN_CGAL_FOUND 1)
endif(DOLFIN_CGAL_INCLUDE_DIR AND DOLFIN_CGAL_LIBRARY AND DOLFIN_MPFR_LIBRARY AND DOLFIN_GMP_LIBRARY)

if(DOLFIN_CGAL_FOUND)
  message(STATUS "  found CGAL, version ${CGAL_VERSION}")
else(DOLFIN_CGAL_FOUND)
  message(STATUS "  package 'CGAL' not found")
endif(DOLFIN_CGAL_FOUND)