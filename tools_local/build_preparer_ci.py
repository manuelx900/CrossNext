"""Generate the simulator database and build the portable preparation engine."""
from pathlib import Path
import os
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]

if __name__ == '__main__':
    config = ROOT / '.pio/preparer-platformio.ini'
    config.parent.mkdir(exist_ok=True)
    base = (ROOT / 'platformio.ini').read_text('utf8')
    simulator = (ROOT / 'platformio.sim.ini').read_text('utf8')
    assert base.count('extra_configs = platformio.sim.ini') == 1
    assert simulator.count('  !sdl2-config --cflags --libs\n') == 1
    config.write_text(base.replace('extra_configs = platformio.sim.ini', '') + '\n' +
                      simulator.replace('  !sdl2-config --cflags --libs\n', ''), 'utf8')
    env = {**os.environ, 'PYTHONUTF8': '1', 'PYTHONIOENCODING': 'utf8'}
    for command in [
        [sys.executable, 'scripts/gen_i18n.py'],
        [sys.executable, '-m', 'platformio', 'run', '-d', str(ROOT), '-c', str(config),
         '-e', 'simulator_x4_pro', '-t', 'compiledb'],
        [sys.executable, 'desktop-preparer/build_engine.py', '--firmware', str(ROOT)],
    ]:
        subprocess.run(command, cwd=ROOT, env=env, check=True)
