# Tests/ekos/ekoslive/CMakeLists.txt

if (NOT BUILD_WITH_QT6)
    set(EKOSLIVE_WEBSOCKETS_LIB Qt5::WebSockets)
else()
    set(EKOSLIVE_WEBSOCKETS_LIB Qt6::WebSockets)
endif()

ADD_EXECUTABLE(testekoslivemedia testekoslivemedia.cpp)
TARGET_LINK_LIBRARIES(testekoslivemedia ${TEST_LIBRARIES} ${EKOSLIVE_WEBSOCKETS_LIB})
ADD_TEST(NAME TestEkosLiveMedia COMMAND testekoslivemedia)
SET_TESTS_PROPERTIES(TestEkosLiveMedia PROPERTIES LABELS "stable")
