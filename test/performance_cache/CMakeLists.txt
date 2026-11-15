add_executable(PerformanceCacheTest PerformanceCacheTest.cpp)
target_include_directories(PerformanceCacheTest PRIVATE ${REPO_ROOT}/test/library_index_file/stubs ${REPO_ROOT}/lib/Memory)
target_link_libraries(PerformanceCacheTest PRIVATE crosspoint_test_common)
add_test(NAME PerformanceCacheTest COMMAND PerformanceCacheTest)
