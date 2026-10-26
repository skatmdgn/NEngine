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
