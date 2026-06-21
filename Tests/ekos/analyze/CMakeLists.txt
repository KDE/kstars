# SPDX-FileCopyrightText: 2026 Eric Dejouhanet <eric.dejouhanet@gmail.com>
# SPDX-License-Identifier: GPL-2.0-or-later

# Tests for kstars/ekos/analyze/

if (NOT BUILD_WITH_QT6)
    set(ANALYZE_TEST_LIBRARIES Qt5::Core Qt5::Network Qt5::Test)
else()
    set(ANALYZE_TEST_LIBRARIES Qt6::Core Qt6::Network Qt6::Test)
endif()

add_executable(TestAnalyzeMetrics
    TestAnalyzeMetrics.cpp
    ../../../kstars/ekos/analyze/analyzemetrics.cpp
    ../../../kstars/ekos/analyze/openmetricsserver.cpp
)
target_include_directories(TestAnalyzeMetrics PRIVATE
    ${kstars_SOURCE_DIR}/kstars
    ${kstars_BINARY_DIR}/kstars
)
target_link_libraries(TestAnalyzeMetrics ${ANALYZE_TEST_LIBRARIES})
add_test(NAME TestAnalyzeMetrics COMMAND TestAnalyzeMetrics)
set_tests_properties(TestAnalyzeMetrics PROPERTIES LABELS "stable")

add_executable(AnalyzeMetricsFixture
    AnalyzeMetricsFixture.cpp
    ../../../kstars/ekos/analyze/analyzemetrics.cpp
    ../../../kstars/ekos/analyze/openmetricsserver.cpp
)
target_include_directories(AnalyzeMetricsFixture PRIVATE
    ${kstars_SOURCE_DIR}/kstars
    ${kstars_BINARY_DIR}/kstars
)
target_link_libraries(AnalyzeMetricsFixture ${ANALYZE_TEST_LIBRARIES})
