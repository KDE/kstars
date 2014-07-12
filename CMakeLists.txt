PROJECT(kstars CXX C)
set (KStars_VERSION_MAJOR 3)
set (KStars_VERSION_MINOR 0)
set (KStars_VERSION_REVISION 0)

# minimal requirements
cmake_minimum_required (VERSION 2.8.12 FATAL_ERROR)
set (QT_MIN_VERSION "5.2.0")

# Find includes in corresponding build directories
set(CMAKE_INCLUDE_CURRENT_DIR ON)
# Instruct CMake to run moc automatically when needed.
set(CMAKE_AUTOMOC ON)

find_package(ECM 0.0.9 REQUIRED NO_MODULE)
set(CMAKE_MODULE_PATH ${ECM_MODULE_PATH})
set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake/modules" ${CMAKE_MODULE_PATH})

find_package(Qt5 5.2 REQUIRED COMPONENTS Gui Qml Quick Xml Sql Svg OpenGL PrintSupport Test)
include(KDEInstallDirs)
include(KDECompilerSettings)
include(KDECMakeSettings)
include(ECMInstallIcons)
include(FeatureSummary)

# Load the frameworks we need
find_package(KF5 REQUIRED COMPONENTS
  Config
  DocTools
  GuiAddons
  WidgetsAddons
  NewStuff
  DBusAddons
  I18n
  Init
  JobWidgets
  KIO
  WindowSystem
  XmlGui
  Plotting
  TextEditor
  IconThemes
  Declarative
)

find_package(Eigen3 REQUIRED)
add_definitions(${EIGEN_DEFINITIONS})
include_directories(${EIGEN3_INCLUDE_DIR})

#PyKDE4 Package TODO? Is there KF5 Package?

find_package(CFitsio)
SET(HAVE_CFITSIO CFITSIO_FOUND)
set_package_properties(CFitsio PROPERTIES DESCRIPTION "FITS IO Library" URL "http://heasarc.gsfc.nasa.gov/fitsio/fitsio.html" TYPE OPTIONAL PURPOSE "Support for the FITS (Flexible Image Transport System) data format in KStars.")

find_package(INDI)
SET(HAVE_INDI INDI_FOUND)
set_package_properties(INDI PROPERTIES DESCRIPTION "Astronomical instrumentation control" URL "http://www.indilib.org" TYPE OPTIONAL PURPOSE "Support for controlling astronomical devices on Linux with KStars.")

find_package(WCSLIB)
set(HAVE_WCSLIB WCSLIB_FOUND)
set_package_properties(WCSLIB PROPERTIES DESCRIPTION "World Coordinate System library" URL "http://www.atnf.csiro.au/people/mcalabre/WCS" TYPE OPTIONAL PURPOSE "WCS enables KStars to read and process world coordinate systems in FITS header.")

# FIXME QJSON is not needed in Qt5
find_package(QJSON)

find_package(Xplanet)
set(HAVE_XPLANET XPLANET_FOUND)
set_package_properties(Xplanet PROPERTIES DESCRIPTION "Renders an image of all the major planets and most satellites" URL "http://xplanet.sourceforge.net" TYPE OPTIONAL PURPOSE "Gives KStars support for xplanet.")

find_package(AstrometryNet)
set(HAVE_ASTROMETRYNET ASTROMETRYNET_FOUND)
set_package_properties(OpenGL PROPERTIES DESCRIPTION "Astrometrics Library" URL "http://www.astrometry.net" TYPE OPTIONAL PURPOSE "Support for plate solving in KStars.")

find_package(OpenGL)
set_package_properties(OpenGL PROPERTIES DESCRIPTION "Open Graphics Library" URL "http://www.opengl.org" TYPE OPTIONAL PURPOSE "Support for hardware rendering in KStars.")

add_definitions(-DQT_USE_FAST_CONCATENATION -DQT_USE_FAST_OPERATOR_PLUS -Dfexceptions)

enable_testing(true)

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/config-kstars.h.cmake
               ${CMAKE_CURRENT_BINARY_DIR}/config-kstars.h )

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/kstars/version.h.cmake
               ${CMAKE_CURRENT_BINARY_DIR}/kstars/version.h )

# let our config.h be found first in any case
include_directories (BEFORE ${CMAKE_CURRENT_BINARY_DIR})

# FIXME should we explicity include it?
include_directories(${CMAKE_CURRENT_SOURCE_DIR})

add_subdirectory(doc)
add_subdirectory(kstars)
add_subdirectory(datahandlers)

# FIXME Enable Tests later!
#add_subdirectory(Tests)

install(FILES kstars.appdata.xml DESTINATION ${SHARE_INSTALL_PREFIX}/appdata/)

#if (${CMAKE_CURRENT_SOURCE_DIR} STREQUAL ${CMAKE_SOURCE_DIR})
#   macro_display_feature_log()
#endif (${CMAKE_CURRENT_SOURCE_DIR} STREQUAL ${CMAKE_SOURCE_DIR})

feature_summary(WHAT ALL FATAL_ON_MISSING_REQUIRED_PACKAGES)
