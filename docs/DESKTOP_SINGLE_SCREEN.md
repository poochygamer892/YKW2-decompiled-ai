# Desktop single-screen branch

This branch adds a desktop presentation/controls layer around the existing
Zakuro 3DS runtime. It follows the *product idea* of KH Melon Mix (game-specific
desktop presentation, one main view, controller-first input); it does not copy
KH Melon Mix or melonDS code, and it is not a new 3DS emulator or a complete
native reimplementation of Yo-kai Watch 2.

## Presentation and input

- The 400x240 3DS top screen fills the main 5:3 viewport, centered with black
  letterboxing as needed.
- The 320x240 touch screen appears as a small 4:3 picture-in-picture. `F1`
  hides/shows it; clicking and dragging within it sends 3DS touch coordinates.
- `F11` toggles fullscreen.
- Keyboard: WASD for Circle Pad, arrows for the D-pad, Z/X for A/B, F/G for
  Y/X, Q/E for L/R, Enter/Backspace for Start/Select. `I/J/K/L` moves a stylus
  cursor over the bottom screen; hold Space to touch, including continuous
  drags for stylus minigames. With a gamepad, the left stick moves the player,
  the right stick moves the stylus, and holding R3 presses the stylus down.
  SDL's left-stick Y is inverted at the boundary because SDL is positive-down
  while the existing mobile frontend sends positive-up to Zakuro.
- Both windows/screens are still rendered from the authorized local ROM at
  runtime. No ROM, extracted assets, keys, or recompiled game library are
  included or redistributed.

## Build and prerequisites

The desktop C++ frontend requires SDL2 development files and the Rust
`ykw2-mobile-host` shared library. Build the host library with Cargo from
`mobile-host/`, then configure CMake with `YKW2_MOBILE_HOST_LIBRARY` pointing at
the resulting `.so`, `.dylib`, or `.dll`. CMake also builds a pure presentation
test target that does not need a ROM or SDL: use `-DBUILD_DESKTOP_APP=OFF` to
build only that target.

### Windows

Install Visual Studio 2022 Build Tools (C++ workload), CMake, Rust stable with
the MSVC target, and vcpkg. Set `VCPKG_ROOT` to the vcpkg checkout and run
PowerShell from the repository root:

```powershell
.\desktop-host\build-windows.ps1
```

The script installs SDL2 with vcpkg if needed, builds the Rust runtime DLL,
builds/tests the SDL frontend, and puts `ykw2_desktop.exe`,
`ykw2_mobile_host.dll`, and `SDL2.dll` under `build\desktop-windows\Release`.
The workflow `Windows desktop build` can also be run manually on GitHub to
produce a zip artifact. It does not build Android or iOS packages.

### Linux

Example after installing SDL2 development files:

```sh
cargo build --release --manifest-path mobile-host/Cargo.toml
cmake -S desktop-host -B build/desktop \
  -DYKW2_MOBILE_HOST_LIBRARY="$PWD/mobile-host/target/release/libykw2_mobile_host.so"
cmake --build build/desktop --config Release
build/desktop/ykw2_desktop /path/to/your-decrypted-game.3ds \
  /path/to/your-recompiled-game.so
```

The recompiled-library argument is optional; whether a given ROM can boot
without it depends on the current Zakuro support for that title.

The current development environment has compiled the Linux SDL frontend and
passed its presentation tests. Windows compilation is configured for a local
MSVC/vcpkg machine and a manually dispatched Windows workflow, but has not been
run in this Linux environment. The existing Zakuro runtime remains partial, so
this frontend is not a completed, fully playable YW2 port.
