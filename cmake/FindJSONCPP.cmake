# - Find JSONCPP
# Find the JSONCPP includes and library
# This module defines
#  JSONCPP_INCLUDE_DIRS, where to find json.h
#  JSONCPP_LIBRARIES, the libraries needed to use JSONCPP.
#  JSONCPP_FOUND, If false, do not try to use JSONCPP.

find_package(PkgConfig QUIET)
if(PKG_CONFIG_FOUND)
  pkg_check_modules(PC_JSONCPP jsoncpp)
endif()

find_path(JSONCPP_INCLUDE_DIR json/json.h
  HINTS
  ${PC_JSONCPP_INCLUDEDIR}
  ${PC_JSONCPP_INCLUDE_DIRS}
  PATH_SUFFIXES jsoncpp
  PATHS
  /usr/include
  /usr/local/include
)

find_library(JSONCPP_LIBRARY NAMES jsoncpp
  HINTS
  ${PC_JSONCPP_LIBDIR}
  ${PC_JSONCPP_LIBRARY_DIRS}
  PATHS
  /usr/lib
  /usr/lib64
  /usr/local/lib
  /usr/local/lib64
)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(JSONCPP
  DEFAULT_MSG
  JSONCPP_LIBRARY JSONCPP_INCLUDE_DIR
)

if(JSONCPP_FOUND)
  set(JSONCPP_LIBRARIES ${JSONCPP_LIBRARY})
  set(JSONCPP_INCLUDE_DIRS ${JSONCPP_INCLUDE_DIR})
endif()

mark_as_advanced(JSONCPP_INCLUDE_DIR JSONCPP_LIBRARY) 