add_executable(PluginCatalogPagingTest
  PluginCatalogPagingTest.cpp
  ${REPO_ROOT}/freeink-sdk/libs/network/CatalogList/src/JsonListParser.cpp
  ${REPO_ROOT}/freeink-sdk/libs/network/JsonSax/src/StreamingJsonParser.cpp
)
target_include_directories(PluginCatalogPagingTest PRIVATE
  ${REPO_ROOT}/src/activities/plugins
  ${REPO_ROOT}/freeink-sdk/libs/network/CatalogList/include
  ${REPO_ROOT}/freeink-sdk/libs/network/JsonSax/include
)
target_link_libraries(PluginCatalogPagingTest PRIVATE crosspoint_test_common GTest::gtest_main)
gtest_discover_tests(PluginCatalogPagingTest)
