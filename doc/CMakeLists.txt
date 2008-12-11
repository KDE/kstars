########### install files ###############
#

kde4_create_handbook(index.docbook INSTALL_DESTINATION ${HTML_INSTALL_DIR}/en)
kde4_create_manpage(man-indi_celestron_gps.1.docbook 1 INSTALL_DESTINATION ${MAN_INSTALL_DIR})
kde4_create_manpage(man-indi_fli_ccd.1.docbook 1 INSTALL_DESTINATION ${MAN_INSTALL_DIR})
kde4_create_manpage(man-indi_lx200_16.1.docbook 1 INSTALL_DESTINATION ${MAN_INSTALL_DIR})
kde4_create_manpage(man-indi_lx200autostar.1.docbook 1 INSTALL_DESTINATION ${MAN_INSTALL_DIR})
kde4_create_manpage(man-indi_lx200classic.1.docbook 1 INSTALL_DESTINATION ${MAN_INSTALL_DIR})
kde4_create_manpage(man-indi_lx200generic.1.docbook 1 INSTALL_DESTINATION ${MAN_INSTALL_DIR})
kde4_create_manpage(man-indiserver.1.docbook 1 INSTALL_DESTINATION ${MAN_INSTALL_DIR})
kde4_create_manpage(man-indi_temma.1.docbook 1 INSTALL_DESTINATION ${MAN_INSTALL_DIR})
kde4_create_manpage(man-indi_v4l_generic.1.docbook 1 INSTALL_DESTINATION ${MAN_INSTALL_DIR})
kde4_create_manpage(man-indi_v4l_philips.1.docbook 1 INSTALL_DESTINATION ${MAN_INSTALL_DIR})

