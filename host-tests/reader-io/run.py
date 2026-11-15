"""Production reader I/O regressions; Windows uses the local Emscripten SDK."""
from pathlib import Path
import os
import shutil
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / ".pio/host-tests/reader-io"
OUT.mkdir(parents=True, exist_ok=True)
WASM = os.name == "nt"
CXX = str(ROOT / ".venv/emsdk/upstream/emscripten/em++.exe") if WASM else os.environ.get("CXX", "c++")
CC = str(ROOT / ".venv/emsdk/upstream/emscripten/emcc.exe") if WASM else os.environ.get("CC", "cc")
FLAGS = ["-sENVIRONMENT=node", "-sEXIT_RUNTIME=1", "-sALLOW_MEMORY_GROWTH=1", "-sFORCE_FILESYSTEM=1",
         "-lnodefs.js", "-sDEFAULT_LIBRARY_FUNCS_TO_INCLUDE=$NODEFS", "--pre-js", str(OUT / "node-fs.js")] if WASM else []
ENV = {**os.environ, "PYTHONUTF8": "1"}
ENV.setdefault("EM_CACHE", str(ROOT / ".pio/em-cache-speed-all"))

if WASM:
    (OUT / "node-fs.js").write_text("Module.preRun = [function() { FS.mkdir('/repo'); FS.mount(NODEFS, {root: process.cwd()}, '/repo'); FS.chdir('/repo'); }];")


def compile_c(source, name, flags=()):
    obj = OUT / (name + ".o")
    subprocess.run([CC, "-O1", *flags, "-c", str(ROOT / source), "-o", str(obj)], cwd=ROOT, env=ENV, check=True)
    return str(obj)


def run(name, sources, includes, extra=(), arguments=()):
    if "--only" in sys.argv and name != sys.argv[sys.argv.index("--only") + 1]:
        return
    binary = OUT / (name + (".cjs" if WASM else ""))
    log = OUT / (name + ".log")
    with log.open("w", encoding="utf-8") as stream:
        result = subprocess.run([CXX, "-std=c++20", "-O1", "-Wall", "-Wextra", "-Wno-unused-parameter",
                                 "-Wno-sign-compare", *FLAGS, *["-I" + str(ROOT / p) for p in includes],
                                 *extra, *sources, "-o", str(binary)], cwd=ROOT, env=ENV, stdout=stream,
                                stderr=subprocess.STDOUT)
    if result.returncode:
        print(log.read_text(encoding="utf-8")[-12000:], flush=True)
        raise SystemExit(result.returncode)
    result = subprocess.run([*([shutil.which("node") or "node"] if WASM else []), str(binary), *arguments],
                            cwd=ROOT, env=ENV, capture_output=True, text=True, encoding="utf-8")
    with log.open("a", encoding="utf-8") as stream:
        stream.write(result.stdout + result.stderr)
    print(name + ": " + result.stdout[-3500:], flush=True)
    if result.returncode:
        print(result.stderr, flush=True)
        raise SystemExit(result.returncode)


bidi = compile_c("lib/MiniBidi/minibidi.c", "minibidi")
def block(source, start):
    brace = source.index("{", start)
    depth, end = 1, brace + 1
    while depth:
        depth += (source[end] == "{") - (source[end] == "}")
        end += 1
    return source[start:end]

manager = (ROOT / "src/activities/ActivityManager.cpp").read_text(encoding="utf-8")
start = manager.index("      RenderLock lock;", manager.index("} else if (pendingActivity)"))
conditional = manager.index("      if (pendingAction == PendingAction::Replace)", start)
push = manager.index(" else if (pendingAction == PendingAction::Push)", conditional)
(OUT / "child-transition-under-test.inc").write_text(manager[start:conditional] + block(manager, conditional) +
                                                    block(manager, push), encoding="utf-8")
reader = (ROOT / "src/activities/reader/EpubReaderActivity.cpp").read_text(encoding="utf-8")
(OUT / "child-hook-under-test.inc").write_text(block(reader, reader.index("void EpubReaderActivity::prepareForChild()")), encoding="utf-8")
run("child-transition", ["host-tests/reader-io/transition.cpp"], [str(OUT)])
activity = (ROOT / "src/activities/library/LibraryListActivity.cpp").read_text(encoding="utf-8")
start = activity.index("bool LibraryListActivity::restoreResidentLibrary()")
end = activity.index("void LibraryListActivity::initializeLibrary()", start)
(OUT / "resident-under-test.inc").write_text(activity[start:end], encoding="utf-8")
section = (ROOT / "lib/Epub/Epub/Section.cpp").read_text(encoding="utf-8")
start = section.index("bool Section::loadReaderLayout(")
brace = section.index("{", start)
depth, end = 1, brace + 1
while depth:
    depth += (section[end] == "{") - (section[end] == "}")
    end += 1
(OUT / "profile-under-test.inc").write_text(section[start:end] + "\n" + block(section, section.index("bool Section::loadSectionFile(")), encoding="utf-8")
run("profiles", ["host-tests/reader-io/profiles.cpp"], ["lib/Epub", str(OUT)])
run("page-read-metrics", ["host-tests/reader-io/test.cpp", "lib/Epub/Epub/blocks/TextBlock.cpp",
                          "lib/Epub/Epub/CjkBreaks.cpp", "lib/MiniBidi/BidiUtils.cpp", "lib/Utf8/Utf8.cpp", bidi],
    ["host-tests/reader-io/stubs", "test/chapter_html_slim_parser/stubs", "src", "lib", "lib/Epub",
     "lib/Epub/Epub", "lib/Epub/Epub/css", "lib/AsciiText", "lib/EpdFont", "lib/Utf8", "lib/Memory",
     "lib/Serialization", "lib/MiniBidi", "lib/FsHelpers"])

if "--small" in sys.argv:
    raise SystemExit(0)
font_sources = ["lib/EpdFont/TtfEpdFont.cpp", "lib/EpdFont/EpdFont.cpp", "lib/EpdFont/EpdFontFamily.cpp", "lib/Utf8/Utf8.cpp"]
for name, source in [("vector-cache", "test.cpp"), ("prepared-font", "prepared.cpp")]:
    run(name, ["host-tests/reader-font-cache/" + source, *font_sources],
        ["host-tests/reader-font-cache/stubs", "test/sd_card_font/stubs", "lib/EpdFont", "lib/Utf8"],
        ["-DBOARD_HAS_PSRAM"])
gtest = Path(os.environ["CROSSNEXT_GTEST"])
gflags = ["-DGTEST_HAS_PTHREAD=0", "-I" + str(gtest / "include"), "-I" + str(gtest)]
objects = []
for name in ["gtest-all", "gtest_main"]:
    obj = OUT / (name + ".o")
    if not obj.exists():
        subprocess.run([CXX, "-std=c++20", "-O1", *gflags, "-c", str(gtest / "src" / (name + ".cc")),
                        "-o", str(obj)], cwd=ROOT, env=ENV, check=True)
    objects.append(str(obj))

run("sd-font", ["test/sd_card_font/SdCardFontTest.cpp", "test/sd_card_font/NotoSerifResourcesTest.cpp",
                "test/sd_card_font/LanguageUiResourcesTest.cpp", "lib/EpdFont/EpdFont.cpp",
                "lib/EpdFont/SdCardFont.cpp", "lib/Utf8/Utf8.cpp", *objects],
    ["test/sd_card_font/stubs", "test/chapter_html_slim_parser/stubs", "lib/EpdFont", "lib/Utf8",
     "lib/Memory", "lib", "."], gflags)
if os.environ.get("CROSSNEXT_IO_BASELINE"):
    baseline = Path(os.environ["CROSSNEXT_IO_BASELINE"])
    run("sd-font-before", ["test/sd_card_font/SdCardFontTest.cpp", "lib/EpdFont/EpdFont.cpp",
                           str(baseline / "lib/EpdFont/SdCardFont.cpp"), "lib/Utf8/Utf8.cpp", *objects],
        [str(baseline / "lib/EpdFont"), "test/sd_card_font/stubs", "test/chapter_html_slim_parser/stubs",
         "lib/EpdFont", "lib/Utf8", "lib/Memory", "lib", "."], [*gflags, "-DREADER_IO_BASELINE"],
        ["--gtest_filter=SdCardFontTest.GlyphRunsBatchReadsWithoutChangingReverseOrderedBitmaps"])
miniz = compile_c("lib/miniz/src/miniz_impl.c", "miniz")
run("zip-catalog", ["test/zip_file/ZipFileTest.cpp", "lib/ZipFile/ZipFile.cpp",
                    "lib/miniz/src/InflateStream.cpp", "lib/Memory/BuildScratch.cpp", "lib/hal/HalMemory.cpp", miniz, *objects],
    ["test/zip_file/stubs", "lib/ZipFile", "lib/Memory", "lib/hal", "lib/miniz/src"],
    gflags)
run("resident-index", ["test/library_index_file/LibraryIndexFileTest.cpp", "lib/LibraryIndex/LibraryIndexFile.cpp",
                       "lib/LibraryIndex/LibraryFormat.cpp", *objects],
    ["test/library_index_file/stubs", "lib/LibraryIndex", "lib", "."], gflags)

expat = [compile_c("lib/expat/" + name + ".c", name, ["-DHAVE_EXPAT_CONFIG_H", "-DXML_GE=0", "-DXML_CONTEXT_BYTES=1024", "-Ilib/expat"])
         for name in ["xmlparse", "xmlrole", "xmltok"]]
run("chapter-layout", ["test/chapter_html_slim_parser/ChapterHtmlSlimParserTest.cpp",
                       "test/chapter_html_slim_parser/ReaderReuseTest.cpp",
                       "test/chapter_html_slim_parser/ParserLinkStubs.cpp", "src/util/DictionaryLookupWord.cpp",
                       "lib/Epub/Epub/Page.cpp", "lib/Epub/Epub/blocks/TextBlock.cpp", "lib/Epub/Epub/CjkBreaks.cpp",
                       "lib/MiniBidi/BidiUtils.cpp", "lib/Epub/Epub/parsers/ChapterHtmlSlimParser.cpp", "lib/Epub/Epub/parsers/ParagraphInputCache.cpp",
                       "lib/Epub/Epub/hyphenation/Hyphenator.cpp", "lib/Epub/Epub/hyphenation/LanguageRegistry.cpp",
                       "lib/Epub/Epub/hyphenation/HyphenationCommon.cpp", "lib/Epub/Epub/hyphenation/LiangHyphenation.cpp",
                       "lib/Epub/Epub/css/CssParser.cpp", "lib/Epub/Epub/ParsedText.cpp", "lib/Epub/Epub/WordStore.cpp", "lib/Epub/Epub/WordWidthCache.cpp",
                       "lib/Epub/Epub/converters/ImageDimsProbe.cpp",
                       "lib/Epub/Epub/converters/ImageToFramebufferDecoder.cpp", "lib/Utf8/Utf8.cpp", bidi,
                       *expat, *objects],
    ["test/chapter_html_slim_parser/stubs", "src", "lib", "lib/Epub", "lib/Epub/Epub", "lib/Epub/Epub/parsers",
     "lib/Epub/Epub/css", "lib/AsciiText", "lib/EpdFont", "lib/Utf8", "lib/Memory", "lib/Serialization",
     "lib/MiniBidi", "lib/FsHelpers", "lib/XmlParserUtils", "lib/expat", "."], [*gflags, "-DREADER_REAL_HYPHENATOR"])
