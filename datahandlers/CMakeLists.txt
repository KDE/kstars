
include_directories(
    ${kstars_SOURCE_DIR}/kstars/tools
    ${kstars_SOURCE_DIR}/kstars/skyobjects
    ${kstars_SOURCE_DIR}/kstars/skycomponents
    ${kstars_SOURCE_DIR}/kstars/auxiliary
    ${kstars_SOURCE_DIR}/kstars/time
    ${kstars_SOURCE_DIR}/kstars/kstarslite
)

SET(LibKSDataHandlers_SRC
    ${kstars_SOURCE_DIR}/datahandlers/catalogentrydata.cpp
    ${kstars_SOURCE_DIR}/datahandlers/catalogdata.cpp
    ${kstars_SOURCE_DIR}/datahandlers/ksparser.cpp
    ${kstars_SOURCE_DIR}/datahandlers/catalogdb.cpp)

IF (UNITY_BUILD)
    ENABLE_UNITY_BUILD(LibKSDataHandlers LibKSDataHandlers_SRC 10 cpp)
ENDIF ()

add_library(LibKSDataHandlers STATIC ${LibKSDataHandlers_SRC})

# Added this because includedir was missing, is this required?
if(BUILD_KSTARS_LITE)
    target_link_libraries(LibKSDataHandlers KF5::I18n Qt5::Sql Qt5::Core Qt5::Gui)
else(BUILD_KSTARS_LITE)
    target_link_libraries(LibKSDataHandlers KF5::WidgetsAddons KF5::I18n Qt5::Sql Qt5::Core Qt5::Gui)
endif(BUILD_KSTARS_LITE)

