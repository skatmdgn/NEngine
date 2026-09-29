# Architecture decisions

## ADR-001 — Knowledge compatibility, not project compatibility
Status: Accepted

Unity projects/binaries are not an interchange target. The target is transfer of developer mental models and workflows.

## ADR-002 — Clean-room implementation
Status: Accepted

Do not use Unity source code or Unity-source-derived implementation. Public concepts, general game-engine techniques, independent specifications and independently designed behavior form the implementation basis.

## ADR-003 — Generational entity handles
Status: Accepted

Entity identity is a 64-bit index+generation handle. Destroyed handles must never silently alias newly created objects that reuse a slot.

## ADR-004 — Same World model for editor and runtime
Status: Accepted

Avoid separate editor/runtime object graphs. Play Mode clones or snapshots the same data model.

## ADR-005 — Vulkan-first graphics
Status: Accepted for 1.0 planning

Windows and Android share a Vulkan-first renderer to keep backend count low. D3D12 is a post-1.0 compatibility/performance option unless QA proves it necessary sooner.

## ADR-006 — C# gameplay + external IDE
Status: Accepted

Visual Studio is the primary Windows C# authoring/debugging experience. Engine-generated project files expose engine assemblies, NuGet packages and plugin assemblies.

## ADR-007 — Android toolchains are independent modules
Status: Accepted

JDK/SDK/NDK/AGP/Gradle combinations are versioned profiles installed side by side. Google requirement changes must not force renderer/scene/physics changes.

## ADR-008 — External SDKs never enter core
Status: Accepted

AdMob, Firebase, Steam, analytics and similar integrations live as packages/adapters behind stable plugin contracts.
