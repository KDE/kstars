ADD_EXECUTABLE( test_poleaxis test_poleaxis.cpp )
TARGET_LINK_LIBRARIES( test_poleaxis ${TEST_LIBRARIES})
ADD_TEST( NAME TestPoleAxis COMMAND test_poleaxis )
SET_TESTS_PROPERTIES( TestPoleAxis PROPERTIES LABELS "stable")

ADD_EXECUTABLE( test_polaralign test_polaralign.cpp )
TARGET_LINK_LIBRARIES( test_polaralign ${TEST_LIBRARIES})
ADD_TEST( NAME TestPolarAlign COMMAND test_polaralign )
SET_TESTS_PROPERTIES( TestPolarAlign PROPERTIES LABELS "stable")
ADD_CUSTOM_COMMAND( TARGET test_polaralign POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy
            ${CMAKE_CURRENT_SOURCE_DIR}/../fitsviewer/ngc4535-autofocus1.fits
            ${CMAKE_CURRENT_BINARY_DIR}/ngc4535-autofocus1.fits)
