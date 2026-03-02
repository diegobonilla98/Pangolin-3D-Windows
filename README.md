# Pangolin on Windows 11 (vcpkg + CMake + MSVC)

Practical, working setup for running Pangolin natively on Windows with Visual Studio 2022 and vcpkg.

This repo currently contains:

- `test_first/` - a verified Pangolin C++ sample (`main.cpp`)
- `INSTALL_PANGOLIN_WINDOWS.md` - install notes

If you want to get running fast, follow the quick start below.

---

## Quick Start

From PowerShell:

```powershell
# 1) Open Developer PowerShell for VS 2022 (recommended), then:
where cl

# 2) Set vcpkg root (adjust if needed)
$env:VCPKG_ROOT = "$env:USERPROFILE\vcpkg"

# 3) Install Pangolin (safe to run again)
& "$env:VCPKG_ROOT\vcpkg.exe" install pangolin:x64-windows

# 4) Build sample
cd <path-to-your-cloned-repo>\test_first
cmake -S . -B build -G "Visual Studio 17 2022" -A x64 `
  -DCMAKE_TOOLCHAIN_FILE="$env:VCPKG_ROOT\scripts\buildsystems\vcpkg.cmake"
cmake --build build --config Release

# 5) Run sample
$env:PATH = "$env:VCPKG_ROOT\installed\x64-windows\bin;$env:PATH"
.\build\Release\pango_test.exe
```

If a Pangolin window opens, your setup is good.

---

## Requirements

- Windows 11
- Visual Studio 2022 Build Tools (or full VS) with:
  - `Desktop development with C++`
- CMake
- vcpkg
- PowerShell

### Verify compiler

```powershell
where cl
```

If not found, open **Developer PowerShell for VS 2022**.

### Verify CMake

```powershell
cmake --version
```

### Verify vcpkg

```powershell
$env:VCPKG_ROOT = "$env:USERPROFILE\vcpkg"
& "$env:VCPKG_ROOT\vcpkg.exe" version
```

---

## Repo Structure

```text
Pangolin/
├─ README.md
├─ INSTALL_PANGOLIN_WINDOWS.md
└─ test_first/
   ├─ CMakeLists.txt
   ├─ main.cpp
   └─ build_command.bat
```

---

## Build and Run `test_first`

From `<path-to-your-cloned-repo>\test_first`:

```powershell
$env:VCPKG_ROOT = "$env:USERPROFILE\vcpkg"

cmake -S . -B build -G "Visual Studio 17 2022" -A x64 `
  -DCMAKE_TOOLCHAIN_FILE="$env:VCPKG_ROOT\scripts\buildsystems\vcpkg.cmake"

cmake --build build --config Release

$env:PATH = "$env:VCPKG_ROOT\installed\x64-windows\bin;$env:PATH"
.\build\Release\pango_test.exe
```

---

## How CMake Linking Works Here

`test_first/CMakeLists.txt` uses:

```cmake
find_package(Pangolin CONFIG REQUIRED)
target_link_libraries(pango_test PRIVATE ${Pangolin_LIBRARIES})
```

This is the correct pattern for the vcpkg Pangolin package used in this repo.

---

## Troubleshooting

### `cl` not found

Use **Developer PowerShell for VS 2022**.

### Missing DLLs when launching executable

Add vcpkg runtime path in the same terminal:

```powershell
$env:PATH = "$env:VCPKG_ROOT\installed\x64-windows\bin;$env:PATH"
```

### CMake cannot find Pangolin

Ensure toolchain flag is passed during configure:

```powershell
-DCMAKE_TOOLCHAIN_FILE="$env:VCPKG_ROOT\scripts\buildsystems\vcpkg.cmake"
```

Also verify package is installed:

```powershell
& "$env:VCPKG_ROOT\vcpkg.exe" list | Select-String pangolin
```

### Safe rebuild from scratch

```powershell
Remove-Item -Recurse -Force .\build
cmake -S . -B build -G "Visual Studio 17 2022" -A x64 `
  -DCMAKE_TOOLCHAIN_FILE="$env:VCPKG_ROOT\scripts\buildsystems\vcpkg.cmake"
cmake --build build --config Release
```

---

## Notes

- This repo is focused on native Windows builds (not WSL).
- `INSTALL_PANGOLIN_WINDOWS.md` contains additional installation details and background.
