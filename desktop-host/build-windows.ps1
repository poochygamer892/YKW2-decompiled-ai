$ErrorActionPreference = 'Stop'

if (-not $env:VCPKG_ROOT) {
    if ($env:VCPKG_INSTALLATION_ROOT) {
        $env:VCPKG_ROOT = $env:VCPKG_INSTALLATION_ROOT
    } else {
        throw 'Instala vcpkg y define VCPKG_ROOT antes de compilar.'
    }
}

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$buildRoot = Join-Path $repoRoot 'build\desktop-windows'
$cargoTarget = Join-Path $repoRoot 'mobile-host\target'
$hostDll = Join-Path $cargoTarget 'release\ykw2_mobile_host.dll'
$hostImport = Join-Path $cargoTarget 'release\ykw2_mobile_host.dll.lib'
$sdlDll = Join-Path $env:VCPKG_ROOT 'installed\x64-windows\bin\SDL2.dll'
$vcpkgToolchain = Join-Path $env:VCPKG_ROOT 'scripts\buildsystems\vcpkg.cmake'

if (-not (Test-Path $sdlDll)) {
    & (Join-Path $env:VCPKG_ROOT 'vcpkg.exe') install sdl2:x64-windows
    if ($LASTEXITCODE -ne 0) { throw 'No se pudo instalar SDL2 con vcpkg.' }
}

rustup target add x86_64-pc-windows-msvc
if ($LASTEXITCODE -ne 0) { throw 'No se pudo instalar el target Rust MSVC.' }

$env:CARGO_TARGET_DIR = $cargoTarget
cargo build --release --target x86_64-pc-windows-msvc --manifest-path (Join-Path $repoRoot 'mobile-host\Cargo.toml')
if ($LASTEXITCODE -ne 0) { throw 'Falló la compilación del host Zakuro para Windows.' }

$hostDll = Join-Path $cargoTarget 'x86_64-pc-windows-msvc\release\ykw2_mobile_host.dll'
$hostImport = Join-Path $cargoTarget 'x86_64-pc-windows-msvc\release\ykw2_mobile_host.dll.lib'
$sdlDll = Join-Path $env:VCPKG_ROOT 'installed\x64-windows\bin\SDL2.dll'

cmake -S (Join-Path $repoRoot 'desktop-host') -B $buildRoot `
    -DCMAKE_TOOLCHAIN_FILE=$vcpkgToolchain `
    -DVCPKG_TARGET_TRIPLET=x64-windows `
    -DYKW2_MOBILE_HOST_LIBRARY=$hostDll `
    -DYKW2_MOBILE_HOST_LINK_LIBRARY=$hostImport `
    -DYKW2_SDL2_RUNTIME_DLL=$sdlDll
if ($LASTEXITCODE -ne 0) { throw 'Falló la configuración de CMake para Windows.' }

cmake --build $buildRoot --config Release --parallel
if ($LASTEXITCODE -ne 0) { throw 'Falló la compilación del frontend Windows.' }

ctest --test-dir $buildRoot -C Release --output-on-failure
if ($LASTEXITCODE -ne 0) { throw 'Fallaron las pruebas del frontend Windows.' }

$exe = Join-Path $buildRoot 'Release\ykw2_desktop.exe'
if (-not (Test-Path $exe)) { throw "No se encontró el ejecutable esperado: $exe" }
Write-Host "Build lista: $exe"
Write-Host 'Copia también SDL2.dll y ykw2_mobile_host.dll (CMake los deja junto al EXE).'
