#install_files
install( FILES INDIControlPanel.qml
    DevicePanel.qml
    DESTINATION ${KDE_INSTALL_DATADIR}/kstars/kstarslite/qml/indi )
    
install( FILES modules/MotionControl.qml
    modules/Led.qml
    modules/Property.qml
    modules/KSTab.qml
    modules/KSButton.qml
    modules/KSINDIText.qml
    modules/KSINDINumber.qml
    modules/KSINDINumberField.qml
    modules/KSCheckBox.qml
    DESTINATION ${KDE_INSTALL_DATADIR}/kstars/kstarslite/qml/indi/modules )

add_subdirectory(modules/images)
