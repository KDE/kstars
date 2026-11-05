ADD_EXECUTABLE( test_mountmodel_halton test_mountmodel_halton.cpp )
TARGET_LINK_LIBRARIES( test_mountmodel_halton ${TEST_LIBRARIES})
ADD_TEST( NAME TestMountModelHalton COMMAND test_mountmodel_halton )
SET_TESTS_PROPERTIES( TestMountModelHalton PROPERTIES LABELS "stable")

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
            ${CMAKE_CURRENT_SOURCE_DIR}/../../fitsviewer/ngc4535-autofocus1.fits
            ${CMAKE_CURRENT_BINARY_DIR}/ngc4535-autofocus1.fits)

ADD_EXECUTABLE( testindexfiles testindexfiles.cpp )
TARGET_LINK_LIBRARIES( testindexfiles ${TEST_LIBRARIES})
ADD_TEST( NAME IndexFilesTest COMMAND testindexfiles )
SET_TESTS_PROPERTIES( IndexFilesTest PROPERTIES LABELS "stable")

ADD_EXECUTABLE( testaligndiagnosis testaligndiagnosis.cpp )
TARGET_LINK_LIBRARIES( testaligndiagnosis ${TEST_LIBRARIES})
ADD_TEST( NAME AlignDiagnosisTest COMMAND testaligndiagnosis )
SET_TESTS_PROPERTIES( AlignDiagnosisTest PROPERTIES LABELS "stable")
