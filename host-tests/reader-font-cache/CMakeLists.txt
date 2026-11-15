add_executable(ReaderVectorCacheTest
  test.cpp
  ${REPO_ROOT}/lib/EpdFont/TtfEpdFont.cpp
  ${REPO_ROOT}/lib/EpdFont/EpdFont.cpp
  ${REPO_ROOT}/lib/EpdFont/EpdFontFamily.cpp
  ${REPO_ROOT}/lib/Utf8/Utf8.cpp
)
target_include_directories(ReaderVectorCacheTest PRIVATE
  stubs ${REPO_ROOT}/test/sd_card_font/stubs ${REPO_ROOT}/lib/EpdFont ${REPO_ROOT}/lib/Utf8
)
target_compile_definitions(ReaderVectorCacheTest PRIVATE BOARD_HAS_PSRAM)
target_link_libraries(ReaderVectorCacheTest PRIVATE crosspoint_test_common)
add_test(NAME ReaderVectorCacheTest COMMAND ReaderVectorCacheTest)

add_executable(PreparedReaderFontTest
  prepared.cpp
  ${REPO_ROOT}/lib/EpdFont/TtfEpdFont.cpp
  ${REPO_ROOT}/lib/EpdFont/EpdFont.cpp
  ${REPO_ROOT}/lib/EpdFont/EpdFontFamily.cpp
  ${REPO_ROOT}/lib/Utf8/Utf8.cpp
)
target_include_directories(PreparedReaderFontTest PRIVATE
  stubs ${REPO_ROOT}/test/sd_card_font/stubs ${REPO_ROOT}/lib/EpdFont ${REPO_ROOT}/lib/Utf8)
target_compile_definitions(PreparedReaderFontTest PRIVATE BOARD_HAS_PSRAM)
target_link_libraries(PreparedReaderFontTest PRIVATE crosspoint_test_common)
add_test(NAME PreparedReaderFontTest COMMAND PreparedReaderFontTest)

add_executable(ReaderFlashFontTest
  flash.cpp
  ${REPO_ROOT}/lib/EpdFont/EpdFont.cpp
  ${REPO_ROOT}/lib/EpdFont/EpdFontFamily.cpp
  ${REPO_ROOT}/lib/Utf8/Utf8.cpp
)
target_include_directories(ReaderFlashFontTest PRIVATE ${REPO_ROOT}/lib/EpdFont ${REPO_ROOT}/lib/Utf8)
target_compile_options(ReaderFlashFontTest PRIVATE -Wno-missing-field-initializers)
target_link_libraries(ReaderFlashFontTest PRIVATE crosspoint_test_common)
add_test(NAME ReaderFlashFontTest COMMAND ReaderFlashFontTest)
