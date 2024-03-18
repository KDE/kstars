
SET(KSTARS_UI_TESTS_SRC
    kstars_ui_tests.cpp
    test_kstars_startup.cpp)

include_directories(${CFITSIO_INCLUDE_DIR})

ECM_ADD_APP_ICON(KSTARS_UI_TESTS_SRC ICONS
        ../../kstars/icons/16-apps-kstars.png
        ../../kstars/icons/32-apps-kstars.png
        ../../kstars/icons/48-apps-kstars.png
        ../../kstars/icons/64-apps-kstars.png
        ../../kstars/icons/128-apps-kstars.png
        )

QT5_ADD_RESOURCES(KSTARS_UI_TESTS_SRC ../../kstars/data/kstars.qrc)

ecm_qt_declare_logging_category(KSTARS_UI_TESTS_SRC HEADER test_ekos_debug.h IDENTIFIER KSTARS_EKOS_TEST CATEGORY_NAME org.kde.kstars.ekos.test)

IF (INDI_FOUND)

INCLUDE_DIRECTORIES(${INDI_INCLUDE_DIR})
SET(KSTARS_UI_EKOS_SRC ${KSTARS_UI_TESTS_SRC} test_ekos_wizard.cpp)
SET(KSTARS_UI_EKOS_LIBS ${TEST_LIBRARIES} ${CFITSIO_LIBRARIES} ${INDI_CLIENT_LIBRARIES} ${NOVA_LIBRARIES} z)

SET(KSTARS_UI_MOCK_SRC mockmodules.cpp)
qt5_add_dbus_adaptor(KSTARS_UI_MOCK_SRC org.kde.mockkstars.MockEkos.MockFocus.xml mockmodules.h Ekos::MockFocus)
qt5_add_dbus_adaptor(KSTARS_UI_MOCK_SRC org.kde.mockkstars.MockEkos.MockMount.xml mockmodules.h Ekos::MockMount)
qt5_add_dbus_adaptor(KSTARS_UI_MOCK_SRC org.kde.mockkstars.MockEkos.MockCapture.xml mockmodules.h Ekos::MockCapture)
qt5_add_dbus_adaptor(KSTARS_UI_MOCK_SRC org.kde.mockkstars.MockEkos.MockAlign.xml mockmodules.h Ekos::MockAlign)
qt5_add_dbus_adaptor(KSTARS_UI_MOCK_SRC org.kde.mockkstars.MockEkos.MockGuide.xml mockmodules.h Ekos::MockGuide)
qt5_add_dbus_adaptor(KSTARS_UI_MOCK_SRC org.kde.mockkstars.MockEkos.xml mockmodules.h Ekos::MockEkos)

# FIXME: this particular test crashes when done on the EGLFS platform
# Disabling until we find the reason, other tests do cover this scenario

# ADD_EXECUTABLE(test_ekos ${KSTARS_UI_EKOS_SRC} test_ekos.cpp)
# TARGET_LINK_LIBRARIES(test_ekos ${KSTARS_UI_EKOS_LIBS})
# ADD_TEST(NAME TestEkosProfiles COMMAND test_ekos)
# SET_TESTS_PROPERTIES( TestEkosProfiles PROPERTIES LABELS "stable;ui")

ADD_EXECUTABLE(test_ekos_simulator ${KSTARS_UI_EKOS_SRC} test_ekos_simulator.cpp)
TARGET_LINK_LIBRARIES(test_ekos_simulator ${KSTARS_UI_EKOS_LIBS})
ADD_TEST(NAME TestEkosSimulator COMMAND test_ekos_simulator)
# SET_TESTS_PROPERTIES( TestEkosSimulator PROPERTIES LABELS "stable;ui")

ADD_EXECUTABLE(test_ekos_focus ${KSTARS_UI_EKOS_SRC} test_ekos_focus.cpp)
TARGET_LINK_LIBRARIES(test_ekos_focus ${KSTARS_UI_EKOS_LIBS})
ADD_TEST(NAME TestEkosFocus COMMAND test_ekos_focus)
# SET_TESTS_PROPERTIES( TestEkosFocus PROPERTIES LABELS "stable;ui")

ADD_EXECUTABLE(test_ekos_capture ${KSTARS_UI_EKOS_SRC} test_ekos_helper.cpp test_ekos_capture_helper.cpp test_ekos_capture.cpp)
TARGET_LINK_LIBRARIES(test_ekos_capture ${KSTARS_UI_EKOS_LIBS})
ADD_TEST(NAME TestEkosCapture COMMAND test_ekos_capture)
SET_TESTS_PROPERTIES( TestEkosCapture PROPERTIES LABELS "stable;ui" TIMEOUT 600)

ADD_EXECUTABLE(test_ekos_filterwheel ${KSTARS_UI_EKOS_SRC} test_ekos_filterwheel.cpp)
TARGET_LINK_LIBRARIES(test_ekos_filterwheel ${KSTARS_UI_EKOS_LIBS})
ADD_TEST(NAME TestEkosFilterWheel COMMAND test_ekos_filterwheel)

ADD_EXECUTABLE(test_ekos_capture_count ${KSTARS_UI_EKOS_SRC} test_ekos_helper.cpp test_ekos_capture_helper.cpp test_ekos_capture_count.cpp)
TARGET_LINK_LIBRARIES(test_ekos_capture_count ${KSTARS_UI_EKOS_LIBS})
# Exculded as this is flakey: December 8, 2022
#ADD_TEST(NAME TestEkosCaptureCount COMMAND test_ekos_capture_count)
#SET_TESTS_PROPERTIES( TestEkosCaptureCount PROPERTIES LABELS "stable;ui" TIMEOUT 1200 )

ADD_EXECUTABLE(test_ekos_capture_workflow ${KSTARS_UI_EKOS_SRC} test_ekos_helper.cpp test_ekos_capture_helper.cpp test_ekos_capture_workflow.cpp)
TARGET_LINK_LIBRARIES(test_ekos_capture_workflow ${KSTARS_UI_EKOS_LIBS})

ADD_EXECUTABLE(test_ekos_align ${KSTARS_UI_EKOS_SRC} test_ekos_helper.cpp test_ekos_capture_helper.cpp test_ekos_scheduler_helper.cpp test_ekos_align.cpp)
TARGET_LINK_LIBRARIES(test_ekos_align ${KSTARS_UI_EKOS_LIBS})
ADD_CUSTOM_COMMAND( TARGET test_ekos_align POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy
            ${CMAKE_CURRENT_SOURCE_DIR}/Kocab.fits
            ${CMAKE_CURRENT_BINARY_DIR}/Kocab.fits)

# excluded to avoid CI timelimit failure
# ADD_TEST(NAME TestEkosAlign COMMAND test_ekos_align)
# SET_TESTS_PROPERTIES( TestEkosAlign PROPERTIES LABELS "unstable;ui" TIMEOUT 600 )

ADD_EXECUTABLE(test_ekos_meridianflip ${KSTARS_UI_EKOS_SRC} test_ekos_helper.cpp test_ekos_capture_helper.cpp test_ekos_meridianflip_base.cpp test_ekos_meridianflip.cpp)
TARGET_LINK_LIBRARIES(test_ekos_meridianflip ${KSTARS_UI_EKOS_LIBS})
# excluded to avoid CI timelimit failure
# ADD_TEST(NAME TestEkosMeridianFlip COMMAND test_ekos_meridianflip)
# SET_TESTS_PROPERTIES( TestEkosMeridianFlip PROPERTIES LABELS "astrometry;ui" )
ADD_CUSTOM_COMMAND( TARGET test_ekos_meridianflip POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy
            ${CMAKE_CURRENT_SOURCE_DIR}/phd2_Simulators_mf.PHDGuidingV2
            ${CMAKE_CURRENT_BINARY_DIR}/.PHDGuidingV2_mf)

ADD_EXECUTABLE(test_ekos_meridianflip_specials ${KSTARS_UI_EKOS_SRC} test_ekos_helper.cpp test_ekos_capture_helper.cpp test_ekos_meridianflip_base.cpp test_ekos_meridianflip_specials.cpp)
TARGET_LINK_LIBRARIES(test_ekos_meridianflip_specials ${KSTARS_UI_EKOS_LIBS})
ADD_CUSTOM_COMMAND( TARGET test_ekos_meridianflip_specials POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy
            ${CMAKE_CURRENT_SOURCE_DIR}/phd2_Simulators_mf.PHDGuidingV2
            ${CMAKE_CURRENT_BINARY_DIR}/.PHDGuidingV2_mf)
# excluded to avoid CI timelimit failure
# ADD_TEST(NAME TestEkosMeridianFlipSpecials COMMAND test_ekos_meridianflip_specials)
# SET_TESTS_PROPERTIES( TestEkosMeridianFlipSpecials PROPERTIES LABELS "unstable;ui" TIMEOUT 600 )

ADD_EXECUTABLE( test_ekos_meridianflipstate ${KSTARS_UI_EKOS_SRC}  test_ekos_helper.cpp test_ekos_meridianflipstate.cpp)
TARGET_LINK_LIBRARIES( test_ekos_meridianflipstate ${TEST_LIBRARIES})
ADD_TEST( NAME TestEkosMeridianFlipState COMMAND test_ekos_meridianflipstate )
SET_TESTS_PROPERTIES( TestEkosMeridianFlipState PROPERTIES LABELS "stable;ui" TIMEOUT 120 )

ADD_EXECUTABLE(test_ekos_scheduler ${KSTARS_UI_EKOS_SRC} test_ekos_scheduler.cpp)
TARGET_LINK_LIBRARIES(test_ekos_scheduler ${KSTARS_UI_EKOS_LIBS})
ADD_CUSTOM_COMMAND( TARGET test_ekos_scheduler POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy
            ${CMAKE_CURRENT_SOURCE_DIR}/../scheduler/1x1s_Lum.esq
            ${CMAKE_CURRENT_BINARY_DIR}/1x1s_Lum.esq)
ADD_CUSTOM_COMMAND( TARGET test_ekos_scheduler POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy
            ${CMAKE_CURRENT_SOURCE_DIR}/../scheduler/1x1s_RGBLumRGB.esq
            ${CMAKE_CURRENT_BINARY_DIR}/1x1s_RGBLumRGB.esq)
ADD_TEST(NAME TestEkosScheduler COMMAND test_ekos_scheduler)
SET_TESTS_PROPERTIES( TestEkosScheduler PROPERTIES LABELS "stable;ui" TIMEOUT 120 )

ADD_EXECUTABLE(test_ekos_scheduler_ops ${KSTARS_UI_EKOS_SRC} ${KSTARS_UI_MOCK_SRC} test_ekos_scheduler_helper.cpp test_ekos_scheduler_ops.cpp)
TARGET_LINK_LIBRARIES(test_ekos_scheduler_ops ${KSTARS_UI_EKOS_LIBS})
ADD_CUSTOM_COMMAND( TARGET test_ekos_scheduler_ops POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy
            ${CMAKE_CURRENT_SOURCE_DIR}/org.kde.mockkstars.MockEkos.xml
            ${CMAKE_CURRENT_SOURCE_DIR}/org.kde.mockkstars.MockEkos.MockAlign.xml
            ${CMAKE_CURRENT_SOURCE_DIR}/org.kde.mockkstars.MockEkos.MockCapture.xml
            ${CMAKE_CURRENT_SOURCE_DIR}/org.kde.mockkstars.MockEkos.MockFocus.xml
            ${CMAKE_CURRENT_SOURCE_DIR}/org.kde.mockkstars.MockEkos.MockGuide.xml
            ${CMAKE_CURRENT_SOURCE_DIR}/org.kde.mockkstars.MockEkos.MockMount.xml
            ${CMAKE_CURRENT_SOURCE_DIR}/Messier-1-40-alt0.esl
            ${CMAKE_CURRENT_SOURCE_DIR}/Messier-1-40-alt30.esl
            ${CMAKE_CURRENT_SOURCE_DIR}/Messier-1-40.esq
            ${CMAKE_CURRENT_BINARY_DIR})

ADD_TEST(NAME TestEkosSchedulerOps COMMAND test_ekos_scheduler_ops)
SET_TESTS_PROPERTIES( TestEkosSchedulerOps PROPERTIES LABELS "stable;ui" TIMEOUT 200 )

ADD_EXECUTABLE(test_artificial_horizon ${KSTARS_UI_EKOS_SRC} test_artificial_horizon.cpp)
TARGET_LINK_LIBRARIES(test_artificial_horizon ${KSTARS_UI_EKOS_LIBS})
ADD_TEST(NAME TestArtificialHorizon COMMAND test_artificial_horizon)
SET_TESTS_PROPERTIES( TestArtificialHorizon PROPERTIES LABELS "stable;ui" TIMEOUT 600 )

# JM 2021-10.16 PHD2 test often fails in CI so it is excluded now until it is fixed.
#ADD_EXECUTABLE(test_ekos_guide ${KSTARS_UI_EKOS_SRC} test_ekos_guide.cpp)
#TARGET_LINK_LIBRARIES(test_ekos_guide ${KSTARS_UI_EKOS_LIBS})
#ADD_CUSTOM_COMMAND( TARGET test_ekos_guide POST_BUILD
    #COMMAND ${CMAKE_COMMAND} -E copy
            #${CMAKE_CURRENT_SOURCE_DIR}/phd2_Simulators.PHDGuidingV2
            #${CMAKE_CURRENT_BINARY_DIR}/.PHDGuidingV2)
#ADD_TEST(NAME TestEkosGuide COMMAND test_ekos_guide)
#SET_TESTS_PROPERTIES( TestEkosGuide PROPERTIES LABELS "unstable;ui" TIMEOUT 600 )

ADD_EXECUTABLE(test_ekos_mount ${KSTARS_UI_EKOS_SRC} test_ekos_mount.cpp)
TARGET_LINK_LIBRARIES(test_ekos_mount ${KSTARS_UI_EKOS_LIBS})
ADD_TEST(NAME TestEkosMount COMMAND test_ekos_mount)
SET_TESTS_PROPERTIES( TestEkosMount PROPERTIES LABELS "no-xvfb;ui" TIMEOUT 600 )

ADD_EXECUTABLE(test_catalog_download ${KSTARS_UI_EKOS_SRC} test_catalog_download.cpp)
TARGET_LINK_LIBRARIES(test_catalog_download ${KSTARS_UI_EKOS_LIBS})
ADD_TEST(NAME TestCatalogDownload COMMAND test_catalog_download)
SET_TESTS_PROPERTIES( TestCatalogDownload PROPERTIES LABELS "stable;ui" TIMEOUT 600 )

ELSE ()

# JM 2010-10-15: Disable this test due to issues in CI
#ADD_EXECUTABLE(test_kstars_ui ${KSTARS_UI_TESTS_SRC})
#TARGET_LINK_LIBRARIES(test_kstars_ui ${TEST_LIBRARIES})
#ADD_TEST(NAME TestKstarsUI COMMAND test_kstars_ui)

ENDIF ()
