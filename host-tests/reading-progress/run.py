"""Production progress scanner, state machine and persistence; fixture parsers replace pagination."""
from pathlib import Path
import argparse, subprocess, tempfile
root=Path(__file__).resolve().parents[2]
p=argparse.ArgumentParser();p.add_argument('--emxx',default=str(root/'.venv/emsdk/upstream/emscripten/em++.exe'));a=p.parse_args()
includes=['host-tests/reading-progress/stubs','host-tests/reading-cloud/stubs','host-tests/reading-suite/stubs','lib/KOReaderSync','lib/Memory','lib/Serialization','lib/FsHelpers','src','.pio/libdeps/simulator_x4_pro/ArduinoJson/src']
files=['host-tests/reading-progress/test.cpp','src/apps_local/reading/ReadingProgressBooks.cpp','src/apps_local/reading/ReadingProgressSync.cpp','lib/KOReaderSync/ProgressComparison.cpp']
with tempfile.TemporaryDirectory(prefix='crossnext-progress-tests-') as d:
    out=Path(d)/'progress.cjs'
    subprocess.run([a.emxx,'-std=c++20','-O1','-Wall','-Wextra','-Werror','-Wno-comment','-sENVIRONMENT=node','-sEXIT_RUNTIME=1','-sALLOW_MEMORY_GROWTH=1','-sSTACK_SIZE=1048576',*['-I'+x for x in includes],*files,'-o',str(out)],cwd=root,check=True)
    subprocess.run(['node',str(out)],cwd=root,check=True)
    out=Path(d)/'document-id.cjs'
    subprocess.run([a.emxx,'-std=c++20','-O1','-Wall','-Wextra','-Werror','-sENVIRONMENT=node','-sEXIT_RUNTIME=1',*['-I'+x for x in includes],'host-tests/reading-progress/test_document_id.cpp','lib/KOReaderSync/KOReaderDocumentId.cpp','-o',str(out)],cwd=root,check=True)
    subprocess.run(['node',str(out)],cwd=root,check=True)
