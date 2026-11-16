"""Keep host directories out of release diagnostics and assertion strings."""
from pathlib import Path

Import("env")  # noqa: F821 -- injected by SCons

for variable, public in (("PROJECT_DIR", "/crossnext"),
                         ("PROJECT_PACKAGES_DIR", "/toolchain-packages")):
    directory = Path(env.subst("$" + variable)).resolve()  # noqa: F821
    for spelling in sorted({str(directory), directory.as_posix()}):
        env.Append(CCFLAGS=["-ffile-prefix-map=" + spelling + "=" + public])  # noqa: F821
