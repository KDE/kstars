if (StellarSolver_FOUND)
ADD_EXECUTABLE( testfitsdata testfitsdata.cpp )
TARGET_LINK_LIBRARIES( testfitsdata ${TEST_LIBRARIES})
ADD_CUSTOM_COMMAND( TARGET testfitsdata POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy
            ${CMAKE_CURRENT_SOURCE_DIR}/m47_sim_stars.fits
            ${CMAKE_CURRENT_BINARY_DIR}/m47_sim_stars.fits)
ADD_CUSTOM_COMMAND( TARGET testfitsdata POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy
            ${CMAKE_CURRENT_SOURCE_DIR}/ngc4535-autofocus1.fits
            ${CMAKE_CURRENT_BINARY_DIR}/ngc4535-autofocus1.fits)
ADD_CUSTOM_COMMAND( TARGET testfitsdata POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy
            ${CMAKE_CURRENT_SOURCE_DIR}/ngc4535-autofocus2.fits
            ${CMAKE_CURRENT_BINARY_DIR}/ngc4535-autofocus2.fits)
ADD_CUSTOM_COMMAND( TARGET testfitsdata POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy
            ${CMAKE_CURRENT_SOURCE_DIR}/ngc4535-autofocus3.fits
            ${CMAKE_CURRENT_BINARY_DIR}/ngc4535-autofocus3.fits)
ADD_CUSTOM_COMMAND( TARGET testfitsdata POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy
            ${CMAKE_CURRENT_SOURCE_DIR}/bahtinov-focus.fits
            ${CMAKE_CURRENT_BINARY_DIR}/bahtinov-focus.fits)
endif()
