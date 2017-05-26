PROJECT(kstars CXX C)

set (KStars_VERSION_MAJOR 2)
set (KStars_VERSION_MINOR 7)
set (KStars_VERSION_REVISION 8)
set (CMAKE_CXX_STANDARD 11)
#Build KStars Lite with -DKSTARS_LITE=ON
option(BUILD_KSTARS_LITE "Build KStars Lite" OFF)
if(BUILD_KSTARS_LITE)
    add_definitions(-DKSTARS_LITE)
endif(BUILD_KSTARS_LITE)
# minimal requirements
cmake_minimum_required (VERSION 2.8.12 FATAL_ERROR)

string(TIMESTAMP KSTARS_BUILD_TS UTC)

if(BUILD_KSTARS_LITE)
    set (QT_MIN_VERSION "5.7.0") #QtQuickControls 2 is available only in the Qt 5.7.0
else()
    set (QT_MIN_VERSION "5.4.0")
endif()

# Find includes in corresponding build directories
set(CMAKE_INCLUDE_CURRENT_DIR ON)
# Instruct CMake to run moc automatically when needed.
set(CMAKE_AUTOMOC ON)

# Ccache support
IF (ANDROID OR UNIX OR APPLE)
    FIND_PROGRAM(CCACHE_FOUND ccache)
    SET(CCACHE_SUPPORT OFF CACHE BOOL "Enable ccache support")
    IF ((CCACHE_FOUND OR ANDROID) AND CCACHE_SUPPORT MATCHES ON)
        SET_PROPERTY(GLOBAL PROPERTY RULE_LAUNCH_COMPILE ccache)
        SET_PROPERTY(GLOBAL PROPERTY RULE_LAUNCH_LINK ccache)
    ENDIF ()
ENDIF ()

if(CMAKE_SYSTEM_NAME STREQUAL Android)
    add_definitions(-DANDROID -D__STDC_LIMIT_MACROS)
    set(ANDROID TRUE)
endif()

if(BUILD_KSTARS_LITE AND ANDROID)
    include(${CMAKE_BINARY_DIR}/kf5/kde/install/share/ECM/cmake/ECMConfig.cmake)
endif()

find_package(ECM 1.7.0 REQUIRED NO_MODULE)
set(CMAKE_MODULE_PATH ${ECM_MODULE_PATH})
set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake/modules" ${CMAKE_MODULE_PATH})

if(BUILD_KSTARS_LITE)
    if(ANDROID)
        set(QT_ANDROID $ENV{QT_ANDROID})
        include(${QT_ANDROID}/lib/cmake/Qt5/Qt5Config.cmake)
        include(${QT_ANDROID}/lib/cmake/Qt5Core/Qt5CoreConfig.cmake)
        include(${QT_ANDROID}/lib/cmake/Qt5Gui/Qt5GuiConfig.cmake)
        include(${QT_ANDROID}/lib/cmake/Qt5Qml/Qt5QmlConfig.cmake)
        include(${QT_ANDROID}/lib/cmake/Qt5Quick/Qt5QuickConfig.cmake)
        include(${QT_ANDROID}/lib/cmake/Qt5QuickControls2/Qt5QuickControls2Config.cmake)
        include(${QT_ANDROID}/lib/cmake/Qt5Xml/Qt5XmlConfig.cmake)
        include(${QT_ANDROID}/lib/cmake/Qt5Svg/Qt5SvgConfig.cmake)
        include(${QT_ANDROID}/lib/cmake/Qt5Sql/Qt5SqlConfig.cmake)
        include(${QT_ANDROID}/lib/cmake/Qt5Network/Qt5NetworkConfig.cmake)
        include(${QT_ANDROID}/lib/cmake/Qt5Positioning/Qt5PositioningConfig.cmake)
        include(${QT_ANDROID}/lib/cmake/Qt5Concurrent/Qt5ConcurrentConfig.cmake)
        include(${QT_ANDROID}/lib/cmake/Qt5AndroidExtras/Qt5AndroidExtrasConfig.cmake)
    else()
        find_package(Qt5 5.7 REQUIRED COMPONENTS Gui Qml Quick QuickControls2 Xml Svg Sql Network Positioning Concurrent ${QT_EXTRA_COMPONENTS})
    endif()
else()
    find_package(Qt5 5.4 REQUIRED COMPONENTS Gui Qml Quick Xml Sql Svg Network PrintSupport Concurrent)
endif()
include(ExternalProject)
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
    if(ANDROID)
        set(KF5_VERSION 5.28.0)
        set(KF5_HOST_TOOLING /usr/lib/x86_64-linux-gnu/cmake/)
        include(${CMAKE_BINARY_DIR}/kf5/kde/install/lib/cmake/KF5Config/KF5ConfigConfig.cmake)
        include(${CMAKE_BINARY_DIR}/kf5/kde/install/lib/cmake/KF5I18n/KF5I18nConfig.cmake)
        include(${CMAKE_BINARY_DIR}/kf5/kde/install/lib/cmake/KF5Plotting/KF5PlottingConfig.cmake)
    else()
        find_package(KF5 REQUIRED COMPONENTS Config I18n Plotting)
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
    if(ANDROID)
        externalproject_add(cfitsio SOURCE_DIR "${CMAKE_BINARY_DIR}/android/3rdparty/cfitsio"
            URL https://heasarc.gsfc.nasa.gov/FTP/software/fitsio/c/cfitsio3370.tar.gz
            PATCH_COMMAND bash -c "cd ${CMAKE_BINARY_DIR}/android/3rdparty/cfitsio && dos2unix ${CMAKE_BINARY_DIR}/android/3rdparty/cfitsio/CMakeLists.txt && patch -p0 < ${CMAKE_SOURCE_DIR}/android/3rdparty/cfitsio.patch"
            CMAKE_ARGS -DCMAKE_TOOLCHAIN_FILE=${CMAKE_SOURCE_DIR}/android/toolchain-android.cmake -DBUILD_SHARED_LIBS=OFF -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
            BUILD_COMMAND make cfitsio
            BUILD_IN_SOURCE 1
            INSTALL_COMMAND "")

        set(CFITSIO_INCLUDE_DIR ${CMAKE_BINARY_DIR}/android/3rdparty/cfitsio)
        set(CFITSIO_LIBRARIES ${CMAKE_BINARY_DIR}/android/3rdparty/cfitsio/libcfitsio.a)
    else()
        find_package(CFitsio REQUIRED)
    endif()
else()
    find_package(CFitsio)
endif()
MACRO_BOOL_TO_01(CFITSIO_FOUND HAVE_CFITSIO)
set_package_properties(CFitsio PROPERTIES DESCRIPTION "FITS IO Library" URL "http://heasarc.gsfc.nasa.gov/fitsio/fitsio.html" TYPE OPTIONAL PURPOSE "Support for the FITS (Flexible Image Transport System) data format in KStars.")

## Nova Library
if (ANDROID)
    set(GIT_EXECUTABLE git)
    externalproject_add(nova SOURCE_DIR "${CMAKE_BINARY_DIR}/android/3rdparty/libnova"
        DOWNLOAD_COMMAND bash -c "mkdir -p ${CMAKE_BINARY_DIR}/android/3rdparty && git clone https://git.code.sf.net/p/libnova/libnova ${CMAKE_BINARY_DIR}/android/3rdparty/libnova"
        PATCH_COMMAND bash -c "cd ${CMAKE_BINARY_DIR}/android/3rdparty/libnova && patch -p0 < ${CMAKE_SOURCE_DIR}/android/3rdparty/libnova.patch"
        CMAKE_ARGS -DCMAKE_TOOLCHAIN_FILE=${CMAKE_SOURCE_DIR}/android/toolchain-android.cmake -DBUILD_SHARED_LIBRARY=OFF -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
        BUILD_COMMAND make libnova
        UPDATE_COMMAND ""
        BUILD_IN_SOURCE 1
        INSTALL_COMMAND "")

    set(NOVA_INCLUDE_DIR ${CMAKE_BINARY_DIR}/android/3rdparty/libnova/src ${CMAKE_BINARY_DIR}/android/3rdparty/libnova/src/libnova)
    set(NOVA_LIBRARIES ${CMAKE_BINARY_DIR}/android/3rdparty/libnova/lib/liblibnova.a -landroid_support)
endif ()

## INDI Library
if (ANDROID)
    set(QT_ANDROID $ENV{QT_ANDROID})
    externalproject_add(indi SOURCE_DIR "${CMAKE_BINARY_DIR}/android/indi"
        DOWNLOAD_COMMAND bash -c "mkdir -p ${CMAKE_BINARY_DIR}/android/indi && git clone https://github.com/indilib/indi/ ${CMAKE_BINARY_DIR}/android/indi"
        CONFIGURE_COMMAND cd libindi && cmake . -DCMAKE_TOOLCHAIN_FILE=${CMAKE_SOURCE_DIR}/android/toolchain-android.cmake -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE} \\
             -DINDI_BUILD_POSIX_CLIENT=OFF -DINDI_BUILD_SERVER=OFF -DINDI_BUILD_DRIVERS=OFF -DINDI_BUILD_UNITTESTS=OFF \\
             -DINDI_BUILD_DRIVERS=OFF -DINDI_BUILD_QT5_CLIENT=ON -DINDI_CALCULATE_MINMAX=ON \\
             -DQT_ANDROID=${QT_ANDROID} -DCFITSIO_DIR=${CMAKE_BINARY_DIR}/android/3rdparty/cfitsio \\
             -DNOVA_DIR=${CMAKE_BINARY_DIR}/android/3rdparty/libnova
        BUILD_COMMAND make -C libindi
        BUILD_IN_SOURCE 1
        DEPENDS nova
        UPDATE_COMMAND ""
        INSTALL_COMMAND "")

    set(INDI_FOUND TRUE)
    set(INDI_INCLUDE_DIR ${CMAKE_BINARY_DIR}/android/indi/libindi/libs/indibase ${CMAKE_BINARY_DIR}/android/indi/libindi ${CMAKE_BINARY_DIR}/android/indi/libindi/libs)
    set(INDI_CLIENT_ANDROID_LIBRARIES ${CMAKE_BINARY_DIR}/android/indi/libindi/libindiclientqt.a)
else ()
    find_package(INDI 1.4.1)
endif ()
MACRO_BOOL_TO_01(INDI_FOUND HAVE_INDI)
set_package_properties(INDI PROPERTIES DESCRIPTION "Astronomical instrumentation control" URL "http://www.indilib.org" TYPE OPTIONAL PURPOSE "Support for controlling astronomical devices on Linux with KStars.")

## Libraw
if(BUILD_KSTARS_LITE AND ANDROID)
    # OpenMP needed for LibRaw
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fopenmp -DLIBRAW_USE_OPENMP")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fopenmp -DLIBRAW_USE_OPENMP")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -lgomp")

    externalproject_add(raw SOURCE_DIR "${CMAKE_BINARY_DIR}/android/3rdparty/libraw"
        URL https://www.libraw.org/data/LibRaw-0.17.2.tar.gz 
        PATCH_COMMAND bash -c "cd ${CMAKE_BINARY_DIR}/android/3rdparty/libraw && wget -qO- https://github.com/LibRaw/LibRaw-cmake/archive/master.zip | jar xvf /dev/stdin && mv -f LibRaw-cmake-master/* . && patch -p0 < ${CMAKE_SOURCE_DIR}/android/3rdparty/libraw.patch"
        CMAKE_ARGS -DCMAKE_TOOLCHAIN_FILE=${CMAKE_SOURCE_DIR}/android/toolchain-android.cmake -DBUILD_SHARED_LIBS=OFF -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE} -DENABLE_OPENMP=OFF
        BUILD_COMMAND make raw
        BUILD_IN_SOURCE 1
        INSTALL_COMMAND "")
    include_directories(${CMAKE_BINARY_DIR}/android/3rdparty/libraw)
    set(LIBRAW_LIBRARIES ${CMAKE_BINARY_DIR}/android/3rdparty/libraw/libraw.a -lgnustl_static)
    set(LibRaw_FOUND true)
else()
    find_package(LibRaw)
endif()
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

# Optimize binary size by dropping unneeded symbols at linking stage
if (${CMAKE_SYSTEM_NAME} MATCHES "Linux")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fdata-sections -ffunction-sections")
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,--gc-sections")
endif()
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

# Final package generation
if (ANDROID)
    set(EXPORT_DIR "${CMAKE_BINARY_DIR}/android/kstars_build_apk/")
    set(ANDROID_APK_DIR "${CMAKE_SOURCE_DIR}/android/apk/")
    set(EXECUTABLE_DESTINATION_PATH "${EXPORT_DIR}/libs/armeabi-v7a/libkstars.so")
    set(ANDROID_NDK $ENV{ANDROID_NDK})
    set(ANDROID_SDK_ROOT $ENV{ANDROID_SDK_ROOT})
    set(ANDROID_API_LEVEL $ENV{ANDROID_API_LEVEL})
    set(ANDROID_TOOLCHAIN arm-linux-androideabi)
    set(ANDROID_GCC_VERSION 4.9)
    set(ANDROID_ABI armeabi-v7a)
    set(_HOST "${CMAKE_HOST_SYSTEM_NAME}-${CMAKE_HOST_SYSTEM_PROCESSOR}")
    string(TOLOWER "${_HOST}" _HOST)
    set(ANDROID_SDK_BUILD_TOOLS_REVISION 21.1.1)
    set(ANDROID_KEYSTORE $ENV{ANDROID_KEYSTORE})
    set(ANDROID_KEYSTORE_ALIAS $ENV{ANDROID_KEYSTORE_ALIAS})
    configure_file("${CMAKE_SOURCE_DIR}/android/deployment-file.json.in" "${CMAKE_BINARY_DIR}/android/kstars-deployment.json.in")


    add_custom_target(create-apk-kstars
        COMMAND cmake -E echo "Generating $<TARGET_NAME:kstars> with $<TARGET_FILE_DIR:Qt5::qmake>/androiddeployqt"
        COMMAND cmake -E remove_directory "${EXPORT_DIR}"
        COMMAND cmake -E copy_directory "${CMAKE_SOURCE_DIR}/android/apk" "${EXPORT_DIR}"
        COMMAND cmake -E copy "$<TARGET_FILE:kstars>" "${EXECUTABLE_DESTINATION_PATH}"
        COMMAND cmake -DINPUT_FILE="${CMAKE_BINARY_DIR}/android/kstars-deployment.json.in" -DOUTPUT_FILE="${CMAKE_BINARY_DIR}/android/kstars-deployment.json" "-DTARGET_DIR=${CMAKE_BINARY_DIR}/kstars" "-DTARGET_NAME=kstars" "-DEXPORT_DIR=${CMAKE_INSTALL_PREFIX}" -P ${CMAKE_SOURCE_DIR}/android/specifydependencies.cmake
        COMMAND $<TARGET_FILE_DIR:Qt5::qmake>/androiddeployqt --input "${CMAKE_BINARY_DIR}/android/kstars-deployment.json" --output "${EXPORT_DIR}" --android-platform android-${ANDROID_API_LEVEL} --debug --deployment bundled "\\$(ARGS)"
        COMMAND $<TARGET_FILE_DIR:Qt5::qmake>/androiddeployqt --input "${CMAKE_BINARY_DIR}/android/kstars-deployment.json" --output "${EXPORT_DIR}" --android-platform android-${ANDROID_API_LEVEL} --release --deployment bundled "\\$(ARGS)"
        )
    if (ANDROID_KEYSTORE AND ANDROID_KEYSTORE_ALIAS)
        add_custom_target(sign-apk-kstars
            COMMAND jarsigner -keystore ${ANDROID_KEYSTORE} ${CMAKE_BINARY_DIR}/android/kstars_build_apk/bin/QtApp-release-unsigned.apk ${ANDROID_KEYSTORE_ALIAS}
            COMMAND rm -rf ${CMAKE_BINARY_DIR}/android/kstars_build_apk/bin/kstars-signed.apk
            COMMAND zipalign -v 4 ${CMAKE_BINARY_DIR}/android/kstars_build_apk/bin/QtApp-release-unsigned.apk ${CMAKE_BINARY_DIR}/android/kstars_build_apk/bin/kstars-signed.apk
            )
    endif ()
endif ()
