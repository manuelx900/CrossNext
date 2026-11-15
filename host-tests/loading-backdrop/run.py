from pathlib import Path
import argparse,os,shutil,subprocess
root=Path(__file__).resolve().parents[2]
parser=argparse.ArgumentParser(description="Verify loading overlays preserve the current frame.")
parser.add_argument("--output",type=Path,default=root/"qa-artifacts/loading-message-only/tests")
parser.add_argument("--source",type=Path,default=root/"src/components/themes/BaseTheme.cpp")
args=parser.parse_args()
out=args.output.resolve();out.mkdir(parents=True,exist_ok=True)
text=args.source.read_text(encoding="utf-8")
start=text.rfind("\n",0,text.index("BaseTheme::drawLoadingScreen("))+1
brace=text.index("{",start);end=brace+1;depth=1
while depth:
 depth+=(text[end]=="{")-(text[end]=="}");end+=1
(out/"loading-under-test.inc").write_text(text[start:end],encoding="utf-8")
wasm=os.name=="nt";compiler=str(root/".venv/emsdk/upstream/emscripten/em++.exe") if wasm else os.environ.get("CXX","c++")
flags=["-sSTACK_SIZE=1048576","-sENVIRONMENT=node","-sEXIT_RUNTIME=1","-sALLOW_MEMORY_GROWTH=1"] if wasm else []
exe=out/("test.cjs" if wasm else "test")
subprocess.run([compiler,"-std=c++20","-O2","-Wall","-Wextra","-Werror","-Wno-comment","-I"+str(out),"-Ifreeink-sdk/libs/ui/FreeInkUI/include",*flags,"host-tests/loading-backdrop/test.cpp","freeink-sdk/libs/ui/FreeInkUI/src/FreeInkUI.cpp","-o",str(exe)],cwd=root,check=True)
node=shutil.which("node") or next((root/".venv/emsdk/node").glob("*/bin/node.exe"),None)
subprocess.run(([str(node)] if wasm else [])+[str(exe)],cwd=root,check=True)
