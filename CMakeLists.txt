PROJECT(kstars CXX C)
set (KStars_VERSION_MAJOR 2)
set (KStars_VERSION_MINOR 6)
set (KStars_VERSION_REVISION 0)

# minimal requirements
cmake_minimum_required (VERSION 2.8.12 FATAL_ERROR)
set (QT_MIN_VERSION "5.4.0")

# Find includes in corresponding build directories
set(CMAKE_INCLUDE_CURRENT_DIR ON)
# Instruct CMake to run moc automatically when needed.
set(CMAKE_AUTOMOC ON)

find_package(ECM 1.7.0 REQUIRED NO_MODULE)
set(CMAKE_MODULE_PATH ${ECM_MODULE_PATH})
set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake/modules" ${CMAKE_MODULE_PATH})

find_package(Qt5 5.4 REQUIRED COMPONENTS Gui Qml Quick Xml Sql Svg Network PrintSupport)

include(KDEInstallDirs)
include(KDECompilerSettings NO_POLICY_SCOPE)
include(KDECMakeSettings)
include(ECMInstallIcons)
include(FeatureSummary)
include(ECMAddAppIcon)

# Load the frameworks we need
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
#    IconThemes
)

find_package(Eigen3 REQUIRED)
add_definitions(${EIGEN_DEFINITIONS})
include_directories(${EIGEN3_INCLUDE_DIR})

find_package(CFitsio)
if(CFITSIO_FOUND)
    set(HAVE_CFITSIO 1)
else(CFITSIO_FOUND)
    set(HAVE_CFITSIO 0)
endif(CFITSIO_FOUND)
set_package_properties(CFitsio PROPERTIES DESCRIPTION "FITS IO Library" URL "http://heasarc.gsfc.nasa.gov/fitsio/fitsio.html" TYPE OPTIONAL PURPOSE "Support for the FITS (Flexible Image Transport System) data format in KStars.")

find_package(INDI 1.2.0)
#FIXME is there a better way to do this?
if(INDI_FOUND)
    #FIXME is the following the right way of doing this?
    find_package(KF5 REQUIRED COMPONENTS Notifications)
    SET(HAVE_INDI 1)
else(INDI_FOUND)
    SET(HAVE_INDI 0)
endif(INDI_FOUND)
set_package_properties(INDI PROPERTIES DESCRIPTION "Astronomical instrumentation control" URL "http://www.indilib.org" TYPE OPTIONAL PURPOSE "Support for controlling astronomical devices on Linux with KStars.")

find_package(WCSLIB)
if(WCSLIB_FOUND)
    set(HAVE_WCSLIB 1)
else(WCSLIB_FOUND)
    set(HAVE_WCSLIB 0)
endif(WCSLIB_FOUND)
set_package_properties(WCSLIB PROPERTIES DESCRIPTION "World Coordinate System library" URL "http://www.atnf.csiro.au/people/mcalabre/WCS" TYPE OPTIONAL PURPOSE "WCS enables KStars to read and process world coordinate systems in FITS header.")

find_package(Xplanet)
if(XPLANET_FOUND)
    set(HAVE_XPLANET 1)
else(XPLANET_FOUND)
    set(HAVE_XPLANET 0)
endif(XPLANET_FOUND)
set_package_properties(Xplanet PROPERTIES DESCRIPTION "Renders an image of all the major planets and most satellites" URL "http://xplanet.sourceforge.net" TYPE OPTIONAL PURPOSE "Gives KStars support for xplanet.")

find_package(AstrometryNet)
if(ASTROMETRYNET_FOUND)
    set(HAVE_ASTROMETRYNET 1)
else(ASTROMETRYNET_FOUND)
    set(HAVE_ASTROMETRYNET 0)
endif(ASTROMETRYNET_FOUND)
set_package_properties(AstrometryNet PROPERTIES DESCRIPTION "Astrometrics Library" URL "http://www.astrometry.net" TYPE OPTIONAL PURPOSE "Support for plate solving in KStars.")

find_package(OpenGL)
set_package_properties(OpenGL PROPERTIES DESCRIPTION "Open Graphics Library" URL "http://www.opengl.org" TYPE OPTIONAL PURPOSE "Support for hardware rendering in KStars.")

if(${KF5_VERSION} VERSION_EQUAL 5.18.0 OR ${KF5_VERSION} VERSION_GREATER 5.18.0)
    SET(HAVE_KF5WIT 1)
else()
    SET(HAVE_KF5WIT 0)
endif()

add_definitions(-DQT_USE_FAST_CONCATENATION -DQT_USE_FAST_OPERATOR_PLUS -Dfexceptions)

configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/config-kstars.h.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/config-kstars.h
)

configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/kstars/version.h.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/kstars/version.h
)

# let our config.h be found first in any case
include_directories (BEFORE ${CMAKE_CURRENT_BINARY_DIR})

add_subdirectory(doc)
add_subdirectory(kstars)
add_subdirectory(datahandlers)

enable_testing(true)
add_subdirectory(Tests)

install(FILES org.kde.kstars.appdata.xml DESTINATION ${KDE_INSTALL_DATAROOTDIR}/appdata/)

feature_summary(WHAT ALL FATAL_ON_MISSING_REQUIRED_PACKAGES)
