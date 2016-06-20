########### install files ###############

install(FILES
    kstars.png
    geomap.png
    citydb.sqlite
    skycultures/skycultures.sqlite
    stars.dat
    starnames.dat namedstars.dat unnamedstars.dat
    Henry-Draper.idx
    starlnum.idx
    ngcic.dat
    clines.dat
    cnames.dat
    milkyway.dat
    lmc.dat
    smc.dat
    cbounds.dat
    cbounds-3.idx  cbounds-4.idx  cbounds-5.idx  cbounds-6.idx
    image_url.dat info_url.dat
    mercury.L0.vsop mercury.L1.vsop mercury.L2.vsop mercury.L3.vsop
    mercury.L4.vsop mercury.L5.vsop mercury.B0.vsop mercury.B1.vsop
    mercury.B2.vsop mercury.B3.vsop mercury.B4.vsop mercury.B5.vsop
    mercury.R0.vsop mercury.R1.vsop mercury.R2.vsop mercury.R3.vsop
    mercury.R4.vsop mercury.R5.vsop
    venus.L0.vsop venus.L1.vsop venus.L2.vsop venus.L3.vsop
    venus.L4.vsop venus.L5.vsop venus.B0.vsop venus.B1.vsop
    venus.B2.vsop venus.B3.vsop venus.B4.vsop venus.B5.vsop
    venus.R0.vsop venus.R1.vsop venus.R2.vsop venus.R3.vsop
    venus.R4.vsop venus.R5.vsop
    earth.L0.vsop earth.L1.vsop earth.L2.vsop earth.L3.vsop
    earth.L4.vsop earth.L5.vsop earth.B0.vsop earth.B1.vsop
    earth.B2.vsop earth.B3.vsop earth.B4.vsop earth.R0.vsop
    earth.R1.vsop earth.R2.vsop earth.R3.vsop earth.R4.vsop
    earth.R5.vsop
    moonB.dat moonLR.dat
    mars.L0.vsop mars.L1.vsop mars.L2.vsop mars.L3.vsop mars.L4.vsop
    mars.L5.vsop mars.B0.vsop mars.B1.vsop mars.B2.vsop mars.B3.vsop
    mars.B4.vsop mars.B5.vsop mars.R0.vsop mars.R1.vsop mars.R2.vsop
    mars.R3.vsop mars.R4.vsop mars.R5.vsop
    jupiter.L0.vsop jupiter.L1.vsop jupiter.L2.vsop jupiter.L3.vsop
    jupiter.L4.vsop jupiter.L5.vsop jupiter.B0.vsop jupiter.B1.vsop
    jupiter.B2.vsop jupiter.B3.vsop jupiter.B4.vsop jupiter.B5.vsop
    jupiter.R0.vsop jupiter.R1.vsop jupiter.R2.vsop jupiter.R3.vsop
    jupiter.R4.vsop jupiter.R5.vsop
    saturn.L0.vsop saturn.L1.vsop saturn.L2.vsop saturn.L3.vsop
    saturn.L4.vsop saturn.L5.vsop saturn.B0.vsop saturn.B1.vsop
    saturn.B2.vsop saturn.B3.vsop saturn.B4.vsop saturn.B5.vsop
    saturn.R0.vsop saturn.R1.vsop saturn.R2.vsop saturn.R3.vsop
    saturn.R4.vsop saturn.R5.vsop
    uranus.L0.vsop uranus.L1.vsop uranus.L2.vsop uranus.L3.vsop
    uranus.L4.vsop uranus.L5.vsop uranus.B0.vsop uranus.B1.vsop
    uranus.B2.vsop uranus.B3.vsop uranus.B4.vsop uranus.R0.vsop
    uranus.R1.vsop uranus.R2.vsop uranus.R3.vsop uranus.R4.vsop
    neptune.L0.vsop neptune.L1.vsop neptune.L2.vsop neptune.L3.vsop
    neptune.L4.vsop neptune.L5.vsop neptune.B0.vsop neptune.B1.vsop
    neptune.B2.vsop neptune.B3.vsop neptune.B4.vsop neptune.B5.vsop
    neptune.R0.vsop neptune.R1.vsop neptune.R2.vsop neptune.R3.vsop
    neptune.R4.vsop
    mercury.orbit venus.orbit earth.orbit mars.orbit jupiter.orbit
    saturn.orbit uranus.orbit neptune.orbit pluto.orbit
    asteroids.dat comets.dat
    wzstars.png wzgeo.png wzscope.png wzdownload.png chart.colors
    classic.colors moonless-night.colors night.colors
    tips TZrules.dat valaav.txt Interesting.dat PlanetFacts.dat
    advinterface.dat satellites.dat	histogram.png noimage.png go-nw.png go-ne.png go-sw.png go-se.png glossary.xml defaultflag.gif
    DESTINATION  ${KDE_INSTALL_DATADIR}/kstars
)

install( FILES KDE-KStars-Alert.ogg KDE-KStars-Warning.ogg  DESTINATION  ${KDE_INSTALL_DATADIR}/sounds )

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
    DESTINATION ${KDE_INSTALL_DATADIR}/kstars/textures
)

install(FILES
    scripts/supernova_updates_parser.py
    DESTINATION ${KDE_INSTALL_DATADIR}/kstars/scripts
)

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
    DESTINATION ${KDE_INSTALL_DATADIR}/kstars/skycultures/western
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
    DESTINATION ${KDE_INSTALL_DATADIR}/kstars/skycultures/inuit
)

