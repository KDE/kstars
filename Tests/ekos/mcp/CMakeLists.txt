# Tests/ekos/mcp/CMakeLists.txt

if (NOT BUILD_WITH_QT6)
    set(MCP_NETWORK_LIB Qt5::Network)
else()
    set(MCP_NETWORK_LIB Qt6::Network)
endif()

ADD_EXECUTABLE(TestMCPTransport
    TestMCPTransport.cpp
    MCPTestClient.cpp
)
TARGET_LINK_LIBRARIES(TestMCPTransport ${TEST_LIBRARIES} ${MCP_NETWORK_LIB})
ADD_TEST(NAME TestMCPTransport COMMAND TestMCPTransport)
SET_TESTS_PROPERTIES(TestMCPTransport PROPERTIES LABELS "stable")

ADD_EXECUTABLE(TestMCPServer
    TestMCPServer.cpp
    MCPTestClient.cpp
)
TARGET_LINK_LIBRARIES(TestMCPServer ${TEST_LIBRARIES} ${MCP_NETWORK_LIB})
ADD_TEST(NAME TestMCPServer COMMAND TestMCPServer)
SET_TESTS_PROPERTIES(TestMCPServer PROPERTIES LABELS "stable")
