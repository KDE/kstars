# kstars/ekos/mcp/CMakeLists.txt
target_sources(KStarsLib PRIVATE
    mcptransport.cpp
    mcptoolregistry.cpp
    mcpserver.cpp
    tools/ekostools.cpp
)

target_include_directories(KStarsLib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
