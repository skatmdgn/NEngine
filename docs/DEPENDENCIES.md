# Dependencies and licensing policy

## Integrated dependencies

### stb_image
- Upstream: nothings/stb
- Pinned commit: `2c980bb59875b0d32144a71867fbdebb2f77cd20` (2026-08-02)
- Vendored files: `third_party/stb/stb_image.h`, `third_party/stb/LICENSE`
- License choice: MIT alternative from the upstream dual-license text.
- NEngine usage: CPU decoding of PNG and JPEG into RGBA8 pixels.
- Build scope: private implementation detail of NEngineRender; only PNG/JPEG decoders are enabled.
- No runtime DLL/shared-library dependency is introduced.

## Selection rules
1. Prefer permissive licenses suitable for redistribution in a commercial engine/runtime.
2. Pin versions and record license notices.
3. Keep external implementation behind a narrow adapter owned by NEngine.
4. Never make renderer/scene/core depend directly on service SDKs.
5. Android SDK/NDK/JDK/Gradle components remain toolchain modules subject to their own licenses and update rules.

Expected categories later (final choice requires license/version review at integration time):
- Vulkan loader/headers/tooling
- physics backend(s)
- image/font/audio/model import libraries
- .NET hosting/toolchain components
- compression/serialization helpers

Every introduced dependency must update this document and third-party notices before merge.
