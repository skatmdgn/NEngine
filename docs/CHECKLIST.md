# NEngine 1.0 — 고정 ID 기반 수용 체크리스트

**검증 기준선:** main 671cddf2d50d184b1889310f86c85a3df43306bd (2026-10-22), [Windows/Ubuntu CI 39713209272](https://github.com/skatmdgn/NEngine/actions/runs/39713209272), 양쪽 12/12 CTest 그룹 통과. 현재는 실 GPU·오디오·.NET 배포 및 Android 기기 검증을 통과했다고 주장하지 않는다.

**현재 집계:** 총 285개, 좁은 소스·자동테스트 범위 확인 53개, 미완료 232개. 항목 수는 **엔진 개발 퍼센트나 남은 시간과 같지 않다.** 새로운 ID 추가·분할은 추적하고, 완료 항목에 항상 검증된 SHA와 증거를 남긴다. 이미 [x]인 항목은 소스/CI 한정으로, 다른 실사용 항목을 자동으로 닫지 않는다.

**우선 순위:** 남은 GOV-007/010의 SHA 검증 강화와 WIN 실기기 수용, Android 빌드 토대를 구축. PR #29는 아직 main 미병합이므로 NAV-003은 반드시 미완료 유지. 작업 규칙은 [WORKFLOW.md](WORKFLOW.md), 근거와 저장소 조사는 [FILE_AUDIT.md](FILE_AUDIT.md)를 참고한다.

## GOV — 개발 관리·검증 (8/10)

참조: .github/workflows/ci.yml; tests/CMakeLists.txt

- [x] GOV-001 main의 Windows/Ubuntu 12개 CTest 그룹 성공을 검증한다 — 증거: CI 39713209272 / CI run 39713209272
- [x] GOV-002 작업 ID 변경과 증거 누락을 검사하는 자동 검증기를 만든다 — 증거: PR #30, tools/verify_checklist.py, tools/test_verify_checklist.py, CI Verify acceptance checklist 게이트
- [x] GOV-003 PR 제목과 본문에 완료한 작업 ID를 강제하는 CI 검증을 추가한다 — 증거: PR #30, tools/verify_checklist.py check_baseline, tools/test_verify_checklist.py, CI Verify acceptance checklist 게이트
- [x] GOV-004 기존 통과 테스트의 회귀를 차단한다 — 증거: PR #31, verify_test_matrix.py BASELINE 12개 유지 및 test_ci_contract.py, Windows/Ubuntu CI
- [x] GOV-005 새 요구를 발견하면 고유 ID와 추가 사유를 기록한다 — 증거: PR #31, verify_checklist.py change journal 새 ID 추가 사유 검사 및 test_verify_checklist.py, Windows/Ubuntu CI
- [x] GOV-006 작업 삭제/분할/통합 시 원본 ID를 추적할 수 있게 한다 — 증거: PR #31, verify_checklist.py append-only split/merge/revise 출처 검사 및 test_verify_checklist.py, Windows/Ubuntu CI
- [ ] GOV-007 완료 체크는 검증 결과와 같은 커밋에서만 변경한다 — 수용: 구현 + 해당 회귀 테스트 + 환경 검증(필요시) + PR/SHA
- [x] GOV-008 실제 Windows 하드웨어 테스트 기록 형식을 검증한다 — 증거: PR #31, verify_acceptance.py Windows 보고서 schema·SHA256 및 test_verify_acceptance.py, Windows/Ubuntu CI; 실제 기기 테스트 미실시
- [x] GOV-009 실제 Android 기기 테스트 기록 형식을 검증한다 — 증거: PR #31, verify_acceptance.py Android 보고서 schema·SHA256 및 test_verify_acceptance.py, Windows/Ubuntu CI; 실제 기기 테스트 미실시
- [ ] GOV-010 릴리스 후보 SHA·실행 파일·검증 로그의 일치를 확인한다 — 수용: 구현 + 해당 회귀 테스트 + 환경 검증(필요시) + PR/SHA

## CORE — Core·Scene·Prefab (9/12)

참조: engine/core; tests/core_tests.cpp

- [x] CORE-001 Entity/World 생명주기 회귀 테스트가 통과한다 — 증거: tests/core_tests.cpp / CI run 39713209272
- [x] CORE-002 Scene 저장과 복원 기본 테스트가 통과한다 — 증거: tests/core_tests.cpp / CI run 39713209272
- [x] CORE-003 Prefab 기본 테스트가 통과한다 — 증거: tests/core_tests.cpp / CI run 39713209272
- [x] CORE-004 트랜스폼 부모 변경과 계층 순환 오류를 통합 테스트한다 — 증거: PR #31, NEngineCoreAcceptanceTests 계층 재부모·순환·해제·stale parent 검증, Windows/Ubuntu CI
- [ ] CORE-005 Scene의 모든 핵심 컴포넌트가 왕복 직렬화된다 — 수용: 구현 + 해당 회귀 테스트 + 환경 검증(필요시) + PR/SHA
- [x] CORE-006 Scene의 손상·절단·과대 파일을 거부하면서 현재 World를 보존한다 — 증거: PR #31, NEngineCoreAcceptanceTests 손상·절단·33MiB 파일 거부 및 World 보존, Windows/Ubuntu CI
- [ ] CORE-007 프리팹 중첩/오버라이드/리버트를 실제 Editor에서 확인한다 — 수용: 구현 + 해당 회귀 테스트 + 환경 검증(필요시) + PR/SHA
- [x] CORE-008 엔티티 참조가 복제·삭제·재로드 뒤에도 안전하다 — 증거: PR #31, NEngineCoreAcceptanceTests 복제·삭제·slot 재사용·Scene 재로드 stale 참조 검증, Windows/Ubuntu CI
- [x] CORE-009 Play/Stop 뒤 Runtime World 변경사항이 편집 World에 남지 않는다 — 증거: PR #32, NEngineRoundTwoCoreTests, Editor Play/Pause/Step/Stop 월드 격리, Windows/Ubuntu CI
- [x] CORE-010 다중 Scene 전환·로드 실패 복구가 가능하다 — 증거: PR #32, NEngineRoundTwoCoreTests, Scene A/B/A 교체·실패 rollback, Windows/Ubuntu CI
- [x] CORE-011 Scene 저장 도중 쓰기 실패에 데이터가 보존된다 — 증거: PR #32, NEngineRoundTwoCoreTests, 씬 손상 저장·권한 실패 시 원본 보존과 atomic publish, Windows/Ubuntu CI
- [ ] CORE-012 최소 2D·3D Scene이 Player에서 동일하게 실행된다 — 수용: 구현 + 해당 회귀 테스트 + 환경 검증(필요시) + PR/SHA

## EDIT — Windows Editor (3/16)

참조: apps/editor; engine/editor; tests/editor_tests.cpp

- [x] EDIT-001 Hierarchy/Inspector 모델 회귀 테스트가 통과한다 — 증거: tests/editor_tests.cpp / CI run 39713209272
- [x] EDIT-002 Undo/Redo·Property 편집 모델 테스트가 통과한다 — 증거: tests/editor_tests.cpp / CI run 39713209272
- [x] EDIT-003 프로젝트 에셋 통합 테스트가 통과한다 — 증거: tests/editor_tests.cpp / CI run 39713209272
- [ ] EDIT-004 Windows 10 에디터 실기기 시작·종료를 검증한다 — 수용: 구현 + 해당 회귀 테스트 + 환경 검증(필요시) + PR/SHA
- [ ] EDIT-005 Windows 11 에디터 실기기 시작·종료를 검증한다 — 수용: 구현 + 해당 회귀 테스트 + 환경 검증(필요시) + PR/SHA
- [ ] EDIT-006 Scene View 2D/3D 피킹·기즈모 조작을 실기기로 검증한다 — 수용: 구현 + 해당 회귀 테스트 + 환경 검증(필요시) + PR/SHA
- [ ] EDIT-007 Inspector에서 컴포넌트 생성·삭제·속성 수정·복원을 검증한다 — 수용: 구현 + 해당 회귀 테스트 + 환경 검증(필요시) + PR/SHA
- [ ] EDIT-008 Hierarchy 다중 선택·자식 이동·복제를 검증한다 — 수용: 구현 + 해당 회귀 테스트 + 환경 검증(필요시) + PR/SHA
- [ ] EDIT-009 자유 도킹/탭/패널 분리와 배치 복원을 구현한다 — 수용: 구현 + 해당 회귀 테스트 + 환경 검증(필요시) + PR/SHA
- [ ] EDIT-010 DPI·멀티 모니터 변경 때 에디터 레이아웃을 검증한다 — 수용: 구현 + 해당 회귀 테스트 + 환경 검증(필요시) + PR/SHA
- [ ] EDIT-011 프로젝트 만들기·열기·오류 복구 UI를 완성한다 — 수용: 구현 + 해당 회귀 테스트 + 환경 검증(필요시) + PR/SHA
- [ ] EDIT-012 Scene 뷰와 Game 뷰에서 실제 Vulkan 렌더러를 사용한다 — 수용: 구현 + 해당 회귀 테스트 + 환경 검증(필요시) + PR/SHA
- [ ] EDIT-013 Asset Browser에서 가져오기·이름변경·삭제·재가져오기를 검증한다 — 수용: 구현 + 해당 회귀 테스트 + 환경 검증(필요시) + PR/SHA
- [ ] EDIT-014 C# 컴파일 진단을 Console과 소스 파일 위치에 표시한다 — 수용: 구현 + 해당 회귀 테스트 + 환경 검증(필요시) + PR/SHA
- [ ] EDIT-015 Play/Pause/Step/Stop을 실제 2D·3D 게임에 적용한다 — 수용: 구현 + 해당 회귀 테스트 + 환경 검증(필요시) + PR/SHA
- [ ] EDIT-016 다중 프로젝트 전환 시 파일 감시·캐시·핸들을 안전하게 복구한다 — 수용: 구현 + 해당 회귀 테스트 + 환경 검증(필요시) + PR/SHA

## ASSET — 에셋 파이프라인 (6/15)

참조: engine/assets; engine/render/src/model_importer.cpp; tests/asset_tests.cpp

- [x] ASSET-001 GUID/.meta와 의존성 기본 회귀 테스트가 통과한다 — 증거: tests/asset_tests.cpp / CI run 39713209272
- [x] ASSET-002 GLB/glTF/OBJ 기본 파서 회귀 테스트가 통과한다 — 증거: tests/asset_tests.cpp / CI run 39713209272
- [ ] ASSET-003 여러 외부 BIN/이미지 참조를 가진 glTF를 가져온다 — 수용: 구현 + 해당 회귀 테스트 + 환경 검증(필요시) + PR/SHA
- [x] ASSET-004 모델 디렉터리 밖의 glTF 리소스에 안전한 명시적 정책을 적용한다 — 증거: PR #32, NEngineRoundTwoAssetTests, glTF URI 경로·scheme·percent escape 거부·rollback; gltf_sidecars.cpp 수정, Windows/Ubuntu CI
- [ ] ASSET-005 FBX 모델·재질·노드 계층 가져오기를 구현한다 — 수용: 구현 + 해당 회귀 테스트 + 환경 검증(필요시) + PR/SHA
- [ ] ASSET-006 PNG·JPEG·BMP·TGA의 색상 및 알파를 실제 렌더러에서 검증한다 — 수용: 구현 + 해당 회귀 테스트 + 환경 검증(필요시) + PR/SHA
- [ ] ASSET-007 WebP 텍스처 가져오기를 구현한다 — 수용: 구현 + 해당 회귀 테스트 + 환경 검증(필요시) + PR/SHA
- [ ] ASSET-008 GPU 압축 텍스처 트랜스코딩과 폴백을 구현한다 — 수용: 구현 + 해당 회귀 테스트 + 환경 검증(필요시) + PR/SHA
- [ ] ASSET-009 멀티 머티리얼 모델의 모든 하위 에셋을 GUID로 저장한다 — 수용: 구현 + 해당 회귀 테스트 + 환경 검증(필요시) + PR/SHA
- [ ] ASSET-010 모든 모델 메쉬·재질·텍스처가 Player로 함께 내보내진다 — 수용: 구현 + 해당 회귀 테스트 + 환경 검증(필요시) + PR/SHA
- [x] ASSET-011 텍스처·모델 크기 한도를 정의하고 과대 입력을 안전하게 거부한다 — 증거: PR #32, NEngineRoundTwoAssetTests, 33MiB glTF·9KiB meta 상한 초과 거부, Windows/Ubuntu CI
- [x] ASSET-012 임포트·리임포트·이름 변경 뒤 에셋 GUID가 안정적이다 — 증거: PR #32, NEngineRoundTwoAssetTests, 재가져오기·이름변경·재시작 후 AssetGuid 안정성, Windows/Ubuntu CI
- [ ] ASSET-013 원본 누락·참조 오류에서 사용자에게 복구 가능한 진단을 제공한다 — 수용: 구현 + 해당 회귀 테스트 + 환경 검증(필요시) + PR/SHA
- [x] ASSET-014 경로 순회와 심볼릭 링크를 포함한 악성 에셋을 거부한다 — 증거: PR #32, NEngineRoundTwoAssetTests, symlinked 에셋 GUID 생성 거부와 glTF 외부 symlink escape; asset_database.cpp 수정, Windows/Ubuntu CI
- [ ] ASSET-015 에셋 핫리로드 후 실제 화면과 런타임 캐시가 동일하다 — 수용: 구현 + 해당 회귀 테스트 + 환경 검증(필요시) + PR/SHA

## REND — 2D/3D 렌더링 (2/16)

참조: engine/render; tests/render_tests.cpp

- [x] REND-001 메시·행렬·카메라 관련 기본 테스트가 통과한다 — 증거: tests/render_tests.cpp / CI run 39713209272
- [x] REND-002 디코딩·재질·렌더 자원 회귀 테스트가 통과한다 — 증거: tests/render_tests.cpp / CI run 39713209272
- [ ] REND-003 Windows 실 GPU에서 텍스처 없는 2D 장면을 표시한다 — 수용: 구현 + 해당 회귀 테스트 + 환경 검증(필요시) + PR/SHA
- [ ] REND-004 Windows 실 GPU에서 텍스처 있는 3D 장면을 표시한다 — 수용: 구현 + 해당 회귀 테스트 + 환경 검증(필요시) + PR/SHA
- [ ] REND-005 Scene View GDI 기본 경로를 Vulkan 렌더링으로 교체한다 — 수용: 구현 + 해당 회귀 테스트 + 환경 검증(필요시) + PR/SHA
- [ ] REND-006 GPU 기기 분실·화면 변경·스왑체인 재생성 상황을 복구한다 — 수용: 구현 + 해당 회귀 테스트 + 환경 검증(필요시) + PR/SHA
- [ ] REND-007 미지원 Vulkan 장치·드라이버에서 오류를 설명한다 — 수용: 구현 + 해당 회귀 테스트 + 환경 검증(필요시) + PR/SHA
- [ ] REND-008 다중 조명과 그림자 렌더링을 검증한다 — 수용: 구현 + 해당 회귀 테스트 + 환경 검증(필요시) + PR/SHA
- [ ] REND-009 PBR 금속성·거칠기·노멀 맵을 참조 장면으로 검증한다 — 수용: 구현 + 해당 회귀 테스트 + 환경 검증(필요시) + PR/SHA
- [ ] REND-010 알파 테스트·블렌딩·투명 정렬을 검증한다 — 수용: 구현 + 해당 회귀 테스트 + 환경 검증(필요시) + PR/SHA
- [ ] REND-011 2D 스프라이트 피벗·정렬 레이어와 깊이를 검증한다 — 수용: 구현 + 해당 회귀 테스트 + 환경 검증(필요시) + PR/SHA
- [ ] REND-012 화면 크기와 카메라 종횡비 변경 때 출력이 유지된다 — 수용: 구현 + 해당 회귀 테스트 + 환경 검증(필요시) + PR/SHA
- [ ] REND-013 멀티 카메라와 렌더 타깃을 검증한다 — 수용: 구현 + 해당 회귀 테스트 + 환경 검증(필요시) + PR/SHA
- [ ] REND-014 텍스처/메시 핫리로드 시 GPU 메모리가 누출되지 않는다 — 수용: 구현 + 해당 회귀 테스트 + 환경 검증(필요시) + PR/SHA
- [ ] REND-015 프레임 시간·드로우 콜·GPU 메모리 기준을 작성해 회귀를 막는다 — 수용: 구현 + 해당 회귀 테스트 + 환경 검증(필요시) + PR/SHA
- [ ] REND-016 Android Vulkan surface 렌더링을 실기기로 검증한다 — 수용: 구현 + 해당 회귀 테스트 + 환경 검증(필요시) + PR/SHA

## CS — C#·IDE (2/14)

참조: engine/scripting; tests/scripting_tests.cpp

- [x] CS-001 C# 프로젝트 생성/빌드 기본 테스트가 통과한다 — 증거: tests/scripting_tests.cpp / CI run 39713209272
- [x] CS-002 hostfxr/Bridge/ScriptBehaviour 기본 테스트가 통과한다 — 증거: tests/scripting_tests.cpp / CI run 39713209272
- [ ] CS-003 새로운 Windows 프로젝트에서 C# 스크립트를 만들고 Play한다 — 수용: 구현 + 해당 회귀 테스트 + 환경 검증(필요시) + PR/SHA
- [ ] CS-004 Windows Player가 같은 C# 스크립트를 번들에서 실행한다 — 수용: 구현 + 해당 회귀 테스트 + 환경 검증(필요시) + PR/SHA
- [ ] CS-005 GameObject/Transform/Component 변경이 managed/native 양쪽에서 같다 — 수용: 구현 + 해당 회귀 테스트 + 환경 검증(필요시) + PR/SHA
- [ ] CS-006 Camera/Renderer/Audio/Collider 속성이 C#에서 동작한다 — 수용: 구현 + 해당 회귀 테스트 + 환경 검증(필요시) + PR/SHA
- [ ] CS-007 코루틴·Time·스크립트 실행 순서를 실제 게임으로 검증한다 — 수용: 구현 + 해당 회귀 테스트 + 환경 검증(필요시) + PR/SHA
- [ ] CS-008 C# 예외가 호스트를 종료하지 않고 스택을 출력한다 — 수용: 구현 + 해당 회귀 테스트 + 환경 검증(필요시) + PR/SHA
- [ ] CS-009 반복 Play/Stop/핫리로드에서 메모리와 핸들이 누출되지 않는다 — 수용: 구현 + 해당 회귀 테스트 + 환경 검증(필요시) + PR/SHA
- [ ] CS-010 Visual Studio 심볼·브레이크포인트 attach를 검증한다 — 수용: 구현 + 해당 회귀 테스트 + 환경 검증(필요시) + PR/SHA
- [ ] CS-011 Rider 프로젝트와 디버깅 워크플로를 검증한다 — 수용: 구현 + 해당 회귀 테스트 + 환경 검증(필요시) + PR/SHA
- [ ] CS-012 GameScripts.dll 이름·버전 불일치 오류를 명확히 진단한다 — 수용: 구현 + 해당 회귀 테스트 + 환경 검증(필요시) + PR/SHA
- [ ] CS-013 관리형 외부 라이브러리 의존성을 컴파일·실행·배포한다 — 수용: 구현 + 해당 회귀 테스트 + 환경 검증(필요시) + PR/SHA
- [ ] CS-014 C# 공개 API에 컴파일 가능한 최소 예제와 참조 문서를 제공한다 — 수용: 구현 + 해당 회귀 테스트 + 환경 검증(필요시) + PR/SHA

## PHYS — 2D/3D 물리 (5/13)

참조: engine/physics; tests/physics_tests.cpp

- [x] PHYS-001 Rigidbody·접촉과 충돌 기본 테스트가 통과한다 — 증거: tests/physics_tests.cpp / CI run 39713209272
- [x] PHYS-002 Box/Sphere/Capsule 및 2D 형태 테스트가 통과한다 — 증거: tests/physics_tests.cpp / CI run 39713209272
- [x] PHYS-003 Raycast/BoxCast/Overlap 기본 테스트가 통과한다 — 증거: tests/physics_tests.cpp / CI run 39713209272
- [ ] PHYS-004 SphereCast 쿼리를 구현·검증한다 — 수용: 구현 + 해당 회귀 테스트 + 환경 검증(필요시) + PR/SHA
- [ ] PHYS-005 CapsuleCast 쿼리를 구현·검증한다 — 수용: 구현 + 해당 회귀 테스트 + 환경 검증(필요시) + PR/SHA
- [ ] PHYS-006 고속 충돌 검출에서 터널링을 방지한다 — 수용: 구현 + 해당 회귀 테스트 + 환경 검증(필요시) + PR/SHA
- [ ] PHYS-007 장시간 쌓인 동적 물체의 안정성과 수치 발산을 검증한다 — 수용: 구현 + 해당 회귀 테스트 + 환경 검증(필요시) + PR/SHA
- [ ] PHYS-008 Trigger/Collision 이벤트가 C#·Editor·Player에서 일치한다 — 수용: 구현 + 해당 회귀 테스트 + 환경 검증(필요시) + PR/SHA
- [x] PHYS-009 레이어 마스크·필터가 물리 쿼리마다 동일하다 — 증거: PR #32, NEngineRoundTwoAnimPhysicsTests, 3D overlap/raycast 계층 mask 및 trigger, Windows/Ubuntu CI
- [ ] PHYS-010 Rigidbody의 정적·동적·키네마틱 전환이 안전하다 — 수용: 구현 + 해당 회귀 테스트 + 환경 검증(필요시) + PR/SHA
- [ ] PHYS-011 FixedUpdate·pause·time scale에 물리 시뮬레이션이 연동된다 — 수용: 구현 + 해당 회귀 테스트 + 환경 검증(필요시) + PR/SHA
- [x] PHYS-012 2D·3D 물리가 의도치 않게 혼합되지 않는다 — 증거: PR #32, NEngineRoundTwoAnimPhysicsTests, 2D·3D 분리 충돌·물리 적분·분리 query, Windows/Ubuntu CI
- [ ] PHYS-013 1000개 이상 물체에서 broadphase 성능과 정확도를 검증한다 — 수용: 구현 + 해당 회귀 테스트 + 환경 검증(필요시) + PR/SHA

## AUD — Audio (2/12)

참조: engine/audio; tests/audio_tests.cpp

- [x] AUD-001 WAV 디코드·재생·소프트웨어 믹싱 기본 테스트가 통과한다 — 증거: tests/audio_tests.cpp / CI run 39713209272
- [x] AUD-002 AudioSource/Listener 기본 회귀 테스트가 통과한다 — 증거: tests/audio_tests.cpp / CI run 39713209272
- [ ] AUD-003 Windows WASAPI shared 실출력을 검증한다 — 수용: 구현 + 해당 회귀 테스트 + 환경 검증(필요시) + PR/SHA
- [ ] AUD-004 오디오 장치 연결·해제·전환 후 재생이 복구된다 — 수용: 구현 + 해당 회귀 테스트 + 환경 검증(필요시) + PR/SHA
- [ ] AUD-005 샘플레이트 차이를 리샘플링해 아티팩트 없이 출력한다 — 수용: 구현 + 해당 회귀 테스트 + 환경 검증(필요시) + PR/SHA
- [ ] AUD-006 OGG Vorbis 디코딩·스트리밍을 구현한다 — 수용: 구현 + 해당 회귀 테스트 + 환경 검증(필요시) + PR/SHA
- [ ] AUD-007 MP3 디코딩·스트리밍을 구현한다 — 수용: 구현 + 해당 회귀 테스트 + 환경 검증(필요시) + PR/SHA
- [ ] AUD-008 FLAC 디코딩·스트리밍을 구현한다 — 수용: 구현 + 해당 회귀 테스트 + 환경 검증(필요시) + PR/SHA
- [ ] AUD-009 BGM 길이·동시 SFX 부하 테스트를 수행한다 — 수용: 구현 + 해당 회귀 테스트 + 환경 검증(필요시) + PR/SHA
- [ ] AUD-010 공간음향 거리 감쇠·패닝을 실제 출력에서 검증한다 — 수용: 구현 + 해당 회귀 테스트 + 환경 검증(필요시) + PR/SHA
- [ ] AUD-011 C# AudioSource Play/Stop/Loop/Volume이 Player에서 동작한다 — 수용: 구현 + 해당 회귀 테스트 + 환경 검증(필요시) + PR/SHA
- [ ] AUD-012 소리 장치 실패 시 안전하게 폴백하고 오류를 표시한다 — 수용: 구현 + 해당 회귀 테스트 + 환경 검증(필요시) + PR/SHA

## INPUT — Input (4/11)

참조: engine/input; tests/input_tests.cpp

- [x] INPUT-001 키보드·마우스 입력 회귀 테스트가 통과한다 — 증거: tests/input_tests.cpp / CI run 39713209272
- [x] INPUT-002 Input.ninput 액션 맵 회귀 테스트가 통과한다 — 증거: tests/input_tests.cpp / CI run 39713209272
- [ ] INPUT-003 Editor에서 Action Map을 시각적으로 생성·수정·삭제한다 — 수용: 구현 + 해당 회귀 테스트 + 환경 검증(필요시) + PR/SHA
- [ ] INPUT-004 Action Map 변경이 Windows Player 번들까지 전달된다 — 수용: 구현 + 해당 회귀 테스트 + 환경 검증(필요시) + PR/SHA
- [ ] INPUT-005 C# 액션 입력과 물리 키 상태가 실제 키보드에서 일치한다 — 수용: 구현 + 해당 회귀 테스트 + 환경 검증(필요시) + PR/SHA
- [ ] INPUT-006 마우스 버튼·좌표·스크롤이 DPI 변경 시 정상 동작한다 — 수용: 구현 + 해당 회귀 테스트 + 환경 검증(필요시) + PR/SHA
- [x] INPUT-007 포커스를 잃으면 눌린 입력이 고착되지 않는다 — 증거: PR #32, NEngineRoundTwoInputUiTests, 같은 프레임 focus-loss 키·마우스 취소 및 커서 리셋; input_state.cpp 수정, Windows/Ubuntu CI
- [ ] INPUT-008 게임패드 입력·연결 해제·재연결을 구현한다 — 수용: 구현 + 해당 회귀 테스트 + 환경 검증(필요시) + PR/SHA
- [ ] INPUT-009 Android 터치·멀티 터치를 실제 기기에서 검증한다 — 수용: 구현 + 해당 회귀 테스트 + 환경 검증(필요시) + PR/SHA
- [ ] INPUT-010 UI가 소비한 입력을 게임플레이가 중복 처리하지 않는다 — 수용: 구현 + 해당 회귀 테스트 + 환경 검증(필요시) + PR/SHA
- [x] INPUT-011 입력맵 부재·손상 오류를 안전하게 처리한다 — 증거: PR #32, NEngineRoundTwoInputUiTests, 악성·누락·과대 ActionMap 거부·기존 Action 보존, Windows/Ubuntu CI

## ANIM — Animation (3/10)

참조: engine/render/src/animation_controller.cpp; tests/render_tests.cpp

- [x] ANIM-001 SpriteAnimation과 Controller 기본 회귀 테스트가 통과한다 — 증거: tests/render_tests.cpp / CI run 39713209272
- [ ] ANIM-002 스프라이트 시트·클립·타임라인 Editor를 완성한다 — 수용: 구현 + 해당 회귀 테스트 + 환경 검증(필요시) + PR/SHA
- [ ] ANIM-003 2D SpriteAnimator 클립을 Scene 저장 후 Player에서 재생한다 — 수용: 구현 + 해당 회귀 테스트 + 환경 검증(필요시) + PR/SHA
- [x] ANIM-004 애니메이션 상태 머신 Transition 조건을 검증한다 — 증거: PR #32, NEngineRoundTwoAnimPhysicsTests, 상태머신 전이 조건·exit 시간·복귀·잘못된 참조 검증, Windows/Ubuntu CI
- [ ] ANIM-005 Blend Tree 보간과 중단·재진입을 검증한다 — 수용: 구현 + 해당 회귀 테스트 + 환경 검증(필요시) + PR/SHA
- [ ] ANIM-006 애니메이션 이벤트를 ScriptBehaviour에 전달한다 — 수용: 구현 + 해당 회귀 테스트 + 환경 검증(필요시) + PR/SHA
- [ ] ANIM-007 3D 스켈레탈 애니메이션 가져오기·스키닝을 구현한다 — 수용: 구현 + 해당 회귀 테스트 + 환경 검증(필요시) + PR/SHA
- [ ] ANIM-008 Root Motion과 시간 배율을 구현한다 — 수용: 구현 + 해당 회귀 테스트 + 환경 검증(필요시) + PR/SHA
- [ ] ANIM-009 Inspector에서 애니메이션 파라미터를 조작·관찰한다 — 수용: 구현 + 해당 회귀 테스트 + 환경 검증(필요시) + PR/SHA
- [x] ANIM-010 중단·재시작·루프·장시간 재생의 안정성을 검증한다 — 증거: PR #32, NEngineRoundTwoAnimPhysicsTests, 1만 프레임 루프·클립 종료·재시작·비정상 시간, Windows/Ubuntu CI

## UI — Canvas·런타임 UI (3/14)

참조: engine/ui; engine/player/src/ui_runtime.cpp; tests/ui_tests.cpp

- [x] UI-001 Canvas 데이터·이벤트 기본 회귀 테스트가 통과한다 — 증거: tests/ui_tests.cpp / CI run 39713209272
- [ ] UI-002 Canvas 계층·Anchor·RectTransform을 Editor에서 편집한다 — 수용: 구현 + 해당 회귀 테스트 + 환경 검증(필요시) + PR/SHA
- [ ] UI-003 Button/Image/Text/Panel을 함께 배치하고 저장한다 — 수용: 구현 + 해당 회귀 테스트 + 환경 검증(필요시) + PR/SHA
- [x] UI-004 Pointer 클릭·드래그·키보드 포커스를 통합 검증한다 — 증거: PR #32, NEngineRoundTwoInputUiTests, pointer hover/down/drag/up/cancel 및 Tab/Enter 포커스, Windows/Ubuntu CI
- [ ] UI-005 UI Button 클릭으로 C# 콜백을 실제 호출한다 — 수용: 구현 + 해당 회귀 테스트 + 환경 검증(필요시) + PR/SHA
- [ ] UI-006 Windows Vulkan 화면에 런타임 UI를 합성한다 — 수용: 구현 + 해당 회귀 테스트 + 환경 검증(필요시) + PR/SHA
- [ ] UI-007 한글 글리프·한글 문자열을 완전하게 렌더한다 — 수용: 구현 + 해당 회귀 테스트 + 환경 검증(필요시) + PR/SHA
- [ ] UI-008 폰트 대체·조합 Unicode·줄바꿈을 검증한다 — 수용: 구현 + 해당 회귀 테스트 + 환경 검증(필요시) + PR/SHA
- [ ] UI-009 UI 셰이핑과 텍스트 정렬·크기 조절을 검증한다 — 수용: 구현 + 해당 회귀 테스트 + 환경 검증(필요시) + PR/SHA
- [x] UI-010 UI 앵커·DPI·화면비 변경을 검증한다 — 증거: PR #32, NEngineRoundTwoInputUiTests, UI anchor viewport 크기 변경·중앙 정렬·0 영역 검증, Windows/Ubuntu CI
- [ ] UI-011 이미지 알파·클리핑·마스크·레이아웃을 검증한다 — 수용: 구현 + 해당 회귀 테스트 + 환경 검증(필요시) + PR/SHA
- [ ] UI-012 AssetGuid 이미지가 Editor·Player에서 같은 결과를 낸다 — 수용: 구현 + 해당 회귀 테스트 + 환경 검증(필요시) + PR/SHA
- [ ] UI-013 최소 UI 메뉴 게임을 Windows Player에서 직접 클릭한다 — 수용: 구현 + 해당 회귀 테스트 + 환경 검증(필요시) + PR/SHA
- [ ] UI-014 Android 터치 UI와 safe area를 검증한다 — 수용: 구현 + 해당 회귀 테스트 + 환경 검증(필요시) + PR/SHA

## NAV — NavMesh·AI 이동 (2/15)

참조: engine/navigation; tests/navigation_tests.cpp

- [x] NAV-001 NavMesh·Agent·Obstacle·Link 기본 테스트가 통과한다 — 증거: tests/navigation_tests.cpp / CI run 39713209272
- [x] NAV-002 .nnav 경로 직렬화 기본 테스트가 통과한다 — 증거: tests/navigation_tests.cpp / CI run 39713209272
- [ ] NAV-003 PR #29를 main에 병합하고 triangle bake 테스트를 main에서 재검증한다 — 수용: 구현 + 해당 회귀 테스트 + 환경 검증(필요시) + PR/SHA
- [ ] NAV-004 Scene 메시로부터 자동 NavMesh 기하를 수집한다 — 수용: 구현 + 해당 회귀 테스트 + 환경 검증(필요시) + PR/SHA
- [ ] NAV-005 모델 트랜스폼·좌표 변환을 Bake에 정확히 적용한다 — 수용: 구현 + 해당 회귀 테스트 + 환경 검증(필요시) + PR/SHA
- [ ] NAV-006 지형 경사·보행 가능 영역·영역 비용 UI를 구현한다 — 수용: 구현 + 해당 회귀 테스트 + 환경 검증(필요시) + PR/SHA
- [ ] NAV-007 에이전트 반경·장애물 clearance를 Bake에 반영한다 — 수용: 구현 + 해당 회귀 테스트 + 환경 검증(필요시) + PR/SHA
- [ ] NAV-008 T-junction·비매니폴드 연결을 처리한다 — 수용: 구현 + 해당 회귀 테스트 + 환경 검증(필요시) + PR/SHA
- [ ] NAV-009 Portal/Funnel 경로 평활화를 구현한다 — 수용: 구현 + 해당 회귀 테스트 + 환경 검증(필요시) + PR/SHA
- [ ] NAV-010 NavMeshAgent를 Scene 컴포넌트와 연결한다 — 수용: 구현 + 해당 회귀 테스트 + 환경 검증(필요시) + PR/SHA
- [ ] NAV-011 NavMeshAgent에 C# 목적지/이동/정지 API를 제공한다 — 수용: 구현 + 해당 회귀 테스트 + 환경 검증(필요시) + PR/SHA
- [ ] NAV-012 동적 장애물 변경·링크 변경 후 경로를 재계산한다 — 수용: 구현 + 해당 회귀 테스트 + 환경 검증(필요시) + PR/SHA
- [ ] NAV-013 다수 에이전트 회피·충돌 및 성능을 검증한다 — 수용: 구현 + 해당 회귀 테스트 + 환경 검증(필요시) + PR/SHA
- [ ] NAV-014 Windows 배포 게임에서 목적지까지 캐릭터가 이동한다 — 수용: 구현 + 해당 회귀 테스트 + 환경 검증(필요시) + PR/SHA
- [ ] NAV-015 Android 게임에서 동일 NavMeshAgent 예제를 실행한다 — 수용: 구현 + 해당 회귀 테스트 + 환경 검증(필요시) + PR/SHA

## WIN — Windows Player (2/17)

참조: engine/player; apps/player; tests/player_tests.cpp

- [x] WIN-001 Windows Player Bundle/Exporter 기본 테스트가 통과한다 — 증거: tests/player_tests.cpp / CI run 39713209272
- [x] WIN-002 Player CLI 스모크가 통과한다 — 증거: tests/player_cli_smoke.cmake / CI run 39713209272
- [ ] WIN-003 새 Windows 10 PC에 게임 번들을 배포·실행한다 — 수용: 구현 + 해당 회귀 테스트 + 환경 검증(필요시) + PR/SHA
- [ ] WIN-004 새 Windows 11 PC에 게임 번들을 배포·실행한다 — 수용: 구현 + 해당 회귀 테스트 + 환경 검증(필요시) + PR/SHA
- [ ] WIN-005 기본 Scene의 World·Physics·Animation을 Player에서 실행한다 — 수용: 구현 + 해당 회귀 테스트 + 환경 검증(필요시) + PR/SHA
- [ ] WIN-006 C# 게임 스크립트를 Player에서 실행한다 — 수용: 구현 + 해당 회귀 테스트 + 환경 검증(필요시) + PR/SHA
- [ ] WIN-007 실제 렌더러에 멀티 머티리얼 모델을 표시한다 — 수용: 구현 + 해당 회귀 테스트 + 환경 검증(필요시) + PR/SHA
- [ ] WIN-008 UI·3D 렌더링·오디오를 동시에 실행한다 — 수용: 구현 + 해당 회귀 테스트 + 환경 검증(필요시) + PR/SHA
- [ ] WIN-009 입력 액션을 Editor 설정대로 Player에서 실행한다 — 수용: 구현 + 해당 회귀 테스트 + 환경 검증(필요시) + PR/SHA
- [ ] WIN-010 누락·손상 파일의 오류를 안내하고 안전 종료한다 — 수용: 구현 + 해당 회귀 테스트 + 환경 검증(필요시) + PR/SHA
- [ ] WIN-011 4K/DPI·화면 크기 변경에서 화면이 유지된다 — 수용: 구현 + 해당 회귀 테스트 + 환경 검증(필요시) + PR/SHA
- [ ] WIN-012 의존 DLL/런타임을 포함한 재현 가능한 번들을 배포한다 — 수용: 구현 + 해당 회귀 테스트 + 환경 검증(필요시) + PR/SHA
- [ ] WIN-013 Project·Assets 경로에 한글이 포함돼도 실행된다 — 수용: 구현 + 해당 회귀 테스트 + 환경 검증(필요시) + PR/SHA
- [ ] WIN-014 Export 작업 실패 시 대상 출력 폴더를 손상시키지 않는다 — 수용: 구현 + 해당 회귀 테스트 + 환경 검증(필요시) + PR/SHA
- [ ] WIN-015 최소 2D 게임을 만들고 Windows Player로 완주한다 — 수용: 구현 + 해당 회귀 테스트 + 환경 검증(필요시) + PR/SHA
- [ ] WIN-016 최소 3D 게임을 만들고 Windows Player로 완주한다 — 수용: 구현 + 해당 회귀 테스트 + 환경 검증(필요시) + PR/SHA
- [ ] WIN-017 두 게임 모두 종료·재시작 뒤 게임 저장 상태가 복원된다 — 수용: 구현 + 해당 회귀 테스트 + 환경 검증(필요시) + PR/SHA

## AND — Android Player·APK/AAB (0/21)

참조: engine/platform; engine/render; engine/player

- [ ] AND-001 Android SDK/NDK/JDK/Gradle 설치 및 최소 버전을 정의한다 — 수용: 구현 + 해당 회귀 테스트 + 환경 검증(필요시) + PR/SHA
- [ ] AND-002 Android 툴체인 선택·검증·프로필을 구현한다 — 수용: 구현 + 해당 회귀 테스트 + 환경 검증(필요시) + PR/SHA
- [ ] AND-003 Android ARM64 CMake 크로스 컴파일을 구현한다 — 수용: 구현 + 해당 회귀 테스트 + 환경 검증(필요시) + PR/SHA
- [ ] AND-004 NativeActivity/Activity 앱 부팅을 구현한다 — 수용: 구현 + 해당 회귀 테스트 + 환경 검증(필요시) + PR/SHA
- [ ] AND-005 Android Vulkan Surface/Swapchain을 구현한다 — 수용: 구현 + 해당 회귀 테스트 + 환경 검증(필요시) + PR/SHA
- [ ] AND-006 화면 회전·백그라운드 전환·수면 복구를 구현한다 — 수용: 구현 + 해당 회귀 테스트 + 환경 검증(필요시) + PR/SHA
- [ ] AND-007 Android 터치·게임 입력 백엔드를 구현한다 — 수용: 구현 + 해당 회귀 테스트 + 환경 검증(필요시) + PR/SHA
- [ ] AND-008 Android 오디오 출력 백엔드를 구현한다 — 수용: 구현 + 해당 회귀 테스트 + 환경 검증(필요시) + PR/SHA
- [ ] AND-009 Android AssetManager·파일 I/O를 구현한다 — 수용: 구현 + 해당 회귀 테스트 + 환경 검증(필요시) + PR/SHA
- [ ] AND-010 Android C# 호스팅 또는 AOT 게임 스크립트 실행을 구현한다 — 수용: 구현 + 해당 회귀 테스트 + 환경 검증(필요시) + PR/SHA
- [ ] AND-011 Android 라이브러리 ABI별 의존성을 패키징한다 — 수용: 구현 + 해당 회귀 테스트 + 환경 검증(필요시) + PR/SHA
- [ ] AND-012 Vulkan 압축 텍스처 포맷 폴백을 구현한다 — 수용: 구현 + 해당 회귀 테스트 + 환경 검증(필요시) + PR/SHA
- [ ] AND-013 Android Debug APK 출력·설치를 구현한다 — 수용: 구현 + 해당 회귀 테스트 + 환경 검증(필요시) + PR/SHA
- [ ] AND-014 Android Release APK 서명·실행을 검증한다 — 수용: 구현 + 해당 회귀 테스트 + 환경 검증(필요시) + PR/SHA
- [ ] AND-015 Android AAB 출력·서명을 검증한다 — 수용: 구현 + 해당 회귀 테스트 + 환경 검증(필요시) + PR/SHA
- [ ] AND-016 Android Manifest/권한/패키지 이름을 Editor에서 관리한다 — 수용: 구현 + 해당 회귀 테스트 + 환경 검증(필요시) + PR/SHA
- [ ] AND-017 Android 앱 업데이트 후 데이터를 보존한다 — 수용: 구현 + 해당 회귀 테스트 + 환경 검증(필요시) + PR/SHA
- [ ] AND-018 실제 Android 폰에서 2D 게임을 실행한다 — 수용: 구현 + 해당 회귀 테스트 + 환경 검증(필요시) + PR/SHA
- [ ] AND-019 실제 Android 폰에서 3D 게임을 실행한다 — 수용: 구현 + 해당 회귀 테스트 + 환경 검증(필요시) + PR/SHA
- [ ] AND-020 두 개 이상 Android 하드웨어/OS 환경에서 동작한다 — 수용: 구현 + 해당 회귀 테스트 + 환경 검증(필요시) + PR/SHA
- [ ] AND-021 Windows와 Android에서 동일 게임 Scene·C# 동작을 검증한다 — 수용: 구현 + 해당 회귀 테스트 + 환경 검증(필요시) + PR/SHA

## PLUG — 패키지·외부 SDK (0/18)

참조: engine/scripting; engine/player

- [ ] PLUG-001 패키지 매니페스트·버전 지정 형식을 정의한다 — 수용: 구현 + 해당 회귀 테스트 + 환경 검증(필요시) + PR/SHA
- [ ] PLUG-002 종속성 그래프·버전 충돌 검출을 구현한다 — 수용: 구현 + 해당 회귀 테스트 + 환경 검증(필요시) + PR/SHA
- [ ] PLUG-003 패키지 캐시·체크섬·잠금파일을 구현한다 — 수용: 구현 + 해당 회귀 테스트 + 환경 검증(필요시) + PR/SHA
- [ ] PLUG-004 NuGet restore를 Editor·C# 빌드에 통합한다 — 수용: 구현 + 해당 회귀 테스트 + 환경 검증(필요시) + PR/SHA
- [ ] PLUG-005 NuGet managed DLL을 Windows Player에 패키징한다 — 수용: 구현 + 해당 회귀 테스트 + 환경 검증(필요시) + PR/SHA
- [ ] PLUG-006 임의 managed DLL 플러그인을 Editor에서 등록한다 — 수용: 구현 + 해당 회귀 테스트 + 환경 검증(필요시) + PR/SHA
- [ ] PLUG-007 Windows native DLL 플러그인 로딩을 구현한다 — 수용: 구현 + 해당 회귀 테스트 + 환경 검증(필요시) + PR/SHA
- [ ] PLUG-008 native DLL 보안 경로·오류 처리를 검증한다 — 수용: 구현 + 해당 회귀 테스트 + 환경 검증(필요시) + PR/SHA
- [ ] PLUG-009 P/Invoke 기반 외부 C# 플러그인을 실행한다 — 수용: 구현 + 해당 회귀 테스트 + 환경 검증(필요시) + PR/SHA
- [ ] PLUG-010 Android AAR 의존성을 앱 빌드에 통합한다 — 수용: 구현 + 해당 회귀 테스트 + 환경 검증(필요시) + PR/SHA
- [ ] PLUG-011 Android JAR과 C#-Java 호출 브리지를 구현한다 — 수용: 구현 + 해당 회귀 테스트 + 환경 검증(필요시) + PR/SHA
- [ ] PLUG-012 Android .so ABI별 패키징을 구현한다 — 수용: 구현 + 해당 회귀 테스트 + 환경 검증(필요시) + PR/SHA
- [ ] PLUG-013 Maven 의존성·Gradle 병합을 구현한다 — 수용: 구현 + 해당 회귀 테스트 + 환경 검증(필요시) + PR/SHA
- [ ] PLUG-014 Android 외부 SDK 초기화·종료와 권한 흐름을 검증한다 — 수용: 구현 + 해당 회귀 테스트 + 환경 검증(필요시) + PR/SHA
- [ ] PLUG-015 Steamworks.NET 계열 실제 Windows 플러그인 샘플을 실행한다 — 수용: 구현 + 해당 회귀 테스트 + 환경 검증(필요시) + PR/SHA
- [ ] PLUG-016 AdMob 계열 실제 Android 플러그인 샘플을 실행한다 — 수용: 구현 + 해당 회귀 테스트 + 환경 검증(필요시) + PR/SHA
- [ ] PLUG-017 JSON NuGet 라이브러리를 예제 C# 게임에 적용한다 — 수용: 구현 + 해당 회귀 테스트 + 환경 검증(필요시) + PR/SHA
- [ ] PLUG-018 외부 플러그인 제거·업데이트 후 캐시와 앱이 정합적이다 — 수용: 구현 + 해당 회귀 테스트 + 환경 검증(필요시) + PR/SHA

## QA — 제품 수용·릴리스 (2/22)

참조: CMakeLists.txt; .github/workflows/ci.yml; tests

- [x] QA-001 Windows와 Ubuntu Release 빌드 및 자동 테스트 통과를 기록한다 — 증거: PR #31, ci.yml Release 구성·빌드·CTest 및 NEngineCoreAcceptanceTests, Windows/Ubuntu CI
- [ ] QA-002 Windows 실제 GPU·입력·오디오 실기기 통합 테스트를 통과한다 — 수용: 구현 + 해당 회귀 테스트 + 환경 검증(필요시) + PR/SHA
- [ ] QA-003 Android 실제 기기 통합 테스트를 통과한다 — 수용: 구현 + 해당 회귀 테스트 + 환경 검증(필요시) + PR/SHA
- [ ] QA-004 Editor 8시간 실행 후 메모리·핸들 누출 기준을 통과한다 — 수용: 구현 + 해당 회귀 테스트 + 환경 검증(필요시) + PR/SHA
- [ ] QA-005 Player 8시간 실행 후 메모리·핸들 누출 기준을 통과한다 — 수용: 구현 + 해당 회귀 테스트 + 환경 검증(필요시) + PR/SHA
- [ ] QA-006 한글/공백/긴 경로 프로젝트 생성·빌드·배포를 검증한다 — 수용: 구현 + 해당 회귀 테스트 + 환경 검증(필요시) + PR/SHA
- [ ] QA-007 Debug/Release 실행 결과 차이를 회귀 테스트한다 — 수용: 구현 + 해당 회귀 테스트 + 환경 검증(필요시) + PR/SHA
- [ ] QA-008 새 Windows PC에서 2D 게임 제작·내보내기를 재현한다 — 수용: 구현 + 해당 회귀 테스트 + 환경 검증(필요시) + PR/SHA
- [ ] QA-009 새 Windows PC에서 3D 게임 제작·내보내기를 재현한다 — 수용: 구현 + 해당 회귀 테스트 + 환경 검증(필요시) + PR/SHA
- [ ] QA-010 Android 배포본 크래시 로그·심볼화를 검증한다 — 수용: 구현 + 해당 회귀 테스트 + 환경 검증(필요시) + PR/SHA
- [ ] QA-011 최소 2D 샘플 게임과 자동 빌드 스모크를 제공한다 — 수용: 구현 + 해당 회귀 테스트 + 환경 검증(필요시) + PR/SHA
- [ ] QA-012 최소 3D 샘플 게임과 자동 빌드 스모크를 제공한다 — 수용: 구현 + 해당 회귀 테스트 + 환경 검증(필요시) + PR/SHA
- [ ] QA-013 사용자 문서·C# SDK 예제가 릴리스와 일치한다 — 수용: 구현 + 해당 회귀 테스트 + 환경 검증(필요시) + PR/SHA
- [ ] QA-014 Editor 설치 관리자와 제거 절차를 검증한다 — 수용: 구현 + 해당 회귀 테스트 + 환경 검증(필요시) + PR/SHA
- [ ] QA-015 업데이트·서명·릴리스 아티팩트 재현 절차를 검증한다 — 수용: 구현 + 해당 회귀 테스트 + 환경 검증(필요시) + PR/SHA
- [ ] QA-016 에셋 파서·불신 입력·플러그인 경로 보안 테스트를 수행한다 — 수용: 구현 + 해당 회귀 테스트 + 환경 검증(필요시) + PR/SHA
- [x] QA-017 OSS 종속성 고지와 실제 포함 라이브러리를 일치시킨다 — 증거: PR #31, verify_third_party.py 제3자 고지·stb 이미지 라이선스 정합 및 test_verify_third_party.py, Windows/Ubuntu CI
- [ ] QA-018 저사양 Windows 장치 최소 성능 기준을 충족한다 — 수용: 구현 + 해당 회귀 테스트 + 환경 검증(필요시) + PR/SHA
- [ ] QA-019 Android 중급 기기 최소 성능 기준을 충족한다 — 수용: 구현 + 해당 회귀 테스트 + 환경 검증(필요시) + PR/SHA
- [ ] QA-020 1.0 차단 이슈가 모두 닫혔음을 재검증한다 — 수용: 구현 + 해당 회귀 테스트 + 환경 검증(필요시) + PR/SHA
- [ ] QA-021 각 1.0 작업에 검증 SHA·환경·테스트 증거가 기록된다 — 수용: 구현 + 해당 회귀 테스트 + 환경 검증(필요시) + PR/SHA
- [ ] QA-022 릴리스 태그·최종 CI·배포 파일이 같은 커밋에서 생성된다 — 수용: 구현 + 해당 회귀 테스트 + 환경 검증(필요시) + PR/SHA

## SCN — 씬 고급 로딩·데이터 자산 (0/7)

참조: engine/core; engine/scripting

- [ ] SCN-001 Scene 비동기 로딩을 프레임 정지 없이 수행한다 — 수용: 구현 + 해당 회귀 테스트 + 환경 검증(필요시) + PR/SHA
- [ ] SCN-002 Scene 비동기 로드 실패 시 이전 월드를 안전하게 보존한다 — 수용: 구현 + 해당 회귀 테스트 + 환경 검증(필요시) + PR/SHA
- [ ] SCN-003 Additive Scene 로드와 엔티티 충돌 방지 규칙을 구현한다 — 수용: 구현 + 해당 회귀 테스트 + 환경 검증(필요시) + PR/SHA
- [ ] SCN-004 Additive Scene 단위 unload와 참조 정리를 구현한다 — 수용: 구현 + 해당 회귀 테스트 + 환경 검증(필요시) + PR/SHA
- [ ] SCN-005 Scriptable 직렬화 데이터 에셋 형식을 정의한다 — 수용: 구현 + 해당 회귀 테스트 + 환경 검증(필요시) + PR/SHA
- [ ] SCN-006 Scriptable 데이터 에셋을 C# 스크립트에서 조회·변경한다 — 수용: 구현 + 해당 회귀 테스트 + 환경 검증(필요시) + PR/SHA
- [ ] SCN-007 씬 로드·전환·추가 로드 후 엔티티 참조를 검증한다 — 수용: 구현 + 해당 회귀 테스트 + 환경 검증(필요시) + PR/SHA

## TWOD — 2D 편집·Tilemap (0/7)

참조: engine/render; apps/editor

- [ ] TWOD-001 Tilemap 에셋의 격자·셀·팔레트 직렬화를 구현한다 — 수용: 구현 + 해당 회귀 테스트 + 환경 검증(필요시) + PR/SHA
- [ ] TWOD-002 Tilemap을 Editor Scene 뷰에서 배치·페인팅한다 — 수용: 구현 + 해당 회귀 테스트 + 환경 검증(필요시) + PR/SHA
- [ ] TWOD-003 Tilemap 렌더링을 Sprite 레이어 순서와 일치시킨다 — 수용: 구현 + 해당 회귀 테스트 + 환경 검증(필요시) + PR/SHA
- [ ] TWOD-004 Tilemap Collider를 2D Physics에 연동한다 — 수용: 구현 + 해당 회귀 테스트 + 환경 검증(필요시) + PR/SHA
- [ ] TWOD-005 스프라이트 시트 자동·수동 슬라이싱을 Editor에 제공한다 — 수용: 구현 + 해당 회귀 테스트 + 환경 검증(필요시) + PR/SHA
- [ ] TWOD-006 2D 카메라의 픽셀 퍼펙트 및 픽셀 스냅을 검증한다 — 수용: 구현 + 해당 회귀 테스트 + 환경 검증(필요시) + PR/SHA
- [ ] TWOD-007 2D 프로젝트 시작 템플릿에서 Tilemap과 Sprite를 함께 실행한다 — 수용: 구현 + 해당 회귀 테스트 + 환경 검증(필요시) + PR/SHA

## VFX — 파티클·효과 (0/6)

참조: engine/render; engine/player

- [ ] VFX-001 ParticleEmitter/ParticleSystem 컴포넌트를 Scene 직렬화에 등록한다 — 수용: 구현 + 해당 회귀 테스트 + 환경 검증(필요시) + PR/SHA
- [ ] VFX-002 파티클 생성율·수명·색상·크기를 시간에 따라 평가한다 — 수용: 구현 + 해당 회귀 테스트 + 환경 검증(필요시) + PR/SHA
- [ ] VFX-003 파티클 중력·속도·기본 버스트 방출을 구현한다 — 수용: 구현 + 해당 회귀 테스트 + 환경 검증(필요시) + PR/SHA
- [ ] VFX-004 2D/3D 파티클을 Vulkan 경로로 그린다 — 수용: 구현 + 해당 회귀 테스트 + 환경 검증(필요시) + PR/SHA
- [ ] VFX-005 ParticleSystem Inspector·Play Mode 조절을 구현한다 — 수용: 구현 + 해당 회귀 테스트 + 환경 검증(필요시) + PR/SHA
- [ ] VFX-006 Windows Player에서 다중 파티클 효과를 실제 GPU로 검증한다 — 수용: 구현 + 해당 회귀 테스트 + 환경 검증(필요시) + PR/SHA

## SHDR — 셰이더 도구·재질 (0/6)

참조: engine/render/shaders; tools

- [ ] SHDR-001 HLSL 소스에서 SPIR-V를 생성하는 도구 경로를 구현한다 — 수용: 구현 + 해당 회귀 테스트 + 환경 검증(필요시) + PR/SHA
- [ ] SHDR-002 GLSL/SPIR-V 컴파일러 버전을 고정하고 오류 메시지를 검증한다 — 수용: 구현 + 해당 회귀 테스트 + 환경 검증(필요시) + PR/SHA
- [ ] SHDR-003 사용자 셰이더 에셋 등록과 캐시 키를 구현한다 — 수용: 구현 + 해당 회귀 테스트 + 환경 검증(필요시) + PR/SHA
- [ ] SHDR-004 셰이더-재질 속성 바인딩과 기본 폴백을 검증한다 — 수용: 구현 + 해당 회귀 테스트 + 환경 검증(필요시) + PR/SHA
- [ ] SHDR-005 무효/비정상 셰이더를 불러오면 원인 진단 후 안전하게 유지한다 — 수용: 구현 + 해당 회귀 테스트 + 환경 검증(필요시) + PR/SHA
- [ ] SHDR-006 Shader variant/키워드 변경 후 Player 재질이 동일하다 — 수용: 구현 + 해당 회귀 테스트 + 환경 검증(필요시) + PR/SHA

## EDTOOL — Editor 생산성 도구 (0/9)

참조: apps/editor; engine/editor

- [ ] EDTOOL-001 Scene View와 분리된 Game View를 제공한다 — 수용: 구현 + 해당 회귀 테스트 + 환경 검증(필요시) + PR/SHA
- [ ] EDTOOL-002 Prefab 격리 편집 모드를 구현한다 — 수용: 구현 + 해당 회귀 테스트 + 환경 검증(필요시) + PR/SHA
- [ ] EDTOOL-003 실제 선택 객체와 연결되는 Component Add 메뉴를 완성한다 — 수용: 구현 + 해당 회귀 테스트 + 환경 검증(필요시) + PR/SHA
- [ ] EDTOOL-004 C# 프로젝트를 Visual Studio/Rider로 여는 동작을 검증한다 — 수용: 구현 + 해당 회귀 테스트 + 환경 검증(필요시) + PR/SHA
- [ ] EDTOOL-005 에디터 패키지 목록·설치·제거 UI를 제공한다 — 수용: 구현 + 해당 회귀 테스트 + 환경 검증(필요시) + PR/SHA
- [ ] EDTOOL-006 Editor/Player 실행 성능 Profiler 화면을 제공한다 — 수용: 구현 + 해당 회귀 테스트 + 환경 검증(필요시) + PR/SHA
- [ ] EDTOOL-007 GPU 프레임 디버거 또는 동등한 진단 도구를 제공한다 — 수용: 구현 + 해당 회귀 테스트 + 환경 검증(필요시) + PR/SHA
- [ ] EDTOOL-008 Windows·Android Build Settings Editor 화면을 제공한다 — 수용: 구현 + 해당 회귀 테스트 + 환경 검증(필요시) + PR/SHA
- [ ] EDTOOL-009 Android SDK/NDK/JDK 도구 경로 설정·검증 UI를 제공한다 — 수용: 구현 + 해당 회귀 테스트 + 환경 검증(필요시) + PR/SHA

## BUILD — 빌드 프로필·애플리케이션 메타데이터 (0/9)

참조: apps/player; engine/player; .github/workflows/ci.yml

- [ ] BUILD-001 Windows Debug/Release 빌드 프로필을 프로젝트별로 저장한다 — 수용: 구현 + 해당 회귀 테스트 + 환경 검증(필요시) + PR/SHA
- [ ] BUILD-002 Android Debug/Release 빌드 프로필을 프로젝트별로 저장한다 — 수용: 구현 + 해당 회귀 테스트 + 환경 검증(필요시) + PR/SHA
- [ ] BUILD-003 애플리케이션 이름·버전·식별자를 Windows/Android 번들에 반영한다 — 수용: 구현 + 해당 회귀 테스트 + 환경 검증(필요시) + PR/SHA
- [ ] BUILD-004 Windows 아이콘·메타데이터를 최종 실행 파일에 삽입한다 — 수용: 구현 + 해당 회귀 테스트 + 환경 검증(필요시) + PR/SHA
- [ ] BUILD-005 Android 아이콘·스플래시·패키지 이름을 APK/AAB에 반영한다 — 수용: 구현 + 해당 회귀 테스트 + 환경 검증(필요시) + PR/SHA
- [ ] BUILD-006 Windows 게임 저장 데이터 경로와 마이그레이션 규칙을 구현한다 — 수용: 구현 + 해당 회귀 테스트 + 환경 검증(필요시) + PR/SHA
- [ ] BUILD-007 Android manifest 권한·기능·Gradle 기여와 충돌을 검출한다 — 수용: 구현 + 해당 회귀 테스트 + 환경 검증(필요시) + PR/SHA
- [ ] BUILD-008 빌드 실패·서명 누락 시 사용자에게 명확한 오류를 표시한다 — 수용: 구현 + 해당 회귀 테스트 + 환경 검증(필요시) + PR/SHA
- [ ] BUILD-009 동일 커밋을 사용한 재현 가능한 빌드 결과를 검증한다 — 수용: 구현 + 해당 회귀 테스트 + 환경 검증(필요시) + PR/SHA

## UTIL — 스크립팅 유틸리티·개발 진단 (0/5)

참조: engine/scripting; engine/editor

- [ ] UTIL-001 C# Debug.Log/Warning/Error 메시지와 Editor Console 위치를 연동한다 — 수용: 구현 + 해당 회귀 테스트 + 환경 검증(필요시) + PR/SHA
- [ ] UTIL-002 C# Mathf/Vector/Quaternion 기본 수학 API를 샘플로 검증한다 — 수용: 구현 + 해당 회귀 테스트 + 환경 검증(필요시) + PR/SHA
- [ ] UTIL-003 C# Timer/Coroutine/WaitForSeconds 중단·취소를 검증한다 — 수용: 구현 + 해당 회귀 테스트 + 환경 검증(필요시) + PR/SHA
- [ ] UTIL-004 프로파일 로그·크래시 로그에 엔진 및 게임 빌드 버전을 남긴다 — 수용: 구현 + 해당 회귀 테스트 + 환경 검증(필요시) + PR/SHA
- [ ] UTIL-005 런타임 오류 시 사용자 데이터 보호·복구 안내를 검증한다 — 수용: 구현 + 해당 회귀 테스트 + 환경 검증(필요시) + PR/SHA

## 작업 기록과 변경 이력

- 2026-10-22: PR #30에서 GOV-002, GOV-003 구현 및 CI 자동 검사 연결. 체크 26→28, 미완료 259→257. 최종 CI는 최신 SHA로 재확인.
완료 체크 시 해당 항목 끝에 (PR #, commit SHA, 테스트명, CI run, OS/하드웨어)를 기재한다. 작업 중 발견된 하위 작업은 새 ID를 추가하고 증감 원인을 PR 본문에 남긴다. 기존 완료 체크를 그대로 옮긴 것이 아니며, 기존 286/319 로드맵 체크와 합산하지 않는다.

- 2026-10-22: 구 MASTER_PLAN 목표와 역대조하여 49개 누락 요구를 ID가 있는 작업으로 추가 (SCN/TWOD/VFX/SHDR/EDTOOL/BUILD/UTIL).
- 2026-10-22: 전체 301개 소스/문서/테스트 파일의 내용을 조회·대조한 새 수용 원장 작성. 실기기 수용 항목은 증거 부재로 미체크.

- 2026-10-22: PR #31에서 GOV-004/005/006/008/009, CORE-004/006/008, QA-001/017 수용 검증과 CI 게이트를 추가. 검증 완료 시 28→38개, 미완료 257→247개. 실기기 Windows/Android 실행 수용은 별도 미완료로 유지. 체크 근거는 PR #31 최종 HEAD의 Windows/Ubuntu CI와 테스트.

- 2026-10-23: PR #32를 PR #31의 커밋 위에 스택으로 생성. CORE-009/010/011, INPUT-007/011, UI-004/010, ANIM-004/010, PHYS-009/012, ASSET-004/011/012/014 총 15 ID를 독립된 네 CTest 그룹에서 검증. 성공한 PR HEAD 기준 38→53 완료, 247→232 미완료; 새 ID 없음. Windows·Ubuntu CI 완료 전에는 main 완료로 산입하지 않는다.
