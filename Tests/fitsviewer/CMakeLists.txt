
ADD_EXECUTABLE( testfitsdata testfitsdata.cpp )
TARGET_LINK_LIBRARIES( testfitsdata ${TEST_LIBRARIES})
ADD_TEST( NAME FitsDataTest COMMAND testfitsdata )
ADD_CUSTOM_COMMAND( TARGET testfitsdata POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy
            ${CMAKE_CURRENT_SOURCE_DIR}/m47_sim_stars.fits
            ${CMAKE_CURRENT_BINARY_DIR}/m47_sim_stars.fits)
