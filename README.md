# NEngine

NEngine is an independent, clean-room game engine for developers familiar with Unity-style **GameObject / Component / Transform / Scene / Prefab** workflows. It targets **Windows 10/11 Editor**, **Windows x64 Player**, **Android Player**, **2D/3D**, **C# gameplay**, and integration with third-party SDKs via package/plugin adapters. Unity source, binary or project compatibility is not a goal.

**Current status: 0.9.0-dev, not a complete/shippable 1.0 release.** Core/Editor/Player/physics/assets/Vulkan/C# foundations exist, but real Windows end-to-end acceptance, Android APK/AAB, package/plugin integration, fonts/UI and other production work remain outstanding.

## Single source of truth

- [docs/README.md](docs/README.md) — product target, architecture and reading order.
- [docs/CHECKLIST.md](docs/CHECKLIST.md) — canonical, **stable-ID 1.0 acceptance checklist**. Each item changes state only with proof. Never infer the percentage of a finished engine from checkbox counts.
- [docs/WORKFLOW.md](docs/WORKFLOW.md) — coding, PR, test evidence, batch work, final-only CI, cross-session continuity.
- [docs/FILE_AUDIT.md](docs/FILE_AUDIT.md) — baseline source inventory, file/test mapping, known gaps.

Previous roadmap and status logs have been intentionally removed from the current docs tree; their content remains in Git history. Developers must read actual source and current GitHub main/PR/CI, not just the docs.

## Build

Requires CMake 3.25+ and a C++20 compiler. Windows Editor/Player usage requires suitable Windows and Vulkan hardware; managed C# gameplay requires a compatible .NET SDK/runtime. Android build/export is **not yet production-implemented**.

```bash
cmake -S . -B build -DNENGINE_BUILD_TESTS=ON -DNENGINE_BUILD_EDITOR=ON -DNENGINE_BUILD_PLAYER=ON
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

As of 2026-10-27, PR [#41](https://github.com/skatmdgn/NEngine/pull/41) carries ten additional **source/automated acceptance candidates** (CORE-005, EDIT-007/008, PHYS-006, ANIM-003, INPUT-003, EDTOOL-001/006/008, UTIL-004). Its checklist shows **135/285 accepted and 150 remaining**, conditional on a successful final-HEAD Windows/Ubuntu Debug+Release CI and merge. The previously merged main baseline is 125/285 accepted. Hardware Windows GUI/GPU/audio and Android APK/device acceptance are excluded. See [docs/CHECKLIST.md](docs/CHECKLIST.md) for exact evidence and scope.

As of 2026-10-26, PR [#40](https://github.com/skatmdgn/NEngine/pull/40) source SHA `acbff3df04b50c6dbc55ef59a6fc71fd4b4ba1f7` passed **38/38 CTest groups** on Windows/Ubuntu Debug+Release ([CI run 40279093322](https://github.com/skatmdgn/NEngine/actions/runs/40279093322)), extending the source-level Navigation agent, managed C# logging/coroutines, Editor workflow and diagnostics. The pre-#41 main baseline recorded **125 accepted / 160 remaining**. This is NOT proof of actual installed Visual Studio/Rider GUI launching, hardware Vulkan output, audio playback or Android device deployment.

All development and progress reporting should use the IDs in [docs/CHECKLIST.md](docs/CHECKLIST.md). Third-party license information remains in [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).
