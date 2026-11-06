#!/bin/sh
# Copies the VADER engine (github.com/sunskiefer/VADER, MIT) into vader/ and applies tools/vader_patch.py.
#
#   tools/vader_vendor.sh /path/to/VADER
#
# What comes along: the engine (src/, without the Force's display plane and web server), the ZYNTH voices (src/zynth)
# with the Mutable Instruments code they use (MIT), stb_image (public domain / MIT), the desktop build's stand-ins for the display and web server
# (desktop/view_bus.c, desktop/compat.h), the example shapes and every licence. Not the example samples (taken from
# commercial sample packs), and not the LXR02 firmware's wavetables (their licence forbids commercial use, which the
# plugin's GPL-3.0 cannot carry): tools/umlauf_wavetables.py computes tables of the same shape instead.
set -e
UP=$1
[ -f "$UP/src/engine.c" ] || { echo "usage: $0 /path/to/VADER" >&2; exit 1; }
ROOT=$(cd "$(dirname "$0")/.." && pwd)
D=$ROOT/vader
rm -rf "$D"
mkdir -p "$D/src/zynth/shim" "$D/desktop" "$D/shapes" "$D/third_party/mpc-vst-plugins/wrapper" "$D/third_party/stb" \
         "$D/third_party/mutable"
cd "$UP"
for f in tables.c shape.c paths.c modes.c shapefile.c screen.c fx.c pictures.c qr.c engine.c \
         vader.h vader_params.h overlay.h web.h qr.h version.h mirror_schema.h; do
    cp "src/$f" "$D/src/"
done
cp src/zynth/*.c src/zynth/*.cc src/zynth/*.h "$D/src/zynth/"
cp src/zynth/shim/* "$D/src/zynth/shim/"
cp desktop/view_bus.c desktop/compat.h "$D/desktop/"
cp shapes/* "$D/shapes/"
cp third_party/mpc-vst-plugins/wrapper/engine.h "$D/third_party/mpc-vst-plugins/wrapper/"
cp third_party/mpc-vst-plugins/LICENSE* "$D/third_party/mpc-vst-plugins/" 2>/dev/null || true
cp third_party/stb/* "$D/third_party/stb/"
cp -r third_party/mutable/braids third_party/mutable/plaits third_party/mutable/stmlib third_party/mutable/LICENSE \
   "$D/third_party/mutable/"
cp LICENSE "$D/LICENSE"
cp README.md "$D/README-VADER.md"
git rev-parse HEAD > "$D/VENDORED_COMMIT" 2>/dev/null || echo unknown > "$D/VENDORED_COMMIT"
python3 "$ROOT/tools/umlauf_wavetables.py" "$D/src/zynth/lxrtab"
python3 "$ROOT/tools/vader_patch.py" "$D"
echo "vendored VADER $(cat "$D/VENDORED_COMMIT") into vader/"
