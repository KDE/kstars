# - Try to find WCSLIB
# Once done this will define
#
#  WCSLIB_FOUND - system has WCSLIB
#  WCSLIB_INCLUDE_DIR - the WCSLIB include directory
#  WCSLIB_LIBRARIES - Link these to use WCSLIB

# Copyright (c) 2006, Jasem Mutlaq <mutlaqja@ikarustech.com>
# Based on FindLibfacile by Carsten Niehaus, <cniehaus@gmx.de>
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
#
# 1. Redistributions of source code must retain the copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
# 3. The name of the author may not be used to endorse or promote products
#    derived from this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
# IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
# OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
# IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
# INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
# NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
# THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

if (WCSLIB_INCLUDE_DIR AND WCSLIB_LIBRARIES)

  # in cache already
  set(WCSLIB_FOUND TRUE)
  message(STATUS "Found WCSLIB: ${WCSLIB_LIBRARIES}, ${WCSLIB_INCLUDE_DIR}")

else (WCSLIB_INCLUDE_DIR AND WCSLIB_LIBRARIES)

  if (NOT WIN32)
    find_package(PkgConfig)
    if (PKG_CONFIG_FOUND)
      pkg_check_modules(PC_WCSLIB wcslib)
    endif (PKG_CONFIG_FOUND)
  endif (NOT WIN32)

  find_path(WCSLIB_INCLUDE_DIR wcs.h
    PATH_SUFFIXES wcslib
    ${PC_WCSLIB_INCLUDE_DIRS}
    ${_obIncDir}
    ${GNUWIN32_DIR}/include
  )
  
  find_library(WCSLIB_LIBRARIES NAMES wcs wcslib
    PATHS
    ${PC_WCSLIB_LIBRARY_DIRS}
    ${_obLinkDir}
    ${GNUWIN32_DIR}/lib
  )
  
  if(WCSLIB_INCLUDE_DIR AND WCSLIB_LIBRARIES)
    set(WCSLIB_FOUND TRUE)
  else (WCSLIB_INCLUDE_DIR AND WCSLIB_LIBRARIES)
    set(WCSLIB_FOUND FALSE)
  endif(WCSLIB_INCLUDE_DIR AND WCSLIB_LIBRARIES)


  if (WCSLIB_FOUND)
    if (NOT WCSLIB_FIND_QUIETLY)
      message(STATUS "Found WCSLIB: ${WCSLIB_LIBRARIES}, ${WCSLIB_INCLUDE_DIR}")
    endif (NOT WCSLIB_FIND_QUIETLY)
  else (WCSLIB_FOUND)
    if (WCSLIB_FIND_REQUIRED)
      message(FATAL_ERROR "WCSLIB not found. Please install wcslib and try again.")
    endif (WCSLIB_FIND_REQUIRED)
  endif (WCSLIB_FOUND)

  mark_as_advanced(WCSLIB_INCLUDE_DIR WCSLIB_LIBRARIES)

endif (WCSLIB_INCLUDE_DIR AND WCSLIB_LIBRARIES)
