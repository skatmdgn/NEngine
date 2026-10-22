# Transactional navigation triangle bake — 30-slice batch (2026-10-22)

## Scope and limits

The new `nav_bake.hpp` native API builds a bounded `NavMesh` from
authored world-space triangles or indexed triangle-list geometry. It filters
unsuitable faces by slope, footprint, height span and area mask, normalizes
face winding, optionally welds vertices, and connects full shared seams.

It does **not** extract scene entities or MeshRenderer meshes from the World,
bake arbitrary complex collision geometry, create triangles from heightmaps,
handle T-junction splitting, implement agent-size erosion/clearance, perform
funnel/portal smoothing, bind NavMeshAgent C# components, or generate Android
player integration. Original NavMesh runtime and Editor/Player .nnav support
remain unchanged. All published bakes are transactional: on input rejection
the prior mesh and report remain unchanged. Filtered-out triangles are
reported explicitly and are not errors.

## Native API

- `bake_nav_triangles(span<const NavBakeTriangle>, NavMesh&, options, report, error)`
- `bake_indexed_nav_mesh(span<const Vec3>, span<const uint32_t>, NavMesh&, options, mask, cost, report, error)`
- `NavBakeOptions`: slope angle, triangle Y span, minimum horizontal
  footprint, quantization/welding tolerance, allowed area mask, input budget,
  seam-connection switch.
- `NavBakeReport`: total/accepted/filtered counters and connected seam count.
- A zero-tolerance bake matches identical world-space vertices for full-edge
  connectivity; configure welding explicitly for nearly identical seams.
  Faces must have 3 vertices and no non-manifold (three-face) shared edges.
- The baked `NavMesh` can be serialized as a standard `.nnav` and routed
  by existing native `find_path`, Editor import and Player native graph APIs.
  Automatic Editor scene-mesh extraction is a separate task.

## 30 independently testable acceptance slices

| Slice | Behavior | Regression |
|---:|---|---|
| 01 | Empty source clears stale output on success | Empty bake |
| 02 | Cook single authored triangle | Triangle insertion |
| 03 | Normalize clockwise/reversed winding | Reverse orientation |
| 04 | Preserve original authoring vertices | Non-mutating input |
| 05 | Ingest standard indexed triangle list | Indexed quad |
| 06 | Reject trailing incomplete index triples | Index count |
| 07 | Reject out-of-range vertex indices safely | Index bounds |
| 08 | Do not dereference unused vertices | Sparse referenced input |
| 09 | Reject zero polygon area masks | Attribute validation |
| 10 | Reject non-finite traversal weights | Cost validation |
| 11 | Enforce configurable source triangle budget | Oversize transactional |
| 12 | Reject NaN/inf geometry | Nonfinite input |
| 13 | Reject coordinates outside engine limits | Bounds validation |
| 14 | Reject coincident/degenerate source faces | Triangle degeneracy |
| 15 | Filter vertical surfaces with no XZ footprint | Vertical filter |
| 16 | Filter surfaces steeper than slope threshold | Steep filter |
| 17 | Accept legitimate shallow ramps | Sloped triangle |
| 18 | Filter geometry exceeding allowed Y span | Height filter |
| 19 | Filter tiny projected triangles | Minimum area |
| 20 | Reject invalid slope configuration | Config validation |
| 21 | Reject excessive weld distance | Quantization bounds |
| 22 | Filter non-selected walkable area classes | Area category |
| 23 | Retain authored area flags in cooked polys | Mask propagation |
| 24 | Retain per-triangle traversal cost | Cost propagation |
| 25 | Auto-connect matching full seams and route | Triangle corridor |
| 26 | Permit intentionally unconnected triangles | Adjacency disabled |
| 27 | Leave separate islands unconnected | Disconnected triangles |
| 28 | Weld near-equal shared vertices when requested | Seam quantization |
| 29 | Reject duplicate/overlapping identical faces | Transactional duplicate |
| 30 | Reject three-face/non-manifold shared edges | Transactional manifold |

The separate `NEngineNavigationBakeTests` suite also checks a baked graph's
`.nnav` persistence roundtrip and a post-load path query.

## Verification protocol

- Portable CMake target and CTest test suite added.
- CI must pass Ubuntu and Windows compilation and all CTest groups on the
  **final PR commit** before considering this batch verified.
- Hosted CI cannot validate a live Windows Vulkan game, Android device,
  Editor scene-mesh extraction, or managed NavMeshAgent gameplay.
