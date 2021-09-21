# SPDX-FileCopyrightText: 2021 Heiko Becker <heiko.becker@kde.org>
#
# SPDX-License-Identifier: BSD-3-Clause

#[=======================================================================[.rst:
FindERFA
----------

Try to find the ERFA (Essential Routines for Fundamental Astronomy) library.

This will define the following variables:

``ERFA_FOUND``
      True if the system has the ERFA library of at least the minimum
      version specified by the version parameter to find_package()
``ERFA_INCLUDE_DIRS``
      The ERFA include dirs for use with target_include_directories
``ERFA_LIBRARIES``
      The ERFA libraries for use with target_link_libraries()
``ERFA_VERSION``
      The version of ERFA that was found

If ``ERFA_FOUND` is TRUE, it will also define the following imported
target:

``ERFA::ERFA``
      The ERFA library

#]=======================================================================]

find_package(PkgConfig QUIET)

pkg_check_modules(PC_ERFA QUIET erfa)

find_path(ERFA_INCLUDE_DIRS
    NAMES erfa.h
    HINTS ${PC_ERFA_INCLUDEDIR}
)

find_library(ERFA_LIBRARIES
    NAMES erfa
    HINTS ${PC_ERFA_LIBDIR}
)

set(ERFA_VERSION ${PC_ERFA_VERSION})

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(ERFA
    FOUND_VAR
        ERFA_FOUND
    REQUIRED_VARS
        ERFA_LIBRARIES
        ERFA_INCLUDE_DIRS
    VERSION_VAR
        ERFA_VERSION
)

if (ERFA_FOUND AND NOT TARGET ERFA::ERFA)
    add_library(ERFA::ERFA UNKNOWN IMPORTED)
    set_target_properties(ERFA::ERFA PROPERTIES
        IMPORTED_LOCATION "${ERFA_LIBRARIES}"
        INTERFACE_INCLUDE_DIRECTORIES "${ERFA_INCLUDE_DIRS}"
    )
endif()

mark_as_advanced(ERFA_LIBRARIES ERFA_INCLUDE_DIRS)

include(FeatureSummary)
set_package_properties(ERFA PROPERTIES
    URL "https://github.com/liberfa/erfa/"
    DESCRIPTION "Essential Routines for Fundamental Astronomy"
)
