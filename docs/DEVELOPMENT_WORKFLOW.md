# NEngine development and cross-session workflow

Canonical codebase: [skatmdgn/NEngine](https://github.com/skatmdgn/NEngine), `main`. This page is for any human or coding assistant working on the repository.

## Read and verify before coding

1. Read `MASTER_PLAN.md`, `CURRENT_STATE.md`, `ROADMAP.md`, `ARCHITECTURE.md`, `DECISIONS.md` and `API_SPEC.md`.
2. Check the current upstream `main` SHA, relevant source and recent CI runs. Recheck any documentation claim against implementation when choosing work.
3. Retain the agreed Unity-familiar **2D, 2D animation, 3D, Windows, Android** scope; do not equate an early demo with product completion.
4. Treat `docs/history/` as immutable legacy notes, not the latest truth. Respect one-way module dependencies and record new architecture decisions in `DECISIONS.md`.

## For each bounded development slice

1. Choose a testable feature or high-priority regression; inspect the related public interface, sources and callers.
2. Implement concrete functionality with regression coverage. Keep diffs reviewable; do not overwrite uncommitted upstream work.
3. Run focused local tests whenever the environment supports them. Run full local CMake build/CTest before the batch PR when available; reserve remote GitHub Actions CI for the final batch gate by default. See [BATCH_DEVELOPMENT.md](BATCH_DEVELOPMENT.md).
4. For Win32/Vulkan or managed changes, require Windows CI, and record real GPU/.NET/device QA **separately**. Do not assert hardware behavior from a successful build.
5. Update ROADMAP for **verified** changes; keep CURRENT_STATE concise, current and explicit about what was *not* tested. Archive detailed chronological notes rather than endlessly appending them above the status heading.
6. Preserve changes in a reviewable branch/PR where possible; do not force-push or silently replace `main`. Report changed files, tests, commit/PR, limitations and next task.

## User-requested N-slice development and final CI

For user prompts such as **`8덩어리` / `10덩어리` / `15덩어리`**, follow the full policy in **[BATCH_DEVELOPMENT.md](BATCH_DEVELOPMENT.md)**. N denotes meaningful implementation/test units, not empty commits. Work in small reviewable checkpoint groups, save intermediate commits to a new working branch **without an open PR**, and trigger GitHub CI by opening the final PR **once after the batch**. Under the currently checked `.github/workflows/ci.yml` triggers, feature-branch checkpoint pushes do not run CI; **pushes to an already-open PR branch do**. Recheck triggers before future sessions. Exceptions for platform-risk and mandatory reruns after failure are documented in the policy; never suppress necessary checks or claim unrun tests passed.

## Status integrity

- Version labels are development/milestone indicators, not proof that all earlier features are production-ready.
- SOURCE/CI VERIFIED, USER ACCEPTED, NEEDS HARDWARE QA and NOT IMPLEMENTED are distinct states.
- If documentation conflicts with source, resolve the conflict before marking a milestone complete.
- Core remains independent from platform-specific service SDKs. New third-party dependencies require license/version notices.
