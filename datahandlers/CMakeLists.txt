set(LibKSDataHandlers_SRCS
        ${kstars_SOURCE_DIR}/datahandlers/catalogentrydata.cpp
        ${kstars_SOURCE_DIR}/datahandlers/catalogdata.cpp
        ${kstars_SOURCE_DIR}/datahandlers/ksparser.cpp
        ${kstars_SOURCE_DIR}/datahandlers/catalogdb.cpp
)

add_library(LibKSDataHandlers STATIC ${LibKSDataHandlers_SRCS})

# Added this because includedir was missing, is this required?
target_link_libraries(LibKSDataHandlers KF5::WidgetsAddons Qt5::Sql Qt5::Core Qt5::Gui)

