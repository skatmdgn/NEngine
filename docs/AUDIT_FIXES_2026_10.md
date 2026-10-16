# Priority audit repairs — October 2026

**Scope:** NEngine main baseline `51db15f`, reviewed in October 2026.
**Proposed integrated PR:** [#12](https://github.com/skatmdgn/NEngine/pull/12).
**Status:** All 18 previously documented priority audit findings have code changes and/or explicit fail-closed behavior proposed in this branch. This does **not** certify every game-engine feature or replace interactive Windows/Android QA.

## Verification

- Complete integrated branch run [38926574308](https://github.com/skatmdgn/NEngine/actions/runs/38926574308): **11/11 registered CTest suites passed on Ubuntu and Windows** at commit `c659555`. New documentation-only commits, if any, must also pass CI.
- The integrated branch includes the native code, corresponding regression tests, and cross-file interaction checks from the individual work items.
- No change has been merged into `main` by this audit workflow.
- Source of truth for the original findings: [NEngine core audit](https://github.com/skatmdgn/NEngine/pull/12) (refer to the PR description and associated individual PRs).

## Finding-by-finding repair matrix

| ID | Finding | Implemented action | PR | Validation / caveat |
|---|---|---|---|---|
| A-01 | Cache symlink escape can remove outside files | Restrict cleanup to canonical in-cache regular files and reject symlink/traversal references | [#3](https://github.com/skatmdgn/NEngine/pull/3) | External victim-file protection regression |
| A-02 | Scene unbounded input, object/component/property counts | Bound Scene input to 32 MiB and cap parsed record counts | [#4](https://github.com/skatmdgn/NEngine/pull/4) | Oversized/malformed input tests; fuzzing remains |
| A-03 | Save failure can truncate original Scene | Serialize fully before staging, flush/close temporary file then atomic replacement | [#4](https://github.com/skatmdgn/NEngine/pull/4) | Failed serialization original preservation tested; power-loss injection remains |
| A-04 | `World::clear` / Scene replacement can revive stale entity handles | Issue process-wide monotonically increasing entity generations on creation | [#5](https://github.com/skatmdgn/NEngine/pull/5) | Clear/reuse/World replacement regression; process-lifetime/handle semantics remain subject to design review |
| A-05 | Managed build command-shell argument interpretation | Windows `CreateProcessW` launch with UTF-16 argv encoding and shell bypass | [#8](https://github.com/skatmdgn/NEngine/pull/8) | Windows real .NET build uses metacharacter-bearing output path |
| A-06 | Vulkan out-of-date swapchain aborts Player | React to `needs_resize()`, recreate swapchain and dependent render resources | [#9](https://github.com/skatmdgn/NEngine/pull/9) | CI compile; **manual Vulkan GPU/monitor/device-loss QA still required** |
| A-07 | Scene capture silently drops unsupported components | Add `capture_checked`; Windows Editor Save refuses incomplete snapshots | [#10](https://github.com/skatmdgn/NEngine/pull/10) | Core strict-capture regression; legacy non-strict API remains available intentionally |
| A-08 | Read-only duplicate GUID scan mutates `.meta` files | Report collisions in `scan(false)` without writing; keep explicit repair in `scan(true)` | [#7](https://github.com/skatmdgn/NEngine/pull/7) | AssetDatabase regression checks unchanged metadata bytes |
| A-09 | Import manifest top-level artifact count unbounded | Cap manifest artifact count before `reserve` | [#3](https://github.com/skatmdgn/NEngine/pull/3) | Malformed oversized count regression |
| A-10 | Cached artifact lookup trusts escaped/external paths | Reuse canonical in-cache path checks in cache hit validation | [#3](https://github.com/skatmdgn/NEngine/pull/3) | Cache symlink escape lookup regression |
| A-11 | glTF source/buffer reads unbounded | 16 MiB JSON and 256 MiB binary input limits | [#7](https://github.com/skatmdgn/NEngine/pull/7) | Compile/integration CI; large-file/fuzz tests remain |
| A-12 | WebP shipped despite missing Player decoder | Fail export explicitly and clean staged bundle | [#11](https://github.com/skatmdgn/NEngine/pull/11) | WebP negative export regression; WebP rendering **not implemented** |
| A-13 | Player requires Assets directory even for data-only export | Always stage empty `Content/Assets` directory | [#6](https://github.com/skatmdgn/NEngine/pull/6) | No-copied-assets Player load regression |
| A-14 | Infinite Audio pitch leads to NaN playback time | Finite serialization checks and runtime overflow guard | [#6](https://github.com/skatmdgn/NEngine/pull/6) | Audio codec + playback non-finite tests |
| A-15 | Standalone Player does not propagate mouse wheel | Forward Win32 Window wheel delta into Player input | [#6](https://github.com/skatmdgn/NEngine/pull/6) | Windows compile; **interactive wheel QA still required** |
| A-16 | Scene trailing records accepted | Reject non-whitespace after `END_SCENE` | [#4](https://github.com/skatmdgn/NEngine/pull/4) | Trailing-token negative regression |
| A-17 | Canvas header line unbounded | Use bounded fixed-size header read | [#7](https://github.com/skatmdgn/NEngine/pull/7) | Oversized Canvas header regression |
| A-18 | Existing invalid Transform hierarchy may make `set_parent` loop forever | Bound hierarchy ancestry traversal by alive entity count | [#5](https://github.com/skatmdgn/NEngine/pull/5) | Injected-cycle regression |

## Not included in the audit's test guarantee

CI is **not** a substitute for:
- Actual Windows Vulkan frame presentation, minimized/restored window, resizing, DPI changes, moving between monitors and swapchain recreation.
- WASAPI output to live devices, genuine mouse-wheel input, interactive Editor Scene save and reload, .NET scripts in the exported standalone Player.
- Android APK/AAB generation or physical Android device runs (these features remain incomplete).
- ASan/UBSan, fuzzing of malformed external files, performance and memory-pressure workloads, recovery from sudden power loss during disk writes.
- Complete 2D/3D renderer/animation/API feature parity with Unity.

## Manual acceptance checklist

1. On a Windows machine with Vulkan drivers, build the Editor and Player and launch a real exported Scene.
2. Resize/minimize/restore, change DPI/display and verify the Player does not exit on transient swapchain changes; collect Vulkan diagnostics on failures.
3. Check C# gameplay input (including wheel), fixed/update callbacks, and managed compilation in project/output paths with whitespace/non-ASCII/metacharacters.
4. Verify audio playback on an actual WASAPI device; exercise loops and invalid values.
5. Open and save Scenes and Prefabs, test recovery from failed saves and unknown-component warnings; test import cache rescan and asset reimport on real projects.

**Integration gate:** Run Windows and Ubuntu CI on the final PR head before merging. No direct `main` edits or history rewriting.
