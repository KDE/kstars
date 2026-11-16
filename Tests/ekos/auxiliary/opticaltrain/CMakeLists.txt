ADD_EXECUTABLE( testopticaltrainsettings testopticaltrainsettings.cpp )
TARGET_LINK_LIBRARIES( testopticaltrainsettings ${TEST_LIBRARIES})
ADD_TEST( NAME OpticalTrainSettingsTest COMMAND testopticaltrainsettings )
SET_TESTS_PROPERTIES( OpticalTrainSettingsTest PROPERTIES LABELS "stable")
