# - Try to find NOVA
# Once done this will define
#
#  NOVA_FOUND - system has NOVA
#  NOVA_INCLUDE_DIR - the NOVA include directory
#  NOVA_LIBRARIES - Link these to use NOVA

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

if (NOVA_INCLUDE_DIR AND NOVA_LIBRARIES)

  # in cache already
  set(NOVA_FOUND TRUE)
  message(STATUS "Found libnova: ${NOVA_LIBRARIES}")

else (NOVA_INCLUDE_DIR AND NOVA_LIBRARIES)

  find_path(NOVA_INCLUDE_DIR libnova.h
    if(ANDROID)
      ${BUILD_KSTARSLITE_DIR}/include
    endif(ANDROID)
    PATH_SUFFIXES libnova
    ${_obIncDir}
    ${GNUWIN32_DIR}/include
  )

  find_library(NOVA_LIBRARIES NAMES nova libnova libnovad
    PATHS
    if(ANDROID)
    ${BUILD_KSTARSLITE_DIR}/android_libs/${ANDROID_ARCHITECTURE}/
    else(ANDROID)
    ${_obLinkDir}
    ${GNUWIN32_DIR}/lib
    endif(ANDROID)
  )

 set(CMAKE_REQUIRED_INCLUDES ${NOVA_INCLUDE_DIR})
 set(CMAKE_REQUIRED_LIBRARIES ${NOVA_LIBRARIES})

   if(NOVA_INCLUDE_DIR AND NOVA_LIBRARIES)
    set(NOVA_FOUND TRUE)
  else (NOVA_INCLUDE_DIR AND NOVA_LIBRARIES)
    set(NOVA_FOUND FALSE)
  endif(NOVA_INCLUDE_DIR AND NOVA_LIBRARIES)

  if (NOVA_FOUND)
    if (NOT Nova_FIND_QUIETLY)
      message(STATUS "Found NOVA: ${NOVA_LIBRARIES}")
    endif (NOT Nova_FIND_QUIETLY)
  else (NOVA_FOUND)
    if (Nova_FIND_REQUIRED)
      message(FATAL_ERROR "libnova not found. Please install libnova development package.")
    endif (Nova_FIND_REQUIRED)
  endif (NOVA_FOUND)

  mark_as_advanced(NOVA_INCLUDE_DIR NOVA_LIBRARIES)

endif (NOVA_INCLUDE_DIR AND NOVA_LIBRARIES)
