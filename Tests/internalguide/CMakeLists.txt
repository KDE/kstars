SET( StarCorrespondenceTests_SRCS teststarcorrespondence.cpp  )

IF (INDI_FOUND)
  INCLUDE_DIRECTORIES(${INDI_INCLUDE_DIR})
  ADD_EXECUTABLE( testguidestars testguidestars.cpp )
  TARGET_LINK_LIBRARIES( testguidestars ${TEST_LIBRARIES})
  ADD_TEST( NAME GuideStarsTest COMMAND testguidestars )
  SET_TESTS_PROPERTIES( GuideStarsTest PROPERTIES LABELS "stable")
ENDIF ()

ADD_EXECUTABLE( teststarcorrespondence teststarcorrespondence.cpp )
TARGET_LINK_LIBRARIES( teststarcorrespondence ${TEST_LIBRARIES})
ADD_TEST( NAME StarCorrespondenceTest COMMAND teststarcorrespondence )
SET_TESTS_PROPERTIES( StarCorrespondenceTest PROPERTIES LABELS "stable")

ADD_EXECUTABLE( testcalibrationprocess testcalibrationprocess.cpp )
TARGET_LINK_LIBRARIES( testcalibrationprocess ${TEST_LIBRARIES})
ADD_TEST( NAME CalibrationProcessTest COMMAND testcalibrationprocess )
SET_TESTS_PROPERTIES( CalibrationProcessTest PROPERTIES LABELS "stable")

