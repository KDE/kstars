#FIXME The following doesn't work, cmake complains of "group (action) is not recognized".
ecm_install_icons(ICONS
hi32-action-kstars_clines.png      hi48-action-kstars_deepsky.png     hi64-action-kstars_cnames.png       hi64-action-kstars_xplanet.png
hi22-action-kstars_cbound.png      hi32-action-kstars_cnames.png      hi48-action-kstars_flag.png        hi64-action-kstars_colors.png       hisc-action-kstars_cbound.svgz
hi22-action-kstars_clines.png      hi32-action-kstars_deepsky.png     hi48-action-kstars_grid.png        hi64-action-kstars_deepsky.png      hisc-action-kstars_clines.svgz
hi22-action-kstars_cnames.png      hi32-action-kstars_flag.png        hi48-action-kstars_hgrid.png       hi64-action-kstars_flag.png         hisc-action-kstars_cnames.svgz
hi22-action-kstars_deepsky.png     hi32-action-kstars_grid.png        hi48-action-kstars_horizon.png     hi64-action-kstars_grid.png         hisc-action-kstars_deepsky.svgz
hi22-action-kstars_flag.png        hi32-action-kstars_hgrid.png       hi48-action-kstars_mw.png          hi64-action-kstars_guides.png       hisc-action-kstars_flag.svg
hi22-action-kstars_grid.png        hi32-action-kstars_horizon.png     hi48-action-kstars_planets.png     hi64-action-kstars_hgrid.png        hisc-action-kstars_grid.svgz
hi22-action-kstars_hgrid.png       hi32-action-kstars_mw.png          hi48-action-kstars_satellites.png  hi64-action-kstars_horizon.png      hisc-action-kstars_hgrid.svg
hi22-action-kstars_horizon.png     hi32-action-kstars_planets.png     hi48-action-kstars_stars.png       hi64-action-kstars_mw.png           hisc-action-kstars_horizon.svgz
hi22-action-kstars_mw.png          hi32-action-kstars_satellites.png  hi48-action-kstars_supernovae.png  hi64-action-kstars_planets.png      hisc-action-kstars_mw.svgz
hi22-action-kstars_planets.png     hi32-action-kstars_stars.png       hi64-action-kstars_advanced.png    hi64-action-kstars_satellites.png   hisc-action-kstars_planets.svgz
hi22-action-kstars_satellites.png  hi32-action-kstars_supernovae.png  hi64-action-kstars_catalog.png     hi64-action-kstars_solarsystem.png  hisc-action-kstars_stars.svgz
hi22-action-kstars_stars.png       hi48-action-kstars_clines.png      hi64-action-kstars_cbound.png      hi64-action-kstars_stars.png
hi22-action-kstars_supernovae.png  hi48-action-kstars_cnames.png      hi64-action-kstars_clines.png      hi64-action-kstars_supernovae.png
THEME hi DESTINATION ${ICON_INSTALL_DIR} )
