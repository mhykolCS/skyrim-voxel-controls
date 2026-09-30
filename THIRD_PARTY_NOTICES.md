# Third-party notices

- CommonLibSSE-NG 10.0.0, alandtse and contributors: GPL-3.0-or-later
  with Modding Exception and GPL-3.0 Linking Exception (with Corresponding Source).
  Its original MIT and hde64 notices remain in the dependency's `licenses` directory.
- Dear ImGui 1.92.3, copyright Omar Cornut: MIT. See `deps/imgui/LICENSE.txt`.
- fmt: MIT. spdlog: MIT. rapidcsv: BSD-3-Clause. DirectXTK and DirectXMath: MIT.
  Exact installed versions are locked by the vcpkg baseline in `vcpkg.json`.
- The Microsoft SDK/CRT, clang/LLVM, xwin, LLVM-MinGW, Wine, and vcpkg are build
  tools or system dependencies and are not included in this project's source release.
- Skyrim, SKSE runtime downloads, and Address Library are installed separately.
  They are not bundled with VoxelControls.
- nifly, Ousnius and contributors: GPL-3.0-or-later. It is used only by the
  optional native mesh generator; its exact revision is in `DEPENDENCIES.json`.
- The classic Steve skin belongs to Mojang/Microsoft. `prepare-steve.py` retrieves
  it from Mojang's official Minecraft 1.8.9 client archive and verifies the archive
  checksum. The texture and generated local assets stay in ignored `local/steve/`;
  the Minecraft texture is not included in the GPL source or a public package.
  The mesh generator and animation code are original project source.

Source dependencies are pinned in `DEPENDENCIES.json`. When distributing a binary,
include the corresponding source and all relevant dependency notices, including
CommonLibSSE-NG's exception text.
