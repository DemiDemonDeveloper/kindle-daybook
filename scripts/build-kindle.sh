#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
: "${ZIG:=zig}"
export ZIG_GLOBAL_CACHE_DIR="$PWD/.tools/zig-cache"
export ZIG_LOCAL_CACHE_DIR="$PWD/.tools/zig-local"
./scripts/build-sodium.sh arm
mkdir -p build dist/daybook/bin dist/daybook/data dist/documents
"$ZIG" cc -target arm-linux-musleabi -mcpu=cortex_a9 -static -Os -std=c11 -Wall -Wextra -Werror src/daybook.c src/model.c src/store.c src/vault.c vendor/cJSON.c -I.tools/sodium-arm/include .tools/sodium-arm/lib/libsodium.a -lm -o build/daybook-arm
cp build/daybook-arm dist/daybook/bin/daybook
cp scripts/run-dashboard.sh dist/daybook/run-dashboard.sh
cp scripts/Daybook.sh dist/documents/Daybook.sh
chmod +x dist/daybook/bin/daybook dist/daybook/run-dashboard.sh dist/documents/Daybook.sh
if [ -f docs/INSTALL.md ]; then cp docs/INSTALL.md dist/INSTALL.txt; fi
printf 'Build ready in dist/. Existing data is never part of the package.\n'
