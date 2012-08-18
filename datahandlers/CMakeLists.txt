set(LibKSDataHandlers_SRCS
        ${kstars_SOURCE_DIR}/datahandlers/ksparser.cpp
        ${kstars_SOURCE_DIR}/datahandlers/catalogdb.cpp
)

kde4_add_library(LibKSDataHandlers STATIC ${LibKSDataHandlers_SRCS})