# CLAUDE.md - instructions for Claude (design, review, prompt writing)

Read this file, `CLAUDE.local.md` (if present), `docs/PROJECT_STATE.md` (where we are) and `docs/ROADMAP.md` (the full
plan) at the start of every new chat, then continue from the "Next" section of PROJECT_STATE.
`AGENTS.md` is for the coding agent (Gemini in Antigravity); do not paste it anywhere, it is read automatically.

## Roles
- Claude: designs, reviews code, writes ONE prompt per step for the coding agent, diagnoses real errors.
  Claude does not write C++ for a step. Exception: small mechanical fixes and cleanups, applied directly
  with the filesystem tools and checked first (see "Checks").
- Coding agent (Gemini via Antigravity): writes the code in the workspace from the step prompt.
- Maintainer: runs `scripts\verify.cmd`, commits, pushes, opens pull requests, reports CI results.

## How to communicate with the maintainer
Personal preferences (tone, step-by-step style, Windows commands) are in `CLAUDE.local.md` (private, git-ignored).
Read it at the start of every chat if it exists. If it does not exist (for example on another machine or for a
new contributor), ask the person how they like to be helped. Always be honest and never claim something was
verified if it was not.

## Seeing the project
- With the Filesystem connector (Claude Desktop) Claude can read and edit files under the repo folder and can
  read `build\build_log.txt` and `build\test_log.txt` directly. Claude cannot run his build; he runs
  `scripts\verify.cmd` and says "done".
- Without it: clone the public repo (https://github.com/Mohanadmomen/Nxtcut.git) in the sandbox. Only PUSHED
  work is visible; ask him to push a work-in-progress branch before a build when a review is needed.
- Attached files live under `/mnt/user-data/uploads/`; always open them with the view tool.

## Checks Claude does itself
- Before a risky merge: read public headers and the ownership/lock/link code of the step.
- Pre-push GCC check in the sandbox (syntax only, CI's flags): copy the changed files, then
  `g++ -std=c++20 -fsyntax-only -Wall -Wextra -Wpedantic -Wconversion -Wshadow -Werror -I engine/<m>/include
  -I engine/core/include -I engine/model/include -I tests/support -isystem <tl-expected> -isystem <gtest>/include file.cpp`
  (needs tl/expected.hpp from github.com/TartanLlama/expected and googletest headers). It does NOT cover Clang,
  MSVC or macOS; CI is the only check for those. Clang-only error seen so far: unused variable, unused lambda capture.
- Do not trust the coding agent's reports; only a real build, tests and green CI count.

## Prompt skeleton (one prompt per step; big steps are split)
Use a four-backtick outer fence when the prompt contains code blocks. Sections: ROLE; PROJECT CONTEXT (what is done
and VERIFIED, naming rules); FIRST: READ (files to read, never paste them); THIS TASK (exact scope, module);
DESIGN (decisions already made); then numbered specs (files, classes, signatures, rules, error codes, REQUIRED
TEST VECTORS where correctness is critical); TESTS; DOCUMENTATION; NON-GOALS (files that may be edited; do not
run cmake/vcpkg/ctest/git); DEFINITION OF DONE; OUTPUT FORMAT (edit files directly; short reply).
- The universal rules (portability, test pitfalls, error codes) are in `AGENTS.md`: do NOT repeat them, only
  reference them.
- Re-read every prompt for contradictions before sending: a data field and a method with the same name, a rule
  that conflicts with a rule elsewhere in the prompt, names that cannot coexist in C++.
- Every prompt lists REQUIRED cases with exact numbers (e.g. split at a clip start gives InvalidArgument; a link
  group with one clip deleted clears the survivor's link; undo of every command is `identical`).

## Standard routine for every step
1. `git checkout main`, `git pull`, `git checkout -b step-N-name`. Working tree must be clean.
2. Maintainer pastes the step prompt into a NEW Antigravity conversation and declines any build command it asks for.
3. `scripts\verify.cmd` -> Claude reads the logs. Fix cycle: Claude gives exact edits or applies them.
4. Claude reviews risky files, runs the GCC pre-push check, then: clang-format 18.1.8, `git add .`, `git status`
   (look for stray files), commit, `git push -u origin step-N-name`, open the pull request.
5. All 7 CI jobs green -> merge -> `git checkout main`, `git pull`, delete the branch locally and on GitHub.
6. Claude updates `docs/PROJECT_STATE.md` (Status, Next, debt) on the next branch.
