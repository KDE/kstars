PROJECT(kstars CXX C)
set (KStars_VERSION_MAJOR 2)
set (KStars_VERSION_MINOR 2)
set (KStars_VERSION_REVISION 0)

set(CMAKE_MODULE_PATH ${kstars_SOURCE_DIR}/cmake/modules ${CMAKE_MODULE_PATH})

configure_file( "${kstars_SOURCE_DIR}/kstars/version.h.cmake"
                "${kstars_BINARY_DIR}/kstars/version.h" )

find_package(Eigen3 REQUIRED)
find_package(KDE4   REQUIRED)
find_package(QJSON REQUIRED)

include(KDE4Defaults)
include_directories (${CMAKE_SOURCE_DIR}
                     ${CMAKE_BINARY_DIR}
                     ${EIGEN3_INCLUDE_DIR}
                     ${KDE4_INCLUDES}
                     ${QT4_INCLUDES})
add_definitions(${EIGEN_DEFINITIONS} ${QJSON_DEFINITIONS} ${qjson_DEFINITIONS} -DQT_USE_FAST_CONCATENATION -DQT_USE_FAST_OPERATOR_PLUS)

# some configure checks needed for kstars
include(CheckIncludeFiles)

macro_optional_find_package(CFitsio)
macro_bool_to_01(CFITSIO_FOUND HAVE_CFITSIO_H)
macro_log_feature(CFITSIO_FOUND "libcfitsio0"
                                "Support for the FITS (Flexible Image Transport System) data format in KStars."
                                "http://indi.sf.net"
                                FALSE "3.09" "")

macro_optional_find_package(INDI 0.9.6)
macro_bool_to_01(INDI_FOUND HAVE_INDI_H)
macro_log_feature(INDI_FOUND "libindi"
                             "Support for controlling astronomical devices on Linux with KStars."
                             "http://www.indilib.org"
                             FALSE "0.9.6" "")

macro_optional_find_package(WCSLIB)
macro_bool_to_01(WCSLIB_FOUND HAVE_WCSLIB)
macro_log_feature(WCSLIB_FOUND "WCSLib"
                               "Support for World Coordinate System in KStars."
                               "http://www.atnf.csiro.au/people/mcalabre/WCS/"
                               FALSE "" "WCS enables KStars to read and process world coordinate systems in FITS header.")

macro_optional_find_package(AstrometryNet)
macro_bool_to_01(ASTROMETRYNET_FOUND HAVE_ASTROMETRYNET)
macro_log_feature(ASTROMETRYNET_FOUND "astrometry.net" "Support for astrometry solver in KStars." "http://www.astrometry.net" FALSE "" "Astrometry.net enables KStars to achieve sub-arcsecond GOTO tracking and determines polar alignment errors in the mount.")

# Temporary dependency until we move to Qt5 which has native support for JSon.
macro_optional_find_package(QJSON)
macro_bool_to_01(QJSON_FOUND HAVE_QJSON)
macro_log_feature(QJSON_FOUND "QJSon Library" "Support for online astrometry solver in KStars using web services." "http://qjson.sourceforge.net" FALSE "" "QJSon enables KStars to utilize online web services to perform astrometric solving for images in Ekos alignment module.")

macro_optional_find_package(Xplanet)
macro_bool_to_01(XPLANET_FOUND HAVE_XPLANET)
macro_log_feature(XPLANET_FOUND "XPlanet"
                                "Renders an image of all the major planets and most satellites"
                                "http://xplanet.sourceforge.net/"
                                FALSE "1.0" "Gives KStars support for xplanet.")

macro_optional_find_package(OpenGL)
macro_bool_to_01(OPENGL_FOUND HAVE_OPENGL)
macro_log_feature(OPENGL_FOUND "OpenGL"
                               "Support for hardware rendering in KStars."
                               "http://www.opengl.org/"
                               FALSE "" "OpenGL rendering is generally faster on hardware with graphics acceleration.")

enable_testing(true)

include_directories(${CMAKE_CURRENT_BINARY_DIR})
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/config-kstars.h.cmake
               ${CMAKE_CURRENT_BINARY_DIR}/config-kstars.h )

add_subdirectory(doc)
add_subdirectory(kstars)
add_subdirectory(datahandlers)
add_subdirectory(Tests)

if (${CMAKE_CURRENT_SOURCE_DIR} STREQUAL ${CMAKE_SOURCE_DIR})
   macro_display_feature_log()
endif (${CMAKE_CURRENT_SOURCE_DIR} STREQUAL ${CMAKE_SOURCE_DIR})
