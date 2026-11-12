# Build tools

`../build.ps1` supports a local llvm-mingw x64 UCRT toolchain or CMake with
MSVC and the Windows SDK. No global PATH changes are required.

The reproducible portable toolchain used for this release is:
https://github.com/mstorsjo/llvm-mingw/releases/tag/20261104

Asset: llvm-mingw-20261104-ucrt-x86_64.zip
SHA-256: 552191b5754c49a5a20723276a6fc56541ea1922c4c50b7b669022333fc1b40f

Inno Setup 6.7.4 is used for the per-user installer:
https://github.com/jrsoftware/issrc/releases/tag/is-6_7_4

Run `./bootstrap.ps1` to fetch and verify the portable toolchain. Add `-Inno`
to install the signed Inno compiler into this tools folder. The bootstrap
requires network access. The built app needs neither tool installed.
