ADD_EXECUTABLE( testdiagnosis testdiagnosis.cpp )
TARGET_LINK_LIBRARIES( testdiagnosis ${TEST_LIBRARIES})
ADD_TEST( NAME DiagnosisTest COMMAND testdiagnosis )
SET_TESTS_PROPERTIES( DiagnosisTest PROPERTIES LABELS "stable")

ADD_EXECUTABLE( testdiagnosismanager testdiagnosismanager.cpp )
TARGET_LINK_LIBRARIES( testdiagnosismanager ${TEST_LIBRARIES})
ADD_TEST( NAME DiagnosisManagerTest COMMAND testdiagnosismanager )
SET_TESTS_PROPERTIES( DiagnosisManagerTest PROPERTIES LABELS "stable")
