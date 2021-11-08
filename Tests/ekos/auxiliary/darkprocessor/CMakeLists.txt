SET( DarkProcessorTests_SRCS testdefects.cpp testsubtraction.cpp )

ADD_EXECUTABLE( test_ekos_defects testdefects.cpp )
TARGET_LINK_LIBRARIES( test_ekos_defects ${TEST_LIBRARIES})
ADD_TEST( NAME DefectTest COMMAND test_ekos_defects )
SET_TESTS_PROPERTIES( DefectTest PROPERTIES LABELS "stable")

ADD_EXECUTABLE( test_ekos_subtraction testsubtraction.cpp )
TARGET_LINK_LIBRARIES( test_ekos_subtraction ${TEST_LIBRARIES})
ADD_TEST( NAME SubtractionTest COMMAND test_ekos_subtraction )
SET_TESTS_PROPERTIES( SubtractionTest PROPERTIES LABELS "stable")

ADD_CUSTOM_COMMAND( TARGET test_ekos_defects POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy
            ${CMAKE_CURRENT_SOURCE_DIR}/hotpixels.fits
            ${CMAKE_CURRENT_BINARY_DIR}/hotpixels.fits)

