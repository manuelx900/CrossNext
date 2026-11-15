add_executable(SdCardFontManagerTest SdCardFontManagerTest.cpp
  ${REPO_ROOT}/lib/EpdFont/SdCardFontManager.cpp)
target_include_directories(SdCardFontManagerTest PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/stubs
  ${REPO_ROOT}/test/sd_card_font/stubs
  ${REPO_ROOT}/lib/EpdFont)
target_link_libraries(SdCardFontManagerTest PRIVATE crosspoint_test_common GTest::gtest_main)
target_compile_options(SdCardFontManagerTest PRIVATE -fno-exceptions)
gtest_discover_tests(SdCardFontManagerTest)
