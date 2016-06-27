# - Try to find INDI
# Once done this will define
#
#  INDI_FOUND - system has INDI
#  INDI_INCLUDE_DIR - the INDI include directory
#  INDI_LIBRARIES - Link to these for XML and INDI Common support
#  INDI_CLIENT_LIBRARIES - Link to these to build INDI clients

# Copyright (c) 2016, Jasem Mutlaq <mutlaqja@ikarustech.com>
# Copyright (c) 2012, Pino Toscano <pino@kde.org>
# Based on FindLibfacile by Carsten Niehaus, <cniehaus@gmx.de>
#
# Redistribution ANDuse is allowed according to the terms of the BSD license.
# For details see the accompanying COPYING-CMAKE-SCRIPTS file.

macro(_INDI_check_version)
  file(READ "${INDI_INCLUDE_DIR}/indiapi.h" _INDI_version_header)

  string(REGEX MATCH "#define INDI_VERSION_MAJOR[ \t]+([0-9]+)" _INDI_version_major_match "${_INDI_version_header}")
  set(INDI_VERSION_MAJOR "${CMAKE_MATCH_1}")
  string(REGEX MATCH "#define INDI_VERSION_MINOR[ \t]+([0-9]+)" _INDI_version_minor_match "${_INDI_version_header}")
  set(INDI_VERSION_MINOR "${CMAKE_MATCH_1}")
  string(REGEX MATCH "#define INDI_VERSION_RELEASE[ \t]+([0-9]+)" _INDI_version_release_match "${_INDI_version_header}")
  set(INDI_VERSION_RELEASE "${CMAKE_MATCH_1}")

  set(INDI_VERSION ${INDI_VERSION_MAJOR}.${INDI_VERSION_MINOR}.${INDI_VERSION_RELEASE})
  if(${INDI_VERSION} VERSION_LESS ${INDI_FIND_VERSION})
    set(INDI_VERSION_OK FALSE)
  else(${INDI_VERSION} VERSION_LESS ${INDI_FIND_VERSION})
    set(INDI_VERSION_OK TRUE)
  endif(${INDI_VERSION} VERSION_LESS ${INDI_FIND_VERSION})

  if(NOT INDI_VERSION_OK)
    message(STATUS "INDI version ${INDI_VERSION} found in ${INDI_INCLUDE_DIR}, "
                   "but at least version ${INDI_FIND_VERSION} is required")
  else(NOT INDI_VERSION_OK)
      mark_as_advanced(INDI_VERSION_MAJOR INDI_VERSION_MINOR INDI_VERSION_RELEASE)
  endif(NOT INDI_VERSION_OK)
endmacro(_INDI_check_version)

if (INDI_INCLUDE_DIR AND INDI_LIBRARIES AND INDI_CLIENT_LIBRARIES)

  # in cache already
  _INDI_check_version()
  set(INDI_FOUND ${INDI_VERSION_OK})
  message(STATUS "Found INDI: ${INDI_LIBRARIES}, ${INDI_CLIENT_LIBRARIES}, ${INDI_INCLUDE_DIR}")

else (INDI_INCLUDE_DIR AND INDI_LIBRARIES AND INDI_CLIENT_LIBRARIES)

  if (NOT WIN32)
    find_package(PkgConfig)
    if (PKG_CONFIG_FOUND)
      pkg_check_modules(PC_INDI INDI)
    endif (PKG_CONFIG_FOUND)
  endif (NOT WIN32)

  find_path(INDI_INCLUDE_DIR indidevapi.h
    PATH_SUFFIXES libindi
    ${PC_INDI_INCLUDE_DIRS}
    ${_obIncDir}
    ${GNUWIN32_DIR}/include
  )

  _INDI_check_version()

  find_library(INDI_LIBRARIES NAMES indi
    PATHS
    ${PC_INDI_LIBRARY_DIRS}
    ${_obLinkDir}
    ${GNUWIN32_DIR}/lib
  )

message("INDI LIB " ${INDI_LIBRARIES})

  find_library(INDI_CLIENT_LIBRARIES NAMES indiclient
    PATHS
    ${PC_INDI_LIBRARY_DIRS}
    ${_obLinkDir}
    ${GNUWIN32_DIR}/lib
  )

message("INDI CLIENT LIB " ${INDI_CLIENT_LIBRARIES})

  if (INDI_INCLUDE_DIR AND INDI_LIBRARIES AND INDI_CLIENT_LIBRARIES)
    set(INDI_FOUND TRUE)
  else (INDI_INCLUDE_DIR AND INDI_LIBRARIES AND INDI_CLIENT_LIBRARIES)
    set(INDI_FOUND FALSE)
  endif (INDI_INCLUDE_DIR AND INDI_LIBRARIES AND INDI_CLIENT_LIBRARIES)

  if (INDI_FOUND)
    if (NOT INDI_FIND_QUIETLY)
      message(STATUS "Found INDI: ${INDI_LIBRARIES}, ${INDI_CLIENT_LIBRARIES}, ${INDI_INCLUDE_DIR}")
    endif (NOT INDI_FIND_QUIETLY)
  else (INDI_FOUND)
    if (INDI_FIND_REQUIRED)
      message(FATAL_ERROR "INDI not found. Please install INDI and try again.")
    endif (INDI_FIND_REQUIRED)
  endif (INDI_FOUND)

  mark_as_advanced(INDI_INCLUDE_DIR INDI_LIBRARIES INDI_CLIENT_LIBRARIES)

endif (INDI_INCLUDE_DIR AND INDI_LIBRARIES AND INDI_CLIENT_LIBRARIES)
