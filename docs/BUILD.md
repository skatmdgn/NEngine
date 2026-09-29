# Build

## Requirements for core development
- CMake 3.25+
- C++20 compiler

## Configure/build/test
```bash
cmake -S . -B build
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

## Windows product builds
Windows CI is the authoritative early compiler check. Native editor/render/plugin QA still requires a real Windows 10/11 x64 machine; compiling successfully is not the same as runtime verification.

## Android
Android tooling is intentionally absent at the core milestone. It will be supplied by versioned Toolchain Manager profiles rather than relying on arbitrary system `JAVA_HOME`, NDK or Gradle installations.
