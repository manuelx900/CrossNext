add_executable(RemoteTurnGateTest RemoteTurnGateTest.cpp)
target_include_directories(RemoteTurnGateTest PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../src)
target_link_libraries(RemoteTurnGateTest PRIVATE GTest::gtest_main)
gtest_discover_tests(RemoteTurnGateTest)
