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

ENDIF ()
