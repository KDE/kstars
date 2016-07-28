#install_files
install( FILES INDIControlPanel.qml
    DevicePanel.qml
    ImagePreview.qml
    DESTINATION ${KDE_INSTALL_DATADIR}/kstars/kstarslite/qml/indi )
    
install( FILES modules/MotionControl.qml
    modules/Led.qml
    modules/Property.qml
    modules/KSTab.qml
    modules/KSLed.qml
    modules/KSButtonSwitch.qml
    modules/KSButtonsSwitchRow.qml
    modules/KSINDIText.qml
    modules/KSINDITextField.qml
    modules/KSCheckBox.qml
    DESTINATION ${KDE_INSTALL_DATADIR}/kstars/kstarslite/qml/indi/modules )

add_subdirectory(modules/images)
