# - Find MySQL
# Find the MySQL includes and client library
# This module defines
#  MYSQL_INCLUDE_DIR, where to find mysql.h
#  MYSQL_LIBRARY, the libraries needed to use MySQL.
#  MYSQL_FOUND, If false, do not try to use MySQL.

if(MYSQL_INCLUDE_DIR AND MYSQL_LIBRARY)
   set(MYSQL_FOUND TRUE)
else()
  find_path(MYSQL_INCLUDE_DIR mysql.h
    /usr/include/mysql
    /usr/local/include/mysql
    /opt/mysql/mysql/include
    /opt/mysql/include
    /usr/include
  )

  find_library(MYSQL_LIBRARY NAMES mysqlclient
    PATHS
    /usr/lib
    /usr/lib64
    /usr/lib/mysql
    /usr/lib64/mysql
    /usr/local/lib/mysql
    /usr/local/lib64/mysql
    /opt/mysql/mysql/lib
    /opt/mysql/lib
  )

  if(MYSQL_INCLUDE_DIR AND MYSQL_LIBRARY)
    set(MYSQL_FOUND TRUE)
    message(STATUS "Found MySQL: ${MYSQL_INCLUDE_DIR}, ${MYSQL_LIBRARY}")
  else()
    set(MYSQL_FOUND FALSE)
    message(STATUS "MySQL not found.")
  endif()

  mark_as_advanced(MYSQL_INCLUDE_DIR MYSQL_LIBRARY)
endif() 