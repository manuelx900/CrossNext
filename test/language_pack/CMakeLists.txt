add_executable(language_pack_test
  LanguagePackTest.cpp
  ${REPO_ROOT}/lib/I18n/LanguagePack.cpp
  ${REPO_ROOT}/lib/I18n/I18n.cpp
  ${REPO_ROOT}/lib/I18n/I18nStrings.cpp
)
target_include_directories(language_pack_test BEFORE PRIVATE stubs ${REPO_ROOT}/lib/I18n)
target_link_libraries(language_pack_test PRIVATE crosspoint_test_common GTest::gtest_main)
gtest_discover_tests(language_pack_test)
