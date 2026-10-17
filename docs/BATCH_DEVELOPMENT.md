# User-requested batch development and final-CI policy

**Applies to:** NEngine development requests such as `8덩어리`, `10덩어리`, `15덩ㄱ`, or “develop as much as possible in one pass.” Read alongside [DEVELOPMENT_WORKFLOW.md](DEVELOPMENT_WORKFLOW.md), [CURRENT_STATE.md](CURRENT_STATE.md), and [ROADMAP.md](ROADMAP.md).

**한국어 핵심 원칙:** 사용자가 요청한 **N덩어리는 의미 있는 기능·수정·테스트 단위 N개**를 뜻한다. 각 덩어리마다 코드 검토와 가능한 로컬 테스트를 수행하고, 중간 결과는 **PR이 없는 개발 브랜치**에 저장한다. GitHub Actions 전체 CI는 **개발 완료 후 최종 PR 생성 시 1회 실행을 목표**로 한다. 실패를 고치면 최종 CI를 다시 실행하며, 실패하거나 실행하지 않은 테스트를 통과했다고 주장하지 않는다.

## 1. Interpret the user's requested number

- `N덩어리` means **N bounded, meaningful implementation slices**, not N arbitrary commits, N tool calls, or N paragraphs. Preserve the requested feature scope and previously agreed Windows, 2D/2D animation, 3D, C#, plugin, and Android roadmap.
- Before implementation, read the current code, docs, open PR dependency chain, upstream `main` SHA, and recent CI. Select concrete, testable slices based on actual gaps; favor runnable functionality, defects and regression tests over cosmetic progress.
- An individual slice should have a precise objective, relevant source change or real verified repair, and a test/verification method. One reviewable commit per slice is preferred, **not compulsory**; supporting fixup commits may make the final count greater than N. A commit by itself is **not** evidence of a completed slice.
- For `15덩어리`, checkpoint the work as **1–5 / 6–10 / 11–15**; adapt proportionally for other counts. Avoid overlarge changes that destabilize the session. If blockers prevent N useful slices, deliver the ones truly implemented and report the shortfall and blocker plainly.

## 2. Development, review and test checkpoints

1. **Preflight:** verify which branch/PR has the actual latest code, read the relevant source and tests, enumerate up to N feasible slices. Never assume an unmerged PR is already in `main`.
2. **Per slice:** edit source, inspect callers and integration paths, add or update focused regression tests, review changed files, and make a focused commit. Do not label tests “passed” merely because test code was authored.
3. **Every group of roughly five:** check compilation/type safety, test interactions across the previous slices, and make a durable checkpoint. Prefer targeted **local** build and test runs when a runner is available; give truthful “not locally run” status when it is not.
4. **Before opening/updating the final PR:** run a full local configure/build/CTest pass if a local build toolchain is available:

   ```bash
   cmake -S . -B build -DNENGINE_BUILD_TESTS=ON -DNENGINE_BUILD_EDITOR=ON -DNENGINE_BUILD_PLAYER=ON
   cmake --build build --config Release
   ctest --test-dir build -C Release --output-on-failure
   ```

5. **Do not replace testing with source-only inspection.** When no local runner is accessible, keep regression tests in the source and mark them as **awaiting CI**.

## 3. Store intermediate work without repeatedly triggering GitHub CI

The repository's `.github/workflows/ci.yml` was checked on **2026-10-17**. At that revision it runs on:

- `push` to **`main`** or **`nengine-bootstrap`**
- **`pull_request`** events (including PR opening and later PR updates)

It also uses `cancel-in-progress: true` for the same PR ref. Thus pushing each slice to an **already-open PR branch** repeatedly schedules CI and may cancel the previous run.

**Default procedure (while these triggers remain unchanged):**

1. Create a **new, uniquely named working branch without an open PR**, based on the latest appropriate verified source or the tip of an unmerged dependency PR.
2. Commit each slice (or small checkpoint group). **Pushing checkpoints to this not-yet-PR branch is allowed and preferred** to preserve work through conversation disconnections. Under the currently checked triggers, ordinary pushes to that feature branch do not invoke the CI workflow.
3. Do **not** open a draft or regular PR until the batch is complete. A draft PR is still a PR event and is **not** a way to avoid CI.
4. If work must be based on an existing open PR, use a **new child branch** for the next batch; avoid per-slice updates to its open parent PR. Open the child PR against that parent branch once ready and preserve the explicit merge order.
5. At batch completion, push the last commits, review the diff and only then **create the PR once** (or, if the PR already exists for a legitimate reason, update its head **once**). This is the desired single final GitHub CI trigger.
6. Git object creation without updating a branch reference is **not** a durable remote checkpoint. Ensure the checkpoint branch actually points to the saved commit and report its exact branch/SHA.

**Recheck the real workflow triggers before each new batch**: other workflows, path rules, protected branches or workflow edits may change whether a push causes CI. Avoid force-pushes, rebasing away another contributor's changes, unreviewed direct `main` pushes and CI-bypass tricks.

## 4. Final CI gate and exceptions

- Wait for the actual **final head SHA** CI result, not a successful run from an earlier SHA or dependency PR. Require the applicable **Ubuntu and Windows** build/test jobs to pass; inspect failed step logs and fix the underlying issue.
- **“One final CI run” is an efficiency target, not a guarantee.** If CI fails, add a genuine fix, push a corrected final head and rerun. Say how many runs actually occurred; never report “all tests passed” on a queued, cancelled or failing run.
- An **intermediate GitHub CI** run is allowed when it is materially safer—for example, a major CMake/dependency refactor, platform-specific toolchain problem that cannot be locally compiled, or a high-risk architecture change. Explain that exception and minimize redundant runs. Do not sacrifice correctness to meet a one-run quota.
- CI does **not** demonstrate working hardware, real GPU Vulkan rendering, audible WASAPI output, physical-device Android export, complete deployed .NET gameplay or long-running stability. These need separate acceptance evidence.
- Treat **implemented in source**, **local tests passed**, **CI passed at SHA**, **user/device accepted**, and **remaining/unverified** as distinct statuses.

## 5. Finish and recover across sessions

- Keep `docs/ROADMAP.md` and `docs/CURRENT_STATE.md` aligned with the actual code and verified results; leave untested hardware milestones open.
- After every checkpoint report **slice indices completed, branch name, short SHA, tests actually run, and open issues**. If the conversation disconnects, restart from the latest pushed branch and compare it to the open PRs and `main`; do not generate the same code twice or infer work was merged.
- In the final response summarize completed slices, changed source/test files, final commit, PR URL, final CI jobs/results, unresolved risks and **whether `main` was merged**. Never call a PR merge a fact until GitHub confirms it.
- Follow existing review and approval policies; a successful CI result does not automatically authorize a silent merge.

**Default sequence:** inspect → plan N meaningful slices → implement/test/commit in small groups → checkpoint on a not-yet-PR branch → review and local tests if available → open PR once → final CI → fix and rerun if necessary → report precise acceptance state.
