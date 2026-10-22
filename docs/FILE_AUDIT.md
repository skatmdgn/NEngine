# NEngine 파일 전수 목록과 기준선 조사

기준 커밋: `671cddf2d50d184b1889310f86c85a3df43306bd` (2026-10-22). GitHub Git tree는 **374개 항목**, **301개 파일**, **73개 디렉터리**이며 truncated=false였다. 이 목록의 파일 301개를 GitHub 파일 콘텐츠 API로 각각 읽어 본 뒤 경로·크기·역할을 대조했다. 하단 개별 SHA는 기준선의 Git blob 식별자이며 최신 main의 영구 상태는 아니다.

**검사 범위 구분:** `읽기 확인` = 파일 전체 UTF-8 내용 API 조회 성공 + 역할/경로 대조. 이것만으로 모든 코드 라인의 논리적 정당성·취약점 전수 심층 감사·하드웨어 실행을 보증하지 않는다. **자동 검증** = [main CI run 39713209272](https://github.com/skatmdgn/NEngine/actions/runs/39713209272)에서 Ubuntu·Windows 각 **12/12 CTest 그룹 PASS** (소스 단위). **실제 Windows GPU/오디오/.NET Player와 Android 실기기 수용은 미검증**이다. 완료 여부는 [CHECKLIST.md](CHECKLIST.md)의 각 ID로만 계산한다.

## 기능-파일-자동 검증 대응

| 영역 | 구현 위치 | 테스트 | 주의사항 |
|---|---|---|---|
| Core/Scene/Prefab | engine/core | tests/core_tests.cpp | 실제 게임 왕복 수용 추가 필요 |
| Windows Editor | apps/editor, engine/editor | tests/editor_tests.cpp | GDI 기본 Scene View, UI 하드웨어 수용 미완 |
| Asset import | engine/assets, engine/render/src | tests/asset_tests.cpp | FBX/WebP/완전한 모델 패키징 미완 |
| Vulkan/Animation | engine/render | tests/render_tests.cpp | GPU 실기기 PBR/그림자/Editor 통합 미완 |
| C# managed | engine/scripting | tests/scripting_tests.cpp | VS attach/플러그인/배포 검증 미완 |
| Physics | engine/physics | tests/physics_tests.cpp | Cast 확장/성능/실기기 검증 미완 |
| Input | engine/input | tests/input_tests.cpp | 게임패드/터치/Editor Action UI 미완 |
| Audio | engine/audio | tests/audio_tests.cpp | 장치 실사용/스트리밍 디코딩 미완 |
| UI | engine/ui, engine/player | tests/ui_tests.cpp | 한글/복잡 셰이핑/실 GPU 미완 |
| NavMesh | engine/navigation | tests/navigation_tests.cpp | #29 베이크 미병합; Scene/managed agent 미완 |
| Windows Player | engine/player, apps/player | tests/player_tests.cpp, player_cli_smoke.cmake | 실제 Windows 번들 전체 게임 미검증 |
| Android | (완성된 앱/빌드 타깃 없음) | (없음) | SDK/NDK/APK/AAB부터 시작 |
| Plugins | (통합 패키지 타깃 없음) | (없음) | NuGet, DLL, AAR/JAR/SO 등 미완 |

### 당시 브랜치 및 정책 기록

- main `671cddf2d50d184b1889310f86c85a3df43306bd`: PR #28 병합. 열린 PR #29은 `feat/navigation-triangle-bake-30-20261022`, Windows/Ubuntu PR CI run [39715038070](https://github.com/skatmdgn/NEngine/actions/runs/39715038070)에서 13/13 PASS, **main 미병합**이므로 원장 NAV-003 미완료.
- 루트 `README.md`는 새 docs 링크로 교체한다. 코드·테스트·서드파티 고지·CI는 이 문서 정리에서 삭제하지 않는다.
- 기존 docs 13개 파일은 Git 커밋 이력으로 복구할 수 있지만 새 세션에서 현재 상태 문서로 사용하지 않는다.

## 기준선 파일 인벤토리

각 파일은 기준선의 경로 및 blob SHA 접두부(12자리). 각 줄의 존재/읽기 확인은 해당 기능이 완료되었다는 의미가 아니다.

### 프로젝트·CI·루트 — 5개

- `.github/workflows/ci.yml` — 2298 B / `6ed2e41bb6eb`
- `.gitignore` — 123 B / `f7f39d65242b`
- `CMakeLists.txt` — 1117 B / `5748922f0459`
- `README.md` — 6955 B / `67bc31fa824d`
- `THIRD_PARTY_NOTICES.md` — 668 B / `f06436173a08`

### Windows Editor/Player 진입점 — 9개

- `apps/editor/CMakeLists.txt` — 566 B / `24f0f90afd1c`
- `apps/editor/main.cpp` — 1520 B / `bf61d4c7110a`
- `apps/editor/win32_editor_shell.cpp` — 157378 B / `d609d3cd81b3`
- `apps/editor/win32_editor_shell.hpp` — 522 B / `275ba1fd647d`
- `apps/editor/winmain_win32.cpp` — 9063 B / `e5a42251f016`
- `apps/player/CMakeLists.txt` — 735 B / `fa20b34f586a`
- `apps/player/export_main.cpp` — 2765 B / `e2d59dc52014`
- `apps/player/main.cpp` — 945 B / `3d73609b8491`
- `apps/player/winmain_win32.cpp` — 9920 B / `d7808ae8a7db`

### 기존 문서 (교체 대상) — 13개

- `docs/API_SPEC.md` — 3751 B / `b0daf2fff019`
- `docs/ARCHITECTURE.md` — 2605 B / `e84a00b50dd8`
- `docs/AUDIT_FIXES_2026_10.md` — 7344 B / `eedc5a2de829`
- `docs/BATCH_DEVELOPMENT.md` — 7730 B / `397b7b957b82`
- `docs/BUILD.md` — 5055 B / `98d301cebbb3`
- `docs/CURRENT_STATE.md` — 22159 B / `56a10a32e203`
- `docs/DECISIONS.md` — 1757 B / `e66a6d0dc5fd`
- `docs/DEPENDENCIES.md` — 1309 B / `5f4c3acad3ab`
- `docs/DEVELOPMENT_WORKFLOW.md` — 3194 B / `173281666783`
- `docs/MASTER_PLAN.md` — 10741 B / `5d9b9ac0a22d`
- `docs/NAVIGATION_FOUNDATION.md` — 3051 B / `69ffdea0111f`
- `docs/ROADMAP.md` — 26183 B / `faffd8a919bf`
- `docs/history/CURRENT_STATE_THROUGH_2026-10-16.md` — 45752 B / `f6f60299a7ab`

### Asset — 20개

- `engine/assets/CMakeLists.txt` — 572 B / `52f60a0e5681`
- `engine/assets/include/nengine/assets/asset_database.hpp` — 2642 B / `32289460d795`
- `engine/assets/include/nengine/assets/asset_guid.hpp` — 987 B / `277f93f4298d`
- `engine/assets/include/nengine/assets/asset_importer.hpp` — 805 B / `648ea9f91145`
- `engine/assets/include/nengine/assets/bounded_input.hpp` — 895 B / `bd710145a511`
- `engine/assets/include/nengine/assets/builtin_processors.hpp` — 487 B / `f250a816c233`
- `engine/assets/include/nengine/assets/dependency_graph.hpp` — 702 B / `b53b62202c1e`
- `engine/assets/include/nengine/assets/file_watcher.hpp` — 1133 B / `a855f38ef175`
- `engine/assets/include/nengine/assets/gltf_sidecars.hpp` — 1419 B / `489be7fde35c`
- `engine/assets/include/nengine/assets/import_pipeline.hpp` — 2451 B / `c5b3d0312785`
- `engine/assets/include/nengine/assets/obj_sidecars.hpp` — 1457 B / `0be44b8b3ef8`
- `engine/assets/src/asset_database.cpp` — 10470 B / `fe4346c2b2ac`
- `engine/assets/src/asset_guid.cpp` — 2972 B / `7c6dca322d97`
- `engine/assets/src/asset_importer.cpp` — 2275 B / `4bb5d5ea8253`
- `engine/assets/src/builtin_processors.cpp` — 27162 B / `536467edd55e`
- `engine/assets/src/dependency_graph.cpp` — 2391 B / `c01a202e967e`
- `engine/assets/src/file_watcher.cpp` — 3047 B / `afe3fbbbf541`
- `engine/assets/src/gltf_sidecars.cpp` — 17332 B / `fa2ce70420d0`
- `engine/assets/src/import_pipeline.cpp` — 22841 B / `bea0516b4530`
- `engine/assets/src/obj_sidecars.cpp` — 14780 B / `1eed011ceb62`

### Audio — 17개

- `engine/audio/CMakeLists.txt` — 868 B / `4c31887d3d86`
- `engine/audio/include/nengine/audio/audio_clip.hpp` — 1113 B / `ba42c2917788`
- `engine/audio/include/nengine/audio/audio_device.hpp` — 1211 B / `482490495029`
- `engine/audio/include/nengine/audio/audio_renderer.hpp` — 786 B / `b84400028ffd`
- `engine/audio/include/nengine/audio/clip_cache.hpp` — 760 B / `7cce1a925d67`
- `engine/audio/include/nengine/audio/components.hpp` — 912 B / `a5153e8dac35`
- `engine/audio/include/nengine/audio/mix_snapshot.hpp` — 976 B / `e590724b6793`
- `engine/audio/include/nengine/audio/playback.hpp` — 964 B / `1fb0acabf24a`
- `engine/audio/include/nengine/audio/registration.hpp` — 338 B / `0049f9afff77`
- `engine/audio/src/audio_clip.cpp` — 12528 B / `4ff63b2ee5fa`
- `engine/audio/src/audio_device_null.cpp` — 2493 B / `225494cadf70`
- `engine/audio/src/audio_device_win32.cpp` — 14768 B / `04bf441c46ec`
- `engine/audio/src/audio_renderer.cpp` — 5376 B / `1c951034f454`
- `engine/audio/src/clip_cache.cpp` — 3368 B / `fe40a392648a`
- `engine/audio/src/mix_snapshot.cpp` — 8385 B / `8089cabd87d7`
- `engine/audio/src/playback.cpp` — 3697 B / `ff8012bae359`
- `engine/audio/src/registration.cpp` — 12566 B / `723cc0895b3d`

### Core — 16개

- `engine/core/CMakeLists.txt` — 451 B / `86faf04dfecf`
- `engine/core/include/nengine/core/component_registry.hpp` — 2140 B / `388620770139`
- `engine/core/include/nengine/core/component_serialization.hpp` — 1731 B / `cc128ba3c40a`
- `engine/core/include/nengine/core/component_storage.hpp` — 2182 B / `2b1c296af0cd`
- `engine/core/include/nengine/core/entity.hpp` — 993 B / `9fcd37a02a87`
- `engine/core/include/nengine/core/math.hpp` — 520 B / `c90f503ac793`
- `engine/core/include/nengine/core/prefab.hpp` — 1269 B / `2bf443813b6e`
- `engine/core/include/nengine/core/property_value.hpp` — 362 B / `5bc611a3959e`
- `engine/core/include/nengine/core/scene.hpp` — 1964 B / `5b8bdcb2d757`
- `engine/core/include/nengine/core/transform.hpp` — 294 B / `6e978ab489d9`
- `engine/core/include/nengine/core/world.hpp` — 4512 B / `56d03c743fb3`
- `engine/core/src/component_registry.cpp` — 2716 B / `c8fa7ba1e5b8`
- `engine/core/src/component_serialization.cpp` — 2016 B / `4e27baca7385`
- `engine/core/src/prefab.cpp` — 4572 B / `bb35e66e0121`
- `engine/core/src/scene.cpp` — 28918 B / `0df270ea3dc5`
- `engine/core/src/world.cpp` — 7036 B / `0f3443889a9a`

### Editor Model — 36개

- `engine/editor/CMakeLists.txt` — 989 B / `b1648abd22f4`
- `engine/editor/include/nengine/editor/audio_integration.hpp` — 525 B / `df08a0565a37`
- `engine/editor/include/nengine/editor/command.hpp` — 4657 B / `a47a3f0fb5b4`
- `engine/editor/include/nengine/editor/component_factory.hpp` — 924 B / `2be305c5697c`
- `engine/editor/include/nengine/editor/console_model.hpp` — 1738 B / `0cf4a0e1a8c2`
- `engine/editor/include/nengine/editor/editor_layout.hpp` — 625 B / `7ac76d9f95ad`
- `engine/editor/include/nengine/editor/editor_model.hpp` — 13694 B / `cf6deca2fd36`
- `engine/editor/include/nengine/editor/physics_integration.hpp` — 529 B / `95cf69ac1dfc`
- `engine/editor/include/nengine/editor/play_session.hpp` — 1254 B / `70d72ca32280`
- `engine/editor/include/nengine/editor/presentation.hpp` — 1857 B / `c24ad85bb9f0`
- `engine/editor/include/nengine/editor/project_manifest.hpp` — 957 B / `43beddfd7e6f`
- `engine/editor/include/nengine/editor/project_session.hpp` — 3879 B / `f2cf1db4a30d`
- `engine/editor/include/nengine/editor/property_access.hpp` — 1832 B / `a984ca2e1e2f`
- `engine/editor/include/nengine/editor/property_command.hpp` — 1203 B / `6c0aa3fc36a3`
- `engine/editor/include/nengine/editor/property_text.hpp` — 433 B / `936f82a1dccc`
- `engine/editor/include/nengine/editor/render_integration.hpp` — 527 B / `5d3e05a50d3f`
- `engine/editor/include/nengine/editor/scene_interaction.hpp` — 1599 B / `8813c99e58ab`
- `engine/editor/include/nengine/editor/scripting_integration.hpp` — 533 B / `44cdf71cd917`
- `engine/editor/include/nengine/editor/selection.hpp` — 1692 B / `e4adae84a80f`
- `engine/editor/src/audio_integration.cpp` — 17425 B / `af0990f6ad62`
- `engine/editor/src/command.cpp` — 4799 B / `1c3ceb629bd6`
- `engine/editor/src/component_factory.cpp` — 1575 B / `b6bdba552635`
- `engine/editor/src/console_model.cpp` — 2205 B / `14f721d4e57e`
- `engine/editor/src/editor_layout.cpp` — 3530 B / `385560fa0492`
- `engine/editor/src/editor_model.cpp` — 51872 B / `b5bfa2646cba`
- `engine/editor/src/physics_integration.cpp` — 47064 B / `c6189d290045`
- `engine/editor/src/play_session.cpp` — 2786 B / `e541ab33bc75`
- `engine/editor/src/presentation.cpp` — 5830 B / `fce1331a12b6`
- `engine/editor/src/project_manifest.cpp` — 6119 B / `99d5d5a1633c`
- `engine/editor/src/project_session.cpp` — 23827 B / `b9aaadc0c0ef`
- `engine/editor/src/property_access.cpp` — 2262 B / `381da94c596d`
- `engine/editor/src/property_command.cpp` — 936 B / `ba25b7db0f71`
- `engine/editor/src/property_text.cpp` — 7099 B / `f180cffd1aa7`
- `engine/editor/src/render_integration.cpp` — 41777 B / `4069d8965b0d`
- `engine/editor/src/scene_interaction.cpp` — 4134 B / `4d1a01f89236`
- `engine/editor/src/scripting_integration.cpp` — 5681 B / `11795730931a`

### Input — 5개

- `engine/input/CMakeLists.txt` — 390 B / `ccf0003eb4d9`
- `engine/input/include/nengine/input/action_map_io.hpp` — 624 B / `b091d862326b`
- `engine/input/include/nengine/input/input_state.hpp` — 3106 B / `c7efe94b1785`
- `engine/input/src/action_map_io.cpp` — 3371 B / `b0c1fc65663c`
- `engine/input/src/input_state.cpp` — 12508 B / `de166d50b4e6`

### Navigation — 6개

- `engine/navigation/CMakeLists.txt` — 505 B / `526b4228def1`
- `engine/navigation/include/nengine/navigation/nav_mesh.hpp` — 4329 B / `482c4520df8a`
- `engine/navigation/src/nav_agent.cpp` — 2888 B / `b98d71952539`
- `engine/navigation/src/nav_mesh.cpp` — 7419 B / `bc0fb9405374`
- `engine/navigation/src/nav_path.cpp` — 6021 B / `3612489cc26f`
- `engine/navigation/src/nav_serialization.cpp` — 6888 B / `da1666c159ba`

### Physics — 12개

- `engine/physics/CMakeLists.txt` — 529 B / `edeacf797a36`
- `engine/physics/include/nengine/physics/collision.hpp` — 4044 B / `37d0d0085372`
- `engine/physics/include/nengine/physics/components.hpp` — 3726 B / `f5b150cc8b60`
- `engine/physics/include/nengine/physics/contact_events.hpp` — 1433 B / `ffcfab2d25e9`
- `engine/physics/include/nengine/physics/mass_properties.hpp` — 7690 B / `6c0e1bc0e792`
- `engine/physics/include/nengine/physics/registration.hpp` — 342 B / `a3a58cd821c6`
- `engine/physics/include/nengine/physics/simulation.hpp` — 824 B / `fe42fc32c619`
- `engine/physics/src/collision.cpp` — 169436 B / `73f63841f5be`
- `engine/physics/src/components.cpp` — 249 B / `f4fccdbd85b5`
- `engine/physics/src/contact_events.cpp` — 3673 B / `194cc4074c71`
- `engine/physics/src/registration.cpp` — 34151 B / `c2a3eb49f5c4`
- `engine/physics/src/simulation.cpp` — 13411 B / `ac90b6535089`

### Platform — 4개

- `engine/platform/CMakeLists.txt` — 725 B / `d4ffbb48ee04`
- `engine/platform/include/nengine/platform/window.hpp` — 1011 B / `43c1f2e0571c`
- `engine/platform/src/window_stub.cpp` — 1374 B / `dfb8bf9dace9`
- `engine/platform/src/window_win32.cpp` — 9557 B / `5945856380ce`

### Player Kit — 23개

- `engine/player/CMakeLists.txt` — 635 B / `d8141183b5da`
- `engine/player/include/nengine/player/assets.hpp` — 1858 B / `f75e178efc94`
- `engine/player/include/nengine/player/bundle.hpp` — 1594 B / `35fb0b06b40e`
- `engine/player/include/nengine/player/exporter.hpp` — 1322 B / `ef8614fa3f5d`
- `engine/player/include/nengine/player/managed_properties.hpp` — 614 B / `4d91ed03d178`
- `engine/player/include/nengine/player/managed_queries.hpp` — 242 B / `9a2eb66db529`
- `engine/player/include/nengine/player/runtime.hpp` — 5511 B / `c406bad0a34c`
- `engine/player/include/nengine/player/subassets.hpp` — 847 B / `bbe2658654af`
- `engine/player/include/nengine/player/ui_runtime.hpp` — 2291 B / `bd1f8f44ff60`
- `engine/player/src/assets.cpp` — 13111 B / `8573b2a51924`
- `engine/player/src/bundle.cpp` — 6550 B / `21edcd4292eb`
- `engine/player/src/exporter.cpp` — 17699 B / `cbf08700a6f7`
- `engine/player/src/managed_audio_properties.cpp` — 6046 B / `a9c3b49e9411`
- `engine/player/src/managed_audio_properties.hpp` — 517 B / `9d043a29292d`
- `engine/player/src/managed_collider_properties.cpp` — 10517 B / `7b56b394057d`
- `engine/player/src/managed_collider_properties.hpp` — 523 B / `159efeb7efb2`
- `engine/player/src/managed_properties.cpp` — 8455 B / `3951926a1d56`
- `engine/player/src/managed_queries.cpp` — 2831 B / `bd3694c89a5b`
- `engine/player/src/managed_render_properties.cpp` — 12940 B / `c3a6e77a6c08`
- `engine/player/src/managed_render_properties.hpp` — 519 B / `12d667e9dc93`
- `engine/player/src/runtime.cpp` — 20306 B / `fc88e2fa3c6e`
- `engine/player/src/subassets.cpp` — 7747 B / `b3999f9ba9e8`
- `engine/player/src/ui_runtime.cpp` — 8299 B / `7c888bfe98c0`

### Render/Vulkan/Animation — 88개

- `engine/render/CMakeLists.txt` — 1764 B / `604724d2b45f`
- `engine/render/include/nengine/render/animation_controller.hpp` — 2926 B / `abe7bdc6c5e7`
- `engine/render/include/nengine/render/animation_controller_asset.hpp` — 1083 B / `e68339e0a389`
- `engine/render/include/nengine/render/asset_resources.hpp` — 2243 B / `ef2adac74583`
- `engine/render/include/nengine/render/builtin_assets.hpp` — 631 B / `90e6e14c0447`
- `engine/render/include/nengine/render/components.hpp` — 2457 B / `7ddf39bc4317`
- `engine/render/include/nengine/render/decoded_mesh.hpp` — 898 B / `1c83fce51640`
- `engine/render/include/nengine/render/decoded_texture.hpp` — 1762 B / `adfcec1d7a42`
- `engine/render/include/nengine/render/diagnostic_shaders.hpp` — 382 B / `f7a2592e05de`
- `engine/render/include/nengine/render/gltf_mesh.hpp` — 3063 B / `d1af1363b8bc`
- `engine/render/include/nengine/render/material_asset.hpp` — 1770 B / `0a091a03fe4d`
- `engine/render/include/nengine/render/matrix.hpp` — 1695 B / `53c1b072e3f5`
- `engine/render/include/nengine/render/mesh_data.hpp` — 1457 B / `49e7af4d7eac`
- `engine/render/include/nengine/render/model_importer.hpp` — 1084 B / `fe8e022d6821`
- `engine/render/include/nengine/render/obj_material.hpp` — 912 B / `8c215191b120`
- `engine/render/include/nengine/render/obj_mesh.hpp` — 1208 B / `29b83cb618f8`
- `engine/render/include/nengine/render/registration.hpp` — 340 B / `e6d8f9ffb221`
- `engine/render/include/nengine/render/render_snapshot.hpp` — 1125 B / `4895f04d0523`
- `engine/render/include/nengine/render/rhi.hpp` — 3149 B / `56e992f16eeb`
- `engine/render/include/nengine/render/sprite_animation.hpp` — 2180 B / `e0358621b9b4`
- `engine/render/include/nengine/render/vulkan_buffer.hpp` — 2304 B / `9e7409777368`
- `engine/render/include/nengine/render/vulkan_builtin_mesh_cache.hpp` — 1136 B / `40bee14f79d4`
- `engine/render/include/nengine/render/vulkan_context.hpp` — 2800 B / `b548bd2577ba`
- `engine/render/include/nengine/render/vulkan_depth_target.hpp` — 1562 B / `33a61e165183`
- `engine/render/include/nengine/render/vulkan_device.hpp` — 2243 B / `deb3cecd0c3d`
- `engine/render/include/nengine/render/vulkan_diagnostic_scene.hpp` — 3919 B / `350de7a0aa8b`
- `engine/render/include/nengine/render/vulkan_instance.hpp` — 1441 B / `d4b521306601`
- `engine/render/include/nengine/render/vulkan_loader.hpp` — 1101 B / `402771e009b1`
- `engine/render/include/nengine/render/vulkan_material.hpp` — 1635 B / `4c96b40b06bd`
- `engine/render/include/nengine/render/vulkan_material_asset_cache.hpp` — 4168 B / `8b27befeed6b`
- `engine/render/include/nengine/render/vulkan_mesh.hpp` — 1441 B / `34ca11123226`
- `engine/render/include/nengine/render/vulkan_mesh_asset_cache.hpp` — 2259 B / `7f05a68e4786`
- `engine/render/include/nengine/render/vulkan_pipeline.hpp` — 2937 B / `ff0e02c84496`
- `engine/render/include/nengine/render/vulkan_presenter.hpp` — 2942 B / `3ecdf43e60b8`
- `engine/render/include/nengine/render/vulkan_render_pass.hpp` — 1529 B / `d5b0414a670a`
- `engine/render/include/nengine/render/vulkan_render_targets.hpp` — 2023 B / `593848df1779`
- `engine/render/include/nengine/render/vulkan_shader.hpp` — 1348 B / `739ab3a08e76`
- `engine/render/include/nengine/render/vulkan_surface.hpp` — 1264 B / `e54d2123cc34`
- `engine/render/include/nengine/render/vulkan_swapchain.hpp` — 1922 B / `e67e9201e621`
- `engine/render/include/nengine/render/vulkan_texture.hpp` — 2295 B / `38292218119f`
- `engine/render/include/nengine/render/vulkan_texture_asset_cache.hpp` — 2623 B / `cc1917467ec9`
- `engine/render/shaders/diagnostic.frag.glsl` — 120 B / `a2ba7681f733`
- `engine/render/shaders/diagnostic.vert.glsl` — 205 B / `dc2342469c62`
- `engine/render/shaders/diagnostic_textured.frag.glsl` — 222 B / `3c731adfce4d`
- `engine/render/shaders/diagnostic_textured.vert.glsl` — 296 B / `b26a2d4ede7e`
- `engine/render/shaders/pbr.frag.glsl` — 6687 B / `346561c66b51`
- `engine/render/shaders/pbr.vert.glsl` — 768 B / `0e81b2458c0e`
- `engine/render/src/animation_controller.cpp` — 9459 B / `565354a91361`
- `engine/render/src/animation_controller_asset.cpp` — 3227 B / `61bba3930d49`
- `engine/render/src/asset_resources.cpp` — 11122 B / `03cb7814791c`
- `engine/render/src/builtin_assets.cpp` — 608 B / `6101f50e11a1`
- `engine/render/src/decoded_mesh.cpp` — 3298 B / `0848dad8c52c`
- `engine/render/src/decoded_texture.cpp` — 16534 B / `3f127f95f023`
- `engine/render/src/diagnostic_shaders.cpp` — 8816 B / `04016e1c1d85`
- `engine/render/src/gltf_mesh.cpp` — 114008 B / `369bc61164de`
- `engine/render/src/material_asset.cpp` — 8687 B / `b65ace7f47b5`
- `engine/render/src/matrix.cpp` — 10869 B / `da4fce5a9ced`
- `engine/render/src/mesh_data.cpp` — 2826 B / `1fcaba50341b`
- `engine/render/src/model_importer.cpp` — 32518 B / `02afcbc2e1de`
- `engine/render/src/obj_material.cpp` — 9829 B / `f9ea39224280`
- `engine/render/src/obj_mesh.cpp` — 24136 B / `a02d6a51d7af`
- `engine/render/src/registration.cpp` — 30919 B / `2d9dc8513cc6`
- `engine/render/src/render_snapshot.cpp` — 2178 B / `a664a726480e`
- `engine/render/src/sprite_animation.cpp` — 12286 B / `6f9b79ee8ab4`
- `engine/render/src/stb_image_impl.cpp` — 144 B / `d8f1f35fd2ce`
- `engine/render/src/vulkan_buffer.cpp` — 23232 B / `62b6601fbb0b`
- `engine/render/src/vulkan_builtin_mesh_cache.cpp` — 1888 B / `020ab2b8303e`
- `engine/render/src/vulkan_context.cpp` — 5713 B / `79bd871904f9`
- `engine/render/src/vulkan_depth_target.cpp` — 15735 B / `a7c3bd93e7b7`
- `engine/render/src/vulkan_device.cpp` — 15784 B / `36b936021fe2`
- `engine/render/src/vulkan_diagnostic_scene.cpp` — 30705 B / `b32335e6ba24`
- `engine/render/src/vulkan_instance.cpp` — 8698 B / `665700f89108`
- `engine/render/src/vulkan_loader_posix.cpp` — 2374 B / `8de45e14a69e`
- `engine/render/src/vulkan_loader_win32.cpp` — 2465 B / `4d0dcac46361`
- `engine/render/src/vulkan_material.cpp` — 12516 B / `76a21036b4aa`
- `engine/render/src/vulkan_material_asset_cache.cpp` — 15149 B / `0e8bc47263bf`
- `engine/render/src/vulkan_mesh.cpp` — 2335 B / `b7ee97a5dd20`
- `engine/render/src/vulkan_mesh_asset_cache.cpp` — 4337 B / `fbe9ec1623db`
- `engine/render/src/vulkan_pipeline.cpp` — 20083 B / `6f5cd0478ebb`
- `engine/render/src/vulkan_presenter.cpp` — 30596 B / `768bc9517db4`
- `engine/render/src/vulkan_render_pass.cpp` — 8136 B / `09b2aa12acec`
- `engine/render/src/vulkan_render_targets.cpp` — 7532 B / `a62bad9cfb9e`
- `engine/render/src/vulkan_shader.cpp` — 4413 B / `0882378bac7b`
- `engine/render/src/vulkan_surface_stub.cpp` — 1593 B / `c584c72f0d35`
- `engine/render/src/vulkan_surface_win32.cpp` — 4035 B / `d2de96d9f548`
- `engine/render/src/vulkan_swapchain.cpp` — 14935 B / `41f25ae1c0ec`
- `engine/render/src/vulkan_texture.cpp` — 37655 B / `735d874fe6db`
- `engine/render/src/vulkan_texture_asset_cache.cpp` — 5138 B / `e1a24cef5a0a`

### Managed Scripting — 14개

- `engine/scripting/CMakeLists.txt` — 666 B / `98a4d3c12c5c`
- `engine/scripting/include/nengine/scripting/components.hpp` — 379 B / `77c2353d19a6`
- `engine/scripting/include/nengine/scripting/dotnet_host.hpp` — 2522 B / `4ce2eda2df1c`
- `engine/scripting/include/nengine/scripting/managed_build.hpp` — 1209 B / `a3277e6d26b5`
- `engine/scripting/include/nengine/scripting/managed_project.hpp` — 1185 B / `7978f06dabb4`
- `engine/scripting/include/nengine/scripting/managed_runtime.hpp` — 22016 B / `e1cc269c8a07`
- `engine/scripting/include/nengine/scripting/registration.hpp` — 346 B / `658b83ec27f2`
- `engine/scripting/include/nengine/scripting/script_system.hpp` — 1927 B / `dd34fd7e0456`
- `engine/scripting/src/dotnet_host.cpp` — 21303 B / `a381f6f06a50`
- `engine/scripting/src/managed_build.cpp` — 10869 B / `d5a50fb865de`
- `engine/scripting/src/managed_project.cpp` — 141638 B / `b0aec0a5ba86`
- `engine/scripting/src/managed_runtime.cpp` — 60024 B / `94ea3aa35f76`
- `engine/scripting/src/registration.cpp` — 4816 B / `3393ff959940`
- `engine/scripting/src/script_system.cpp` — 31414 B / `4e849408dcc1`

### UI Canvas — 15개

- `engine/ui/CMakeLists.txt` — 571 B / `1a842e1d2cdf`
- `engine/ui/include/nengine/ui/canvas.hpp` — 4706 B / `a39dc6b48da2`
- `engine/ui/include/nengine/ui/canvas_asset.hpp` — 934 B / `e3ae1808ce65`
- `engine/ui/include/nengine/ui/canvas_edit_history.hpp` — 934 B / `eff9b55573ee`
- `engine/ui/include/nengine/ui/canvas_serialization.hpp` — 500 B / `6fc5b9fb60a4`
- `engine/ui/include/nengine/ui/keyboard_router.hpp` — 1011 B / `7b5e036fd565`
- `engine/ui/include/nengine/ui/pointer_router.hpp` — 1187 B / `7c2e6c9db20f`
- `engine/ui/include/nengine/ui/rasterizer.hpp` — 1310 B / `70a4db7bd237`
- `engine/ui/src/canvas.cpp` — 17642 B / `9d604e4f1d50`
- `engine/ui/src/canvas_asset.cpp` — 2541 B / `f51929b7c1f3`
- `engine/ui/src/canvas_edit_history.cpp` — 2035 B / `6dcff024dbe9`
- `engine/ui/src/canvas_serialization.cpp` — 6849 B / `4f259d4e40e2`
- `engine/ui/src/keyboard_router.cpp` — 3520 B / `0a0315aa1d2a`
- `engine/ui/src/pointer_router.cpp` — 2104 B / `26e6f49f2f37`
- `engine/ui/src/rasterizer.cpp` — 11832 B / `7a74795b09ba`

### 자동 테스트 — 14개

- `tests/CMakeLists.txt` — 3286 B / `e19c75e4919e`
- `tests/asset_tests.cpp` — 26445 B / `887443748195`
- `tests/audio_tests.cpp` — 22452 B / `368fcacd73ca`
- `tests/core_tests.cpp` — 16821 B / `a540779ed952`
- `tests/editor_tests.cpp` — 86932 B / `95173f012b39`
- `tests/input_tests.cpp` — 15032 B / `96d60ee52027`
- `tests/navigation_tests.cpp` — 11028 B / `9c782237930f`
- `tests/physics_tests.cpp` — 170446 B / `b8819876f94a`
- `tests/player_cli_fixture.cpp` — 2236 B / `3559d204bc4d`
- `tests/player_cli_smoke.cmake` — 2188 B / `e11e8b1477a5`
- `tests/player_tests.cpp` — 86480 B / `2f11034d2c4d`
- `tests/render_tests.cpp` — 180834 B / `d88fbf4a96a5`
- `tests/scripting_tests.cpp` — 173600 B / `59a5f0127b48`
- `tests/ui_tests.cpp` — 36872 B / `986e70eddc8e`

### 외부 라이선스·공급 코드 — 2개

- `third_party/stb/LICENSE` — 2510 B / `a77ae91f3ec0`
- `third_party/stb/stb_image.h` — 283010 B / `9eedabedc45b`

### 셰이더 도구 — 2개

- `tools/compile_shader.py` — 3497 B / `0bfb25d69425`
- `tools/embed_spirv.py` — 1054 B / `f4e848fd2305`

