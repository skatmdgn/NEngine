# Dependencies and licensing policy

No third-party runtime dependency is committed yet.

Selection rules:
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
