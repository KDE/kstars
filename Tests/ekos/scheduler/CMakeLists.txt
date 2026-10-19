IF (INDI_FOUND)
INCLUDE_DIRECTORIES(${INDI_INCLUDE_DIR})

SET( SchedulerUnitTests_SRCS testschedulerunit.cpp  )

ADD_EXECUTABLE( testschedulerunit testschedulerunit.cpp )
TARGET_LINK_LIBRARIES( testschedulerunit ${TEST_LIBRARIES})
ADD_CUSTOM_COMMAND( TARGET testschedulerunit POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy
            ${CMAKE_CURRENT_SOURCE_DIR}/9filters.esq
            ${CMAKE_CURRENT_BINARY_DIR}/9filters.esq)
ADD_TEST( NAME SchedulerunitTest COMMAND testschedulerunit )
SET_TESTS_PROPERTIES( SchedulerunitTest PROPERTIES LABELS "stable" TIMEOUT 600)

ADD_EXECUTABLE( testtasktemplate testtasktemplate.cpp )
TARGET_LINK_LIBRARIES( testtasktemplate ${TEST_LIBRARIES})
TARGET_COMPILE_DEFINITIONS( testtasktemplate PRIVATE TASKQUEUE_DATA_DIR="${CMAKE_SOURCE_DIR}/kstars/data/taskqueue" )
ADD_TEST( NAME TaskTemplateTest COMMAND testtasktemplate )
SET_TESTS_PROPERTIES( TaskTemplateTest PROPERTIES LABELS "stable" )

ENDIF ()
