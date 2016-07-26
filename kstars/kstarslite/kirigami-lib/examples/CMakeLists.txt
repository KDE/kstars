
add_subdirectory(android)

install(DIRECTORY gallery/ DESTINATION ${CMAKE_INSTALL_PREFIX}/${DATA_INSTALL_DIR}/kpackage/genericqml/org.kde.kirigami.gallery)
install(FILES gallery/metadata.desktop DESTINATION ${XDG_APPS_INSTALL_DIR} RENAME org.kde.kirigami.gallery.desktop)
