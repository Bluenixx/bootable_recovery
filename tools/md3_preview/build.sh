#!/bin/sh
# Host preview of the MD3 Expressive recovery screens (no Android build needed).
# usage: tools/md3_preview/build.sh <output-dir>   (needs g++, python3 + Pillow)
set -e
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
OUT="${1:-/tmp/md3_preview}"
mkdir -p "$OUT"
python3 - "$ROOT" "$OUT" <<'PY'
import sys
from PIL import Image
root, out = sys.argv[1:3]
open(out + '/font.raw', 'wb').write(Image.open(root + '/fonts/18x32.png').tobytes())
open(out + '/logo.raw', 'wb').write(Image.open(root + '/res-xxhdpi/images/logo_image.png').convert('RGB').tobytes())
open(out + '/wm.raw', 'wb').write(Image.open(root + '/res-xxhdpi/images/brand_wordmark.png').tobytes())
PY
g++ -std=c++20 -O2 -I"$ROOT/tools/md3_preview" -I"$ROOT/tools/md3_preview/stubs" \
  -I"$ROOT/minui/include" -I"$ROOT/recovery_ui/include" \
  "$ROOT/tools/md3_preview/preview.cpp" "$ROOT/tools/md3_preview/mock_minui.cpp" \
  "$ROOT/recovery_ui/md3_ui.cpp" -o "$OUT/md3_preview"
"$OUT/md3_preview" "$OUT/font.raw" "$OUT/"
echo "PPM previews written to $OUT"
