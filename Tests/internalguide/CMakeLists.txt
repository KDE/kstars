SET( StarCorrespondenceTests_SRCS teststarcorrespondence.cpp  )

IF (INDI_FOUND)
  INCLUDE_DIRECTORIES(${INDI_INCLUDE_DIR})
  ADD_EXECUTABLE( testguidestars testguidestars.cpp )
  TARGET_LINK_LIBRARIES( testguidestars ${TEST_LIBRARIES})
  ADD_TEST( NAME GuideStarsTest COMMAND testguidestars )
ENDIF ()

ADD_EXECUTABLE( teststarcorrespondence teststarcorrespondence.cpp )
TARGET_LINK_LIBRARIES( teststarcorrespondence ${TEST_LIBRARIES})
ADD_TEST( NAME StarCorrespondenceTest COMMAND teststarcorrespondence )

