if (NOVA_FOUND)
ADD_EXECUTABLE( test_skypoint test_skypoint.cpp )
TARGET_LINK_LIBRARIES( test_skypoint ${TEST_LIBRARIES})
ADD_TEST( NAME TestSkyPoint COMMAND test_skypoint )
endif()

ADD_EXECUTABLE( test_starobject test_starobject.cpp )
TARGET_LINK_LIBRARIES( test_starobject ${TEST_LIBRARIES} ${ERFA_LIBRARIES})
ADD_TEST( NAME TestStarobject COMMAND test_starobject )
