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
    modules/TimePage.qml
    modules/KSLabel.qml
    modules/KSTabBarArrow.qml
    modules/KSTabButton.qml
    modules/KSText.qml
    modules/KSTextField.qml
    modules/KSButton.qml
    DESTINATION ${KDE_INSTALL_DATADIR}/kstars/kstarslite/qml/modules )

install( FILES dialogs/FindDialog.qml
    dialogs/LocationDialog.qml
    dialogs/DetailsDialog.qml
    dialogs/AboutDialog.qml
    DESTINATION ${KDE_INSTALL_DATADIR}/kstars/kstarslite/qml/dialogs )

install( FILES dialogs/helpers/DetailsAddLink.qml
    dialogs/helpers/LocationEdit.qml
    dialogs/helpers/DetailsItem.qml
    dialogs/helpers/LocationLoading.qml
    DESTINATION ${KDE_INSTALL_DATADIR}/kstars/kstarslite/qml/dialogs/helpers )

install( FILES dialogs/menus/DetailsLinkMenu.qml
    dialogs/menus/LocationsGeoMenu.qml
    DESTINATION ${KDE_INSTALL_DATADIR}/kstars/kstarslite/qml/dialogs/menus )

install( FILES modules/popups/ColorSchemePopup.qml
    modules/popups/ProjectionsPopup.qml
    modules/popups/FOVPopup.qml
    DESTINATION ${KDE_INSTALL_DATADIR}/kstars/kstarslite/qml/modules/popups)

install ( FILES modules/menus/ContextMenu.qml
    DESTINATION ${KDE_INSTALL_DATADIR}/kstars/kstarslite/qml/modules/menus)

install( FILES modules/helpers/TopMenuButton.qml
    modules/helpers/BottomMenuButton.qml
    modules/helpers/TimeSpinBox.qml
    modules/helpers/PassiveNotification.qml
    modules/helpers/Units.qml
    modules/helpers/KSMenuItem.qml
    modules/helpers/TelescopeControl.qml
    DESTINATION ${KDE_INSTALL_DATADIR}/kstars/kstarslite/qml/modules/helpers )

install( FILES modules/tutorial/TutorialStep1.qml
        modules/tutorial/TutorialStep2.qml
        modules/tutorial/TutorialStep3.qml
        modules/tutorial/TutorialStep4.qml
        modules/tutorial/TutorialStep5.qml
        modules/tutorial/TutorialPopup.qml
        modules/tutorial/TutorialExitPopup.qml
        modules/tutorial/TutorialPane.qml
    DESTINATION ${KDE_INSTALL_DATADIR}/kstars/kstarslite/qml/modules/tutorial )

add_subdirectory(images)
add_subdirectory(indi)
