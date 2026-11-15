add_executable(ChapterBuildGateTest ChapterBuildGateTest.cpp)
target_include_directories(ChapterBuildGateTest PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../src ${CMAKE_CURRENT_SOURCE_DIR}/../../lib/BlePageTurner/include ${CMAKE_CURRENT_SOURCE_DIR}/../../freeink-sdk/libs/network/BleKeyboardHost/include)
target_link_libraries(ChapterBuildGateTest PRIVATE GTest::gtest_main)
gtest_discover_tests(ChapterBuildGateTest)
