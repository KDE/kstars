IF (INDI_FOUND)
INCLUDE_DIRECTORIES(${INDI_INCLUDE_DIR})

if (NOT WIN32)
ADD_EXECUTABLE( test_placeholderpath test_placeholderpath.cpp test_placeholderpath.qrc)
TARGET_LINK_LIBRARIES( test_placeholderpath ${TEST_LIBRARIES})
ADD_CUSTOM_COMMAND( TARGET test_placeholderpath POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy
            ${CMAKE_CURRENT_SOURCE_DIR}/testSchedulerProcessJobInfo_data.csv
            ${CMAKE_CURRENT_BINARY_DIR}/testSchedulerProcessJobInfo_data.csv)
ADD_CUSTOM_COMMAND( TARGET test_placeholderpath POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy
            ${CMAKE_CURRENT_SOURCE_DIR}/testFullNamingSequence_data_small.csv
            ${CMAKE_CURRENT_BINARY_DIR}/testFullNamingSequence_data_small.csv)
ADD_CUSTOM_COMMAND( TARGET test_placeholderpath POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy
            ${CMAKE_CURRENT_SOURCE_DIR}/testFlexibleNaming_data_small.csv
            ${CMAKE_CURRENT_BINARY_DIR}/testFlexibleNaming_data_small.csv)
ADD_CUSTOM_COMMAND( TARGET test_placeholderpath POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy
            ${CMAKE_CURRENT_SOURCE_DIR}/testFlexibleNamingChangeBehavior_data_small.csv
            ${CMAKE_CURRENT_BINARY_DIR}/testFlexibleNamingChangeBehavior_data_small.csv)
ADD_TEST( NAME TestPlaceholderPath COMMAND test_placeholderpath )
SET_TESTS_PROPERTIES( TestPlaceholderPath PROPERTIES LABELS "stable" )
endif()

ADD_EXECUTABLE( test_capturetypes test_capturetypes.cpp)
TARGET_LINK_LIBRARIES( test_capturetypes ${TEST_LIBRARIES})
ADD_TEST( NAME TestCaptureTypes COMMAND test_capturetypes )
SET_TESTS_PROPERTIES( TestCaptureTypes PROPERTIES LABELS "stable" )

ADD_EXECUTABLE( test_flatcalibrationmethod test_flatcalibrationmethod.cpp)
TARGET_LINK_LIBRARIES( test_flatcalibrationmethod ${TEST_LIBRARIES})
ADD_TEST( NAME TestFlatCalibrationMethod COMMAND test_flatcalibrationmethod )
SET_TESTS_PROPERTIES( TestFlatCalibrationMethod PROPERTIES LABELS "stable" )

ADD_EXECUTABLE( test_flatcalibrationdefaults test_flatcalibrationdefaults.cpp)
TARGET_LINK_LIBRARIES( test_flatcalibrationdefaults ${TEST_LIBRARIES})
ADD_TEST( NAME TestFlatCalibrationDefaults COMMAND test_flatcalibrationdefaults )
SET_TESTS_PROPERTIES( TestFlatCalibrationDefaults PROPERTIES LABELS "stable" )

ADD_EXECUTABLE( test_flatbrightnesscalibration test_flatbrightnesscalibration.cpp)
TARGET_LINK_LIBRARIES( test_flatbrightnesscalibration ${TEST_LIBRARIES})
ADD_TEST( NAME TestFlatBrightnessCalibration COMMAND test_flatbrightnesscalibration )
SET_TESTS_PROPERTIES( TestFlatBrightnessCalibration PROPERTIES LABELS "stable" )

ADD_EXECUTABLE( test_flatbrightnesswait test_flatbrightnesswait.cpp)
TARGET_LINK_LIBRARIES( test_flatbrightnesswait ${TEST_LIBRARIES})
ADD_TEST( NAME TestFlatBrightnessWait COMMAND test_flatbrightnesswait )
SET_TESTS_PROPERTIES( TestFlatBrightnessWait PROPERTIES LABELS "stable" )

ADD_EXECUTABLE( test_sequencejobstate test_sequencejobstate.cpp)
TARGET_LINK_LIBRARIES( test_sequencejobstate ${TEST_LIBRARIES})
ADD_TEST( NAME TestSequenceJobState COMMAND test_sequencejobstate )
SET_TESTS_PROPERTIES( TestSequenceJobState PROPERTIES LABELS "unstable" )

ADD_EXECUTABLE( test_cameraprocess test_cameraprocess.cpp)
TARGET_LINK_LIBRARIES( test_cameraprocess ${TEST_LIBRARIES})
ADD_TEST( NAME TestCameraProcess COMMAND test_cameraprocess )
SET_TESTS_PROPERTIES( TestCameraProcess PROPERTIES LABELS "stable" )

ADD_EXECUTABLE( test_capturehistorynavigation test_capturehistorynavigation.cpp)
TARGET_LINK_LIBRARIES( test_capturehistorynavigation ${TEST_LIBRARIES})
ADD_TEST( NAME TestCaptureHistoryNavigation COMMAND test_capturehistorynavigation )
SET_TESTS_PROPERTIES( TestCaptureHistoryNavigation PROPERTIES LABELS "stable" )

ADD_EXECUTABLE( test_cameradata test_cameradata.cpp)
TARGET_LINK_LIBRARIES( test_cameradata ${TEST_LIBRARIES})
TARGET_COMPILE_DEFINITIONS( test_cameradata PRIVATE KSTARS_CAMERADATA_DIR="${kstars_SOURCE_DIR}/kstars/data/cameradata")
ADD_TEST( NAME TestCameraData COMMAND test_cameradata )
SET_TESTS_PROPERTIES( TestCameraData PROPERTIES LABELS "stable" )

ENDIF ()
