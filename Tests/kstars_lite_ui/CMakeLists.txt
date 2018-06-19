
SET(KSTARS_LITE_UI_TESTS_SRC kstars_lite_ui_tests.cpp)

ECM_ADD_APP_ICON(KSTARS_UI_TESTS_SRC ICONS
        ../../kstars/icons/16-apps-kstars.png
        ../../kstars/icons/32-apps-kstars.png
        ../../kstars/icons/48-apps-kstars.png
        ../../kstars/icons/64-apps-kstars.png
        ../../kstars/icons/128-apps-kstars.png
        )

ADD_EXECUTABLE(kstars_lite_ui_tests ${KSTARS_LITE_UI_TESTS_SRC})
TARGET_LINK_LIBRARIES(kstars_lite_ui_tests ${TEST_KSLITE_LIBRARIES} -lpthread)

IF (INDI_FOUND)
    INCLUDE_DIRECTORIES(${INDI_INCLUDE_DIR})
    TARGET_LINK_LIBRARIES(kstars_lite_ui_tests ${INDI_CLIENT_QT_LIBRARIES} ${NOVA_LIBRARIES} z)
ENDIF ()

ADD_TEST(NAME KStarsLiteUiTests COMMAND ${CMAKE_CURRENT_BINARY_DIR}/kstars)
