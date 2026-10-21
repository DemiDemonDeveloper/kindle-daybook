#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
ROOT="$PWD"
MODE="${1:-host}"
case "$MODE" in host|arm) ;; *) exit 2 ;; esac
PREFIX="$ROOT/.tools/sodium-$MODE"
if [ -f "$PREFIX/lib/libsodium.a" ]; then exit 0; fi
python3 - <<'PY'
import hashlib
from pathlib import Path
p=Path('vendor/libsodium-1.0.22-stable.tar.gz')
assert hashlib.sha256(p.read_bytes()).hexdigest()=='710c3aa4ab7d3c4afb1f09e98cf771e28cfa91aef6cddbc1c67accb674fbd4ba', 'libsodium source checksum mismatch'
PY
mkdir -p "$ROOT/.tools/sodium-$MODE-source" "$PREFIX"
tar -xzf vendor/libsodium-1.0.22-stable.tar.gz --strip-components=1 -C "$ROOT/.tools/sodium-$MODE-source"
cd "$ROOT/.tools/sodium-$MODE-source"
if [ "$MODE" = arm ]; then
 : "${ZIG:=zig}"
 export ZIG
 export ZIG_GLOBAL_CACHE_DIR="$ROOT/.tools/zig-cache"
 export ZIG_LOCAL_CACHE_DIR="$ROOT/.tools/zig-local"
 cat > "$PREFIX/cc" <<'WRAP'
#!/bin/sh
exec "$ZIG" cc -target arm-linux-musleabi -mcpu=cortex_a9 "$@"
WRAP
 cat > "$PREFIX/ar" <<'WRAP'
#!/bin/sh
exec "$ZIG" ar "$@"
WRAP
 cat > "$PREFIX/ranlib" <<'WRAP'
#!/bin/sh
exec "$ZIG" ranlib "$@"
WRAP
 chmod +x "$PREFIX/cc" "$PREFIX/ar" "$PREFIX/ranlib"
 CC="$PREFIX/cc" AR="$PREFIX/ar" RANLIB="$PREFIX/ranlib" CFLAGS='-Os' ./configure --host=arm-linux-musleabi --prefix="$PREFIX" --disable-shared --enable-static --enable-minimal --disable-asm > "$PREFIX/build.log" 2>&1
else
 CFLAGS='-O2' ./configure --prefix="$PREFIX" --disable-shared --enable-static --enable-minimal >> "$PREFIX/build.log" 2>&1
fi
make -j4 >> "$PREFIX/build.log" 2>&1
make install >> "$PREFIX/build.log" 2>&1
printf 'libsodium ready for %s\n' "$MODE"
