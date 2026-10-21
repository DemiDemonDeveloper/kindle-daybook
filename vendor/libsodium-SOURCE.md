# libsodium source

Version: 1.0.22-stable, archived source snapshot from the official distribution.

URL: https://download.libsodium.org/libsodium/releases/libsodium-1.0.22-stable.tar.gz

SHA-256: `710c3aa4ab7d3c4afb1f09e98cf771e28cfa91aef6cddbc1c67accb674fbd4ba`

The source archive is included so builds do not depend on a changing upstream stable download. `scripts/build-sodium.sh` verifies the pinned checksum before the first build. No source modifications are applied. See `libsodium.LICENSE` (ISC). Host and ARM builds are separate, static, minimal builds under the ignored `.tools` directory.
