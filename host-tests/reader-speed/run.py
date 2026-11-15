"""Exercise production image caches and decoders with a host HAL and fixed framebuffer."""
from pathlib import Path
import os, shutil, subprocess, sys, zipfile
ROOT=Path(__file__).resolve().parents[2]
OUT=ROOT/"qa-artifacts/reader-speed-implementation/checks"
OUT.mkdir(parents=True,exist_ok=True)
wasm=os.name=="nt"
cc=str(ROOT/".venv/emsdk/upstream/emscripten/emcc.exe") if wasm else os.environ.get("CC","cc")
cxx=str(ROOT/".venv/emsdk/upstream/emscripten/em++.exe") if wasm else os.environ.get("CXX","c++")
runtime=[shutil.which("node") or "node"] if wasm else []
flags=["-sENVIRONMENT=node","-sEXIT_RUNTIME=1","-sALLOW_MEMORY_GROWTH=1","-sNODERAWFS=1"] if wasm else []
includes=["host-tests/reader-speed/stubs","test/chapter_html_slim_parser/stubs","test/image_bw/stubs","lib/Epub","lib/Memory","lib/Performance","lib/Serialization","lib/GfxRenderer"]
def run(name,sources,extra=(),arguments=()):
    binary=OUT/(name+(".cjs" if wasm else ""))
    log=OUT/(name+".log")
    command=[cxx,"-std=c++20","-O1","-Wall","-Wextra","-Wno-unused-parameter",*flags,*["-I"+str(ROOT/p) for p in includes],*extra,*sources,"-o",str(binary)]
    with log.open("w",encoding="utf-8") as f:
        result=subprocess.run(command,cwd=ROOT,stdout=f,stderr=subprocess.STDOUT)
    if result.returncode:
        print(log.read_text(encoding="utf-8")[-12000:],flush=True)
        raise SystemExit(result.returncode)
    result=subprocess.run([*runtime,str(binary),*arguments],cwd=ROOT,capture_output=True,text=True)
    with log.open("a",encoding="utf-8") as f: f.write(result.stdout+result.stderr)
    print(name+": "+result.stdout.strip(),flush=True)
    if result.returncode: print(result.stderr,flush=True);raise SystemExit(result.returncode)
run("image-cache",["host-tests/reader-speed/images.cpp","lib/Epub/Epub/blocks/ImageBlock.cpp"])
deps=ROOT/".pio/libdeps/x4pro"
if not (deps/"PNGdec/src/PNGdec.h").is_file(): raise SystemExit("Install the x4pro PlatformIO dependencies to exercise JPEGDEC/PNGdec.")
objects=[]
for source in (deps/"PNGdec/src").glob("*.c"):
    obj=OUT/(source.stem+".o")
    subprocess.run([cc,"-O1","-w","-c",str(source),"-o",str(obj)],cwd=ROOT,check=True)
    objects.append(str(obj))
fixtures=OUT/"fixtures";fixtures.mkdir(exist_ok=True)
for kind,extension in [("jpeg","jpg"),("png","png")]:
    with zipfile.ZipFile(ROOT/("test/epubs/test_"+kind+"_images.epub")) as archive:
        name=next(n for n in archive.namelist() if n.lower().endswith((".jpg",".jpeg") if kind=="jpeg" else ".png"))
        (fixtures/("fixture."+extension)).write_bytes(archive.read(name))
run("decoder-cancellation",["host-tests/reader-speed/decoders.cpp","lib/Epub/Epub/blocks/ImageBlock.cpp","lib/Epub/Epub/converters/JpegToFramebufferConverter.cpp","lib/Epub/Epub/converters/PngToFramebufferConverter.cpp","lib/Epub/Epub/converters/ImageToFramebufferDecoder.cpp",str(deps/"JPEGDEC/src/JPEGDEC.cpp"),str(deps/"PNGdec/src/PNGdec.cpp"),*objects],["-I"+str(deps/"JPEGDEC/src"),"-I"+str(deps/"PNGdec/src")],[str(fixtures/"fixture.jpg"),str(fixtures/"fixture.png")])
