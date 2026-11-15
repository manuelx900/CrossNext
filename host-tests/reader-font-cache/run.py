"""Run reader font regressions through the repository's standard CMake setup."""
from pathlib import Path
import hashlib
import os
import subprocess
import tempfile

repo = Path(__file__).resolve().parents[2]
key = hashlib.sha256(str(repo).encode()).hexdigest()[:12]
build = Path(os.environ.get('CROSSNEXT_TEST_BUILD', Path(tempfile.gettempdir()) / ('crossnext-tests-' + key)))
subprocess.run(['cmake', '-S', str(repo / 'test'), '-B', str(build), '-DCMAKE_BUILD_TYPE=Release'], check=True)
subprocess.run(['cmake', '--build', str(build), '--config', 'Release', '--target', 'ReaderVectorCacheTest', 'ReaderFlashFontTest',
                'FontCacheManagerTest', 'FontCacheManagerVectorTest', 'SdCardFontTest'], check=True)
subprocess.run(['ctest', '--test-dir', str(build), '-C', 'Release', '--output-on-failure', '-R',
                'ReaderVectorCacheTest|ReaderFlashFontTest|FontCacheManagerTest|FontCacheManagerVectorTest|SdCardFontTest|NotoSerifResources'], check=True)
