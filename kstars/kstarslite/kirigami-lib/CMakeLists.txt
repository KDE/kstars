project(kirigami)

cmake_minimum_required(VERSION 2.8.12)
set(KF5_MIN_VERSION "5.18.0")
set(QT_MIN_VERSION "5.5.0")

################# Disallow in-source build #################

if("${CMAKE_SOURCE_DIR}" STREQUAL "${CMAKE_BINARY_DIR}")
   message(FATAL_ERROR "kirigami requires an out of source build. Please create a separate build directory and run 'cmake path_to_kirigami [options]' there.")
endif()

option(PLASMA_ENABLED "Build and install The Plasma style" ON)
option(DESKTOP_ENABLED "Build and install The Desktop style" ON)
option(STATIC_LIBRARY "Build as a static library" OFF)
option(BUILD_EXAMPLES "Build and install examples" OFF)

# Make CPack available to easy generate binary packages
include(CPack)
include(FeatureSummary)

IF(STATIC_LIBRARY)
	set(CMAKE_AUTOMOC ON)
	set(CMAKE_INCLUDE_CURRENT_DIR ON)
	ADD_DEFINITIONS(-DKIRIGAMI_BUILD_TYPE_STATIC)
	find_package(Qt5 ${QT_MIN_VERSION} REQUIRED NO_MODULE COMPONENTS Core Quick Test Gui Svg)
	set (QT_MIN_VERSION "5.5.0")
	if(UNIX)
		set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -std=c++0x")
	endif()
	add_subdirectory(src)

ELSE(STATIC_LIBRARY)


################# set KDE specific information #################

find_package(ECM 0.0.8 REQUIRED NO_MODULE)

# where to look first for cmake modules, before ${CMAKE_ROOT}/Modules/ is checked
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${ECM_MODULE_PATH} ${ECM_KDE_MODULE_DIR})

include(ECMSetupVersion)
include(ECMGenerateHeaders)
include(ECMPackageConfigHelpers)
include(KDEInstallDirs)
include(KDECMakeSettings)
include(KDECompilerSettings NO_POLICY_SCOPE)

set(CMAKECONFIG_INSTALL_DIR "${KDE_INSTALL_CMAKEPACKAGEDIR}/KF5Kirigami")

ecm_configure_package_config_file(
  "${CMAKE_CURRENT_SOURCE_DIR}/KF5KirigamiConfig.cmake.in"
  "${CMAKE_CURRENT_BINARY_DIR}/KF5KirigamiConfig.cmake"
  INSTALL_DESTINATION  ${CMAKECONFIG_INSTALL_DIR}
  PATH_VARS  KF5_INCLUDE_INSTALL_DIR CMAKE_INSTALL_PREFIX
)

install(FILES
  "${CMAKE_CURRENT_BINARY_DIR}/KF5KirigamiConfig.cmake"
  "${CMAKE_CURRENT_BINARY_DIR}/KF5KirigamiConfigVersion.cmake"
  "${CMAKE_CURRENT_SOURCE_DIR}/KF5KirigamiMacros.cmake"
  DESTINATION "${CMAKECONFIG_INSTALL_DIR}"
  COMPONENT Devel
)


set(KF5_VERSION "5.22.0")
ecm_setup_version(${KF5_VERSION}
    VARIABLE_PREFIX KIRIGAMI
    VERSION_HEADER "${CMAKE_CURRENT_BINARY_DIR}/kirigami_version.h"
    PACKAGE_VERSION_FILE "${CMAKE_CURRENT_BINARY_DIR}/KF5KirigamiConfigVersion.cmake"
    SOVERSION 5)

find_package(Qt5 ${QT_MIN_VERSION} REQUIRED NO_MODULE COMPONENTS Core Quick Test Gui Svg)

if(PLASMA_ENABLED)
# treat plasma as an optinal dep: full functionality is expected with only Qt
find_package(KF5Plasma ${KF5_MIN_VERSION})
set_package_properties(KF5Plasma PROPERTIES DESCRIPTION "Plasma framework"
                       TYPE OPTIONAL
                       PURPOSE "Optional Look and feel integration with Plasma Mobile."
                      )
endif()

if(DESKTOP_ENABLED)
find_package(KF5Declarative ${KF5_MIN_VERSION})
set_package_properties(KF5Declarative PROPERTIES DESCRIPTION "KDeclarative framework"
                       TYPE OPTIONAL
                       PURPOSE "Optional Look and feel integration with desktop colors and icons."
                      )
endif()

if(KF5Plasma_FOUND AND THEME STREQUAL "Plasma")
  set(PLASMA_ENABLED 1)
endif()

if(KF5Declarative_FOUND AND THEME STREQUAL "System")
  set(DESKTOP_ENABLED 1)
endif()

################# Enable C++11 features for clang and gcc #################

if(UNIX)
   set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -std=c++0x")
endif()

################# now find all used packages #################

set (QT_MIN_VERSION "5.5.0")

find_package(PkgConfig)
#########################################################################

add_subdirectory(src)
#TODO: conditional
if (BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()

ENDIF(STATIC_LIBRARY)

feature_summary(WHAT ALL INCLUDE_QUIET_PACKAGES FATAL_ON_MISSING_REQUIRED_PACKAGES)

