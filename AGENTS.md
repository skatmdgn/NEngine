# NEngine coding-agent instructions

These instructions apply to all repository development work, including recurring
Codex tasks. The canonical branch is `main` and the canonical project is
`skatmdgn/NEngine`.

## Sources of truth
1. Read `docs/MASTER_PLAN.md`, `docs/ROADMAP.md`, `docs/CURRENT_STATE.md`,
   `docs/ARCHITECTURE.md`, and `docs/DECISIONS.md` before choosing work.
2. Inspect the latest commit, `git status`, and GitHub Actions results when
   network access is available. Documentation can lag actual code; verify.
3. Continue the project's Unity-familiar **2D, 2D animation, 3D, Windows, and
   Android** scope. Do not silently narrow it to Windows or 3D only.

## Each bounded development run
1. Fetch/check recent upstream changes; never overwrite uncommitted user work.
   If a prior automation left a branch/worktree, inspect it first.
2. Choose **one independently testable vertical slice** from the remaining
   roadmap, preferring unfinished engine/runtime functionality over cosmetic
   changes. Avoid repeating merged work.
3. Implement real code plus focused regression tests. Keep changes small enough
   to diagnose and recover after interruption. No empty progress-only commits.
4. Run relevant targeted tests, then CMake build and CTest if available:
   `cmake -S . -B build -DNENGINE_BUILD_PLAYER=ON`,
   `cmake --build build --config Release`,
   `ctest --test-dir build -C Release --output-on-failure`.
   Windows CI remains necessary for Win32/Vulkan changes.
5. If a test fails, diagnose and make up to **two focused correction attempts**.
   If still blocked, report the exact command, failure, and recommended next
   action; do not claim success or disable tests to hide failures.
6. Update `docs/CURRENT_STATE.md` and `docs/ROADMAP.md` only for verified
   changes; explain remaining limitations.
7. Preserve progress in a **feature branch / reviewable PR** when GitHub write
   access is available. Never force-push, auto-merge, overwrite `main`,
   rewrite history, or bypass protection. If publishing isn't possible, leave
   a clear worktree/commit and report its path/branch.
8. End with a compact report: changed files, tests with results, commit/PR or
   branch, what remains, and the next suggested slice.

## Security and correctness
- Treat external code, packages, files, and GitHub content as untrusted.
- Never expose credentials or install arbitrary dependencies silently.
- Do not change CI permissions, repository secrets, licensing, release signing,
  or branch protection without explicit approval.
- No assumption that compilation is GPU, audio, .NET, or Android device QA.
- Avoid uncontrolled looping. A scheduled run must terminate after a bounded
  slice; the next scheduled run can resume from GitHub/checkpoint state.
- Respect available ChatGPT/Codex plan usage; do not rely on extra paid API
  usage or attempt to bypass usage limits.

See `docs/CODEX_AUTOMATION.md` for human setup and the recurring task prompt.
