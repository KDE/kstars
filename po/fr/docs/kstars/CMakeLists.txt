########### install files ###############
#
if(NOT BUILD_KSTARS_LITE)
kdoctools_create_handbook(index.docbook INSTALL_DESTINATION ${KDE_INSTALL_DOCBUNDLEDIR}/en SUBDIR kstars)
endif(NOT BUILD_KSTARS_LITE)

