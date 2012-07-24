find_package (Qt4 REQUIRED QtCore QtGui QtXml QtTest)

set (LIBNAME kstars_lib)
set (LIBDIR kstars)
set (EXENAME tests)

set (HEADERS 
        testksparser.h
    )

set (SOURCES 
        testksparser.cpp
    )

include_directories (. ${KSTARS_SOURCE_DIR}/${LIBDIR})

link_directories (${KSTARS_BINARY_DIR}/${LIBDIR})

qt4_wrap_cpp (MOC_SOURCES ${HEADERS})

include (${QT_USE_FILE})

add_definitions (${QT_DEFINITIONS})

add_executable (${EXENAME} ${kstars_SOURCE_DIR}/kstars/main.cpp ${SOURCES} ${MOC_SOURCES})

target_link_libraries (${EXENAME} ${LIBNAME} ${QT_LIBRARIES})

add_test(cpp ${EXENAME} WORKING_DIRECTORY ${EXECUTABLE_OUTPUT_PATH})