# - Try to find OpenCV
# Once done this will define
#
#  OpenCV_FOUND - system has OpenCV
#  OpenCV_INCLUDE_DIR - the OpenCV include directory
#  OpenCV_LIBRARIES - Link these to use OpenCV
#
# Currently the following OpenCV libraries are used
#   opencv_core - core
#   opencv_imageproc - image processing routines, e.g. Sobel, Laplassian, etc
#   opencv_highgui - used for debug (when stable this could be removed)
#
# OpenCV is installed in /opencv4/opencv2/.. and the include path needs to be at the /opencv4 level
# to avoid compilation errors

# SPDX-FileCopyrightText: 2024 John Evans <john.e.evans.email@googlemail.com>
#
# Based on FindLibfacile:
# SPDX-FileCopyrightText: Carsten Niehaus, <cniehaus@gmx.de>
#
# SPDX-License-Identifier: BSD-3-Clause

if (OpenCV_INCLUDE_DIR AND OpenCV_LIBRARIES)

  # in cache already
  set(OpenCV_FOUND TRUE)
  message(STATUS "Found OpenCV: ${OpenCV_LIBRARIES}, ${OpenCV_INCLUDE_DIR}")

else (OpenCV_INCLUDE_DIR AND OpenCV_LIBRARIES)

  if (NOT WIN32)
    find_package(PkgConfig)
  endif (NOT WIN32)

  find_path(OpenCV_INCLUDE_DIR opencv2/opencv.hpp
    PATH_SUFFIXES opencv4
    ${PC_OPENCV_INCLUDE_DIRS}
    ${_obIncDir}
    ${GNUWIN32_DIR}/include
  )

  find_library(OpenCV_core_LIBRARY NAMES opencv_core
    PATHS
    ${PC_OPENCV_LIBRARY_DIRS}
    ${_obLinkDir}
    ${GNUWIN32_DIR}/lib
  )
  find_library(OpenCV_imgproc_LIBRARY NAMES opencv_imgproc
   PATHS
   ${PC_OPENCV_LIBRARY_DIRS}
   ${_obLinkDir}
   ${GNUWIN32_DIR}/lib
  )
  find_library(OpenCV_highgui_LIBRARY NAMES opencv_highgui
    PATHS
    ${PC_OPENCV_LIBRARY_DIRS}
    ${_obLinkDir}
    ${GNUWIN32_DIR}/lib
  )
  set(OpenCV_LIBRARIES
    ${OpenCV_core_LIBRARY}
    ${OpenCV_imgproc_LIBRARY}
    ${OpenCV_highgui_LIBRARY})

  if(OpenCV_INCLUDE_DIR AND OpenCV_LIBRARIES)
    set(OpenCV_FOUND TRUE)
  else (OpenCV_INCLUDE_DIR AND OpenCV_LIBRARIES)
    set(OpenCV_FOUND FALSE)
  endif(OpenCV_INCLUDE_DIR AND OpenCV_LIBRARIES)

  if (OpenCV_FOUND)
    if (NOT OpenCV_FIND_QUIETLY)
      message(STATUS "Found OpenCV: ${OpenCV_LIBRARIES}, ${OpenCV_INCLUDE_DIR}")
    endif (NOT OpenCV_FIND_QUIETLY)
  else (OpenCV_FOUND)
    if (OpenCV_FIND_REQUIRED)
      message(FATAL_ERROR "OpenCV not found. Please install OpenCV and try again.")
    endif (OpenCV_FIND_REQUIRED)
  endif (OpenCV_FOUND)

  mark_as_advanced(OpenCV_INCLUDE_DIR OpenCV_LIBRARIES)

endif (OpenCV_INCLUDE_DIR AND OpenCV_LIBRARIES)
