# - Try to find CFITSIO
# Once done this will define
#
#  CFITSIO_FOUND - system has CFITSIO
#  CFITSIO_INCLUDE_DIR - the CFITSIO include directory
#  CFITSIO_LIBRARIES - Link these to use CFITSIO

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

if (CFITSIO_INCLUDE_DIR AND CFITSIO_LIBRARIES)

  # in cache already
  set(CFITSIO_FOUND TRUE)
  message(STATUS "Found CFITSIO: ${CFITSIO_LIBRARIES}")


else (CFITSIO_INCLUDE_DIR AND CFITSIO_LIBRARIES)

  if (NOT WIN32)
    find_package(PkgConfig)
    if (PKG_CONFIG_FOUND)
      pkg_check_modules(PC_CFITSIO cfitsio)
    endif (PKG_CONFIG_FOUND)
  endif (NOT WIN32)

    if(ANDROID)
        find_path(CFITSIO_INCLUDE_DIR fitsio.h
            ${BUILD_KSTARSLITE_DIR}/include
            NO_DEFAULT_PATH
        )
    else(ANDROID)
        find_path(CFITSIO_INCLUDE_DIR fitsio.h
            ${PC_CFITSIO_INCLUDE_DIRS}
            ${_obIncDir}
            ${GNUWIN32_DIR}/include
        )
    endif(ANDROID)

  find_library(CFITSIO_LIBRARIES NAMES cfitsio libcfitsio
    PATHS
    if(ANDROID)
          ${BUILD_KSTARSLITE_DIR}/android_libs/${ANDROID_ARCHITECTURE}
    else(ANDROID)
        ${PC_CFITSIO_LIBRARY_DIRS}
        ${_obIncDir}
        ${GNUWIN32_DIR}/include
    endif(ANDROID)
  )

  if(CFITSIO_INCLUDE_DIR AND CFITSIO_LIBRARIES)
    set(CFITSIO_FOUND TRUE)
  else (CFITSIO_INCLUDE_DIR AND CFITSIO_LIBRARIES)
    set(CFITSIO_FOUND FALSE)
  endif(CFITSIO_INCLUDE_DIR AND CFITSIO_LIBRARIES)

  if (CFITSIO_FOUND)
    if (NOT CFitsio_FIND_QUIETLY)
      message(STATUS "Found CFITSIO: ${CFITSIO_LIBRARIES}")
    endif (NOT CFitsio_FIND_QUIETLY)
  else (CFITSIO_FOUND)
    if (CFitsio_FIND_REQUIRED)
      message(FATAL_ERROR "CFITSIO not found. Please install libcfitsio-dev and try again.")
    endif (CFitsio_FIND_REQUIRED)
  endif (CFITSIO_FOUND)

  mark_as_advanced(CFITSIO_INCLUDE_DIR CFITSIO_LIBRARIES)

endif (CFITSIO_INCLUDE_DIR AND CFITSIO_LIBRARIES)
