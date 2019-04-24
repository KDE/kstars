# - Try to find astrometry.net
# Once done this will define
#
#  ASTROMETRYNET_FOUND - system has ASTROMETRYNET
#  ASTROMETRYNET_EXECUTABLE - the primary astrometry.net executable

# Copyright (c) 2016, Jasem Mutlaq <mutlaqja@ikarustech.com>
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

if (ASTROMETRYNET_EXECUTABLE)

  # in cache already
  set(ASTROMETRYNET_FOUND TRUE)
  message(STATUS "Found astrometry.net: ${ASTROMETRYNET_EXECUTABLE}")

else (ASTROMETRYNET_EXECUTABLE)

   set(ASTROMETRYNET_EXECUTABLE ASTROMETRYNET_EXECUTABLE-NOTFOUND)
   find_program(ASTROMETRYNET_EXECUTABLE NAMES solve-field)

  if(ASTROMETRYNET_EXECUTABLE)
    set(ASTROMETRYNET_FOUND TRUE)
  else (ASTROMETRYNET_EXECUTABLE)
    set(ASTROMETRYNET_FOUND FALSE)
  endif(ASTROMETRYNET_EXECUTABLE)

  if (ASTROMETRYNET_FOUND)
    if (NOT ASTROMETRYNET_FIND_QUIETLY)
      message(STATUS "Found astrometry.net: ${ASTROMETRYNET_EXECUTABLE}")
    endif (NOT ASTROMETRYNET_FIND_QUIETLY)
  else (ASTROMETRYNET_FOUND)
    if (ASTROMETRYNET_FIND_REQUIRED)
      message(FATAL_ERROR "astrometry.net not found. Please install astrometry.net and try again.")
    endif (ASTROMETRYNET_FIND_REQUIRED)
  endif (ASTROMETRYNET_FOUND)

endif (ASTROMETRYNET_EXECUTABLE)
