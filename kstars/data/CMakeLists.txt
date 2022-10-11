
########### install files ###############
if(APPLE)
    set(KSTARS_DATADIR "${BUNDLED_DATA_LOCATION}")
else(APPLE)
	set(KSTARS_DATADIR "${KDE_INSTALL_DATADIR}")
endif(APPLE)

install(FILES
    kstars.png
    geomap.jpg
    citydb.sqlite
    skycultures/skycultures.sqlite
    stars.dat
    starnames.dat namedstars.dat unnamedstars.dat
    Henry-Draper.idx
    starlnum.idx
    clines.dat
    cnames.dat
    milkyway.dat
    lmc.dat
    smc.dat
    cbounds.dat
    cbounds-3.idx  cbounds-4.idx  cbounds-5.idx  cbounds-6.idx
    image_url.dat info_url.dat
    moonB.dat moonLR.dat
    mercury.orbit venus.orbit earth.orbit mars.orbit jupiter.orbit
    saturn.orbit uranus.orbit neptune.orbit pluto.orbit
    asteroids.dat comets.dat cometels.json.gz wzserialportassistant.png
    wzstars.png wzgeo.png wzscope.png wzdownload.png wzekos.png ekoslive.png
    chart.colors classic.colors moonless-night.colors night.colors
    tips TZrules.dat Interesting.dat PlanetFacts.dat indidrivers.xml
    advinterface.dat catalog.min.json satellites.dat glossary.xml defaultflag.gif
    OpenNGC.kscat
    DESTINATION ${KSTARS_DATADIR}/kstars
)

file(GLOB vsop87_files vsop87/*)
install( FILES ${vsop87_files} DESTINATION ${KSTARS_DATADIR}/kstars)

# N.B. On Windows, the sound files do not exist at all.
# On Linux, it will overwrite the Oxygen-* files.
file(GLOB sound_files sounds/*)
install( FILES ${sound_files} DESTINATION ${KSTARS_DATADIR}/sounds )

file(GLOB scheme_files themes/*)
install( FILES ${scheme_files} DESTINATION ${KSTARS_DATADIR}/kstars/themes )

install(FILES
    textures/star.png
    textures/galaxy.png
    textures/open-cluster.png
    textures/globular-cluster.png
    textures/galaxy-cluster.png
    textures/gaseous-nebula.png
    textures/planetary-nebula.png
    textures/obslistsymbol.png
    textures/sun.png
    textures/mercury.png
    textures/venus.png
    textures/mars.png
    textures/jupiter.png
    textures/saturn.png
    textures/uranus.png
    textures/neptune.png
    textures/pluto.png
    textures/moon00.png
    textures/moon01.png
    textures/moon02.png
    textures/moon03.png
    textures/moon04.png
    textures/moon05.png
    textures/moon06.png
    textures/moon07.png
    textures/moon08.png
    textures/moon09.png
    textures/moon10.png
    textures/moon11.png
    textures/moon12.png
    textures/moon13.png
    textures/moon14.png
    textures/moon15.png
    textures/moon16.png
    textures/moon17.png
    textures/moon18.png
    textures/moon19.png
    textures/moon20.png
    textures/moon21.png
    textures/moon22.png
    textures/moon23.png
    textures/moon24.png
    textures/moon25.png
    textures/moon26.png
    textures/moon27.png
    textures/moon28.png
    textures/moon29.png
    textures/moon30.png
    textures/moon31.png
    textures/moon32.png
    textures/moon33.png
    textures/moon34.png
    textures/moon35.png
    textures/defaultflag.png
    DESTINATION ${KSTARS_DATADIR}/kstars/textures
)

#install(FILES
#    scripts/supernova_updates_parser.py
#    DESTINATION ${KSTARS_DATADIR}/kstars/scripts
#)

install(FILES
    skycultures/western/andromeda.png
    skycultures/western/antlia.png
    skycultures/western/apus.png
    skycultures/western/aquarius.png
    skycultures/western/aquila.png
    skycultures/western/ara.png
    skycultures/western/argonavis.png
    skycultures/western/aries.png
    skycultures/western/auriga.png
    skycultures/western/bootes.png
    skycultures/western/caelum.png
    skycultures/western/camelopardalis.png
    skycultures/western/cancer.png
    skycultures/western/canes-venatici.png
    skycultures/western/canis-major.png
    skycultures/western/canis-minor.png
    skycultures/western/capricornus.png
    skycultures/western/cassiopeia.png
    skycultures/western/centaurus.png
    skycultures/western/cepheus.png
    skycultures/western/cetus.png
    skycultures/western/chamaeleon.png
    skycultures/western/circinus.png
    skycultures/western/columba.png
    skycultures/western/coma-berenices.png
    skycultures/western/corona-australis.png
    skycultures/western/corona-borealis.png
    skycultures/western/corvus.png
    skycultures/western/crater.png
    skycultures/western/crux.png
    skycultures/western/cygnus.png
    skycultures/western/delphinus.png
    skycultures/western/dorado.png
    skycultures/western/draco.png
    skycultures/western/equuleus.png
    skycultures/western/eridanus.png
    skycultures/western/fornax.png
    skycultures/western/gemini.png
    skycultures/western/grus.png
    skycultures/western/hercules.png
    skycultures/western/horlogium.png
    skycultures/western/hydra.png
    skycultures/western/hydrus.png
    skycultures/western/indus.png
    skycultures/western/lacerta.png
    skycultures/western/leo-minor.png
    skycultures/western/leo.png
    skycultures/western/lepus.png
    skycultures/western/libra.png
    skycultures/western/lupus.png
    skycultures/western/lynx.png
    skycultures/western/lyra.png
    skycultures/western/mensa.png
    skycultures/western/microscopium.png
    skycultures/western/monoceros.png
    skycultures/western/musca.png
    skycultures/western/norma.png
    skycultures/western/octans.png
    skycultures/western/ophiuchus.png
    skycultures/western/orion.png
    skycultures/western/pavo.png
    skycultures/western/pegasus.png
    skycultures/western/perseus.png
    skycultures/western/phoenix.png
    skycultures/western/pictor.png
    skycultures/western/pisces.png
    skycultures/western/piscis-austrinus.png
    skycultures/western/pyxis.png
    skycultures/western/reticulum.png
    skycultures/western/sagitta.png
    skycultures/western/sagittarius.png
    skycultures/western/scorpius.png
    skycultures/western/sculptor.png
    skycultures/western/scutum.png
    skycultures/western/sextans.png
    skycultures/western/taurus.png
    skycultures/western/telescopium.png
    skycultures/western/triangulum-australe.png
    skycultures/western/triangulum.png
    skycultures/western/tucana.png
    skycultures/western/ursa-major.png
    skycultures/western/ursa-minor.png
    skycultures/western/virgo.png
    skycultures/western/volans.png
    skycultures/western/vulpecula.png
    DESTINATION ${KSTARS_DATADIR}/kstars/skycultures/western
)

install(FILES
    skycultures/inuit/blubber-container.png
    skycultures/inuit/breastbone.png
    skycultures/inuit/caribou.png
    skycultures/inuit/collarbones.png
    skycultures/inuit/dogs.png
    skycultures/inuit/lamp-stand.png
    skycultures/inuit/runners.png
    skycultures/inuit/the-one-behind.png
    skycultures/inuit/two-in-front.png
    skycultures/inuit/two-placed-far-apart.png
    skycultures/inuit/two-sunbeams.png
    DESTINATION ${KSTARS_DATADIR}/kstars/skycultures/inuit
)


add_subdirectory(fr)
add_subdirectory(nds)
