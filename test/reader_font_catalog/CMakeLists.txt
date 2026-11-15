add_executable(ReaderFontCatalogTest ReaderFontCatalogTest.cpp ${REPO_ROOT}/src/ReaderFontSizes.cpp)
target_include_directories(ReaderFontCatalogTest PRIVATE ${REPO_ROOT}/src ${REPO_ROOT}/lib/EpdFont)
target_link_libraries(ReaderFontCatalogTest PRIVATE GTest::gtest_main)
gtest_discover_tests(ReaderFontCatalogTest)
