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
    modules/KSTab.qml
    modules/KSListView.qml
    modules/SkyMapLiteWrapper.qml
    modules/ObjectDetails.qml
    modules/TimePage.qml
    DESTINATION ${KDE_INSTALL_DATADIR}/kstars/kstarslite/qml/modules )

install( FILES dialogs/FindDialog.qml
    DESTINATION ${KDE_INSTALL_DATADIR}/kstars/kstarslite/qml/dialogs )
    
install( FILES modules/popups/ColorSchemePopup.qml
    modules/popups/ProjectionsPopup.qml
    modules/popups/AddLinkPopup.qml
    modules/popups/LinkMenu.qml
    DESTINATION ${KDE_INSTALL_DATADIR}/kstars/kstarslite/qml/modules/popups)

install ( FILES modules/menus/ContextMenu.qml
    DESTINATION ${KDE_INSTALL_DATADIR}/kstars/kstarslite/qml/modules/menus)

install( FILES modules/helpers/TopMenuButton.qml
    modules/helpers/BottomMenuButton.qml
    modules/helpers/TimeSpinBox.qml
    modules/helpers/PassiveNotification.qml
    modules/helpers/Units.qml
    modules/helpers/DetailsItem.qml
    modules/helpers/KSMenuItem.qml
    modules/helpers/TelescopeControl.qml
    DESTINATION ${KDE_INSTALL_DATADIR}/kstars/kstarslite/qml/modules/helpers )

add_subdirectory(images)
add_subdirectory(indi)
