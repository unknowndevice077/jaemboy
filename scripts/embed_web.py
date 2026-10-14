# PlatformIO pre-build script: gzips data/controller.html into
# include/controller_html.h so the page lives in flash (PROGMEM) and no
# separate filesystem upload is needed.
import gzip
import os

Import("env")  # noqa: F821  (provided by PlatformIO)

root = env.subst("$PROJECT_DIR")  # noqa: F821
src = os.path.join(root, "data", "controller.html")
dst = os.path.join(root, "include", "controller_html.h")

with open(src, "rb") as f:
    raw = f.read()
data = gzip.compress(raw, compresslevel=9, mtime=0)

lines = []
for i in range(0, len(data), 16):
    lines.append("  " + ", ".join("0x%02x" % b for b in data[i:i + 16]) + ",")

header = (
    "// Generated from data/controller.html by scripts/embed_web.py. Do not edit.\n"
    "#pragma once\n#include <Arduino.h>\n\n"
    "const uint8_t CONTROLLER_HTML_GZ[] PROGMEM = {\n" + "\n".join(lines) + "\n};\n"
    "const size_t CONTROLLER_HTML_GZ_LEN = %d;\n" % len(data)
)

old = open(dst).read() if os.path.exists(dst) else None
if old != header:  # avoid needless rebuilds
    with open(dst, "w") as f:
        f.write(header)
print("embed_web: %d bytes html -> %d bytes gzip" % (len(raw), len(data)))
