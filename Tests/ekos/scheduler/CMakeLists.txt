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

SET( SchedulerTaskQueueExecutorTests_SRCS testtaskqueueexecutor.cpp  )

ADD_EXECUTABLE( testtaskqueueexecutor testtaskqueueexecutor.cpp )
TARGET_LINK_LIBRARIES( testtaskqueueexecutor ${TEST_LIBRARIES})
ADD_TEST( NAME SchedulerTaskQueueExecutorTest COMMAND testtaskqueueexecutor )
SET_TESTS_PROPERTIES( SchedulerTaskQueueExecutorTest PROPERTIES LABELS "stable" TIMEOUT 60)

SET( SchedulerTaskQueueItemTests_SRCS testtaskqueueitem.cpp  )

ADD_EXECUTABLE( testtaskqueueitem testtaskqueueitem.cpp )
TARGET_LINK_LIBRARIES( testtaskqueueitem ${TEST_LIBRARIES})
ADD_TEST( NAME SchedulerTaskQueueItemTest COMMAND testtaskqueueitem )
SET_TESTS_PROPERTIES( SchedulerTaskQueueItemTest PROPERTIES LABELS "stable" TIMEOUT 60)

SET( SchedulerTaskQueueManagerTests_SRCS testtaskqueuemanager.cpp  )

ADD_EXECUTABLE( testtaskqueuemanager testtaskqueuemanager.cpp )
TARGET_LINK_LIBRARIES( testtaskqueuemanager ${TEST_LIBRARIES})
ADD_TEST( NAME SchedulerTaskQueueManagerTest COMMAND testtaskqueuemanager )
SET_TESTS_PROPERTIES( SchedulerTaskQueueManagerTest PROPERTIES LABELS "stable" TIMEOUT 60)

ENDIF ()
