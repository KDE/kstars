PROJECT(kstars CXX C)
set (KStars_VERSION_MAJOR 2)
set (KStars_VERSION_MINOR 7)
set (KStars_VERSION_REVISION 3)
set (CMAKE_CXX_STANDARD 11)
#Build KStars Lite with -DKSTARS_LITE=ON
option(BUILD_KSTARS_LITE "Build KStars Lite" OFF)
if(BUILD_KSTARS_LITE)
    add_definitions(-DKSTARS_LITE)
endif(BUILD_KSTARS_LITE)
# minimal requirements
cmake_minimum_required (VERSION 2.8.12 FATAL_ERROR)

if(BUILD_KSTARS_LITE)
    set (QT_MIN_VERSION "5.7.0") #QtQuickControls 2 is available only in the Qt 5.7.0
else()
    set (QT_MIN_VERSION "5.4.0")
endif()

# Find includes in corresponding build directories
set(CMAKE_INCLUDE_CURRENT_DIR ON)
# Instruct CMake to run moc automatically when needed.
set(CMAKE_AUTOMOC ON)

if(CMAKE_SYSTEM_NAME STREQUAL Android)
    add_definitions(-DANDROID )
    set(ANDROID TRUE)
endif()

find_package(ECM 1.7.0 REQUIRED NO_MODULE)
set(CMAKE_MODULE_PATH ${ECM_MODULE_PATH})
set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake/modules" ${CMAKE_MODULE_PATH})

if(BUILD_KSTARS_LITE)
    find_package(Qt5 5.7 REQUIRED COMPONENTS Gui Qml Quick QuickControls2 Xml Svg Sql Network Positioning Concurrent)
else()
    find_package(Qt5 5.4 REQUIRED COMPONENTS Gui Qml Quick Xml Sql Svg Network PrintSupport Concurrent)
endif()
include(ECMInstallIcons)
include(ECMAddAppIcon)
include(KDEInstallDirs)
include(MacroBoolTo01)
if(NOT ANDROID)
    #We get error "could not find main method" with this module
    include(KDECompilerSettings NO_POLICY_SCOPE)
endif()
include(KDECMakeSettings)
include(FeatureSummary)

# Load the frameworks we need
if(BUILD_KSTARS_LITE)
    find_package(KF5 REQUIRED COMPONENTS
      Config
      I18n
      Plotting
    )
if(ANDROID)
    find_package(OpenMP REQUIRED) #Needed for LibRaw
    if(OPENMP_FOUND)
         set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${OpenMP_C_FLAGS}")
         set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${OpenMP_CXX_FLAGS}")
         set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} ${OpenMP_EXE_LINKER_FLAGS}")
      endif()
     include_directories(SYSTEM ${OpenMP_INCLUDE_PATH})
endif()
else(BUILD_KSTARS_LITE)
    # Find Optional package NotifyConfig for desktop notifications
    find_package(KF5 COMPONENTS NotifyConfig)
    MACRO_BOOL_TO_01(KF5NotifyConfig_FOUND HAVE_NOTIFYCONFIG)

    find_package(KF5 REQUIRED COMPONENTS
      Config
      Crash
      DocTools
      WidgetsAddons
      NewStuff
      I18n
      KIO
      XmlGui
      Plotting
      Notifications      
    )    
endif(BUILD_KSTARS_LITE)

## Eigen3 Library
find_package(Eigen3 REQUIRED)
add_definitions(${EIGEN_DEFINITIONS})
include_directories(${EIGEN3_INCLUDE_DIR})

## CFITSIO Library
if(BUILD_KSTARS_LITE)
    find_package(CFitsio REQUIRED)
else()
    find_package(CFitsio)
endif()
MACRO_BOOL_TO_01(CFITSIO_FOUND HAVE_CFITSIO)
set_package_properties(CFitsio PROPERTIES DESCRIPTION "FITS IO Library" URL "http://heasarc.gsfc.nasa.gov/fitsio/fitsio.html" TYPE OPTIONAL PURPOSE "Support for the FITS (Flexible Image Transport System) data format in KStars.")

## INDI Library
find_package(INDI 1.3.1)
MACRO_BOOL_TO_01(INDI_FOUND HAVE_INDI)
set_package_properties(INDI PROPERTIES DESCRIPTION "Astronomical instrumentation control" URL "http://www.indilib.org" TYPE OPTIONAL PURPOSE "Support for controlling astronomical devices on Linux with KStars.")

## Libraw
find_package(LibRaw)
MACRO_BOOL_TO_01(LibRaw_FOUND HAVE_LIBRAW)
set_package_properties(LibRaw PROPERTIES DESCRIPTION "Library for reading RAW files" URL "http://www.libraw.org" TYPE OPTIONAL PURPOSE "Support for reading and displaying RAW files in KStars.")

## WCS Library
find_package(WCSLIB)
MACRO_BOOL_TO_01(WCSLIB_FOUND HAVE_WCSLIB)
set_package_properties(WCSLIB PROPERTIES DESCRIPTION "World Coordinate System library" URL "http://www.atnf.csiro.au/people/mcalabre/WCS" TYPE OPTIONAL PURPOSE "WCS enables KStars to read and process world coordinate systems in FITS header.")

## XPlanet
find_package(Xplanet)
MACRO_BOOL_TO_01(XPLANET_FOUND HAVE_XPLANET)
set_package_properties(Xplanet PROPERTIES DESCRIPTION "Renders an image of all the major planets and most satellites" URL "http://xplanet.sourceforge.net" TYPE OPTIONAL PURPOSE "Gives KStars support for xplanet.")

## Astrometry.net
find_package(AstrometryNet)
MACRO_BOOL_TO_01(ASTROMETRYNET_FOUND HAVE_ASTROMETRYNET)
set_package_properties(AstrometryNet PROPERTIES DESCRIPTION "Astrometrics Library" URL "http://www.astrometry.net" TYPE OPTIONAL PURPOSE "Support for plate solving in KStars.")

## OpenGL
find_package(OpenGL)
set_package_properties(OpenGL PROPERTIES DESCRIPTION "Open Graphics Library" URL "http://www.opengl.org" TYPE OPTIONAL PURPOSE "Support for hardware rendering in KStars.")

if (${KF5_VERSION} VERSION_GREATER 5.17.0)
SET(HAVE_KF5WIT 1)
else()
SET(HAVE_KF5WIT 0)
endif()

add_definitions(-DQT_USE_FAST_CONCATENATION -DQT_USE_FAST_OPERATOR_PLUS)
add_definitions(-DQT_NO_URL_CAST_FROM_STRING)
if (CMAKE_BUILD_TYPE STREQUAL "Debug")
  add_definitions(-DQT_STRICT_ITERATORS)
endif()

if (UNIX)
add_definitions(-Dfexceptions)

# TEMPORARY: To disable QCustomPlot warning until 2.0.0 is released which fixes these warnings
add_definitions(-Wno-non-virtual-dtor)
endif(UNIX)

configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/config-kstars.h.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/config-kstars.h
)

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/kstars/version.h.cmake
               ${CMAKE_CURRENT_BINARY_DIR}/kstars/version.h )

# let our config.h be found first in any case
include_directories (BEFORE ${CMAKE_CURRENT_BINARY_DIR})

add_subdirectory(doc)
add_subdirectory(kstars)
add_subdirectory(datahandlers)

if(NOT BUILD_KSTARS_LITE)
    enable_testing(true)
    add_subdirectory(Tests)
endif(NOT BUILD_KSTARS_LITE)

install(FILES org.kde.kstars.appdata.xml DESTINATION ${KDE_INSTALL_METAINFODIR})

feature_summary(WHAT ALL FATAL_ON_MISSING_REQUIRED_PACKAGES)
