add_executable(ImageBwTest ImageBwTest.cpp)
target_include_directories(ImageBwTest PRIVATE stubs ${REPO_ROOT}/lib/Epub ${REPO_ROOT}/lib/GfxRenderer)
target_link_libraries(ImageBwTest PRIVATE crosspoint_test_common GTest::gtest_main)
gtest_discover_tests(ImageBwTest)
