#install_files
install( FILES main.qml
    DESTINATION ${KDE_INSTALL_DATADIR}/kstars/kstarslite/qml )

install( FILES constants/Constants.qml
    constants/qmldir
    DESTINATION ${KDE_INSTALL_DATADIR}/kstars/kstarslite/qml/constants )

install( FILES modules/BottomMenu.qml
    modules/Splash.qml
    modules/TopMenu.qml
    modules/KSPage.qml
    modules/KSScrollablePage.qml
    modules/ContextDrawerMenu.qml
    modules/TelescopeControlMenu.qml
    DESTINATION ${KDE_INSTALL_DATADIR}/kstars/kstarslite/qml/modules )

install( FILES modules/helpers/TopMenuButton.qml
    DESTINATION ${KDE_INSTALL_DATADIR}/kstars/kstarslite/qml/modules/helpers )

add_subdirectory(modules/images)
add_subdirectory(indi)
