# NEngine 문서 시작점 — 제품 목표와 현재 기준

NEngine은 **Unity의 사용 경험과 개념에 익숙한 개발자가 쓸 수 있는 독립적인 클린룸 게임 엔진**이다. Unity 소스 코드/프로젝트/바이너리와의 호환을 목표로 하지 않는다. 목표는 **Windows 10/11 에디터, Windows x64 및 Android 앱 플레이어, 2D/범용 3D, C# 게임 로직, 외부 SDK·플러그인 활용**을 지원하여 실제 게임 하나를 제작하고 배포할 수 있게 하는 것이다.

## 정식 문서 — 단 네 개

1. **[CHECKLIST.md](CHECKLIST.md)**: 1.0 요구사항의 유일한 원장. 각 ID를 하나씩 통과시키며, 근거 없이 체크하지 않는다.
2. **[WORKFLOW.md](WORKFLOW.md)**: 브랜치·개발·증거·검증·PR·CI·새 채팅 재개 정책.
3. **[FILE_AUDIT.md](FILE_AUDIT.md)**: 2026-10-22 기준 `main` 파일 301개의 파일명·blob SHA·영역별 테스트/미검증 매핑.
4. **이 파일**: 제품 정의, 개발 범위와 참고할 읽기 순서.

구버전 ROADMAP/MASTER_PLAN/CURRENT_STATE/BUILD 등의 모순된 체크·상태 기록은 Git 이력에만 남기고, 현행 `docs/`에서는 제거한다. 앞으로 '몇 퍼센트?'라고 물으면 **코드/CI 항목 수의 완료율을 곧바로 1.0 제품 완성률로 해석하지 않는다**. 새 요구사항이 늘면 이유와 ID를 기록한다.

## 기술·아키텍처 불변 원칙

- **Windows x64 에디터**가 Scene/Hierarchy/Inspector/Assets/Console, 2D·3D 배치, 플레이 테스트를 담당한다.
- **Core World 모델**을 Editor Play와 실행 Player가 공유한다. Entity는 세대형 식별자, Transform/Component/Scene/Prefab은 독립 형식이다.
- **AssetGuid 및 메타파일**로 리소스 식별자를 안정적으로 유지한다. 불신 파일은 크기·경로·참조를 검증한다.
- **Vulkan 우선 렌더링**을 지향하되 기존 Win32/GDI 안전 폴백과 아직 미완성인 Editor Vulkan 통합의 차이를 숨기지 않는다.
- **C# 게임플레이**는 독립 API·Bridge 및 .NET 호스트를 통해 구현한다. managed/native ABI 호환과 게임 DLL 배포를 명시적으로 다룬다.
- **외부 SDK와 패키지**를 엔진 코어에 직접 섞지 않고 NuGet, managed DLL, Windows native DLL, Android AAR/JAR/SO, Maven 경로로 연동한다.
- **Android 툴체인**은 버전별 SDK/NDK/JDK/Gradle 프로필로 관리한다. Windows 빌드 성공이 Android 앱 실행을 의미하지 않는다.
- 목표는 Unity 전체 API·엔진 코드·패키지 재현이 아닌, 실제 2D/3D 게임 제작과 배포에 필요한 제한된 독립적 제품 완성이다.

## 2026-10-27 PR #42 — 외부 C# 라이브러리/패키지 파이프라인

- [PR #42](https://github.com/skatmdgn/NEngine/pull/42): PLUG-004/005/006/009/017 및 CS-005/006/007/008/013의 정확히 10개 기존 소스 수용 후보. 기존 `main` 135/285와 구별해서, PR 브랜치는 최종 Windows/Ubuntu CI 성공을 조건으로 145/285를 기록한다.
- `Packages/managed-packages.txt`의 NuGet 패키지 참조를 .NET 빌드에 전달하여 외부 네트워크에 의존하지 않도록 독립적으로 pack한 `Sample.Json` NuGet 예제를 로컬 피드에서 복원·실행한다. 공개 nuget.org 패키지 다운로드 자체는 별도 수용 대상이다. `Assets/Plugins/Managed/*.dll`은 관리형 참조 및 복사, `Assets/Plugins/Native/*.{dll,so,dylib}`은 OS별 P/Invoke 공유 라이브러리 복사 대상으로 제한한다. `Content/Managed` 내보내기는 프로젝트 DLL·API/Bridge와 추가 런타임 라이브러리를 검증된 파일명/개수/크기 정책에 따라 스테이징한다.
- 추가 검증: 외부 NuGet 라이브러리로 최초 C# 빌드가 성공한 뒤 게임 코드만 변경하여 hot reload하면 최초 복원된 assets와 함께 `--no-restore` 빌드로 새로운 출력 경로를 생성한다. 동일 의존성을 불필요하게 재복원하지 않는다.
- C# 생명주기 예외는 원본 .NET 스택과 함께 콘솔 로그를 남기고 네이티브 호스트를 보호한다. C# World Transform·Component 속성·Time/Coroutine 실행은 실제 관리형 통합 테스트로 검증한다. CI 통과가 Windows GPU·실기기 GUI·Android 호환성의 증거는 아니다.

## 2026-10-27 PR #41 — 10개 소스 수용 후보와 후속 충돌 수정

- [PR #41](https://github.com/skatmdgn/NEngine/pull/41): CORE-005, EDIT-007/008, PHYS-006, ANIM-003, INPUT-003, EDTOOL-001/006/008, UTIL-004. 소스 HEAD `f6160c138fc0669e834f8d48a5c2e960239300dd` 기준 자동 검증과 Windows/Ubuntu Release·Debug 최종 CI가 모두 성공해야 병합 가능하다. 체크리스트 285개 중 135개 완료/150개 미완료는 해당 최종 CI 성공을 조건으로 하는 소스·자동화 수용치다.
- 기존 기능 코드 `7f8039b1d38359e8de251961263fa4188e71a387`에서는 [CI 40427979420](https://github.com/skatmdgn/NEngine/actions/runs/40427979420) Windows/Ubuntu Release·Debug 성공. 이후 **microstep 중간에서만 생긴 Trigger 접촉이 버려지는 문제**를 수정하고 2D/3D 회귀를 추가했으므로 이 이전 성공 로그로 수정 HEAD 검증을 대체하지 않는다.
- 범위 주의: PHYS-006은 최대 128 substep의 opt-in Box 고속 충돌이며 임의 속도·모든 Collider의 완전한 swept CCD가 아니다. EDTOOL-006은 CPU 프로파일러이고 EDTOOL-008은 빌드 타깃 설정 화면이지 Android APK 생성기능은 아니다. 실제 Windows GUI·GPU·오디오·Android 실기기 실행은 별도 수용 대상이다.

## 2026-10-26 PR #40 — 7개 소스·자동화 수용 ID

- PR [#40](https://github.com/skatmdgn/NEngine/pull/40) 소스 SHA `acbff3df04b50c6dbc55ef59a6fc71fd4b4ba1f7` ([CI 40279093322](https://github.com/skatmdgn/NEngine/actions/runs/40279093322)): Windows/Ubuntu Release·Debug 모두 **38/38 CTest PASS**. NAV-013, UTIL-003, EDTOOL-003, CS-014, EDTOOL-004, UTIL-001, EDIT-014만 증거 기반 추가하여 285개 중 완료 125 / 미완료 160. 실제 설치된 VS/Rider GUI/Windows GPU 및 Android 실기기 수용 아님.
- NAV-013 군집 회피는 결정적인 단순 반경·스윕 충돌 방지와 제한적인 회피 우회이며, 대규모 crowd/ORCA 수준 완성으로 해석하지 않는다. Debug.Log는 caller path/line 및 severity를 보존하고 Editor Console에 연결한다. C# 컴파일 오류는 프로젝트 안의 유효한 파일만 더블 클릭 열기를 허용한다.
- 본 저장소의 1.0 요구 및 미완료 내역은 [CHECKLIST.md](CHECKLIST.md)가 유일한 원장이다.

## 2026-10-26 현행 Navigation 수용 보충

- PR [#39](https://github.com/skatmdgn/NEngine/pull/39) 코드 SHA `8fe399b6b57b5c6e01b98fcd17f030ef3720eaa4`: Scene MeshRenderer Bake, 계층 Transform, 정적 장애물 clearance, T-junction·funnel, Scene/Editor/Player 및 C# NavMeshAgent, 장애물 변경 재탐색을 Windows·Ubuntu Debug/Release 각각 **37/37 CTest PASS**로 검증([CI 40249542956](https://github.com/skatmdgn/NEngine/actions/runs/40249542956)). 1.0 수용 원장에는 **서로 다른 8개 ID만** 반영하고, 미완료 167개를 남긴다.
- 기존 2026-10-22 기준 파일/PR 조사 결과는 **역사적 기준선**이며 현재 Open PR이나 구현 범위를 나타내지 않는다. 실제 현재 상태는 GitHub main·PR과 [CHECKLIST.md](CHECKLIST.md)에서 재확인한다. 실제 Windows GPU/Android 기기 수용은 여전히 미완료다.

## 2026-10-22 확인된 사실

- 기준 `main` 커밋: `671cddf2d50d184b1889310f86c85a3df43306bd`, PR #28 병합 상태.
- 원본 트리 파일 **301개**, 기존 `docs` Markdown **13개**.
- `main` Windows·Ubuntu [CI run 39713209272](https://github.com/skatmdgn/NEngine/actions/runs/39713209272) 각 12/12 CTest 그룹 PASS. **호스트 CI는 실제 Windows GPU/오디오/Android 단말 품질을 대신하지 않는다.**
- 별도 오픈 PR [#29](https://github.com/skatmdgn/NEngine/pull/29)에서 네이티브 triangle NavMesh 베이킹과 새 CTest를 구현했으나 아직 main 병합 전이다.
- Git/CMake 버전 `0.9.0-dev`는 선언적 개발 버전이지 완성률 90%를 뜻하지 않는다.
- 가장 큰 제품 공백은 **Android Player 및 APK/AAB, 외부 플러그인 시스템, Windows Player 실제 종단간 게임 배포, UI/폰트, 완성된 Editor Vulkan 뷰, Scene/C# NavMesh 통합**이다.

## 최초 작업 순서

1. 현재 GitHub `main`, 미병합 PR, 최신 CI를 읽는다.
2. [CHECKLIST.md](CHECKLIST.md)에서 ID·수용 조건·의존성을 확인한다.
3. [FILE_AUDIT.md](FILE_AUDIT.md)를 참고하되 실제 코드와 테스트를 다시 확인한다.
4. [WORKFLOW.md](WORKFLOW.md) 방식으로 개발, 필요한 로컬 테스트, 최종 PR과 CI, 증거 기반 체크를 수행한다.

## C# 컴파일 진단 (EDIT-014)

Editor에서 **Build C#**을 실행하면 `dotnet build`의 오류와 경고 텍스트를 최대 2MiB까지 수집한다. `Assets/Scripts/파일.cs(행,열): error CSxxxx` 형식의 진단을 프로젝트 루트 안의 실제 파일로 검증하고 Console에 **심각도·CS 코드·파일·행·열**과 함께 표시한다. **Console 진단을 더블 클릭**하면 해당 C# 파일을 Windows 기본 연결 프로그램에서 연다. 줄 번호는 Console에 노출되며 연결 프로그램이 해당 행으로 자동 이동한다고 보장하지 않는다. 프로젝트 외부 파일은 자동 실행 대상으로 등록하지 않는다.

## IDE 연결 워크플로 (EDTOOL-004)

Windows 에디터에서 프로젝트를 연 뒤 상단 **Scripts**를 누르면 관리형 솔루션을 생성한다. **일반 클릭**은 사용자가 Windows에 연결한 기본 `.sln` IDE를 열고, **Ctrl+Scripts**는 `devenv.exe`(Visual Studio), **Shift+Scripts**는 `rider64.exe`(Rider)를 실행한다. 전용 IDE 명령은 해당 EXE가 PATH에 있거나 Windows에서 실행 가능해야 한다. 실패 시 Console에 경고를 남기고 프로젝트 생성물은 보존한다. 공백·한글 파일 경로는 UTF-16 및 인용된 인수로 전달한다. CI는 명령 계획과 에러 처리만 검증하며 실제 설치된 VS/Rider GUI 실행은 기기 검증이 별도로 필요하다.

## C# SDK 최소 예제 및 API 참조 (CS-014)

실제 컴파일 검증 대상: [NavQuickstart.cs](../samples/managed/NavQuickstart.cs). `Assets/Scripts/NavQuickstart.cs`에 복사한 뒤, 프로젝트의 스크립트 솔루션을 생성하고 .NET SDK로 빌드한다. 에디터 상단 **Scripts** 버튼은 `.nengine/Managed`의 솔루션을 생성·연다. **Build C#**은 해당 프로젝트 DLL을 컴파일하고, 사용 가능한 .NET 호스트가 있으면 에디터에 로드한다. Windows에서 실제 게임 실행은 별도 검증이 필요하다.

```csharp
using System.Collections;
using NEngine;

public sealed class SimpleMover : Behaviour
{
    private Coroutine? job;
    private void Start() { job = StartCoroutine(Go()); }
    private IEnumerator Go()
    {
        yield return new WaitForSeconds(0.25f);
        GetComponent<NavMeshAgent>()?.SetDestination(
            new Vector3(3f, 0f, 2f));
    }
    private void OnDisable()
    {
        if (job != null) StopCoroutine(job);
    }
}
```

| 공개 API | 타입·동작 | 전제 |
|---|---|---|
| `Behaviour.StartCoroutine(IEnumerator)` | `Coroutine` 반환, Play 중 업데이트 | 스크립트가 Scene에 연결되어야 함 |
| `Behaviour.StopCoroutine(Coroutine)` / `StopAllCoroutines()` | 대기 중 작업 취소 및 iterator finally 정리 | 종료된 객체에서는 호출 불필요 |
| `WaitForSeconds(float)` | 게임 업데이트 누적 시간에 따라 지연 | 음수/NaN은 거부 |
| `NavMeshAgent.SetDestination(Vector3)` | 유한한 목적지 유효성 확인 후 대상 설정 | Scene 컴포넌트와 로드된 `.nnav` 필요 |
| `NavMeshAgent.speed/radius/stoppingDistance` | 에이전트 이동 및 clearance 설정 | 반경·속도는 0 이상 |
| `GetComponent<T>()`, `Transform.localPosition` | native-bound Scene 컴포넌트 조회/이동 | 지원되는 NEngine native 컴포넌트만 |
| `Time.deltaTime`, `Mathf.Clamp/Lerp`, `Vector3` | 프레임 시간 및 기초 게임 수학 | .NET API 런타임 필요 |

**범위:** 이 예제는 관리형 API의 **컴파일 가능성**을 자동 테스트하며, 실제 Windows GUI GPU/오디오 배포 또는 Android 실행까지 입증하지 않는다. C# 공개 API를 확장할 때는 이 예제를 같은 빌드 체인에서 컴파일하고 문서와 함께 갱신한다.

## 프로젝트·빌드 기본 참고

- CMake 3.25+, C++20, Windows Editor/Player 실사용에는 Windows 10/11 x64와 Vulkan 가능한 GPU·드라이버.
- C# 프로젝트 빌드와 런타임에는 각각 호환되는 .NET SDK와 hostfxr/runtime가 필요하다.
- 빌드: `cmake -S . -B build -DNENGINE_BUILD_TESTS=ON -DNENGINE_BUILD_EDITOR=ON -DNENGINE_BUILD_PLAYER=ON`.
- 테스트: `cmake --build build --config Release` 후 `ctest --test-dir build -C Release --output-on-failure`.
- Windows 내보내기 예시는 [WORKFLOW.md](WORKFLOW.md) 하단에 기록한다. 실제 Android 빌드와 외부 플러그인 배포는 미완성이다.

**문서의 체크는 코드·테스트의 대체물이 아니다.** 체크 완료는 ID의 좁은 수용 조건이 같은 SHA와 검증 증거로 충족될 때만 인정한다.
