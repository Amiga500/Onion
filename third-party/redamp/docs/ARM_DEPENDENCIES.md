# Private ARM dependency sources

`tools/build_arm_deps.sh` uses pinned, checksum-verified original sources and
stages **only ARM libraries**. It never downloads prebuilt binaries.

- **zlib 1.3.1** — https://zlib.net/fossils/zlib-1.3.1.tar.gz
  SHA-256: `9a93b2b7dfdac77ceba5a558a580e74667dd6fede4585b91eefb60f03b72df23`
- **OpenSSL 3.5.9** — https://www.openssl.org/source/openssl-3.5.9.tar.gz
  SHA-256: `603f5602e2eef00d77fbd429d34dcd5822bb301757a1bc9cdb24c670f1eb859a`
- **curl 8.23.0** — https://curl.se/download/curl-8.23.0.tar.xz
  SHA-256: `8dadd7d2536a314f982ad05a651b713f426bf7d02cc2c76cffb9796f2218e6c5`
- **Opus 1.5.2** — https://downloads.xiph.org/releases/opus/opus-1.5.2.tar.gz
  SHA-256: `65c1d2f78b9f2fb20082c38cbe47c951ad5839345876e46941612ee87f9a7ce1`

License families: zlib (zlib), OpenSSL 3 (Apache-2.0), libcurl (curl license),
Opus (BSD-3-Clause). Redistributed binaries need appropriate corresponding
license notices, and RedAmp's existing FFmpeg LGPLv3 requirements are separate.

Build on the Linux VM:

```sh
cd /root/workspace/RedOnion/src/RedAmp
make arm-deps
make player
make ffmpeg-audio
```

Or just `make` for the wrapper, dependencies, player and FFmpeg in order.

The first dependency build requires network access (unless source archives are
pre-cached), `tar`, `sha256sum`, `readelf`, `make`, `perl`, a host curl/wget
command, and the existing Zig 0.14.1 toolchain. It does not use host x86
libraries for linking. The scripts reject wrong-architecture generated outputs.

This developer build is **not** a completed security/ABI audit and is not yet
validated on Miyoo hardware.
