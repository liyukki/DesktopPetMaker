# Third Party Notices

The machine-readable component inventory is
`licenses/THIRD_PARTY_COMPONENTS.json`.

## Qt 6.10.3

The runtime dynamically links Qt libraries and plugins. The release owner must
confirm whether the distribution uses a commercial Qt license or complies
with the applicable LGPLv3/GPLv3 route and all corresponding obligations.
License texts are included in `licenses/Qt-LGPL-3.0.txt` and
`licenses/GPL-3.0.txt`.

## LLVM Runtime

`libc++.dll` and `libunwind.dll` come from the LLVM-MinGW toolchain and are
distributed under Apache-2.0 WITH LLVM-exception. The toolchain license text
is included in `licenses/LLVM-Apache-2.0-with-LLVM-exception.txt`.

## Microsoft And Graphics Runtime Files

Qt deployment may copy Microsoft and software OpenGL runtime files. See
`licenses/Microsoft-Runtime-Notices.txt`. Release counsel must verify the exact
artifact and applicable redistribution terms.

## Shimeji Compatibility

No Java runtime or shimeji-ee source code is bundled. Import support reads
local XML metadata through Qt. Java behavior, scripting, mascot physics, and
external runtime execution are not imported.

## External Tools And AI Providers

LibreSprite and other external tools are not bundled. AI providers are
user-configured services; no API key or provider credential is distributed.

## Character Assets

The current workspace character projects are not cleared for public
redistribution and are excluded from the public runtime package. See
`ASSET_PROVENANCE.md` and `ASSET_LICENSES.json`.
