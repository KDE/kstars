# - Try to find SexySolver
# Once done this will define
#
#  SEXYSOLVER_FOUND - system has SexySolver
#  SEXYSOLVER_INCLUDE_DIR - the CFITSIO include directory
#  SEXYSOLVER_LIBRARIES - Link these to use CFITSIO
# Copyright (c) 2020, Robert Lancaster <rlancaste@gmail.com>
# Based on FindCFITSIO by Jasem Mutlaq <mutlaqja@ikarustech.com>
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

if (SEXYSOLVER_INCLUDE_DIR AND SEXYSOLVER_LIBRARIES)

  # in cache already
  set(SEXYSOLVER_FOUND TRUE)
  message(STATUS "Found SexySolver: ${SEXYSOLVER_LIBRARIES}")

else (SEXYSOLVER_INCLUDE_DIR AND SEXYSOLVER_LIBRARIES)

  if (NOT WIN32)
    find_package(PkgConfig)
    if (PKG_CONFIG_FOUND)
      pkg_check_modules(PC_SEXYSOLVER sexysolver)
    endif (PKG_CONFIG_FOUND)
  endif (NOT WIN32)

  find_path(SEXYSOLVER_INCLUDE_DIR sexysolver.h
        ${_obIncDir}
        ${PC_SEXYSOLVER_INCLUDE_DIRS}
        ${GNUWIN32_DIR}/include/sexysolver
        ${CMAKE_INSTALL_PREFIX}/include/sexysolver
  )

  find_library(SEXYSOLVER_LIBRARIES NAMES sexysolver libsexysolver
    PATHS
        ${_obLinkDir}
        ${PC_SEXYSOLVER_LIBRARY_DIRS}
        ${GNUWIN32_DIR}/lib
        ${CMAKE_INSTALL_PREFIX}/lib
  )

  if(SEXYSOLVER_INCLUDE_DIR AND SEXYSOLVER_LIBRARIES)
    set(SEXYSOLVER_FOUND TRUE)
  else (SEXYSOLVER_INCLUDE_DIR AND SEXYSOLVER_LIBRARIES)
    set(SEXYSOLVER_FOUND FALSE)
  endif(SEXYSOLVER_INCLUDE_DIR AND SEXYSOLVER_LIBRARIES)

  if (SEXYSOLVER_FOUND)
    if (NOT SEXYSOLVER_FIND_QUIETLY)
      message(STATUS "Found SexySolver: ${SEXYSOLVER_LIBRARIES}")
    endif (NOT SEXYSOLVER_FIND_QUIETLY)
  else (SEXYSOLVER_FOUND)
    if (SEXYSOLVER_FIND_REQUIRED)
      message(FATAL_ERROR "SexySolver not found. Please install libsexysolver and try again.")
    endif (SEXYSOLVER_FIND_REQUIRED)
  endif (SEXYSOLVER_FOUND)

  mark_as_advanced(SEXYSOLVER_INCLUDE_DIR SEXYSOLVER_LIBRARIES)

endif (SEXYSOLVER_INCLUDE_DIR AND SEXYSOLVER_LIBRARIES)
