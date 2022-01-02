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

ADD_EXECUTABLE( test_sequencejobstate test_sequencejobstate.cpp)
TARGET_LINK_LIBRARIES( test_sequencejobstate ${TEST_LIBRARIES})
ADD_TEST( NAME TestSequenceJobState COMMAND test_sequencejobstate )
SET_TESTS_PROPERTIES( TestSequenceJobState PROPERTIES LABELS "stable" )

ENDIF ()
