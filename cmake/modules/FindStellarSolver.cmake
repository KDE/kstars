# - Try to find StellarSolver
# Once done this will define
#
#  STELLARSOLVER_FOUND - system has StellarSolver
#  STELLARSOLVER_INCLUDE_DIR - the CFITSIO include directory
#  STELLARSOLVER_LIBRARIES - Link these to use CFITSIO
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

if (STELLARSOLVER_INCLUDE_DIR AND STELLARSOLVER_LIBRARIES)

  # in cache already
  set(STELLARSOLVER_FOUND TRUE)
  message(STATUS "Found StellarSolver: ${STELLARSOLVER_LIBRARIES}")

else (STELLARSOLVER_INCLUDE_DIR AND STELLARSOLVER_LIBRARIES)

  if (NOT WIN32)
    find_package(PkgConfig)
    if (PKG_CONFIG_FOUND)
      pkg_check_modules(PC_STELLARSOLVER stellarsolver)
    endif (PKG_CONFIG_FOUND)
  endif (NOT WIN32)

  find_path(STELLARSOLVER_INCLUDE_DIR stellarsolver.h
            ${_obIncDir}
            ${GNUWIN32_DIR}/include/libstellarsolver/
            /usr/include/libstellarsolver
            /usr/local/include/libstellarsolver
            /Users/rlancaste/AstroRoot/craft-root/include/libstellarsolver/
        )

  find_library(STELLARSOLVER_LIBRARIES NAMES stellarsolver libstellarsolver.a
    PATHS
        ${_obIncDir}
        ${GNUWIN32_DIR}/lib
	/usr/lib64
	/usr/lib
	/usr/lib32
	/usr/local/lib64
	/usr/local/lib
	/usr/local/lib32
        /Users/rlancaste/AstroRoot/craft-root/lib
  )

  if(STELLARSOLVER_INCLUDE_DIR AND STELLARSOLVER_LIBRARIES)
    set(STELLARSOLVER_FOUND TRUE)
  else (STELLARSOLVER_INCLUDE_DIR AND STELLARSOLVER_LIBRARIES)
    set(STELLARSOLVER_FOUND FALSE)
  endif(STELLARSOLVER_INCLUDE_DIR AND STELLARSOLVER_LIBRARIES)

  if (STELLARSOLVER_FOUND)
    if (NOT STELLARSOLVER_FIND_QUIETLY)
      message(STATUS "Found StellarSolver: ${STELLARSOLVER_LIBRARIES}")
    endif (NOT STELLARSOLVER_FIND_QUIETLY)
  else (STELLARSOLVER_FOUND)
    if (STELLARSOLVER_FIND_REQUIRED)
      message(FATAL_ERROR "StellarSolver not found. Please install libstellarsolver and try again.")
    endif (STELLARSOLVER_FIND_REQUIRED)
  endif (STELLARSOLVER_FOUND)

  mark_as_advanced(STELLARSOLVER_INCLUDE_DIR STELLARSOLVER_LIBRARIES)

endif (STELLARSOLVER_INCLUDE_DIR AND STELLARSOLVER_LIBRARIES)
