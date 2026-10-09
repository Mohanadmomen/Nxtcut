find_package(GTest REQUIRED)

add_library(nxtcut_test_support INTERFACE)
target_include_directories(nxtcut_test_support INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(nxtcut_test_support INTERFACE nxtcut::model nxtcut::core GTest::gtest)
