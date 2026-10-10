# AGENTS.md - rules for AI coding agents working on NxtCut

NxtCut is a professional multi-track video editor in C++20 with Qt 6 (Widgets), MIT licensed.
You write code directly in this workspace. The developer builds and tests on his own machine.
Each task arrives as a step prompt; these rules apply to EVERY step.

## Project layout and naming
- Modules live in `engine/<module>/` (include/nxtcut/<module>/*.hpp, src/*.cpp). Tests in `tests/<module>/`.
- Namespace `nxtcut::<module>`; include as `#include <nxtcut/<module>/file.hpp>`.
- CMake targets `nxtcut_<module>` (alias `nxtcut::<module>`), test executable `nxtcut_test_<module>`.
- Sources and tests are listed EXPLICITLY in CMakeLists.txt: register every new file.
- Allowed module dependencies are enforced by `scripts/check_architecture.py`. `engine/` never includes Qt.
- Never write any old project name (the project was once called FreeCut-CPP); always "NxtCut".
- Read `docs/PROJECT_STATE.md`, `docs/ARCHITECTURE.md`, `docs/CODING_STANDARDS.md` and the existing code of
  the module you extend BEFORE writing anything. Copy its conventions.

## Hard rules
1. Check EVERY function, type and constant you call against the REAL headers in this repo. Never invent API.
   If something you need does not exist, say so in your final reply instead of making it up.
2. Do not run cmake, ninja, ctest, vcpkg or git. The developer does that (his terminal has the MSVC
   environment, yours does not). `python scripts/check_architecture.py` and read-only searches are fine.
3. Never claim you built, tested or verified anything. Report only what you changed and could not do.
4. No exceptions for expected failures: use `core::Result<T>` / `core::Status`. No `throw`, and never call
   `.value()` on a `Result` that was not just checked with `has_value()`.
5. No raw owning pointers, no `new`/`delete`, no global mutable state, no singletons, no detached threads,
   no sleep-based tests. Inject dependencies. Document thread-safety on every public class.
6. Abstract interfaces only at real boundaries (several implementations, plugins, testing). No speculative
   abstraction.
7. Time uses integer ticks. Never add floating-point time constructors to production types. Compute time
   conversions from the absolute frame index; never accumulate per-frame deltas.
8. Do not weaken or delete existing tests to make something pass. Do not edit files outside the step's scope.

## Portability (CI: MSVC 2022, GCC 13, Clang 17, Apple Clang, ASan/UBSan, TSan)
- Warnings are errors: MSVC /W4 /WX; GCC and Clang -Wall -Wextra -Wpedantic -Wconversion -Wshadow -Werror.
- Forbidden: compiler builtins and `_MSC_VER`/`__GNUC__` checks, `std::format`, `std::jthread`,
  `std::stop_token`, `std::latch`, `std::barrier`, `std::counting_semaphore`, `std::expected`
  (use tl::expected via `core::Result`), chrono calendar types, `localtime`/`gmtime`.
- Include what you use: add every standard header you use. MSVC hides missing includes that GCC rejects.
- No unused variables, parameters, lambda captures or functions (Clang/GCC fail the build). Remove them.
- Ignoring a `[[nodiscard]]` result fails the build: write `static_cast<void>(call());`.
- A loop that always `break`s on its first iteration triggers MSVC C4702 (unreachable code).
- Do not default `operator==` on structs with float members; do not default `<=>` on rational types.
- Threads need `find_package(Threads REQUIRED)` and `Threads::Threads` in the module's link dependencies.
- Concept/"must not compile" checks use templated concepts from `compile_checks.hpp`, never `requires`
  on concrete types outside a template.
- `make_error(ErrorCode, std::string_view)` is the ONLY overload. `tl::unexpected` has `.value()`;
  only `Result` has `.error()`.

## Command and naming traps
- A data field and a method must never share a name (a struct with field `label` and method `label()`
  cannot compile). Commands are plain structs with `build(...) const` and `label() const`.
- Error conventions for commands: unknown entity -> NotFound; bad parameter, locked track (message contains
  "locked"), object in use (message contains "in use"), overlap (message contains "overlap") -> InvalidArgument;
  bad index -> OutOfRange; duplicate id -> AlreadyExists. An edit that changes nothing returns an EMPTY
  ChangeSet (success).
- Locked tracks are enforced in commands' `build`, never in `apply` (so undo always works).
- Factory-only construction with `std::make_unique`: nested passkey struct with a private default
  constructor and a `friend` declaration for the owning class.

## Test rules
- Every public behaviour has a GoogleTest test; every command goes through `test::round_trip`
  (execute, validate, undo is `identical`, redo is `identical`).
- Never pick an entity with `map.begin()` (UUID order is random). Locate entities by what they are.
- Use ONE `UuidGenerator` for the fixture and the Editor in a test. Two identically seeded generators
  replay the same ids.
- Never keep a reference into a `Project` after moving it into an Editor or after a commit (dangling).
  Copy ids into plain variables first.
- Use `test::is_ok(result)` / `test::is_error(result, code)` from `nxtcut_test/assertions.hpp`.
- Use the test helpers `timeline_at_seconds` / `duration_of_seconds`; never add float conversions to
  production types.
- Every mutation test must be isolated: exactly the intended issue, no side effects.
- Where numerical correctness is critical, the step prompt provides REQUIRED test vectors. Use them
  exactly; never derive expected values from the code under test.

## Documentation
- Keep `docs/*.md` accurate to what you actually built, not what you intended. Update the relevant doc.

## Final reply format
Reply ONLY with: (1) decisions you made that the prompt did not specify, (2) the list of files changed,
(3) anything you could not do or are unsure about. Do not claim the build or tests pass.
