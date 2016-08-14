#install_files
install( FILES kstars.png
    DESTINATION  ${KDE_INSTALL_DATADIR}/kstars/kstarslite/qml/modules/images )
    
add_subdirectory(hdpi) 
add_subdirectory(ldpi)
add_subdirectory(mdpi)
add_subdirectory(xhdpi)
add_subdirectory(xxhdpi)
add_subdirectory(xxxhdpi)
