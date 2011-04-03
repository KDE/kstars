# - Try to find INDI
# Once done this will define
#
#  INDI_FOUND - system has INDI
#  INDI_INCLUDE_DIR - the INDI include directory
#  INDI_LIBRARIES - Link these to use INDI

# Copyright (c) 2006, Jasem Mutlaq <mutlaqja@ikarustech.com>
# Based on FindLibfacile by Carsten Niehaus, <cniehaus@gmx.de>
#
# Redistribution and use is allowed according to the terms of the BSD license.
# For details see the accompanying COPYING-CMAKE-SCRIPTS file.

if (INDI_INCLUDE_DIR AND INDI_LIBRARIES AND INDI_DRIVER_LIBRARIES)

  # in cache already
  set(INDI_FOUND TRUE)
  message(STATUS "Found INDI: ${INDI_LIBRARIES}")


else (INDI_INCLUDE_DIR AND INDI_LIBRARIES AND INDI_DRIVER_LIBRARIES)

  find_path(INDI_INCLUDE_DIR indidevapi.h
    PATH_SUFFIXES libindi
    ${_obIncDir}
    ${GNUWIN32_DIR}/include
  )

  find_library(INDI_DRIVER_LIBRARIES NAMES indidriver
    PATHS
    ${_obLinkDir}
    ${GNUWIN32_DIR}/lib
  )

  find_library(INDI_LIBRARIES NAMES indi
    PATHS
    ${_obLinkDir}
    ${GNUWIN32_DIR}/lib
  )

  if(INDI_INCLUDE_DIR AND INDI_LIBRARIES AND INDI_DRIVER_LIBRARIES)
    set(INDI_FOUND TRUE)
  else (INDI_INCLUDE_DIR AND INDI_LIBRARIES AND INDI_DRIVER_LIBRARIES)
    set(INDI_FOUND FALSE)
  endif(INDI_INCLUDE_DIR AND INDI_LIBRARIES AND INDI_DRIVER_LIBRARIES)

  # Find pkg-config
  FIND_PROGRAM(PKGCONFIG_EXECUTABLE NAMES pkg-config PATHS /usr/bin/ /usr/local/bin )

  # query pkg-config asking for a libindi >= 0.6.2
     EXEC_PROGRAM(${PKGCONFIG_EXECUTABLE} ARGS --atleast-version=0.6.2 libindi RETURN_VALUE _return_VALUE OUTPUT_VARIABLE _pkgconfigDevNull )
     if(_return_VALUE STREQUAL "0")
        set(INDI_FOUND TRUE)
     else(_return_VALUE STREQUAL "0")
	set(INDI_FOUND FALSE)
	message(STATUS "Could NOT find libindi. pkg-config indicates that libindi >= 0.6.2 is not installed.")
     endif(_return_VALUE STREQUAL "0")

  # Find INDI Server
  FIND_PROGRAM(INDISERVER NAMES indiserver PATHS /usr/bin/ /usr/local/bin )

  if (INDISERVER-NOTFOUND)
    set(INDI_FOUND FALSE)
  endif(INDISERVER-NOTFOUND)
 
 if (INDI_FOUND)
    if (NOT INDI_FIND_QUIETLY)
      message(STATUS "Found INDI: ${INDI_LIBRARIES}, ${INDI_DRIVER_LIBRARIES}")
    endif (NOT INDI_FIND_QUIETLY)
  else (INDI_FOUND)
    if (INDI_FIND_REQUIRED)
      message(FATAL_ERROR "libindi and indiserver not found. Please install libindi and try again. http://www.indilib.org")
    endif (INDI_FIND_REQUIRED)
  endif (INDI_FOUND)

  mark_as_advanced(INDI_INCLUDE_DIR INDI_LIBRARIES INDI_DRIVER_LIBRARIES)

endif (INDI_INCLUDE_DIR AND INDI_LIBRARIES AND INDI_DRIVER_LIBRARIES)
