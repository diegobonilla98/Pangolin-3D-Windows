# Install Pangolin on Windows 11 (vcpkg + CMake + MSVC)

This guide is for a native Windows workflow with:

- Visual Studio 2022 (MSVC)
- CMake
- vcpkg
- PowerShell

It is written to be copy-paste friendly and reproducible.

---

## 1) Prerequisites

Install:

- Visual Studio 2022 Build Tools (or full VS)
  - Workload: `Desktop development with C++`
- CMake
- Git

Open **Developer PowerShell for VS 2022** and verify compiler:

```powershell
where cl
```

Verify CMake:

```powershell
cmake --version
```

If either command fails, fix that before continuing.

---

## 2) Setup vcpkg

If you already have vcpkg installed:

```powershell
$env:VCPKG_ROOT = "<your-vcpkg-path>"
& "$env:VCPKG_ROOT\vcpkg.exe" version
```

If you need to install vcpkg:

```powershell
$env:VCPKG_ROOT = "$env:USERPROFILE\vcpkg"
git clone https://github.com/microsoft/vcpkg "$env:VCPKG_ROOT"
& "$env:VCPKG_ROOT\bootstrap-vcpkg.bat"
& "$env:VCPKG_ROOT\vcpkg.exe" version
```

Optional integration:

```powershell
& "$env:VCPKG_ROOT\vcpkg.exe" integrate install
```

---

## 3) Install Pangolin with vcpkg

```powershell
& "$env:VCPKG_ROOT\vcpkg.exe" install pangolin:x64-windows
```

Verify:

```powershell
& "$env:VCPKG_ROOT\vcpkg.exe" list | Select-String pangolin
```

---

## 4) Minimal CMake project (reference)

### `main.cpp`

```cpp
#include <pangolin/pangolin.h>

int main() {
    pangolin::CreateWindowAndBind("Pangolin Test", 640, 480);
    glEnable(GL_DEPTH_TEST);

    while (!pangolin::ShouldQuit()) {
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        pangolin::FinishFrame();
    }
    return 0;
}
```

### `CMakeLists.txt`

```cmake
cmake_minimum_required(VERSION 3.20)
project(pango_test CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Pangolin CONFIG REQUIRED)

add_executable(pango_test main.cpp)
target_link_libraries(pango_test PRIVATE ${Pangolin_LIBRARIES})
```

Note: with this vcpkg package, `${Pangolin_LIBRARIES}` is the correct link pattern.

---

## 5) Configure and Build

From the project folder:

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64 `
  -DCMAKE_TOOLCHAIN_FILE="$env:VCPKG_ROOT\scripts\buildsystems\vcpkg.cmake"

cmake --build build --config Release
```

---

## 6) Run

If Windows reports missing DLLs, prepend vcpkg runtime path:

```powershell
$env:PATH = "$env:VCPKG_ROOT\installed\x64-windows\bin;$env:PATH"
.\build\Release\pango_test.exe
```

If the Pangolin window opens, install/build is good.

---

## 7) Troubleshooting

### `cl` not found

Use **Developer PowerShell for VS 2022**.

### CMake cannot find Pangolin

Make sure configure includes:

```powershell
-DCMAKE_TOOLCHAIN_FILE="$env:VCPKG_ROOT\scripts\buildsystems\vcpkg.cmake"
```

and Pangolin is installed in `x64-windows`.

### Rebuild cleanly

```powershell
Remove-Item -Recurse -Force .\build
cmake -S . -B build -G "Visual Studio 17 2022" -A x64 `
  -DCMAKE_TOOLCHAIN_FILE="$env:VCPKG_ROOT\scripts\buildsystems\vcpkg.cmake"
cmake --build build --config Release
```

### vcpkg refresh (if ports break)

```powershell
cd $env:VCPKG_ROOT
git pull
.\bootstrap-vcpkg.bat
.\vcpkg.exe install pangolin:x64-windows
```

---

## 8) Repo Example

This repository already includes a working sample:

- `test_first/main.cpp`
- `test_first/CMakeLists.txt`

See the root `README.md` for the fastest build/run path in this repo.
