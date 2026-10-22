# NEngine 개발·체크리스트 운영 규칙

**유일한 작업 원장:** [CHECKLIST.md](CHECKLIST.md). 이 문서 및 [FILE_AUDIT.md](FILE_AUDIT.md), [README.md](README.md) 외의 과거 docs 보고서는 현재 상태로 사용하지 않는다. 이 규칙은 개발자와 새 채팅 세션 모두 동일하게 적용된다.

## 1. 새 세션의 시작 (코드가 최종 권위)

1. GitHub `skatmdgn/NEngine`의 **실시간 main HEAD**와 열린 PR, 최신 성공/실패 CI run을 확인한다.
2. `docs/CHECKLIST.md`를 읽고 ID별 미완료 항목과 의존성을 확인한다. 문서는 코드의 증거가 아니다.
3. 해당 ID와 연관된 `engine/`, `apps/`, `tests/`, `tools/` 실제 파일을 확인한다. `FILE_AUDIT.md`는 베이스라인 목록이지 영구적인 최신 상태가 아니다.
4. 아직 병합되지 않은 PR의 코드와 main 코드를 혼동하지 않는다. 중복 개발을 피하도록 open PR 내용을 별도 확인한다.
5. 시작 SHA와 목표 ID 목록을 PR·커밋 기록에 명시한다.

## 2. 작업 1개의 정의와 분할

**작업 1개 = 하나의 재현 가능한 수용 조건을 실제로 충족하고 증거를 남긴 체크리스트 ID 1개.** 커밋·파일·수정 줄 수·독립적인 작은 assert 30개는 30개의 완료 작업으로 세지 않는다.

- 미완료 `- [ ] ID` 중 요구·실행 경로·성공 조건이 분명한 것을 선택한다.
- 거대한 항목은 구현 전에 `원본-ID-A`, `원본-ID-B`처럼 **명시적으로 재분해**하고, 원본을 동시에 완료로 세지 않는다. 마이그레이션 사유·날짜를 기록한다.
- 발견된 새 결함·누락 기능은 영속적 새 ID로 추가한다. 삭제·이름 변경·재사용 금지. 중복은 기존 ID에 연결하고 합산하지 않는다.
- 사용자가 '30덩어리'라고 하면 서로 다른 체크 ID **30개 수용 완료를 목표로** 하지만, 실제 완료한 ID 수만 보고한다. 실패/미완료를 성공에 포함하지 않는다. 수용조건 없이 개수 맞추는 작은 단위로 쪼개지 않는다.
- 체크리스트 변경 전과 후의 ID별 집계를 비교하고 신규/완료/재오픈/분할을 구분해 보고한다.

## 3. 체크 표시의 완료 근거

1. 소스 변경·테스트 내용·관련 파일이 HEAD에서 존재한다.
2. 요구사항의 부정/경계 입력을 포함한 회귀 테스트가 있고 실제 PASS 로그가 있다.
3. **자동 검증** 항목은 해당 정확한 SHA에 대한 Windows/Ubuntu CI·CTest 결과를 기록한다. 다른 SHA의 CI를 근거로 사용하지 않는다.
4. **Windows GPU/오디오/.NET 실사용** 항목은 사용자 또는 검증자가 실제 장치·OS·드라이버/런타임·재현 절차·결과를 제시해야 완료한다.
5. **Android** 항목은 SDK/NDK, APK/AAB 산출물뿐 아니라 명시한 단말·ABI·OS에서의 실행 증거가 있어야 완료한다.
6. 문서·배포 품질 항목은 산출물·링크가 검증되어야 한다.
7. 해당 증거가 부족하거나 CI가 진행 중이라면 `[ ]`를 유지한다. '구현함', '테스트 작성', 'PR 생성'은 수용 완료와 다르다.

체크할 때 예시:

`- [x] CORE-012 ... — 증거: PR #41, abcdef..., NEngineCoreTests PASS, Ubuntu/Windows run 123..., 환경 N/A`

실기기 검증은 OS 빌드, GPU, 드라이버, .NET 버전, Android 단말/OS를 남긴다. 실패 결과를 통과로 서술하지 않는다. 본 원장의 기준선 체크는 좁은 **소스/자동 테스트 범위**만 인정되며 1.0 배포 가능의 증거가 아니다.

## 4. 브랜치/커밋/최종 CI

- 항상 최신 main 기반의 별도 개발 브랜치에서 작업한다. main 직접 변경하지 않는다.
- 서로 독립적인 기능은 의미 있는 커밋으로 저장하고 검증 가능한 중간 상태를 만든다. 실제 코드의 디버그/빌드 검증은 가능한 즉시 수행한다.
- **GitHub Actions 전체 CI는 최종 PR을 열 때만 실행하는 것을 목표로 한다.** 브랜치를 main으로 푸시하거나 개발 중 여러 PR을 만들지 않는다.
- PR 본문에 (a) 시작 main SHA, (b) 완료 ID와 근거, (c) 새로 추가된 ID, (d) 테스트/미검증 항목, (e) 삭제·호환성 영향을 기록한다.
- 최종 head SHA에 연결된 Windows/Ubuntu CI가 모두 PASS인 것을 확인한다. 마지막 수정 후 이전 통과 로그 재사용 금지.
- CI 실패 시 체크를 되돌리거나 수정→재실행하여 같은 SHA 기준 검증한다.
- **PR 병합은 명시된 지시 또는 프로젝트의 승인된 병합 절차에 의해서만 수행한다.** PR 생성·CI 통과가 병합을 의미하지 않는다.
- 병합 후 main HEAD와 테스트 상태를 다시 확인하고, 새 세션에서 그 HEAD를 기준으로 시작한다.

## 5. 진행률·작업량 보고

- 보고 형식: `시작 ID n개 / 실제 수용 PASS x개 / 계속 미완료 y개 / 새로 발견 z개 / CI 확인 여부 / main 또는 PR HEAD`.
- **체크박스 완료율은 기능 완성도 %, 남은 공수 또는 개발 속도 %로 표현하지 않는다.** 프로젝트 1.0 완성도를 퍼센트로 요구하면 기능 난이도와 실제 Windows·Android 수용 여부를 별도로 설명한다.
- 범위에 신규 요구가 추가되면 전체 체크 수와 미완료 수가 증가할 수 있다. 그 이유·연결 PR을 기록해야 하므로 숫자가 이유 없이 제자리걸음하는 상황을 방지한다.
- CI에 성공했더라도 모든 플랫폼 실제 수용이 끝났다고 단언하지 않는다.

## 6. 변경 및 문서 정리 안전장치

- `docs` 정식 파일은 README, CHECKLIST, WORKFLOW, FILE_AUDIT로 제한한다.
- 옛 `docs/ROADMAP.md`, `CURRENT_STATE.md` 등은 이 PR에서 제거하되 **Git 이력에 보존**한다. 기존 체크 숫자를 새 체크에 기계적으로 계승하지 않는다.
- 루트 `README.md`는 이 네 파일만 가리키도록 고친다.
- **PR #29는 독립된 미병합 Navigation 작업이다.** 새 체크리스트 체계가 main에 병합된 후 #29를 통합할 때 구버전 docs 변경을 되살리지 말고 NAV 작업의 코드·테스트와 근거만 이관한다.
- `THIRD_PARTY_NOTICES.md`, `third_party/stb/LICENSE`, 빌드·테스트·소스는 문서 정리의 삭제 대상이 아니다.

## 7. 빌드/개발 검증 명령

필수 CMake 3.25+, C++20 빌드 도구. 실제 Windows 10/11 Editor/Player, Vulkan 가능한 GPU/드라이버와 관리형 실행용 적합한 .NET SDK/hostfxr는 별도 수용 전제다. Android toolchain은 아직 구현·배포 검증 대상이다.

```bash
cmake -S . -B build -DNENGINE_BUILD_TESTS=ON -DNENGINE_BUILD_EDITOR=ON -DNENGINE_BUILD_PLAYER=ON
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

Windows Player 내보내기 예시 (먼저 Editor에서 NEngine.nproject 포함 프로젝트 생성):

```powershell
.\build\apps\player\Release\NEnginePlayerBuild.exe "C:\Games\MyNEngineProject" "C:\Exports\MyGame"
```

출력 위치는 프로젝트 외부의 사용하지 않은 폴더. 실제 C# 코드 실행은 적합한 .NET 런타임과 빌드된 게임 DLL이 필요하다. CI의 Player CLI smoke가 이를 모두 대체하지 않는다.
