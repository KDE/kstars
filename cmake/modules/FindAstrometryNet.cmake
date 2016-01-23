# - Try to find astrometry.net
# Once done this will define
#
#  ASTROMETRYNET_FOUND - system has ASTROMETRYNET
#  ASTROMETRYNET_EXECUTABLE - the primary astrometry.net executable

# Copyright (c) 2016, Jasem Mutlaq <mutlaqja@ikarustech.com>
# Based on FindLibfacile by Carsten Niehaus, <cniehaus@gmx.de>
#
# Redistribution and use is allowed according to the terms of the BSD license.
# For details see the accompanying COPYING-CMAKE-SCRIPTS file.

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
