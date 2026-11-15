add_executable(OpdsFeedTest OpdsFeedTest.cpp
  ${REPO_ROOT}/src/util/UrlUtils.cpp
  ${REPO_ROOT}/freeink-sdk/libs/network/Opds/src/OpdsParser.cpp
  ${REPO_ROOT}/freeink-sdk/libs/network/Opds/src/Opds2Parser.cpp
  ${REPO_ROOT}/freeink-sdk/libs/network/Opds/src/OpdsFeedParser.cpp
  ${REPO_ROOT}/freeink-sdk/libs/network/Opds/src/OpdsSearchTemplate.cpp
  ${REPO_ROOT}/freeink-sdk/libs/network/JsonSax/src/StreamingJsonParser.cpp
  ${REPO_ROOT}/lib/expat/xmlparse.c ${REPO_ROOT}/lib/expat/xmlrole.c ${REPO_ROOT}/lib/expat/xmltok.c)
target_include_directories(OpdsFeedTest PRIVATE stubs
  ${REPO_ROOT}/src
  ${REPO_ROOT}/test/chapter_html_slim_parser/stubs
  ${REPO_ROOT}/freeink-sdk/libs/network/Opds/include
  ${REPO_ROOT}/freeink-sdk/libs/network/JsonSax/include ${REPO_ROOT}/lib/expat)
target_compile_definitions(OpdsFeedTest PRIVATE XML_GE=0 XML_CONTEXT_BYTES=0)
target_link_libraries(OpdsFeedTest PRIVATE crosspoint_test_common GTest::gtest_main)
gtest_discover_tests(OpdsFeedTest)
