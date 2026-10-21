# Native navigation foundation — 30 slices

Implemented source in engine/navigation. Final Windows/Ubuntu CI required before verified status.
This is a convex XZ polygon graph authored by code, not an automatic navmesh baker.

## Implemented native contract

- Convex polygons with up to 32 vertices and near-planar Y coordinates, 4096 polygon cap.
- Shared-edge adjacency, area-mask weighted deterministic Dijkstra and bounded expansions.
- Directed/bidirectional off-mesh links and dynamic circular obstacles (radius expanded by agent radius).
- Constant-speed NavAgent moving across route waypoints; stop distance, invalid delta checks.
- Versioned .nnav serialization with transactional bounded 4MiB reader/writer.
- 34 focused regression assertions in a new NEngineNavigationTests group.

## 30 independently testable slices

01. Convex polygon insertion
02. Stable contiguous polygon handles
03. Clockwise/counterclockwise topology
04. Non-convex geometry rejection
05. Repeated/zero-length edge rejection
06. Finite bounded vertices
07. Nonzero area flags
08. Finite positive traversal costs
09. Interior/boundary hit testing
10. Separated floor validation
11. Shared-edge bidirectional connectivity
12. Invalid/duplicate edge rejection
13. Nonsharing polygon rejection
14. Deterministic shortest corridor
15. Area-mask traversal filtering
16. Runtime polygon enable/disable
17. Area-cost updates
18. Dynamic circle obstacle creation
19. Obstacle toggle and removal
20. Agent-radius obstacle inflation
21. Directed off-mesh links
22. Disabled and nonreversible links
23. Bidirectional off-mesh traversal
24. Best-reachable partial paths
25. Search expansion budget
26. Invalid input query rejection
27. Agent route assignment
28. Delta-time movement and arrival
29. Invalid time-step rejection
30. Versioned graph roundtrip

Additional regression checks: malformed .nnav is transactional, oversized .nnav rejected,
file save/load roundtrip, and graph clearing.

## Limitations before full NavMesh milestone

Scene mesh baking, triangle/portal funnel smoothing, Editor authoring, C# GameObject
agent components, adaptive obstacle repathing, link traversal animation,
Windows gameplay acceptance and Android integration remain unimplemented.

Tests: cmake configure/build and ctest; final GitHub CI on Ubuntu and Windows.
