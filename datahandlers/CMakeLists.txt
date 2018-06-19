
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

ecm_qt_declare_logging_category(LibKSDataHandlers_SRC HEADER catalog_debug.h IDENTIFIER KSTARS_CATALOG CATEGORY_NAME org.kde.kstars.catalog)

add_library(LibKSDataHandlers STATIC ${LibKSDataHandlers_SRC})

# Added this because includedir was missing, is this required?
if (ANDROID)
    target_link_libraries(LibKSDataHandlers KF5::I18n Qt5::Sql Qt5::Core Qt5::Gui)
else ()
    target_link_libraries(LibKSDataHandlers KF5::WidgetsAddons KF5::I18n Qt5::Sql Qt5::Core Qt5::Gui)
endif ()

