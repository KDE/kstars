# - Try to find OpenCV
# Once done this will define
#
#  OpenCV_FOUND - system has OpenCV
#  OpenCV_INCLUDE_DIRS - the OpenCV include directories (plural)
#  OpenCV_INCLUDE_DIR - the first OpenCV include directory (singular, for compatibility)
#  OpenCV_LIBRARIES - Link these to use OpenCV
#  OpenCV_VERSION - The version of OpenCV found
#
# Currently the following OpenCV libraries are sought:
#   opencv_core
#   opencv_imgproc
#   opencv_highgui
#
# This module follows a layered approach:
# 1. Tries find_package(OpenCV CONFIG) - modern CMake, works for all platforms if config files exist.
# 2. If CONFIG fails AND NOT on Windows, tries PkgConfig.
# 3. If still not found (or on Windows and CONFIG failed), performs a manual search.
#    - Manual search uses hints like $ENV{OpenCV_DIR}, GNUWIN32_DIR (Windows), _obIncDir, _obLinkDir.
#    - Attempts to parse version.hpp to find versioned library names.

# SPDX-FileCopyrightText: 2024 John Evans <john.e.evans.email@googlemail.com>
# SPDX-FileCopyrightText: 2024 Cline (AI Software Engineer)
#
# Based on FindLibfacile:
# SPDX-FileCopyrightText: Carsten Niehaus, <cniehaus@gmx.de>
#
# SPDX-License-Identifier: BSD-3-Clause

# Clear previous results if any, to ensure a clean find operation upon re-configuration
unset(OpenCV_LIBRARIES CACHE)
unset(OpenCV_INCLUDE_DIRS CACHE)
unset(OpenCV_INCLUDE_DIR CACHE)
unset(OpenCV_VERSION CACHE)
unset(OpenCV_FOUND CACHE)
unset(OpenCV_core_LIBRARY CACHE)
unset(OpenCV_imgproc_LIBRARY CACHE)
unset(OpenCV_highgui_LIBRARY CACHE)

if(DEFINED OpenCV_LIBRARIES AND DEFINED OpenCV_INCLUDE_DIRS AND OpenCV_FOUND)
  # Already in cache and found, likely from a previous successful run in the same CMake configure pass
  # or if the user explicitly set them.
  if(OpenCV_INCLUDE_DIRS AND NOT OpenCV_INCLUDE_DIR)
    list(GET OpenCV_INCLUDE_DIRS 0 OpenCV_INCLUDE_DIR)
  endif()
  if(NOT OpenCV_FIND_QUIETLY)
    string(REPLACE ";" "\n    " OpenCV_INCLUDE_DIRS_CACHED_FORMATTED "${OpenCV_INCLUDE_DIRS}")
    string(REPLACE ";" "\n    " OpenCV_LIBRARIES_CACHED_FORMATTED "${OpenCV_LIBRARIES}")
    message(STATUS "Found OpenCV (cached): Version ${OpenCV_VERSION}\n  Includes:\n    ${OpenCV_INCLUDE_DIRS_CACHED_FORMATTED}\n  Libraries:\n    ${OpenCV_LIBRARIES_CACHED_FORMATTED}")
  endif()
else()
  set(OpenCV_CMAKE_CONFIG_FOUND FALSE) # Initialize: TRUE if found by CONFIG, FALSE otherwise
  # 1. Attempt find_package(OpenCV CONFIG) - Works on all platforms if config files are present
  # It will search for OpenCVConfig.cmake or opencv-config.cmake
  # Users can hint this with OpenCV_DIR or by adding to CMAKE_PREFIX_PATH.
  find_package(OpenCV QUIET CONFIG)

  if(OpenCV_FOUND)
    if(OpenCV_INCLUDE_DIRS AND NOT OpenCV_INCLUDE_DIR)
      list(GET OpenCV_INCLUDE_DIRS 0 OpenCV_INCLUDE_DIR)
    elseif(OpenCV_INCLUDE_DIR AND NOT OpenCV_INCLUDE_DIRS)
      set(OpenCV_INCLUDE_DIRS ${OpenCV_INCLUDE_DIR})
    endif()
    # OpenCV_LIBRARIES and OpenCV_VERSION are set by OpenCV's own config files.
    # It often sets component targets like OpenCV::core, OpenCV::imgproc.
    # For compatibility with scripts expecting OpenCV_LIBRARIES variable, ensure it's populated if possible.
    # If OpenCV_LIBRARIES is not set by the config but component targets are, we might need to list them.
    # However, modern usage prefers linking against targets like OpenCV::core.
    # For this script, we'll assume OpenCV_LIBRARIES is sufficiently populated by the config or we'll build it.
    set(OpenCV_CMAKE_CONFIG_FOUND TRUE) # Mark that CONFIG mode found it
    # Prioritize specific targets KStars needs if they exist
    if(TARGET OpenCV::core AND TARGET OpenCV::imgproc AND TARGET OpenCV::highgui)
      set(OpenCV_LIBRARIES OpenCV::core OpenCV::imgproc OpenCV::highgui)
    elseif(NOT OpenCV_LIBRARIES)
      # Config found OpenCV, but OpenCV_LIBRARIES is empty and not all required targets exist.
      # This is an unusual state. Let FPHSA handle it based on REQUIRED_VARS.
      if(NOT OpenCV_FIND_QUIETLY)
        message(WARNING "OpenCV found by CONFIG, but OpenCV_LIBRARIES is empty and not all required targets (OpenCV::core, OpenCV::imgproc, OpenCV::highgui) are defined. Check OpenCV installation.")
      endif()
    endif()
    # If OpenCV_LIBRARIES was already set by config (e.g. to a list of all components),
    # and the specific targets exist, we've now overridden it with the specific targets.
    if(NOT OpenCV_FIND_QUIETLY)
      string(REPLACE ";" "\n    " OpenCV_INCLUDE_DIRS_CONFIG_FORMATTED "${OpenCV_INCLUDE_DIRS}")
      string(REPLACE ";" "\n    " OpenCV_LIBRARIES_CONFIG_FORMATTED "${OpenCV_LIBRARIES}")
      message(STATUS "Found OpenCV (CONFIG): Version ${OpenCV_VERSION}\n  Includes:\n    ${OpenCV_INCLUDE_DIRS_CONFIG_FORMATTED}\n  Libraries:\n    ${OpenCV_LIBRARIES_CONFIG_FORMATTED}")
    endif()
  else()
    # OpenCV_CMAKE_CONFIG_FOUND remains FALSE
    if(NOT OpenCV_FIND_QUIETLY)
      message(STATUS "OpenCVConfig.cmake not found or OpenCV_DIR not set, trying other methods.")
    endif()
    set(OpenCV_FOUND FALSE) # Explicitly set to FALSE before trying other methods

    if (NOT WIN32 AND NOT APPLE) # PkgConfig is common on Linux/BSD, less so on macOS for OpenCV
      # 2. On non-Windows/non-Apple, try PkgConfig as the next best option
      find_package(PkgConfig QUIET)
      if (PKG_CONFIG_FOUND)
        pkg_check_modules(PC_OPENCV QUIET opencv4) # opencv4 is common for OpenCV 4+
        if (PC_OPENCV_FOUND)
          set(OpenCV_LIBRARIES ${PC_OPENCV_LIBRARIES}) # This might be ;-separated list of full paths or -llibname
          set(OpenCV_INCLUDE_DIRS ${PC_OPENCV_INCLUDE_DIRS})
          set(OpenCV_VERSION ${PC_OPENCV_VERSION})
          set(OpenCV_FOUND TRUE)
          if(OpenCV_INCLUDE_DIRS)
            list(GET OpenCV_INCLUDE_DIRS 0 OpenCV_INCLUDE_DIR)
          endif()
          if(NOT OpenCV_FIND_QUIETLY)
            string(REPLACE ";" "\n    " OpenCV_INCLUDE_DIRS_PKG_FORMATTED "${OpenCV_INCLUDE_DIRS}")
            string(REPLACE ";" "\n    " OpenCV_LIBRARIES_PKG_FORMATTED "${OpenCV_LIBRARIES}")
            message(STATUS "Found OpenCV (PkgConfig): Version ${OpenCV_VERSION}\n  Includes:\n    ${OpenCV_INCLUDE_DIRS_PKG_FORMATTED}\n  Libraries:\n    ${OpenCV_LIBRARIES_PKG_FORMATTED}")
          endif()
        else()
          if(NOT OpenCV_FIND_QUIETLY)
            message(STATUS "PkgConfig found, but 'opencv4.pc' not found or failed. Trying manual search.")
          endif()
        endif()
      else()
        if(NOT OpenCV_FIND_QUIETLY)
          message(STATUS "PkgConfig not found. Trying manual search.")
        endif()
      endif()
    endif() # NOT WIN32 AND NOT APPLE

    if (NOT OpenCV_FOUND)
      # 3. Fallback to manual search
      if(NOT OpenCV_FIND_QUIETLY)
        message(STATUS "Attempting manual search for OpenCV.")
      endif()

      set(OpenCV_MANUAL_INCLUDE_HINTS
        $ENV{OpenCV_DIR}
        ${_obIncDir} # From original KStars script
      )
      if(WIN32 AND DEFINED GNUWIN32_DIR)
        list(APPEND OpenCV_MANUAL_INCLUDE_HINTS "${GNUWIN32_DIR}/include")
      elseif(APPLE)
        list(APPEND OpenCV_MANUAL_INCLUDE_HINTS "/usr/local/opt/opencv/include" "/opt/homebrew/opt/opencv/include")
      else() # Generic Unix
        list(APPEND OpenCV_MANUAL_INCLUDE_HINTS "/usr/local/include" "/usr/include" "/opt/local/include")
      endif()

      find_path(OpenCV_MANUAL_INCLUDE_DIR opencv2/opencv.hpp
        HINTS ${OpenCV_MANUAL_INCLUDE_HINTS}
        PATH_SUFFIXES opencv4 include/opencv4 include # Common suffixes
      )

      set(OpenCV_VERSION_FROM_HEADER "NOT_FOUND")
      set(OpenCV_VERSION_SUFFIX "") # e.g. 480 for version 4.8.0

      if(OpenCV_MANUAL_INCLUDE_DIR AND EXISTS "${OpenCV_MANUAL_INCLUDE_DIR}/opencv2/core/version.hpp")
        file(STRINGS "${OpenCV_MANUAL_INCLUDE_DIR}/opencv2/core/version.hpp" CV_MAJOR REGEX "^#define CV_VERSION_MAJOR[ \t]+[0-9]+")
        file(STRINGS "${OpenCV_MANUAL_INCLUDE_DIR}/opencv2/core/version.hpp" CV_MINOR REGEX "^#define CV_VERSION_MINOR[ \t]+[0-9]+")
        file(STRINGS "${OpenCV_MANUAL_INCLUDE_DIR}/opencv2/core/version.hpp" CV_PATCH REGEX "^#define CV_VERSION_REVISION[ \t]+[0-9]+") # OpenCV uses REVISION for patch

        string(REGEX REPLACE "^#define CV_VERSION_MAJOR[ \t]+([0-9]+)" "\\1" CV_MAJOR "${CV_MAJOR}")
        string(REGEX REPLACE "^#define CV_VERSION_MINOR[ \t]+([0-9]+)" "\\1" CV_MINOR "${CV_MINOR}")
        string(REGEX REPLACE "^#define CV_VERSION_REVISION[ \t]+([0-9]+)" "\\1" CV_PATCH "${CV_PATCH}")

        if(CV_MAJOR AND CV_MINOR AND CV_PATCH)
          set(OpenCV_VERSION_FROM_HEADER "${CV_MAJOR}.${CV_MINOR}.${CV_PATCH}")
          set(OpenCV_VERSION_SUFFIX "${CV_MAJOR}${CV_MINOR}${CV_PATCH}")
          if(NOT OpenCV_FIND_QUIETLY)
            message(STATUS "Parsed OpenCV version from header: ${OpenCV_VERSION_FROM_HEADER}")
          endif()
        else()
          if(NOT OpenCV_FIND_QUIETLY)
            message(STATUS "Could not parse OpenCV version from version.hpp.")
          endif()
        endif()
      else()
        if(NOT OpenCV_FIND_QUIETLY)
          message(STATUS "OpenCV version.hpp not found in ${OpenCV_MANUAL_INCLUDE_DIR}/opencv2/core/version.hpp. Will try common library names.")
        endif()
      endif()
      
      if (NOT OpenCV_VERSION) # If not set by CONFIG or PkgConfig
        set(OpenCV_VERSION ${OpenCV_VERSION_FROM_HEADER})
      endif()

      # Define library names to search for, with and without version suffix
      set(OpenCV_LIB_NAMES_core "opencv_core${OpenCV_VERSION_SUFFIX}" "opencv_core")
      set(OpenCV_LIB_NAMES_imgproc "opencv_imgproc${OpenCV_VERSION_SUFFIX}" "opencv_imgproc")
      set(OpenCV_LIB_NAMES_highgui "opencv_highgui${OpenCV_VERSION_SUFFIX}" "opencv_highgui")

      set(OpenCV_MANUAL_LIB_HINTS
        $ENV{OpenCV_DIR}
        ${_obLinkDir} # From original KStars script
      )
      if(WIN32 AND DEFINED GNUWIN32_DIR)
        list(APPEND OpenCV_MANUAL_LIB_HINTS "${GNUWIN32_DIR}/lib")
      elseif(APPLE)
        list(APPEND OpenCV_MANUAL_LIB_HINTS "/usr/local/opt/opencv/lib" "/opt/homebrew/opt/opencv/lib")
      else() # Generic Unix
        list(APPEND OpenCV_MANUAL_LIB_HINTS "/usr/local/lib" "/usr/lib" "/opt/local/lib")
      endif()
      
      # Add common library path suffixes, especially for Windows MSVC builds
      set(OpenCV_LIB_PATH_SUFFIXES lib)
      if(WIN32)
        # For MSVC, libraries might be in arch/compiler specific subdirs
        # e.g. x64/vc16/lib, x64/vc17/lib
        # This is hard to generalize perfectly without knowing the compiler version.
        # OpenCV_DIR (if set by user for a compiled build) should point to the root of such structure.
        # For Craft, the structure is like image-RelWithDebInfo-X.Y.Z/gitlab/craft/windows-msvc2019_64-cl/x64/vc16/lib
        # We rely on $ENV{OpenCV_DIR} or GNUWIN32_DIR to point to a base where these can be found.
        # Or that they are in standard system paths.
        list(APPEND OpenCV_LIB_PATH_SUFFIXES x64/vc17/lib x64/vc16/lib x64/vc15/lib) # Common MSVC versions
      endif()

      find_library(OpenCV_core_LIBRARY NAMES ${OpenCV_LIB_NAMES_core} HINTS ${OpenCV_MANUAL_LIB_HINTS} PATH_SUFFIXES ${OpenCV_LIB_PATH_SUFFIXES})
      find_library(OpenCV_imgproc_LIBRARY NAMES ${OpenCV_LIB_NAMES_imgproc} HINTS ${OpenCV_MANUAL_LIB_HINTS} PATH_SUFFIXES ${OpenCV_LIB_PATH_SUFFIXES})
      find_library(OpenCV_highgui_LIBRARY NAMES ${OpenCV_LIB_NAMES_highgui} HINTS ${OpenCV_MANUAL_LIB_HINTS} PATH_SUFFIXES ${OpenCV_LIB_PATH_SUFFIXES})

      if(OpenCV_MANUAL_INCLUDE_DIR AND OpenCV_core_LIBRARY AND OpenCV_imgproc_LIBRARY AND OpenCV_highgui_LIBRARY)
        set(OpenCV_FOUND TRUE)
        set(OpenCV_INCLUDE_DIR ${OpenCV_MANUAL_INCLUDE_DIR})
        set(OpenCV_INCLUDE_DIRS ${OpenCV_MANUAL_INCLUDE_DIR})
        set(OpenCV_LIBRARIES "")
        if(OpenCV_core_LIBRARY)
          list(APPEND OpenCV_LIBRARIES ${OpenCV_core_LIBRARY})
        endif()
        if(OpenCV_imgproc_LIBRARY)
          list(APPEND OpenCV_LIBRARIES ${OpenCV_imgproc_LIBRARY})
        endif()
        if(OpenCV_highgui_LIBRARY)
          list(APPEND OpenCV_LIBRARIES ${OpenCV_highgui_LIBRARY})
        endif()
        # OpenCV_VERSION was set earlier from header or other methods
        if(NOT OpenCV_FIND_QUIETLY)
          string(REPLACE ";" "\n    " OpenCV_INCLUDE_DIRS_MANUAL_FORMATTED "${OpenCV_INCLUDE_DIRS}")
          string(REPLACE ";" "\n    " OpenCV_LIBRARIES_MANUAL_FORMATTED "${OpenCV_LIBRARIES}")
          message(STATUS "Found OpenCV (manual): Version ${OpenCV_VERSION}\n  Includes:\n    ${OpenCV_INCLUDE_DIRS_MANUAL_FORMATTED}\n  Libraries:\n    ${OpenCV_LIBRARIES_MANUAL_FORMATTED}")
        endif()
      else()
        set(OpenCV_FOUND FALSE) # Ensure it's false if manual search fails
        if(NOT OpenCV_FIND_QUIETLY)
          message(STATUS "Manual search for OpenCV failed.")
        endif()
        # Warnings should typically always be shown
        if(NOT OpenCV_MANUAL_INCLUDE_DIR)
          message(WARNING "Manual search: OpenCV include directory NOT found (searched for opencv2/opencv.hpp).")
        endif()
        if(NOT OpenCV_core_LIBRARY)
          message(WARNING "Manual search: OpenCV core library NOT found (tried: ${OpenCV_LIB_NAMES_core}).")
        endif()
        if(NOT OpenCV_imgproc_LIBRARY)
          message(WARNING "Manual search: OpenCV imgproc library NOT found (tried: ${OpenCV_LIB_NAMES_imgproc}).")
        endif()
        if(NOT OpenCV_highgui_LIBRARY)
          message(WARNING "Manual search: OpenCV highgui library NOT found (tried: ${OpenCV_LIB_NAMES_highgui}).")
        endif()
      endif()
    endif() # NOT OpenCV_FOUND (after PkgConfig attempt or if on Windows/Apple)
  endif() # End of CONFIG search failed block

  # Standard handling for FOUND_VAR, REQUIRED_VARS, VERSION_VAR
  include(FindPackageHandleStandardArgs)
  find_package_handle_standard_args(OpenCV
      FOUND_VAR OpenCV_FOUND
      REQUIRED_VARS OpenCV_LIBRARIES OpenCV_INCLUDE_DIRS # OpenCV_INCLUDE_DIR will be derived
      VERSION_VAR OpenCV_VERSION
  )

  if(OpenCV_FOUND)
    if(OpenCV_INCLUDE_DIRS AND NOT OpenCV_INCLUDE_DIR)
      list(GET OpenCV_INCLUDE_DIRS 0 OpenCV_INCLUDE_DIR)
    elseif(OpenCV_INCLUDE_DIR AND NOT OpenCV_INCLUDE_DIRS)
      set(OpenCV_INCLUDE_DIRS ${OpenCV_INCLUDE_DIR})
    endif()

    # Create an imported target if OpenCVConfig.cmake didn't (e.g. manual or PkgConfig find)
    # Modern OpenCVConfig.cmake provides component targets like OpenCV::core
    # For simplicity, if we found manually or via PkgConfig, we can create a single INTERFACE target
    # if the user prefers to link against a single target.
    # However, the original script just set variables, so we'll stick to that primarily.
    # If specific component targets (e.g. OpenCV::core) are not defined by OpenCV's own config,
    # and we found the libraries, we could define them.
    # For now, ensure variables are primary.
    if(NOT TARGET OpenCV::core AND OpenCV_core_LIBRARY)
        add_library(OpenCV::core UNKNOWN IMPORTED)
        set_target_properties(OpenCV::core PROPERTIES IMPORTED_LOCATION "${OpenCV_core_LIBRARY}")
        if(OpenCV_INCLUDE_DIRS)
             set_target_properties(OpenCV::core PROPERTIES INTERFACE_INCLUDE_DIRECTORIES "${OpenCV_INCLUDE_DIRS}")
        endif()
    endif()
    if(NOT TARGET OpenCV::imgproc AND OpenCV_imgproc_LIBRARY)
        add_library(OpenCV::imgproc UNKNOWN IMPORTED)
        set_target_properties(OpenCV::imgproc PROPERTIES IMPORTED_LOCATION "${OpenCV_imgproc_LIBRARY}")
         if(OpenCV_INCLUDE_DIRS)
             set_target_properties(OpenCV::imgproc PROPERTIES INTERFACE_INCLUDE_DIRECTORIES "${OpenCV_INCLUDE_DIRS}")
        endif()
        if(TARGET OpenCV::core) # imgproc depends on core
            set_target_properties(OpenCV::imgproc PROPERTIES INTERFACE_LINK_LIBRARIES OpenCV::core)
        endif()
    endif()
    if(NOT TARGET OpenCV::highgui AND OpenCV_highgui_LIBRARY)
        add_library(OpenCV::highgui UNKNOWN IMPORTED)
        set_target_properties(OpenCV::highgui PROPERTIES IMPORTED_LOCATION "${OpenCV_highgui_LIBRARY}")
        if(OpenCV_INCLUDE_DIRS)
             set_target_properties(OpenCV::highgui PROPERTIES INTERFACE_INCLUDE_DIRECTORIES "${OpenCV_INCLUDE_DIRS}")
        endif()
        if(TARGET OpenCV::core) # highgui often depends on core and imgproc
             set_target_properties(OpenCV::highgui PROPERTIES INTERFACE_LINK_LIBRARIES OpenCV::core)
             if(TARGET OpenCV::imgproc)
                target_link_libraries(OpenCV::highgui INTERFACE OpenCV::imgproc)
             endif()
        endif()
    endif()

  endif()

  mark_as_advanced(
    OpenCV_INCLUDE_DIR OpenCV_INCLUDE_DIRS OpenCV_LIBRARIES OpenCV_VERSION
    OpenCV_core_LIBRARY OpenCV_imgproc_LIBRARY OpenCV_highgui_LIBRARY
    OpenCV_MANUAL_INCLUDE_DIR # Internal variable for manual search
  )

  include(FeatureSummary) # Safe to include multiple times
  if(OpenCV_FOUND AND NOT OpenCV_CMAKE_CONFIG_FOUND)
    # Only set properties if found by PkgConfig or Manually,
    # as CONFIG mode should provide its own.
    set_package_properties(OpenCV PROPERTIES
        URL "https://opencv.org/"
        DESCRIPTION "Open Source Computer Vision Library"
    )
  endif()

endif() # End of main if/else (OpenCV_LIBRARIES AND OpenCV_INCLUDE_DIRS defined check)
