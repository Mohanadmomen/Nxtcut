# FreeCut-CPP Coding Standards

These coding standards are mandatory across the entire codebase (both `engine/` and `app/`). All code is checked against these rules in code reviews, static analysis (`clang-tidy`), and automated tests.

---

## 1. Language & Compiler Standards

- **Standard**: Modern C++20 (`-std=c++20`).
- **Header Guards**: `#pragma once` is mandatory at the beginning of every header.
- **Portability**: No compiler-specific extensions (`__attribute__`, `#pragma omp`, MSVC `__declspec` without portability macros).
- **Compilation**: Code must compile warning-free with `-Wall -Wextra -Wpedantic -Wconversion -Wshadow -Werror` (GCC/Clang) and `/W4 /WX /permissive-` (MSVC).

---

## 2. Naming Conventions

| Category | Style | Example | Notes |
| :--- | :--- | :--- | :--- |
| **Types, Classes, Structs** | `PascalCase` | `TimelineModel`, `FrameBuffer` | Concrete & abstract types |
| **Enums & Enum Values** | `PascalCase` | `enum class TrackKind { Video, Audio };` | Scoped `enum class` only |
| **Functions & Methods** | `snake_case` | `render_frame()`, `duration()` | Action or accessor |
| **Local Variables** | `snake_case` | `clip_count`, `target_time` | Clear descriptive names |
| **Member Variables** | `snake_case_` | `duration_`, `playback_speed_` | Trailing underscore for private members |
| **Constants & Enumerators** | `kPascalCase` | `kMaxTrackCount`, `kDefaultFrameRate` | Compile-time or static constants |
| **Namespaces** | `lowercase` | `freecut::core`, `freecut::model` | Nested, concise |
| **Files & Directories** | `snake_case` | `timeline_model.hpp`, `version.cpp` | Mirror primary type or responsibility |

---

## 3. Memory & Resource Ownership (RAII)

- **Strict RAII**: All resources (memory, file handles, FFmpeg contexts, GPU textures) must be managed by RAII wrappers.
- **No Manual Management**: `new` and `delete` are forbidden.
- **Default Ownership**: Use `std::unique_ptr` for exclusive ownership.
- **Shared Ownership**: Use `std::shared_ptr` / `std::weak_ptr` *only* when ownership is genuinely shared across independent asynchronous lifecycles. Document shared ownership rationale at the declaration site.
- **Non-Owning Access**:
  - Prefer references (`const T&` or `T&`) for guaranteed non-null instances.
  - Use `std::string_view` for string parameters.
  - Use `std::span<T>` / `std::span<const T>` for contiguous sequences.
  - Use raw pointers (`T*` or `const T*`) *strictly* for non-owning, nullable handles.

---

## 4. State & Dependency Management

- **No Global Mutable State**: Mutable global variables and function-local static variables holding mutable state are prohibited.
- **No Singletons**: The singleton pattern is banned.
- **Explicit Dependency Injection**: Pass all collaborators explicitly via constructors or method parameters. This keeps components testable, re-entrant, and deterministic.

---

## 5. Error Handling Architecture

- **Expected Failures**: Do not use exceptions for expected operational failures (e.g. invalid user input, unsupported codec, missing file, disk full).
- **Result Pattern**: Use a value-based `Result<T>` or `std::expected`-style error type for expected failures.
- **Programmer Errors & Preconditions**: Exceptions are reserved solely for unrecoverable logic bugs and internal invariant violations (e.g., standard library memory exhaustion).
- **Module Boundary Invariant**: Exceptions must **never** escape across engine module boundaries or across the engine-to-application boundary.

---

## 6. Software Design & Clean Architecture

- **Single Responsibility Principle**: One primary responsibility per class and per file.
- **Function Size**: Functions should be small and focused. Aim for under 40 lines.
- **Composition over Inheritance**: Favor object composition over class inheritance hierarchies.
- **Abstract Interfaces**: Introduce pure virtual interfaces (`class IFoo`) *only* when there are genuinely multiple implementations (e.g., CPU vs. GPU render backends) or when required for mocking in tests. Never add speculative abstractions.
- **Const Correctness**: Mark all member functions that do not modify observable state as `const`.
- **Nodiscard**: Apply `[[nodiscard]]` to functions returning error codes, tokens, calculations, or ownership handles where discarding the result constitutes a likely bug.
- **Explicit Constructors**: Single-argument constructors must be marked `explicit` to prevent unintended implicit conversions.
- **Noexcept**: Apply `noexcept` to move operations, swap, simple accessors, and functions guaranteed never to throw.

---

## 7. Documentation & Comments

- **Doxygen for Public APIs**: Every public class, struct, function, and interface must have clear Doxygen documentation (`/** ... */`).
- **Thread Safety Guarantees**: Every public class must explicitly document its thread safety guarantees in its Doxygen comment (e.g. `Thread-safe`, `Thread-compatible`, or `Main-thread only`).
- **Code Comments**: Implementation comments must explain **WHY** an approach was chosen, edge cases handled, or non-obvious algorithms, never merely restating **WHAT** the code does.

---

## 8. Unit Testing Conventions

- **Framework**: GoogleTest (`gtest`).
- **File Structure**: Test directories and files directly mirror the production source code:
  - Production: `engine/core/src/version.cpp`
  - Test: `tests/core/version_test.cpp`
- **Isolation**: Each test case must be completely independent and deterministic. No dependencies on network access, ambient system time, or test execution order.
- **Speed**: Tests must execute in milliseconds. Heavy media rendering tests should use minimal synthetic fixtures.
