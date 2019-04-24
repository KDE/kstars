# - Try to find XPlanet
# Once done this will define
#
#  XPLANET_FOUND - system has XPlanet
#  XPLANET_EXECUTABLE - the XPlanet executable
#
# Copyright (c) 2008, Jerome SONRIER, <jsid@emor3j.fr.eu.org>
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

