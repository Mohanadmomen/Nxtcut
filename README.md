# NxtCut-CPP

NxtCut-CPP is a professional, open-source multi-track desktop video editor inspired by modern web and desktop editing workflows. Written in modern C++20, it features a strictly decoupled architecture:

- **`engine/`**: A headless, UI-free C++20 core video editing engine handling timeline data models, undo/redo commands, keyframe animations, media decoding (FFmpeg), CPU/GPU rendering, audio mixing, export pipelines, and plugin APIs. **The engine never links or includes Qt.**
- **`app/`**: A desktop user interface built with Qt 6 Widgets that consumes the engine.

NxtCut-CPP is licensed under the permissive [MIT License](LICENSE).

---

## Architecture & Planned Modules

Engine modules are introduced in a strictly validated dependency graph:
```
core -> model -> commands -> keyframes -> storage -> media -> playback ->
render -> effects -> audio -> export -> plugins -> analysis
```

For detailed architectural contracts and design rules, see [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md).
For coding style, RAII ownership, and design principles, see [docs/CODING_STANDARDS.md](docs/CODING_STANDARDS.md).

---

## Prerequisites

Ensure the following tools are installed and available in your `PATH`:

1. **CMake**: Version 3.25 or newer.
2. **C++20 Compiler**:
   - **Windows**: MSVC v143 (Visual Studio 2022 17.4+ or newer)
   - **Linux**: GCC 12+ or Clang 15+
   - **macOS**: Apple Clang (Xcode 15+) or LLVM Clang 15+
3. **vcpkg**: Microsoft C++ Library Manager (Manifest Mode).
   - Clone vcpkg: `git clone https://github.com/microsoft/vcpkg.git`
   - Bootstrap vcpkg:
     - Windows: `.\vcpkg\bootstrap-vcpkg.bat`
     - Linux/macOS: `./vcpkg/bootstrap-vcpkg.sh`
   - Set the `VCPKG_ROOT` environment variable to your vcpkg installation path:
     - Windows (PowerShell): `$env:VCPKG_ROOT = "C:\path\to\vcpkg"`
     - Linux/macOS: `export VCPKG_ROOT="/path/to/vcpkg"`
4. **Python**: Python 3.10 or newer (used for architectural boundary validation).

---

## Note on vcpkg `builtin-baseline`

This project uses **vcpkg Manifest Mode** (`vcpkg.json`). Dependencies (`gtest`, `fmt`, `spdlog`, `nlohmann-json`) are automatically fetched and built during CMake configuration.

The `"builtin-baseline"` field inside `vcpkg.json` specifies the exact Git commit of the vcpkg registry to resolve dependency versions from, ensuring 100% reproducible builds across all machines and CI.

### Updating or Setting the Baseline

To pin `builtin-baseline` to your local vcpkg repository commit:
```bash
# On Linux/macOS
git -C "$VCPKG_ROOT" rev-parse HEAD

# On Windows (PowerShell)
git -C "$env:VCPKG_ROOT" rev-parse HEAD
```
Copy the 40-character commit SHA output and paste it into the `"builtin-baseline"` property in `vcpkg.json`.

---

## Build & Test Instructions

### Windows (MSVC)

#### Using CMake Presets (Recommended)
```powershell
# 1. Ensure VCPKG_ROOT is defined in your environment
$env:VCPKG_ROOT = "C:\vcpkg"

# 2. Configure using Windows Debug preset
cmake --preset windows-debug

# 3. Build all targets
cmake --build --preset windows-debug

# 4. Run tests (both GoogleTest suites and architecture checks)
ctest --preset windows-debug --output-on-failure
```

#### Manual CMake Commands
```powershell
cmake -B build -S . `
  -DCMAKE_TOOLCHAIN_FILE="$env:VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake" `
  -DCMAKE_BUILD_TYPE=Debug

cmake --build build --config Debug
ctest --test-dir build -C Debug --output-on-failure
```

---

### Linux (GCC / Clang)

#### Using CMake Presets (Recommended)
```bash
# 1. Ensure VCPKG_ROOT is defined in your environment
export VCPKG_ROOT="/opt/vcpkg"

# 2. Configure using Debug preset
cmake --preset debug

# 3. Build all targets
cmake --build --preset debug

# 4. Run test suite
ctest --preset debug --output-on-failure
```

#### Running Sanitizers
```bash
# ASan + UBSan
cmake --preset asan-ubsan
cmake --build --preset asan-ubsan
ctest --preset asan-ubsan --output-on-failure

# TSan (ThreadSanitizer)
cmake --preset tsan
cmake --build --preset tsan
ctest --preset tsan --output-on-failure
```

#### Manual CMake Commands
```bash
cmake -B build -S . \
  -DCMAKE_TOOLCHAIN_FILE="${VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake" \
  -DCMAKE_BUILD_TYPE=Debug

cmake --build build
ctest --test-dir build --output-on-failure
```

---

### macOS (Apple Clang)

#### Using CMake Presets
```bash
# 1. Ensure VCPKG_ROOT is defined in your environment
export VCPKG_ROOT="$HOME/vcpkg"

# 2. Configure using Debug preset
cmake --preset debug

# 3. Build all targets
cmake --build --preset debug

# 4. Run test suite
ctest --preset debug --output-on-failure
```

---

## Architectural Enforcement

To ensure strict layering, `scripts/check_architecture.py` verifies:
1. No Qt header `<Q...>` or `<Qt...>` is ever included in `engine/`.
2. No Qt target (`Qt6::*`) is ever linked in `engine/`.
3. Engine modules strictly obey the allowed dependency hierarchy table.

You can run the architectural check manually at any time:
```bash
# Standalone execution
python3 scripts/check_architecture.py

# Or via CTest after configuring CMake
ctest -R check_architecture --output-on-failure
```

---

## Code Quality & Formatting

- **clang-format**: Code formatting is enforced using `.clang-format` (Google style, 100 column limit, 4 spaces).
  ```bash
  clang-format -i engine/core/include/nxtcut/core/*.hpp engine/core/src/*.cpp tests/core/*.cpp
  ```
- **clang-tidy**: Static analysis configuration is maintained in `.clang-tidy`.

---

## License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.
