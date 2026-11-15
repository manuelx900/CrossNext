add_executable(FontCacheManagerTest
  FontCacheManagerTest.cpp
  ${REPO_ROOT}/lib/GfxRenderer/FontCacheManager.cpp
  ${REPO_ROOT}/lib/Utf8/Utf8.cpp
)

target_include_directories(FontCacheManagerTest PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/stubs
  ${REPO_ROOT}/lib/GfxRenderer
  ${REPO_ROOT}/lib/Utf8
)

target_link_libraries(FontCacheManagerTest PRIVATE
  crosspoint_test_common
  GTest::gtest_main
)

gtest_discover_tests(FontCacheManagerTest)

add_executable(FontCacheManagerVectorTest
  FontCacheManagerTest.cpp
  ${REPO_ROOT}/lib/GfxRenderer/FontCacheManager.cpp
  ${REPO_ROOT}/lib/Utf8/Utf8.cpp
)
target_include_directories(FontCacheManagerVectorTest PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/stubs ${REPO_ROOT}/lib/GfxRenderer ${REPO_ROOT}/lib/Utf8
)
target_compile_definitions(FontCacheManagerVectorTest PRIVATE FCM_TEST_VECTOR)
target_link_libraries(FontCacheManagerVectorTest PRIVATE crosspoint_test_common GTest::gtest_main)
gtest_discover_tests(FontCacheManagerVectorTest TEST_PREFIX "Vector.")
