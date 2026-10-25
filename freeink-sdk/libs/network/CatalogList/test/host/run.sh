#!/bin/sh
# Builds and runs the CatalogList host tests: XmlListParser against the expat
# FreeInkBook vendors, JsonListParser against JsonSax.
set -e
cd "$(dirname "$0")"
EXPAT=../../../../book/FreeInkBook/third_party/expat
JSONSAX=../../../JsonSax
BUILD_DIR="${TMPDIR:-/tmp}/freeink-catalog-list-host-tests"
mkdir -p "$BUILD_DIR"
for f in xmlparse xmlrole xmltok; do
  cc -c -w -DXML_GE=0 -DXML_CONTEXT_BYTES=1024 -I"$EXPAT" "$EXPAT/$f.c" -o "$BUILD_DIR/$f.o"
done
c++ -std=c++17 -Wall -Wextra -Werror -I../../include -I"$EXPAT" \
  test_xml_list_parser.cpp ../../src/XmlListParser.cpp "$BUILD_DIR"/xml*.o \
  -o "$BUILD_DIR/test_xml_list_parser"
c++ -std=c++17 -Wall -Wextra -Werror -I../../include -I"$JSONSAX/include" \
  test_json_list_parser.cpp ../../src/JsonListParser.cpp "$JSONSAX/src/StreamingJsonParser.cpp" \
  -o "$BUILD_DIR/test_json_list_parser"
"$BUILD_DIR/test_xml_list_parser"
"$BUILD_DIR/test_json_list_parser"
