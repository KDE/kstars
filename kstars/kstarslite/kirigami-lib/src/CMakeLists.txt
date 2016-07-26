project(kirigami)

set(kirigami_SRCS
    kirigamiplugin.cpp
    )

IF(STATIC_LIBRARY)


qt5_add_resources(kirigami_SRCS ${CMAKE_SOURCE_DIR}/kirigami.qrc)
add_library(kirigamiplugin STATIC ${kirigami_SRCS} ${RESOURCES})
target_link_libraries(kirigamiplugin Qt5::Core  Qt5::Qml Qt5::Quick)

ELSE(STATIC_LIBRARY)

add_library(kirigamiplugin SHARED ${kirigami_SRCS})
target_link_libraries(kirigamiplugin Qt5::Core  Qt5::Qml Qt5::Quick)

install(TARGETS kirigamiplugin DESTINATION ${KDE_INSTALL_QMLDIR}/org/kde/kirigami)

install(DIRECTORY controls/ DESTINATION ${KDE_INSTALL_QMLDIR}/org/kde/kirigami)

if (PLASMA_ENABLED)
    install(DIRECTORY styles/Plasma DESTINATION ${KDE_INSTALL_QMLDIR}/org/kde/kirigami/styles)
endif()
if (DESKTOP_ENABLED)
    install(DIRECTORY styles/Desktop DESTINATION ${KDE_INSTALL_QMLDIR}/org/kde/kirigami/styles)
endif()

install(FILES ${platformspecific} DESTINATION ${KDE_INSTALL_QMLDIR}/org/kde/kirigami)

include(ECMGeneratePriFile)
ecm_generate_pri_file(BASE_NAME Kirigami LIB_NAME KF5Kirigami DEPS "core qml quick svg" FILENAME_VAR PRI_FILENAME )
install(FILES ${PRI_FILENAME}
        DESTINATION ${ECM_MKSPECS_INSTALL_DIR})

ENDIF(STATIC_LIBRARY)
