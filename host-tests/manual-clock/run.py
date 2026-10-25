"""Compile the real clock HAL with a fake RTC; no host clock is changed."""
from pathlib import Path
import os,subprocess,tempfile
root=Path(__file__).resolve().parents[2]
with tempfile.TemporaryDirectory(prefix="crossplay-clock-") as folder:
 out=Path(folder)/"clock.js"
 subprocess.run([os.environ.get("EMXX","em++"),"-std=c++17","-Ihost-tests/manual-clock/stubs","host-tests/manual-clock/test.cpp","lib/hal/HalClock.cpp","-Wl,--wrap=settimeofday","-o",str(out)],cwd=root,check=True)
 for zone,arg in [("UTC",None),("America/Punta_Arenas","punta"),("America/New_York","dst"),("Pacific/Kiritimati","positive")]:
  env=os.environ.copy();env["TZ"]=zone
  subprocess.run([os.environ.get("NODE","node"),str(out)]+([arg] if arg else []),env=env,check=True)
