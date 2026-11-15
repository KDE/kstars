# Tests for kstars/hips/

ADD_EXECUTABLE( testhipsofflinelevels testhipsofflinelevels.cpp )
target_include_directories( testhipsofflinelevels PRIVATE ${kstars_SOURCE_DIR}/kstars ${kstars_BINARY_DIR}/kstars )
TARGET_LINK_LIBRARIES( testhipsofflinelevels ${TEST_LIBRARIES} )
ADD_TEST( NAME HIPSOfflineLevelsTest COMMAND testhipsofflinelevels )
SET_TESTS_PROPERTIES( HIPSOfflineLevelsTest PROPERTIES LABELS "stable" )

ADD_EXECUTABLE( testhipssources testhipssources.cpp )
target_include_directories( testhipssources PRIVATE ${kstars_SOURCE_DIR}/kstars ${kstars_BINARY_DIR}/kstars )
TARGET_LINK_LIBRARIES( testhipssources ${TEST_LIBRARIES} )
ADD_TEST( NAME HIPSSourcesTest COMMAND testhipssources )
SET_TESTS_PROPERTIES( HIPSSourcesTest PROPERTIES LABELS "stable" )
