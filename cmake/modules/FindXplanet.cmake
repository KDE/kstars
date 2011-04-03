# - Try to find XPlanet
# Once done this will define
#
#  XPLANET_FOUND - system has XPlanet
#  XPLANET_EXECUTABLE - the XPlanet executable
#
# Copyright (c) 2008, Jerome SONRIER, <jsid@emor3j.fr.eu.org>
# Redistribution and use is allowed according to the terms of the BSD license.
# For details see the accompanying COPYING-CMAKE-SCRIPTS file.
#
# THIS FILE NEEDS IMPROVEMENTS
#

# if ( XPLANET_EXECUTABLE )
# # in cache already
# set(XPLANET_FOUND TRUE)
# message(STATUS "Found XPlanet (in cache): ${XPLANET_EXECUTABLE}")
# else ( XPLANET_EXECUTABLE )
    set(XPLANET_EXECUTABLE XPLANET_EXECUTABLE-NOTFOUND)
    find_program(XPLANET_EXECUTABLE NAMES xplanet)

    if ( XPLANET_EXECUTABLE )
        set(XPLANET_FOUND TRUE)
    else ( XPLANET_EXECUTABLE )
        set(XPLANET_FOUND FALSE)
        message(STATUS "xplanet not found.")
    endif( XPLANET_EXECUTABLE )

    if ( XPLANET_FOUND )
        if ( NOT XPLANET_FIND_QUIETLY )
            message(STATUS "Found Xplanet: ${XPLANET_EXECUTABLE}")
        endif ( NOT XPLANET_FIND_QUIETLY )
    else ( XPLANET_FOUND )
        if ( XPLANET_FIND_REQUIRED )
            message(FATAL_ERROR "xplanet is required.")
        endif ( XPLANET_FIND_REQUIRED )
    endif ( XPLANET_FOUND )

    mark_as_advanced ( XPLANET_FOUND )

# endif( XPLANET_EXECUTABLE )

