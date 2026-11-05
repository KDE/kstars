ADD_EXECUTABLE( testimageanalysis testimageanalysis.cpp )
TARGET_LINK_LIBRARIES( testimageanalysis ${TEST_LIBRARIES})
ADD_TEST( NAME ImageAnalysisTest COMMAND testimageanalysis )
SET_TESTS_PROPERTIES( ImageAnalysisTest PROPERTIES LABELS "stable")
