# Codex scheduled development for NEngine

This document is a setup/playbook, **not** an automatically activated task.
The user must create a schedule in the ChatGPT/Codex interface and authorize
any needed repository/file permissions. Using **Sign in with ChatGPT** draws
from the plan's included Codex usage, not from a separate OpenAI API key; plan
usage caps still apply.

## Recommended Windows setup

1. Install or open the official ChatGPT desktop app with Codex support.
   Sign in using the existing ChatGPT Plus account, not an API key.
2. Clone `https://github.com/skatmdgn/NEngine.git` locally and open its
   directory as the Codex project. Check that `AGENTS.md` is visible and
   `git status` is clean. Install CMake and a C++20 toolchain. See `BUILD.md`.
3. Run the prompt below once interactively. Check the resulting code diff,
   tests, and actual branch/PR before enabling unattended runs.
4. Create a **scheduled task** in a Codex chat or from the desktop app's
   Scheduled interface. Start with **once daily**, in a dedicated Git
   worktree when possible. Verify the chosen project and permissions.
5. Review the first few scheduled runs and the repository's CI. Only increase
   frequency once reviews are reliable and Plus usage allows it.
6. Local-project scheduled work requires the **Windows computer to stay on**
   and **the ChatGPT desktop app to stay running**. Web/cloud tasks may use
   connected GitHub tools instead, but do not automatically have your local
   source/build directory.
7. Track actual Codex/Work usage in ChatGPT Settings > Usage. On exhaustion,
   wait for the plan limit reset; this playbook does not purchase credits.

## Recurring automation prompt (copy into Codex)

Continue development of `skatmdgn/NEngine` toward the documented NEngine
1.0 goal without needing me to type "15 chunks" repeatedly. Read the
repository `AGENTS.md` and `docs/{MASTER_PLAN,ROADMAP,CURRENT_STATE,
ARCHITECTURE,DECISIONS,BUILD}.md`, examine the latest `main` and CI, and
inspect prior automation branches before choosing work. Select one small but
substantial missing feature, with balanced attention to 2D, 2D animation,
3D, the Windows editor/player, and Android. Implement production-oriented
code, add focused tests, run the relevant build/tests, and repair at most two
rounds of test failures. Never suppress failing tests or claim untested
hardware verification. Update the roadmap/current-state docs for changes
that actually pass. Keep all changes reviewable in an isolated worktree or
feature branch and open a pull request if permissions allow. Never merge
directly into `main`, force push, alter security settings, buy credits,
or deploy/release without my approval. Report the PR/branch, tests, known
blockers, and the next suggested development slice. If unfinished because
of a disconnect or usage limit, leave a durable checkpoint in version
control and resume from that state on the next scheduled run. Complete this
single run after one bounded development slice.

## What this does not guarantee

- A task cannot run past the included Plus allowance indefinitely.
- Local schedules do not run while the PC is off.
- A failed environment or unavailable repository permission may require
  user action; unlimited error retries are intentionally disabled.
- Automated tests do not replace real Windows GPU/Audio, C# runtime, or
  Android device acceptance testing.
- Finishing every roadmap item is a goal, not a guaranteed outcome.
